#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <format>
#include <initializer_list>
#include <iterator>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace log_core {

// PascalCase names intentionally avoid the ERROR macro from Windows.h.
enum class Level : uint8_t {
  Trace,
  Debug,
  Info,
  Success,
  Warn,
  Error,
  Critical,
  Off,
};

using FieldValue = std::variant<std::string, bool, int64_t, uint64_t, double>;

struct Field {
  std::string key;
  FieldValue value;

  Field(std::string k, const char *v) : key(std::move(k)), value(std::string(v ? v : "")) {}
  Field(std::string k, std::string v) : key(std::move(k)), value(std::move(v)) {}
  Field(std::string k, std::string_view v) : key(std::move(k)), value(std::string(v)) {}
  Field(std::string k, bool v) : key(std::move(k)), value(v) {}

  template <typename T>
    requires(std::is_integral_v<std::remove_cvref_t<T>> &&
             !std::is_same_v<std::remove_cvref_t<T>, bool>)
  Field(std::string k, T v) : key(std::move(k)), value(make_integer(v)) {}

  template <typename T>
    requires(std::is_floating_point_v<std::remove_cvref_t<T>>)
  Field(std::string k, T v) : key(std::move(k)), value(static_cast<double>(v)) {}

  template <typename T>
    requires(!std::is_integral_v<std::remove_cvref_t<T>> &&
             !std::is_floating_point_v<std::remove_cvref_t<T>> &&
             !std::is_same_v<std::remove_cvref_t<T>, const char *> &&
             !std::is_same_v<std::remove_cvref_t<T>, char *> &&
             !std::is_same_v<std::remove_cvref_t<T>, std::string> &&
             !std::is_same_v<std::remove_cvref_t<T>, std::string_view> &&
             std::formattable<const T &, char>)
  Field(std::string k, const T &v) : key(std::move(k)), value(std::format("{}", v)) {}

private:
  template <typename T> static FieldValue make_integer(T value) {
    if constexpr (std::is_signed_v<T>)
      return static_cast<int64_t>(value);
    else
      return static_cast<uint64_t>(value);
  }
};

struct Fields {
  std::vector<Field> values;

  Fields() = default;
  Fields(std::initializer_list<Field> list) : values(list) {}
  explicit Fields(std::vector<Field> v) : values(std::move(v)) {}
};

// Compatibility aliases for the original API.
using kv = Field;
using ctx = Fields;

template <typename T> struct is_fields : std::false_type {};
template <> struct is_fields<Fields> : std::true_type {};
template <> struct is_fields<Fields &> : std::true_type {};
template <> struct is_fields<const Fields &> : std::true_type {};
template <> struct is_fields<Fields &&> : std::true_type {};
template <typename T> inline constexpr bool is_fields_v = is_fields<std::remove_cvref_t<T>>::value;

struct LogMessage {
  Level level = Level::Info;
  std::chrono::system_clock::time_point time{};
  uint32_t thread_id = 0;
  std::string thread_name;
  std::source_location loc;
  std::string text;
  std::vector<Field> fields;
  uint64_t sequence = 0;
  size_t payload_bytes = 0;

  // Compatibility name used by older sink implementations.
  std::vector<Field> &kv_pairs() { return fields; }
  const std::vector<Field> &kv_pairs() const { return fields; }
};

inline constexpr bool is_error_level(Level level) noexcept {
  return level == Level::Error || level == Level::Critical;
}

inline std::string_view level_to_string(Level level) noexcept {
  switch (level) {
  case Level::Trace:
    return "TRC";
  case Level::Debug:
    return "DBG";
  case Level::Info:
    return "INF";
  case Level::Success:
    return "SUC";
  case Level::Warn:
    return "WRN";
  case Level::Error:
    return "ERR";
  case Level::Critical:
    return "CRT";
  case Level::Off:
    return "OFF";
  }
  return "UNK";
}

inline std::string_view level_to_color(Level level) noexcept {
  switch (level) {
  case Level::Trace:
    return "\x1b[38;2;148;163;184m";
  case Level::Debug:
    return "\x1b[38;2;167;139;250m";
  case Level::Info:
    return "\x1b[38;2;96;165;250m";
  case Level::Success:
    return "\x1b[38;2;52;211;153m";
  case Level::Warn:
    return "\x1b[38;2;251;191;36m";
  case Level::Error:
    return "\x1b[38;2;248;113;113m";
  case Level::Critical:
    return "\x1b[38;2;255;153;153m";
  case Level::Off:
    break;
  }
  return "\x1b[0m";
}

inline std::string_view level_to_background_color(Level level) noexcept {
  switch (level) {
  case Level::Trace:
    return "\x1b[48;2;51;65;85m";
  case Level::Debug:
    return "\x1b[48;2;76;29;149m";
  case Level::Info:
    return "\x1b[48;2;30;58;138m";
  case Level::Success:
    return "\x1b[48;2;6;95;70m";
  case Level::Warn:
    return "\x1b[48;2;120;53;15m";
  case Level::Error:
    return "\x1b[48;2;127;29;29m";
  case Level::Critical:
    return "\x1b[48;2;153;27;27m";
  case Level::Off:
    break;
  }
  return "\x1b[0m";
}

