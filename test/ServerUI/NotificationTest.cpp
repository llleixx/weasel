#include <windows.h>
#include <WeaselUI.h>
#include <iostream>

int main() {
  CoInitialize(nullptr);
  weasel::UI ui;
  if (!ui.CreateAsyncServer(nullptr))
    return 2;
  ui.ShowWithTimeout(1000);
  // _GetStatus may publish a notification, and _ShowMessage immediately
  // checks whether it is pending. It must not depend on the UI thread having
  // processed the snapshot yet.
  const bool pending = !!ui.IsCountingDown();
  ui.Hide();
  const bool cancelled = !ui.IsCountingDown();
  ui.Destroy(true);
  CoUninitialize();
  if (!pending || !cancelled) {
    std::cerr << "FAIL: pending/cancelled timeout state before message pump\n";
    return 1;
  }
  std::cout
      << "PASS: timeout state is published synchronously with the request\n";
}
