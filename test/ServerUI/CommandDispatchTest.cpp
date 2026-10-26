// Exercise both production entry points without starting the named pipe or
// connecting to a user's running input method. Including the implementation
// gives this regression access to the same non-recursive dispatch mutex.
#include "../../WeaselIPCServer/WeaselServerImpl.cpp"
#include <iostream>

CAppModule _Module;

class Handler : public weasel::RequestHandler {
 public:
  void SetOption(DWORD session, const std::string& name, bool value) override {
    if (session != 7 || name != "ascii_mode")
      std::abort();
    ++calls;
    ascii = value;
  }
  int calls = 0;
  bool ascii = false;
};

int main() {
  Handler handler;
  weasel::ServerImpl server;
  server.SetRequestHandler(&handler);
  BOOL handled = TRUE;
  server.OnCommand(WM_COMMAND, ID_WEASELTRAY_ENABLE_ASCII, 7, handled);
  if (!handled || handler.calls != 1 || !handler.ascii)
    return 1;
  {
    // HandlePipeMessage already owns this lock before dispatching a tray
    // command. Reacquiring it in OnCommand would deadlock here.
    std::lock_guard<std::mutex> lock(g_api_mutex);
    server.OnCommand(WEASEL_IPC_TRAY_COMMAND, ID_WEASELTRAY_DISABLE_ASCII, 7);
  }
  if (handler.calls != 2 || handler.ascii)
    return 1;
  std::cout << "PASS: native and IPC commands acquire the API lock once\n";
}
