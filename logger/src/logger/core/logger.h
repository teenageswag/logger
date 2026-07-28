#pragma once

#include "types.h"
#include "../sinks/sink.h"

#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace log_core {

#if defined(_DEBUG)
inline constexpr bool is_debug_build = true;
#else
inline constexpr bool is_debug_build = false;
#endif

struct LoggerConfig {
  Level level = Level::Info;
  bool only_in_debug_build = false;
  size_t queue_capacity = 4096;
};

class Logger {
public:
  static Logger &instance() {
    static Logger instance;
    return instance;
  }

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void set_level(Level level) noexcept {
    min_level_.store(level, std::memory_order_relaxed);
  }

  void configure(const LoggerConfig &config) {
    set_level(config.level);
    set_only_in_debug_build(config.only_in_debug_build);
    set_queue_capacity(config.queue_capacity);
  }

  Level get_level() const noexcept {
    return min_level_.load(std::memory_order_relaxed);
  }

  bool enabled(Level level) const noexcept {
    return level >= get_level() && accepting_.load(std::memory_order_acquire) &&
           (!only_in_debug_build_.load(std::memory_order_relaxed) ||
            is_debug_build);
  }

  void set_only_in_debug_build(bool enabled) noexcept {
    only_in_debug_build_.store(enabled, std::memory_order_relaxed);
    if (enabled && !is_debug_build)
      remove_console_sinks();
  }

  bool only_in_debug_build() const noexcept {
    return only_in_debug_build_.load(std::memory_order_relaxed);
  }

  void add_sink(std::unique_ptr<Sink> sink);
  void remove_console_sinks();

  void enqueue(Level level, const std::source_location &loc,
               std::string_view text);
  void enqueue_with_fields(Level level, const std::source_location &loc,
                           std::string_view text, std::vector<Field> fields);

  uint32_t get_thread_id() const { return get_current_thread_id(); }
  void set_thread_name(std::string name);
  std::string get_thread_name() const;

  size_t dropped_count() const noexcept {
    return dropped_count_.load(std::memory_order_relaxed);
  }

  size_t sink_error_count() const noexcept {
    return sink_error_count_.load(std::memory_order_relaxed);
  }

  bool set_queue_capacity(size_t capacity);
  void flush();
  void shutdown() noexcept;

private:
  Logger();
  ~Logger();

  void enqueue_impl(Level level, const std::source_location &loc,
                    std::string_view text, std::vector<Field> fields);
  void worker_loop(std::stop_token stop_token);
  void process_batch(const std::vector<LogMessage> &batch) noexcept;
  void flush_sinks_unsafe() noexcept;
  void flush_sinks() noexcept;
  static uint32_t get_current_thread_id();

  std::vector<std::unique_ptr<Sink>> sinks_;
  std::atomic<Level> min_level_{Level::Info};
  std::mutex sinks_mutex_;

  size_t queue_capacity_ = 4096;
  std::vector<LogMessage> queue_;
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t count_ = 0;
  std::atomic<size_t> dropped_count_{0};
  std::atomic<size_t> sink_error_count_{0};

  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::jthread worker_;
  std::atomic<bool> accepting_{true};
  std::atomic<bool> shutdown_started_{false};
  std::atomic<bool> only_in_debug_build_{false};
};

inline thread_local std::string tls_thread_name;

} // namespace log_core
