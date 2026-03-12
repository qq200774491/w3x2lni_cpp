#include "core/config/config_manager.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

#include "core/filesystem/filesystem_utils.h"

namespace w3x_toolkit::core {

namespace {

std::string TrimCopy(std::string_view value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }
  std::size_t end = value.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return std::string(value.substr(begin, end - begin));
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

std::string StripQuotes(std::string_view value) {
  std::string trimmed = TrimCopy(value);
  if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
    return trimmed.substr(1, trimmed.size() - 2);
  }
  return trimmed;
}

}  // namespace

ConfigManager::ConfigManager()
    : definitions_({
          {"global", "lang", ConfigValueType::kString, true},
          {"global", "data", ConfigValueType::kString, true},
          {"global", "data_ui", ConfigValueType::kString, true},
          {"global", "data_meta", ConfigValueType::kString, true},
          {"global", "data_wes", ConfigValueType::kString, true},
          {"global", "data_load", ConfigValueType::kString, true},
          {"lni", "read_slk", ConfigValueType::kBoolean, true},
          {"lni", "find_id_times", ConfigValueType::kInteger, true},
          {"lni", "export_lua", ConfigValueType::kBoolean, true},
          {"lni", "extra_check", ConfigValueType::kBoolean, true},
          {"slk", "read_slk", ConfigValueType::kBoolean, true},
          {"slk", "remove_unuse_object", ConfigValueType::kBoolean, true},
          {"slk", "optimize_jass", ConfigValueType::kBoolean, true},
          {"slk", "mdx_squf", ConfigValueType::kBoolean, true},
          {"slk", "remove_we_only", ConfigValueType::kBoolean, true},
          {"slk", "slk_doodad", ConfigValueType::kBoolean, true},
          {"slk", "find_id_times", ConfigValueType::kInteger, true},
          {"slk", "remove_same", ConfigValueType::kBoolean, true},
          {"slk", "confused", ConfigValueType::kBoolean, true},
          {"slk", "confusion", ConfigValueType::kString, true},
          {"slk", "computed_text", ConfigValueType::kBoolean, true},
          {"slk", "export_lua", ConfigValueType::kBoolean, true},
          {"slk", "extra_check", ConfigValueType::kBoolean, true},
          {"obj", "read_slk", ConfigValueType::kBoolean, true},
          {"obj", "remove_unuse_object", ConfigValueType::kBoolean, true},
          {"obj", "optimize_jass", ConfigValueType::kBoolean, true},
          {"obj", "mdx_squf", ConfigValueType::kBoolean, true},
          {"obj", "remove_we_only", ConfigValueType::kBoolean, true},
          {"obj", "slk_doodad", ConfigValueType::kBoolean, true},
          {"obj", "find_id_times", ConfigValueType::kInteger, true},
          {"obj", "remove_same", ConfigValueType::kBoolean, true},
          {"obj", "confused", ConfigValueType::kBoolean, true},
          {"obj", "confusion", ConfigValueType::kString, true},
          {"obj", "computed_text", ConfigValueType::kBoolean, true},
          {"obj", "export_lua", ConfigValueType::kBoolean, true},
          {"obj", "extra_check", ConfigValueType::kBoolean, true},
      }) {
  for (const EntryDefinition& definition : definitions_) {
    if (std::ranges::find(section_order_, definition.section) ==
        section_order_.end()) {
      section_order_.push_back(definition.section);
    }
    key_order_[definition.section].push_back(definition.key);
  }
}

Result<void> ConfigManager::LoadDefaults(const std::filesystem::path& path) {
  return LoadLayer(path, defaults_, true);
}

Result<void> ConfigManager::LoadGlobal(const std::filesystem::path& path) {
  return LoadLayer(path, global_, false);
}

Result<void> ConfigManager::LoadMap(const std::filesystem::path& path) {
  return LoadLayer(path, map_, false);
}

Result<void> ConfigManager::SaveGlobal(const std::filesystem::path& path) const {
  return SaveLayer(path, global_);
}

Result<void> ConfigManager::SaveMap(const std::filesystem::path& path) const {
  return SaveLayer(path, map_);
}

