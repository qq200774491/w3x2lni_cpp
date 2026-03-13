// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "cli/commands/analyze_command.h"
#include "cli/commands/config_command.h"
#include "cli/commands/convert_command.h"
#include "cli/commands/extract_command.h"
#include "cli/commands/log_command.h"
#include "cli/commands/obj_command.h"
#include "cli/commands/pack_command.h"
#include "cli/commands/slk_command.h"
#include "cli/commands/template_command.h"
#include "cli/commands/unpack_command.h"
#include "core/filesystem/filesystem_utils.h"
#include "parser/slk/slk_parser.h"
#include "parser/w3x/map_archive_io.h"
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

int FindSlkColumn(const w3x_toolkit::parser::slk::SlkTable& table,
                  std::string_view header) {
  for (int col = 0; col < table.num_columns; ++col) {
    if (table.GetCell(0, col) == header) {
      return col;
    }
  }
  return -1;
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

std::uint32_t RawcodeBigEndianValue(std::string_view rawcode) {
  if (rawcode.size() != 4) {
    throw std::runtime_error("Rawcode must contain exactly 4 characters");
  }
  return (static_cast<std::uint32_t>(static_cast<unsigned char>(rawcode[0]))
          << 24) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(rawcode[1]))
          << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(rawcode[2]))
          << 8) |
         static_cast<std::uint32_t>(static_cast<unsigned char>(rawcode[3]));
}

std::string RawcodeHexLiteral(std::string_view rawcode) {
  std::ostringstream out;
  out << "0x" << std::uppercase << std::hex << std::setfill('0')
      << std::setw(8) << RawcodeBigEndianValue(rawcode);
  return out.str();
}

