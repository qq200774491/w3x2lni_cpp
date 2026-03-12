// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/lni_command.h"

#include "cli/commands/convert_command.h"

namespace w3x_toolkit::cli {

std::string LniCommand::Name() const { return "lni"; }

std::string LniCommand::Description() const {
  return "Convert a map to the LNI workspace layout";
}

std::string LniCommand::Usage() const {
  return "lni <input_map_dir|input_map.w3x|input_map.w3m> <output_dir> "
         "[--no-map-files] [--no-table-data] [--no-triggers] [--no-config]";
}

core::Result<void> LniCommand::Execute(const std::vector<std::string>& args) {
  ConvertCommand convert;
  return convert.Execute(args);
}

}  // namespace w3x_toolkit::cli
