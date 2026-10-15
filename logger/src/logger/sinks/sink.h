#pragma once

#include "../core/types.h"
#include <atomic>

namespace log_core {

class Sink {
public:
  virtual ~Sink() = default;
  virtual void write(const LogMessage &msg) = 0;
  virtual void flush() = 0;
  virtual bool is_console() const noexcept { return false; }
  void set_level(Level level) noexcept { level_.store(level, std::memory_order_relaxed); }
  bool enabled(Level level) const noexcept {
    return level < Level::Off && level >= level_.load(std::memory_order_relaxed);
  }

private:
  std::atomic<Level> level_{Level::Trace};
};

} // namespace log_core
