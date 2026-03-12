#include "core/error/error.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace w3x_toolkit::core {

std::string_view ErrorCodeToString(ErrorCode code) {
  switch (code) {
    case ErrorCode::kOk:
      return "Ok";
    case ErrorCode::kFileNotFound:
      return "FileNotFound";
    case ErrorCode::kParseError:
      return "ParseError";
    case ErrorCode::kInvalidFormat:
      return "InvalidFormat";
    case ErrorCode::kIOError:
      return "IOError";
    case ErrorCode::kConfigError:
      return "ConfigError";
    case ErrorCode::kConvertError:
      return "ConvertError";
    case ErrorCode::kUnknown:
      return "Unknown";
  }
  return "Unknown";
}

std::string Error::ToString() const {
  // Extract just the filename from the full path for concise output.
  std::filesystem::path file_path(location_.file_name());
  std::string filename = file_path.filename().string();

  std::string result;
  result.reserve(64);
  result += ErrorCodeToString(code_);
  if (!message_.empty()) {
    result += ": ";
    result += message_;
  }
  result += " (";
  result += filename;
  result += ':';
  result += std::to_string(location_.line());
  result += ')';
  return result;
}

std::string Error::ToDetailedString() const {
  std::string result;
  result.reserve(128);
  result += "[";
  result += ErrorCodeToString(code_);
  result += "]";
  if (!message_.empty()) {
    result += " ";
    result += message_;
  }
  result += "\n  at ";
  result += location_.function_name();
  result += "\n  in ";
  result += location_.file_name();
  result += ':';
  result += std::to_string(location_.line());
  result += ':';
  result += std::to_string(location_.column());
  return result;
}

}  // namespace w3x_toolkit::core
