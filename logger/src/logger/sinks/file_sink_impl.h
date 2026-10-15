#pragma once

#include "../core/types.h"
#include "file_sink.h"

#include <format>

namespace log_core {

inline FileSink::FileSink(std::filesystem::path path, uint64_t max_size, size_t max_files,
                          std::chrono::milliseconds flush_interval)
    : file_(std::move(path), max_size, max_files), flush_interval_(flush_interval) {}

inline void FileSink::write(const LogMessage &message) {
  const auto filename = shorten_filename(message.loc.file_name());
  buffer_.clear();
  buffer_ += '[';
  append_local_timestamp(buffer_, message.time);
  std::format_to(std::back_inserter(buffer_), "] [{}] [{}:{}]", level_to_string(message.level),
                 filename, message.loc.line());
  if (!message.thread_name.empty())
    std::format_to(std::back_inserter(buffer_), " [{}]", message.thread_name);
  buffer_ += ' ';
  buffer_ += message.text;
  append_fields(buffer_, message.fields);
  buffer_ += '\n';
  file_.write(buffer_);

  const auto now = std::chrono::steady_clock::now();
  if (now - last_flush_ >= flush_interval_) {
    file_.flush();
    last_flush_ = now;
  }
}

inline void FileSink::flush() { file_.flush(); }

} // namespace log_core
