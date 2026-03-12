#ifndef W3X_TOOLKIT_PARSER_W3I_W3I_PARSER_H_
#define W3X_TOOLKIT_PARSER_W3I_W3I_PARSER_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::parser::w3i {

// Camera bounds: left, bottom, right, top, left-complement, right-complement,
// top-complement, bottom-complement.
struct CameraBounds {
  float bounds[8] = {};
  int32_t complements[4] = {};
};

// Map configuration flags.
struct MapFlags {
  bool disable_preview = false;
  bool custom_ally = false;
  bool melee_map = false;
  bool large_map = false;
  bool masked_area_show_terrain = false;
  bool fix_force_setting = false;
  bool custom_force = false;
  bool custom_techtree = false;
  bool custom_ability = false;
  bool custom_upgrade = false;
  bool map_menu_mark = false;
  bool show_wave_on_cliff = false;
  bool show_wave_on_rolling = false;
};

// Loading screen data.
struct LoadingScreen {
  int32_t id = 0;
  std::string path;
  std::string text;
  std::string title;
  std::string subtitle;
};

// Prologue screen data.
struct PrologueScreen {
  int32_t id = 0;
  std::string path;
  std::string text;
  std::string title;
  std::string subtitle;
};

// Fog settings.
struct FogSettings {
  int32_t type = 0;
  float start_z = 0.0f;
  float end_z = 0.0f;
  float density = 0.0f;
  uint8_t color[4] = {};  // RGBA
};

// Environment settings (TFT+).
struct EnvironmentSettings {
  std::string weather_id;   // 4-char ID
  std::string sound;
  char light = '\0';
  uint8_t water_color[4] = {};  // RGBA
};

// Player slot definition.
struct PlayerSlot {
  int32_t player_id = 0;
  int32_t type = 0;       // 1=human, 2=computer, 3=neutral, 4=rescuable
  int32_t race = 0;       // 1=human, 2=orc, 3=undead, 4=night_elf
  int32_t fix_start = 0;
  std::string name;
  float start_x = 0.0f;
  float start_y = 0.0f;
  uint32_t ally_low_flag = 0;
  uint32_t ally_high_flag = 0;
};

// Force definition.
struct Force {
  bool allied = false;
  bool allied_victory = false;
  bool share_vision = false;
  bool share_control = false;
  bool share_advanced = false;
  uint32_t player_mask = 0;
  std::string name;
};

// Upgrade availability override.
struct UpgradeAvailability {
  uint32_t player_mask = 0;
  std::string upgrade_id;  // 4-char rawcode
  int32_t level = 0;
  int32_t availability = 0;
};

// Tech availability override.
struct TechAvailability {
  uint32_t player_mask = 0;
  std::string tech_id;  // 4-char rawcode
};

// Complete war3map.w3i data structure.
struct W3iData {
  // Header
  int32_t file_version = 0;
  int32_t map_version = 0;
  int32_t editor_version = 0;
  std::array<int32_t, 4> game_version = {};  // major, minor, patch, build

  std::string map_name;
  std::string author;
  std::string description;
  std::string players_recommended;

  // Camera
  CameraBounds camera;

  // Map dimensions
  int32_t map_width = 0;
  int32_t map_height = 0;
  char main_ground = '\0';

  // Flags
  MapFlags flags;
  int32_t game_data_setting = 0;  // TFT+

  // Loading / Prologue
  LoadingScreen loading_screen;
  PrologueScreen prologue;

  // Fog / Environment (TFT+)
  FogSettings fog;
  EnvironmentSettings environment;

  // Script type (version >= 28): 0=JASS, 1=Lua
  int32_t script_type = 0;

  // Players and forces
  std::vector<PlayerSlot> players;
  std::vector<Force> forces;

  // Availability overrides
  std::vector<UpgradeAvailability> upgrades;
  std::vector<TechAvailability> techs;
};

// Parses a war3map.w3i binary buffer.
// Supports versions 18 (RoC), 25-31 (TFT / Reforged).
core::Result<W3iData> ParseW3i(const uint8_t* data, size_t size);

// Convenience overload accepting a byte vector.
core::Result<W3iData> ParseW3i(const std::vector<uint8_t>& data);

}  // namespace w3x_toolkit::parser::w3i

#endif  // W3X_TOOLKIT_PARSER_W3I_W3I_PARSER_H_
