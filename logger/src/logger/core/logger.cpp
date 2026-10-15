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

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace log_core {

static_assert(std::is_nothrow_move_constructible_v<LogMessage>);
static_assert(std::is_nothrow_move_assignable_v<LogMessage>);

bool Logger::valid_config(const LoggerConfig &c) noexcept {
  return c.level <= Level::Off && c.queue_capacity > 0 &&
      c.queue_capacity <= 1024 * 1024 && c.max_queue_bytes > 0 &&
      c.max_record_bytes > 0 && c.max_record_bytes <= 1024 * 1024 &&
      c.max_fields <= 1024 && c.flush_interval.count() >= 0 &&
      c.flush_interval <= std::chrono::hours(24) &&
      (c.mode == Mode::Synchronous || c.mode == Mode::Asynchronous) &&
      (c.overflow == OverflowPolicy::DropOldest || c.overflow == OverflowPolicy::DropNewest);
}

Logger::Logger(const LoggerConfig &config) : config_(config) {
  if (!valid_config(config)) throw std::invalid_argument("invalid logger configuration");
  if (config.mode == Mode::Asynchronous) queue_.resize(config.queue_capacity);
  min_level_.store(config.level);
  only_in_debug_build_.store(config.only_in_debug_build);
  max_record_bytes_.store(config.max_record_bytes);
}

Logger::~Logger() {
  if (is_default_) detail::default_logger_destroyed.store(true, std::memory_order_release);
  shutdown();
}

Logger *Logger::try_instance() noexcept {
  if (detail::default_logger_destroyed.load(std::memory_order_acquire)) return nullptr;
  try { static Logger logger(DefaultTag{}); return &logger; }
  catch (...) { return nullptr; }
}

Logger &Logger::instance() {
  if (auto *logger = try_instance()) return *logger;
  throw std::runtime_error("default logger is unavailable");
}

void Logger::record_error(Error error) noexcept {
  std::lock_guard lock(error_mutex_);
  last_error_ = error;
}

Error Logger::last_error() const {
  std::lock_guard lock(error_mutex_);
  return last_error_;
}

bool Logger::reject(ErrorCode code) noexcept {
  rejected_count_.fetch_add(1, std::memory_order_relaxed);
  record_error({code, {}});
  return false;
}

Result Logger::configure_locked(const LoggerConfig &config) {
  if (stopping_) return std::unexpected(Error{ErrorCode::Stopped, {}});
  if (started_ && config.mode != config_.mode)
    return std::unexpected(Error{ErrorCode::ModeAlreadyStarted, {}});
  std::vector<LogMessage> resized;
  const bool replace = config.mode == Mode::Asynchronous && queue_.size() != config.queue_capacity;
  try { if (replace) resized.resize(config.queue_capacity); }
  catch (const std::bad_alloc &) {
    return std::unexpected(Error{ErrorCode::AllocationFailed, {}});
  }
  while (count_ > config.queue_capacity || queued_bytes_ > config.max_queue_bytes) drop_front();
  if (replace) {
    for (size_t i = 0; i < count_; ++i)
      resized[i] = std::move(queue_[(head_ + i) % queue_.size()]);
    queue_ = std::move(resized);
    head_ = 0;
    tail_ = count_ % queue_.size();
  } else if (config.mode == Mode::Synchronous) {
    queue_.clear();
  }
  config_ = config;
  min_level_.store(config.level, std::memory_order_relaxed);
  only_in_debug_build_.store(config.only_in_debug_build, std::memory_order_relaxed);
  max_record_bytes_.store(config.max_record_bytes, std::memory_order_relaxed);
  ++config_revision_;
  return {};
}

Result Logger::configure(const LoggerConfig &config) {
  if (in_callback()) return std::unexpected(Error{ErrorCode::ReentrantCall, {}});
  if (!valid_config(config)) return std::unexpected(Error{ErrorCode::InvalidConfig, {}});
  std::lock_guard control(control_mutex_);
  {
    std::lock_guard lock(queue_mutex_);
    auto result = configure_locked(config);
    if (!result) return result;
  }
  queue_cv_.notify_one();
  if (config.only_in_debug_build && !is_debug_build) {
    std::lock_guard lock(sinks_mutex_);
    detail::CallbackFrame guard(this);
    std::erase_if(sinks_, [](const auto &sink) { return sink->is_console(); });
  }
  return {};
}

void Logger::set_only_in_debug_build(bool value) {
  if (in_callback()) { reject(ErrorCode::ReentrantCall); return; }
  std::lock_guard control(control_mutex_);
  if (!accepting_.load()) return;
  { std::lock_guard lock(queue_mutex_); config_.only_in_debug_build = value;
    only_in_debug_build_.store(value); }
  if (value && !is_debug_build) {
    std::lock_guard lock(sinks_mutex_);
    detail::CallbackFrame guard(this);
    std::erase_if(sinks_, [](const auto &sink) { return sink->is_console(); });
  }
}

Result Logger::add_sink(std::unique_ptr<Sink> sink) {
  if (in_callback()) return std::unexpected(Error{ErrorCode::ReentrantCall, {}});
  if (!sink) return std::unexpected(Error{ErrorCode::InvalidConfig, {}});
  std::lock_guard control(control_mutex_);
  if (!accepting_.load()) return std::unexpected(Error{ErrorCode::Stopped, {}});
  std::lock_guard lock(sinks_mutex_);
  detail::CallbackFrame guard(this);
  try { sinks_.push_back(std::move(sink)); }
  catch (const std::bad_alloc &) { return std::unexpected(Error{ErrorCode::AllocationFailed, {}}); }
  return {};
}

Result Logger::remove_console_sinks() {
  if (in_callback()) return std::unexpected(Error{ErrorCode::ReentrantCall, {}});
  std::lock_guard control(control_mutex_);
  std::lock_guard lock(sinks_mutex_);
  detail::CallbackFrame guard(this);
  std::erase_if(sinks_, [](const auto &sink) { return sink->is_console(); });
  return {};
}

Result Logger::init_console(ConsoleSinkConfig config) {
  if (in_callback()) return std::unexpected(Error{ErrorCode::ReentrantCall, {}});
  std::lock_guard control(control_mutex_);
  if (!accepting_.load()) return std::unexpected(Error{ErrorCode::Stopped, {}});
  std::lock_guard lock(sinks_mutex_);
  detail::CallbackFrame guard(this);
  try {
    sinks_.reserve(sinks_.size() + 1);
    std::erase_if(sinks_, [](const auto &sink) { return sink->is_console(); });
    if (!only_in_debug_build() || is_debug_build)
      sinks_.push_back(std::make_unique<ConsoleSink>(std::move(config)));
    return {};
  } catch (const std::system_error &e) {
    return std::unexpected(Error{ErrorCode::SinkFailure, e.code()});
  } catch (const std::bad_alloc &) {
    return std::unexpected(Error{ErrorCode::AllocationFailed, {}});
  } catch (const std::invalid_argument &) {
    return std::unexpected(Error{ErrorCode::InvalidConfig, {}});
  }
}

void Logger::drop_front() noexcept {
  auto &record = queue_[head_];
  queued_bytes_ -= record.payload_bytes;
  dropped_count_.fetch_add(1, std::memory_order_relaxed);
  dropped_by_level_[static_cast<size_t>(record.level)].fetch_add(1, std::memory_order_relaxed);
  record = LogMessage{};
  head_ = (head_ + 1) % queue_.size();
  --count_;
}

bool Logger::enqueue(Level level, const std::source_location &loc, std::string_view text) noexcept {
  if (!enabled(level)) return false;
  if (text.size() > max_record_bytes_.load()) return reject(ErrorCode::RecordTooLarge);
  try { return enqueue_owned(level, loc, std::string(text)); }
  catch (const std::bad_alloc &) { return reject(ErrorCode::AllocationFailed); }
}

bool Logger::enqueue_with_fields(Level level, const std::source_location &loc,
                                  std::string_view text, std::vector<Field> fields) noexcept {
  if (!enabled(level)) return false;
  if (text.size() > max_record_bytes_.load()) return reject(ErrorCode::RecordTooLarge);
  try { return enqueue_owned(level, loc, std::string(text), std::move(fields)); }
  catch (const std::bad_alloc &) { return reject(ErrorCode::AllocationFailed); }
}

bool Logger::enqueue_owned(Level level, const std::source_location &loc,
                            std::string text, std::vector<Field> fields) noexcept {
  if (!enabled(level)) return false;
  if (in_callback()) return reject(ErrorCode::ReentrantCall);
  try {
    LogMessage record;
    record.level = level;
    record.time = std::chrono::system_clock::now();
    record.thread_id = get_thread_id();
    record.thread_name.assign(detail::thread_name.bytes.data(), detail::thread_name.size);
    record.loc = loc;
    record.text = std::move(text);
    record.fields = std::move(fields);
    const auto limit = max_record_bytes_.load(std::memory_order_relaxed);
    size_t bytes = 0;
    const auto add = [&](size_t size) {
      if (size > limit - bytes) return false;
      bytes += size;
      return true;
    };
    if (!add(record.text.size()) || !add(record.thread_name.size()))
      return reject(ErrorCode::RecordTooLarge);
    for (const auto &field : record.fields) {
      if (!add(sizeof(Field)) || !add(field.key.size())) return reject(ErrorCode::RecordTooLarge);
      if (const auto *value = std::get_if<std::string>(&field.value); value && !add(value->size()))
        return reject(ErrorCode::RecordTooLarge);
    }
    record.payload_bytes = bytes;
    std::unique_lock lock(queue_mutex_);
    if (!enabled(level)) return false;
    if (record.fields.size() > config_.max_fields) return reject(ErrorCode::TooManyFields);
    if (bytes > config_.max_record_bytes) return reject(ErrorCode::RecordTooLarge);
    started_ = true;
    if (config_.mode == Mode::Synchronous) {
      record.sequence = ++next_sequence_;
      lock.unlock();
      const bool ok = process_batch(std::span<const LogMessage>(&record, 1), false);
      return flush_sinks() && ok;
    }
    if (bytes > config_.max_queue_bytes) return reject(ErrorCode::RecordTooLarge);
    if (!worker_.joinable()) {
      try { worker_ = std::jthread([this] { worker_loop(); }); }
      catch (const std::system_error &e) {
        record_error({ErrorCode::WorkerStartFailed, e.code()});
        rejected_count_.fetch_add(1); return false;
      }
    }
    if (config_.overflow == OverflowPolicy::DropNewest &&
        (count_ == queue_.size() || bytes > config_.max_queue_bytes - queued_bytes_)) {
      dropped_count_.fetch_add(1, std::memory_order_relaxed);
      dropped_by_level_[static_cast<size_t>(level)].fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    while (count_ == queue_.size() || bytes > config_.max_queue_bytes - queued_bytes_) drop_front();
    record.sequence = ++next_sequence_;
    queue_[tail_] = std::move(record);
    tail_ = (tail_ + 1) % queue_.size();
    queued_bytes_ += bytes;
    ++count_;
    high_watermark_ = (std::max)(high_watermark_, count_);
    accepted_count_.fetch_add(1, std::memory_order_relaxed);
    lock.unlock();
    queue_cv_.notify_one();
    return true;
  } catch (const std::bad_alloc &) { return reject(ErrorCode::AllocationFailed); }
}

bool Logger::process_batch(std::span<const LogMessage> batch, bool asynchronous) noexcept {
  std::lock_guard lock(sinks_mutex_);
  detail::CallbackFrame guard(this);
  if (!asynchronous && !accepting_.load()) return false;
  bool ok = true;
  for (const auto &message : batch) {
    if (!asynchronous) accepted_count_.fetch_add(1, std::memory_order_relaxed);
    for (auto &sink : sinks_) {
      if (!sink->enabled(message.level)) continue;
      try { sink->write(message); }
      catch (const std::system_error &e) {
        record_error({ErrorCode::SinkFailure, e.code()}); ++sink_error_count_; ok = false;
      } catch (...) {
        record_error({ErrorCode::SinkFailure, {}}); ++sink_error_count_; ok = false;
      }
    }
    processed_count_.fetch_add(1, std::memory_order_relaxed);
  }
  return ok;
}

bool Logger::flush_sinks_unsafe() noexcept {
  bool ok = true;
  detail::CallbackFrame guard(this);
  for (auto &sink : sinks_) {
    try { sink->flush(); }
    catch (const std::system_error &e) {
      record_error({ErrorCode::SinkFailure, e.code()}); ++sink_error_count_; ok = false;
    } catch (...) {
      record_error({ErrorCode::SinkFailure, {}}); ++sink_error_count_; ok = false;
    }
  }
  return ok;
}

bool Logger::flush_sinks() noexcept {
  std::lock_guard lock(sinks_mutex_);
  return flush_sinks_unsafe();
}

void Logger::worker_loop() noexcept {
  std::array<LogMessage, 256> batch;
  uint64_t revision = 0;
  auto deadline = std::chrono::steady_clock::time_point::max();
  for (;;) {
    size_t size = 0;
    bool timed_flush = false;
    {
      std::unique_lock lock(queue_mutex_);
      const auto refresh_deadline = [&] {
        revision = config_revision_;
        deadline = config_.flush_interval.count() == 0
            ? std::chrono::steady_clock::time_point::max()
            : std::chrono::steady_clock::now() + config_.flush_interval;
      };
      if (deadline == std::chrono::steady_clock::time_point::max() || revision != config_revision_)
        refresh_deadline();
      queue_cv_.wait_until(lock, deadline, [&] {
        return stopping_ || count_ > 0 || flush_pending_ || revision != config_revision_;
      });
      if (revision != config_revision_) refresh_deadline();
      timed_flush = std::chrono::steady_clock::now() >= deadline;
      if (stopping_ && count_ == 0) break;
      while (count_ > 0 && size < batch.size()) {
        queued_bytes_ -= queue_[head_].payload_bytes;
        batch[size++] = std::move(queue_[head_]);
        head_ = (head_ + 1) % queue_.size();
        --count_;
      }
    }
    bool urgent = false;
    if (size) {
      process_batch(std::span<const LogMessage>(batch.data(), size), true);
      for (size_t i = 0; i < size; ++i) urgent |= is_error_level(batch[i].level);
    }
    bool barrier;
    {
      std::lock_guard lock(queue_mutex_);
      if (size) completed_sequence_ = batch[size - 1].sequence;
      barrier = flush_pending_ && (completed_sequence_ >= flush_target_ || count_ == 0);
    }
    for (size_t i = 0; i < size; ++i) batch[i] = LogMessage{};
    if (barrier || urgent || timed_flush || std::chrono::steady_clock::now() >= deadline) {
      const bool ok = flush_sinks();
      std::lock_guard lock(queue_mutex_);
      if (barrier) { flush_ok_ = ok; flush_pending_ = false; flushed_cv_.notify_all(); }
      deadline = config_.flush_interval.count() == 0
          ? std::chrono::steady_clock::time_point::max()
          : std::chrono::steady_clock::now() + config_.flush_interval;
    }
  }
}

Result Logger::flush() {
  if (in_callback()) return std::unexpected(Error{ErrorCode::ReentrantCall, {}});
  std::lock_guard control(control_mutex_);
  std::unique_lock lock(queue_mutex_);
  if (stopping_) return std::unexpected(Error{ErrorCode::Stopped, {}});
  if (!worker_.joinable()) {
    lock.unlock();
    if (!flush_sinks()) return std::unexpected(last_error());
    return {};
  }
  flush_target_ = next_sequence_;
  flush_pending_ = true;
  queue_cv_.notify_one();
  flushed_cv_.wait(lock, [&] { return !flush_pending_; });
  if (!flush_ok_) return std::unexpected(last_error());
  return {};
}

bool Logger::shutdown() noexcept {
  if (in_callback()) return reject(ErrorCode::ReentrantCall);
  std::lock_guard control(control_mutex_);
  {
    std::lock_guard lock(queue_mutex_);
    if (stopped_) return true;
    accepting_.store(false, std::memory_order_release);
    stopping_ = true;
  }
  queue_cv_.notify_one();
  if (worker_.joinable()) worker_.join();
  bool ok;
  {
    std::lock_guard lock(sinks_mutex_);
    detail::CallbackFrame guard(this);
    ok = flush_sinks_unsafe();
    sinks_.clear();
  }
  { std::lock_guard lock(queue_mutex_); stopped_ = true; }
  return ok;
}

bool Logger::set_queue_capacity(size_t capacity) {
  if (in_callback()) return reject(ErrorCode::ReentrantCall);
  std::lock_guard control(control_mutex_);
  std::lock_guard lock(queue_mutex_);
  auto config = config_;
  config.level = min_level_.load();
  config.only_in_debug_build = only_in_debug_build_.load();
  config.queue_capacity = capacity;
  if (!valid_config(config)) return false;
  const bool ok = configure_locked(config).has_value();
  queue_cv_.notify_one();
  return ok;
}

uint32_t Logger::get_thread_id() const noexcept {
#if defined(_WIN32)
  return static_cast<uint32_t>(GetCurrentThreadId());
#else
  return static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
}

void Logger::set_thread_name(std::string_view name) noexcept {
  auto size = (std::min)(name.size(), detail::thread_name.bytes.size());
  if (size < name.size())
    while (size && (static_cast<unsigned char>(name[size]) & 0xc0) == 0x80) --size;
  std::copy_n(name.data(), size, detail::thread_name.bytes.data());
  detail::thread_name.size = size;
}

std::string Logger::get_thread_name() const {
  return {detail::thread_name.bytes.data(), detail::thread_name.size};
}

Statistics Logger::statistics() const {
  Statistics result;
  result.accepted = accepted_count_.load(); result.processed = processed_count_.load();
  result.dropped = dropped_count_.load(); result.rejected = rejected_count_.load();
  result.sink_errors = sink_error_count_.load();
  for (size_t i = 0; i < result.dropped_by_level.size(); ++i)
    result.dropped_by_level[i] = dropped_by_level_[i].load();
  std::lock_guard lock(queue_mutex_);
  result.queued = count_; result.queued_bytes = queued_bytes_;
  result.high_watermark = high_watermark_;
  return result;
}

} // namespace log_core
