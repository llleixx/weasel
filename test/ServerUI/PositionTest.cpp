#include <ServerUIPosition.h>
#include <iostream>

int main() {
  RECT source{-15360, -8640, 16384, 9216};
  auto wire = weasel::EncodeServerUIPosition(source);
  RECT decoded{};
  if (!weasel::DecodeServerUIPosition(wire.data(), wire.size(), decoded) ||
      decoded.left != source.left || decoded.top != source.top ||
      decoded.right != source.right || decoded.bottom != source.bottom)
    return 1;
  for (const auto* invalid : {L"rect 1 2 3", L"rect 2147483648 0 0 0",
                              L"rect 0 0 0 0 trailing", L"other 0 0 0 0"}) {
    if (weasel::DecodeServerUIPosition(invalid, wcslen(invalid), decoded))
      return 2;
  }
  if (weasel::DecodeServerUIPosition(nullptr, 0, decoded))
    return 3;
  std::cout
      << "PASS: full signed screen coordinates and invalid input rejection\n";
}
