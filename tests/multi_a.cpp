#if defined(_WIN32)
#include <windows.h>
#endif
#define LOG_ACTIVE_LEVEL ::log::Level::Error
#include "logger/logger_include.h"

void *address_a() { return &log::instance(); }
int disabled_callsite() {
  int calls = 0;
  LOG_INFO("{}", ++calls);
  return calls;
}
