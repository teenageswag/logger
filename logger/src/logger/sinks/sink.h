#pragma once

#include "../core/types.h"

namespace log_core {

class Sink {
public:
  virtual ~Sink() = default;
  virtual void write(const LogMessage &msg) = 0;
  virtual void flush() = 0;
};

} // namespace log_core
