#pragma once
#include <windows.h>
#include <algorithm>
#include <sstream>
#include <string>

namespace weasel {
inline std::wstring EncodeServerUIPosition(const RECT& rc) {
  return L"rect " + std::to_wstring(rc.left) + L" " + std::to_wstring(rc.top) +
         L" " + std::to_wstring(rc.right) + L" " + std::to_wstring(rc.bottom) +
         L"\n";
}

inline bool DecodeServerUIPosition(const wchar_t* buffer,
                                   size_t length,
                                   RECT& result) {
  if (!buffer || !length)
    return false;
  const size_t count = wcsnlen_s(buffer, (std::min)(length, size_t{128}));
  if (count == 128)
    return false;
  std::wistringstream input(std::wstring(buffer, count));
  std::wstring tag;
  RECT rc{};
  if (!(input >> tag >> rc.left >> rc.top >> rc.right >> rc.bottom) ||
      tag != L"rect")
    return false;
  input >> std::ws;
  if (!input.eof())
    return false;
  result = rc;
  return true;
}
}  // namespace weasel