Result<void> ConfigManager::SaveEffective(
    const std::filesystem::path& path) const {
  return SaveLayer(path, BuildEffectiveVisibleData());
}

void ConfigManager::ClearGlobal() { global_.clear(); }

void ConfigManager::ClearMap() { map_.clear(); }

bool ConfigManager::HasDefinition(std::string_view section,
                                  std::string_view key) const {
  return FindDefinition(section, key) != nullptr;
}

bool ConfigManager::IsVisible(std::string_view section,
                              std::string_view key) const {
  const EntryDefinition* definition = FindDefinition(section, key);
  return definition != nullptr && definition->visible;
}

std::optional<ConfigValueInfo> ConfigManager::GetRaw(
    std::string_view section, std::string_view key) const {
  const auto lookup = [&](const IniData& layer,
                          ConfigLayer source) -> std::optional<ConfigValueInfo> {
    auto section_it = layer.find(std::string(section));
    if (section_it == layer.end()) {
      return std::nullopt;
    }
    auto value_it = section_it->second.find(std::string(key));
    if (value_it == section_it->second.end()) {
      return std::nullopt;
    }
    return ConfigValueInfo{value_it->second, source};
  };

  if (auto map_value = lookup(map_, ConfigLayer::kMap)) {
    return map_value;
  }
  if (auto global_value = lookup(global_, ConfigLayer::kGlobal)) {
    return global_value;
  }
  return lookup(defaults_, ConfigLayer::kDefault);
}

std::string ConfigManager::GetString(std::string_view section,
                                     std::string_view key,
                                     std::string_view fallback) const {
  if (auto value = GetRaw(section, key)) {
    return value->value;
  }
  return std::string(fallback);
}

bool ConfigManager::GetBool(std::string_view section, std::string_view key,
                            bool fallback) const {
  if (auto value = GetRaw(section, key)) {
    return Lower(value->value) == "true";
  }
  return fallback;
}

int ConfigManager::GetInt(std::string_view section, std::string_view key,
                          int fallback) const {
  if (auto value = GetRaw(section, key)) {
    int parsed = fallback;
    const auto text = value->value;
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec == std::errc()) {
      return parsed;
    }
  }
  return fallback;
}

Result<void> ConfigManager::SetGlobal(std::string_view section,
                                      std::string_view key,
                                      std::string_view value) {
  const EntryDefinition* definition = FindDefinition(section, key);
  if (definition == nullptr) {
    return std::unexpected(Error::ConfigError(
        "Unknown config key '" + std::string(section) + "." +
        std::string(key) + "'"));
  }
  W3X_ASSIGN_OR_RETURN(
      auto normalized,
      NormalizeValue(definition, value,
                     std::string(section) + "." + std::string(key)));
  global_[std::string(section)][std::string(key)] = normalized;
  return {};
}

Result<void> ConfigManager::SetMap(std::string_view section, std::string_view key,
                                   std::string_view value) {
  const EntryDefinition* definition = FindDefinition(section, key);
  if (definition == nullptr) {
    return std::unexpected(Error::ConfigError(
        "Unknown config key '" + std::string(section) + "." +
        std::string(key) + "'"));
  }
  W3X_ASSIGN_OR_RETURN(
      auto normalized,
      NormalizeValue(definition, value,
                     std::string(section) + "." + std::string(key)));
  map_[std::string(section)][std::string(key)] = normalized;
  return {};
}

bool ConfigManager::RemoveGlobal(std::string_view section, std::string_view key) {
  auto section_it = global_.find(std::string(section));
  if (section_it == global_.end()) {
    return false;
  }
  const std::size_t removed = section_it->second.erase(std::string(key));
  if (section_it->second.empty()) {
    global_.erase(section_it);
  }
  return removed != 0;
}

bool ConfigManager::RemoveMap(std::string_view section, std::string_view key) {
  auto section_it = map_.find(std::string(section));
  if (section_it == map_.end()) {
    return false;
  }
  const std::size_t removed = section_it->second.erase(std::string(key));
  if (section_it->second.empty()) {
    map_.erase(section_it);
  }
  return removed != 0;
}

