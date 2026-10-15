#include "json_sink.h"
#include "../detail/utf8.h"
#include <cmath>
#include <iterator>

namespace log_core::detail {

inline void json_escape_to(std::string &output, std::string_view value) {
  constexpr char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < value.size();) {
    const auto c = static_cast<unsigned char>(value[i]);
    if (c >= 0x80) {
      if (const auto length = utf8_length(value, i)) {
        output.append(value.substr(i, length)); i += length;
      } else { output += "\\ufffd"; ++i; }
      continue;
    }
    ++i;
    switch (c) {
    case '"': output += "\\\""; break;
    case '\\': output += "\\\\"; break;
    case '\n': output += "\\n"; break;
    case '\r': output += "\\r"; break;
    case '\t': output += "\\t"; break;
    default:
      if (c < 0x20) {
        output += "\\u00"; output += hex[c >> 4]; output += hex[c & 15];
      } else output += static_cast<char>(c);
    }
  }
}

inline void append_json_value(std::string &output, const FieldValue &value) {
  std::visit([&](const auto &stored) {
    using T = std::remove_cvref_t<decltype(stored)>;
    if constexpr (std::is_same_v<T, std::string>) {
      output += '"'; json_escape_to(output, stored); output += '"';
    } else if constexpr (std::is_same_v<T, bool>) {
      output += stored ? "true" : "false";
    } else if constexpr (std::is_floating_point_v<T>) {
      if (std::isfinite(stored)) std::format_to(std::back_inserter(output), "{}", stored);
      else output += "null";
    } else std::format_to(std::back_inserter(output), "{}", stored);
  }, value);
}

} // namespace log_core::detail

namespace log_core {

JsonSink::JsonSink(JsonSinkConfig config)
    : file_(std::move(config.path), config.max_size, config.max_files) {}

void JsonSink::write(const LogMessage &message) {
  auto &json = buffer_;
  json.clear();
  json += "{\"timestamp\":\"";
  json += format_iso_timestamp(message.time);
  json += "\",\"level\":\"";
  json += level_to_string(message.level);
  json += "\",\"file\":\"";
  detail::json_escape_to(json, shorten_filename(message.loc.file_name()));
  std::format_to(std::back_inserter(json), "\",\"line\":{},\"thread\":{},\"sequence\":{}",
                 message.loc.line(), message.thread_id, message.sequence);
  if (!message.thread_name.empty()) {
    json += ",\"thread_name\":\"";
    detail::json_escape_to(json, message.thread_name);
    json += '"';
  }
  json += ",\"message\":\"";
  detail::json_escape_to(json, message.text);
  json += "\",\"fields\":{";
  bool first = true;
  for (size_t i = 0; i < message.fields.size(); ++i) {
    const auto &field = message.fields[i];
    bool shadowed = false;
    for (size_t j = i + 1; j < message.fields.size(); ++j)
      shadowed |= field.key == message.fields[j].key;
    if (shadowed) continue; // Repeated user keys use the last value.
    if (!first) json += ',';
    first = false;
    json += '"';
    detail::json_escape_to(json, field.key);
    json += "\":";
    detail::append_json_value(json, field.value);
  }
  json += "}}\n";
  file_.write(json);
}

void JsonSink::flush() { file_.flush(); }

} // namespace log_core
