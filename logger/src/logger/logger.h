#pragma once

#include "core/logger.h"
#include "sinks/console_sink.h"
#include "sinks/file_sink.h"
#include "sinks/json_sink.h"

#include <filesystem>
#include <memory>

struct log {
  using Level = log_core::Level;
  using Mode = log_core::Mode;
  using OverflowPolicy = log_core::OverflowPolicy;
  using Config = log_core::LoggerConfig;
  using Logger = log_core::Logger;
  using Result = log_core::Result;
  using Error = log_core::Error;
  using ErrorCode = log_core::ErrorCode;
  using Sink = log_core::Sink;
  using ConsoleConfig = log_core::ConsoleSinkConfig;
  using Field = log_core::Field;
  using Fields = log_core::Fields;
  using kv = Field;
  using ctx = Fields;

  static Logger &instance() { return Logger::instance(); }
  static void set_level(Level level) noexcept { if (auto *p = get()) p->set_level(level); }
  static Level level() noexcept { auto *p = get(); return p ? p->get_level() : Level::Off; }
  static bool enabled(Level level) noexcept { auto *p = get(); return p && p->enabled(level); }
  static Result configure(const Config &config = {}) {
    auto *p = get(); return p ? p->configure(config) : stopped();
  }
  static Result add_sink(std::unique_ptr<Sink> sink) {
    auto *p = get(); return p ? p->add_sink(std::move(sink)) : stopped();
  }
  static Result add_file_sink(const std::filesystem::path &path,
                             uint64_t max_size = 5 * 1024 * 1024, size_t max_files = 1) {
    return make_sink<log_core::FileSink>(path, max_size, max_files);
  }
  static Result add_json_sink(const std::filesystem::path &path,
                             uint64_t max_size = 10 * 1024 * 1024, size_t max_files = 3) {
    return make_sink<log_core::JsonSink>(log_core::JsonSinkConfig{path, max_size, max_files});
  }
  static Result add_console_sink(ConsoleConfig config = {}) {
    auto *p = get(); return p ? p->init_console(std::move(config)) : stopped();
  }
  static Result init_console(ConsoleConfig config = {}) { return add_console_sink(std::move(config)); }
  static Result remove_console_sink() {
    auto *p = get(); return p ? p->remove_console_sinks() : stopped();
  }
  static void set_thread_name(std::string_view name) noexcept {
    if (auto *p = get()) p->set_thread_name(name);
  }
  static uint64_t dropped_count() noexcept { auto *p = get(); return p ? p->dropped_count() : 0; }
  static uint64_t sink_error_count() noexcept { auto *p = get(); return p ? p->sink_error_count() : 0; }
  static log_core::Statistics statistics() { auto *p = get(); return p ? p->statistics() : log_core::Statistics{}; }
  static Error last_error() { auto *p = get(); return p ? p->last_error() : Error{ErrorCode::Stopped, {}}; }
  static bool set_queue_capacity(size_t capacity) {
    auto *p = get(); return p && p->set_queue_capacity(capacity);
  }
  static Result flush() { auto *p = get(); return p ? p->flush() : stopped(); }
  static bool shutdown() noexcept { auto *p = get(); return !p || p->shutdown(); }

  static bool text(Level level, std::string_view text,
                   std::source_location loc = std::source_location::current()) noexcept {
    auto *p = get(); return p && p->enqueue(level, loc, text);
  }
  template <typename Factory>
  static bool lazy(Level level, Factory &&factory,
                   std::source_location loc = std::source_location::current()) noexcept {
    auto *p = get(); return p && p->lazy(level, std::forward<Factory>(factory), loc);
  }

