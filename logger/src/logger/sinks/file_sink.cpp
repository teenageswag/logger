#include "file_sink.h"
#include "../core/types.h"

#include <filesystem>
#include <format>
#include <fstream>

namespace log_core {

FileSink::FileSink(std::filesystem::path path, size_t max_size,
                   size_t max_files,
                   std::chrono::milliseconds flush_interval)
    : path_(std::move(path)), max_size_(max_size), max_files_(max_files),
      flush_interval_(flush_interval) {
  open_file();
}

void FileSink::open_file() {
  file_.open(path_, std::ios::app);
  if (file_.is_open()) {
    file_.seekp(0, std::ios::end);
    current_size_ = static_cast<size_t>(file_.tellp());
  }
}

void FileSink::rotate() {
  if (file_.is_open())
    file_.close();

  std::error_code ec;

  // Remove the oldest backup if we've reached max_files
  if (max_files_ > 0) {
    auto oldest = path_;
    oldest += std::format(".{}", max_files_);
    if (std::filesystem::exists(oldest, ec)) {
      std::filesystem::remove(oldest, ec);
    }
  }

  // Shift existing backups: .N-1 → .N, .N-2 → .N-1, ..., .1 → .2
  for (size_t i = max_files_; i >= 2; --i) {
    auto src = path_;
    src += std::format(".{}", i - 1);
    auto dst = path_;
    dst += std::format(".{}", i);
    if (std::filesystem::exists(src, ec)) {
      std::filesystem::rename(src, dst, ec);
    }
  }

  // Current file becomes .1
  auto rotated_path = path_;
  rotated_path += ".1";
  if (std::filesystem::exists(path_, ec)) {
    std::filesystem::rename(path_, rotated_path, ec);
  }

  open_file();
}

void FileSink::write(const LogMessage &msg) {
  if (!file_.is_open())
    return;

  struct tm tm_info;
  get_time_info(msg.time, tm_info);

  std::string_view filename = shorten_filename(msg.loc.file_name());
  std::string kv_suffix = format_kv_pairs(msg.kv_pairs);

  std::string formatted = std::format(
      "[{:04}-{:02}-{:02} {:02}:{:02}:{:02}] [{}] [{}:{}] {}{}\n",
      tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
      tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
      level_to_string(msg.level), filename, msg.loc.line(), msg.text,
      kv_suffix);

  if (current_size_ + formatted.size() > max_size_) {
    rotate();
    if (!file_.is_open())
      return;
  }

  file_.write(formatted.data(), formatted.size());
  current_size_ += formatted.size();

  // Periodic flush
  auto now = std::chrono::steady_clock::now();
  if (now - last_flush_ >= flush_interval_) {
    file_.flush();
    last_flush_ = now;
  }
}

void FileSink::flush() {
  if (file_.is_open())
    file_.flush();
}

} // namespace log_core
