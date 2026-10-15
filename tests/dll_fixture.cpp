#include "logger/logger.h"

namespace {
struct LateLog {
  ~LateLog() { log::info("late destructor"); }
} late_log;
} // namespace

extern "C" int run_logger(const wchar_t *path, int asynchronous) noexcept {
  try {
    log::Config config;
    config.mode = asynchronous == 1 ? log::Mode::Asynchronous : log::Mode::Synchronous;
    if (!log::configure(config))
      return 0;
    log::ConsoleConfig console;
    console.auto_alloc = asynchronous == 2;
    console.show_date = console.show_time = console.show_file = false;
    console.enable_colors = false;
    if (!log::init_console(console))
      return 0;
    if (!log::add_file_sink(path, 0, 0))
      return 0;
    log::set_thread_name("existing host thread");
    return log::info({{"mode", asynchronous ? "async" : "sync"}}, "DLL record: Привет") ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

extern "C" int stop_logger() noexcept { return log::shutdown() ? 1 : 0; }

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
