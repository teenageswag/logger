# C++23 Zero-Overhead Logger

A high-performance, single-header C++23 logging library for Windows. Designed for production-grade systems requiring zero-cost abstractions, thread-safety, and macro-free public APIs. It uses a background thread to process logs, ensuring your main application loop never blocks.

## Features

* ⚡ **Zero-Cost Compile-Time Filtering**: Logs below the configured threshold are completely compiled out with zero runtime cost.
* 🚀 **Zero-Allocation Hot Path**: Messages are formatted into thread-local buffers, and capacity is reused in the async queue. Fixed-size messages do not allocate after the warmup phase.
* 🧵 **Lock-Based Async Logging**: A dedicated background thread processes log entries via a bounded ring buffer.
* 🎨 **Rich Console Sink**: Automatic ANSI color highlighting for different log levels.
* 📁 **File Sink with Rotation**: Automatically buffers writes and flushes. Includes size-based rotation out-of-the-box.
* 🛠️ **Modern C++23 API**: Utilizes `std::format` and `std::source_location`. No macros required for logging calls.

## Installation

Simply drop `logger.h` into your project and `#include` it. No separate `.cpp` files or dependencies required.

## How to Work With File Logs

By default, the logger **only writes to the console**. Log files are not created automatically to prevent cluttering your disk.

To **enable** file logging, call the `add_file_sink` function, providing a file path and a maximum file size. Once the file reaches the maximum size, it will be saved as a backup (`app.log.1`), and logging will seamlessly continue in a fresh `app.log`.

```cpp
// Enable file logging to app.log (maximum size of 5 Megabytes)
log::add_file_sink("app.log", 5LL * 1024 * 1024 * 1024);
```

If you do not call this function, file logging will remain **disabled**. If you need multiple log files, you can call this function multiple times with different paths.

## Runtime Log Filtering (`log::set_level`)

The logger uses a minimum threshold approach for filtering messages at runtime. The log levels are ordered by severity:

1. `TRACE` (Lowest severity)
2. `DEBUG`
3. `INFO`
4. `WARN`
5. `ERROR` (Highest severity)

`log::set_level(level)` establishes the minimum severity threshold for a log message to be processed. Any log message with a severity **equal to or greater than** the configured level will be written, while everything below it will be completely ignored.

* `log::set_level(log::Level::TRACE)`: All logs from TRACE to ERROR will be recorded.
* `log::set_level(log::Level::INFO)`: Only INFO, WARN, and ERROR logs will be recorded. TRACE and DEBUG are safely ignored.
* `log::set_level(log::Level::ERROR)`: Only ERROR logs will be recorded.

This threshold can be changed dynamically **while the program is running**, without restarting. This allows you to run your application quietly at the `INFO` or `ERROR` level in production, but instantly drop the threshold to `TRACE` to gather highly detailed diagnostic data if a bug occurs.

## Quick Start (Example)

```cpp
#include <iostream>
#include <thread>

// Optional: define the compile-time active level (default is TRACE).
// Logs below this threshold will not even be compiled into your binary.
#define LOG_ACTIVE_LEVEL log::Level::TRACE
#include "logger/logger.h"

int main() {
    // Enable file logging with rotation every 1 MB
    log::add_file_sink("app.log", 1 * 1024 * 1024);

    // Set the runtime filter (cuts off TRACE and DEBUG)
    log::set_level(log::Level::INFO);

    // This log will NOT be output (level is below INFO)
    log::debug("This is a debug message");

    // This log WILL be output
    log::info("Application started successfully.");

    // Using advanced C++23 std::format syntax
    int users = 42;
    log::info("Current active users: {}", users);

    log::error("Failed to load config: {}", "config.json");

    return 0;
}
```
