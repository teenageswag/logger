# C++23 Async Logger

A high-performance async logging library for Windows, built with modern C++23. Uses a background thread to process logs, so your main application loop never blocks.

## Features

* **Zero-Cost Compile-Time Filtering** -- Logs below the configured threshold are compiled out entirely.
* **Async Non-Blocking Logging** -- Bounded ring buffer with a background worker thread. On overflow, oldest messages are dropped (never blocks the caller).
* **Rich Console Sink** -- ANSI color highlighting, automatic Windows console detection/allocation.
* **File Sink with Rotation** -- Size-based rotation with configurable number of backup files and periodic flushing.
* **JSON Sink** -- JSON Lines output for log aggregation systems (ELK, Loki, etc.).
* **Structured Logging** -- Attach key-value pairs to any log message for machine-readable context.
* **Thread Naming** -- Register human-readable thread names that appear in log output.
* **Modern C++23 API** -- `std::format`, `std::source_location`, `std::jthread`. No macros required.

## Installation

Include the umbrella header in your project:

```cpp
#define LOG_ACTIVE_LEVEL log::Level::TRACE
#include "logger/logger.h"
```

Add the source files to your build:

```
logger/src/logger/
  logger.h              ← single include entry point
  core/types.h
  core/logger.h / .cpp
  sinks/sink.h
  sinks/console_sink.h / .cpp
  sinks/file_sink.h / .cpp
  sinks/json_sink.h / .cpp
```

## Console Logging

Console logging works out of the box for console applications. For GUI applications (or DLLs without a host console), explicitly allocate a console:

```cpp
// Basic
log::init_console();

// Custom window title and buffer size
log::ConsoleConfig cfg;
cfg.title = "My App";
cfg.buffer_width = 150;
cfg.buffer_height = 40;
log::init_console(cfg);
```

## File Logging

File logging is disabled by default. Enable it with `add_file_sink`:

```cpp
// Rotate after 1 MB, keep 3 backup files (.1, .2, .3)
log::add_file_sink("app.log", 1 * 1024 * 1024, 3);
```

Once the file reaches `max_size`, it is rotated to `.1` (oldest backups beyond `max_files` are deleted). Files are periodically flushed (default: every 1 second).

## JSON Logging

Output logs as JSON Lines -- one JSON object per line, compatible with log aggregation tools:

```cpp
log::add_json_sink("logs.json", 5 * 1024 * 1024, 3);
```

Example output:

```json
{"timestamp":"2026-07-06T12:00:00","level":"INF","file":"main.cpp","line":42,"thread":1234,"message":"User logged in","user_id":"42","ip":"192.168.1.1"}
```

Structured kv pairs are automatically serialized as top-level JSON fields.

## Structured Logging (kv pairs)

Attach key-value context to any log message:

```cpp
log::info_ctx("User authenticated", "user", "Artem", "user_id", 42, "ip", "192.168.1.1");
log::error_ctx("DB query failed", "host", "db-primary", "port", 5432, "timeout_ms", 5000);
```

Console output:

```
[12:00:00] [INF] [main.cpp:42] User authenticated {user=Artem, user_id=42, ip=192.168.1.1}
```

Available for all levels: `trace_ctx`, `debug_ctx`, `info_ctx`, `warn_ctx`, `error_ctx`.

## Thread Naming

Register a human-readable name for the current thread:

```cpp
log::set_thread_name("WorkerPool-1");
```

## Runtime Log Filtering

Filter by severity threshold at runtime:

```cpp
log::set_level(log::Level::INFO);   // Only INFO, WARN, ERROR
log::set_level(log::Level::TRACE);  // Everything
log::set_level(log::Level::ERROR);  // Only errors
```

Change the threshold at any time without restarting.

## Compile-Time Filtering

Define `LOG_ACTIVE_LEVEL` before including the header to strip logs from the binary entirely:

```cpp
#define LOG_ACTIVE_LEVEL log::Level::INFO
#include "logger/logger.h"

// This compiles to nothing:
log::debug("This is gone");
// This stays:
log::info("This remains");
```

## Overflow Behavior

The ring buffer holds 4096 messages by default. When full, the **oldest** messages are dropped and a counter is incremented:

```cpp
size_t dropped = log::dropped_count();
```

This ensures the logging thread never blocks your application.

## Custom Sinks

Implement the `log::Sink` interface and add your own:

```cpp
class MySink : public log::Sink {
public:
    void write(const log_core::LogMessage &msg) override {
        // Your custom output logic
    }
    void flush() override {}
};

log::instance().add_sink(std::make_unique<MySink>());
```

## Quick Start

```cpp
#include <iostream>
#include <thread>

#define LOG_ACTIVE_LEVEL log::Level::TRACE
#include "logger/logger.h"

int main() {
    log::init_console();
    log::add_file_sink("app.log", 1 * 1024 * 1024, 3);
    log::add_json_sink("logs.json");
    log::set_level(log::Level::TRACE);

    log::info("Application started");

    // Formatted logging
    int user_id = 42;
    log::info("User {} logged in", user_id);

    // Structured logging
    log::info_ctx("Request processed", "user_id", user_id, "latency_ms", 12);

    // Thread naming + multithreading
    log::set_thread_name("Main");

    auto worker = [](int id) {
        log::set_thread_name("Worker-" + std::to_string(id));
        for (int i = 0; i < 5; ++i)
            log::info_ctx("Processing", "worker", id, "item", i);
    };

    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    t1.join();
    t2.join();

    log::info("Done (dropped: {} messages)", log::dropped_count());
    return 0;
}
```

## API Reference

| Function | Description |
|---|---|
| `log::info("msg {}", arg)` | Log at INFO level with format string |
| `log::info_ctx("msg {}", arg, {{"k","v"}})` | Log with structured kv pairs |
| `log::set_level(level)` | Set runtime severity threshold |
| `log::init_console(cfg)` | Initialize console sink |
| `log::add_file_sink(path, max_size, max_files)` | Add file sink with rotation |
| `log::add_json_sink(path, max_size, max_files)` | Add JSON Lines sink |
| `log::set_thread_name(name)` | Register thread name for output |
| `log::dropped_count()` | Get count of dropped messages |
| `log::instance()` | Access Logger directly for custom sinks |
