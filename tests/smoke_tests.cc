// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "cli/commands/analyze_command.h"
#include "cli/commands/config_command.h"
#include "cli/commands/convert_command.h"
#include "cli/commands/extract_command.h"
#include "cli/commands/log_command.h"
#include "cli/commands/pack_command.h"
#include "cli/commands/template_command.h"
#include "cli/commands/unpack_command.h"
#include "core/filesystem/filesystem_utils.h"
#include "parser/w3u/w3u_parser.h"
#include "parser/w3u/w3u_writer.h"

namespace fs = std::filesystem;

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto unique_suffix =
        std::to_string(std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count());
    path_ = fs::temp_directory_path() /
            ("w3x_toolkit_smoke_tests_" + unique_suffix);
    fs::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

class CurrentDirectoryGuard {
 public:
  explicit CurrentDirectoryGuard(const fs::path& path)
      : original_(fs::current_path()) {
    fs::current_path(path);
  }

  ~CurrentDirectoryGuard() { fs::current_path(original_); }

 private:
  fs::path original_;
};

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void ExpectFileExists(const fs::path& path) {
  Expect(fs::exists(path), "Expected file to exist: " + path.string());
}

void WriteText(const fs::path& path, const std::string& content) {
  auto result = w3x_toolkit::core::FilesystemUtils::WriteTextFile(path, content);
  if (!result.has_value()) {
    throw std::runtime_error(result.error());
  }
}

std::string ReadText(const fs::path& path) {
  auto result = w3x_toolkit::core::FilesystemUtils::ReadTextFile(path);
  if (!result.has_value()) {
    throw std::runtime_error(result.error());
  }
  return result.value();
}

std::vector<std::uint8_t> ReadBinary(const fs::path& path) {
  auto result = w3x_toolkit::core::FilesystemUtils::ReadBinaryFile(path);
  if (!result.has_value()) {
    throw std::runtime_error(result.error());
  }
  return result.value();
}

std::vector<std::uint8_t> BuildMinimalW3i() {
  std::vector<std::uint8_t> data;

  auto append_int32 = [&data](std::int32_t value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    data.insert(data.end(), bytes, bytes + sizeof(value));
  };
  auto append_uint32 = [&data](std::uint32_t value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    data.insert(data.end(), bytes, bytes + sizeof(value));
  };
  auto append_float = [&data](float value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    data.insert(data.end(), bytes, bytes + sizeof(value));
  };
  auto append_byte = [&data](std::uint8_t value) {
    data.push_back(value);
  };
  auto append_string = [&data](std::string_view value) {
    data.insert(data.end(), value.begin(), value.end());
    data.push_back(0);
  };
  auto append_fixed_string = [&data](std::string_view value, std::size_t size) {
    const auto actual_size = std::min(value.size(), size);
    data.insert(data.end(), value.begin(), value.begin() + actual_size);
    data.insert(data.end(), size - actual_size, 0);
  };

  append_int32(28);
  append_int32(1);
  append_int32(1);
  append_int32(1);
  append_int32(0);
  append_int32(0);
  append_int32(0);
  append_string("Smoke Test Map");
  append_string("Codex");
  append_string("Directory-mode smoke test");
  append_string("1v1");

  for (int i = 0; i < 8; ++i) {
    append_float(0.0f);
  }
  for (int i = 0; i < 4; ++i) {
    append_int32(0);
  }

  append_int32(64);
  append_int32(64);
  append_uint32(0);
  append_byte(static_cast<std::uint8_t>('A'));

  append_int32(0);
  append_string("");
  append_string("");
  append_string("");
  append_string("");
  append_int32(0);

  append_string("");
  append_string("");
  append_string("");
  append_string("");

  append_int32(0);
  append_float(0.0f);
  append_float(0.0f);
  append_float(0.0f);
  append_byte(0);
  append_byte(0);
  append_byte(0);
  append_byte(0);

  append_fixed_string("", 4);
  append_string("");
  append_byte(0);
  append_byte(0);
  append_byte(0);
  append_byte(0);
  append_byte(0);

  append_int32(0);

  append_int32(1);
  append_int32(0);
  append_int32(1);
  append_int32(1);
  append_int32(0);
  append_string("Player 1");
  append_float(0.0f);
  append_float(0.0f);
  append_uint32(0);
  append_uint32(0);

  append_int32(0);

  return data;
}

