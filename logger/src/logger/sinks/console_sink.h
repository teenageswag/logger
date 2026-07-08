#pragma once

#include "sink.h"

#if defined(_WIN32)
// Forward-declare Windows types to avoid including <windows.h> in headers,
// which redefines the ERROR macro and breaks Level::ERROR.
extern "C" {
struct _iobuf;
typedef struct _iobuf FILE;
}
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