inline constexpr std::string_view reset_color = "\x1b[0m";

inline bool get_time_info(std::chrono::system_clock::time_point tp, struct tm &tm_info) noexcept {
  const auto time =
      std::chrono::system_clock::to_time_t(std::chrono::floor<std::chrono::seconds>(tp));
#if defined(_WIN32)
  return localtime_s(&tm_info, &time) == 0;
#else
  return localtime_r(&time, &tm_info) != nullptr;
#endif
}

inline std::tm checked_local_time(std::chrono::system_clock::time_point tp) {
  std::tm result{};
  if (!get_time_info(tp, result))
    throw std::range_error("log timestamp outside localtime range");
  return result;
}

inline auto milliseconds_part(std::chrono::system_clock::time_point tp) noexcept {
  return (std::chrono::floor<std::chrono::milliseconds>(tp) -
          std::chrono::floor<std::chrono::seconds>(tp))
      .count();
}

inline void append_local_timestamp(std::string &output, std::chrono::system_clock::time_point tp,
                                   bool show_date = true, bool show_time = true,
                                   bool with_milliseconds = true, char separator = '.') {
  const auto tm = checked_local_time(tp);
  auto out = std::back_inserter(output);
  if (show_date)
    std::format_to(out, "{:04}-{:02}-{:02}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  if (show_date && show_time)
    output += ' ';
  if (show_time) {
    std::format_to(out, "{:02}:{:02}:{:02}", tm.tm_hour, tm.tm_min, tm.tm_sec);
    if (with_milliseconds)
      std::format_to(out, "{}{:03}", separator, milliseconds_part(tp));
  }
}

inline std::string format_date(std::chrono::system_clock::time_point tp) {
  std::string result;
  append_local_timestamp(result, tp, true, false);
  return result;
}

inline std::string format_time(std::chrono::system_clock::time_point tp,
                               bool with_milliseconds = true) {
  std::string result;
  append_local_timestamp(result, tp, false, true, with_milliseconds, ':');
  return result;
}

inline std::string format_timestamp(std::chrono::system_clock::time_point tp) {
  std::string result;
  append_local_timestamp(result, tp);
  return result;
}

inline void append_iso_timestamp(std::string &output, std::chrono::system_clock::time_point tp) {
  using namespace std::chrono;
  // Calendar arithmetic handles pre-epoch timestamps without the CRT localtime limits.
  const auto ms = floor<milliseconds>(tp.time_since_epoch());
  const auto day = floor<days>(ms);
  const year_month_day date{sys_days{day}};
  const hh_mm_ss time{ms - day};
  std::format_to(std::back_inserter(output), "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z",
                 static_cast<int>(date.year()), static_cast<unsigned>(date.month()),
                 static_cast<unsigned>(date.day()), time.hours().count(), time.minutes().count(),
                 time.seconds().count(), time.subseconds().count());
}

inline std::string format_iso_timestamp(std::chrono::system_clock::time_point tp) {
  std::string result;
  append_iso_timestamp(result, tp);
  return result;
}

inline std::string_view shorten_filename(std::string_view filename) noexcept {
  if (const auto pos = filename.find_last_of("/\\"); pos != std::string_view::npos)
    return filename.substr(pos + 1);
  return filename;
}

inline std::string field_value_to_string(const FieldValue &value) {
  return std::visit(
      [](const auto &v) -> std::string {
        using T = std::remove_cvref_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>)
          return v;
        else if constexpr (std::is_same_v<T, bool>)
          return v ? "true" : "false";
        else
          return std::format("{}", v);
      },
      value);
}

inline void append_fields(std::string &output, const std::vector<Field> &fields) {
  if (fields.empty())
    return;
  output += " {";
  bool first = true;
  for (const auto &field : fields) {
    if (!first)
      output += ", ";
    first = false;
    output += field.key;
    output += '=';
    std::visit(
        [&](const auto &value) {
          using T = std::remove_cvref_t<decltype(value)>;
          if constexpr (std::is_same_v<T, std::string>)
            output += value;
          else
            std::format_to(std::back_inserter(output), "{}", value);
        },
        field.value);
  }
  output += '}';
}

inline std::string format_fields(const std::vector<Field> &fields) {
  std::string result;
  append_fields(result, fields);
  return result;
}

inline std::string format_kv_pairs(const std::vector<Field> &fields) {
  return format_fields(fields);
}

template <typename... Args> struct log_format_string {
  std::format_string<Args...> fmt;
  std::source_location loc;

  template <typename String>
  consteval log_format_string(const String &s,
                              const std::source_location &l = std::source_location::current())
      : fmt(s), loc(l) {}
};

} // namespace log_core
