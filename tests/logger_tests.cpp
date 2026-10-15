#include "logger/core/logger.h"
#include <iostream>
#include <semaphore>
#include <stdexcept>

using namespace log_core;
using namespace std::chrono_literals;
#define CHECK(expression) do { if (!(expression)) \
  throw std::runtime_error("check failed: " #expression); } while (false)

struct Shared {
  std::vector<LogMessage> records;
  std::atomic<int> flushes{0};
  std::atomic<bool> destroyed{false};
};
struct Capture : Sink {
  std::shared_ptr<Shared> data;
  explicit Capture(std::shared_ptr<Shared> value) : data(std::move(value)) {}
  ~Capture() override { data->destroyed = true; }
  void write(const LogMessage &m) override { data->records.push_back(m); }
  void flush() override { ++data->flushes; }
};
struct Gate : Capture {
  std::binary_semaphore entered{0}, release{0}, second_entered{0}, second_release{0};
  bool second = false;
  using Capture::Capture;
  void write(const LogMessage &m) override {
    Capture::write(m);
    if (m.text == "gate") {
      entered.release();
      if (!release.try_acquire_for(10s)) throw std::runtime_error("gate timeout");
    }
    if (second && m.text == "C") {
      second_entered.release();
      if (!second_release.try_acquire_for(10s)) throw std::runtime_error("gate timeout");
    }
  }
};
LoggerConfig async_config(size_t capacity = 4096) {
  LoggerConfig c;
  c.mode = Mode::Asynchronous;
  c.queue_capacity = capacity;
  return c;
}
void put(Logger &logger, std::string_view text) {
  CHECK(logger.enqueue(Level::Info, std::source_location::current(), text));
}
void check_text(const Shared &data, std::initializer_list<std::string_view> expected) {
  CHECK(data.records.size() == expected.size());
  size_t i = 0;
  for (auto text : expected) CHECK(data.records[i++].text == text);
}
void queue_test(std::string_view test) {
  auto config = async_config(3);
  if (test == "drop_newest") config.overflow = OverflowPolicy::DropNewest;
  Logger logger(config);
  auto data = std::make_shared<Shared>();
  auto gate = std::make_unique<Gate>(data);
  auto *control = gate.get();
  gate->second = test == "ring";
  CHECK(logger.add_sink(std::move(gate)));
  put(logger, "gate");
  CHECK(control->entered.try_acquire_for(5s));
  for (auto text : {"A", "B", "C"}) put(logger, text);
  for (auto text : {"D", "E"}) {
    const bool ok = logger.enqueue(Level::Info, std::source_location::current(), text);
    CHECK(ok == (test != "drop_newest"));
  }
  if (test == "resize") {
    logger.set_level(Level::Trace);
    CHECK(logger.set_queue_capacity(2));
    CHECK(logger.get_level() == Level::Trace);
    CHECK(!logger.set_queue_capacity(0));
  }
  control->release.release();
  if (test == "ring") {
    CHECK(control->second_entered.try_acquire_for(5s));
    put(logger, "F");
    control->second_release.release();
  }
  CHECK(logger.shutdown());
  if (test == "ring") check_text(*data, {"gate", "C", "D", "E", "F"});
  else if (test == "resize") check_text(*data, {"gate", "D", "E"});
  else check_text(*data, {"gate", "A", "B", "C"});
  CHECK(data->destroyed);
  CHECK(logger.dropped_count() == (test == "resize" ? 3u : 2u));
  CHECK(logger.statistics().dropped_by_level[static_cast<size_t>(Level::Info)] ==
        logger.dropped_count());
}
void flush_test() {
  Logger logger(async_config());
  auto data = std::make_shared<Shared>();
  CHECK(logger.add_sink(std::make_unique<Capture>(data)));
  for (int i = 0; i < 100; ++i) {
    CHECK(logger.write<Level::Info>("marker {}", i));
    CHECK(logger.flush());
    CHECK(data->records.size() == static_cast<size_t>(i + 1));
    CHECK(data->records.back().text == std::format("marker {}", i));
  }
}
void shutdown_test() {
  for (int iteration = 0; iteration < 100; ++iteration) {
    Logger logger(async_config());
    auto data = std::make_shared<Shared>();
    CHECK(logger.add_sink(std::make_unique<Capture>(data)));
    put(logger, "idle");
    CHECK(logger.flush());
    std::jthread a([&] { CHECK(logger.shutdown()); });
    std::jthread b([&] { CHECK(logger.shutdown()); });
    a.join(); b.join();
    CHECK(data->destroyed);
    CHECK(!logger.enabled(Level::Info));
    CHECK(!logger.add_sink(std::make_unique<Capture>(data)));
    CHECK(!logger.flush());
    CHECK(logger.shutdown());
  }
  Logger logger(async_config(32768));
  auto data = std::make_shared<Shared>();
  CHECK(logger.add_sink(std::make_unique<Capture>(data)));
  std::vector<std::jthread> producers;
  for (int thread = 0; thread < 4; ++thread)
    producers.emplace_back([&, thread] {
      for (int i = 0; i < 1000; ++i) CHECK(logger.write<Level::Info>("{}:{}", thread, i));
    });
  producers.clear();
  CHECK(logger.shutdown());
  CHECK(data->records.size() == 4000);
  CHECK(logger.dropped_count() == 0);
  for (size_t i = 1; i < data->records.size(); ++i)
    CHECK(data->records[i].sequence > data->records[i - 1].sequence);
}
struct RecursiveSink : Sink {
  Logger &logger;
  bool checked = false;
  explicit RecursiveSink(Logger &owner) : logger(owner) {}
  void write(const LogMessage &) override {
    CHECK(logger.flush().error().code == ErrorCode::ReentrantCall);
    CHECK(!logger.shutdown());
    CHECK(!logger.enqueue(Level::Info, std::source_location::current(), "recursive"));
    CHECK(logger.remove_console_sinks().error().code == ErrorCode::ReentrantCall);
    checked = true;
  }
  void flush() override {}
};
void reentrant_test() {
  Logger logger;
  auto sink = std::make_unique<RecursiveSink>(logger);
  auto *view = sink.get();
  CHECK(logger.add_sink(std::move(sink)));
  put(logger, "outer");
  CHECK(view->checked);
  CHECK(logger.statistics().rejected == 2);
}
struct Marker { Logger *logger; bool nested; };
template <> struct std::formatter<Marker> {
  constexpr auto parse(std::format_parse_context &c) { return c.begin(); }
  auto format(const Marker &v, std::format_context &c) const {
    if (!v.nested) CHECK(v.logger->write<Level::Info>("inner {}", Marker{v.logger, true}));
    return std::format_to(c.out(), "{}", v.nested ? "NESTED" : "OUTER");
  }
};
struct Throwing {};
template <> struct std::formatter<Throwing> {
  constexpr auto parse(std::format_parse_context &c) { return c.begin(); }
  auto format(const Throwing &, std::format_context &c) const {
    throw std::runtime_error("formatter failed");
    return c.out();
  }
};
void formatting_test() {
  Logger logger;
  auto data = std::make_shared<Shared>();
  CHECK(logger.add_sink(std::make_unique<Capture>(data)));
  CHECK(logger.write<Level::Info>("before {} after", Marker{&logger, false}));
  CHECK(!logger.write<Level::Info>("{}", Throwing{}));
  CHECK(logger.last_error().code == ErrorCode::FormattingFailed);
  check_text(*data, {"inner NESTED", "before OUTER after"});
}
void limits_test() {
  auto config = async_config(3);
  config.max_record_bytes = 256;
  config.max_queue_bytes = 4;
  config.max_fields = 1;
  Logger logger(config);
  auto data = std::make_shared<Shared>();
  auto gate = std::make_unique<Gate>(data);
  auto *control = gate.get();
  CHECK(logger.add_sink(std::move(gate)));
  put(logger, "gate");
  CHECK(control->entered.try_acquire_for(5s));
  put(logger, "AA"); put(logger, "BB"); put(logger, "CC");
  CHECK(logger.statistics().queued_bytes == 4);
  CHECK(logger.dropped_count() == 1);
  CHECK(!logger.write<Level::Info>("{:10000}", 1));
  CHECK(logger.last_error().code == ErrorCode::RecordTooLarge);
  CHECK(!logger.write_fields<Level::Info>({{"a", 1}, {"b", 2}}, "fields"));
  CHECK(logger.last_error().code == ErrorCode::TooManyFields);
  control->release.release();
  CHECK(logger.shutdown());
  check_text(*data, {"gate", "BB", "CC"});
}
void config_test() {
  Logger logger;
  auto invalid = async_config(0);
  invalid.level = Level::Trace;
  CHECK(logger.configure(invalid).error().code == ErrorCode::InvalidConfig);
  CHECK(logger.get_level() == Level::Info);
  int calls = 0;
  CHECK(!logger.lazy(Level::Debug, [&] { ++calls; return "unused"; }));
  CHECK(calls == 0);
  auto data = std::make_shared<Shared>();
  auto sink = std::make_unique<Capture>(data);
  sink->set_level(Level::Error);
  CHECK(logger.add_sink(std::move(sink)));
  put(logger, "filtered at sink");
  CHECK(logger.write<Level::Error>("kept"));
  check_text(*data, {"kept"});
  CHECK(logger.configure(async_config()).error().code == ErrorCode::ModeAlreadyStarted);
  logger.set_level(Level::Off);
  CHECK(!logger.enabled(Level::Critical));
}
void timer_test() {
  auto config = async_config();
  config.flush_interval = 20ms;
  Logger logger(config);
  auto data = std::make_shared<Shared>();
  CHECK(logger.add_sink(std::make_unique<Capture>(data)));
  put(logger, "one");
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (data->flushes == 0 && std::chrono::steady_clock::now() < deadline)
    std::this_thread::yield();
  CHECK(data->flushes > 0);
  CHECK(logger.shutdown());
  CHECK(data->records.size() == 1);
}
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  try {
    const std::string_view test = argv[1];
    if (test == "ring" || test == "drop_newest" || test == "resize") queue_test(test);
    else if (test == "flush") flush_test(); else if (test == "shutdown") shutdown_test();
    else if (test == "reentrant") reentrant_test(); else if (test == "formatting") formatting_test();
    else if (test == "limits") limits_test(); else if (test == "config") config_test();
    else if (test == "timer") timer_test(); else return 2;
    std::cout << "PASS " << test << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