void WriteBinary(const fs::path& path, const std::vector<std::uint8_t>& content) {
  auto result =
      w3x_toolkit::core::FilesystemUtils::WriteBinaryFile(path, content);
  if (!result.has_value()) {
    throw std::runtime_error(result.error());
  }
}

void WriteMinimalRuntimeData(const fs::path& root) {
  const fs::path data_root = root / "data";
  fs::create_directories(data_root / "zhCN-1.32.8" / "prebuilt" / "Melee");
  fs::create_directories(data_root / "zhCN-1.32.8" / "prebuilt" / "Custom");

  WriteText(data_root / "default_config.ini",
            "[global]\n"
            "lang = zhCN\n"
            "data = zhCN-1.32.8\n"
            "data_ui = ${YDWE}\n"
            "data_meta = ${DEFAULT}\n"
            "data_wes = ${DEFAULT}\n"
            "data_load = script\\backend\\data_load.lua\n"
            "\n"
            "[lni]\n"
            "read_slk = false\n"
            "find_id_times = 0\n"
            "export_lua = true\n"
            "extra_check = false\n"
            "\n"
            "[slk]\n"
            "remove_unuse_object = true\n"
            "optimize_jass = true\n"
            "mdx_squf = true\n"
            "remove_we_only = true\n"
            "slk_doodad = true\n"
            "find_id_times = 10\n"
            "confused = false\n"
            "confusion = abc123_\n"
            "extra_check = false\n"
            "\n"
            "[obj]\n"
            "read_slk = false\n"
            "find_id_times = 0\n"
            "extra_check = false\n");

  WriteText(data_root / "zhCN-1.32.8" / "prebuilt" / "Melee" / "ability.ini",
            "[A000]\nName=Melee Ability\n");
  WriteText(data_root / "zhCN-1.32.8" / "prebuilt" / "Custom" / "unit.ini",
            "[hfoo]\nName=Custom Footman\n");
}

void TestDirectoryModeCommands() {
  TemporaryDirectory temp;
  const fs::path map_dir = temp.path() / "map_dir";
  const fs::path convert_out = temp.path() / "converted";
  const fs::path extract_out = temp.path() / "extracted";
  fs::create_directories(map_dir / "scripts");
  fs::create_directories(map_dir / "units");
  fs::create_directories(map_dir / "ui");
  fs::create_directories(map_dir / "textures");
  fs::create_directories(map_dir / "sound");

  WriteBinary(map_dir / "war3map.w3i", BuildMinimalW3i());
  WriteText(map_dir / "scripts" / "war3map.j", "function main takes nothing returns nothing\nendfunction\n");
  WriteText(map_dir / "units" / "war3map.w3u", "raw object data");
  WriteText(map_dir / "war3map.wts", "STRING 1\n{\nHello\n}\n");
  WriteText(map_dir / "ui" / "Frame.fdf", "Frame \"SIMPLEFRAME\" \"Test\" {}\n");
  WriteText(map_dir / "textures" / "Preview.blp", "fake texture");
  WriteText(map_dir / "sound" / "Theme.mp3", "fake sound");

  w3x_toolkit::cli::ConvertCommand convert;
  auto convert_result =
      convert.Execute({map_dir.string(), convert_out.string()});
  Expect(convert_result.has_value(), "Convert command should succeed");
  ExpectFileExists(convert_out / ".w3x");
  ExpectFileExists(convert_out / "map" / "war3map.w3i");
  ExpectFileExists(convert_out / "map" / "war3map.wts");
  ExpectFileExists(convert_out / "table" / "units" / "war3map.w3u");
  ExpectFileExists(convert_out / "trigger" / "scripts" / "war3map.j");
  ExpectFileExists(convert_out / "resource" / "textures" / "Preview.blp");
  ExpectFileExists(convert_out / "sound" / "sound" / "Theme.mp3");
  ExpectFileExists(convert_out / "table" / "w3i.ini");
  ExpectFileExists(convert_out / "table" / "imp.ini");
  ExpectFileExists(convert_out / "w3x2lni" / "locale" / "w3i.lng");
  ExpectFileExists(convert_out / "w3x2lni" / "locale" / "lml.lng");
  ExpectFileExists(convert_out / "w3x2lni" / "config.ini");
  ExpectFileExists(convert_out / "w3x2lni.ini");

  w3x_toolkit::cli::ExtractCommand extract;
  auto extract_result =
      extract.Execute({map_dir.string(), extract_out.string(), "--type=scripts"});
  Expect(extract_result.has_value(), "Extract command should succeed");
  ExpectFileExists(extract_out / "scripts" / "war3map.j");
  Expect(!fs::exists(extract_out / "ui" / "Frame.fdf"),
         "Script-only extraction should not copy UI files");

  w3x_toolkit::cli::AnalyzeCommand analyze;
  auto analyze_result = analyze.Execute({map_dir.string()});
  Expect(analyze_result.has_value(), "Analyze command should succeed");
}

