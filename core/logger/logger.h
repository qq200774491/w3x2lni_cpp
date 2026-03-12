// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CORE_LOGGER_LOGGER_H_
#define W3X_TOOLKIT_CORE_LOGGER_LOGGER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "spdlog/spdlog.h"

namespace w3x_toolkit::core {

// Supported log levels, mapping directly to spdlog levels.
enum class LogLevel : std::uint8_t {
  kTrace = 0,
  kDebug = 1,
  kInfo = 2,
  kWarn = 3,
  kError = 4,
  kCritical = 5,
  kOff = 6,
};

// Thread-safe singleton logger wrapping spdlog.
//
// Outputs to both the console (stdout) and a rotating log file.  The default
// log file is "w3x_toolkit.log" with a 5 MB rotation limit and up to 3
// rotated files kept on disk.
//
// Usage:
//   Logger::Instance().Info("Player {} joined", player_name);
//   Logger::Instance().SetLevel(LogLevel::kDebug);
//
class Logger {
 public:
  // Returns the global logger instance.  The instance is created on first
  // call with default settings (console + file sinks, info level).
  static Logger& Instance();

  // Not copyable or movable.
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger(Logger&&) = delete;
  Logger& operator=(Logger&&) = delete;

  // ---------------------------------------------------------------------------
  // Configuration
  // ---------------------------------------------------------------------------

  // Sets the minimum log level.
  void SetLevel(LogLevel level);

  // Returns the current log level.
  LogLevel GetLevel() const;

  // Sets the spdlog pattern string applied to every sink.
  // See https://github.com/gabime/spdlog/wiki/3.-Custom-formatting for the
  // pattern syntax.
  void SetPattern(std::string_view pattern);

  // Flushes all buffered log messages immediately.
  void Flush();

  // Sets the minimum level that triggers an automatic flush.
  void SetFlushLevel(LogLevel level);

  // ---------------------------------------------------------------------------
  // Logging helpers
  // ---------------------------------------------------------------------------

  template <typename... Args>
  void Trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    logger_->trace(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    logger_->debug(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    logger_->info(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    logger_->warn(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    logger_->error(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    logger_->critical(fmt, std::forward<Args>(args)...);
  }

  // Non-templated overloads for plain string messages.
  void Trace(std::string_view msg);
  void Debug(std::string_view msg);
  void Info(std::string_view msg);
  void Warn(std::string_view msg);
  void Error(std::string_view msg);
  void Critical(std::string_view msg);

  // Provides direct access to the underlying spdlog logger for advanced use.
  std::shared_ptr<spdlog::logger> GetSpdlogLogger() const;

 private:
  // Constructs the logger with default console + rotating-file sinks.
  Logger();
  ~Logger();

  // Converts our LogLevel enum to the corresponding spdlog level.
  static spdlog::level::level_enum ToSpdlogLevel(LogLevel level);

  // Default configuration constants.
  static constexpr const char* kDefaultLoggerName = "w3x_toolkit";
  static constexpr const char* kDefaultLogFile = "w3x_toolkit.log";
  static constexpr std::size_t kMaxFileSize = 5 * 1024 * 1024;  // 5 MB
  static constexpr std::size_t kMaxFiles = 3;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace w3x_toolkit::core

#endif  // W3X_TOOLKIT_CORE_LOGGER_LOGGER_H_
