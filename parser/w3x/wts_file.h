#ifndef W3X_TOOLKIT_PARSER_W3X_WTS_FILE_H_
#define W3X_TOOLKIT_PARSER_W3X_WTS_FILE_H_

#include <map>
#include <string>
#include <string_view>

#include "core/error/error.h"

namespace w3x_toolkit::parser::w3x {

struct WtsFile {
  std::map<int, std::string, std::less<>> entries;

  std::string AddString(std::string text);
  std::string Render() const;
};

core::Result<WtsFile> ParseWtsFile(std::string_view content);

}  // namespace w3x_toolkit::parser::w3x

#endif  // W3X_TOOLKIT_PARSER_W3X_WTS_FILE_H_
