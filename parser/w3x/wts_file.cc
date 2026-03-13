#include "parser/w3x/wts_file.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace w3x_toolkit::parser::w3x {

namespace {

std::string_view Trim(std::string_view value) {
  while (!value.empty() &&
         (value.front() == ' ' || value.front() == '\t' ||
          value.front() == '\r' || value.front() == '\n')) {
    value.remove_prefix(1);
  }
  while (!value.empty() &&
         (value.back() == ' ' || value.back() == '\t' ||
          value.back() == '\r' || value.back() == '\n')) {
    value.remove_suffix(1);
  }
  return value;
}

std::vector<std::string_view> SplitLines(std::string_view content) {
  std::vector<std::string_view> lines;
  std::size_t line_start = 0;
  for (std::size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n' || content[i] == '\r') {
      lines.push_back(content.substr(line_start, i - line_start));
      if (content[i] == '\r' && i + 1 < content.size() &&
          content[i + 1] == '\n') {
        ++i;
      }
      line_start = i + 1;
    }
  }
  if (line_start < content.size()) {
    lines.push_back(content.substr(line_start));
  }
  return lines;
}

std::string FormatTrigStr(int index) {
  std::ostringstream stream;
  stream << "TRIGSTR_" << std::setfill('0') << std::setw(3) << index;
  return stream.str();
}

}  // namespace

std::string WtsFile::AddString(std::string text) {
  for (char& ch : text) {
    if (ch == '}') {
      ch = '|';
    }
  }

  const int next_index = entries.empty() ? 0 : (entries.rbegin()->first + 1);
  entries.emplace(next_index, std::move(text));
  return FormatTrigStr(next_index);
}

std::string WtsFile::Render() const {
  std::string output;
  bool first = true;
  for (const auto& [index, text] : entries) {
    if (!first) {
      output += "\r\n\r\n";
    }
    first = false;
    output += "STRING " + std::to_string(index) + "\r\n{\r\n";
    output += text;
    output += "\r\n}";
  }
  return output;
}

core::Result<WtsFile> ParseWtsFile(std::string_view content) {
  WtsFile file;
  const std::vector<std::string_view> lines = SplitLines(content);
  std::size_t index = 0;
  while (index < lines.size()) {
    const std::string_view line = Trim(lines[index]);
    if (line.empty()) {
      ++index;
      continue;
    }
    if (!line.starts_with("STRING ")) {
      return std::unexpected(core::Error::InvalidFormat(
          "Invalid WTS record header: " + std::string(line)));
    }

    int string_index = 0;
    std::istringstream number_stream(std::string(line.substr(7)));
    number_stream >> string_index;
    if (number_stream.fail()) {
      return std::unexpected(core::Error::InvalidFormat(
          "Invalid WTS string index: " + std::string(line)));
    }

    ++index;
    while (index < lines.size() && Trim(lines[index]).empty()) {
      ++index;
    }
    if (index >= lines.size() || Trim(lines[index]) != "{") {
      return std::unexpected(core::Error::InvalidFormat(
          "Missing WTS opening brace after STRING " +
          std::to_string(string_index)));
    }

    ++index;
    std::string text;
    bool found_closing_brace = false;
    while (index < lines.size()) {
      const std::string_view raw_line = lines[index];
      if (Trim(raw_line) == "}") {
        found_closing_brace = true;
        ++index;
        break;
      }
      if (!text.empty()) {
        text += "\n";
      }
      text += std::string(raw_line);
      ++index;
    }
    if (!found_closing_brace) {
      return std::unexpected(core::Error::InvalidFormat(
          "Missing WTS closing brace for STRING " +
          std::to_string(string_index)));
    }

    file.entries[string_index] = std::move(text);
  }
  return file;
}

}  // namespace w3x_toolkit::parser::w3x
