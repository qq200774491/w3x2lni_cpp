#ifndef W3X_TOOLKIT_CORE_ERROR_ERROR_H_
#define W3X_TOOLKIT_CORE_ERROR_ERROR_H_

#include <cstdint>
#include <expected>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace w3x_toolkit::core {

// Enumeration of all error categories used throughout the toolkit.
enum class ErrorCode : uint8_t {
  kOk = 0,
  kFileNotFound,
  kParseError,
  kInvalidFormat,
  kIOError,
  kConfigError,
  kConvertError,
  kUnknown,
};

// Returns a human-readable name for the given error code.
std::string_view ErrorCodeToString(ErrorCode code);

// Lightweight, copyable error type that carries an error code, a descriptive
// message, and the source location where the error originated.
class Error {
 public:
  // Constructs an error with the given code, message, and automatic source
  // location capture.
  explicit Error(
      ErrorCode code, std::string message,
      std::source_location location = std::source_location::current())
      : code_(code),
        message_(std::move(message)),
        location_(location) {}

  // Constructs an error with only a code (empty message).
  explicit Error(
      ErrorCode code,
      std::source_location location = std::source_location::current())
      : code_(code), location_(location) {}

  // Returns the error code.
  ErrorCode code() const { return code_; }

  // Returns the error message.
  const std::string& message() const { return message_; }

  // Returns the source location where the error was created.
  const std::source_location& location() const { return location_; }

  // Returns true if this represents an actual error (code != kOk).
  bool IsError() const { return code_ != ErrorCode::kOk; }

  // Returns true if this represents success (code == kOk).
  bool IsOk() const { return code_ == ErrorCode::kOk; }

  // Returns a formatted string: "ErrorCode: message (file:line)".
  std::string ToString() const;

  // Returns a formatted string with full details including function name.
  std::string ToDetailedString() const;

  // Convenience factory for common error types.
  static Error FileNotFound(
      std::string message,
      std::source_location location = std::source_location::current()) {
    return Error(ErrorCode::kFileNotFound, std::move(message), location);
  }

  static Error ParseError(
      std::string message,
      std::source_location location = std::source_location::current()) {
    return Error(ErrorCode::kParseError, std::move(message), location);
  }

  static Error InvalidFormat(
      std::string message,
      std::source_location location = std::source_location::current()) {
    return Error(ErrorCode::kInvalidFormat, std::move(message), location);
  }

  static Error IOError(
      std::string message,
      std::source_location location = std::source_location::current()) {
    return Error(ErrorCode::kIOError, std::move(message), location);
  }

  static Error ConfigError(
      std::string message,
      std::source_location location = std::source_location::current()) {
    return Error(ErrorCode::kConfigError, std::move(message), location);
  }

  static Error ConvertError(
      std::string message,
      std::source_location location = std::source_location::current()) {
    return Error(ErrorCode::kConvertError, std::move(message), location);
  }

  static Error Unknown(
      std::string message,
      std::source_location location = std::source_location::current()) {
    return Error(ErrorCode::kUnknown, std::move(message), location);
  }

 private:
  ErrorCode code_;
  std::string message_;
  std::source_location location_;
};

// Result<T> is the primary return type for fallible operations.
// On success it holds a value of type T; on failure it holds an Error.
template <typename T>
using Result = std::expected<T, Error>;

// Macro: if the expression returns an unexpected Result, propagate it.
// Usage:
//   W3X_RETURN_IF_ERROR(SomeCall());
//
// The expression must yield a Result<T> (or any std::expected with Error).
// If the result is an error, the macro returns the unexpected value from the
// enclosing function.
#define W3X_RETURN_IF_ERROR(expr)                              \
  do {                                                         \
    auto w3x_status_or_ = (expr);                              \
    if (!w3x_status_or_.has_value()) {                         \
      return std::unexpected(std::move(w3x_status_or_.error())); \
    }                                                          \
  } while (false)

// Macro: evaluate a Result-returning expression and assign the value on
// success, or return the error on failure.
// Usage:
//   W3X_ASSIGN_OR_RETURN(auto data, ReadFile(path));
//   W3X_ASSIGN_OR_RETURN(std::string data, ReadFile(path));
//
// Implementation detail: uses line-number pasting to create unique temporaries.
#define W3X_ASSIGN_OR_RETURN(lhs, expr) \
  W3X_ASSIGN_OR_RETURN_IMPL_(lhs, expr, W3X_CONCAT_(w3x_aor_, __LINE__))

#define W3X_ASSIGN_OR_RETURN_IMPL_(lhs, expr, temp) \
  auto temp = (expr);                                \
  if (!temp.has_value()) {                           \
    return std::unexpected(std::move(temp.error()));  \
  }                                                  \
  lhs = std::move(temp.value())

#define W3X_CONCAT_(a, b) W3X_CONCAT_INNER_(a, b)
#define W3X_CONCAT_INNER_(a, b) a##b

}  // namespace w3x_toolkit::core

#endif  // W3X_TOOLKIT_CORE_ERROR_ERROR_H_
