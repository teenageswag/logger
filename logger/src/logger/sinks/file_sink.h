#pragma once

#include "sink.h"
#include "rotating_file.h"

#include <chrono>
#include <filesystem>

namespace log_core {

class FileSink final : public Sink {
public:
  explicit FileSink(
      std::filesystem::path path, size_t max_size = 5 * 1024 * 1024,
      size_t max_files = 1,
      std::chrono::milliseconds flush_interval =
          std::chrono::milliseconds(1000));

  void write(const LogMessage &message) override;
  void flush() override;

private:
  RotatingFile file_;
  std::chrono::milliseconds flush_interval_;
  std::chrono::steady_clock::time_point last_flush_ =
      std::chrono::steady_clock::now();
};

} // namespace log_core
