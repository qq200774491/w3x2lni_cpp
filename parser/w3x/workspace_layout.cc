#include "parser/w3x/workspace_layout.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/config/config_manager.h"
#include "core/config/runtime_paths.h"
#include "core/filesystem/filesystem_utils.h"
#include "parser/w3i/w3i_parser.h"
#include "parser/w3x/object_pack_generator.h"
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

std::string QuoteString(const std::string& value) {
  std::string quoted = "\"";
  for (char ch : value) {
    if (ch == '\\' || ch == '"') {
      quoted.push_back('\\');
    }
    quoted.push_back(ch);
  }
  quoted.push_back('"');
  return quoted;
}

std::string FormatFloat4(float value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(4) << value;
  return stream.str();
}

std::string FormatIntListFromMask(std::uint32_t mask) {
  std::ostringstream stream;
  stream << "{";
  bool first = true;
  for (int bit = 0; bit < 32; ++bit) {
    if (((mask >> bit) & 1u) == 0) {
      continue;
    }
    if (!first) {
      stream << ", ";
    }
    stream << (bit + 1);
    first = false;
  }
  stream << "}";
  return stream.str();
}

std::string FormatByteArray(const std::array<std::uint8_t, 4>& values) {
  std::ostringstream stream;
  stream << "{" << static_cast<int>(values[0]) << ", "
         << static_cast<int>(values[1]) << ", "
         << static_cast<int>(values[2]) << ", "
         << static_cast<int>(values[3]) << "}";
  return stream.str();
}

std::string FormatWeatherId(const std::string& weather_id) {
  if (weather_id.empty()) {
    return "\"\\0\\0\\0\\0\"";
  }
  return QuoteString(weather_id);
}

std::string FormatLightChar(char value) {
  if (value == '\0') {
    return "\"\\0\"";
  }
  return QuoteString(std::string(1, value));
}

