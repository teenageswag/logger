#pragma once

#include "core/logger.h"
#include "core/types.h"
#include "sinks/console_sink.h"
#include "sinks/file_sink.h"
#include "sinks/json_sink.h"

#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <source_location>
#include <string>
#include <type_traits>
#include <utility>

#ifndef LOG_ACTIVE_LEVEL
#define LOG_ACTIVE_LEVEL log::Level::Trace
#endif

struct log {
  using Level = log_core::Level;
  using Config = log_core::LoggerConfig;
  using Sink = log_core::Sink;
  using ConsoleConfig = log_core::ConsoleSinkConfig;
  using Field = log_core::Field;
  using Fields = log_core::Fields;
  using kv = log_core::kv;
  using ctx = log_core::ctx;

  static log_core::Logger &instance() { return log_core::Logger::instance(); }

  static void set_level(Level level) { instance().set_level(level); }
  static Level level() { return instance().get_level(); }
  static bool enabled(Level level) { return instance().enabled(level); }
  static void configure(const Config &config = {}) { instance().configure(config); }

  static void add_sink(std::unique_ptr<Sink> sink) {
    instance().add_sink(std::move(sink));
  }

  static void add_file_sink(const std::filesystem::path &path,
                            size_t max_size = 5 * 1024 * 1024,
                            size_t max_files = 1) {
    add_sink(std::make_unique<log_core::FileSink>(path, max_size, max_files));
  }

  static void add_json_sink(const std::filesystem::path &path,
                            size_t max_size = 10 * 1024 * 1024,
                            size_t max_files = 3) {
    log_core::JsonSinkConfig config;
    config.path = path;
    config.max_size = max_size;
    config.max_files = max_files;
    add_sink(std::make_unique<log_core::JsonSink>(std::move(config)));
  }

  static void add_console_sink(ConsoleConfig config = {}) {
    if (instance().only_in_debug_build() && !log_core::is_debug_build) {
      instance().remove_console_sinks();
      return;
    }
    instance().remove_console_sinks();
    add_sink(std::make_unique<log_core::ConsoleSink>(std::move(config)));
  }

  static void remove_console_sink() { instance().remove_console_sinks(); }
  static void init_console(ConsoleConfig config = {}) {
    add_console_sink(std::move(config));
  }

  static void set_thread_name(std::string name) {
    instance().set_thread_name(std::move(name));
  }

  static size_t dropped_count() { return instance().dropped_count(); }
  static size_t sink_error_count() { return instance().sink_error_count(); }
  static bool set_queue_capacity(size_t capacity) {
    return instance().set_queue_capacity(capacity);
  }
  static void flush() { instance().flush(); }
  static void shutdown() { instance().shutdown(); }

private:
  template <Level L, typename... Args>
  static void log_impl(
      log_core::log_format_string<std::type_identity_t<Args>...> fmt,
      Args &&...args) {
    if constexpr (LOG_ACTIVE_LEVEL <= L) {
      if (!instance().enabled(L))
        return;
      thread_local std::string buffer;
      buffer.clear();
      std::format_to(std::back_inserter(buffer), fmt.fmt,
                     std::forward<Args>(args)...);
      instance().enqueue(L, fmt.loc, buffer);
    }
  }

  template <Level L, typename... Args>
  static void fields_log_impl(
      log_core::Fields fields,
      log_core::log_format_string<std::type_identity_t<Args>...> fmt,
      Args &&...args) {
    if constexpr (LOG_ACTIVE_LEVEL <= L) {
      if (!instance().enabled(L))
        return;
      thread_local std::string buffer;
      buffer.clear();
      std::format_to(std::back_inserter(buffer), fmt.fmt,
                     std::forward<Args>(args)...);
      instance().enqueue_with_fields(L, fmt.loc, buffer,
                                     std::move(fields.values));
    }
  }

  template <Level L>
  static void ctx_impl(std::string text, log_core::Fields fields,
                       std::source_location loc) {
    if constexpr (LOG_ACTIVE_LEVEL <= L) {
      if (!instance().enabled(L))
        return;
      instance().enqueue_with_fields(L, loc, text, std::move(fields.values));
    }
  }

public:
  template <typename... Args>
  static void trace(log_core::log_format_string<std::type_identity_t<Args>...>
                        fmt,
                    Args &&...args) {
    log_impl<Level::Trace>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static void debug(log_core::log_format_string<std::type_identity_t<Args>...>
                        fmt,
                    Args &&...args) {
    log_impl<Level::Debug>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static void info(log_core::log_format_string<std::type_identity_t<Args>...>
                       fmt,
                   Args &&...args) {
    log_impl<Level::Info>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static void success(
      log_core::log_format_string<std::type_identity_t<Args>...> fmt,
      Args &&...args) {
    log_impl<Level::Success>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static void warn(log_core::log_format_string<std::type_identity_t<Args>...>
                       fmt,
                   Args &&...args) {
    log_impl<Level::Warn>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static void error(log_core::log_format_string<std::type_identity_t<Args>...>
                        fmt,
                    Args &&...args) {
    log_impl<Level::Error>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static void critical(
      log_core::log_format_string<std::type_identity_t<Args>...> fmt,
      Args &&...args) {
    log_impl<Level::Critical>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void info(
      log_core::Fields fields,
      log_core::log_format_string<std::type_identity_t<Args>...> fmt,
      Args &&...args) {
    fields_log_impl<Level::Info>(std::move(fields), fmt,
                                 std::forward<Args>(args)...);
  }

  static void trace_ctx(std::string text, log_core::ctx fields,
                        std::source_location loc =
                            std::source_location::current()) {
    ctx_impl<Level::Trace>(std::move(text), std::move(fields), loc);
  }
  static void debug_ctx(std::string text, log_core::ctx fields,
                        std::source_location loc =
                            std::source_location::current()) {
    ctx_impl<Level::Debug>(std::move(text), std::move(fields), loc);
  }
  static void info_ctx(std::string text, log_core::ctx fields,
                       std::source_location loc =
                           std::source_location::current()) {
    ctx_impl<Level::Info>(std::move(text), std::move(fields), loc);
  }
  static void success_ctx(std::string text, log_core::ctx fields,
                          std::source_location loc =
                              std::source_location::current()) {
    ctx_impl<Level::Success>(std::move(text), std::move(fields), loc);
  }
  static void warn_ctx(std::string text, log_core::ctx fields,
                       std::source_location loc =
                           std::source_location::current()) {
    ctx_impl<Level::Warn>(std::move(text), std::move(fields), loc);
  }
  static void error_ctx(std::string text, log_core::ctx fields,
                        std::source_location loc =
                            std::source_location::current()) {
    ctx_impl<Level::Error>(std::move(text), std::move(fields), loc);
  }
  static void critical_ctx(std::string text, log_core::ctx fields,
                           std::source_location loc =
                               std::source_location::current()) {
    ctx_impl<Level::Critical>(std::move(text), std::move(fields), loc);
  }
};
