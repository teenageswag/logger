#pragma once

#include "sink.h"
#include "rotating_file.h"

#include <filesystem>

namespace log_core {

struct JsonSinkConfig {
  std::filesystem::path path;
  size_t max_size = 10 * 1024 * 1024;
  size_t max_files = 3;
};

class JsonSink final : public Sink {
public:
  explicit JsonSink(JsonSinkConfig config);

  void write(const LogMessage &message) override;
  void flush() override;

private:
  RotatingFile file_;
};

} // namespace log_core
