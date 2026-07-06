#pragma once

#include <chrono>
#include <cstdint>
#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace log_core {

enum class Level : uint8_t { TRACE, DEBUG, INFO, WARN, ERROR };

struct kv {
  std::string key;
  std::string value;

  kv(std::string k, const char *v) : key(std::move(k)), value(v) {}
  kv(std::string k, std::string v) : key(std::move(k)), value(std::move(v)) {}
  kv(std::string k, std::string_view v) : key(std::move(k)), value(v) {}

  template <typename T>
    requires(!std::is_same_v<std::decay_t<T>, const char *> &&
             !std::is_same_v<std::decay_t<T>, std::string> &&
             !std::is_same_v<std::decay_t<T>, std::string_view>)
  kv(std::string k, T &&v)
      : key(std::move(k)), value(std::format("{}", std::forward<T>(v))) {}
};

struct ctx {
  std::vector<kv> pairs;
  ctx() = default;
  ctx(std::initializer_list<kv> list) : pairs(list) {}
  ctx(std::vector<kv> v) : pairs(std::move(v)) {}
};

template <typename T> struct is_ctx : std::false_type {};
template <> struct is_ctx<ctx> : std::true_type {};
template <> struct is_ctx<ctx &> : std::true_type {};
template <> struct is_ctx<const ctx &> : std::true_type {};
template <> struct is_ctx<ctx &&> : std::true_type {};
template <typename T>
inline constexpr bool is_ctx_v = is_ctx<std::remove_cvref_t<T>>::value;

struct LogMessage {
  Level level;
  std::chrono::system_clock::time_point time;
  uint32_t thread_id;
  std::source_location loc;
  std::string text;
  std::vector<kv> kv_pairs;
};

inline std::string_view level_to_string(Level level) noexcept {
  switch (level) {
  case Level::TRACE:
    return "TRC";
  case Level::DEBUG:
    return "DBG";
  case Level::INFO:
    return "INF";
  case Level::WARN:
    return "WRN";
  case Level::ERROR:
    return "ERR";
  default:
    return "UNK";
  }
}

inline std::string_view level_to_color(Level level) noexcept {
  switch (level) {
  case Level::TRACE:
    return "\x1b[38;2;140;140;140m"; // #8C8C8C
  case Level::DEBUG:
    return "\x1b[38;2;128;191;64m"; // #80BF40
  case Level::INFO:
    return "\x1b[38;2;51;166;204m"; // #33A6CC
  case Level::WARN:
    return "\x1b[38;2;204;191;51m"; // #CCBF33
  case Level::ERROR:
    return "\x1b[38;2;204;51;64m"; // #CC3340
  default:
    return "\x1b[0m";
  }
}

inline constexpr std::string_view reset_color = "\x1b[0m";

inline void get_time_info(std::chrono::system_clock::time_point tp,
                          struct tm &tm_info) {
  auto t_c = std::chrono::system_clock::to_time_t(tp);
#if defined(_WIN32)
  localtime_s(&tm_info, &t_c);
#else
  localtime_r(&t_c, &tm_info);
#endif
}

inline std::string_view shorten_filename(std::string_view filename) noexcept {
  if (auto pos = filename.find_last_of("/\\"); pos != std::string_view::npos) {
    return filename.substr(pos + 1);
  }
  return filename;
}

template <typename... Args> struct log_format_string {
  std::format_string<Args...> fmt;
  std::source_location loc;

  template <typename String>
  consteval log_format_string(
      const String &s,
      const std::source_location &l = std::source_location::current())
      : fmt(s), loc(l) {}
};

} // namespace log_core
