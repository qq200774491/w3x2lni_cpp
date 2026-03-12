// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_COMMANDS_EXTRACT_COMMAND_H_
#define W3X_TOOLKIT_CLI_COMMANDS_EXTRACT_COMMAND_H_

#include <string>
#include <vector>

#include "cli/commands/command.h"
#include "core/error/error.h"

namespace w3x_toolkit::cli {

// Supported resource type filters for the extract command.
enum class ResourceType {
  kAll,      // Extract everything
  kModels,   // .mdx / .mdl model files
  kTextures, // .blp / .tga texture files
  kSounds,   // .mp3 / .wav / .flac audio files
  kScripts,  // .j / .lua script files
};

// Extracts resources from a Warcraft III .w3x map archive.
//
// Usage:
//   w3x_toolkit extract <input.w3x> <output_dir> [--type=all|models|textures|sounds|scripts]
//
// If --type is omitted the default is "all".
class ExtractCommand final : public Command {
 public:
  ExtractCommand() = default;
  ~ExtractCommand() override = default;

  std::string Name() const override;
  std::string Description() const override;
  std::string Usage() const override;
  core::Result<void> Execute(const std::vector<std::string>& args) override;

 private:
  // Parses a resource type string into a ResourceType enum.
  static core::Result<ResourceType> ParseResourceType(
      const std::string& value);

  // Runs the extraction pipeline.
  core::Result<void> RunExtraction(const std::string& input_path,
                                   const std::string& output_path,
                                   ResourceType type);
};

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_COMMANDS_EXTRACT_COMMAND_H_
