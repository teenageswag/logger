#include "logger/logger.h"
#include <iostream>
#include <stdexcept>
void *address_a();
void *address_b();
int disabled_callsite();
int enabled_callsite();
struct Unsupported {};
static_assert(!std::constructible_from<log::Field, std::string, Unsupported>);
struct Capture final : log::Sink {
  std::vector<log_core::LogMessage> messages;
  void write(const log_core::LogMessage &message) override { messages.push_back(message); }
  void flush() override {}
};
int main() {
  if (address_a() != address_b() || disabled_callsite() != 0 || enabled_callsite() != 1) return 1;
  auto sink = std::make_unique<Capture>();
  auto *view = sink.get();
  if (!log::add_sink(std::move(sink))) return 2;
  log::set_level(log::Level::Trace);
  const auto line = __LINE__ + 1;
  log::warn({{"id", 7}}, "structured {}", 42);
  log::trace({{"id", 1}}, "trace");
  log::debug({{"id", 2}}, "debug");
  log::success({{"id", 3}}, "success");
  log::error({{"id", 4}}, "error");
  log::critical({{"id", 5}}, "critical");
  if (view->messages.size() != 6 || view->messages[0].loc.line() != line ||
      view->messages[0].text != "structured 42") return 3;
  log::set_level(log::Level::Off);
  int calls = 0;
  LOG_CRITICAL("{}", ++calls);
  log::lazy(log::Level::Info, [&] { ++calls; return "unused"; });
  if (calls) return 4;
  if (!log::shutdown() || log::info("after stop")) return 5;
  std::cout << "PASS header-only, ODR, call-site filtering and fields API\n";
}
