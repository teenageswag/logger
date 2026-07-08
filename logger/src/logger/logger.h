#pragma once

#include "core/logger.h"
#include "core/types.h"
#include "sinks/console_sink.h"
#include "sinks/file_sink.h"
#include "sinks/json_sink.h"

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#ifndef LOG_ACTIVE_LEVEL
#define LOG_ACTIVE_LEVEL log::Level::TRACE
#endif

struct log {
  using Level = log_core::Level;
  using Sink = log_core::Sink;
  using ConsoleConfig = log_core::ConsoleSinkConfig;
  using kv = log_core::kv;
  using ctx = log_core::ctx;

  static log_core::Logger &instance() { return log_core::Logger::instance(); }

  static void set_level(Level level) {
    log_core::Logger::instance().set_level(level);
  }

  static void add_file_sink(const std::filesystem::path &path,
                            size_t max_size = 5 * 1024 * 1024,
                            size_t max_files = 1) {
    log_core::Logger::instance().add_sink(
        std::make_unique<log_core::FileSink>(path, max_size, max_files));
  }

  static void add_json_sink(const std::filesystem::path &path,
                            size_t max_size = 10 * 1024 * 1024,
                            size_t max_files = 3) {
    log_core::JsonSinkConfig cfg;
    cfg.path = path;
    cfg.max_size = max_size;
    cfg.max_files = max_files;
    log_core::Logger::instance().add_sink(
        std::make_unique<log_core::JsonSink>(std::move(cfg)));
  }

  static void add_console_sink(ConsoleConfig cfg = {}) {
    log_core::Logger::instance().remove_console_sinks();
    log_core::Logger::instance().add_sink(
        std::make_unique<log_core::ConsoleSink>(std::move(cfg)));
  }

  static void remove_console_sink() {
    log_core::Logger::instance().remove_console_sinks();
  }

  static void init_console(ConsoleConfig cfg = {}) {
    add_console_sink(std::move(cfg));
  }

  static void set_thread_name(std::string name) {
    log_core::Logger::instance().set_thread_name(std::move(name));
  }

  static size_t dropped_count() {
    return log_core::Logger::instance().dropped_count();
  }

  // --- Internal: compile-time format + enqueue ---
  template <Level L, typename... Args>
  static void log_impl(
      log_core::log_format_string<std::type_identity_t<Args>...> fmt,
      Args &&...args) {
    if constexpr (LOG_ACTIVE_LEVEL <= L) {
      thread_local std::string buf;
      buf.clear();
      std::format_to(std::back_inserter(buf), fmt.fmt,
                     std::forward<Args>(args)...);
      log_core::Logger::instance().enqueue(L, fmt.loc, buf);
    }
  }

  // --- Internal: pre-formatted text + kv context ---
  template <Level L>
  static void ctx_impl(std::string text, log_core::ctx kv_ctx,
                        std::source_location loc =
                            std::source_location::current()) {
    if constexpr (LOG_ACTIVE_LEVEL <= L) {
      log_core::Logger::instance().enqueue_with_kv(
          L, loc, std::move(text), std::move(kv_ctx.pairs));
    }
  }

  // --- Basic log methods ---
  template <typename... Args>
  static void trace(log_core::log_format_string<std::type_identity_t<Args>...>
                        fmt,
                    Args &&...args) {
    log_impl<Level::TRACE>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void debug(log_core::log_format_string<std::type_identity_t<Args>...>
                        fmt,
                    Args &&...args) {
    log_impl<Level::DEBUG>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void info(log_core::log_format_string<std::type_identity_t<Args>...>
                       fmt,
                   Args &&...args) {
    log_impl<Level::INFO>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void success(log_core::log_format_string<std::type_identity_t<Args>...>
                          fmt,
                      Args &&...args) {
    log_impl<Level::SUCCESS>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void warn(log_core::log_format_string<std::type_identity_t<Args>...>
                       fmt,
                   Args &&...args) {
    log_impl<Level::WARN>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void error(log_core::log_format_string<std::type_identity_t<Args>...>
                        fmt,
                    Args &&...args) {
    log_impl<Level::ERROR>(fmt, std::forward<Args>(args)...);
  }

  // --- Structured log methods with kv pairs ---
  static void trace_ctx(
      std::string text, log_core::ctx kv_ctx,
      std::source_location loc = std::source_location::current()) {
    ctx_impl<Level::TRACE>(std::move(text), std::move(kv_ctx), loc);
  }

  static void debug_ctx(
      std::string text, log_core::ctx kv_ctx,
      std::source_location loc = std::source_location::current()) {
    ctx_impl<Level::DEBUG>(std::move(text), std::move(kv_ctx), loc);
  }

  static void info_ctx(
      std::string text, log_core::ctx kv_ctx,
      std::source_location loc = std::source_location::current()) {
    ctx_impl<Level::INFO>(std::move(text), std::move(kv_ctx), loc);
  }

  static void success_ctx(
      std::string text, log_core::ctx kv_ctx,
      std::source_location loc = std::source_location::current()) {
    ctx_impl<Level::SUCCESS>(std::move(text), std::move(kv_ctx), loc);
  }

  static void warn_ctx(
      std::string text, log_core::ctx kv_ctx,
      std::source_location loc = std::source_location::current()) {
    ctx_impl<Level::WARN>(std::move(text), std::move(kv_ctx), loc);
  }

  static void error_ctx(
      std::string text, log_core::ctx kv_ctx,
      std::source_location loc = std::source_location::current()) {
    ctx_impl<Level::ERROR>(std::move(text), std::move(kv_ctx), loc);
  }
};