std::vector<std::string> ConfigManager::Sections() const {
  std::vector<std::string> sections = section_order_;
  const auto append_unknown_sections = [&sections](const IniData& data) {
    for (const auto& [section, _] : data) {
      if (std::ranges::find(sections, section) == sections.end()) {
        sections.push_back(section);
      }
    }
  };
  append_unknown_sections(defaults_);
  append_unknown_sections(global_);
  append_unknown_sections(map_);
  return sections;
}

std::vector<std::string> ConfigManager::Keys(std::string_view section) const {
  std::vector<std::string> keys;
  if (auto it = key_order_.find(std::string(section)); it != key_order_.end()) {
    keys = it->second;
  }

  const auto append_unknown_keys = [&](const IniData& data) {
    auto section_it = data.find(std::string(section));
    if (section_it == data.end()) {
      return;
    }
    for (const auto& [key, _] : section_it->second) {
      if (std::ranges::find(keys, key) == keys.end()) {
        keys.push_back(key);
      }
    }
  };

  append_unknown_keys(defaults_);
  append_unknown_keys(global_);
  append_unknown_keys(map_);
  return keys;
}

Result<std::pair<std::string, std::string>> ConfigManager::ParseQualifiedKey(
    std::string_view qualified_key) {
  const std::size_t dot_pos = qualified_key.find('.');
  if (dot_pos == std::string_view::npos || dot_pos == 0 ||
      dot_pos + 1 >= qualified_key.size()) {
    return std::unexpected(Error::ConfigError(
        "Config key must use <section>.<key> format"));
  }
  return std::make_pair(std::string(qualified_key.substr(0, dot_pos)),
                        std::string(qualified_key.substr(dot_pos + 1)));
}

std::string ConfigManager::LayerName(ConfigLayer layer) {
  switch (layer) {
    case ConfigLayer::kDefault:
      return "default";
    case ConfigLayer::kGlobal:
      return "global";
    case ConfigLayer::kMap:
      return "map";
  }
  return "unknown";
}

const ConfigManager::EntryDefinition* ConfigManager::FindDefinition(
    std::string_view section, std::string_view key) const {
  const auto definition_it = std::ranges::find_if(
      definitions_, [&](const EntryDefinition& definition) {
        return definition.section == section && definition.key == key;
      });
  if (definition_it == definitions_.end()) {
    return nullptr;
  }
  return &(*definition_it);
}

Result<void> ConfigManager::LoadLayer(const std::filesystem::path& path,
                                      IniData& target, bool required) {
  target.clear();
  if (!FilesystemUtils::Exists(path)) {
    if (required) {
      return std::unexpected(
          Error::FileNotFound("Config file not found: " + path.string()));
    }
    return {};
  }

  W3X_ASSIGN_OR_RETURN(auto content, FilesystemUtils::ReadTextFile(path)
                                         .transform_error([&path](const std::string& error) {
                                           return Error::IOError(
                                               "Failed to read config '" +
                                               path.string() + "': " + error);
                                         }));
  W3X_ASSIGN_OR_RETURN(target, ParseIni(content, path));
  return {};
}

Result<void> ConfigManager::SaveLayer(const std::filesystem::path& path,
                                      const IniData& data) const {
  auto create_result = FilesystemUtils::CreateDirectories(path.parent_path());
  if (!create_result.has_value()) {
    return std::unexpected(Error::IOError(
        "Failed to create config directory '" + path.parent_path().string() +
        "': " + create_result.error()));
  }

  auto write_result = FilesystemUtils::WriteTextFile(path, SerializeIni(data));
  if (!write_result.has_value()) {
    return std::unexpected(Error::IOError(
        "Failed to write config '" + path.string() + "': " +
        write_result.error()));
  }
  return {};
}

ConfigManager::IniData ConfigManager::BuildEffectiveVisibleData() const {
  IniData effective;
  for (const std::string& section : Sections()) {
    for (const std::string& key : Keys(section)) {
      if (!IsVisible(section, key)) {
        continue;
      }
      if (auto value = GetRaw(section, key)) {
        effective[section][key] = value->value;
      }
    }
  }
  return effective;
}