void TestPackedArchiveRejection() {
  TemporaryDirectory temp;
  const fs::path packed_map = temp.path() / "test_map.w3x";
  WriteText(packed_map, "not a real archive");

  w3x_toolkit::cli::ConvertCommand convert;
  auto convert_result =
      convert.Execute({packed_map.string(), (temp.path() / "out").string()});
  Expect(!convert_result.has_value(),
         "Invalid packed map input should still be rejected");
}

void TestPackedArchiveCommands() {
  TemporaryDirectory temp;
  const fs::path map_dir = temp.path() / "packed_commands_input";
  const fs::path packed_map = temp.path() / "packed_commands_map.w3x";
  const fs::path convert_out = temp.path() / "packed_converted";
  const fs::path extract_out = temp.path() / "packed_extracted";

  fs::create_directories(map_dir / "scripts");
  WriteBinary(map_dir / "war3map.w3i", BuildMinimalW3i());
  WriteText(map_dir / "war3map.j",
            "function main takes nothing returns nothing\nendfunction\n");
  WriteText(map_dir / "scripts" / "helper.lua", "print('helper')\n");
  WriteText(map_dir / "war3map.wts", "STRING 1\n{\nPacked\n}\n");

  w3x_toolkit::cli::PackCommand pack;
  auto pack_result = pack.Execute({map_dir.string(), packed_map.string()});
  Expect(pack_result.has_value(), "Pack command should create a packed map");

  w3x_toolkit::cli::AnalyzeCommand analyze;
  auto analyze_result = analyze.Execute({packed_map.string()});
  Expect(analyze_result.has_value(),
         "Analyze command should accept packed map input");

  w3x_toolkit::cli::ExtractCommand extract;
  auto extract_result = extract.Execute(
      {packed_map.string(), extract_out.string(), "--type=scripts"});
  Expect(extract_result.has_value(),
         "Extract command should accept packed map input");
  ExpectFileExists(extract_out / "war3map.j");
  ExpectFileExists(extract_out / "scripts" / "helper.lua");

  w3x_toolkit::cli::ConvertCommand convert;
  auto convert_result = convert.Execute(
      {packed_map.string(), convert_out.string(), "--no-table-data"});
  Expect(convert_result.has_value(),
         "Convert command should accept packed map input");
  ExpectFileExists(convert_out / ".w3x");
  ExpectFileExists(convert_out / "map" / "war3map.w3i");
  ExpectFileExists(convert_out / "map" / "war3map.wts");
  ExpectFileExists(convert_out / "trigger" / "war3map.j");
  ExpectFileExists(convert_out / "trigger" / "scripts" / "helper.lua");
  ExpectFileExists(convert_out / "table" / "w3i.ini");
  ExpectFileExists(convert_out / "table" / "imp.ini");
  ExpectFileExists(convert_out / "w3x2lni" / "locale" / "w3i.lng");
  ExpectFileExists(convert_out / "w3x2lni" / "locale" / "lml.lng");
  ExpectFileExists(convert_out / "w3x2lni" / "config.ini");
  ExpectFileExists(convert_out / "w3x2lni.ini");
}

