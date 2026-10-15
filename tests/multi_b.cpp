#define LOG_ACTIVE_LEVEL ::log::Level::Trace
#include "logger/logger.h"
#if defined(_WIN32)
#include <windows.h>
#endif
void *address_b() { return &log::instance(); }
int enabled_callsite() {
  int calls = 0;
  LOG_INFO("{}", ++calls);
  return calls;
}
