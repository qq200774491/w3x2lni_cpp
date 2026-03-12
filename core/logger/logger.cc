// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "core/logger/logger.h"

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

namespace w3x_toolkit::core {

// -----------------------------------------------------------------------------
// Public static
// -----------------------------------------------------------------------------

Logger& Logger::Instance() {
  static Logger instance;
  return instance;
}

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

void Logger::SetLevel(LogLevel level) {
  logger_->set_level(ToSpdlogLevel(level));
}

LogLevel Logger::GetLevel() const {
  auto spdlog_level = logger_->level();
  return static_cast<LogLevel>(static_cast<std::uint8_t>(spdlog_level));
}

void Logger::SetPattern(std::string_view pattern) {
  logger_->set_pattern(std::string(pattern));
}

void Logger::Flush() { logger_->flush(); }

void Logger::SetFlushLevel(LogLevel level) {
  logger_->flush_on(ToSpdlogLevel(level));
}

// -----------------------------------------------------------------------------
// Plain-string logging overloads
// -----------------------------------------------------------------------------

void Logger::Trace(std::string_view msg) { logger_->trace("{}", msg); }

void Logger::Debug(std::string_view msg) { logger_->debug("{}", msg); }

void Logger::Info(std::string_view msg) { logger_->info("{}", msg); }

void Logger::Warn(std::string_view msg) { logger_->warn("{}", msg); }

void Logger::Error(std::string_view msg) { logger_->error("{}", msg); }

void Logger::Critical(std::string_view msg) { logger_->critical("{}", msg); }

// -----------------------------------------------------------------------------
// Accessor
// -----------------------------------------------------------------------------

std::shared_ptr<spdlog::logger> Logger::GetSpdlogLogger() const {
  return logger_;
}

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

Logger::Logger() {
  // Build the list of sinks: colored console + rotating file.
  std::vector<spdlog::sink_ptr> sinks;

  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::trace);
  sinks.push_back(console_sink);

  auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      kDefaultLogFile, kMaxFileSize, kMaxFiles);
  file_sink->set_level(spdlog::level::trace);
  sinks.push_back(file_sink);

  // Create the multi-sink logger.
  logger_ = std::make_shared<spdlog::logger>(
      kDefaultLoggerName, sinks.begin(), sinks.end());

  // Default level: info.
  logger_->set_level(spdlog::level::info);

  // Flush automatically on warning or above.
  logger_->flush_on(spdlog::level::warn);

  // Default pattern: [timestamp] [level] [logger] message
  logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

  // Register so spdlog::get() can also find it.
  spdlog::register_logger(logger_);
}

Logger::~Logger() {
  logger_->flush();
  spdlog::drop(kDefaultLoggerName);
}

spdlog::level::level_enum Logger::ToSpdlogLevel(LogLevel level) {
  switch (level) {
    case LogLevel::kTrace:
      return spdlog::level::trace;
    case LogLevel::kDebug:
      return spdlog::level::debug;
    case LogLevel::kInfo:
      return spdlog::level::info;
    case LogLevel::kWarn:
      return spdlog::level::warn;
    case LogLevel::kError:
      return spdlog::level::err;
    case LogLevel::kCritical:
      return spdlog::level::critical;
    case LogLevel::kOff:
      return spdlog::level::off;
  }
  // Unreachable, but satisfy compilers that warn about missing return.
  return spdlog::level::info;
}

}  // namespace w3x_toolkit::core
