#include "stdafx.h"

#include "WeaselTSF.h"
#include "CandidateList.h"
#include <KeyEvent.h>
#include <math.h>

using namespace std;
using namespace weasel;

CCandidateList::CCandidateList(com_ptr<WeaselTSF> pTextService)
    : _ui(make_unique<UI>()), _tsf(pTextService) {
  _cRef = 1;
}

CCandidateList::~CCandidateList() {
  _DestroyCallbackWindow();
}

void CCandidateList::ConnectServerUI() {
  _serverUI = false;
  _shown = false;
  _sentToken = 0;
  _remoteShown = false;
  _remoteReposition = false;
  _InvalidateUIActions();
  // Immersive hosts require the candidate window to be owned by their view.
  // Retain that existing path; desktop clients negotiate server rendering.
  if (_tsf->isImmersive())
    return;
  if (!_callbackWindow) {
    EnterCriticalSection(&g_cs);
    WNDCLASS wc{};
    wc.hInstance = g_hInst;
    wc.lpfnWndProc = CallbackProc;
    wc.lpszClassName = WEASEL_UI_CALLBACK_CLASS;
    if (!RegisterClass(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      LeaveCriticalSection(&g_cs);
      return;
    }
    _callbackWindow = CreateWindow(wc.lpszClassName, L"", 0, 0, 0, 0, 0,
                                   HWND_MESSAGE, nullptr, g_hInst, this);
    LeaveCriticalSection(&g_cs);
  }
  if (_callbackWindow)
    _serverUI = _tsf->m_client.EnableServerUI(_callbackWindow);
  if (_serverUI)
    _ui->Destroy(true);
}

void CCandidateList::_DestroyCallbackWindow() {
  EnterCriticalSection(&g_cs);
  if (_callbackWindow) {
    DestroyWindow(_callbackWindow);
    _callbackWindow = nullptr;
  }
  // Fails harmlessly while another instance still owns a window of this
  // class. The last instance removes the DLL's WndProc registration.
  UnregisterClass(WEASEL_UI_CALLBACK_CLASS, g_hInst);
  LeaveCriticalSection(&g_cs);
}

LRESULT CALLBACK CCandidateList::CallbackProc(HWND hwnd,
                                              UINT msg,
                                              WPARAM wp,
                                              LPARAM lp) {
  auto self =
      reinterpret_cast<CCandidateList*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (msg == WM_NCCREATE) {
    self = static_cast<CCandidateList*>(
        reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  if (self && msg == WM_WEASEL_UI_CALLBACK) {
    com_ptr<CCandidateList> keep_alive(self);
    if (!self->_serverUI || wp != self->_uiToken)
      return 0;
    const auto action = static_cast<DWORD>(lp) & 0xff;
    const auto value = static_cast<DWORD>(lp) >> 8;
    if (action == UI_LAYOUT) {
      if (value > 3)
        return 0;
      self->_remoteReposition = (value & 1) != 0;
      self->_remoteShown = self->_visibility.visible() && (value & 2) != 0;
      return 0;
    }
    if (!self->_shown ||
        !self->_visibility.AcceptsAction(static_cast<DWORD>(wp),
                                         self->_uiToken) ||
        !self->_tsf->_status.composing)
      return 0;
    com_ptr<ITfDocumentMgr> focused, owner;
    if (!self->_pContextDocument || !self->_tsf->_GetThreadMgr() ||
        FAILED(self->_tsf->_GetThreadMgr()->GetFocus(&focused)) ||
        FAILED(self->_pContextDocument->GetDocumentMgr(&owner)) ||
        focused != owner)
      return 0;
    size_t index = static_cast<DWORD>(lp) >> 8;
    bool forward = index != 0;
    switch (lp & 0xff) {
      case UI_SELECT:
      case UI_HOVER:
        if (index >= self->_ui->ctx().cinfo.candies.size())
          return 0;
        if ((lp & 0xff) == UI_SELECT)
          self->_tsf->HandleUICallback(&index, nullptr, nullptr, nullptr);
        else
          self->_tsf->HandleUICallback(nullptr, &index, nullptr, nullptr);
        break;
      case UI_PAGE:
        if (index > 1)
          return 0;
        self->_tsf->HandleUICallback(nullptr, nullptr, &forward, nullptr);
        break;
      case UI_SCROLL:
        if (index > 1)
          return 0;
        self->_tsf->HandleUICallback(nullptr, nullptr, nullptr, &forward);
        break;
    }
    return 0;
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}

STDMETHODIMP CCandidateList::QueryInterface(REFIID riid, void** ppvObj) {
  if (ppvObj == nullptr) {
    return E_INVALIDARG;
  }

  *ppvObj = nullptr;

  if (IsEqualIID(riid, IID_ITfUIElement) ||
      IsEqualIID(riid, IID_ITfCandidateListUIElement) ||
      IsEqualIID(riid, IID_ITfCandidateListUIElementBehavior)) {
    *ppvObj = (ITfCandidateListUIElementBehavior*)this;
  } else if (IsEqualIID(riid, IID_IUnknown) ||
             IsEqualIID(riid,
                        __uuidof(ITfIntegratableCandidateListUIElement))) {
    *ppvObj = (ITfIntegratableCandidateListUIElement*)this;
  }

  if (*ppvObj) {
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CCandidateList::AddRef(void) {
  return ++_cRef;
}

STDMETHODIMP_(ULONG) CCandidateList::Release(void) {
  LONG cr = --_cRef;

  assert(_cRef >= 0);

  if (_cRef == 0) {
    delete this;
  }

  return cr;
}

STDMETHODIMP CCandidateList::GetDescription(BSTR* pbstr) {
  static auto str = SysAllocString(L"Candidate List");
  if (pbstr) {
    *pbstr = str;
  }
  return S_OK;
}

STDMETHODIMP CCandidateList::GetGUID(GUID* pguid) {
  /// 36c3c795-7159-45aa-ab12-30229a51dbd3
  *pguid = {0x36c3c795,
            0x7159,
            0x45aa,
            {0xab, 0x12, 0x30, 0x22, 0x9a, 0x51, 0xdb, 0xd3}};
  return S_OK;
}

STDMETHODIMP CCandidateList::Show(BOOL showCandidateWindow) {
  _visibility.SetHostAllowed(!!showCandidateWindow);
  return _UpdateVisibility();
}

void CCandidateList::SetFocused(bool focused) {
  _visibility.SetFocused(focused);
  _UpdateVisibility();
}

void CCandidateList::_InvalidateUIActions() {
  static LONG token = 0;
  _uiToken = static_cast<DWORD>(InterlockedIncrement(&token));
  if (!_uiToken)
    _uiToken = static_cast<DWORD>(InterlockedIncrement(&token));
}

HRESULT CCandidateList::_UpdateVisibility() {
  const bool show = _visibility.visible();
  if (_serverUI) {
    if (!show)
      _remoteShown = false;
    if (show != _shown ||
        (show && (_sentToken != _uiToken || _sentRevision != _revision))) {
      if (!_tsf->m_client.ShowServerUI(show ? _uiToken : 0, _revision))
        return E_FAIL;
      _sentRevision = _revision;
      _shown = show;
      _sentToken = show ? _uiToken : 0;
    }
  } else if (show) {
    if (!_ui->hwnd())
      _MakeUIWindow();
    _ui->Show();
  } else {
    _ui->Hide();
  }
  return S_OK;
}

STDMETHODIMP CCandidateList::IsShown(BOOL* pIsShow) {
  if (!pIsShow)
    return E_INVALIDARG;
  *pIsShow = _serverUI ? _remoteShown : _ui->IsShown();
  return S_OK;
}

STDMETHODIMP CCandidateList::GetUpdatedFlags(DWORD* pdwFlags) {
  if (!pdwFlags)
    return E_INVALIDARG;

  *pdwFlags = TF_CLUIE_DOCUMENTMGR | TF_CLUIE_COUNT | TF_CLUIE_SELECTION |
              TF_CLUIE_STRING | TF_CLUIE_CURRENTPAGE;
  return S_OK;
}

STDMETHODIMP CCandidateList::GetDocumentMgr(ITfDocumentMgr** ppdim) {
  *ppdim = nullptr;
  auto pThreadMgr = _tsf->_GetThreadMgr();
  if (pThreadMgr == nullptr) {
    return E_FAIL;
  }
  if (FAILED(pThreadMgr->GetFocus(ppdim)) || (*ppdim == nullptr)) {
    return E_FAIL;
  }
  return S_OK;
}

STDMETHODIMP CCandidateList::GetCount(UINT* pCandidateCount) {
  *pCandidateCount = static_cast<UINT>(_ui->ctx().cinfo.candies.size());
  return S_OK;
}

STDMETHODIMP CCandidateList::GetSelection(UINT* pSelectedCandidateIndex) {
  *pSelectedCandidateIndex = _ui->ctx().cinfo.highlighted;
  return S_OK;
}

STDMETHODIMP CCandidateList::GetString(UINT uIndex, BSTR* pbstr) {
  *pbstr = nullptr;
  auto& cinfo = _ui->ctx().cinfo;
  if (uIndex >= cinfo.candies.size())
    return E_INVALIDARG;

  auto& str = cinfo.candies[uIndex].str;
  *pbstr = SysAllocStringLen(str.c_str(), static_cast<UINT>(str.size()) + 1);

  return S_OK;
}

STDMETHODIMP CCandidateList::GetPageIndex(UINT* pIndex,
                                          UINT uSize,
                                          UINT* puPageCnt) {
  if (!puPageCnt)
    return E_INVALIDARG;
  *puPageCnt = 1;
  if (pIndex) {
    if (uSize < *puPageCnt) {
      return E_INVALIDARG;
    }
    *pIndex = 0;
  }
  return S_OK;
}

STDMETHODIMP CCandidateList::SetPageIndex(UINT* pIndex, UINT uPageCnt) {
  if (!pIndex)
    return E_INVALIDARG;
  return S_OK;
}

STDMETHODIMP CCandidateList::GetCurrentPage(UINT* puPage) {
  *puPage = 0;
  return S_OK;
}

STDMETHODIMP CCandidateList::SetSelection(UINT nIndex) {
  _ui->ctx().cinfo.highlighted = nIndex;
  return S_OK;
}

STDMETHODIMP CCandidateList::Finalize(void) {
  Destroy();
  return S_OK;
}

STDMETHODIMP CCandidateList::Abort(void) {
  _tsf->_AbortComposition(true);
  Destroy();
  return S_OK;
}

STDMETHODIMP CCandidateList::SetIntegrationStyle(GUID guidIntegrationStyle) {
  return S_OK;
}

STDMETHODIMP CCandidateList::GetSelectionStyle(
    TfIntegratableCandidateListSelectionStyle* ptfSelectionStyle) {
  *ptfSelectionStyle = _selectionStyle;
  return S_OK;
}

STDMETHODIMP CCandidateList::OnKeyDown(WPARAM wParam,
                                       LPARAM lParam,
                                       BOOL* pIsEaten) {
  *pIsEaten = TRUE;
  return S_OK;
}

STDMETHODIMP CCandidateList::ShowCandidateNumbers(BOOL* pIsShow) {
  *pIsShow = TRUE;
  return S_OK;
}

STDMETHODIMP CCandidateList::FinalizeExactCompositionString() {
  _tsf->_AbortComposition(false);
  return E_NOTIMPL;
}

void CCandidateList::UpdateUI(const Context& ctx,
                              const Status& status,
                              DWORD revision) {
  if (_ui->style().inline_preedit) {
    _ui->style().client_caps |= weasel::INLINE_PREEDIT_CAPABLE;
  } else {
    _ui->style().client_caps &= ~weasel::INLINE_PREEDIT_CAPABLE;
  }

  /// In UWP, candidate window will only be shown
  /// if it is owned by active view window
  //_UpdateOwner();
  if (_serverUI && !(_ui->ctx() == ctx))
    _InvalidateUIActions();
  _ui->Update(ctx, status);
  _revision = revision;
  _visibility.SetComposing(status.composing);
  if (_visibility.started())
    _UpdateUIElement();

  _UpdateVisibility();
}

void CCandidateList::UpdateStyle(const UIStyle& sty) {
  _ui->style() = sty;
}

void CCandidateList::UpdateInputPosition(RECT const& rc) {
  _ui->UpdateInputPosition(rc);
}

void CCandidateList::Destroy() {
  EndUI();
  _DisposeUIWindow();
}

void CCandidateList::DestroyAll() {
  EndUI();
  _DisposeUIWindowAll();
  _pContextDocument = nullptr;
  _DestroyCallbackWindow();
  _serverUI = false;
}
UIStyle& CCandidateList::style() {
  // return _ui->style();
  return _style;
}

HWND CCandidateList::_GetActiveWnd() {
  com_ptr<ITfDocumentMgr> pDocumentMgr;
  com_ptr<ITfContext> pContext;
  com_ptr<ITfContextView> pContextView;
  com_ptr<ITfThreadMgr> pThreadMgr = _tsf->_GetThreadMgr();

  HWND w = NULL;

  // Reset current context
  _pContextDocument = nullptr;

  if (pThreadMgr != nullptr && SUCCEEDED(pThreadMgr->GetFocus(&pDocumentMgr)) &&
      SUCCEEDED(pDocumentMgr->GetTop(&pContext)) &&
      SUCCEEDED(pContext->GetActiveView(&pContextView))) {
    // Set current context
    _pContextDocument = pContext;
    pContextView->GetWnd(&w);
  }

  if (w == NULL)
    w = ::GetFocus();
  return w;
}

HRESULT CCandidateList::_UpdateUIElement() {
  HRESULT hr = S_OK;

  com_ptr<ITfUIElementMgr> pUIElementMgr;
  com_ptr<ITfThreadMgr> pThreadMgr = _tsf->_GetThreadMgr();
  if (nullptr == pThreadMgr) {
    return S_OK;
  }
  hr = pThreadMgr->QueryInterface(IID_ITfUIElementMgr, (void**)&pUIElementMgr);

  if (hr == S_OK) {
    pUIElementMgr->UpdateUIElement(uiid);
  }

  return S_OK;
}

void CCandidateList::StartUI() {
  if (_visibility.started())
    return;

  com_ptr<ITfThreadMgr> pThreadMgr = _tsf->_GetThreadMgr();
  if (!pThreadMgr) {
    return;
  }

  com_ptr<ITfUIElementMgr> pUIElementMgr;
  auto hr = pThreadMgr->QueryInterface(&pUIElementMgr);
  if (FAILED(hr))
    return;

  if (pUIElementMgr == NULL) {
    return;
  }

  if (!_ui->uiCallback())
    _ui->SetUICallBack([this](size_t* const sel, size_t* const hov,
                              bool* const next, bool* const scroll_next) {
      _tsf->HandleUICallback(sel, hov, next, scroll_next);
    });
  BOOL show = TRUE;
  if (FAILED(pUIElementMgr->BeginUIElement(this, &show, &uiid)))
    return;
  _visibility.Begin(!!show);
  _GetActiveWnd();
  _InvalidateUIActions();
  if (show && !_serverUI) {
    _ui->style() = _style;
    _MakeUIWindow();
    if (!_ui_warmed_up && _ui->hwnd() != nullptr) {
      // Warm up render resources before first visible paint in TSF path.
      _ui->Refresh();
      _ui->Hide();
      _ui_warmed_up = true;
    }
  }
}

void CCandidateList::EndUI() {
  if (!_visibility.started())
    return;
  _visibility.End();
  _UpdateVisibility();

  com_ptr<ITfThreadMgr> pThreadMgr = _tsf->_GetThreadMgr();
  if (pThreadMgr) {
    com_ptr<ITfUIElementMgr> emgr;
    auto hr = pThreadMgr->QueryInterface(&emgr);
    if (SUCCEEDED(hr) && emgr != NULL)
      emgr->EndUIElement(uiid);
  }
  _DisposeUIWindow();
}

com_ptr<ITfContext> CCandidateList::GetContextDocument() {
  return _pContextDocument;
}

void CCandidateList::_DisposeUIWindow() {
  if (_ui == nullptr) {
    return;
  }

  _ui->Destroy();
}

void CCandidateList::_DisposeUIWindowAll() {
  if (_ui == nullptr) {
    return;
  }

  // call _ui->Destroy(true) to clean resources
  _ui->Destroy(true);
  _ui_warmed_up = false;
}

void CCandidateList::_MakeUIWindow() {
  HWND p = _GetActiveWnd();
  _ui->Create(p);
}

void WeaselTSF::_UpdateUI(const Context& ctx,
                          const Status& status,
                          DWORD revision) {
  _cand->UpdateUI(ctx, status, revision);
}

void WeaselTSF::_StartUI() {
  _cand->StartUI();
}

void WeaselTSF::_EndUI() {
  _cand->EndUI();
}

void WeaselTSF::_ShowUI() {
  _cand->SetFocused(true);
}

void WeaselTSF::_HideUI() {
  _cand->SetFocused(false);
}

com_ptr<ITfContext> WeaselTSF::_GetUIContextDocument() {
  return _cand->GetContextDocument();
}

void WeaselTSF::_DeleteCandidateList() {
  _cand->Destroy();
}

void WeaselTSF::_SelectCandidateOnCurrentPage(size_t index) {
  m_client.SelectCandidateOnCurrentPage(index);
  if (_cand->_serverUI) {
    if (m_client.ReadState() && _pEditSessionContext)
      _UpdateComposition(_pEditSessionContext);
    return;
  }
  // simulate a VK_SELECT presskey to get data back and DoEditSession
  // the simulated keycode must be the one make TranslateKeycode Non-Zero return
  // fix me: are there any better ways?
  INPUT inputs[2];
  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki = {VK_SELECT, 0, 0, 0, 0};
  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki = {VK_SELECT, 0, KEYEVENTF_KEYUP, 0, 0};
  ::SendInput(sizeof(inputs) / sizeof(INPUT), inputs, sizeof(INPUT));
}

void WeaselTSF::_HandleMousePageEvent(bool* const nextPage,
                                      bool* const scrollNextPage) {
  // from scrolling event
  if (scrollNextPage) {
    if (_cand->style().paging_on_scroll)
      m_client.ChangePage(!(*scrollNextPage));
    else {
      UINT current_select = 0, cand_count = 0;
      _cand->GetSelection(&current_select);
      _cand->GetCount(&cand_count);
      bool is_reposition = _cand->GetIsReposition();
      int offset = *scrollNextPage ? 1 : -1;
      offset = offset * (is_reposition ? -1 : 1);
      int index = (int)current_select + offset;
      if (index >= 0 && index < (int)cand_count)
        m_client.HighlightCandidateOnCurrentPage((size_t)index);
      else {
        KeyEvent ke{0, 0};
        ke.keycode = (index < 0) ? ibus::Up : ibus::Down;
        m_client.ProcessKeyEvent(ke);
      }
    }
  } else {  // from click event
    m_client.ChangePage(!(*nextPage));
  }
  _UpdateComposition(_pEditSessionContext);
}

void WeaselTSF::_HandleMouseHoverEvent(const size_t index) {
  UINT current_select = 0;
  _cand->GetSelection(&current_select);

  if (index != current_select) {
    m_client.HighlightCandidateOnCurrentPage(index);
    _UpdateComposition(_pEditSessionContext);
  }
}

void WeaselTSF::HandleUICallback(size_t* const sel,
                                 size_t* const hov,
                                 bool* const next,
                                 bool* const scroll_next) {
  if (sel)
    _SelectCandidateOnCurrentPage(*sel);
  else if (hov)
    _HandleMouseHoverEvent(*hov);
  else if (next || scroll_next)
    _HandleMousePageEvent(next, scroll_next);
}
