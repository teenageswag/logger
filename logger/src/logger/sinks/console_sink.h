#pragma once

#include "sink.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef ERROR
#endif

namespace log_core {

struct ConsoleSinkConfig {
  bool auto_alloc = true;
  std::string title = "";
  uint16_t buffer_width = 120;
  uint16_t buffer_height = 30;
  bool redirect_streams = true;
};

class ConsoleSink : public Sink {
public:
  explicit ConsoleSink(ConsoleSinkConfig cfg = {});
  ~ConsoleSink() override;

  ConsoleSink(const ConsoleSink &) = delete;
  ConsoleSink &operator=(const ConsoleSink &) = delete;

  void write(const LogMessage &msg) override;
  void flush() override;

private:
#if defined(_WIN32)
  FILE *stdout_file_ = nullptr;
  FILE *stderr_file_ = nullptr;
  FILE *stdin_file_ = nullptr;
#endif
};

} // namespace log_core
