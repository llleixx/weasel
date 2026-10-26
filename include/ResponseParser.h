#pragma once
#include <WeaselIPCData.h>
#include <windows.h>
#include <map>
#include <memory>
#include <string>

namespace weasel {
class Deserializer;

// Owned data for a deferred TSF edit. Pipe buffers are reused by every IPC
// call, including candidate visibility and caret-position updates.
struct ResponseSnapshot {
  std::wstring commit;
  Context context;
  Status status;
  Config config;
  UIStyle style;
  DWORD ui_revision = 0;
  bool Parse(LPWSTR buffer, UINT length);
};

// 解析server回應文本
struct ResponseParser {
  std::map<std::wstring, std::shared_ptr<Deserializer> > deserializers;

  std::wstring* p_commit;
  Context* p_context;
  Status* p_status;
  Config* p_config;
  UIStyle* p_style;
  DWORD* p_ui_revision;

  ResponseParser(std::wstring* commit,
                 Context* context = 0,
                 Status* status = 0,
                 Config* config = 0,
                 UIStyle* style = 0,
                 DWORD* ui_revision = nullptr);

  // 重載函數調用運算符, 以扮做ResponseHandler
  bool operator()(LPWSTR buffer, UINT length);

  // 處理一行回應文本
  void Feed(const std::wstring& line);
};

}  // namespace weasel
