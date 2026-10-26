#include <windows.h>
#include <WeaselUI.h>
#include <WeaselIPC.h>
#include <ResponseParser.h>
#include <thread>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* label) {
  if (!ok)
    throw std::runtime_error(label);
  std::cout << "PASS: " << label << std::endl;
}
void pump(int ms = 250) {
  auto until = GetTickCount64() + ms;
  do {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    Sleep(5);
  } while (GetTickCount64() < until);
}
DWORD layout_token = 0;
LRESULT CALLBACK callback(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_WEASEL_UI_CALLBACK && (l & 255) == UI_LAYOUT)
    layout_token = (DWORD)w;
  return DefWindowProc(h, m, w, l);
}
HWND endpoint() {
  WNDCLASS wc{};
  wc.lpfnWndProc = callback;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = WEASEL_UI_CALLBACK_CLASS;
  RegisterClass(&wc);
  return CreateWindow(wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                      nullptr, wc.hInstance, nullptr);
}
void test_ui() {
  weasel::UI ui;
  ui.InServer() = true;
  check(ui.CreateAsyncServer(nullptr), "server presenter initializes");
  auto owner = endpoint();
  ui.SetLifetimeWindow(owner);
  weasel::Context ctx;
  ctx.preedit.str = L"nihao";
  ctx.cinfo.candies.emplace_back(L"你好");
  ctx.cinfo.labels.emplace_back(L"1");
  ctx.cinfo.comments.emplace_back(L"");
  weasel::Status status;
  status.composing = true;
  // Intentionally do not pump the owner thread until the worker has joined.
  // A synchronous cross-thread paint would deadlock this test.
  std::thread worker([&] {
    ui.style().font_face = L"Microsoft YaHei";
    ui.style().font_point = 16;
    ui.UpdateInputPosition(RECT{100, 100, 105, 120});
    ui.Update(ctx, status);
    ui.Show();
  });
  worker.join();
  check(!IsWindowVisible(ui.hwnd()), "publishing does not wait for UI thread");
  pump();
  DWORD pid = 0;
  DWORD tid = GetWindowThreadProcessId(ui.hwnd(), &pid);
  check(pid == GetCurrentProcessId() && tid == GetCurrentThreadId(),
        "candidate window belongs to presenter thread");
  check(ui.IsShown(), "published candidates become visible");
  std::thread hide([&] {
    ui.Show();
    ui.Hide();
  });
  hide.join();
  pump();
  check(!ui.IsShown(), "latest hide wins over queued show");
  {
    weasel::UI::UpdateBatch batch(ui);
    ui.Show();
    pump();
    check(!ui.IsShown(), "partial batch is never published to presenter");
    ui.Hide();
  }
  pump();
  check(!ui.IsShown(), "complete batch publishes its final visibility");
  ui.Show();
  pump();
  DestroyWindow(owner);
  pump(350);
  check(!ui.IsShown(), "destroyed client endpoint hides candidates");
  ui.SetLifetimeWindow(nullptr);
  ui.ShowWithTimeout(100);
  pump(350);
  check(!ui.IsShown(),
        "notification timeout survives asynchronous presentation");
  ui.Destroy(true);
}
int main() {
  CoInitialize(nullptr);
  try {
    test_ui();
    CoUninitialize();
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << std::endl;
    return 1;
  }
}