core::Result<void> WriteW3iIni(const fs::path& flat_dir,
                               const fs::path& workspace_dir) {
  const fs::path w3i_path = flat_dir / "war3map.w3i";
  if (!core::FilesystemUtils::Exists(w3i_path)) {
    return {};
  }

  W3X_ASSIGN_OR_RETURN(
      auto bytes,
      core::FilesystemUtils::ReadBinaryFile(w3i_path).transform_error(
          [&w3i_path](const std::string& error) {
            return core::Error::IOError("Failed to read '" + w3i_path.string() +
                                        "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto w3i, parser::w3i::ParseW3i(bytes));

  std::ostringstream out;
  out << "[地图]\r\n";
  out << "文件版本 = " << w3i.file_version << "\r\n";
  out << "地图版本 = " << w3i.map_version << "\r\n";
  out << "编辑器版本 = " << w3i.editor_version << "\r\n";
  out << "地图名称 = " << QuoteString(w3i.map_name) << "\r\n";
  out << "作者名字 = " << QuoteString(w3i.author) << "\r\n";
  out << "地图描述 = " << QuoteString(w3i.description) << "\r\n";
  out << "推荐玩家 = " << QuoteString(w3i.players_recommended) << "\r\n\r\n";

  out << "[镜头]\r\n";
  out << "镜头范围 = {\r\n";
  for (int i = 0; i < 8; ++i) {
    out << FormatFloat4(w3i.camera.bounds[i]) << "\r\n";
  }
  out << "}\r\n";
  out << "镜头范围扩充 = {" << w3i.camera.complements[0] << ", "
      << w3i.camera.complements[1] << ", " << w3i.camera.complements[2]
      << ", " << w3i.camera.complements[3] << "}\r\n\r\n";

  out << "[地形]\r\n";
  out << "地图宽度 = " << w3i.map_width << "\r\n";
  out << "地图长度 = " << w3i.map_height << "\r\n";
  out << "地形类型 = " << QuoteString(std::string(1, w3i.main_ground))
      << "\r\n\r\n";

  out << "[选项]\r\n";
  out << "使用的游戏数据设置 = " << w3i.game_data_setting << "\r\n";
  out << "关闭预览图 = " << static_cast<int>(w3i.flags.disable_preview) << "\r\n";
  out << "自定义结盟优先权 = " << static_cast<int>(w3i.flags.custom_ally)
      << "\r\n";
  out << "对战地图 = " << static_cast<int>(w3i.flags.melee_map) << "\r\n";
  out << "大型地图 = " << static_cast<int>(w3i.flags.large_map) << "\r\n";
  out << "迷雾区域显示地形 = "
      << static_cast<int>(w3i.flags.masked_area_show_terrain) << "\r\n";
  out << "自定义玩家分组 = "
      << static_cast<int>(w3i.flags.fix_force_setting) << "\r\n";
  out << "自定义队伍 = " << static_cast<int>(w3i.flags.custom_force) << "\r\n";
  out << "自定义科技树 = " << static_cast<int>(w3i.flags.custom_techtree)
      << "\r\n";
  out << "自定义技能 = " << static_cast<int>(w3i.flags.custom_ability)
      << "\r\n";
  out << "自定义升级 = " << static_cast<int>(w3i.flags.custom_upgrade)
      << "\r\n";
  out << "地图菜单标记 = " << static_cast<int>(w3i.flags.map_menu_mark)
      << "\r\n";
  out << "地形悬崖显示水波 = "
      << static_cast<int>(w3i.flags.show_wave_on_cliff) << "\r\n";
  out << "地形起伏显示水波 = "
      << static_cast<int>(w3i.flags.show_wave_on_rolling) << "\r\n\r\n";

  out << "[载入图]\r\n";
  out << "序号 = " << w3i.loading_screen.id << "\r\n";
  out << "路径 = " << QuoteString(w3i.loading_screen.path) << "\r\n";
  out << "文本 = " << QuoteString(w3i.loading_screen.text) << "\r\n";
  out << "标题 = " << QuoteString(w3i.loading_screen.title) << "\r\n";
  out << "子标题 = " << QuoteString(w3i.loading_screen.subtitle) << "\r\n\r\n";

  out << "[战役]\r\n";
  out << "路径 = " << QuoteString(w3i.prologue.path) << "\r\n";
  out << "文本 = " << QuoteString(w3i.prologue.text) << "\r\n";
  out << "标题 = " << QuoteString(w3i.prologue.title) << "\r\n";
  out << "子标题 = " << QuoteString(w3i.prologue.subtitle) << "\r\n\r\n";

  out << "[迷雾]\r\n";
  out << "类型 = " << w3i.fog.type << "\r\n";
  out << "z轴起点 = " << FormatFloat4(w3i.fog.start_z) << "\r\n";
  out << "z轴终点 = " << FormatFloat4(w3i.fog.end_z) << "\r\n";
  out << "密度 = " << FormatFloat4(w3i.fog.density) << "\r\n";
  out << "颜色 = {" << static_cast<int>(w3i.fog.color[0]) << ", "
      << static_cast<int>(w3i.fog.color[1]) << ", "
      << static_cast<int>(w3i.fog.color[2]) << ", "
      << static_cast<int>(w3i.fog.color[3]) << "}\r\n\r\n";

  out << "[环境]\r\n";
  out << "天气 = " << FormatWeatherId(w3i.environment.weather_id) << "\r\n";
  out << "音效 = " << QuoteString(w3i.environment.sound) << "\r\n";
  out << "光照 = " << FormatLightChar(w3i.environment.light) << "\r\n";
  out << "水面颜色 = {" << static_cast<int>(w3i.environment.water_color[0])
      << ", " << static_cast<int>(w3i.environment.water_color[1]) << ", "
      << static_cast<int>(w3i.environment.water_color[2]) << ", "
      << static_cast<int>(w3i.environment.water_color[3]) << "}\r\n\r\n";

  out << "[玩家]\r\n";
  out << "玩家数量 = " << w3i.players.size() << "\r\n\r\n";
  for (std::size_t i = 0; i < w3i.players.size(); ++i) {
    const auto& player = w3i.players[i];
    out << "[玩家" << (i + 1) << "]\r\n";
    out << "玩家 = " << player.player_id << "\r\n";
    out << "类型 = " << player.type << "\r\n";
    out << "种族 = " << player.race << "\r\n";
    out << "修正出生点 = " << player.fix_start << "\r\n";
    out << "名字 = " << QuoteString(player.name) << "\r\n";
    out << "出生点 = {" << FormatFloat4(player.start_x) << ", "
        << FormatFloat4(player.start_y) << "}\r\n";
    out << "低结盟优先权标记 = "
        << FormatIntListFromMask(player.ally_low_flag) << "\r\n";
    out << "高结盟优先权标记 = "
        << FormatIntListFromMask(player.ally_high_flag) << "\r\n\r\n";
  }

  out << "[队伍]\r\n";
  out << "队伍数量 = " << w3i.forces.size() << "\r\n\r\n";
  for (std::size_t i = 0; i < w3i.forces.size(); ++i) {
    const auto& force = w3i.forces[i];
    out << "[队伍" << (i + 1) << "]\r\n";
    out << "结盟 = " << static_cast<int>(force.allied) << "\r\n";
    out << "结盟胜利 = " << static_cast<int>(force.allied_victory) << "\r\n";
    out << "共享视野 = " << static_cast<int>(force.share_vision) << "\r\n";
    out << "共享单位控制 = " << static_cast<int>(force.share_control) << "\r\n";
    out << "共享高级单位设置 = " << static_cast<int>(force.share_advanced)
        << "\r\n";
    out << "玩家列表 = " << FormatIntListFromMask(force.player_mask) << "\r\n";
    out << "队伍名称 = " << QuoteString(force.name) << "\r\n\r\n";
  }

  return core::FilesystemUtils::WriteTextFile(workspace_dir / "table" / "w3i.ini",
                                              out.str())
      .transform_error([&workspace_dir](const std::string& error) {
        return core::Error::IOError("Failed to write w3i.ini in '" +
                                    workspace_dir.string() + "': " + error);
      });
}

core::Result<void> WriteImportList(const fs::path& workspace_dir) {
  std::vector<std::string> imports;
  for (const std::string_view dir_name : {"resource", "sound"}) {
    const fs::path dir = workspace_dir / std::string(dir_name);
    if (!core::FilesystemUtils::Exists(dir)) {
      continue;
    }
    W3X_ASSIGN_OR_RETURN(
        auto files,
        core::FilesystemUtils::ListDirectoryRecursive(dir).transform_error(
            [&dir](const std::string& error) {
              return core::Error::IOError(
                  "Failed to enumerate import directory '" + dir.string() +
                  "': " + error);
            }));
    for (const fs::path& file : files) {
      if (!core::FilesystemUtils::IsFile(file)) {
        continue;
      }
      imports.push_back(
          fs::relative(file, dir).generic_string());
    }
  }

  std::ranges::sort(imports);
  std::string content = "import = {\r\n";
  for (const std::string& item : imports) {
    std::string windows_path = item;
    std::replace(windows_path.begin(), windows_path.end(), '/', '\\');
    content += "\"" + windows_path + "\",\r\n";
  }
  content += "}\r\n";

  return core::FilesystemUtils::WriteTextFile(workspace_dir / "table" / "imp.ini",
                                              content)
      .transform_error([&workspace_dir](const std::string& error) {
        return core::Error::IOError("Failed to write imp.ini in '" +
                                    workspace_dir.string() + "': " + error);
      });
}

core::Result<fs::path> ResolveLocaleDirectory() {
  core::ConfigManager config;
  W3X_RETURN_IF_ERROR(
      config.LoadDefaults(core::RuntimePaths::ResolveDefaultConfigPath()));
  W3X_RETURN_IF_ERROR(
      config.LoadGlobal(core::RuntimePaths::ResolveGlobalConfigPath()));

  std::vector<fs::path> candidates;
  const std::string preferred_lang = config.GetString("global", "lang", "zhCN");
  if (!preferred_lang.empty() && !preferred_lang.starts_with("${")) {
    if (auto data_root = core::RuntimePaths::ResolveDataRoot();
        data_root.has_value()) {
      candidates.push_back(data_root.value() / "locale" / preferred_lang);
    }
  }
  if (auto data_root = core::RuntimePaths::ResolveDataRoot();
      data_root.has_value()) {
    candidates.push_back(data_root.value() / "locale" / "zhCN");
    candidates.push_back(data_root.value() / "locale" / "enUS");
  }

  const fs::path source_root = core::RuntimePaths::GetSourceRoot();
  if (!source_root.empty()) {
    candidates.push_back(source_root / "external" / "script" / "locale" / "zhCN");
    candidates.push_back(source_root / "external" / "script" / "locale" / "enUS");
  }

  for (const fs::path& candidate : candidates) {
    if (fs::exists(candidate / "w3i.lng") && fs::exists(candidate / "lml.lng")) {
      return candidate;
    }
  }
  return std::unexpected(core::Error::FileNotFound(
      "No locale directory with w3i.lng and lml.lng was found"));
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

core::Result<void> WriteDerivedTableFiles(const fs::path& flat_dir,
                                          const fs::path& workspace_dir) {
  W3X_ASSIGN_OR_RETURN(auto generated_files, GenerateDerivedTableFiles(flat_dir));
  for (const GeneratedMapFile& generated : generated_files) {
    const fs::path target = workspace_dir / "table" / generated.relative_path;
    if (core::FilesystemUtils::Exists(target)) {
      continue;
    }
    W3X_RETURN_IF_ERROR(
        core::FilesystemUtils::WriteBinaryFile(target, generated.content)
            .transform_error([&target](const std::string& error) {
              return core::Error::IOError("Failed to write derived table file '" +
                                          target.string() + "': " + error);
            }));
  }
  return {};
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
  W3X_RETURN_IF_ERROR(WriteDerivedTableFiles(flat_dir, workspace_dir));
  return FinalizeWorkspaceDirectory(flat_dir, workspace_dir);
}

core::Result<void> FinalizeWorkspaceDirectory(
    const fs::path& flat_dir, const fs::path& workspace_dir) {
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
  W3X_RETURN_IF_ERROR(WriteImportList(workspace_dir));
  W3X_RETURN_IF_ERROR(WriteW3iIni(flat_dir, workspace_dir));
  return {};
}

core::Result<void> ConvertWorkspaceToFlatDirectory(
    const fs::path& workspace_dir, const fs::path& flat_dir) {
  return ConvertDirectory(workspace_dir, flat_dir, false);
}

}  // namespace w3x_toolkit::parser::w3x