  template <typename... Args>
  static bool trace(log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get(); return p && p->write<Level::Trace>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static bool trace(Fields fields, log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get();
    return p && p->write_fields<Level::Trace>(std::move(fields), fmt, std::forward<Args>(args)...);
  }
  static bool trace_ctx(std::string text, ctx fields,
                         std::source_location loc = std::source_location::current()) noexcept {
    auto *p = get();
    return p && p->enqueue_owned(Level::Trace, loc, std::move(text), std::move(fields.values));
  }
  template <typename... Args>
  static bool debug(log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get(); return p && p->write<Level::Debug>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static bool debug(Fields fields, log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get();
    return p && p->write_fields<Level::Debug>(std::move(fields), fmt, std::forward<Args>(args)...);
  }
  static bool debug_ctx(std::string text, ctx fields,
                         std::source_location loc = std::source_location::current()) noexcept {
    auto *p = get();
    return p && p->enqueue_owned(Level::Debug, loc, std::move(text), std::move(fields.values));
  }
  template <typename... Args>
  static bool info(log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get(); return p && p->write<Level::Info>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static bool info(Fields fields, log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get();
    return p && p->write_fields<Level::Info>(std::move(fields), fmt, std::forward<Args>(args)...);
  }
  static bool info_ctx(std::string text, ctx fields,
                         std::source_location loc = std::source_location::current()) noexcept {
    auto *p = get();
    return p && p->enqueue_owned(Level::Info, loc, std::move(text), std::move(fields.values));
  }
  template <typename... Args>
  static bool success(log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get(); return p && p->write<Level::Success>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static bool success(Fields fields, log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get();
    return p && p->write_fields<Level::Success>(std::move(fields), fmt, std::forward<Args>(args)...);
  }
  static bool success_ctx(std::string text, ctx fields,
                         std::source_location loc = std::source_location::current()) noexcept {
    auto *p = get();
    return p && p->enqueue_owned(Level::Success, loc, std::move(text), std::move(fields.values));
  }
  template <typename... Args>
  static bool warn(log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get(); return p && p->write<Level::Warn>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static bool warn(Fields fields, log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get();
    return p && p->write_fields<Level::Warn>(std::move(fields), fmt, std::forward<Args>(args)...);
  }
  static bool warn_ctx(std::string text, ctx fields,
                         std::source_location loc = std::source_location::current()) noexcept {
    auto *p = get();
    return p && p->enqueue_owned(Level::Warn, loc, std::move(text), std::move(fields.values));
  }
  template <typename... Args>
  static bool error(log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get(); return p && p->write<Level::Error>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static bool error(Fields fields, log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get();
    return p && p->write_fields<Level::Error>(std::move(fields), fmt, std::forward<Args>(args)...);
  }
  static bool error_ctx(std::string text, ctx fields,
                         std::source_location loc = std::source_location::current()) noexcept {
    auto *p = get();
    return p && p->enqueue_owned(Level::Error, loc, std::move(text), std::move(fields.values));
  }
  template <typename... Args>
  static bool critical(log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get(); return p && p->write<Level::Critical>(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  static bool critical(Fields fields, log_core::log_format_string<std::type_identity_t<Args>...> fmt,
                     Args &&...args) noexcept {
    auto *p = get();
    return p && p->write_fields<Level::Critical>(std::move(fields), fmt, std::forward<Args>(args)...);
  }
  static bool critical_ctx(std::string text, ctx fields,
                         std::source_location loc = std::source_location::current()) noexcept {
    auto *p = get();
    return p && p->enqueue_owned(Level::Critical, loc, std::move(text), std::move(fields.values));
  }

private:
  static Logger *get() noexcept { return Logger::try_instance(); }
  static Result stopped() { return std::unexpected(Error{ErrorCode::Stopped, {}}); }
  template <typename T, typename... Args>
  static Result make_sink(Args &&...args) {
    auto *p = get();
    if (!p || !p->running()) return stopped();
    if (log_core::detail::CallbackFrame::contains(p))
      return std::unexpected(Error{ErrorCode::ReentrantCall, {}});
    if (p->only_in_debug_build() && !log_core::is_debug_build) return {};
    try { return p->add_sink(std::make_unique<T>(std::forward<Args>(args)...)); }
    catch (const std::system_error &e) {
      return std::unexpected(Error{ErrorCode::SinkFailure, e.code()});
    } catch (const std::bad_alloc &) {
      return std::unexpected(Error{ErrorCode::AllocationFailed, {}});
    } catch (const std::invalid_argument &) {
      return std::unexpected(Error{ErrorCode::InvalidConfig, {}});
    }
  }
};

// Only call-site macros depend on this setting: shared inline definitions stay identical.
#ifndef LOG_ACTIVE_LEVEL
#define LOG_ACTIVE_LEVEL ::log::Level::Trace
#endif
#define LOG_DETAIL_WRITE(method, severity, ...) do { \
  if constexpr ((LOG_ACTIVE_LEVEL) <= ::log::Level::severity) { \
    if (::log::enabled(::log::Level::severity)) (void)::log::method(__VA_ARGS__); \
  } \
} while (false)
#define LOG_TRACE(...) LOG_DETAIL_WRITE(trace, Trace, __VA_ARGS__)
#define LOG_DEBUG(...) LOG_DETAIL_WRITE(debug, Debug, __VA_ARGS__)
#define LOG_INFO(...) LOG_DETAIL_WRITE(info, Info, __VA_ARGS__)
#define LOG_SUCCESS(...) LOG_DETAIL_WRITE(success, Success, __VA_ARGS__)
#define LOG_WARN(...) LOG_DETAIL_WRITE(warn, Warn, __VA_ARGS__)
#define LOG_ERROR(...) LOG_DETAIL_WRITE(error, Error, __VA_ARGS__)
#define LOG_CRITICAL(...) LOG_DETAIL_WRITE(critical, Critical, __VA_ARGS__)
