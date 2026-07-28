#include "file_sink.h"
#include "../core/types.h"

#include <format>

namespace log_core {

FileSink::FileSink(std::filesystem::path path, size_t max_size,
                   size_t max_files,
                   std::chrono::milliseconds flush_interval)
    : file_(std::move(path), max_size, max_files),
      flush_interval_(flush_interval) {}

void FileSink::write(const LogMessage &message) {
  const auto filename = shorten_filename(message.loc.file_name());
  const auto formatted = std::format(
      "[{}] [{}] [{}:{}]{} {}{}\n", format_timestamp(message.time),
      level_to_string(message.level), filename, message.loc.line(),
      message.thread_name.empty() ? std::string{} :
                                    std::format(" [{}]", message.thread_name),
      message.text, format_fields(message.fields));

  if (!file_.write(formatted))
    return;

  const auto now = std::chrono::steady_clock::now();
  if (now - last_flush_ >= flush_interval_) {
    file_.flush();
    last_flush_ = now;
  }
}

void FileSink::flush() { file_.flush(); }

} // namespace log_core
