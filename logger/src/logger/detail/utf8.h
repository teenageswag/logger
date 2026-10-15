#pragma once
#include <string>
#include <string_view>

namespace log_core::detail {

inline size_t utf8_length(std::string_view input, size_t offset) noexcept {
  const auto first = static_cast<unsigned char>(input[offset]);
  if (first < 0x80) return 1;
  size_t length = first >= 0xc2 && first <= 0xdf ? 2 :
                  first >= 0xe0 && first <= 0xef ? 3 :
                  first >= 0xf0 && first <= 0xf4 ? 4 : 0;
  if (!length || length > input.size() - offset) return 0;
  for (size_t i = 1; i < length; ++i)
    if ((static_cast<unsigned char>(input[offset + i]) & 0xc0) != 0x80) return 0;
  const auto second = static_cast<unsigned char>(input[offset + 1]);
  if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0) ||
      (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90)) return 0;
  return length;
}

inline std::string sanitize_utf8(std::string_view input) {
  std::string output;
  output.reserve(input.size());
  for (size_t i = 0; i < input.size();) {
    if (const auto length = utf8_length(input, i)) {
      output.append(input.substr(i, length));
      i += length;
    } else {
      output += "\xef\xbf\xbd";
      ++i;
    }
  }
  return output;
}

} // namespace log_core::detail
