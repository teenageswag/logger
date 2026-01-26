# Simple Logger

A lightweight, modern, and easy-to-use C++23 logging library for Windows. Designed to be simple yet powerful, providing beautiful console output and reliable file logging with zero external dependencies.

## Features

* **⚡ Lightweight & Header-only**: Easy to integrate, just drop `logger.h` into your project.
* **🎨 Rich Console Colors**: Visual distinction for Info, Warning, Error, and Success logs.
* **📁 File Logging**: Automatically logs to `log.txt` in the executable's directory.
* **🛠️ Modern C++**: Built using C++20/23 features like `std::format` and `std::print`.
* **🔒 Thread Safe**: File operations are protected with mutexes.

## Requirements

* A C++23 compatible compiler (e.g., MSVC).
* Windows OS.

## Installation

1. Copy the `logger` folder (containing `logger.h`) to your project source.
2. Include the header in your code.

## Usage

Simply define the logger modes you need **before** including the header, then use the global `g_ConsoleLogger` or `g_FileLogger` instances.

### Quick Start

```cpp
#include <iostream>

// 1. Enable the loggers you want
#define CONSOLE_LOGGER_ENABLE // If you want to use console logger
#define FILE_LOGGER_ENABLE    // If you want to use file logger

// 2. Include the library
#include "logger/logger.h"

int main() {
  // Simple string logging
  g_ConsoleLogger.Info("Application initialized");

  // Formatted logging (like std::format)
  int userId = 42;
  g_ConsoleLogger.Success("User {} connected successfully", userId);

  g_ConsoleLogger.Warn("Resource usage is high: {}%", 85);

  g_ConsoleLogger.Error("Failed to load configuration");

  // Visual separator
  g_ConsoleLogger.Separator();

  // File logging works the same way
  g_FileLogger.Info("This goes to log.txt");

  std::cin.get();
  return 0;
}
```

## API Reference

Both `ConsoleLogger` and `FileLogger` support the following methods:

* **`Info(msg, args...)`**: Log standard information (Blue).
* **`Warn(msg, args...)`**: Log warnings (Orange).
* **`Error(msg, args...)`**: Log errors (Red).
* **`Success(msg, args...)`**: Log success messages (Green).
* **`Separator()`**: Prints a horizontal divider line.
* **`NewLine()`**: Prints an empty line.

**Console Only:**

* **`Clear()`**: Clears the console screen.

## License

This project is open source. Feel free to use and modify it in your projects.
