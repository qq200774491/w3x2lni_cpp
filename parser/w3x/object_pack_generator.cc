#include "parser/w3x/object_pack_generator.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/config/config_manager.h"
#include "core/config/runtime_paths.h"
#include "core/filesystem/filesystem_utils.h"
#include "parser/slk/slk_parser.h"
#include "parser/w3u/w3u_parser.h"
#include "parser/w3u/w3u_writer.h"

namespace w3x_toolkit::parser::w3x {

namespace fs = std::filesystem;

namespace {

struct MetaEntry {
  std::string field;
  std::string id;
  parser::w3u::ModificationType type = parser::w3u::ModificationType::kString;
  std::optional<int> data_pointer;
  std::optional<int> index;
  bool repeat = false;
};

using SectionEntries = std::vector<std::pair<std::string, std::string>>;
using IniSections = std::unordered_map<std::string, SectionEntries>;
using MetadataTable = std::unordered_map<std::string, MetaEntry>;
using MetadataTables = std::unordered_map<std::string, MetadataTable>;

struct ReverseMetaEntry {
  std::string key;
  MetaEntry meta;
};

struct FieldAggregation {
  std::optional<std::string> scalar_value;
  std::map<int, std::string> indexed_values;
  std::map<int, std::string> repeated_values;
};

enum class SyntheticType {
  kAbility,
  kBuff,
  kDestructable,
  kDoodad,
  kUnit,
  kItem,
  kUpgrade,
};

struct SyntheticTypeSpec {
  SyntheticType type;
  const char* logical_name;
  const char* input_ini;
  const char* output_file;
  const char* slk_code_file;
  parser::w3u::ObjectFileKind kind;
};

constexpr SyntheticTypeSpec kSyntheticSpecs[] = {
    {SyntheticType::kAbility, "ability", "ability.ini", "war3map.w3a",
     "units/abilitydata.slk", parser::w3u::ObjectFileKind::kComplex},
    {SyntheticType::kBuff, "buff", "buff.ini", "war3map.w3h",
     "units/abilitybuffdata.slk", parser::w3u::ObjectFileKind::kComplex},
    {SyntheticType::kDestructable, "destructable", "destructable.ini",
     "war3map.w3b", "units/destructabledata.slk",
     parser::w3u::ObjectFileKind::kSimple},
    {SyntheticType::kDoodad, "doodad", "doodad.ini", "war3map.w3d",
     "doodads/doodads.slk", parser::w3u::ObjectFileKind::kComplex},
    {SyntheticType::kUnit, "unit", "unit.ini", "war3map.w3u", nullptr,
     parser::w3u::ObjectFileKind::kSimple},
    {SyntheticType::kItem, "item", "item.ini", "war3map.w3t", nullptr,
     parser::w3u::ObjectFileKind::kSimple},
    {SyntheticType::kUpgrade, "upgrade", "upgrade.ini", "war3map.w3q", nullptr,
     parser::w3u::ObjectFileKind::kComplex},
};

std::string NormalizeKey(std::string_view value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (char ch : value) {
    normalized.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return normalized;
}

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

std::string StripQuotes(std::string_view value) {
  std::string trimmed = TrimCopy(value);
  if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
    std::string unquoted;
    unquoted.reserve(trimmed.size() - 2);
    for (std::size_t i = 1; i + 1 < trimmed.size(); ++i) {
      if (trimmed[i] == '"' && i + 1 < trimmed.size() - 1 &&
          trimmed[i + 1] == '"') {
        unquoted.push_back('"');
        ++i;
      } else {
        unquoted.push_back(trimmed[i]);
      }
    }
    return unquoted;
  }
  return trimmed;
}

std::vector<std::string> SplitCsvPreserveEmpty(std::string_view value) {
  std::vector<std::string> parts;
  std::string current;
  bool in_quotes = false;
  for (std::size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (ch == '"') {
      if (in_quotes && i + 1 < value.size() && value[i + 1] == '"') {
        current.push_back('"');
        ++i;
      } else {
        in_quotes = !in_quotes;
      }
      continue;
    }
    if (ch == ',' && !in_quotes) {
      parts.push_back(TrimCopy(current));
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  parts.push_back(TrimCopy(current));
  return parts;
}

std::optional<std::int32_t> ParseInt32(std::string_view value) {
  const std::string stripped = StripQuotes(value);
  if (stripped.empty()) {
    return std::int32_t{0};
  }
  std::int32_t parsed = 0;
  const auto result = std::from_chars(
      stripped.data(), stripped.data() + stripped.size(), parsed);
  if (result.ec == std::errc()) {
    return parsed;
  }
  return std::nullopt;
}

std::optional<float> ParseFloat(std::string_view value) {
  const std::string stripped = StripQuotes(value);
  if (stripped.empty()) {
    return 0.0f;
  }
  char* end = nullptr;
  const float parsed = std::strtof(stripped.c_str(), &end);
  if (end != stripped.c_str() + stripped.size()) {
    return std::nullopt;
  }
  return parsed;
}

core::Result<IniSections> ParseSectionedText(std::string_view content) {
  IniSections sections;
  std::string current_section;
  std::size_t line_start = 0;

  const auto consume_line = [&](std::string_view raw_line)
      -> core::Result<void> {
    const std::string line = TrimCopy(raw_line);
    if (line.empty() || line.starts_with(';') || line.starts_with('#') ||
        line.starts_with("//")) {
      return {};
    }

    if (line.front() == '[' && line.back() == ']') {
      current_section = StripQuotes(line.substr(1, line.size() - 2));
      sections[current_section];
      return {};
    }

    if (current_section.empty()) {
      return {};
    }

    const std::size_t equal_pos = line.find('=');
    if (equal_pos == std::string::npos) {
      return std::unexpected(core::Error::InvalidFormat(
          "Invalid ini line without '=' inside section [" + current_section +
          "]"));
    }

    std::string key = TrimCopy(line.substr(0, equal_pos));
    std::string value = TrimCopy(line.substr(equal_pos + 1));
    sections[current_section].push_back({std::move(key), std::move(value)});
    return {};
  };

  for (std::size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n' || content[i] == '\r') {
      W3X_RETURN_IF_ERROR(
          consume_line(content.substr(line_start, i - line_start)));
      if (content[i] == '\r' && i + 1 < content.size() && content[i + 1] == '\n') {
        ++i;
      }
      line_start = i + 1;
    }
  }
  if (line_start < content.size()) {
    W3X_RETURN_IF_ERROR(consume_line(content.substr(line_start)));
  }
  return sections;
}

core::Result<MetadataTables> LoadMetadataTables(const fs::path& metadata_path) {
  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(metadata_path).transform_error(
          [&metadata_path](const std::string& error) {
            return core::Error::IOError("Failed to read metadata file '" +
                                        metadata_path.string() + "': " + error);
          }));

  MetadataTables tables;
  std::string current_table;
  std::string current_entry;
  std::size_t line_start = 0;

  const auto consume_line = [&](std::string_view raw_line)
      -> core::Result<void> {
    const std::string line = TrimCopy(raw_line);
    if (line.empty() || line.starts_with(';') || line.starts_with('#') ||
        line.starts_with("//")) {
      return {};
    }

    if (line.front() == '[' && line.back() == ']') {
      const std::string name = StripQuotes(line.substr(1, line.size() - 2));
      if (!name.empty() && name.front() == '.') {
        if (current_table.empty()) {
          return std::unexpected(core::Error::InvalidFormat(
              "Metadata subsection appears before a parent table"));
        }
        current_entry = NormalizeKey(name.substr(1));
        tables[current_table][current_entry];
      } else {
        current_table = name;
        current_entry.clear();
        tables[current_table];
      }
      return {};
    }

    if (current_table.empty() || current_entry.empty()) {
      return {};
    }

    const std::size_t equal_pos = line.find('=');
    if (equal_pos == std::string::npos) {
      return {};
    }
    const std::string key = NormalizeKey(TrimCopy(line.substr(0, equal_pos)));
    const std::string value = StripQuotes(line.substr(equal_pos + 1));
    MetaEntry& entry = tables[current_table][current_entry];
    if (key == "field") {
      entry.field = value;
    } else if (key == "id") {
      entry.id = value;
    } else if (key == "type") {
      const auto parsed = ParseInt32(value);
      if (!parsed.has_value() || *parsed < 0 || *parsed > 3) {
        return std::unexpected(core::Error::InvalidFormat(
            "Invalid metadata type in '" + metadata_path.string() + "'"));
      }
      entry.type =
          static_cast<parser::w3u::ModificationType>(parsed.value());
    } else if (key == "data") {
      entry.data_pointer = ParseInt32(value);
    } else if (key == "index") {
      entry.index = ParseInt32(value);
    } else if (key == "repeat") {
      const auto parsed = ParseInt32(value);
      entry.repeat = parsed.has_value() && parsed.value() > 0;
    }
    return {};
  };

  for (std::size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n' || content[i] == '\r') {
      W3X_RETURN_IF_ERROR(
          consume_line(std::string_view(content).substr(line_start, i - line_start)));
      if (content[i] == '\r' && i + 1 < content.size() && content[i + 1] == '\n') {
        ++i;
      }
      line_start = i + 1;
    }
  }
  if (line_start < content.size()) {
    W3X_RETURN_IF_ERROR(
        consume_line(std::string_view(content).substr(line_start)));
  }

