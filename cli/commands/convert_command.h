// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_COMMANDS_CONVERT_COMMAND_H_
#define W3X_TOOLKIT_CLI_COMMANDS_CONVERT_COMMAND_H_

#include <filesystem>
#include <string>
#include <vector>

#include "cli/commands/command.h"
#include "converter/w3x2lni/w3x_to_lni.h"
#include "core/error/error.h"

namespace w3x_toolkit::cli {

// Converts an unpacked Warcraft III map directory to the LNI workspace layout.
//
// Usage:
//   w3x_toolkit convert <input_map_dir> <output_dir>
//       [--no-map-files] [--no-table-data] [--no-triggers] [--no-config]
class ConvertCommand final : public Command {
 public:
  ConvertCommand() = default;
  ~ConvertCommand() override = default;

  std::string Name() const override;
  std::string Description() const override;
 std::string Usage() const override;
  core::Result<void> Execute(const std::vector<std::string>& args) override;

 private:
  core::Result<converter::W3xToLniOptions> ParseOptions(
      const std::vector<std::string>& args, std::size_t first_option_index);

  core::Result<void> RunConversion(
      const std::filesystem::path& input_path,
      const std::filesystem::path& output_path,
      const converter::W3xToLniOptions& options);
};

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_COMMANDS_CONVERT_COMMAND_H_
