#include "console_sink.h"
#include "../detail/utf8.h"

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

#include <cerrno>
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <system_error>

namespace log_core {
namespace detail {
#if defined(_WIN32)
inline bool valid_handle(HANDLE handle) noexcept {
  return handle && handle != INVALID_HANDLE_VALUE;
}
inline void console_error(const char *operation) {
  throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
}
inline std::wstring utf8_to_wide(std::string_view text) {
  const auto clean = sanitize_utf8(text);
  if (clean.size() > static_cast<size_t>(INT_MAX)) throw std::length_error("console record too large");
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, clean.data(),
                                      static_cast<int>(clean.size()), nullptr, 0);
  if (!size && !clean.empty()) console_error("decode UTF-8");
  std::wstring wide(static_cast<size_t>(size), L'\0');
  if (size && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, clean.data(),
                                  static_cast<int>(clean.size()), wide.data(), size))
    console_error("decode UTF-8");
  return wide;
}
#endif
} // namespace detail

ConsoleSink::ConsoleSink(ConsoleSinkConfig config) : config_(std::move(config)) {
  if (config_.buffer_width > 32767 || config_.buffer_height > 32767)
    throw std::invalid_argument("console dimensions exceed SHORT");
#if defined(_WIN32)
  try {
    HANDLE probe = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    bool attached = detail::valid_handle(probe);
    if (attached) CloseHandle(probe);
    if (!attached && config_.auto_alloc) {
      const HANDLE old_input = GetStdHandle(STD_INPUT_HANDLE);
      const HANDLE old_output = GetStdHandle(STD_OUTPUT_HANDLE);
      const HANDLE old_error = GetStdHandle(STD_ERROR_HANDLE);
      if (!AllocConsole()) detail::console_error("allocate logger console");
      owns_console_ = attached = true;
      // AllocConsole initializes process standard handles. Preserve the host's table.
      const HANDLE allocated[] = {GetStdHandle(STD_INPUT_HANDLE),
          GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE)};
      SetStdHandle(STD_INPUT_HANDLE, old_input);
      SetStdHandle(STD_OUTPUT_HANDLE, old_output);
      SetStdHandle(STD_ERROR_HANDLE, old_error);
      for (size_t i = 0; i < 3; ++i) {
        if (!detail::valid_handle(allocated[i]) || allocated[i] == old_input ||
            allocated[i] == old_output || allocated[i] == old_error) continue;
        bool duplicate = false;
        for (size_t j = 0; j < i; ++j) duplicate |= allocated[i] == allocated[j];
        if (!duplicate) CloseHandle(allocated[i]);
      }
    }
    const auto open_output = [&](DWORD channel, void *&target, bool &ansi) {
      DWORD mode = 0;
      if (!owns_console_ && !GetConsoleMode(GetStdHandle(channel), &mode)) {
        ansi = config_.force_ansi;
        return;
      }
      HANDLE handle = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
      if (!detail::valid_handle(handle)) detail::console_error("open logger console");
      target = handle;
      if (!GetConsoleMode(handle, &mode)) detail::console_error("get console mode");
      if ((owns_console_ || config_.modify_host_console) &&
          (config_.enable_colors || config_.enable_backgrounds)) {
        // Output mode belongs to the screen buffer, shared by CONOUT$ handles.
        if (SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT))
          mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      }
      ansi = config_.force_ansi || (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
    };
    open_output(STD_OUTPUT_HANDLE, stdout_handle_, stdout_ansi_);
    open_output(STD_ERROR_HANDLE, stderr_handle_, stderr_ansi_);
    if (attached && (owns_console_ || config_.modify_host_console)) {
      if (!config_.title.empty() && !SetConsoleTitleW(detail::utf8_to_wide(config_.title).c_str()))
        detail::console_error("set console title");
      if (config_.buffer_width && config_.buffer_height && stdout_handle_) {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(stdout_handle_, &info)) detail::console_error("get console size");
        COORD size{static_cast<SHORT>((std::max)(static_cast<int>(config_.buffer_width),
                                                static_cast<int>(info.srWindow.Right) + 1)),
                   static_cast<SHORT>((std::max)(static_cast<int>(config_.buffer_height),
                                                static_cast<int>(info.srWindow.Bottom) + 1))};
        if (!SetConsoleScreenBufferSize(stdout_handle_, size)) detail::console_error("set console size");
      }
    }
  } catch (...) { release(); throw; }
