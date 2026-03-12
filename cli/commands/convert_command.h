// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_COMMANDS_CONVERT_COMMAND_H_
#define W3X_TOOLKIT_CLI_COMMANDS_CONVERT_COMMAND_H_

#include <string>
#include <vector>

#include "cli/commands/command.h"
#include "core/error/error.h"

namespace w3x_toolkit::cli {

// Supported output formats for the convert command.
enum class ConvertFormat {
  kLni,  // LNI (Lua Native Interface) format
  kSlk,  // SLK (Spreadsheet) format
  kObj,  // OBJ (Object data) format
};

// Converts a Warcraft III .w3x map file to LNI, SLK, or OBJ format.
//
// Usage:
//   w3x_toolkit convert <input.w3x> <output_dir> [--format=lni|slk|obj]
//
// If --format is omitted the default is LNI.
class ConvertCommand final : public Command {
 public:
  ConvertCommand() = default;
  ~ConvertCommand() override = default;

  std::string Name() const override;
  std::string Description() const override;
  std::string Usage() const override;
  core::Result<void> Execute(const std::vector<std::string>& args) override;

 private:
  // Parses a format string ("lni", "slk", "obj") into a ConvertFormat value.
  static core::Result<ConvertFormat> ParseFormat(const std::string& value);

  // Runs the conversion pipeline.
  core::Result<void> RunConversion(const std::string& input_path,
                                   const std::string& output_path,
                                   ConvertFormat format);
};

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_COMMANDS_CONVERT_COMMAND_H_