  for (auto it = tables.begin(); it != tables.end();) {
    for (auto entry_it = it->second.begin(); entry_it != it->second.end();) {
      if (entry_it->second.field.empty() || entry_it->second.id.empty()) {
        entry_it = it->second.erase(entry_it);
      } else {
        ++entry_it;
      }
    }
    if (it->second.empty()) {
      it = tables.erase(it);
    } else {
      ++it;
    }
  }

  return tables;
}

core::Result<fs::path> ResolvePrebuiltRoot() {
  core::ConfigManager config;
  W3X_RETURN_IF_ERROR(
      config.LoadDefaults(core::RuntimePaths::ResolveDefaultConfigPath()));
  W3X_RETURN_IF_ERROR(
      config.LoadGlobal(core::RuntimePaths::ResolveGlobalConfigPath()));

  if (auto data_root = core::RuntimePaths::ResolveDataRoot();
      data_root.has_value()) {
    const std::string preferred_version =
        config.GetString("global", "data", "zhCN-1.32.8");
    const fs::path preferred = data_root.value() / preferred_version / "prebuilt";
    if (fs::exists(preferred)) {
      return preferred;
    }

    for (const auto& entry : fs::directory_iterator(data_root.value())) {
      if (!entry.is_directory()) {
        continue;
      }
      const fs::path candidate = entry.path() / "prebuilt";
      if (fs::exists(candidate)) {
        return candidate;
      }
    }
  }

  const fs::path source_root = core::RuntimePaths::GetSourceRoot();
  if (!source_root.empty()) {
    const std::string preferred_version =
        config.GetString("global", "data", "zhCN-1.32.8");
    const fs::path preferred =
        source_root / "external" / "data" / preferred_version / "prebuilt";
    if (fs::exists(preferred)) {
      return preferred;
    }

    const fs::path fallback_data_root = source_root / "external" / "data";
    if (fs::exists(fallback_data_root)) {
      for (const auto& entry : fs::directory_iterator(fallback_data_root)) {
        if (!entry.is_directory()) {
          continue;
        }
        const fs::path candidate = entry.path() / "prebuilt";
        if (fs::exists(candidate)) {
          return candidate;
        }
      }
    }
  }

  return std::unexpected(core::Error::FileNotFound(
      "No bundled or fallback prebuilt metadata directory was found"));
}

core::Result<std::unordered_set<std::string>> LoadDefaultIds(
    const fs::path& prebuilt_root, std::string_view logical_name) {
  const fs::path path =
      prebuilt_root / "Custom" / (std::string(logical_name) + ".ini");
  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(path).transform_error(
          [&path](const std::string& error) {
            return core::Error::IOError("Failed to read default id file '" +
                                        path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto sections, ParseSectionedText(content));
  std::unordered_set<std::string> ids;
  ids.reserve(sections.size());
  for (const auto& [name, _] : sections) {
    ids.insert(name);
  }
  return ids;
}

core::Result<std::unordered_map<std::string, std::string>> LoadSlkCodeMap(
    const fs::path& slk_path) {
  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(slk_path).transform_error(
          [&slk_path](const std::string& error) {
            return core::Error::IOError("Failed to read slk file '" +
                                        slk_path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto table, parser::slk::ParseSlk(content));

  int alias_col = -1;
  int code_col = -1;
  for (int col = 0; col < table.num_columns; ++col) {
    const std::string normalized = NormalizeKey(table.GetCell(0, col));
    if (normalized == "alias" || normalized == "unitid" ||
        normalized == "itemid" || normalized == "upgradeid" ||
        normalized == "destructableid" || normalized == "doodid") {
      alias_col = col;
    } else if (normalized == "code") {
      code_col = col;
    }
  }

  std::unordered_map<std::string, std::string> result;
  if (alias_col < 0 || code_col < 0) {
    return result;
  }

  for (int row = 1; row < table.num_rows; ++row) {
    const std::string& alias = table.GetCell(row, alias_col);
    const std::string& code = table.GetCell(row, code_col);
    if (!alias.empty() && !code.empty()) {
      result.emplace(alias, code);
    }
  }
  return result;
}

MetadataTable MergeMetadataForObject(const MetadataTables& tables,
                                     std::string_view logical_name,
                                     const std::optional<std::string>& code) {
  MetadataTable merged;
  if (const auto it = tables.find(std::string(logical_name)); it != tables.end()) {
    merged = it->second;
  }
  if (code.has_value()) {
    if (const auto it = tables.find(*code); it != tables.end()) {
      for (const auto& [key, value] : it->second) {
        merged[key] = value;
      }
    }
  }
  return merged;
}

std::unordered_map<std::string, std::vector<MetaEntry>> BuildFieldLookup(
    const MetadataTable& metadata) {
  std::unordered_map<std::string, std::vector<MetaEntry>> lookup;
  for (const auto& [key, meta] : metadata) {
    (void)key;
    lookup[NormalizeKey(meta.field)].push_back(meta);
  }
  for (auto& [field, metas] : lookup) {
    std::ranges::sort(metas, [](const MetaEntry& lhs, const MetaEntry& rhs) {
      return lhs.index.value_or(0) < rhs.index.value_or(0);
    });
  }
  return lookup;
}

std::vector<std::pair<std::string, std::string>> NormalizeSectionEntries(
    const SectionEntries& entries) {
  std::vector<std::pair<std::string, std::string>> normalized;
  normalized.reserve(entries.size());
  for (const auto& [key, value] : entries) {
    if (!key.empty() && key.front() == '_') {
      continue;
    }
    normalized.push_back({NormalizeKey(key), TrimCopy(value)});
  }
  std::ranges::sort(normalized, [](const auto& lhs, const auto& rhs) {
    if (lhs.first != rhs.first) {
      return lhs.first < rhs.first;
    }
    return lhs.second < rhs.second;
  });
  return normalized;
}

std::string QuoteIniString(std::string_view value) {
  const bool needs_quotes =
      value.empty() || value.find(',') != std::string_view::npos ||
      value.find('"') != std::string_view::npos ||
      value.find('\r') != std::string_view::npos ||
      value.find('\n') != std::string_view::npos ||
      std::isspace(static_cast<unsigned char>(value.front())) ||
      std::isspace(static_cast<unsigned char>(value.back()));
  if (!needs_quotes) {
    return std::string(value);
  }

  std::string quoted;
  quoted.reserve(value.size() + 2);
  quoted.push_back('"');
  for (char ch : value) {
    if (ch == '"') {
      quoted.push_back('"');
    }
    quoted.push_back(ch);
  }
  quoted.push_back('"');
  return quoted;
}

std::string FormatFloat(float value) {
  std::ostringstream stream;
  stream << std::setprecision(9) << value;
  return stream.str();
}

std::string FormatModificationValue(const parser::w3u::Modification& modification) {
  switch (modification.type) {
    case parser::w3u::ModificationType::kInteger:
      return std::to_string(std::get<std::int32_t>(modification.value));
    case parser::w3u::ModificationType::kReal:
    case parser::w3u::ModificationType::kUnreal:
      return FormatFloat(std::get<float>(modification.value));
    case parser::w3u::ModificationType::kString:
      return QuoteIniString(std::get<std::string>(modification.value));
  }
  return {};
}

std::string JoinFieldValues(const FieldAggregation& field) {
  if (!field.indexed_values.empty()) {
    const int max_index = field.indexed_values.rbegin()->first;
    std::string joined;
    for (int index = 1; index <= max_index; ++index) {
      if (index > 1) {
        joined += ",";
      }
      if (const auto it = field.indexed_values.find(index);
          it != field.indexed_values.end()) {
        joined += it->second;
      }
    }
    return joined;
  }
  if (!field.repeated_values.empty()) {
    const int max_level = field.repeated_values.rbegin()->first;
    std::string joined;
    for (int level = 1; level <= max_level; ++level) {
      if (level > 1) {
        joined += ",";
      }
      if (const auto it = field.repeated_values.find(level);
          it != field.repeated_values.end()) {
        joined += it->second;
      }
    }
    return joined;
  }
  return field.scalar_value.value_or("");
}

std::string EncodeRawFieldKey(const parser::w3u::Modification& modification) {
  return "raw(" + modification.field_id + "," +
         std::to_string(static_cast<int>(modification.type)) + "," +
         std::to_string(modification.level) + "," +
         std::to_string(modification.data_pointer) + ")";
}

struct RawFieldDescriptor {
  std::string field_id;
  parser::w3u::ModificationType type;
  int level = 0;
  int data_pointer = 0;
};

std::optional<RawFieldDescriptor> TryParseRawFieldKey(std::string_view key) {
  const std::string trimmed = TrimCopy(key);
  if (!trimmed.starts_with("raw(") || !trimmed.ends_with(")")) {
    return std::nullopt;
  }
  const std::vector<std::string> parts =
      SplitCsvPreserveEmpty(trimmed.substr(4, trimmed.size() - 5));
  if (parts.size() != 4 || parts[0].empty()) {
    return std::nullopt;
  }

  const auto parsed_type = ParseInt32(parts[1]);
  const auto parsed_level = ParseInt32(parts[2]);
  const auto parsed_data_pointer = ParseInt32(parts[3]);
  if (!parsed_type.has_value() || !parsed_level.has_value() ||
      !parsed_data_pointer.has_value() || *parsed_type < 0 ||
      *parsed_type > 3) {
    return std::nullopt;
  }

  return RawFieldDescriptor{
      StripQuotes(parts[0]),
      static_cast<parser::w3u::ModificationType>(*parsed_type),
      *parsed_level,
      *parsed_data_pointer,
  };
}

std::optional<std::string> TryResolveParentFromSection(
    const std::string& object_name, const IniSections& sections,
    const std::unordered_set<std::string>& default_ids) {
  const auto object_it = sections.find(object_name);
  if (object_it == sections.end()) {
    return std::nullopt;
  }

  for (const auto& [key, value] : object_it->second) {
    if (NormalizeKey(key) == "_parent") {
      const std::string parent = StripQuotes(value);
      if (!parent.empty()) {
        return parent;
      }
    }
  }

  const auto signature = NormalizeSectionEntries(object_it->second);
  if (signature.empty()) {
    return std::nullopt;
  }

  for (const auto& [candidate_name, candidate_entries] : sections) {
    if (!default_ids.contains(candidate_name)) {
      continue;
    }
    if (candidate_name == object_name) {
      continue;
    }
    if (NormalizeSectionEntries(candidate_entries) == signature) {
      return candidate_name;
    }
  }
  return std::nullopt;
}

core::Result<parser::w3u::Modification> BuildScalarModification(
    const MetaEntry& meta, std::string_view raw_value, int level) {
  parser::w3u::Modification modification;
  modification.field_id = meta.id;
  modification.type = meta.type;
  modification.level = level;
  modification.data_pointer = meta.data_pointer.value_or(0);

  switch (meta.type) {
    case parser::w3u::ModificationType::kInteger: {
      const auto parsed = ParseInt32(raw_value);
      if (!parsed.has_value()) {
        return std::unexpected(core::Error::InvalidFormat(
            "Failed to parse integer field '" + meta.field + "'"));
      }
      modification.value = parsed.value();
      break;
    }
    case parser::w3u::ModificationType::kReal:
    case parser::w3u::ModificationType::kUnreal: {
      const auto parsed = ParseFloat(raw_value);
      if (!parsed.has_value()) {
        return std::unexpected(core::Error::InvalidFormat(
            "Failed to parse float field '" + meta.field + "'"));
      }
      modification.value = parsed.value();
      break;
    }
    case parser::w3u::ModificationType::kString:
      modification.value = StripQuotes(raw_value);
      break;
  }
  return modification;
}

core::Result<std::vector<parser::w3u::Modification>> BuildModificationsForField(
    const std::string& key, const std::string& value,
    const std::unordered_map<std::string, std::vector<MetaEntry>>& lookup) {
  std::vector<parser::w3u::Modification> modifications;
  const auto it = lookup.find(NormalizeKey(key));
  if (it == lookup.end()) {
    if (const auto raw = TryParseRawFieldKey(key); raw.has_value()) {
      W3X_ASSIGN_OR_RETURN(
          auto modification,
          BuildScalarModification(
              MetaEntry{
                  .field = raw->field_id,
                  .id = raw->field_id,
                  .type = raw->type,
                  .data_pointer = raw->data_pointer,
              },
              value, raw->level));
      modifications.push_back(std::move(modification));
    }
    return modifications;
  }

  const std::vector<std::string> parts = SplitCsvPreserveEmpty(value);
  for (const MetaEntry& meta : it->second) {
    if (meta.index.has_value()) {
      const std::size_t index = static_cast<std::size_t>(meta.index.value() - 1);
      const std::string part = index < parts.size() ? parts[index] : std::string();
      W3X_ASSIGN_OR_RETURN(auto modification,
                           BuildScalarModification(meta, part, 0));
      modifications.push_back(std::move(modification));
      continue;
    }

    if (meta.repeat) {
      for (std::size_t i = 0; i < parts.size(); ++i) {
        W3X_ASSIGN_OR_RETURN(
            auto modification,
            BuildScalarModification(meta, parts[i], static_cast<int>(i + 1)));
        modifications.push_back(std::move(modification));
      }
      continue;
    }

    W3X_ASSIGN_OR_RETURN(auto modification,
                         BuildScalarModification(meta, value, 0));
    modifications.push_back(std::move(modification));
  }
  return modifications;
}

std::unordered_map<std::string, std::vector<ReverseMetaEntry>> BuildIdLookup(
    const MetadataTable& metadata) {
  std::unordered_map<std::string, std::vector<ReverseMetaEntry>> lookup;
  for (const auto& [key, meta] : metadata) {
    lookup[NormalizeKey(meta.id)].push_back(ReverseMetaEntry{key, meta});
  }
  return lookup;
}

const ReverseMetaEntry* FindReverseMetaEntry(
    const std::unordered_map<std::string, std::vector<ReverseMetaEntry>>& lookup,
    const parser::w3u::Modification& modification) {
  const auto it = lookup.find(NormalizeKey(modification.field_id));
  if (it == lookup.end()) {
    return nullptr;
  }

  const ReverseMetaEntry* best = nullptr;
  int best_score = -1;
  for (const ReverseMetaEntry& candidate : it->second) {
    if (candidate.meta.type != modification.type) {
      continue;
    }

    int score = 0;
    if (candidate.meta.data_pointer.has_value()) {
      if (candidate.meta.data_pointer.value() != modification.data_pointer) {
        continue;
      }
      score += 4;
    } else if (modification.data_pointer == 0) {
      score += 1;
    }

    if (candidate.meta.repeat) {
      score += modification.level > 0 ? 2 : 0;
    } else if (candidate.meta.index.has_value()) {
      score += 2;
    } else if (modification.level == 0) {
      score += 1;
    }

    if (score > best_score) {
      best = &candidate;
      best_score = score;
    }
  }
  return best;
}

core::Result<std::optional<GeneratedMapFile>> GenerateDerivedObjectIniFile(
    const fs::path& input_dir, const MetadataTables& metadata_tables,
    const SyntheticTypeSpec& spec) {
  const fs::path input_path = input_dir / spec.output_file;
  if (!core::FilesystemUtils::Exists(input_path)) {
    return std::optional<GeneratedMapFile>();
  }

  W3X_ASSIGN_OR_RETURN(
      auto bytes,
      core::FilesystemUtils::ReadBinaryFile(input_path).transform_error(
          [&input_path](const std::string& error) {
            return core::Error::IOError("Failed to read object file '" +
                                        input_path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto object_data,
                       parser::w3u::ParseObjectFile(bytes, spec.kind));

  std::string output;
  bool wrote_anything = false;
  const auto emit_object =
      [&](const parser::w3u::ObjectDef& object, bool is_custom)
      -> core::Result<void> {
    const std::string section_name =
        is_custom ? object.custom_id : object.original_id;
    if (section_name.empty()) {
      return {};
    }

    const MetadataTable merged = MergeMetadataForObject(
        metadata_tables, spec.logical_name, object.original_id);
    const auto reverse_lookup = BuildIdLookup(merged);

    std::map<std::string, FieldAggregation, std::less<>> fields;
    std::vector<std::pair<std::string, std::string>> raw_fields;
    raw_fields.reserve(object.modifications.size());
    for (const parser::w3u::Modification& modification : object.modifications) {
      if (const ReverseMetaEntry* meta =
              FindReverseMetaEntry(reverse_lookup, modification);
          meta != nullptr) {
        FieldAggregation& field = fields[meta->key];
        if (meta->meta.index.has_value()) {
          field.indexed_values[meta->meta.index.value()] =
              FormatModificationValue(modification);
        } else if (meta->meta.repeat) {
          field.repeated_values[std::max(1, modification.level)] =
              FormatModificationValue(modification);
        } else {
          field.scalar_value = FormatModificationValue(modification);
        }
        continue;
      }
      raw_fields.push_back(
          {EncodeRawFieldKey(modification), FormatModificationValue(modification)});
    }

    if (fields.empty() && raw_fields.empty()) {
      return {};
    }

    if (wrote_anything) {
      output += "\r\n";
    }
    output += "[" + section_name + "]\r\n";
    if (is_custom) {
      output += "_parent=" + object.original_id + "\r\n";
    }
    for (const auto& [key, field] : fields) {
      output += key + "=" + JoinFieldValues(field) + "\r\n";
    }
    std::ranges::sort(raw_fields, [](const auto& lhs, const auto& rhs) {
      return lhs.first < rhs.first;
    });
    for (const auto& [key, value] : raw_fields) {
      output += key + "=" + value + "\r\n";
    }
    wrote_anything = true;
    return {};
  };

  for (const parser::w3u::ObjectDef& object : object_data.original_objects) {
    W3X_RETURN_IF_ERROR(emit_object(object, false));
  }
  for (const parser::w3u::ObjectDef& object : object_data.custom_objects) {
    W3X_RETURN_IF_ERROR(emit_object(object, true));
  }

  if (!wrote_anything) {
    return std::optional<GeneratedMapFile>();
  }
  return std::optional<GeneratedMapFile>(
      GeneratedMapFile{spec.input_ini,
                       std::vector<std::uint8_t>(output.begin(), output.end())});
}

std::unordered_map<std::string, std::vector<ReverseMetaEntry>>
BuildMiscFieldLookup(const MetadataTable& metadata) {
  std::unordered_map<std::string, std::vector<ReverseMetaEntry>> lookup;
  for (const auto& [key, meta] : metadata) {
    lookup[NormalizeKey(meta.field)].push_back(ReverseMetaEntry{key, meta});
  }
  return lookup;
}

core::Result<std::optional<GeneratedMapFile>> GenerateDerivedMiscIniFile(
    const fs::path& input_dir, const MetadataTables& metadata_tables) {
  const fs::path input_path = input_dir / "war3mapmisc.txt";
  if (!core::FilesystemUtils::Exists(input_path)) {
    return std::optional<GeneratedMapFile>();
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(input_path).transform_error(
          [&input_path](const std::string& error) {
            return core::Error::IOError("Failed to read misc file '" +
                                        input_path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto sections, ParseSectionedText(content));

  std::vector<std::string> section_names;
  section_names.reserve(sections.size());
  for (const auto& [name, _] : sections) {
    section_names.push_back(name);
  }
  std::ranges::sort(section_names);

  std::string output;
  bool wrote_anything = false;
  for (const std::string& section_name : section_names) {
    const auto metadata_it = metadata_tables.find(section_name);
    if (metadata_it == metadata_tables.end()) {
      continue;
    }
    const auto lookup = BuildMiscFieldLookup(metadata_it->second);

    std::map<std::string, std::string, std::less<>> fields;
    for (const auto& [field_name, raw_value] : sections.at(section_name)) {
      if (field_name.empty() || field_name.front() == '_') {
        continue;
      }
      const auto lookup_it = lookup.find(NormalizeKey(field_name));
      if (lookup_it == lookup.end() || lookup_it->second.empty()) {
        continue;
      }
      const ReverseMetaEntry& entry = lookup_it->second.front();
      if (entry.meta.type == parser::w3u::ModificationType::kString) {
        fields[entry.key] = QuoteIniString(StripQuotes(raw_value));
      } else {
        fields[entry.key] = TrimCopy(raw_value);
      }
    }

    if (fields.empty()) {
      continue;
    }

    if (wrote_anything) {
      output += "\r\n";
    }
    output += "[" + section_name + "]\r\n";
    for (const auto& [key, value] : fields) {
      output += key + "=" + value + "\r\n";
    }
    wrote_anything = true;
  }

  if (!wrote_anything) {
    return std::optional<GeneratedMapFile>();
  }
  return std::optional<GeneratedMapFile>(
      GeneratedMapFile{"misc.ini",
                       std::vector<std::uint8_t>(output.begin(), output.end())});
}

core::Result<std::vector<GeneratedMapFile>> GenerateSyntheticObjectFile(
    const fs::path& input_dir, const fs::path& prebuilt_root,
    const MetadataTables& metadata_tables, const SyntheticTypeSpec& spec) {
  const fs::path input_path = input_dir / spec.input_ini;
  if (!core::FilesystemUtils::Exists(input_path)) {
    return std::vector<GeneratedMapFile>{};
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(input_path).transform_error(
          [&input_path](const std::string& error) {
            return core::Error::IOError("Failed to read object ini '" +
                                        input_path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto sections, ParseSectionedText(content));
  W3X_ASSIGN_OR_RETURN(auto default_ids,
                       LoadDefaultIds(prebuilt_root, spec.logical_name));

  std::unordered_map<std::string, std::string> code_map;
  if (spec.slk_code_file != nullptr) {
    const fs::path slk_path = input_dir / spec.slk_code_file;
    if (core::FilesystemUtils::Exists(slk_path)) {
      W3X_ASSIGN_OR_RETURN(code_map, LoadSlkCodeMap(slk_path));
    }
  }

  parser::w3u::ObjectData object_data;
  object_data.version = 2;

  std::vector<std::string> object_names;
  object_names.reserve(sections.size());
  for (const auto& [name, _] : sections) {
    object_names.push_back(name);
  }
  std::ranges::sort(object_names, std::greater<>());

  for (const std::string& object_name : object_names) {
    const bool is_original = default_ids.contains(object_name);
    std::optional<std::string> object_code;
    if (const auto it = code_map.find(object_name); it != code_map.end()) {
      object_code = it->second;
    }

    if (!is_original) {
      if (!object_code.has_value()) {
        object_code =
            TryResolveParentFromSection(object_name, sections, default_ids);
      }
      if (!object_code.has_value() &&
          (spec.type == SyntheticType::kAbility ||
           spec.type == SyntheticType::kBuff)) {
        return std::unexpected(core::Error::InvalidFormat(
            "Cannot determine parent rawcode for custom " +
            std::string(spec.logical_name) + " object '" + object_name + "'"));
      }
      if (!object_code.has_value()) {
        return std::unexpected(core::Error::InvalidFormat(
            "Custom " + std::string(spec.logical_name) +
            " objects cannot be packed yet without parent information: '" +
            object_name + "'"));
      }
    }

    const MetadataTable merged =
        MergeMetadataForObject(metadata_tables, spec.logical_name, object_code);
    const auto lookup = BuildFieldLookup(merged);
    std::vector<parser::w3u::Modification> modifications;
    for (const auto& [key, value] : sections.at(object_name)) {
      W3X_ASSIGN_OR_RETURN(
          auto field_modifications,
          BuildModificationsForField(key, value, lookup));
      modifications.insert(modifications.end(),
                           std::make_move_iterator(field_modifications.begin()),
                           std::make_move_iterator(field_modifications.end()));
    }

    if (modifications.empty()) {
      continue;
    }

    parser::w3u::ObjectDef object;
    if (is_original) {
      object.original_id = object_name;
      object.custom_id.clear();
      object.modifications = std::move(modifications);
      object_data.original_objects.push_back(std::move(object));
    } else {
      object.original_id = *object_code;
      object.custom_id = object_name;
      object.modifications = std::move(modifications);
      object_data.custom_objects.push_back(std::move(object));
    }
  }

  if (object_data.original_objects.empty() && object_data.custom_objects.empty()) {
    return std::vector<GeneratedMapFile>{};
  }

  W3X_ASSIGN_OR_RETURN(
      auto bytes,
      parser::w3u::SerializeObjectFile(object_data, spec.kind));
  return std::vector<GeneratedMapFile>{
      GeneratedMapFile{spec.output_file, std::move(bytes)}};
}

std::string FormatMiscValue(const MetaEntry& meta, const std::string& value) {
  if (meta.type == parser::w3u::ModificationType::kInteger) {
    const auto parsed = ParseInt32(value);
    return parsed.has_value() ? std::to_string(parsed.value()) : "0";
  }
  if (meta.type == parser::w3u::ModificationType::kReal ||
      meta.type == parser::w3u::ModificationType::kUnreal) {
    const std::string stripped = StripQuotes(value);
    return stripped.empty() ? "0" : stripped;
  }
  return StripQuotes(value);
}

core::Result<std::optional<GeneratedMapFile>> GenerateSyntheticMiscFile(
    const fs::path& input_dir, const MetadataTables& metadata_tables) {
  const fs::path input_path = input_dir / "misc.ini";
  if (!core::FilesystemUtils::Exists(input_path)) {
    return std::optional<GeneratedMapFile>();
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(input_path).transform_error(
          [&input_path](const std::string& error) {
            return core::Error::IOError("Failed to read misc ini '" +
                                        input_path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto sections, ParseSectionedText(content));

  std::vector<std::string> section_names;
  section_names.reserve(sections.size());
  for (const auto& [name, _] : sections) {
    section_names.push_back(name);
  }
  std::ranges::sort(section_names);

  std::string output;
  bool wrote_anything = false;
  for (const std::string& section_name : section_names) {
    const auto metadata_it = metadata_tables.find(section_name);
    if (metadata_it == metadata_tables.end()) {
      continue;
    }
    const auto lookup = BuildFieldLookup(metadata_it->second);
    std::vector<std::pair<std::string, std::string>> lines;
    for (const auto& [key, value] : sections.at(section_name)) {
      const auto it = lookup.find(NormalizeKey(key));
      if (it == lookup.end() || it->second.empty()) {
        continue;
      }
      const MetaEntry& meta = it->second.front();
      lines.push_back({meta.field, FormatMiscValue(meta, value)});
    }
    if (lines.empty()) {
      continue;
    }

    std::ranges::sort(lines, [](const auto& lhs, const auto& rhs) {
      return lhs.first < rhs.first;
    });
    if (wrote_anything) {
      output += "\r\n";
    }
    output += "[" + section_name + "]\r\n";
    for (const auto& [field, value] : lines) {
      output += field + "=" + value + "\r\n";
    }
    wrote_anything = true;
  }

  if (!wrote_anything) {
    return std::optional<GeneratedMapFile>();
  }
  return std::optional<GeneratedMapFile>(
      GeneratedMapFile{"war3mapmisc.txt",
                       std::vector<std::uint8_t>(output.begin(), output.end())});
}

}  // namespace

core::Result<std::vector<GeneratedMapFile>> GenerateSyntheticMapFiles(
    const fs::path& input_dir) {
  W3X_ASSIGN_OR_RETURN(auto prebuilt_root, ResolvePrebuiltRoot());
  W3X_ASSIGN_OR_RETURN(auto metadata_tables,
                       LoadMetadataTables(prebuilt_root / "metadata.ini"));

  std::vector<GeneratedMapFile> generated;
  for (const SyntheticTypeSpec& spec : kSyntheticSpecs) {
    W3X_ASSIGN_OR_RETURN(auto files,
                         GenerateSyntheticObjectFile(input_dir, prebuilt_root,
                                                     metadata_tables, spec));
    generated.insert(generated.end(), std::make_move_iterator(files.begin()),
                     std::make_move_iterator(files.end()));
  }

  W3X_ASSIGN_OR_RETURN(auto misc_file,
                       GenerateSyntheticMiscFile(input_dir, metadata_tables));
  if (misc_file.has_value()) {
    generated.push_back(std::move(*misc_file));
  }
  return generated;
}

core::Result<std::vector<GeneratedMapFile>> GenerateDerivedTableFiles(
    const fs::path& input_dir) {
  W3X_ASSIGN_OR_RETURN(auto prebuilt_root, ResolvePrebuiltRoot());
  W3X_ASSIGN_OR_RETURN(auto metadata_tables,
                       LoadMetadataTables(prebuilt_root / "metadata.ini"));

  std::vector<GeneratedMapFile> generated;
  for (const SyntheticTypeSpec& spec : kSyntheticSpecs) {
    W3X_ASSIGN_OR_RETURN(auto derived_file,
                         GenerateDerivedObjectIniFile(input_dir, metadata_tables,
                                                      spec));
    if (derived_file.has_value()) {
      generated.push_back(std::move(*derived_file));
    }
  }

  W3X_ASSIGN_OR_RETURN(auto misc_file,
                       GenerateDerivedMiscIniFile(input_dir, metadata_tables));
  if (misc_file.has_value()) {
    generated.push_back(std::move(*misc_file));
  }
  return generated;
}

}  // namespace w3x_toolkit::parser::w3x