#else
  stdout_ansi_ = config_.force_ansi || isatty(fileno(stdout)) != 0;
  stderr_ansi_ = config_.force_ansi || isatty(fileno(stderr)) != 0;
#endif
}

ConsoleSink::~ConsoleSink() { release(); }

void ConsoleSink::release() noexcept {
#if defined(_WIN32)
  if (detail::valid_handle(stdout_handle_)) CloseHandle(stdout_handle_);
  if (detail::valid_handle(stderr_handle_)) CloseHandle(stderr_handle_);
  stdout_handle_ = stderr_handle_ = nullptr;
  if (owns_console_ && config_.free_owned_console) FreeConsole();
  owns_console_ = false;
#endif
}

void ConsoleSink::write(const LogMessage &message) {
  buffer_.clear();
  const bool error = is_error_level(message.level);
  const bool ansi = (error ? stderr_ansi_ : stdout_ansi_) &&
                    (config_.enable_colors || config_.enable_backgrounds);
  if (ansi) {
    if (config_.enable_colors) buffer_ += level_to_color(message.level);
    if (config_.enable_backgrounds) buffer_ += level_to_background_color(message.level);
  }
  if (config_.show_date || config_.show_time) {
    buffer_ += '[';
    if (config_.show_date) buffer_ += format_date(message.time);
    if (config_.show_date && config_.show_time) buffer_ += ' ';
    if (config_.show_time)
      buffer_ += format_time(message.time,
          config_.time_format == ConsoleSinkConfig::TimeFormat::WithMilliseconds);
    buffer_ += "] ";
  }
  std::format_to(std::back_inserter(buffer_), "[{}] ", level_to_string(message.level));
  if (config_.show_file)
    std::format_to(std::back_inserter(buffer_), "[{}:{}] ",
                   shorten_filename(message.loc.file_name()), message.loc.line());
  if (config_.show_thread_name && !message.thread_name.empty())
    std::format_to(std::back_inserter(buffer_), "[{}] ", message.thread_name);
  if (config_.show_thread_id)
    std::format_to(std::back_inserter(buffer_), "[tid={}] ", message.thread_id);
  buffer_ += message.text;
  buffer_ += format_fields(message.fields);
  if (ansi) buffer_ += reset_color;
  buffer_ += '\n';
#if defined(_WIN32)
  HANDLE handle = error ? stderr_handle_ : stdout_handle_;
  if (detail::valid_handle(handle)) {
    auto wide = detail::utf8_to_wide(buffer_);
    size_t offset = 0;
    while (offset < wide.size()) {
      auto size = (std::min)(wide.size() - offset, size_t{16384});
      if (offset + size < wide.size() && wide[offset + size - 1] >= 0xd800 &&
          wide[offset + size - 1] <= 0xdbff) --size;
      DWORD written = 0;
      if (!WriteConsoleW(handle, wide.data() + offset, static_cast<DWORD>(size), &written, nullptr))
        detail::console_error("write logger console");
      if (!written) throw std::system_error(std::make_error_code(std::errc::io_error), "short console write");
      offset += written;
    }
    return;
  }
#endif
  FILE *stream = error ? stderr : stdout;
  if (std::fwrite(buffer_.data(), 1, buffer_.size(), stream) != buffer_.size())
    throw std::system_error(errno ? std::error_code(errno, std::generic_category()) :
        std::make_error_code(std::errc::io_error), "write logger stream");
  if (error) used_stderr_ = true; else used_stdout_ = true;
}

void ConsoleSink::flush() {
  if (used_stdout_ && std::fflush(stdout) != 0)
    throw std::system_error(errno, std::generic_category(), "flush stdout");
  if (used_stderr_ && std::fflush(stderr) != 0)
    throw std::system_error(errno, std::generic_category(), "flush stderr");
}

} // namespace log_core
