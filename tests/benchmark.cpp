#include "logger/logger.h"
#include <algorithm>
#include <barrier>
#include <iostream>

using namespace log_core;
using Clock = std::chrono::steady_clock;

struct NullSink final : Sink {
  void write(const LogMessage &) override {}
  void flush() override {}
};

void run(Mode mode, int threads, size_t bytes, int fields, int iterations,
         std::string_view sink_name = "null", bool disabled = false) {
  LoggerConfig config;
  config.mode = mode;
  config.level = disabled ? Level::Off : Level::Info;
  config.queue_capacity = 8192;
  config.max_queue_bytes = 64 * 1024 * 1024;
  Logger logger(config);
  const auto path = std::filesystem::current_path() / "benchmark-output.tmp";
  std::filesystem::remove(path);
  std::unique_ptr<Sink> sink;
  if (sink_name == "file")
    sink = std::make_unique<FileSink>(path, 0, 0);
  else if (sink_name == "json")
    sink = std::make_unique<JsonSink>(JsonSinkConfig{path, 0, 0});
  else
    sink = std::make_unique<NullSink>();
  if (!logger.add_sink(std::move(sink)))
    throw std::runtime_error("benchmark sink failed");
  const std::string text(bytes, 'x');
  std::vector<std::vector<int64_t>> samples(static_cast<size_t>(threads));
  for (auto &sample : samples)
    sample.resize(static_cast<size_t>(iterations));
  std::barrier ready(threads + 1);
  std::vector<std::jthread> producers;
  for (int thread = 0; thread < threads; ++thread) {
    producers.emplace_back([&, thread] {
      ready.arrive_and_wait();
      for (int i = 0; i < iterations; ++i) {
        const auto start = Clock::now();
        if (fields)
          logger.write_fields<Level::Info>(
              {{"worker", thread}, {"item", i}, {"ok", true}, {"latency", 1.25}, {"name", "event"}},
              "{}", text);
        else
          logger.write<Level::Info>("{}", text);
        samples[static_cast<size_t>(thread)][static_cast<size_t>(i)] =
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
      }
    });
  }
  const auto start = Clock::now();
  ready.arrive_and_wait();
  producers.clear();
  if (!logger.flush() || !logger.shutdown())
    throw std::runtime_error("benchmark delivery failed");
  const auto seconds = std::chrono::duration<double>(Clock::now() - start).count();
  const auto stats = logger.statistics();
  std::vector<int64_t> combined;
  for (const auto &sample : samples)
    combined.insert(combined.end(), sample.begin(), sample.end());
  std::sort(combined.begin(), combined.end());
  const auto percentile = [&](size_t value) {
    return combined[(combined.size() - 1) * value / 100];
  };
  std::cout << (mode == Mode::Synchronous ? "sync" : "async") << ',' << sink_name << ',' << disabled
            << ',' << threads << ',' << bytes << ',' << fields << ',' << combined.size() << ','
            << percentile(50) << ',' << percentile(95) << ',' << percentile(99) << ','
            << static_cast<uint64_t>(combined.size() / seconds) << ','
            << static_cast<uint64_t>(stats.processed / seconds) << ',' << stats.dropped << ','
            << stats.high_watermark << '\n';
  std::filesystem::remove(path);
}

int main() {
  try {
    std::cout << "mode,sink,disabled,threads,bytes,fields,calls,p50_ns,p95_ns,p99_ns,"
                 "attempts_per_sec,processed_per_sec,dropped,high_watermark\n";
    for (auto mode : {Mode::Synchronous, Mode::Asynchronous})
      for (int threads : {1, 2, 8})
        for (size_t bytes : {32u, 256u, 4096u})
          run(mode, threads, bytes, 0, 5000);
    for (int threads : {1, 8}) {
      run(Mode::Asynchronous, threads, 256, 5, 5000);
      run(Mode::Asynchronous, threads, 256, 0, 5000, "null", true);
    }
    run(Mode::Asynchronous, 1, 256, 5, 5000, "file");
    run(Mode::Asynchronous, 1, 256, 5, 5000, "json");
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
