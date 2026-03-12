#ifndef W3X_TOOLKIT_PARSER_SLK_SLK_PARSER_H_
#define W3X_TOOLKIT_PARSER_SLK_SLK_PARSER_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::parser::slk {

// Represents a parsed SLK spreadsheet as a 2D table of string cells.
// Row 0 typically contains column headers.
struct SlkTable {
  int32_t num_rows = 0;
  int32_t num_columns = 0;
  // Cells indexed as [row][column], both 0-based.
  std::vector<std::vector<std::string>> cells;

  // Returns the cell value at the given row and column, or empty string if
  // out of bounds.
  const std::string& GetCell(int32_t row, int32_t col) const;
};

// Parses a Warcraft III SLK (Symbolic Link / SYLK) spreadsheet file.
//
// SLK is a text-based spreadsheet format using semicolon-delimited records:
//   ID;P   - file header (must be "ID" record)
//   B;X<cols>;Y<rows> - bounds record defining table dimensions
//   C;X<col>;Y<row>;K<value> - cell data record
//   F;...  - format record (ignored)
//   E      - end record
//
// Returns a SlkTable containing all cell data on success.
core::Result<SlkTable> ParseSlk(std::string_view content);

}  // namespace w3x_toolkit::parser::slk

#endif  // W3X_TOOLKIT_PARSER_SLK_SLK_PARSER_H_