Result<ConfigManager::IniData> ConfigManager::ParseIni(
    std::string_view content, const std::filesystem::path& path) const {
  IniData parsed;
  std::string current_section;
  std::istringstream stream{std::string(content)};
  std::string line;
  std::size_t line_number = 0;

  while (std::getline(stream, line)) {
    ++line_number;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    const std::string trimmed = TrimCopy(line);
    if (trimmed.empty() || trimmed.starts_with(';') || trimmed.starts_with('#')) {
      continue;
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      current_section = TrimCopy(trimmed.substr(1, trimmed.size() - 2));
      if (current_section.empty()) {
        return std::unexpected(Error::ParseError(
            "Empty INI section in '" + path.string() + "' at line " +
            std::to_string(line_number)));
      }
      continue;
    }
    const std::size_t equals_pos = trimmed.find('=');
    if (equals_pos == std::string::npos || current_section.empty()) {
      return std::unexpected(Error::ParseError(
          "Invalid INI entry in '" + path.string() + "' at line " +
          std::to_string(line_number)));
    }

    const std::string key = TrimCopy(trimmed.substr(0, equals_pos));
    if (key.empty()) {
      return std::unexpected(Error::ParseError(
          "Empty INI key in '" + path.string() + "' at line " +
          std::to_string(line_number)));
    }

    const EntryDefinition* definition = FindDefinition(current_section, key);
    W3X_ASSIGN_OR_RETURN(
        auto normalized,
        NormalizeValue(definition, trimmed.substr(equals_pos + 1),
                       path.string() + ":" + std::to_string(line_number)));
    parsed[current_section][key] = normalized;
  }

  return parsed;
}

Result<std::string> ConfigManager::NormalizeValue(
    const EntryDefinition* definition, std::string_view value,
    std::string_view context) const {
  std::string cleaned = StripQuotes(value);
  if (definition == nullptr) {
    return cleaned;
  }

  switch (definition->type) {
    case ConfigValueType::kString:
      return cleaned;

    case ConfigValueType::kBoolean: {
      const std::string lowered = Lower(cleaned);
      if (lowered == "true" || lowered == "1" || lowered == "yes" ||
          lowered == "on") {
        return std::string("true");
      }
      if (lowered == "false" || lowered == "0" || lowered == "no" ||
          lowered == "off") {
        return std::string("false");
      }
      return std::unexpected(Error::ConfigError(
          "Invalid boolean value '" + cleaned + "' for " +
          std::string(context)));
    }

    case ConfigValueType::kInteger: {
      int parsed = 0;
      const auto result = std::from_chars(
          cleaned.data(), cleaned.data() + cleaned.size(), parsed);
      if (result.ec != std::errc()) {
        return std::unexpected(Error::ConfigError(
            "Invalid integer value '" + cleaned + "' for " +
            std::string(context)));
      }
      return std::to_string(parsed);
    }
  }

  return cleaned;
}

std::string ConfigManager::SerializeIni(const IniData& data) const {
  std::ostringstream out;
  std::set<std::string, std::less<>> emitted_sections;

  const auto write_section = [&](const std::string& section) {
    auto section_it = data.find(section);
    if (section_it == data.end() || section_it->second.empty()) {
      return;
    }
    out << "[" << section << "]\r\n";

    std::set<std::string, std::less<>> emitted_keys;
    if (auto order_it = key_order_.find(section); order_it != key_order_.end()) {
      for (const std::string& key : order_it->second) {
        auto value_it = section_it->second.find(key);
        if (value_it == section_it->second.end()) {
          continue;
        }
        out << key << " = " << value_it->second << "\r\n";
        emitted_keys.insert(key);
      }
    }
    for (const auto& [key, value] : section_it->second) {
      if (emitted_keys.contains(key)) {
        continue;
      }
      out << key << " = " << value << "\r\n";
    }
    out << "\r\n";
  };

  for (const std::string& section : section_order_) {
    write_section(section);
    emitted_sections.insert(section);
  }
  for (const auto& [section, _] : data) {
    if (emitted_sections.contains(section)) {
      continue;
    }
    write_section(section);
  }

  return out.str();
}

}  // namespace w3x_toolkit::core