void TestConfigTemplateAndLogCommands() {
  TemporaryDirectory temp;
  WriteMinimalRuntimeData(temp.path());
  CurrentDirectoryGuard cwd_guard(temp.path());

  w3x_toolkit::cli::ConfigCommand config;
  auto config_result = config.Execute({"lni.export_lua=false"});
  Expect(config_result.has_value(), "Config command should update global config");
  ExpectFileExists(temp.path() / "config.ini");
  Expect(ReadText(temp.path() / "config.ini").find("export_lua = false") !=
             std::string::npos,
         "Global config should persist normalized values");

  const fs::path workspace = temp.path() / "workspace";
  auto map_config_result =
      config.Execute({"--map", workspace.string(), "obj.find_id_times=7"});
  Expect(map_config_result.has_value(),
         "Config command should update map config");
  ExpectFileExists(workspace / "w3x2lni" / "config.ini");
  Expect(ReadText(workspace / "w3x2lni" / "config.ini")
                 .find("find_id_times = 7") != std::string::npos,
         "Map config should persist normalized values");

  w3x_toolkit::cli::TemplateCommand template_command;
  const fs::path template_out = temp.path() / "template_export";
  auto template_result = template_command.Execute({template_out.string()});
  Expect(template_result.has_value(),
         "Template command should export bundled templates");
  ExpectFileExists(template_out / "Melee" / "ability.ini");
  ExpectFileExists(template_out / "Custom" / "unit.ini");

  WriteText(temp.path() / "w3x_toolkit.log", "line1\nline2\n");
  w3x_toolkit::cli::LogCommand log;
  auto log_result = log.Execute({});
  Expect(log_result.has_value(), "Log command should print the current log file");
}

