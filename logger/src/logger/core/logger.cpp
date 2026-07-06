#include "logger.h"
#include "types.h"
#include "../sinks/console_sink.h"

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

namespace log_core {

Logger::Logger() : queue_(queue_capacity_) {
#if defined(_WIN32)
  if (GetConsoleWindow() != NULL) {
    ConsoleSinkConfig cfg;
    cfg.auto_alloc = false;
    sinks_.push_back(std::make_unique<ConsoleSink>(cfg));
  }
#else
  sinks_.push_back(std::make_unique<ConsoleSink>());
#endif
  worker_ = std::jthread([this](std::stop_token stoken) {
    worker_loop(stoken);
  });
}

Logger::~Logger() {
  {
    std::unique_lock lock(queue_mutex_);
    // Signal stop via the jthread's stop token — no bool needed
  }
  cv_pop_.notify_one();
  // jthread destructor requests stop + joins automatically

  flush_sinks();
}

void Logger::worker_loop(std::stop_token stoken) {
  std::vector<LogMessage> local_batch;
  local_batch.reserve(256);

  while (!stoken.stop_requested()) {
    {
      std::unique_lock lock(queue_mutex_);
      cv_pop_.wait_for(lock, std::chrono::milliseconds(500),
                       [this, &stoken] { return count_ > 0 || stoken.stop_requested(); });

      if (stoken.stop_requested() && count_ == 0)
        break;

      while (count_ > 0 && local_batch.size() < 256) {
        local_batch.push_back(std::move(queue_[head_]));
        head_ = (head_ + 1) % queue_capacity_;
        --count_;
      }
    }

    if (!local_batch.empty()) {
      cv_pop_.notify_all(); // wake producer if blocked (shouldn't happen with drop-oldest)

      std::lock_guard lock(sinks_mutex_);
      bool should_flush = false;
      for (const auto &msg : local_batch) {
        for (auto &sink : sinks_) {
          sink->write(msg);
        }
        if (msg.level >= Level::ERROR)
          should_flush = true;
      }
      if (should_flush)
        flush_sinks_unsafe();

      local_batch.clear();
    } else {
      flush_sinks();
    }
  }

  // Drain any remaining messages on shutdown
  {
    std::unique_lock lock(queue_mutex_);
    while (count_ > 0) {
      local_batch.push_back(std::move(queue_[head_]));
      head_ = (head_ + 1) % queue_capacity_;
      --count_;
    }
  }

  if (!local_batch.empty()) {
    std::lock_guard lock(sinks_mutex_);
    for (const auto &msg : local_batch) {
      for (auto &sink : sinks_) {
        sink->write(msg);
      }
    }
    flush_sinks_unsafe();
  }
}

void Logger::flush_sinks_unsafe() {
  for (auto &sink : sinks_)
    sink->flush();
}

void Logger::flush_sinks() {
  std::lock_guard lock(sinks_mutex_);
  flush_sinks_unsafe();
}

uint32_t Logger::get_current_thread_id() {
  thread_local uint32_t tid = []() {
#if defined(_WIN32)
    return static_cast<uint32_t>(GetCurrentThreadId());
#else
    return static_cast<uint32_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
  }();
  return tid;
}

void Logger::remove_console_sinks() {
  std::lock_guard lock(sinks_mutex_);
  for (auto it = sinks_.begin(); it != sinks_.end();) {
    if (dynamic_cast<ConsoleSink *>(it->get())) {
      it = sinks_.erase(it);
    } else {
      ++it;
    }
  }
}

void Logger::enqueue(Level level, const std::source_location &loc,
                     std::string_view text) {
  std::unique_lock lock(queue_mutex_);

  if (count_ >= queue_capacity_) {
    // Drop oldest: overwrite head and advance
    auto &entry = queue_[head_];
    entry.level = level;
    entry.time = std::chrono::system_clock::now();
    entry.thread_id = get_current_thread_id();
    entry.loc = loc;
    entry.text.assign(text);
    entry.kv_pairs.clear();

    head_ = (head_ + 1) % queue_capacity_;
    // tail_ stays — we overwrote the oldest, count_ stays the same
    dropped_count_.fetch_add(1, std::memory_order_relaxed);
  } else {
    auto &entry = queue_[tail_];
    entry.level = level;
    entry.time = std::chrono::system_clock::now();
    entry.thread_id = get_current_thread_id();
    entry.loc = loc;
    entry.text.assign(text);
    entry.kv_pairs.clear();

    tail_ = (tail_ + 1) % queue_capacity_;
    ++count_;
  }

  lock.unlock();
  cv_pop_.notify_one();
}

void Logger::enqueue_with_kv(Level level, const std::source_location &loc,
                             std::string_view text,
                             std::vector<kv> kv_pairs) {
  std::unique_lock lock(queue_mutex_);

  if (count_ >= queue_capacity_) {
    auto &entry = queue_[head_];
    entry.level = level;
    entry.time = std::chrono::system_clock::now();
    entry.thread_id = get_current_thread_id();
    entry.loc = loc;
    entry.text.assign(text);
    entry.kv_pairs = std::move(kv_pairs);

    head_ = (head_ + 1) % queue_capacity_;
    dropped_count_.fetch_add(1, std::memory_order_relaxed);
  } else {
    auto &entry = queue_[tail_];
    entry.level = level;
    entry.time = std::chrono::system_clock::now();
    entry.thread_id = get_current_thread_id();
    entry.loc = loc;
    entry.text.assign(text);
    entry.kv_pairs = std::move(kv_pairs);

    tail_ = (tail_ + 1) % queue_capacity_;
    ++count_;
  }

  lock.unlock();
  cv_pop_.notify_one();
}

void Logger::set_thread_name(std::string name) {
  tls_thread_name = std::move(name);
}

std::string Logger::get_thread_name() const { return tls_thread_name; }

void Logger::set_queue_capacity(size_t capacity) {
  std::lock_guard lock(queue_mutex_);
  queue_capacity_ = capacity;
  queue_.resize(capacity);
  head_ = 0;
  tail_ = 0;
  count_ = 0;
}

} // namespace log_core
