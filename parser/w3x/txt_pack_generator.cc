#include "parser/w3x/txt_pack_generator.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

#include "core/filesystem/filesystem_utils.h"

namespace w3x_toolkit::parser::w3x {

namespace {

constexpr std::array<std::string_view, 44> kRecognizedTxtFiles = {
    "units/campaignunitstrings.txt",
    "units/humanunitstrings.txt",
    "units/neutralunitstrings.txt",
    "units/nightelfunitstrings.txt",
    "units/orcunitstrings.txt",
    "units/undeadunitstrings.txt",
    "units/campaignunitfunc.txt",
    "units/humanunitfunc.txt",
    "units/neutralunitfunc.txt",
    "units/nightelfunitfunc.txt",
    "units/orcunitfunc.txt",
    "units/undeadunitfunc.txt",
    "units/campaignabilitystrings.txt",
    "units/commonabilitystrings.txt",
    "units/humanabilitystrings.txt",
    "units/neutralabilitystrings.txt",
    "units/nightelfabilitystrings.txt",
    "units/orcabilitystrings.txt",
    "units/undeadabilitystrings.txt",
    "units/itemabilitystrings.txt",
    "units/campaignabilityfunc.txt",
    "units/commonabilityfunc.txt",
    "units/humanabilityfunc.txt",
    "units/neutralabilityfunc.txt",
    "units/nightelfabilityfunc.txt",
    "units/orcabilityfunc.txt",
    "units/undeadabilityfunc.txt",
    "units/itemabilityfunc.txt",
    "units/campaignupgradestrings.txt",
    "units/neutralupgradestrings.txt",
    "units/nightelfupgradestrings.txt",
    "units/humanupgradestrings.txt",
    "units/orcupgradestrings.txt",
    "units/undeadupgradestrings.txt",
    "units/campaignupgradefunc.txt",
    "units/humanupgradefunc.txt",
    "units/neutralupgradefunc.txt",
    "units/nightelfupgradefunc.txt",
    "units/orcupgradefunc.txt",
    "units/undeadupgradefunc.txt",
    "units/itemstrings.txt",
    "units/itemfunc.txt",
    "units/destructableskin.txt",
    "doodads/doodadskins.txt",
};

core::Result<std::optional<GeneratedMapFile>> LoadOneGeneratedFile(
    const std::filesystem::path& input_dir, std::string_view relative_path) {
  const std::filesystem::path path = input_dir / std::string(relative_path);
  if (!core::FilesystemUtils::Exists(path)) {
    return std::optional<GeneratedMapFile>();
  }
  W3X_ASSIGN_OR_RETURN(
      auto bytes,
      core::FilesystemUtils::ReadBinaryFile(path).transform_error(
          [&path](const std::string& error) {
            return core::Error::IOError("Failed to read text sidecar '" +
                                        path.string() + "': " + error);
          }));
  return std::optional<GeneratedMapFile>(
      GeneratedMapFile{std::string(relative_path), std::move(bytes)});
}

}  // namespace

core::Result<TxtPackResult> GenerateSyntheticTxtFiles(
    const std::filesystem::path& input_dir, const PackOptions& options) {
  TxtPackResult result;
  if (options.profile != PackProfile::kSlk) {
    return result;
  }

  for (std::string_view relative_path : kRecognizedTxtFiles) {
    W3X_ASSIGN_OR_RETURN(auto generated,
                         LoadOneGeneratedFile(input_dir, relative_path));
    if (generated.has_value()) {
      result.generated_files.push_back(std::move(*generated));
    }
  }

  for (std::string_view extra_path :
       {"war3mapextra.txt", "war3mapskin.txt", "war3map.wts"}) {
    W3X_ASSIGN_OR_RETURN(auto generated,
                         LoadOneGeneratedFile(input_dir, extra_path));
    if (generated.has_value()) {
      result.generated_files.push_back(std::move(*generated));
    }
  }
  return result;
}

}  // namespace w3x_toolkit::parser::w3x
