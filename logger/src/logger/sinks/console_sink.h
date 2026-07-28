#pragma once

#include "sink.h"

namespace log_core {

struct ConsoleSinkConfig {
  bool auto_alloc = true;
  std::string title;
  uint16_t buffer_width = 120;
  uint16_t buffer_height = 30;

  // Kept for source compatibility. ConsoleSink no longer redirects or closes
  // the host process' standard streams.
  bool redirect_streams = false;
  bool enable_colors = true;
  bool enable_backgrounds = false;
  bool force_ansi = false;
  bool free_owned_console = true;
  bool show_thread_name = true;
  bool show_thread_id = false;
};

class ConsoleSink final : public Sink {
public:
  explicit ConsoleSink(ConsoleSinkConfig config = {});
  ~ConsoleSink() override;

  ConsoleSink(const ConsoleSink &) = delete;
  ConsoleSink &operator=(const ConsoleSink &) = delete;

  void write(const LogMessage &message) override;
  void flush() override;

private:
  ConsoleSinkConfig config_;
  bool owns_console_ = false;
  bool ansi_enabled_ = false;

#if defined(_WIN32)
  void *stdout_handle_ = nullptr;
  void *stderr_handle_ = nullptr;
#endif
};

} // namespace log_core