std::vector<std::uint8_t> BuildMinimalDoo(
    const std::vector<std::string>& destructable_ids,
    const std::vector<std::string>& doodad_ids) {
  std::vector<std::uint8_t> data;

  auto append_int32 = [&data](std::int32_t value) {
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
  auto append_rawcode = [&data](std::string_view rawcode) {
    if (rawcode.size() != 4) {
      throw std::runtime_error("DOO rawcode must contain exactly 4 characters");
    }
    data.insert(data.end(), rawcode.begin(), rawcode.end());
  };

  append_int32(0);
  append_int32(7);
  append_int32(0);
  append_int32(static_cast<std::int32_t>(destructable_ids.size()));
  for (const std::string& rawcode : destructable_ids) {
    append_rawcode(rawcode);
    append_int32(0);
    append_float(0.0f);
    append_float(0.0f);
    append_float(0.0f);
    append_float(0.0f);
    append_float(1.0f);
    append_float(1.0f);
    append_float(1.0f);
    append_byte(0);
    append_byte(100);
    append_int32(0);
  }

  append_int32(0);
  append_int32(static_cast<std::int32_t>(doodad_ids.size()));
  for (const std::string& rawcode : doodad_ids) {
    append_rawcode(rawcode);
    append_int32(0);
    append_int32(0);
    append_int32(0);
  }

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
            "read_slk = true\n"
            "remove_unuse_object = true\n"
            "optimize_jass = true\n"
            "mdx_squf = true\n"
            "remove_we_only = true\n"
            "slk_doodad = true\n"
            "find_id_times = 10\n"
            "remove_same = false\n"
            "confused = false\n"
            "confusion = abc123_\n"
            "computed_text = true\n"
            "export_lua = true\n"
            "extra_check = false\n"
            "\n"
            "[obj]\n"
            "read_slk = false\n"
            "remove_unuse_object = false\n"
            "optimize_jass = false\n"
            "mdx_squf = false\n"
            "remove_we_only = false\n"
            "slk_doodad = false\n"
            "find_id_times = 0\n"
            "remove_same = true\n"
            "confused = false\n"
            "confusion = abc123_\n"
            "computed_text = false\n"
            "export_lua = true\n"
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

void TestObjCommandAndArchiveFiltering() {
  TemporaryDirectory temp;
  const fs::path workspace = temp.path() / "obj_workspace";
  const fs::path obj_map = temp.path() / "obj_output.w3x";
  const fs::path raw_unpacked = temp.path() / "obj_raw";
  const fs::path workspace_unpacked = temp.path() / "obj_workspace_out";

  fs::create_directories(workspace / "table");
  WriteBinary(workspace / ".w3x", std::vector<std::uint8_t>{'H', 'M', '3', 'W'});
  WriteBinary(workspace / "map" / "war3map.w3i", BuildMinimalW3i());
  WriteText(workspace / "table" / "ability.ini",
            "[A000]\n_parent=ANcl\nname=ObjAbility\n");
  WriteText(workspace / "table" / "misc.ini", "[HERO]\nname=Obj Hero\n");

  w3x_toolkit::cli::ObjCommand obj;
  auto obj_result = obj.Execute({workspace.string(), obj_map.string()});
  Expect(obj_result.has_value(), "Obj command should pack a workspace to .w3x");
  ExpectFileExists(obj_map);

  auto raw_unpack_result =
      w3x_toolkit::parser::w3x::UnpackMapArchive(obj_map, raw_unpacked);
  Expect(raw_unpack_result.has_value(),
         "Obj output archive should unpack to a raw directory");
  ExpectFileExists(raw_unpacked / "war3map.w3a");
  ExpectFileExists(raw_unpacked / "war3mapmisc.txt");
  Expect(!fs::exists(raw_unpacked / "ability.ini"),
         "Obj output archive should not carry source-only ability.ini");
  Expect(!fs::exists(raw_unpacked / "misc.ini"),
         "Obj output archive should not carry source-only misc.ini");

  w3x_toolkit::cli::UnpackCommand unpack;
  auto workspace_unpack_result =
      unpack.Execute({obj_map.string(), workspace_unpacked.string()});
  Expect(workspace_unpack_result.has_value(),
         "Obj output archive should unpack to workspace form");
  ExpectFileExists(workspace_unpacked / "table" / "ability.ini");
  Expect(ReadText(workspace_unpacked / "table" / "ability.ini")
                 .find("name=ObjAbility") != std::string::npos,
         "Unpack should derive ability.ini back from the Obj archive");
}

void TestSlkCommandGenerationCleanupAndSidecars() {
  TemporaryDirectory temp;
  w3x_toolkit::cli::SlkCommand slk;

  const fs::path passthrough_workspace = temp.path() / "slk_passthrough";
  const fs::path passthrough_map = temp.path() / "slk_passthrough.w3x";
  const fs::path passthrough_raw = temp.path() / "slk_passthrough_raw";
  fs::create_directories(passthrough_workspace / "map");
  fs::create_directories(passthrough_workspace / "table" / "units");
  WriteBinary(passthrough_workspace / ".w3x",
              std::vector<std::uint8_t>{'H', 'M', '3', 'W'});
  WriteBinary(passthrough_workspace / "map" / "war3map.w3i",
              BuildMinimalW3i());
  WriteText(passthrough_workspace / "map" / "war3map.w3a",
            "legacy ability object");
  WriteText(passthrough_workspace / "map" / "war3map.wtg", "legacy wtg");
  WriteText(passthrough_workspace / "map" / "war3map.wct", "legacy wct");
  WriteText(passthrough_workspace / "map" / "war3map.w3r", "legacy regions");
  WriteText(passthrough_workspace / "map" / "war3map.wts",
            "STRING 1\n{\nPassthrough\n}\n");
  WriteText(passthrough_workspace / "table" / "units" /
                "campaignabilitystrings.txt",
            "Passthrough campaign ability strings\n");
  WriteText(passthrough_workspace / "table" / "units" / "abilitydata.slk",
            "ID;PWXL;N;E\n"
            "B;X2;Y2;D0\n"
            "C;X1;Y1;K\"alias\"\n"
            "C;X2;K\"code\"\n"
            "C;X1;Y2;K\"A000\"\n"
            "C;X2;K\"ANcl\"\n"
            "E\n");

  auto passthrough_result =
      slk.Execute({passthrough_workspace.string(), passthrough_map.string()});
  Expect(passthrough_result.has_value(),
         "Slk command should pack a workspace containing existing slk/txt files");
  auto passthrough_unpack_result =
      w3x_toolkit::parser::w3x::UnpackMapArchive(passthrough_map,
                                                 passthrough_raw);
  Expect(passthrough_unpack_result.has_value(),
         "Passthrough slk archive should unpack");
  Expect(!fs::exists(passthrough_raw / "war3map.w3a"),
         "Existing covered SLK data should suppress redundant war3map.w3a");
  Expect(!fs::exists(passthrough_raw / "war3map.wtg"),
         "remove_we_only should drop war3map.wtg from slk output");
  Expect(!fs::exists(passthrough_raw / "war3map.wct"),
         "remove_we_only should drop war3map.wct from slk output");
  Expect(!fs::exists(passthrough_raw / "war3map.w3r"),
         "remove_we_only should drop war3map.w3r from slk output");
  ExpectFileExists(passthrough_raw / "units" / "abilitydata.slk");
  ExpectFileExists(passthrough_raw / "units" / "campaignabilitystrings.txt");
  ExpectFileExists(passthrough_raw / "war3map.wts");

  const fs::path generated_workspace = temp.path() / "slk_generated";
  const fs::path generated_map = temp.path() / "slk_generated.w3x";
  const fs::path generated_raw = temp.path() / "slk_generated_raw";
  fs::create_directories(generated_workspace / "map");
  fs::create_directories(generated_workspace / "table");
  WriteBinary(generated_workspace / ".w3x",
              std::vector<std::uint8_t>{'H', 'M', '3', 'W'});
  WriteBinary(generated_workspace / "map" / "war3map.w3i",
              BuildMinimalW3i());
  WriteText(generated_workspace / "map" / "war3map.j",
            "function main takes nothing returns nothing\n"
            "    call UnitAddAbility(null, 'A000')\n"
            "endfunction\n");
  WriteText(generated_workspace / "map" / "war3map.w3a",
            "stale object should be suppressed");
  WriteText(generated_workspace / "table" / "ability.ini",
            "[A000]\n_parent=ANcl\nname=GeneratedSlkAbility\n");

  auto generated_result =
      slk.Execute({generated_workspace.string(), generated_map.string()});
  Expect(generated_result.has_value(),
         "Slk command should generate slk output from ability.ini");
  auto generated_unpack_result =
      w3x_toolkit::parser::w3x::UnpackMapArchive(generated_map, generated_raw);
  Expect(generated_unpack_result.has_value(),
         "Generated slk archive should unpack");
  Expect(!fs::exists(generated_raw / "war3map.w3a"),
         "Ability ini covered by generated slk should suppress war3map.w3a");
  ExpectFileExists(generated_raw / "units" / "abilitydata.slk");
  auto generated_slk = w3x_toolkit::parser::slk::ParseSlk(
      ReadText(generated_raw / "units" / "abilitydata.slk"));
  Expect(generated_slk.has_value(),
         "Generated abilitydata.slk should be parseable");
  int alias_col = -1;
  int code_col = -1;
  for (int col = 0; col < generated_slk->num_columns; ++col) {
    const std::string header = generated_slk->GetCell(0, col);
    if (header == "alias") {
      alias_col = col;
    } else if (header == "code") {
      code_col = col;
    }
  }
  Expect(alias_col >= 0 && code_col >= 0,
         "Generated abilitydata.slk should contain alias/code headers");
  Expect(generated_slk->GetCell(1, alias_col) == "A000",
         "Generated abilitydata.slk should preserve alias");
  Expect(generated_slk->GetCell(1, code_col) == "ANcl",
         "Generated abilitydata.slk should preserve parent rawcode");

  const fs::path raw_workspace = temp.path() / "slk_raw_fallback";
  const fs::path raw_map = temp.path() / "slk_raw_fallback.w3x";
  const fs::path raw_archive = temp.path() / "slk_raw_fallback_raw";
  fs::create_directories(raw_workspace / "map");
  fs::create_directories(raw_workspace / "table");
  WriteBinary(raw_workspace / ".w3x",
              std::vector<std::uint8_t>{'H', 'M', '3', 'W'});
  WriteBinary(raw_workspace / "map" / "war3map.w3i", BuildMinimalW3i());
  WriteText(raw_workspace / "map" / "war3map.j",
            "function main takes nothing returns nothing\n"
            "    call UnitAddAbility(null, 'A000')\n"
            "endfunction\n");
  WriteText(raw_workspace / "table" / "ability.ini",
            "[A000]\n"
            "_parent=ANcl\n"
            "name=RawFallbackAbility\n"
            "raw(zzzz,3,2,7)=mystery-value\n");

  auto raw_result = slk.Execute({raw_workspace.string(), raw_map.string()});
  Expect(raw_result.has_value(),
         "Slk command should still succeed when raw fields require obj fallback");
  auto raw_unpack_result =
      w3x_toolkit::parser::w3x::UnpackMapArchive(raw_map, raw_archive);
  Expect(raw_unpack_result.has_value(),
         "Raw fallback slk archive should unpack");
  ExpectFileExists(raw_archive / "units" / "abilitydata.slk");
  ExpectFileExists(raw_archive / "war3map.w3a");

  auto raw_ability_obj = w3x_toolkit::parser::w3u::ParseW3a(
      ReadBinary(raw_archive / "war3map.w3a"));
  Expect(raw_ability_obj.has_value(),
         "Raw fallback war3map.w3a should be regenerated as a parseable object file");
  Expect(!raw_ability_obj->custom_objects.empty(),
         "Raw fallback war3map.w3a should contain the custom object");

  const fs::path unit_workspace = temp.path() / "slk_unit_split";
  const fs::path unit_map = temp.path() / "slk_unit_split.w3x";
  const fs::path unit_archive = temp.path() / "slk_unit_split_raw";
  fs::create_directories(unit_workspace / "units");
  WriteBinary(unit_workspace / "war3map.w3i", BuildMinimalW3i());
  WriteText(unit_workspace / "unit.ini",
            "[u000]\n"
            "_parent=hfoo\n"
            "Name=SplitUnit\n"
            "race=human\n"
            "movetp=foot\n"
            "HP=420\n"
            "abilList=Adef,Aatk\n"
            "rangeN1=100\n"
            "targs1=ground,air\n");
  for (const fs::path& stale_slk : {
           unit_workspace / "units" / "unitui.slk",
           unit_workspace / "units" / "unitdata.slk",
           unit_workspace / "units" / "unitbalance.slk",
           unit_workspace / "units" / "unitabilities.slk",
           unit_workspace / "units" / "unitweapons.slk"}) {
    WriteText(stale_slk, "stale unit slk");
  }

  auto unit_result = slk.Execute({unit_workspace.string(), unit_map.string()});
  Expect(unit_result.has_value(),
         "Slk command should split unit.ini into the unit slk set");
  for (const fs::path& stale_slk : {
           unit_workspace / "units" / "unitui.slk",
           unit_workspace / "units" / "unitdata.slk",
           unit_workspace / "units" / "unitbalance.slk",
           unit_workspace / "units" / "unitabilities.slk",
           unit_workspace / "units" / "unitweapons.slk"}) {
    Expect(!fs::exists(stale_slk),
           "Unit slk cleanup should remove stale slk inputs before packing");
  }

  auto unit_unpack_result =
      w3x_toolkit::parser::w3x::UnpackMapArchive(unit_map, unit_archive);
  Expect(unit_unpack_result.has_value(),
         "Unit slk archive should unpack successfully");
  ExpectFileExists(unit_archive / "units" / "unitui.slk");
  ExpectFileExists(unit_archive / "units" / "unitdata.slk");
  ExpectFileExists(unit_archive / "units" / "unitbalance.slk");
  ExpectFileExists(unit_archive / "units" / "unitabilities.slk");
  ExpectFileExists(unit_archive / "units" / "unitweapons.slk");
  ExpectFileExists(unit_archive / "war3map.w3u");

  auto unit_ui = w3x_toolkit::parser::slk::ParseSlk(
      ReadText(unit_archive / "units" / "unitui.slk"));
  Expect(unit_ui.has_value(), "Generated unitui.slk should be parseable");
  const int unit_ui_id_col = FindSlkColumn(*unit_ui, "unitUIID");
  const int unit_ui_name_col = FindSlkColumn(*unit_ui, "name");
  Expect(unit_ui_id_col >= 0 && unit_ui_name_col >= 0,
         "Generated unitui.slk should expose unitUIID/name");
  Expect(unit_ui->GetCell(1, unit_ui_id_col) == "u000",
         "Generated unitui.slk should preserve unit alias");
  Expect(unit_ui->GetCell(1, unit_ui_name_col) == "SplitUnit",
         "Generated unitui.slk should preserve name");

  auto unit_data = w3x_toolkit::parser::slk::ParseSlk(
      ReadText(unit_archive / "units" / "unitdata.slk"));
  Expect(unit_data.has_value(), "Generated unitdata.slk should be parseable");
  const int unit_data_race_col = FindSlkColumn(*unit_data, "race");
  const int unit_data_move_col = FindSlkColumn(*unit_data, "movetp");
  Expect(unit_data_race_col >= 0 && unit_data_move_col >= 0,
         "Generated unitdata.slk should expose race/movetp");
  Expect(unit_data->GetCell(1, unit_data_race_col) == "human",
         "Generated unitdata.slk should preserve unit race");
  Expect(unit_data->GetCell(1, unit_data_move_col) == "foot",
         "Generated unitdata.slk should preserve movement type");

  auto unit_balance = w3x_toolkit::parser::slk::ParseSlk(
      ReadText(unit_archive / "units" / "unitbalance.slk"));
  Expect(unit_balance.has_value(),
         "Generated unitbalance.slk should be parseable");
  const int unit_balance_hp_col = FindSlkColumn(*unit_balance, "HP");
  Expect(unit_balance_hp_col >= 0,
         "Generated unitbalance.slk should expose HP");
  Expect(unit_balance->GetCell(1, unit_balance_hp_col) == "420",
         "Generated unitbalance.slk should preserve HP");

  auto unit_abilities = w3x_toolkit::parser::slk::ParseSlk(
      ReadText(unit_archive / "units" / "unitabilities.slk"));
  Expect(unit_abilities.has_value(),
         "Generated unitabilities.slk should be parseable");
  const int unit_abilities_list_col = FindSlkColumn(*unit_abilities, "abilList");
  Expect(unit_abilities_list_col >= 0,
         "Generated unitabilities.slk should expose abilList");
  Expect(unit_abilities->GetCell(1, unit_abilities_list_col) == "Adef,Aatk",
         "Generated unitabilities.slk should preserve abilList");

  auto unit_weapons = w3x_toolkit::parser::slk::ParseSlk(
      ReadText(unit_archive / "units" / "unitweapons.slk"));
  Expect(unit_weapons.has_value(),
         "Generated unitweapons.slk should be parseable");
  const int unit_weapons_range_col = FindSlkColumn(*unit_weapons, "rangeN1");
  const int unit_weapons_targets_col = FindSlkColumn(*unit_weapons, "targs1");
  Expect(unit_weapons_range_col >= 0 && unit_weapons_targets_col >= 0,
         "Generated unitweapons.slk should expose rangeN1/targs1");
  Expect(unit_weapons->GetCell(1, unit_weapons_range_col) == "100",
         "Generated unitweapons.slk should preserve primary range");
  Expect(unit_weapons->GetCell(1, unit_weapons_targets_col) == "ground,air",
         "Generated unitweapons.slk should preserve primary targets");

  auto unit_obj = w3x_toolkit::parser::w3u::ParseW3u(
      ReadBinary(unit_archive / "war3map.w3u"));
  Expect(unit_obj.has_value(),
         "Generated war3map.w3u should remain parseable alongside split unit slk");
  Expect(unit_obj->custom_objects.size() == 1,
         "Generated war3map.w3u should keep the custom unit sidecar");
  Expect(unit_obj->custom_objects[0].original_id == "hfoo",
         "Generated war3map.w3u should preserve the custom unit parent");
  Expect(unit_obj->custom_objects[0].custom_id == "u000",
         "Generated war3map.w3u should preserve the custom unit id");

  const fs::path txt_workspace = temp.path() / "slk_txt_generated";
  const fs::path txt_map = temp.path() / "slk_txt_generated.w3x";
  const fs::path txt_archive = temp.path() / "slk_txt_generated_raw";
  fs::create_directories(txt_workspace / "units");
  fs::create_directories(txt_workspace / "doodads");
  WriteBinary(txt_workspace / "war3map.w3i", BuildMinimalW3i());
  WriteText(txt_workspace / "txt.ini",
            "[A000]\n"
            "skintype={\"ability\"}\n"
            "skinnableid={\"A000\"}\n"
            "Name={\"Generated Txt Ability\"}\n"
            "Ubertip={\"Line1|nLine2\"}\n"
            "Tip={\"Unsafe, \\\"Quoted\\\" Text\"}\n"
            "\n"
            "[H000]\n"
            "skintype={\"unit\"}\n"
            "skinnableid={\"H000\"}\n"
            "Name={\"Generated Txt Unit\"}\n"
            "\n"
            "[I000]\n"
            "skintype={\"item\"}\n"
            "skinnableid={\"I000\"}\n"
            "Name={\"Generated Txt Item\"}\n"
            "\n"
            "[R000]\n"
            "skintype={\"upgrade\"}\n"
            "skinnableid={\"R000\"}\n"
            "Name={\"Generated Txt Upgrade\"}\n"
            "\n"
            "[B000]\n"
            "skintype={\"buff\"}\n"
            "skinnableid={\"B000\"}\n"
            "Name={\"Generated Txt Buff\"}\n"
            "\n"
            "[D000]\n"
            "skintype={\"doodad\"}\n"
            "skinnableid={\"D000\"}\n"
            "Name={\"Generated Txt Doodad\"}\n"
            "\n"
            "[T000]\n"
            "skintype={\"destructable\"}\n"
            "skinnableid={\"T000\"}\n"
            "Name={\"Generated Txt Destructable\"}\n"
            "\n"
            "[X000]\n"
            "skintype={\"effect\"}\n"
            "Name={\"Generated Txt Extra\"}\n");
  WriteText(txt_workspace / "units" / "campaignabilitystrings.txt",
            "stale ability txt");
  WriteText(txt_workspace / "doodads" / "doodadskins.txt",
            "stale doodad txt");
  WriteText(txt_workspace / "war3mapextra.txt",
            "[EXTRA]\nName=Keep Extra Txt\n");
  WriteText(txt_workspace / "war3mapskin.txt",
            "[SKIN]\nName=Keep Skin Txt\n");

  auto txt_result = slk.Execute({txt_workspace.string(), txt_map.string()});
  Expect(txt_result.has_value(),
         "Slk command should generate mapped txt sidecars from txt.ini");
  Expect(!fs::exists(txt_workspace / "units" / "campaignabilitystrings.txt"),
         "Txt cleanup should remove stale generated txt inputs before packing");
  Expect(!fs::exists(txt_workspace / "doodads" / "doodadskins.txt"),
         "Txt cleanup should remove stale doodad txt inputs before packing");

  auto txt_unpack_result =
      w3x_toolkit::parser::w3x::UnpackMapArchive(txt_map, txt_archive);
  Expect(txt_unpack_result.has_value(),
         "Generated txt archive should unpack successfully");
  ExpectFileExists(txt_archive / "units" / "campaignabilitystrings.txt");
  ExpectFileExists(txt_archive / "units" / "commonabilitystrings.txt");
  ExpectFileExists(txt_archive / "units" / "campaignunitstrings.txt");
  ExpectFileExists(txt_archive / "units" / "itemstrings.txt");
  ExpectFileExists(txt_archive / "units" / "campaignupgradestrings.txt");
  ExpectFileExists(txt_archive / "units" / "itemabilitystrings.txt");
  ExpectFileExists(txt_archive / "units" / "orcunitstrings.txt");
  ExpectFileExists(txt_archive / "doodads" / "doodadskins.txt");
  ExpectFileExists(txt_archive / "war3mapextra.txt");
  ExpectFileExists(txt_archive / "war3mapskin.txt");
  ExpectFileExists(txt_archive / "war3map.wts");

  const std::string ability_txt =
      ReadText(txt_archive / "units" / "campaignabilitystrings.txt");
  Expect(ability_txt.find("[A000]") != std::string::npos,
         "Generated ability txt should keep the routed section id");
  Expect(ability_txt.find("Name=Generated Txt Ability") != std::string::npos,
         "Generated ability txt should preserve ability text");
  Expect(ability_txt.find("Ubertip=Line1|nLine2") != std::string::npos,
         "Generated ability txt should preserve multiline markers");
  Expect(ability_txt.find("Tip=TRIGSTR_000") != std::string::npos,
         "Generated ability txt should externalize unsafe txt strings to WTS");

  const std::string buff_txt =
      ReadText(txt_archive / "units" / "commonabilitystrings.txt");
  Expect(buff_txt.find("[B000]") != std::string::npos,
         "Generated buff txt should route to commonabilitystrings");
  Expect(buff_txt.find("Name=Generated Txt Buff") != std::string::npos,
         "Generated buff txt should preserve buff text");

  const std::string unit_txt =
      ReadText(txt_archive / "units" / "campaignunitstrings.txt");
  Expect(unit_txt.find("[H000]") != std::string::npos,
         "Generated unit txt should route to campaignunitstrings");
  Expect(unit_txt.find("Name=Generated Txt Unit") != std::string::npos,
         "Generated unit txt should preserve unit text");

  const std::string item_txt =
      ReadText(txt_archive / "units" / "itemstrings.txt");
  Expect(item_txt.find("[I000]") != std::string::npos,
         "Generated item txt should route to itemstrings");
  Expect(item_txt.find("Name=Generated Txt Item") != std::string::npos,
         "Generated item txt should preserve item text");

  const std::string upgrade_txt =
      ReadText(txt_archive / "units" / "campaignupgradestrings.txt");
  Expect(upgrade_txt.find("[R000]") != std::string::npos,
         "Generated upgrade txt should route to campaignupgradestrings");
  Expect(upgrade_txt.find("Name=Generated Txt Upgrade") != std::string::npos,
         "Generated upgrade txt should preserve upgrade text");

  const std::string destructable_txt =
      ReadText(txt_archive / "units" / "orcunitstrings.txt");
  Expect(destructable_txt.find("[T000]") != std::string::npos,
         "Generated destructable txt should route to orcunitstrings");
  Expect(destructable_txt.find("Name=Generated Txt Destructable") !=
             std::string::npos,
         "Generated destructable txt should preserve destructable text");

  const std::string doodad_txt =
      ReadText(txt_archive / "doodads" / "doodadskins.txt");
  Expect(doodad_txt.find("[D000]") != std::string::npos,
         "Generated doodad txt should route to doodadskins");
  Expect(doodad_txt.find("Name=Generated Txt Doodad") != std::string::npos,
         "Generated doodad txt should preserve doodad text");

  const std::string extra_txt =
      ReadText(txt_archive / "units" / "itemabilitystrings.txt");
  Expect(extra_txt.find("[X000]") != std::string::npos,
         "Generated fallback txt should route unknown skin types to itemabilitystrings");
  Expect(extra_txt.find("Name=Generated Txt Extra") != std::string::npos,
         "Generated fallback txt should preserve extra text");

  Expect(ReadText(txt_archive / "war3mapextra.txt")
                 .find("Keep Extra Txt") != std::string::npos,
         "war3mapextra.txt should still pass through during txt generation");
  Expect(ReadText(txt_archive / "war3mapskin.txt")
                 .find("Keep Skin Txt") != std::string::npos,
         "war3mapskin.txt should still pass through during txt generation");
  const std::string rebuilt_wts = ReadText(txt_archive / "war3map.wts");
  Expect(rebuilt_wts.find("STRING 0") != std::string::npos,
         "Generated WTS should be rebuilt with canonical STRING records");
  Expect(rebuilt_wts.find("Unsafe, \"Quoted\" Text") != std::string::npos,
         "Generated WTS should contain externalized txt strings");
}

void TestSlkContentCleanupPasses() {
  TemporaryDirectory temp;
  const fs::path cleanup_workspace = temp.path() / "slk_cleanup";
  const fs::path cleanup_map = temp.path() / "slk_cleanup.w3x";
  const fs::path cleanup_raw = temp.path() / "slk_cleanup_raw";
  fs::create_directories(cleanup_workspace);

  WriteBinary(cleanup_workspace / "war3map.w3i", BuildMinimalW3i());
  WriteBinary(cleanup_workspace / "war3map.doo",
              BuildMinimalDoo({"BT01"}, {"DO01"}));
  WriteText(
      cleanup_workspace / "war3map.j",
      std::string("function main takes nothing returns nothing\n"
                  "    local integer keepUnit = ") +
          std::to_string(RawcodeBigEndianValue("u100")) + "\n"
      "    local integer keepItem = " + RawcodeHexLiteral("I001") + "\n"
      "    local integer keepMarket = 'nmrk'\n"
      "    call ChooseRandomCreepBJ()\n"
      "    call ChooseRandomNPBuildingBJ()\n"
      "    call InitNeutralBuildings()\n"
      "endfunction\n");
  WriteText(cleanup_workspace / "ability.ini",
            "[AAns]\n"
            "name=\"收取黄金和木材\"\n"
            "\n"
            "[A001]\n"
            "_parent=ANcl\n"
            "name=KeepAbility\n"
            "buffid={\"B001\",\"\",\"\",\"\"}\n"
            "researchubertip=\"Research <R001,base1>\"\n"
            "ubertip={\"Min <u100,mindmg1> Max <u100,maxdmg1> HP <u100,realhp> Buff <A001,dataa1,%>%\"}\n"
            "dataa={0.15,0,0,0}\n"
            "\n"
            "[A002]\n"
            "_parent=ANcl\n"
            "name=UnusedAbility\n"
            "\n"
            "[A003]\n"
            "_parent=ANcl\n"
            "name=KeepReforgeAbility\n"
            "buttonpos_1=1\n"
            "\"buttonpos_1:hd\"=1\n"
            "\"buttonpos_1:sd\"=1\n");
  WriteText(cleanup_workspace / "buff.ini",
            "[B001]\n"
            "_parent=BPSE\n"
            "name=KeepBuff\n"
            "\n"
            "[B002]\n"
            "_parent=BPSE\n"
            "name=UnusedBuff\n");
  WriteText(cleanup_workspace / "upgrade.ini",
            "[R001]\n"
            "_parent=Recb\n"
            "name={\"Keep Upgrade\",\"Keep Upgrade\",\"Keep Upgrade\"}\n"
            "base1=7.0\n"
            "requires={\"\",\"\"}\n"
            "requiresamount={\"\",\"\"}\n"
            "ubertip={\"Upgrade <A001,dataa1,%>%\"}\n"
            "\n"
            "[R002]\n"
            "_parent=Recb\n"
            "name={\"Unused Upgrade\"}\n");
  WriteText(cleanup_workspace / "unit.ini",
            "[u100]\n"
            "_parent=hfoo\n"
            "name=CleanupUnit\n"
            "abillist=A001,A003\n"
            "researches=R001\n"
            "dmgplus1=7\n"
            "dice1=2\n"
            "sides1=4\n"
            "hp=555\n"
            "\n"
            "[u101]\n"
            "_parent=hfoo\n"
            "name=RandomCreepUnit\n"
            "race=creeps\n"
            "tilesets=A\n"
            "isbldg=0\n"
            "special=0\n"
            "\n"
            "[u102]\n"
            "_parent=hfoo\n"
            "name=RandomCreepBuilding\n"
            "race=creeps\n"
            "tilesets=A\n"
            "isbldg=1\n"
            "nbrandom=1\n"
            "special=0\n"
            "\n"
            "[u103]\n"
            "_parent=hfoo\n"
            "name=UnusedUnit\n");
  WriteText(cleanup_workspace / "item.ini",
            "[I001]\n"
            "_parent=amrc\n"
            "name=CleanupItem\n"
            "description=\"Deals <A001,dataa1,%> damage.\"\n"
            "ubertip=\"HP <u100,realhp>\"\n"
            "\n"
            "[I002]\n"
            "_parent=amrc\n"
            "name=MarketplaceItem\n"
            "pickrandom=1\n"
            "sellable=1\n"
            "\n"
            "[I003]\n"
            "_parent=amrc\n"
            "name=UnusedRandomItem\n"
            "pickrandom=1\n"
            "sellable=0\n"
            "\n"
            "[I004]\n"
            "_parent=amrc\n"
            "name=UnusedItem\n");
  WriteText(cleanup_workspace / "destructable.ini",
            "[BT01]\n"
            "_parent=LTlt\n"
            "name=PlacedDestructable\n"
            "\n"
            "[BT02]\n"
            "_parent=LTlt\n"
            "name=UnusedDestructable\n");
  WriteText(cleanup_workspace / "doodad.ini",
            "[DO01]\n"
            "_parent=ACt0\n"
            "name=PlacedDoodad\n"
            "numvar=2\n"
            "\n"
            "[DO02]\n"
            "_parent=ACt0\n"
            "name=UnusedDoodad\n"
            "numvar=2\n");
  WriteText(cleanup_workspace / "txt.ini",
            "[aami]\n"
            "name={\"物品 - 反魔法护罩\"}\n"
            "\n"
            "[A001]\n"
            "skintype={\"ability\"}\n"
            "skinnableid={\"A001\"}\n"
            "Name={\"Keep Ability Txt\"}\n"
            "CustomText={\"Keep Ability Extra\"}\n"
            "\n"
            "[A002]\n"
            "skintype={\"ability\"}\n"
            "skinnableid={\"A002\"}\n"
            "Name={\"Unused Ability Txt\"}\n"
            "CustomText={\"Unused Ability Extra\"}\n"
            "\n"
            "[I002]\n"
            "skintype={\"item\"}\n"
            "skinnableid={\"I002\"}\n"
            "Name={\"Marketplace Item Txt\"}\n"
            "CustomText={\"Marketplace Item Extra\"}\n"
            "\n"
            "[I003]\n"
            "skintype={\"item\"}\n"
            "skinnableid={\"I003\"}\n"
            "Name={\"Unused Random Item Txt\"}\n");

  w3x_toolkit::parser::w3x::PackOptions options;
  options.profile = w3x_toolkit::parser::w3x::PackProfile::kSlk;
  options.read_slk = true;
  options.slk_doodad = true;
  options.remove_unused_objects = true;
  options.remove_we_only = false;
  options.remove_same = true;
  options.computed_text = true;

  auto pack_result = w3x_toolkit::parser::w3x::PackMapDirectory(
      cleanup_workspace, cleanup_map, options);
  Expect(pack_result.has_value(),
         "Direct slk packing should apply content cleanup passes: " +
             (pack_result.has_value() ? std::string()
                                      : pack_result.error().message()));

  const std::string cleaned_ability = ReadText(cleanup_workspace / "ability.ini");
  Expect(cleaned_ability.find("[AAns]") == std::string::npos,
         "remove_same should drop ability sections identical to defaults");
  Expect(cleaned_ability.find("[A002]") == std::string::npos,
         "remove_unuse_object should drop unreferenced custom abilities");
  Expect(cleaned_ability.find("\"buttonpos_1:hd\"") == std::string::npos,
         "remove_same should drop reforge hd fields that match the base field");
  Expect(cleaned_ability.find("\"buttonpos_1:sd\"") == std::string::npos,
         "remove_same should drop reforge sd fields that match the base field");
  Expect(cleaned_ability.find("Research 7") != std::string::npos,
         "computed_text should resolve ability researchubertip placeholders");
  Expect(cleaned_ability.find("Min 9 Max 15 HP 555 Buff 15") != std::string::npos,
         "computed_text should resolve ability ubertip formulas");

  const std::string cleaned_item = ReadText(cleanup_workspace / "item.ini");
  Expect(cleaned_item.find("Deals 15 damage.") != std::string::npos,
         "computed_text should resolve item description placeholders");
  Expect(cleaned_item.find("HP 555") != std::string::npos,
         "computed_text should resolve item ubertip placeholders");
  Expect(cleaned_item.find("[I002]") != std::string::npos,
         "remove_unuse_object should keep marketplace random items");
  Expect(cleaned_item.find("[I003]") == std::string::npos,
         "remove_unuse_object should still drop random items excluded by marketplace rules");
  Expect(cleaned_item.find("[I004]") == std::string::npos,
         "remove_unuse_object should drop unreferenced custom items: " +
             cleaned_item);

  const std::string cleaned_upgrade = ReadText(cleanup_workspace / "upgrade.ini");
  Expect(cleaned_upgrade.find("[R002]") == std::string::npos,
         "remove_unuse_object should drop unreferenced custom upgrades");
  Expect(cleaned_upgrade.find("Upgrade 15") != std::string::npos,
         "computed_text should resolve upgrade ubertip placeholders");
  Expect(cleaned_upgrade.find(
             "name={\"Keep Upgrade\",\"Keep Upgrade\",\"Keep Upgrade\"}") ==
             std::string::npos,
         "remove_same should trim redundant repeated txt arrays: " +
             cleaned_upgrade);
  Expect(cleaned_upgrade.find("name={Keep Upgrade}") != std::string::npos ||
             cleaned_upgrade.find("name={\"Keep Upgrade\"}") !=
                 std::string::npos,
         "remove_same should collapse repeated txt arrays to the minimal prefix: " +
             cleaned_upgrade);
  Expect(cleaned_upgrade.find("requires={\"\",\"\"}") == std::string::npos,
         "remove_same should drop appendindex arrays that only restate defaults");
  Expect(cleaned_upgrade.find("requiresamount={\"\",\"\"}") ==
             std::string::npos,
         "remove_same should drop appendindex companion arrays that only restate defaults");

  const std::string cleaned_buff = ReadText(cleanup_workspace / "buff.ini");
  Expect(cleaned_buff.find("[B002]") == std::string::npos,
         "remove_unuse_object should drop unreferenced custom buffs");

  const std::string cleaned_unit = ReadText(cleanup_workspace / "unit.ini");
  Expect(cleaned_unit.find("[u100]") != std::string::npos,
         "remove_unuse_object should keep units referenced by integer rawcodes");
  Expect(cleaned_unit.find("[u101]") != std::string::npos,
         "remove_unuse_object should keep random creep units required by BJ flags");
  Expect(cleaned_unit.find("[u102]") != std::string::npos,
         "remove_unuse_object should keep random creep buildings required by BJ flags");
  Expect(cleaned_unit.find("[u103]") == std::string::npos,
         "remove_unuse_object should drop unreferenced custom units");

  const std::string cleaned_destructable =
      ReadText(cleanup_workspace / "destructable.ini");
  Expect(cleaned_destructable.find("[BT01]") != std::string::npos,
         "remove_unuse_object should keep destructables placed in war3map.doo");
  Expect(cleaned_destructable.find("[BT02]") == std::string::npos,
         "remove_unuse_object should drop unreferenced custom destructables");

  const std::string cleaned_doodad = ReadText(cleanup_workspace / "doodad.ini");
  Expect(cleaned_doodad.find("[DO01]") != std::string::npos,
         "remove_unuse_object should keep doodads placed in war3map.doo");
  Expect(cleaned_doodad.find("[DO02]") == std::string::npos,
         "remove_unuse_object should drop unreferenced custom doodads");

  const std::string cleaned_txt = ReadText(cleanup_workspace / "txt.ini");
  Expect(cleaned_txt.find("[aami]") == std::string::npos,
         "remove_same should drop txt sections identical to defaults");
  Expect(cleaned_txt.find("[A002]") == std::string::npos,
         "remove_unuse_object should drop txt sections tied to removed objects");
  Expect(cleaned_txt.find("[A001]") != std::string::npos,
         "remove_unuse_object should retain txt sections tied to kept objects");
  Expect(cleaned_txt.find("Name={\"Keep Ability Txt\"}") == std::string::npos,
         "remove_same should clean txt keys that belong to object metadata");
  Expect(cleaned_txt.find("CustomText={\"Keep Ability Extra\"}") !=
             std::string::npos,
         "txt cleanup should retain non-object text keys");
  Expect(cleaned_txt.find("[I002]") != std::string::npos,
         "remove_unuse_object should retain txt sections tied to kept marketplace items");
  Expect(cleaned_txt.find("[I003]") == std::string::npos,
         "remove_unuse_object should drop txt sections tied to removed items");

  auto unpack_result =
      w3x_toolkit::parser::w3x::UnpackMapArchive(cleanup_map, cleanup_raw);
  Expect(unpack_result.has_value(),
         "Cleanup slk archive should unpack successfully");
  ExpectFileExists(cleanup_raw / "units" / "abilitydata.slk");
  ExpectFileExists(cleanup_raw / "units" / "abilitybuffdata.slk");
  ExpectFileExists(cleanup_raw / "units" / "itemdata.slk");
  ExpectFileExists(cleanup_raw / "units" / "upgradedata.slk");
  ExpectFileExists(cleanup_raw / "units" / "destructabledata.slk");
  ExpectFileExists(cleanup_raw / "doodads" / "doodads.slk");
  ExpectFileExists(cleanup_raw / "units" / "unitdata.slk");

  const std::string ability_slk =
      ReadText(cleanup_raw / "units" / "abilitydata.slk");
  Expect(ability_slk.find("A001") != std::string::npos,
         "Generated abilitydata.slk should keep referenced abilities");
  Expect(ability_slk.find("A002") == std::string::npos,
         "Generated abilitydata.slk should omit removed abilities");

  const std::string buff_slk =
      ReadText(cleanup_raw / "units" / "abilitybuffdata.slk");
  Expect(buff_slk.find("B001") != std::string::npos,
         "Generated abilitybuffdata.slk should keep referenced buffs");
  Expect(buff_slk.find("B002") == std::string::npos,
         "Generated abilitybuffdata.slk should omit removed buffs");

  const std::string upgrade_slk =
      ReadText(cleanup_raw / "units" / "upgradedata.slk");
  Expect(upgrade_slk.find("R001") != std::string::npos,
         "Generated upgradedata.slk should keep referenced upgrades");
  Expect(upgrade_slk.find("R002") == std::string::npos,
         "Generated upgradedata.slk should omit removed upgrades");

  const std::string item_slk = ReadText(cleanup_raw / "units" / "itemdata.slk");
  Expect(item_slk.find("I001") != std::string::npos,
         "Generated itemdata.slk should keep items referenced by JASS hex rawcodes");
  Expect(item_slk.find("I002") != std::string::npos,
         "Generated itemdata.slk should keep marketplace random items");
  Expect(item_slk.find("I003") == std::string::npos,
         "Generated itemdata.slk should omit removed random items");
  Expect(item_slk.find("I004") == std::string::npos,
         "Generated itemdata.slk should omit removed unreferenced items");

  const std::string destructable_slk =
      ReadText(cleanup_raw / "units" / "destructabledata.slk");
  Expect(destructable_slk.find("BT01") != std::string::npos,
         "Generated destructabledata.slk should keep placed destructables");
  Expect(destructable_slk.find("BT02") == std::string::npos,
         "Generated destructabledata.slk should omit removed destructables");

  const std::string doodad_slk =
      ReadText(cleanup_raw / "doodads" / "doodads.slk");
  Expect(doodad_slk.find("DO01") != std::string::npos,
         "Generated doodads.slk should keep placed doodads");
  Expect(doodad_slk.find("DO02") == std::string::npos,
         "Generated doodads.slk should omit removed doodads");

  const std::string unit_slk = ReadText(cleanup_raw / "units" / "unitdata.slk");
  Expect(unit_slk.find("u100") != std::string::npos,
         "Generated unitdata.slk should keep units referenced by integer rawcodes");
  Expect(unit_slk.find("u101") != std::string::npos,
         "Generated unitdata.slk should keep random creep units");
  Expect(unit_slk.find("u102") != std::string::npos,
         "Generated unitdata.slk should keep random creep buildings");
  Expect(unit_slk.find("u103") == std::string::npos,
         "Generated unitdata.slk should omit removed unreferenced units");
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
    TestObjCommandAndArchiveFiltering();
    TestSlkCommandGenerationCleanupAndSidecars();
    TestSlkContentCleanupPasses();
    std::cout << "Smoke tests passed." << std::endl;
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Smoke tests failed: " << ex.what() << std::endl;
    return 1;
  }
}
