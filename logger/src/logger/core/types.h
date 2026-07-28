#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <format>
#include <initializer_list>
#include <source_location>
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
};

using FieldValue =
    std::variant<std::string, bool, int64_t, uint64_t, double>;

struct Field {
  std::string key;
  FieldValue value;

  Field(std::string k, const char *v) : key(std::move(k)), value(std::string(v ? v : "")) {}
  Field(std::string k, std::string v)
      : key(std::move(k)), value(std::move(v)) {}
  Field(std::string k, std::string_view v)
      : key(std::move(k)), value(std::string(v)) {}
  Field(std::string k, bool v) : key(std::move(k)), value(v) {}

  template <typename T>
    requires(std::is_integral_v<std::remove_cvref_t<T>> &&
             !std::is_same_v<std::remove_cvref_t<T>, bool>)
  Field(std::string k, T v) : key(std::move(k)), value(make_integer(v)) {}

  template <typename T>
    requires(std::is_floating_point_v<std::remove_cvref_t<T>>)
  Field(std::string k, T v)
      : key(std::move(k)), value(static_cast<double>(v)) {}

  template <typename T>
    requires(!std::is_integral_v<std::remove_cvref_t<T>> &&
             !std::is_floating_point_v<std::remove_cvref_t<T>> &&
             !std::is_same_v<std::remove_cvref_t<T>, const char *> &&
             !std::is_same_v<std::remove_cvref_t<T>, char *> &&
             !std::is_same_v<std::remove_cvref_t<T>, std::string> &&
             !std::is_same_v<std::remove_cvref_t<T>, std::string_view> &&
             requires(const T &v) { std::format("{}", v); })
  Field(std::string k, const T &v)
      : key(std::move(k)), value(std::format("{}", v)) {}

private:
  template <typename T>
  static FieldValue make_integer(T value) {
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
template <typename T>
inline constexpr bool is_fields_v = is_fields<std::remove_cvref_t<T>>::value;

struct LogMessage {
  Level level = Level::Info;
  std::chrono::system_clock::time_point time{};
  uint32_t thread_id = 0;
  std::string thread_name;
  std::source_location loc;
  std::string text;
  std::vector<Field> fields;

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
  }
  return "UNK";
}

inline std::string_view level_to_color(Level level) noexcept {
  switch (level) {
  case Level::Trace:
    return "\x1b[38;2;140;140;140m";
  case Level::Debug:
    return "\x1b[38;2;128;191;64m";
  case Level::Info:
    return "\x1b[38;2;51;166;204m";
  case Level::Success:
    return "\x1b[38;2;76;175;80m";
  case Level::Warn:
    return "\x1b[38;2;204;191;51m";
  case Level::Error:
    return "\x1b[38;2;204;51;64m";
  case Level::Critical:
    return "\x1b[38;2;255;255;255m";
  }
  return "\x1b[0m";
}

inline std::string_view level_to_background_color(Level level) noexcept {
  switch (level) {
  case Level::Trace:
    return "\x1b[48;2;55;55;55m";
  case Level::Debug:
    return "\x1b[48;2;24;72;24m";
  case Level::Info:
    return "\x1b[48;2;18;58;78m";
  case Level::Success:
    return "\x1b[48;2;20;75;32m";
  case Level::Warn:
    return "\x1b[48;2;90;75;18m";
  case Level::Error:
    return "\x1b[48;2;95;20;28m";
  case Level::Critical:
    return "\x1b[48;2;155;20;20m";
  }
  return "\x1b[0m";
}

inline constexpr std::string_view reset_color = "\x1b[0m";

inline bool get_time_info(std::chrono::system_clock::time_point tp,
                          struct tm &tm_info) noexcept {
  const auto time = std::chrono::system_clock::to_time_t(tp);
#if defined(_WIN32)
  return localtime_s(&tm_info, &time) == 0;
#else
  return localtime_r(&time, &tm_info) != nullptr;
#endif
}

inline std::string format_timestamp(
    std::chrono::system_clock::time_point tp) {
  struct tm tm_info {};
  get_time_info(tp, tm_info);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      tp.time_since_epoch()) %
                  1000;
  return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
                     tm_info.tm_year + 1900, tm_info.tm_mon + 1,
                     tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min,
                     tm_info.tm_sec, ms.count());
}

inline std::string format_iso_timestamp(
    std::chrono::system_clock::time_point tp) {
  struct tm tm_info {};
  get_time_info(tp, tm_info);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      tp.time_since_epoch()) %
                  1000;
  return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}",
                     tm_info.tm_year + 1900, tm_info.tm_mon + 1,
                     tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min,
                     tm_info.tm_sec, ms.count());
}

inline std::string_view shorten_filename(std::string_view filename) noexcept {
  if (const auto pos = filename.find_last_of("/\\");
      pos != std::string_view::npos)
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

inline std::string format_fields(const std::vector<Field> &fields) {
  if (fields.empty())
    return {};

  std::string result = " {";
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i > 0)
      result += ", ";
    result += fields[i].key;
    result += '=';
    result += field_value_to_string(fields[i].value);
  }
  result += '}';
  return result;
}

inline std::string format_kv_pairs(const std::vector<Field> &fields) {
  return format_fields(fields);
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
