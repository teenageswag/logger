#pragma once

#include "../sinks/sink.h"
#include "error.h"
#include "types.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <iterator>
#include <memory>
#include <mutex>
#include <span>
#include <thread>

namespace log_core {

#if defined(_DEBUG)
inline constexpr bool is_debug_build = true;
#else
inline constexpr bool is_debug_build = false;
#endif

enum class Mode { Synchronous, Asynchronous };
enum class OverflowPolicy { DropOldest, DropNewest };

struct LoggerConfig {
  Level level = Level::Info;
  bool only_in_debug_build = false;
  size_t queue_capacity = 4096;
  Mode mode = Mode::Synchronous;
  OverflowPolicy overflow = OverflowPolicy::DropOldest;
  size_t max_queue_bytes = 8 * 1024 * 1024;
  size_t max_record_bytes = 64 * 1024;
  size_t max_fields = 32;
  std::chrono::milliseconds flush_interval{1000};
};

struct Statistics {
  uint64_t accepted = 0, processed = 0, dropped = 0, rejected = 0, sink_errors = 0;
  size_t queued = 0, queued_bytes = 0, high_watermark = 0;
  std::array<uint64_t, 7> dropped_by_level{};
};

namespace detail {
// Trivial TLS avoids destructors running after module teardown.
struct ThreadName {
  std::array<char, 128> bytes{};
  size_t size = 0;
};
inline thread_local ThreadName thread_name;
inline std::atomic<bool> default_logger_destroyed{false};
struct CallbackFrame {
  const void *logger;
  CallbackFrame *previous;
  static inline thread_local CallbackFrame *current = nullptr;
  explicit CallbackFrame(const void *owner) noexcept : logger(owner), previous(current) {
    current = this;
  }
  ~CallbackFrame() { current = previous; }
  static bool contains(const void *owner) noexcept {
    for (auto *frame = current; frame; frame = frame->previous)
      if (frame->logger == owner)
        return true;
    return false;
  }
};
} // namespace detail

struct ConsoleSinkConfig;

class Logger {
public:
  explicit Logger(const LoggerConfig &config = {});
  ~Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  static Logger &instance();
  static Logger *try_instance() noexcept;
  Result configure(const LoggerConfig &config);
  void set_level(Level level) noexcept { min_level_.store(level); }
  Level get_level() const noexcept { return min_level_.load(); }
  bool enabled(Level level) const noexcept {
    return level < Level::Off && level >= min_level_.load(std::memory_order_relaxed) &&
           accepting_.load(std::memory_order_acquire) &&
           (!only_in_debug_build_.load(std::memory_order_relaxed) || is_debug_build);
  }
  void set_only_in_debug_build(bool value);
  bool only_in_debug_build() const noexcept { return only_in_debug_build_.load(); }
  bool running() const noexcept { return accepting_.load(std::memory_order_acquire); }

  Result add_sink(std::unique_ptr<Sink> sink);
  Result remove_console_sinks();
  Result init_console(ConsoleSinkConfig config);
  bool enqueue(Level level, const std::source_location &loc, std::string_view text) noexcept;
  bool enqueue_with_fields(Level level, const std::source_location &loc, std::string_view text,
                           std::vector<Field> fields) noexcept;
  bool enqueue_owned(Level level, const std::source_location &loc, std::string text,
                     std::vector<Field> fields = {}) noexcept;

  template <Level L, typename... Args>
  bool write(log_format_string<std::type_identity_t<Args>...> fmt, Args &&...args) noexcept {
    return write_fields<L>({}, fmt, std::forward<Args>(args)...);
  }

  template <Level L, typename... Args>
  bool write_fields(Fields fields, log_format_string<std::type_identity_t<Args>...> fmt,
                    Args &&...args) noexcept {
    if (!enabled(L))
      return false;
    if (in_callback())
      return reject(ErrorCode::ReentrantCall);
    try {
      std::string text;
      const auto limit = max_record_bytes_.load(std::memory_order_relaxed);
      const auto result =
          std::format_to_n(std::back_inserter(text), static_cast<std::ptrdiff_t>(limit), fmt.fmt,
                           std::forward<Args>(args)...);
      if (static_cast<size_t>(result.size) > limit)
        return reject(ErrorCode::RecordTooLarge);
      return enqueue_owned(L, fmt.loc, std::move(text), std::move(fields.values));
    } catch (const std::bad_alloc &) {
      return reject(ErrorCode::AllocationFailed);
    } catch (...) {
      return reject(ErrorCode::FormattingFailed);
    }
  }

  template <typename Factory>
  bool lazy(Level level, Factory &&make_text,
            std::source_location loc = std::source_location::current()) noexcept {
    if (!enabled(level))
      return false;
    if (in_callback())
      return reject(ErrorCode::ReentrantCall);
    try {
      return enqueue_owned(level, loc, std::string(std::forward<Factory>(make_text)()));
    } catch (const std::bad_alloc &) {
      return reject(ErrorCode::AllocationFailed);
    } catch (...) {
      return reject(ErrorCode::FormattingFailed);
    }
  }

  uint32_t get_thread_id() const noexcept;
  void set_thread_name(std::string_view name) noexcept;
  std::string get_thread_name() const;
  uint64_t dropped_count() const noexcept { return dropped_count_.load(); }
  uint64_t sink_error_count() const noexcept { return sink_error_count_.load(); }
  Statistics statistics() const;
  Error last_error() const;
  bool set_queue_capacity(size_t capacity);
  Result flush();
  bool shutdown() noexcept;
  bool reject(ErrorCode code) noexcept;

private:
  struct DefaultTag {};
  explicit Logger(DefaultTag) : Logger(LoggerConfig{}) { is_default_ = true; }
  static bool valid_config(const LoggerConfig &config) noexcept;
  bool in_callback() const noexcept { return detail::CallbackFrame::contains(this); }
  Result configure_locked(const LoggerConfig &config); // control_mutex_ and queue_mutex_ held
  void record_error(Error error) noexcept;
  void drop_front() noexcept; // queue_mutex_ held
  void worker_loop() noexcept;
  bool process_batch(std::span<const LogMessage> batch, bool asynchronous) noexcept;
  bool flush_sinks() noexcept;
  bool flush_sinks_unsafe() noexcept;

  std::mutex control_mutex_;
  mutable std::mutex queue_mutex_;
  std::condition_variable queue_cv_, flushed_cv_;
  LoggerConfig config_;
  std::vector<LogMessage> queue_;
  size_t head_ = 0, tail_ = 0, count_ = 0, queued_bytes_ = 0, high_watermark_ = 0;
  uint64_t next_sequence_ = 0, completed_sequence_ = 0, config_revision_ = 0;
  bool stopping_ = false, stopped_ = false, started_ = false;
  bool flush_pending_ = false, flush_ok_ = true;
  uint64_t flush_target_ = 0;
  std::jthread worker_;
  std::mutex sinks_mutex_;
  std::vector<std::unique_ptr<Sink>> sinks_;
  mutable std::mutex error_mutex_;
  Error last_error_;
  std::atomic<Level> min_level_{Level::Info};
  std::atomic<bool> accepting_{true}, only_in_debug_build_{false};
  std::atomic<size_t> max_record_bytes_{64 * 1024};
  std::atomic<uint64_t> accepted_count_{0}, processed_count_{0};
  std::atomic<uint64_t> dropped_count_{0}, rejected_count_{0}, sink_error_count_{0};
  std::array<std::atomic<uint64_t>, 7> dropped_by_level_{};
  bool is_default_ = false;
};

} // namespace log_core

#include "logger_impl.h"