void TestPackUnpackRoundTrip() {
  TemporaryDirectory temp;
  const fs::path map_dir = temp.path() / "packed_input";
  const fs::path packed_map = temp.path() / "packed_map.w3x";
  const fs::path unpacked_dir = temp.path() / "unpacked";
  const fs::path repacked_map = temp.path() / "repacked_map.w3x";
  const fs::path repacked_unpacked_dir = temp.path() / "repacked_unpacked";

  fs::create_directories(map_dir / "units");

  WriteBinary(map_dir / "war3map.w3i", BuildMinimalW3i());
  WriteText(map_dir / "war3map.j",
            "function main takes nothing returns nothing\nendfunction\n");
  WriteText(map_dir / "ability.ini",
            "[A000]\nName=TestAbility\nTip=Test Tip\n");
  WriteText(map_dir / "destructable.ini", "[ATg1]\nName=TestGate\nHP=123\n");
  WriteText(map_dir / "doodad.ini", "[ACt0]\nName=TestStall\nnumVar=2\n");
  WriteText(map_dir / "misc.ini", "[HERO]\nName=Test Hero\n");
  WriteText(map_dir / "units" / "abilitydata.slk",
            "ID;PWXL;N;E\n"
            "B;X2;Y2;D0\n"
            "C;X1;Y1;K\"alias\"\n"
            "C;X2;K\"code\"\n"
            "C;X1;Y2;K\"A000\"\n"
            "C;X2;K\"ANcl\"\n"
            "E\n");

  w3x_toolkit::cli::PackCommand pack;
  auto pack_result = pack.Execute({map_dir.string(), packed_map.string()});
  Expect(pack_result.has_value(), "Pack command should succeed");
  ExpectFileExists(packed_map);

  w3x_toolkit::cli::UnpackCommand unpack;
  auto unpack_result =
      unpack.Execute({packed_map.string(), unpacked_dir.string()});
  Expect(unpack_result.has_value(), "Unpack command should succeed");
  ExpectFileExists(unpacked_dir / ".w3x");
  ExpectFileExists(unpacked_dir / "map" / "war3map.w3i");
  ExpectFileExists(unpacked_dir / "map" / "war3map.j");
  ExpectFileExists(unpacked_dir / "table" / "ability.ini");
  ExpectFileExists(unpacked_dir / "table" / "destructable.ini");
  ExpectFileExists(unpacked_dir / "table" / "doodad.ini");
  ExpectFileExists(unpacked_dir / "table" / "units" / "abilitydata.slk");
  ExpectFileExists(unpacked_dir / "map" / "war3map.w3a");
  ExpectFileExists(unpacked_dir / "map" / "war3map.w3b");
  ExpectFileExists(unpacked_dir / "map" / "war3map.w3d");
  ExpectFileExists(unpacked_dir / "map" / "war3mapmisc.txt");
  ExpectFileExists(unpacked_dir / "w3x2lni" / ".w3x_manifest.json");

  auto ability_obj =
      w3x_toolkit::parser::w3u::ParseW3a(
          ReadBinary(unpacked_dir / "map" / "war3map.w3a"));
  Expect(ability_obj.has_value(), "Generated war3map.w3a should be parseable");
  Expect(ability_obj->custom_objects.size() == 1,
         "Generated war3map.w3a should contain one custom object");
  Expect(ability_obj->custom_objects[0].original_id == "ANcl",
         "Custom object should preserve its parent rawcode");
  Expect(ability_obj->custom_objects[0].custom_id == "A000",
         "Custom object should preserve its custom rawcode");

  auto destructable_obj =
      w3x_toolkit::parser::w3u::ParseW3b(
          ReadBinary(unpacked_dir / "map" / "war3map.w3b"));
  Expect(destructable_obj.has_value(),
         "Generated war3map.w3b should be parseable");
  Expect(!destructable_obj->original_objects.empty(),
         "Generated war3map.w3b should contain one original object");

  auto doodad_obj = w3x_toolkit::parser::w3u::ParseW3d(
      ReadBinary(unpacked_dir / "map" / "war3map.w3d"));
  Expect(doodad_obj.has_value(), "Generated war3map.w3d should be parseable");
  Expect(!doodad_obj->original_objects.empty(),
         "Generated war3map.w3d should contain one original object");

  WriteText(unpacked_dir / "table" / "units" / "abilitydata.slk",
            ReadText(unpacked_dir / "table" / "units" / "abilitydata.slk") +
                "C;X1;Y2;K\"roundtrip\"\n");
  WriteText(unpacked_dir / "table" / "ability.ini",
            "[A000]\nName=UpdatedAbility\nTip=Updated Tip\n");

  auto repack_result =
      pack.Execute({unpacked_dir.string(), repacked_map.string()});
  Expect(repack_result.has_value(), "Pack command should repack unpacked dir");
  ExpectFileExists(repacked_map);

  auto re_unpacked_result =
      unpack.Execute({repacked_map.string(), repacked_unpacked_dir.string()});
  Expect(re_unpacked_result.has_value(),
         "Unpack command should unpack repacked archive");
  ExpectFileExists(repacked_unpacked_dir / ".w3x");
  ExpectFileExists(repacked_unpacked_dir / "map" / "war3map.w3a");
  ExpectFileExists(repacked_unpacked_dir / "map" / "war3map.w3b");
  ExpectFileExists(repacked_unpacked_dir / "map" / "war3map.w3d");
  ExpectFileExists(repacked_unpacked_dir / "map" / "war3mapmisc.txt");
  Expect(ReadText(repacked_unpacked_dir / "table" / "units" / "abilitydata.slk")
             .find("roundtrip") != std::string::npos,
         "Modified SLK content should survive repack");

  auto repacked_ability_obj = w3x_toolkit::parser::w3u::ParseW3a(
      ReadBinary(repacked_unpacked_dir / "map" / "war3map.w3a"));
  Expect(repacked_ability_obj.has_value(),
         "Repacked war3map.w3a should remain parseable");
  Expect(!repacked_ability_obj->custom_objects.empty(),
         "Repacked war3map.w3a should still contain the custom object");
  bool found_updated_name = false;
  for (const auto& modification :
       repacked_ability_obj->custom_objects[0].modifications) {
    if (modification.field_id == "anam" &&
        std::holds_alternative<std::string>(modification.value) &&
        std::get<std::string>(modification.value) == "UpdatedAbility") {
      found_updated_name = true;
      break;
    }
  }
  Expect(found_updated_name,
         "Repacked war3map.w3a should be regenerated from ability.ini");
}

