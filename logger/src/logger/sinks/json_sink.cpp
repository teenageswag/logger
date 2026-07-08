#include "json_sink.h"
#include "../core/types.h"

#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace log_core {

// Escape a string for JSON, appending directly to output
static void json_escape_to(std::string &out, std::string_view s) {
  for (char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\u%04x",
                      static_cast<unsigned int>(static_cast<unsigned char>(c)));
        out += buf;
      } else {
        out += c;
      }
    }
  }
}

JsonSink::JsonSink(JsonSinkConfig cfg)
    : path_(std::move(cfg.path)), max_size_(cfg.max_size),
      max_files_(cfg.max_files) {
  open_file();
}

void JsonSink::open_file() {
  file_.open(path_, std::ios::app);
  if (file_.is_open()) {
    file_.seekp(0, std::ios::end);
    current_size_ = static_cast<size_t>(file_.tellp());
  }
}

void JsonSink::rotate() {
  if (file_.is_open())
    file_.close();

  std::error_code ec;

  if (max_files_ > 0) {
    auto oldest = path_;
    oldest += std::format(".{}", max_files_);
    if (std::filesystem::exists(oldest, ec)) {
      std::filesystem::remove(oldest, ec);
    }
  }

  for (size_t i = max_files_; i >= 2; --i) {
    auto src = path_;
    src += std::format(".{}", i - 1);
    auto dst = path_;
    dst += std::format(".{}", i);
    if (std::filesystem::exists(src, ec)) {
      std::filesystem::rename(src, dst, ec);
    }
  }

  auto rotated_path = path_;
  rotated_path += ".1";
  if (std::filesystem::exists(path_, ec)) {
    std::filesystem::rename(path_, rotated_path, ec);
  }

  open_file();
}

void JsonSink::write(const LogMessage &msg) {
  if (!file_.is_open())
    return;

  struct tm tm_info;
  get_time_info(msg.time, tm_info);

  // Format timestamp as ISO 8601
  char timestamp_buf[32];
  std::snprintf(timestamp_buf, sizeof(timestamp_buf),
                "%04d-%02d-%02dT%02d:%02d:%02d", tm_info.tm_year + 1900,
                tm_info.tm_mon + 1, tm_info.tm_mday, tm_info.tm_hour,
                tm_info.tm_min, tm_info.tm_sec);

  std::string_view filename = shorten_filename(msg.loc.file_name());

  // Build JSON object
  std::string json;
  json.reserve(256);
  json += "{\"timestamp\":\"";
  json += timestamp_buf;
  json += "\",\"level\":\"";
  json += level_to_string(msg.level);
  json += "\",\"file\":\"";
  json_escape_to(json, filename);
  json += "\",\"line\":";
  json += std::to_string(msg.loc.line());
  json += ",\"thread\":";
  json += std::to_string(msg.thread_id);
  json += ",\"message\":\"";
  json_escape_to(json, msg.text);
  json += "\"";

  // Add kv pairs as top-level fields
  for (const auto &kv : msg.kv_pairs) {
    json += ",\"";
    json_escape_to(json, kv.key);
    json += "\":\"";
    json_escape_to(json, kv.value);
    json += "\"";
  }

  json += "}\n";

  if (current_size_ + json.size() > max_size_) {
    rotate();
    if (!file_.is_open())
      return;
  }

  file_.write(json.data(), json.size());
  current_size_ += json.size();
}

void JsonSink::flush() {
  if (file_.is_open())
    file_.flush();
}

} // namespace log_core
