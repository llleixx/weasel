#pragma once
#include <windows.h>
#include <string>

namespace weasel {
// Revision zero is reserved for legacy responses and hidden UI.
inline bool ParseServerUIRevision(const std::wstring& text, DWORD& revision) {
  if (text.empty() || text.size() > 10)
    return false;
  unsigned long long value = 0;
  for (wchar_t c : text) {
    if (c < L'0' || c > L'9')
      return false;
    value = value * 10 + (c - L'0');
  }
  if (value > MAXDWORD)
    return false;
  revision = static_cast<DWORD>(value);
  return true;
}
class ServerUIRevision {
 public:
  DWORD Advance() {
    if (!++current_)
      ++current_;
    return current_;
  }
  bool Acknowledge(DWORD revision) {
    if (!revision || revision != current_)
      return false;
    acknowledged_ = revision;
    return true;
  }
  bool pending() const { return current_ != acknowledged_; }

 private:
  DWORD current_ = 0;
  DWORD acknowledged_ = 0;
};
}  // namespace weasel
