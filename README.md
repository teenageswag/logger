# C++23 Async Logger

A modern asynchronous logging library for Windows and DLL-based applications.
Log records are pushed into a bounded ring buffer and processed by a dedicated
worker thread.

## Features

- C++23 API with `std::format`, `std::source_location`, and `std::jthread`;
- asynchronous bounded queue with drop-oldest overflow policy;
- runtime severity filtering and compile-time filtering;
- log levels: `Trace`, `Debug`, `Info`, `Success`, `Warn`, `Error`, `Critical`;
- console, rotating file, and JSON Lines sinks;
- typed structured fields for JSON output;
- optional ANSI foreground and background colors;
- thread names and thread IDs in log records;
- safe console ownership for injected DLLs;
- sink error isolation and dropped-message statistics.

## Quick start

```cpp
#define LOG_ACTIVE_LEVEL log::Level::Trace
#include "logger/logger.h"

int main() {
    log::Config logger_config;
    logger_config.level = log::Level::Info;
    logger_config.only_in_debug_build = false;
    log::configure(logger_config);

    log::ConsoleConfig console_config;
    console_config.enable_colors = true;
    console_config.enable_backgrounds = true;
    log::init_console(console_config);

    log::info("Application started: {}", "demo");
    log::success("Loaded {} records", 42);
    log::warn("Disk usage is high: {}%", 87);
    log::error("Database connection failed: {}", "db-primary");
    log::critical("Fatal service error");

    log::flush();
    return 0;
}
```

## Log levels

Levels are ordered by severity:

```text
Trace < Debug < Info < Success < Warn < Error < Critical
```

Use `log::set_level()` or `Config::level` to set the runtime threshold.
`Error` and `Critical` are written to stderr/`CONOUT$` and trigger an immediate
sink flush.

## Central configuration

Logger-wide options are configured through `log::Config`:

```cpp
log::Config config;
config.level = log::Level::Trace;
config.only_in_debug_build = true;
config.queue_capacity = 4096;
log::configure(config);
```

### Debug-only builds

When `only_in_debug_build` is `true`:

- Debug builds (`_DEBUG`) log normally and may create a console;
- Release builds do not create a logger console;
- existing logger-owned consoles are released;
- all log calls are rejected before `std::format` is executed.

When it is `false`, logging is available in every build configuration.

This removes the need to wrap every call manually:

```cpp
// No #ifdef _DEBUG is required.
log::info("Detailed diagnostic information");
```

`LOG_ACTIVE_LEVEL` remains available for compile-time removal of lower levels.

## Console logging

```cpp
log::ConsoleConfig config;
config.auto_alloc = true;
config.enable_colors = true;
config.enable_backgrounds = true;
config.show_thread_name = true;
config.show_thread_id = false;
log::init_console(config);
```

Console output supports optional ANSI foreground and background colors. ASCII
icons are intentionally not used.

For DLL usage, `ConsoleSink`:

- allocates a console only when `auto_alloc` is enabled and no console exists;
- tracks whether the console was created by the logger;
- closes only its own `CONOUT$` handles;
- never calls `freopen_s` for the host process;
- never closes or replaces the host's `stdin`, `stdout`, or `stderr`;
- frees only a console owned by the logger.

Call `log::shutdown()` before unloading a DLL when possible.

## File logging

```cpp
log::add_file_sink("logs/app.log", 5 * 1024 * 1024, 3);
```

The file sink supports size-based rotation. Parent directories are created
automatically.

- `max_size == 0` disables size-based rotation;
- `max_files == 0` disables rotation backups;
- filesystem errors are detected instead of being silently ignored.

## JSON logging

```cpp
log::add_json_sink("logs/events.json", 10 * 1024 * 1024, 3);
```

JSON Lines records include timestamp, level, source file, line, thread data,
message text, and structured fields. Structured values preserve their JSON
types:

```cpp
log::info({{"user_id", 42}, {"admin", true}, {"latency_ms", 12.5}},
          "User authenticated");
```

## Thread names and statistics

```cpp
log::set_thread_name("Worker-1");

const auto dropped = log::dropped_count();
const auto sink_errors = log::sink_error_count();
```

Thread metadata is captured when the record is created, so it remains correct
even though sinks run on a background worker thread.

## Lifecycle and queue control

```cpp
log::flush();
log::set_queue_capacity(4096);
log::shutdown();
```

`shutdown()` stops the worker, drains queued records, flushes sinks, and makes
subsequent log calls no-ops. The queue never accepts a zero capacity.

## Custom sinks

```cpp
class MySink final : public log::Sink {
public:
    void write(const log_core::LogMessage& message) override {
        // Custom output.
    }

    void flush() override {}
};

log::add_sink(std::make_unique<MySink>());
```

Exceptions from one sink are isolated and counted by `sink_error_count()` so a
broken sink cannot terminate the logging worker.

## Project layout

```text
logger/src/logger/logger.h
logger/src/logger/core/types.h
logger/src/logger/core/logger.h/.cpp
logger/src/logger/sinks/sink.h
logger/src/logger/sinks/rotating_file.h
logger/src/logger/sinks/console_sink.h/.cpp
logger/src/logger/sinks/file_sink.h/.cpp
logger/src/logger/sinks/json_sink.h/.cpp
```

`logger_include.h` remains as a compatibility forwarding header. New code
should include `logger/logger.h`.