void TestConvertObjectBinaryToIniRoundTrip() {
  TemporaryDirectory temp;
  const fs::path map_dir = temp.path() / "object_binary_input";
  const fs::path convert_out = temp.path() / "object_binary_workspace";
  const fs::path repacked_map = temp.path() / "object_binary_repacked.w3x";
  const fs::path repacked_unpacked_dir = temp.path() / "object_binary_unpacked";

  fs::create_directories(map_dir);
  WriteBinary(map_dir / "war3map.w3i", BuildMinimalW3i());

  w3x_toolkit::parser::w3u::ObjectData object_data;
  object_data.version = 2;
  w3x_toolkit::parser::w3u::ObjectDef ability;
  ability.original_id = "ANcl";
  ability.custom_id = "A000";
  ability.modifications.push_back({
      .field_id = "anam",
      .type = w3x_toolkit::parser::w3u::ModificationType::kString,
      .level = 0,
      .data_pointer = 0,
      .value = std::string("BinaryConvertedAbility"),
  });
  ability.modifications.push_back({
      .field_id = "zzzz",
      .type = w3x_toolkit::parser::w3u::ModificationType::kString,
      .level = 2,
      .data_pointer = 7,
      .value = std::string("mystery-value"),
  });
  object_data.custom_objects.push_back(ability);

  auto ability_bytes =
      w3x_toolkit::parser::w3u::SerializeObjectFile(
          object_data, w3x_toolkit::parser::w3u::ObjectFileKind::kComplex);
  Expect(ability_bytes.has_value(), "Ability object file should serialize");
  WriteBinary(map_dir / "war3map.w3a", ability_bytes.value());
  WriteText(map_dir / "war3mapmisc.txt", "[HERO]\nName=Binary Hero\n");

  w3x_toolkit::cli::ConvertCommand convert;
  auto convert_result =
      convert.Execute({map_dir.string(), convert_out.string()});
  Expect(convert_result.has_value(),
         "Convert should derive ini files from binary object data");
  ExpectFileExists(convert_out / "table" / "ability.ini");
  ExpectFileExists(convert_out / "table" / "misc.ini");

  const std::string ability_ini = ReadText(convert_out / "table" / "ability.ini");
  Expect(ability_ini.find("[A000]") != std::string::npos,
         "Derived ability.ini should contain the custom object section");
  Expect(ability_ini.find("_parent=ANcl") != std::string::npos,
         "Derived ability.ini should preserve parent rawcode");
  Expect(ability_ini.find("name=BinaryConvertedAbility") != std::string::npos,
         "Derived ability.ini should decode known metadata fields");
  Expect(ability_ini.find("raw(zzzz,3,2,7)=mystery-value") !=
             std::string::npos,
         "Derived ability.ini should preserve unknown fields via raw syntax");
  Expect(ReadText(convert_out / "table" / "misc.ini")
                 .find("name=Binary Hero") != std::string::npos,
         "Derived misc.ini should decode war3mapmisc.txt");

  w3x_toolkit::cli::PackCommand pack;
  auto repack_result =
      pack.Execute({convert_out.string(), repacked_map.string()});
  Expect(repack_result.has_value(),
         "Pack should accept convert output containing raw fallback syntax");

  w3x_toolkit::cli::UnpackCommand unpack;
  auto unpack_result =
      unpack.Execute({repacked_map.string(), repacked_unpacked_dir.string()});
  Expect(unpack_result.has_value(),
         "Repacked archive should unpack successfully");
  auto repacked_ability_obj = w3x_toolkit::parser::w3u::ParseW3a(
      ReadBinary(repacked_unpacked_dir / "map" / "war3map.w3a"));
  Expect(repacked_ability_obj.has_value(),
         "Repacked ability object file should remain parseable");
  Expect(!repacked_ability_obj->custom_objects.empty(),
         "Repacked ability object file should contain the custom object");

  bool found_name = false;
  bool found_unknown = false;
  for (const auto& modification :
       repacked_ability_obj->custom_objects[0].modifications) {
    if (modification.field_id == "anam" &&
        std::holds_alternative<std::string>(modification.value) &&
        std::get<std::string>(modification.value) ==
            "BinaryConvertedAbility") {
      found_name = true;
    }
    if (modification.field_id == "zzzz" &&
        modification.level == 2 &&
        modification.data_pointer == 7 &&
        std::holds_alternative<std::string>(modification.value) &&
        std::get<std::string>(modification.value) == "mystery-value") {
      found_unknown = true;
    }
  }
  Expect(found_name, "Known metadata fields should survive convert -> pack");
  Expect(found_unknown,
         "Unknown raw fields should survive convert -> pack via raw syntax");
}

}  // namespace

int main() {
  try {
    TestDirectoryModeCommands();
    TestPackedArchiveRejection();
    TestPackedArchiveCommands();
    TestPackUnpackRoundTrip();
    TestConfigTemplateAndLogCommands();
    TestConvertObjectBinaryToIniRoundTrip();
    std::cout << "Smoke tests passed." << std::endl;
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Smoke tests failed: " << ex.what() << std::endl;
    return 1;
  }
}
