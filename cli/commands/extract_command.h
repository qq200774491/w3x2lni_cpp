// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_COMMANDS_EXTRACT_COMMAND_H_
#define W3X_TOOLKIT_CLI_COMMANDS_EXTRACT_COMMAND_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "cli/commands/command.h"
#include "converter/resource_extract/resource_extractor.h"
#include "core/error/error.h"

namespace w3x_toolkit::cli {

// Extracts resources from an unpacked Warcraft III map directory.
//
// Usage:
//   w3x_toolkit extract <input_map_dir> <output_dir>
//       [--type=all|models|textures|sounds|scripts|ui|data|map]
class ExtractCommand final : public Command {
 public:
  ExtractCommand() = default;
  ~ExtractCommand() override = default;

  std::string Name() const override;
  std::string Description() const override;
  std::string Usage() const override;
  core::Result<void> Execute(const std::vector<std::string>& args) override;

 private:
  static core::Result<std::optional<converter::ResourceType>> ParseResourceType(
      const std::string& value);

  core::Result<void> RunExtraction(
      const std::filesystem::path& input_path,
      const std::filesystem::path& output_path,
      const std::optional<converter::ResourceType>& type);
};

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_COMMANDS_EXTRACT_COMMAND_H_
