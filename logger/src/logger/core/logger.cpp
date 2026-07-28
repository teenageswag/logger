#include "logger.h"
#include "../sinks/console_sink.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <functional>
#include <algorithm>

namespace log_core {

#if defined(_WIN32)
namespace {
bool has_console_output() {
  const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  return output != nullptr && output != INVALID_HANDLE_VALUE &&
         GetConsoleMode(output, &mode) != FALSE;
}
} // namespace
#endif

Logger::Logger() : queue_(queue_capacity_) {
#if defined(_WIN32)
  if (has_console_output()) {
    ConsoleSinkConfig config;
    config.auto_alloc = false;
    add_sink(std::make_unique<ConsoleSink>(config));
  }
#else
  add_sink(std::make_unique<ConsoleSink>());
#endif

  worker_ = std::jthread([this](std::stop_token stop_token) {
    worker_loop(stop_token);
  });
}

Logger::~Logger() { shutdown(); }

void Logger::add_sink(std::unique_ptr<Sink> sink) {
  if (!sink || shutdown_started_.load(std::memory_order_acquire))
    return;
  std::lock_guard lock(sinks_mutex_);
  sinks_.push_back(std::move(sink));
}

void Logger::worker_loop(std::stop_token stop_token) {
  std::vector<LogMessage> batch;
  batch.reserve(256);

  for (;;) {
    {
      std::unique_lock lock(queue_mutex_);
      queue_cv_.wait(lock, [this, &stop_token] {
        return count_ > 0 || stop_token.stop_requested();
      });

      if (count_ == 0 && stop_token.stop_requested())
        break;

      while (count_ > 0 && batch.size() < 256) {
        batch.push_back(std::move(queue_[head_]));
        head_ = (head_ + 1) % queue_capacity_;
        --count_;
      }
    }

    if (!batch.empty()) {
      process_batch(batch);
      batch.clear();
    }
  }

  {
    std::unique_lock lock(queue_mutex_);
    while (count_ > 0) {
      batch.push_back(std::move(queue_[head_]));
      head_ = (head_ + 1) % queue_capacity_;
      --count_;
    }
  }

  if (!batch.empty()) {
    process_batch(batch);
    batch.clear();
  }
  flush_sinks();
}

void Logger::process_batch(const std::vector<LogMessage> &batch) noexcept {
  std::lock_guard lock(sinks_mutex_);
  bool should_flush = false;

  for (const auto &message : batch) {
    for (auto &sink : sinks_) {
      try {
        sink->write(message);
      } catch (...) {
        sink_error_count_.fetch_add(1, std::memory_order_relaxed);
      }
    }
    if (is_error_level(message.level))
      should_flush = true;
  }

  if (should_flush)
    flush_sinks_unsafe();
}

void Logger::flush_sinks_unsafe() noexcept {
  for (auto &sink : sinks_) {
    try {
      sink->flush();
    } catch (...) {
      sink_error_count_.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

void Logger::flush_sinks() noexcept {
  std::lock_guard lock(sinks_mutex_);
  flush_sinks_unsafe();
}

void Logger::flush() { flush_sinks(); }

void Logger::shutdown() noexcept {
  bool expected = false;
  if (!shutdown_started_.compare_exchange_strong(
          expected, true, std::memory_order_acq_rel))
    return;

  accepting_.store(false, std::memory_order_release);
  worker_.request_stop();
  queue_cv_.notify_one();
  if (worker_.joinable())
    worker_.join();
  flush_sinks();
}

uint32_t Logger::get_current_thread_id() {
  thread_local const uint32_t thread_id = [] {
#if defined(_WIN32)
    return static_cast<uint32_t>(GetCurrentThreadId());
#else
    return static_cast<uint32_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
  }();
  return thread_id;
}

void Logger::remove_console_sinks() {
  std::lock_guard lock(sinks_mutex_);
  for (auto it = sinks_.begin(); it != sinks_.end();) {
    if (dynamic_cast<ConsoleSink *>(it->get()) != nullptr)
      it = sinks_.erase(it);
    else
      ++it;
  }
}

void Logger::enqueue(Level level, const std::source_location &loc,
                     std::string_view text) {
  enqueue_impl(level, loc, text, {});
}

void Logger::enqueue_with_fields(Level level, const std::source_location &loc,
                                 std::string_view text,
                                 std::vector<Field> fields) {
  enqueue_impl(level, loc, text, std::move(fields));
}

void Logger::enqueue_impl(Level level, const std::source_location &loc,
                          std::string_view text, std::vector<Field> fields) {
  if (!enabled(level))
    return;

  std::unique_lock lock(queue_mutex_);
  if (!accepting_.load(std::memory_order_acquire))
    return;

  auto &entry = count_ == queue_capacity_ ? queue_[head_] : queue_[tail_];
  entry.level = level;
  entry.time = std::chrono::system_clock::now();
  entry.thread_id = get_current_thread_id();
  entry.thread_name = tls_thread_name;
  entry.loc = loc;
  entry.text.assign(text);
  entry.fields = std::move(fields);

  if (count_ == queue_capacity_) {
    head_ = (head_ + 1) % queue_capacity_;
    dropped_count_.fetch_add(1, std::memory_order_relaxed);
  } else {
    tail_ = (tail_ + 1) % queue_capacity_;
    ++count_;
  }

  lock.unlock();
  queue_cv_.notify_one();
}

void Logger::set_thread_name(std::string name) {
  tls_thread_name = std::move(name);
}

std::string Logger::get_thread_name() const { return tls_thread_name; }

bool Logger::set_queue_capacity(size_t capacity) {
  if (capacity == 0)
    return false;

  std::lock_guard lock(queue_mutex_);
  if (capacity == queue_capacity_)
    return true;

  std::vector<LogMessage> resized(capacity);
  const size_t keep = std::min(count_, capacity);
  const size_t first_kept = (head_ + count_ - keep) % queue_capacity_;

  for (size_t i = 0; i < keep; ++i)
    resized[i] = std::move(queue_[(first_kept + i) % queue_capacity_]);

  if (count_ > keep)
    dropped_count_.fetch_add(count_ - keep, std::memory_order_relaxed);

  queue_ = std::move(resized);
  queue_capacity_ = capacity;
  head_ = 0;
  count_ = keep;
  tail_ = keep % capacity;
  queue_cv_.notify_one();
  return true;
}

} // namespace log_core
