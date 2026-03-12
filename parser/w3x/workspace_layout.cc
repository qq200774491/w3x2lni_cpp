#include "parser/w3x/workspace_layout.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/filesystem/filesystem_utils.h"
#include "parser/w3x/map_archive_io.h"

namespace w3x_toolkit::parser::w3x {

namespace fs = std::filesystem;

namespace {

std::string Normalize(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.starts_with("./")) {
    path.erase(0, 2);
  }
  return path;
}

std::string Lower(std::string_view value) {
  std::string lowered;
  lowered.reserve(value.size());
  for (char ch : value) {
    lowered.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return lowered;
}

bool HasExtension(std::string_view path, std::string_view extension) {
  return Lower(fs::path(path).extension().string()) == Lower(extension);
}

bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.substr(0, prefix.size()) == prefix;
}

bool IsTablePath(std::string_view path) {
  static const std::unordered_set<std::string> kTableFiles = {
      "ability.ini",        "destructable.ini", "doodad.ini",
      "buff.ini",           "upgrade.ini",      "item.ini",
      "unit.ini",           "misc.ini",         "txt.ini",
  };
  const std::string normalized = Lower(Normalize(std::string(path)));
  if (kTableFiles.contains(normalized)) {
    return true;
  }
  return StartsWith(normalized, "units/") || StartsWith(normalized, "doodads/");
}

bool IsMapPath(std::string_view path) {
  const std::string normalized = Lower(Normalize(std::string(path)));
  return StartsWith(normalized, "war3map") ||
         StartsWith(normalized, "war3campaign") ||
         StartsWith(normalized, "scripts/");
}

bool IsResourcePath(std::string_view path) {
  return HasExtension(path, ".mdx") || HasExtension(path, ".mdl") ||
         HasExtension(path, ".blp") || HasExtension(path, ".tga") ||
         HasExtension(path, ".dds") || HasExtension(path, ".tif");
}

bool IsSoundPath(std::string_view path) {
  return HasExtension(path, ".wav") || HasExtension(path, ".mp3");
}

core::Result<fs::path> ResolveLocaleDirectory() {
#ifndef W3X_SOURCE_DIR
  return std::unexpected(core::Error::ConfigError(
      "W3X_SOURCE_DIR is not defined; cannot resolve locale files"));
#else
  const fs::path source_root(W3X_SOURCE_DIR);
  const fs::path zhcn = source_root / "external" / "script" / "locale" / "zhCN";
  if (fs::exists(zhcn / "w3i.lng") && fs::exists(zhcn / "lml.lng")) {
    return zhcn;
  }
  const fs::path enus = source_root / "external" / "script" / "locale" / "enUS";
  if (fs::exists(enus / "w3i.lng") && fs::exists(enus / "lml.lng")) {
    return enus;
  }
  return std::unexpected(core::Error::FileNotFound(
      "No locale directory with w3i.lng and lml.lng was found"));
#endif
}

core::Result<void> CopyOneFile(const fs::path& source, const fs::path& target) {
  W3X_ASSIGN_OR_RETURN(
      auto data,
      core::FilesystemUtils::ReadBinaryFile(source).transform_error(
          [&source](const std::string& error) {
            return core::Error::IOError("Failed to read '" + source.string() +
                                        "': " + error);
          }));
  return core::FilesystemUtils::WriteBinaryFile(target, data)
      .transform_error([&target](const std::string& error) {
        return core::Error::IOError("Failed to write '" + target.string() +
                                    "': " + error);
      });
}

std::vector<std::uint8_t> BuildWorkspaceMarker() {
  std::vector<std::uint8_t> bytes(12, 0);
  bytes[0] = static_cast<std::uint8_t>('H');
  bytes[1] = static_cast<std::uint8_t>('M');
  bytes[2] = static_cast<std::uint8_t>('3');
  bytes[3] = static_cast<std::uint8_t>('W');
  bytes[8] = static_cast<std::uint8_t>('W');
  bytes[9] = static_cast<std::uint8_t>('2');
  bytes[10] = static_cast<std::uint8_t>('L');
  bytes[11] = 0x01;
  return bytes;
}

core::Result<void> ConvertDirectory(const fs::path& source_dir,
                                    const fs::path& target_dir,
                                    bool to_workspace) {
  auto create_result = core::FilesystemUtils::CreateDirectories(target_dir);
  if (!create_result.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to create output directory: " + target_dir.string() + ": " +
        create_result.error()));
  }

  W3X_ASSIGN_OR_RETURN(
      auto files,
      core::FilesystemUtils::ListDirectoryRecursive(source_dir).transform_error(
          [&source_dir](const std::string& error) {
            return core::Error::IOError(
                "Failed to enumerate directory '" + source_dir.string() +
                "': " + error);
          }));

  for (const fs::path& file : files) {
    if (!core::FilesystemUtils::IsFile(file)) {
      continue;
    }
    const std::string relative =
        Normalize(fs::relative(file, source_dir).generic_string());
    const std::string mapped =
        to_workspace ? ToWorkspacePath(relative) : ToFlatPath(relative);
    if (mapped.empty()) {
      continue;
    }
    W3X_RETURN_IF_ERROR(CopyOneFile(file, target_dir / mapped));
  }
  return {};
}

}  // namespace

