#include "json_sink.h"
#include "../core/types.h"

#include <cmath>
#include <format>

namespace log_core {
namespace {

void json_escape_to(std::string &output, std::string_view value) {
  for (const char character : value) {
    switch (character) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(character) < 0x20)
        output += std::format("\\u{:04x}", static_cast<unsigned int>(
                                             static_cast<unsigned char>(character)));
      else
        output += character;
    }
  }
}

void append_json_value(std::string &output, const FieldValue &value) {
  std::visit(
      [&output](const auto &stored) {
        using T = std::remove_cvref_t<decltype(stored)>;
        if constexpr (std::is_same_v<T, std::string>) {
          output += '"';
          json_escape_to(output, stored);
          output += '"';
        } else if constexpr (std::is_same_v<T, bool>) {
          output += stored ? "true" : "false";
        } else if constexpr (std::is_floating_point_v<T>) {
          if (std::isfinite(stored))
            output += std::format("{}", stored);
          else
            output += "null";
        } else {
          output += std::format("{}", stored);
        }
      },
      value);
}

} // namespace

JsonSink::JsonSink(JsonSinkConfig config)
    : file_(std::move(config.path), config.max_size, config.max_files) {}

void JsonSink::write(const LogMessage &message) {
  std::string json;
  json.reserve(320);
  json += "{\"timestamp\":\"";
  json_escape_to(json, format_iso_timestamp(message.time));
  json += "\",\"level\":\"";
  json += level_to_string(message.level);
  json += "\",\"file\":\"";
  json_escape_to(json, shorten_filename(message.loc.file_name()));
  json += "\",\"line\":";
  json += std::to_string(message.loc.line());
  json += ",\"thread\":";
  json += std::to_string(message.thread_id);
  if (!message.thread_name.empty()) {
    json += ",\"thread_name\":\"";
    json_escape_to(json, message.thread_name);
    json += '"';
  }
  json += ",\"message\":\"";
  json_escape_to(json, message.text);
  json += '"';

  for (const auto &field : message.fields) {
    json += ",\"";
    json_escape_to(json, field.key);
    json += "\":";
    append_json_value(json, field.value);
  }
  json += "}\n";
  file_.write(json);
}

void JsonSink::flush() { file_.flush(); }

} // namespace log_core
