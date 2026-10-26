#include "WeaselPanel.h"
#include <WeaselUI.h>
#include <mutex>
#include <atomic>

namespace weasel {
// ----------------------------------------------------------------------------
class UIImpl {
 public:
  WeaselPanel panel;
  UIImpl(UI& ui) : panel(ui) {}
  ~UIImpl() {}
  bool IsShown() const {
    return panel.IsWindow() && ::IsWindowVisible(panel.hwnd());
  }
  void Refresh() {
    if (!panel.IsWindow())
      return;
    panel.Refresh();
  }
  void RepositionPreview() {
    if (panel.IsWindow())
      panel.RepositionPreview();
  }
  void Show() {
    if (!panel.IsWindow())
      return;
    panel.ShowWindow(SW_SHOWNA);
  }
  void Hide() {
    if (!panel.IsWindow())
      return;
    panel.ShowWindow(SW_HIDE);
  }
  void ShowWithTimeout(size_t millisec) { panel.ShowWithTimeout(millisec); }
  bool IsCountingDown() const { return panel.IsCountingDown(); }
};

// Only immutable snapshots cross the IPC/window thread boundary. In
// particular the panel must not hold references to the worker's model.
class AsyncServerUI {
 public:
  struct Snapshot {
    Context ctx;
    Status status;
    UIStyle style;
    UICallbackFunc callback;
    std::function<void(bool, bool)> layout_callback;
    RECT position{};
    bool has_position = false;
    bool visible = false;
    ULONGLONG deadline = 0;
    HWND lifetime = nullptr;
  };
  UI presenter;
  HWND window = nullptr;
  std::atomic<bool> shown{false};
  std::atomic<bool> counting{false};
  std::atomic<bool> reposition{false};

  bool IsCountingDown() const {
    std::lock_guard<std::mutex> lock(mutex);
    return requested.visible && requested.deadline > GetTickCount64();
  }

