#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace log_core::detail {

inline size_t utf8_length(std::string_view input, size_t offset) noexcept {
  const auto first = static_cast<unsigned char>(input[offset]);
  if (first < 0x80)
    return 1;
  size_t length = first >= 0xc2 && first <= 0xdf   ? 2
                  : first >= 0xe0 && first <= 0xef ? 3
                  : first >= 0xf0 && first <= 0xf4 ? 4
                                                   : 0;
  if (!length || length > input.size() - offset)
    return 0;
  for (size_t i = 1; i < length; ++i)
    if ((static_cast<unsigned char>(input[offset + i]) & 0xc0) != 0x80)
      return 0;
  const auto second = static_cast<unsigned char>(input[offset + 1]);
  if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0) ||
      (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90))
    return 0;
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

inline uint32_t next_utf8_codepoint(std::string_view input, size_t &offset) noexcept {
  const auto length = utf8_length(input, offset);
  if (!length) {
    ++offset;
    return 0xfffd;
  }
  uint32_t value = static_cast<unsigned char>(input[offset]);
  if (length > 1)
    value &= (1u << (7 - length)) - 1;
  for (size_t i = 1; i < length; ++i)
    value = (value << 6) | (static_cast<unsigned char>(input[offset + i]) & 0x3f);
  offset += length;
  return value;
}

inline bool sanitized_utf8_equal(std::string_view left, std::string_view right) noexcept {
  if (left == right)
    return true;
  size_t a = 0, b = 0;
  while (a < left.size() && b < right.size())
    if (next_utf8_codepoint(left, a) != next_utf8_codepoint(right, b))
      return false;
  return a == left.size() && b == right.size();
}

} // namespace log_core::detail
