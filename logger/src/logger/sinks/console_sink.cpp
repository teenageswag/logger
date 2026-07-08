#include "console_sink.h"
#include "../core/logger.h"
#include "../core/types.h"

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

#include <cstdio>
#include <format>
#include <string>

namespace log_core {

ConsoleSink::ConsoleSink(ConsoleSinkConfig cfg) {
#if defined(_WIN32)
  if (cfg.auto_alloc && GetConsoleWindow() == NULL) {
    AllocConsole();
  }

  if (!cfg.title.empty()) {
    SetConsoleTitleA(cfg.title.c_str());
  }

  if (cfg.redirect_streams) {
    freopen_s(&stdout_file_, "CONOUT$", "w", stdout);
    freopen_s(&stderr_file_, "CONOUT$", "w", stderr);
    freopen_s(&stdin_file_, "CONIN$", "r", stdin);
  }

  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut != INVALID_HANDLE_VALUE) {
    COORD coord = {static_cast<SHORT>(cfg.buffer_width),
                   static_cast<SHORT>(cfg.buffer_height)};
    SetConsoleScreenBufferSize(hOut, coord);

    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
      SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
  }

  HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
  if (hErr != INVALID_HANDLE_VALUE) {
    DWORD dwMode = 0;
    if (GetConsoleMode(hErr, &dwMode)) {
      SetConsoleMode(hErr, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
  }

  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif
}

ConsoleSink::~ConsoleSink() {
#if defined(_WIN32)
  if (stdout_file_) {
    std::fclose(stdout_file_);
    stdout_file_ = nullptr;
  }
  if (stderr_file_) {
    std::fclose(stderr_file_);
    stderr_file_ = nullptr;
  }
  if (stdin_file_) {
    std::fclose(stdin_file_);
    stdin_file_ = nullptr;
  }
#endif
}

void ConsoleSink::write(const LogMessage &msg) {
  struct tm tm_info;
  get_time_info(msg.time, tm_info);

  std::string_view filename = shorten_filename(msg.loc.file_name());
  std::string kv_suffix = format_kv_pairs(msg.kv_pairs);

  // Thread name segment
  std::string thread_segment;
  const auto &tname = tls_thread_name;
  if (!tname.empty()) {
    thread_segment = std::format(" [{}]", tname);
  }

  std::string final_msg = std::format(
      "{}[{:02}:{:02}:{:02}] [{}] [{}:{}]{} {}{}{}\n",
      level_to_color(msg.level), tm_info.tm_hour, tm_info.tm_min,
      tm_info.tm_sec, level_to_string(msg.level), filename,
      msg.loc.line(), thread_segment, msg.text, kv_suffix, reset_color);

  if (msg.level >= Level::ERROR) {
    std::fputs(final_msg.c_str(), stderr);
  } else {
    std::fputs(final_msg.c_str(), stdout);
  }
}

void ConsoleSink::flush() {
  std::fflush(stdout);
  std::fflush(stderr);
}

} // namespace log_core
