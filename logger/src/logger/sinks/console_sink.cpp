#include "console_sink.h"
#include "../core/types.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <format>
#include <string>
#include <vector>

namespace log_core {

#if defined(_WIN32)
namespace {
bool valid_handle(HANDLE handle) {
  return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

HANDLE open_console_output() {
  return CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                     0, nullptr);
}

// Freshly attached/allocated conhost buffers don't have VT sequence
// processing enabled by default (unlike handles inherited from a modern
// terminal host). Without this, ANSI escape codes print as literal text
// instead of being interpreted as color/formatting.
void enable_vt_processing(HANDLE handle) {
  if (!valid_handle(handle))
    return;
  DWORD mode = 0;
  if (GetConsoleMode(handle, &mode))
    SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
} // namespace
#endif

ConsoleSink::ConsoleSink(ConsoleSinkConfig config)
    : config_(std::move(config)) {
#if defined(_WIN32)
  HANDLE std_output = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  bool has_console = valid_handle(std_output) &&
                     GetConsoleMode(std_output, &mode) != FALSE;

  if (!has_console && config_.auto_alloc) {
    if (AllocConsole() != FALSE) {
      owns_console_ = true;
      has_console = true;
    }
  }

  if (!config_.title.empty() && has_console)
    SetConsoleTitleA(config_.title.c_str());

  if (has_console) {
    HANDLE output = open_console_output();
    HANDLE error = open_console_output();
    stdout_handle_ = valid_handle(output) ? output : nullptr;
    stderr_handle_ = valid_handle(error) ? error : nullptr;

    if (valid_handle(output)) {
      COORD coord{static_cast<SHORT>(config_.buffer_width),
                  static_cast<SHORT>(config_.buffer_height)};
      SetConsoleScreenBufferSize(output, coord);
      SetConsoleOutputCP(CP_UTF8);
    }

    // Must be set on each CONOUT$ handle individually — stdout_handle_ and
    // stderr_handle_ are separate opens and don't share console mode state.
    enable_vt_processing(stdout_handle_);
    enable_vt_processing(stderr_handle_);
  }

  ansi_enabled_ = config_.enable_colors &&
                  (config_.force_ansi || has_console);
#else
  ansi_enabled_ = config_.enable_colors &&
                  (config_.force_ansi || isatty(fileno(stdout)) != 0);
#endif
}

ConsoleSink::~ConsoleSink() {
#if defined(_WIN32)
  if (valid_handle(static_cast<HANDLE>(stdout_handle_)))
    CloseHandle(static_cast<HANDLE>(stdout_handle_));
  if (valid_handle(static_cast<HANDLE>(stderr_handle_)))
    CloseHandle(static_cast<HANDLE>(stderr_handle_));
  stdout_handle_ = nullptr;
  stderr_handle_ = nullptr;

  // Only detach/free a console allocated by this sink. An injected DLL must
  // never detach the console owned by the host process.
  if (owns_console_ && config_.free_owned_console)
    FreeConsole();
#endif
}

void ConsoleSink::write(const LogMessage &message) {
  const auto fields = format_fields(message.fields);

  std::vector<std::string> segments;
  if (config_.show_date || config_.show_time) {
    std::string timestamp;
    if (config_.show_date)
      timestamp = format_date(message.time);
    if (config_.show_time) {
      if (!timestamp.empty())
        timestamp += ' ';
      timestamp += format_time(
          message.time,
          config_.time_format == ConsoleSinkConfig::TimeFormat::WithMilliseconds);
    }
    segments.push_back(std::format("[{}]", timestamp));
  }
  segments.push_back(std::format("[{}]", level_to_string(message.level)));
  if (config_.show_file) {
    segments.push_back(std::format("[{}:{}]",
                                   shorten_filename(message.loc.file_name()),
                                   message.loc.line()));
  }

  std::string thread_segment;
  if (config_.show_thread_name && !message.thread_name.empty())
    thread_segment = std::format(" [{}]", message.thread_name);
  if (config_.show_thread_id)
    thread_segment += std::format(" [tid={}]", message.thread_id);

  std::string ansi_prefix;
  if (ansi_enabled_) {
    if (config_.enable_colors)
      ansi_prefix += level_to_color(message.level);
    if (config_.enable_backgrounds)
      ansi_prefix += level_to_background_color(message.level);
  }

  std::string prefix = ansi_prefix;
  for (const auto &segment : segments) {
    prefix += segment;
    prefix += ' ';
  }

  const std::string final_message = std::format(
      "{}{}{}{}{}\n", prefix, thread_segment,
      thread_segment.empty() ? std::string{} : std::string{" "}, message.text,
      ansi_enabled_ ? std::string(reset_color) : std::string{});

#if defined(_WIN32)
  HANDLE handle = is_error_level(message.level)
                      ? static_cast<HANDLE>(stderr_handle_)
                      : static_cast<HANDLE>(stdout_handle_);
  if (valid_handle(handle)) {
    DWORD written = 0;
    WriteFile(handle, final_message.data(),
              static_cast<DWORD>(final_message.size()), &written, nullptr);
    return;
  }
#endif

  FILE *stream = is_error_level(message.level) ? stderr : stdout;
  std::fwrite(final_message.data(), 1, final_message.size(), stream);
}

void ConsoleSink::flush() {
#if defined(_WIN32)
  if (valid_handle(static_cast<HANDLE>(stdout_handle_)))
    FlushFileBuffers(static_cast<HANDLE>(stdout_handle_));
  if (valid_handle(static_cast<HANDLE>(stderr_handle_)))
    FlushFileBuffers(static_cast<HANDLE>(stderr_handle_));
#else
  std::fflush(stdout);
  std::fflush(stderr);
#endif
}

} // namespace log_core
