#pragma once

#include "sink.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace log_core {

class FileSink : public Sink {
public:
  explicit FileSink(std::filesystem::path path,
                    size_t max_size = 5 * 1024 * 1024,
                    size_t max_files = 1,
                    std::chrono::milliseconds flush_interval =
                        std::chrono::milliseconds(1000));

  void write(const LogMessage &msg) override;
  void flush() override;

private:
  void open_file();
  void rotate();

  std::filesystem::path path_;
  std::ofstream file_;
  size_t max_size_;
  size_t max_files_;
  size_t current_size_ = 0;
  std::chrono::milliseconds flush_interval_;
  std::chrono::steady_clock::time_point last_flush_ =
      std::chrono::steady_clock::now();
};

} // namespace log_core