bool IsWorkspaceLayout(const fs::path& input_dir) {
  return core::FilesystemUtils::Exists(input_dir / "map") ||
         core::FilesystemUtils::Exists(input_dir / "table") ||
         core::FilesystemUtils::Exists(input_dir / "trigger") ||
         core::FilesystemUtils::Exists(input_dir / "w3x2lni") ||
         core::FilesystemUtils::Exists(input_dir / "resource") ||
         core::FilesystemUtils::Exists(input_dir / "sound");
}

std::string ToWorkspacePath(const std::string& flat_relative_path) {
  const std::string normalized = Normalize(flat_relative_path);
  if (normalized == std::string(kUnpackManifestFileName)) {
    return "w3x2lni/" + normalized;
  }
  if (IsResourcePath(normalized)) {
    return "resource/" + normalized;
  }
  if (IsSoundPath(normalized)) {
    return "sound/" + normalized;
  }
  if (IsMapPath(normalized)) {
    return "map/" + normalized;
  }
  if (IsTablePath(normalized)) {
    return "table/" + normalized;
  }
  return "map/" + normalized;
}

std::string ToFlatPath(const std::string& workspace_relative_path) {
  const std::string normalized = Normalize(workspace_relative_path);
  if (normalized == ".w3x") {
    return "";
  }
  auto strip_prefix = [&](std::string_view prefix) -> std::string {
    return normalized.substr(prefix.size());
  };
  if (StartsWith(normalized, "table/")) {
    return strip_prefix("table/");
  }
  if (StartsWith(normalized, "map/")) {
    return strip_prefix("map/");
  }
  if (StartsWith(normalized, "trigger/")) {
    return strip_prefix("trigger/");
  }
  if (StartsWith(normalized, "resource/")) {
    return strip_prefix("resource/");
  }
  if (StartsWith(normalized, "sound/")) {
    return strip_prefix("sound/");
  }
  if (StartsWith(normalized, "w3x2lni/")) {
    const std::string rest = strip_prefix("w3x2lni/");
    if (rest == std::string(kUnpackManifestFileName)) {
      return rest;
    }
    return "";
  }
  return normalized;
}

core::Result<void> ConvertFlatDirectoryToWorkspace(
    const fs::path& flat_dir, const fs::path& workspace_dir) {
  W3X_RETURN_IF_ERROR(ConvertDirectory(flat_dir, workspace_dir, true));
  W3X_RETURN_IF_ERROR(
      core::FilesystemUtils::WriteBinaryFile(workspace_dir / ".w3x",
                                             BuildWorkspaceMarker())
          .transform_error([&workspace_dir](const std::string& error) {
            return core::Error::IOError(
                "Failed to write workspace marker in '" +
                workspace_dir.string() + "': " + error);
          }));

  W3X_ASSIGN_OR_RETURN(auto locale_dir, ResolveLocaleDirectory());
  W3X_RETURN_IF_ERROR(CopyOneFile(locale_dir / "w3i.lng",
                                  workspace_dir / "w3x2lni" / "locale" / "w3i.lng"));
  W3X_RETURN_IF_ERROR(CopyOneFile(locale_dir / "lml.lng",
                                  workspace_dir / "w3x2lni" / "locale" / "lml.lng"));
  return {};
}

core::Result<void> ConvertWorkspaceToFlatDirectory(
    const fs::path& workspace_dir, const fs::path& flat_dir) {
  return ConvertDirectory(workspace_dir, flat_dir, false);
}

}  // namespace w3x_toolkit::parser::w3x
