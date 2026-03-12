#include "parser/w3x/object_cleanup.h"

#include <array>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace w3x_toolkit::parser::w3x {

namespace {

void RemoveIfExists(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

bool HasIniSource(const std::filesystem::path& input_dir,
                  std::string_view filename) {
  return std::filesystem::exists(input_dir / std::string(filename));
}

}  // namespace

core::Result<void> ApplyPackCleanupPasses(const std::filesystem::path& input_dir,
                                          const PackOptions& options) {
  if (options.profile != PackProfile::kSlk) {
    return {};
  }

  if (options.remove_we_only) {
    for (std::string_view relative_path :
         {"war3map.wtg", "war3map.wct", "war3map.imp", "war3map.w3s",
          "war3map.w3r", "war3map.w3c", "war3mapunits.doo"}) {
      RemoveIfExists(input_dir / std::string(relative_path));
    }
  }

  if (options.read_slk) {
    if (HasIniSource(input_dir, "ability.ini")) {
      RemoveIfExists(input_dir / "units" / "abilitydata.slk");
    }
    if (HasIniSource(input_dir, "buff.ini")) {
      RemoveIfExists(input_dir / "units" / "abilitybuffdata.slk");
    }
    if (HasIniSource(input_dir, "item.ini")) {
      RemoveIfExists(input_dir / "units" / "itemdata.slk");
    }
    if (HasIniSource(input_dir, "upgrade.ini")) {
      RemoveIfExists(input_dir / "units" / "upgradedata.slk");
    }
    if (options.slk_doodad && HasIniSource(input_dir, "destructable.ini")) {
      RemoveIfExists(input_dir / "units" / "destructabledata.slk");
    }
    if (options.slk_doodad && HasIniSource(input_dir, "doodad.ini")) {
      RemoveIfExists(input_dir / "doodads" / "doodads.slk");
    }
  }
  return {};
}

}  // namespace w3x_toolkit::parser::w3x
