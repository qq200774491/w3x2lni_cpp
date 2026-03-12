#include "parser/slk/slk_parser.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>

namespace w3x_toolkit::parser::slk {

namespace {

// Trims leading and trailing whitespace from a string_view.
std::string_view Trim(std::string_view sv) {
  while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t')) {
    sv.remove_prefix(1);
  }
  while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' ||
                          sv.back() == '\r')) {
    sv.remove_suffix(1);
  }
  return sv;
}

// Splits a line by semicolons, returning field pairs like "X5", "Y3", "Kvalue".
std::vector<std::string_view> SplitFields(std::string_view line) {
  std::vector<std::string_view> fields;
  size_t start = 0;
  while (start < line.size()) {
    size_t pos = line.find(';', start);
    if (pos == std::string_view::npos) {
      fields.push_back(line.substr(start));
      break;
    }
    fields.push_back(line.substr(start, pos - start));
    start = pos + 1;
  }
  return fields;
}

// Splits content into lines, handling \r\n, \r, or \n.
std::vector<std::string_view> SplitLines(std::string_view content) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\r') {
      lines.push_back(content.substr(start, i - start));
      if (i + 1 < content.size() && content[i + 1] == '\n') {
        ++i;
      }
      start = i + 1;
    } else if (content[i] == '\n') {
      lines.push_back(content.substr(start, i - start));
      start = i + 1;
    }
  }
  if (start < content.size()) {
    lines.push_back(content.substr(start));
  }
  return lines;
}

// Parses an integer from a string_view. Returns 0 on failure.
int32_t ParseInt(std::string_view sv) {
  int32_t result = 0;
  std::from_chars(sv.data(), sv.data() + sv.size(), result);
  return result;
}

// Extracts the value from a K field. Handles quoted strings.
std::string ExtractKValue(std::string_view field) {
  // field starts after 'K'
  if (field.empty()) {
    return "";
  }
  if (field.front() == '"') {
    // Quoted string - find closing quote.
    // SLK uses "" for escaped quotes within strings.
    std::string result;
    size_t i = 1;
    while (i < field.size()) {
      if (field[i] == '"') {
        if (i + 1 < field.size() && field[i + 1] == '"') {
          result += '"';
          i += 2;
        } else {
          break;
        }
      } else {
        result += field[i];
        ++i;
      }
    }
    return result;
  }
  // Unquoted value - return as-is.
  return std::string(field);
}

}  // namespace

static const std::string kEmptyString;

const std::string& SlkTable::GetCell(int32_t row, int32_t col) const {
  if (row < 0 || row >= static_cast<int32_t>(cells.size())) {
    return kEmptyString;
  }
  if (col < 0 || col >= static_cast<int32_t>(cells[row].size())) {
    return kEmptyString;
  }
  return cells[row][col];
}

core::Result<SlkTable> ParseSlk(std::string_view content) {
  auto lines = SplitLines(content);
  if (lines.empty()) {
    return std::unexpected(core::Error::ParseError("Empty SLK content"));
  }

  // First line must be an ID record.
  auto first_line = Trim(lines[0]);
  if (first_line.substr(0, 2) != "ID") {
    return std::unexpected(
        core::Error::ParseError("SLK file must start with 'ID' record"));
  }

  SlkTable table;
  int32_t current_row = 0;
  int32_t current_col = 0;

  for (size_t line_idx = 1; line_idx < lines.size(); ++line_idx) {
    auto line = Trim(lines[line_idx]);
    if (line.empty()) {
      continue;
    }

    char record_type = line[0];

    if (record_type == 'E') {
      // End record.
      break;
    }

    auto fields = SplitFields(line);
    if (fields.empty()) {
      continue;
    }

    if (record_type == 'B') {
      // Bounds record: B;X<cols>;Y<rows>[;D...]
      for (size_t i = 1; i < fields.size(); ++i) {
        auto field = Trim(fields[i]);
        if (field.empty()) continue;
        if (field[0] == 'X') {
          table.num_columns = ParseInt(field.substr(1));
        } else if (field[0] == 'Y') {
          table.num_rows = ParseInt(field.substr(1));
        }
      }
      // Allocate cells.
      if (table.num_rows > 0 && table.num_columns > 0) {
        table.cells.resize(static_cast<size_t>(table.num_rows));
        for (auto& row : table.cells) {
          row.resize(static_cast<size_t>(table.num_columns));
        }
      }
    } else if (record_type == 'C') {
      // Cell record: C;X<col>;Y<row>;K<value>
      std::optional<std::string> value;
      for (size_t i = 1; i < fields.size(); ++i) {
        auto field = Trim(fields[i]);
        if (field.empty()) continue;
        if (field[0] == 'X') {
          current_col = ParseInt(field.substr(1)) - 1;  // 1-based -> 0-based
        } else if (field[0] == 'Y') {
          current_row = ParseInt(field.substr(1)) - 1;  // 1-based -> 0-based
        } else if (field[0] == 'K') {
          value = ExtractKValue(field.substr(1));
        }
      }
      // Store the value.
      if (value.has_value() && current_row >= 0 && current_col >= 0) {
        // Grow table if needed (some SLK files lack B records).
        if (current_row >= static_cast<int32_t>(table.cells.size())) {
          table.cells.resize(static_cast<size_t>(current_row + 1));
          table.num_rows = current_row + 1;
        }
        auto& row = table.cells[static_cast<size_t>(current_row)];
        if (current_col >= static_cast<int32_t>(row.size())) {
          row.resize(static_cast<size_t>(current_col + 1));
          if (current_col + 1 > table.num_columns) {
            table.num_columns = current_col + 1;
          }
        }
        row[static_cast<size_t>(current_col)] = std::move(*value);
      }
    }
    // F (format) and other records are ignored.
  }

  return table;
}

}  // namespace w3x_toolkit::parser::slk
