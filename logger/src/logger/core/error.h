#pragma once

#include <expected>
#include <system_error>

namespace log_core {

enum class ErrorCode {
  None, InvalidConfig, Stopped, ReentrantCall, ModeAlreadyStarted,
  AllocationFailed, FormattingFailed, SinkFailure, WorkerStartFailed,
  RecordTooLarge, TooManyFields,
};
struct Error {
  ErrorCode code = ErrorCode::None;
  std::error_code system;
};
using Result = std::expected<void, Error>;

} // namespace log_core