  bool Create(HWND parent) {
    WNDCLASS wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"WeaselAsyncPresenter.v1";
    if (!RegisterClass(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
      return false;
    window = CreateWindow(wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                          nullptr, wc.hInstance, this);
    if (!window)
      return false;
    presenter.InServer() = true;
    if (!presenter.Create(parent))
      return false;
    SetTimer(window, 1, 200, nullptr);
    return true;
  }
  ~AsyncServerUI() {
    if (window)
      DestroyWindow(window);
    presenter.Destroy(true);
  }
  void Publish(UI& model,
               int visible = -1,
               size_t timeout = 0,
               const RECT* position = nullptr) {
    std::lock_guard<std::mutex> lock(mutex);
    requested.ctx = model.ctx();
    requested.status = model.status();
    requested.style = model.style();
    requested.callback = model.uiCallback();
    requested.layout_callback = model.layout_callback_;
    requested.lifetime = model.lifetime_window_;
    if (position) {
      requested.position = *position;
      requested.has_position = true;
    }
    if (visible >= 0) {
      requested.visible = visible != 0;
      requested.deadline = timeout ? GetTickCount64() + timeout : 0;
    }
    dirty = true;
    if (!batch_depth)
      CommitLocked();
  }
  void BeginUpdate() {
    std::lock_guard<std::mutex> lock(mutex);
    ++batch_depth;
  }
  void EndUpdate() {
    std::lock_guard<std::mutex> lock(mutex);
    if (batch_depth && !--batch_depth)
      CommitLocked();
  }

 private:
  mutable std::mutex mutex;
  Snapshot requested, pending, current;
  unsigned batch_depth = 0;
  bool dirty = false;
  void CommitLocked() {
    if (!dirty)
      return;
    pending = requested;
    dirty = false;
    if (!posted)
      posted = !!PostMessage(window, WM_APP + 1, 0, 0);
  }
  bool posted = false;
  void Apply() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      current = pending;
      posted = false;
    }
    const bool style_changed = presenter.style() != current.style;
    const bool data_changed = !(presenter.ctx() == current.ctx &&
                                presenter.status() == current.status);
    presenter.style() = current.style;
    presenter.SetUICallBack(current.callback);
    presenter.Update(current.ctx, current.status);
    // Style-only changes also need layout/paint.
    if (style_changed && !data_changed)
      presenter.Refresh();
    // MoveTo requires a layout. On the first snapshot Update creates it.
    if (current.has_position)
      presenter.UpdateInputPosition(current.position);
    const auto now = GetTickCount64();
    if (!current.visible || (current.lifetime && !IsWindow(current.lifetime)) ||
        (current.deadline && current.deadline <= now))
      presenter.Hide();
    else if (current.deadline)
      presenter.ShowWithTimeout(static_cast<size_t>(current.deadline - now));
    else
      presenter.Show();
    Observe(true);
  }
  void Observe(bool force = false) {
    // A crashed host cannot send FocusOut/EndSession.
    if (current.lifetime && !IsWindow(current.lifetime))
      presenter.Hide();
    const bool visible = !!presenter.IsShown();
    const bool reversed = presenter.GetIsReposition();
    if (current.layout_callback &&
        (force || visible != shown || reversed != reposition))
      current.layout_callback(reversed, visible);
    shown = visible;
    counting = !!presenter.IsCountingDown();
    reposition = reversed;
  }
  static LRESULT CALLBACK WindowProc(HWND hwnd,
                                     UINT msg,
                                     WPARAM wp,
                                     LPARAM lp) {
    auto self =
        reinterpret_cast<AsyncServerUI*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
      self = static_cast<AsyncServerUI*>(
          reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams);
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self && msg == WM_APP + 1) {
      self->Apply();
      return 0;
    }
    if (self && msg == WM_TIMER) {
      self->Observe();
      return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
  }
};
// ----------------------------------------------------------------------------
BOOL UI::IsCountingDown() const {
  if (async_)
    return async_->IsCountingDown();
  return pimpl_ && pimpl_->panel.IsCountingDown();
};

UI::UI() : pimpl_(nullptr), in_server_(false) {}

UI::~UI() {
  if (pimpl_)
    Destroy(true);
}
BOOL UI::IsShown() const {
  if (async_)
    return async_->shown;
  return pimpl_ && pimpl_->IsShown();
}
void UI::UpdateInputPosition(RECT const& rc) {
  if (async_) {
    async_->Publish(*this, -1, 0, &rc);
    return;
  }
  if (pimpl_ && pimpl_->panel.IsWindow()) {
    pimpl_->panel.MoveTo(rc);
  }
}
void UI::Update(const Context& ctx, const Status& status) {
  if (ctx_ == ctx && status_ == status)
    return;
  ctx_ = ctx;
  status_ = status;
  // The asynchronous presenter applies display transformations once. Keep
  // the worker's model intact so a second update cannot abbreviate it again.
  if (!async_ && style_.candidate_abbreviate_length > 0) {
    for (auto& c : ctx_.cinfo.candies) {
      if (c.str.length() > (size_t)style_.candidate_abbreviate_length) {
        c.str =
            c.str.substr(0, (size_t)style_.candidate_abbreviate_length - 1) +
            L"..." + c.str.substr(c.str.length() - 1);
      }
    }
  }
  Refresh();
}
void UI::Refresh() {
  if (async_) {
    async_->Publish(*this);
    return;
  }
  if (pimpl_) {
    pimpl_->Refresh();
  }
}
void UI::RepositionPreview() {
  if (pimpl_)
    pimpl_->RepositionPreview();
}
void UI::ShowWithTimeout(size_t millisec) {
  if (async_) {
    async_->Publish(*this, 1, millisec);
    return;
  }
  if (pimpl_) {
    pimpl_->ShowWithTimeout(millisec);
  }
}
void UI::Show() {
  if (async_) {
    async_->Publish(*this, 1);
    return;
  }
  if (pimpl_) {
    pimpl_->Show();
  }
}
void UI::Hide() {
  if (async_) {
    async_->Publish(*this, 0);
    return;
  }
  if (pimpl_) {
    pimpl_->Hide();
  }
}
void UI::Destroy(bool full) {
  if (async_) {
    async_.reset();
    return;
  }
  if (pimpl_) {
    if (pimpl_->panel.IsWindow())
      pimpl_->panel.DestroyWindow();
    if (full) {
      // ensure window resources and shared devices are released
      pimpl_->panel.ReleaseAllResources();
      pimpl_.reset();
    }
  }
}
bool UI::Create(HWND parent, bool preview_mode) {
  if (pimpl_) {
    pimpl_->panel.SetPreviewMode(preview_mode);
    pimpl_->panel.Create(parent, preview_mode);
    return true;
  }
  pimpl_ = std::make_unique<UIImpl>(*this);
  if (!pimpl_)
    return false;
  return pimpl_->panel.Create(parent, preview_mode);
}
bool UI::GetIsReposition() {
  if (async_)
    return async_->reposition;
  return pimpl_ && pimpl_->panel.GetIsReposition();
}

HWND UI::hwnd() {
  if (async_)
    return async_->presenter.hwnd();
  if (pimpl_ && pimpl_->panel.IsWindow())
    return pimpl_->panel.hwnd();
  return nullptr;
}

UI::UpdateBatch::UpdateBatch(UI& ui) : ui_(ui) {
  if (ui_.async_)
    ui_.async_->BeginUpdate();
}

UI::UpdateBatch::~UpdateBatch() {
  if (ui_.async_)
    ui_.async_->EndUpdate();
}

bool UI::CreateAsyncServer(HWND parent) {
  async_ = std::make_unique<AsyncServerUI>();
  if (async_->Create(parent))
    return true;
  async_.reset();
  return false;
}
}  // namespace weasel
