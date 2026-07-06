#pragma once

#include "types.h"
#include "../sinks/sink.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace log_core {

class Logger {
public:
  static Logger &instance() {
    static Logger inst;
    return inst;
  }

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void set_level(Level level) { min_level_.store(level, std::memory_order_relaxed); }

  Level get_level() const {
    return min_level_.load(std::memory_order_relaxed);
  }

  void add_sink(std::unique_ptr<Sink> sink) {
    std::lock_guard lock(sinks_mutex_);
    sinks_.push_back(std::move(sink));
  }

  void remove_console_sinks();

  void enqueue(Level level, const std::source_location &loc,
               std::string_view text);

  void enqueue_with_kv(Level level, const std::source_location &loc,
                       std::string_view text, std::vector<kv> kv_pairs);

  uint32_t get_thread_id() const { return get_current_thread_id(); }

  void set_thread_name(std::string name);

  std::string get_thread_name() const;

  size_t dropped_count() const {
    return dropped_count_.load(std::memory_order_relaxed);
  }

  void set_queue_capacity(size_t capacity);

private:
  Logger();
  ~Logger();

  void worker_loop(std::stop_token stoken);
  void flush_sinks_unsafe();
  void flush_sinks();

  static uint32_t get_current_thread_id();

  std::vector<std::unique_ptr<Sink>> sinks_;
  std::atomic<Level> min_level_{Level::INFO};
  std::mutex sinks_mutex_;

  // Ring buffer
  size_t queue_capacity_ = 4096;
  std::vector<LogMessage> queue_;
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t count_ = 0;
  std::atomic<size_t> dropped_count_{0};

  std::mutex queue_mutex_;
  std::condition_variable cv_pop_;
  std::jthread worker_;

  // Thread names (thread_local)
};

inline thread_local std::string tls_thread_name;

} // namespace log_core
