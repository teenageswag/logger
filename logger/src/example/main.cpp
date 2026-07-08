#include <iostream>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#define LOG_ACTIVE_LEVEL log::Level::TRACE
#include "logger/logger.h"

int main() {
  // --- Console setup ---
  log::ConsoleConfig cfg;
  cfg.title = "Logger Demo";
  cfg.buffer_width = 150;
  cfg.buffer_height = 40;
  log::init_console(cfg);

  // --- File sink with 3 rotation backups ---
  log::add_file_sink("app.log", 1 * 1024 * 1024, 3);

  // --- JSON sink for structured log aggregation ---
  log::add_json_sink("logs.json", 5 * 1024 * 1024, 3);

  // --- Runtime level (TRACE = log everything) ---
  log::set_level(log::Level::TRACE);

  // --- Basic logging ---
  log::trace("This is a trace message");
  log::debug("This is a debug message");
  log::info("Application started successfully");
  log::success("Database migration completed: {} tables", 12);
  log::warn("Disk usage is at 87%");
  log::error("Failed to connect to database: {}", "localhost:5432");

  // --- Formatted logging ---
  int user_id = 42;
  std::string user_name = "Artem";
  log::info("User {} (ID: {}) logged in successfully", user_name, user_id);
  log::success("User {} authenticated via OAuth2", user_name);

  // --- Structured logging with kv pairs ---
  log::info_ctx(
      std::format("User {} authenticated", user_name),
      {{"user_id", user_id}, {"ip", "192.168.1.100"}}
  );
  log::success_ctx(
      std::format("Backup completed for {}", user_name),
      {{"size_mb", 1024}, {"duration_sec", 45}}
  );
  log::error_ctx(
      "Database query failed",
      {{"host", "db-primary"}, {"port", 5432},
      {"query", "SELECT * FROM users"}}
  );
  log::warn_ctx(
      std::format("High memory usage"),
      {{"rss_mb", 2048}, {"threshold_mb", 4096}}
  );

  // --- Thread names ---
  log::set_thread_name("Main");

  // --- Multithreading demo ---
  auto worker = [](int id) {
    log::set_thread_name("Worker-" + std::to_string(id));
    for (int i = 0; i < 5; ++i) {
      log::info_ctx(
          std::format("Processing item {}", i),
          {{"worker", id}, {"item", i}}
      );
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    log::success("Worker-{} finished all tasks", id);
  };

  std::thread t1(worker, 1);
  std::thread t2(worker, 2);
  std::thread t3(worker, 3);

  t1.join();
  t2.join();
  t3.join();

  log::success("All workers finished (dropped: {} messages)", log::dropped_count());

  std::cout << "\nPress Enter to exit..." << std::endl;
  std::cin.get();
  return 0;
}
