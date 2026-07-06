#pragma once

#include "sink.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace log_core {

struct JsonSinkConfig {
  std::filesystem::path path;
  size_t max_size = 10 * 1024 * 1024; // 10 MB
  size_t max_files = 3;
};

class JsonSink : public Sink {
public:
  explicit JsonSink(JsonSinkConfig cfg);

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
};

} // namespace log_core
