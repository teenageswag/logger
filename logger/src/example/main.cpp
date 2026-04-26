#include <iostream>
#include "logger/logger.h"

int main() {
    // Used to create a file with logs (file_name.log, size)
    log::add_file_sink("app.log", 1 * 1024 * 1024);

	// Set minimum runtime log level (by default it's TRACE, so this is optional)
    log::set_level(log::Level::TRACE);

    // Basic logging
    log::trace("This is a trace message");
    log::debug("This is a debug message");
    log::info("This is an info message");
    log::warn("This is a warning message");
    log::error("This is an error message");

    // Formatted logging (C++23 std::format)
    int user_id = 42;
    std::string user_name = "Artem";
    log::info("User {} (ID: {}) logged in successfully.", user_name, user_id);

    // Multithreading demo
    auto worker = [](int id) {
        for (int i = 0; i < 5; ++i) {
            log::info("Worker {} processing item {}", id, i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    std::thread t1(worker, 1);
    std::thread t2(worker, 2);

    t1.join();
    t2.join();

    log::info("All workers finished!");

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}
