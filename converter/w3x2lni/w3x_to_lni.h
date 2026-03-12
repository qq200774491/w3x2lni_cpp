#ifndef W3X_TOOLKIT_CONVERTER_W3X_TO_LNI_H_
#define W3X_TOOLKIT_CONVERTER_W3X_TO_LNI_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::converter {

// Specifies which components of a W3X map to extract into LNI format.
struct W3xToLniOptions {
  // Extract map files (terrain, pathing, etc.) into the map/ subdirectory.
  bool extract_map_files = true;

  // Extract SLK/object data into table/ as .ini files.
  bool extract_table_data = true;

  // Extract trigger scripts into trigger/.
  bool extract_triggers = true;

  // Generate w3x2lni.ini configuration file.
  bool generate_config = true;

  // Remove unused objects from object data during conversion.
  bool remove_unused_objects = false;

  // Inline WTS (war3map.wts) strings directly into data files.
  bool inline_wts_strings = true;
};

// Progress information passed to the callback during conversion.
struct ConversionProgress {
  // Current phase name (e.g. "Loading archive", "Writing table data").
  std::string phase;

  // Number of items processed so far in the current phase.
  std::size_t items_done = 0;

  // Total number of items in the current phase (0 if unknown).
  std::size_t items_total = 0;

  // Overall progress as a fraction in [0.0, 1.0].
  double overall_fraction = 0.0;
};

// Signature for the progress callback.
// Return false from the callback to request cancellation.
using ProgressCallback = std::function<bool(const ConversionProgress&)>;

// Converts a Warcraft III map archive (.w3x) to a LNI (Lua Native INI)
// directory structure suitable for version control and manual editing.
//
// The output directory is organized as:
//   <output>/
//     map/          -- raw map files (terrain, pathing, shadows, etc.)
//     table/        -- object data converted to .ini format
//     trigger/      -- trigger scripts (JASS or Lua)
//     w3x2lni.ini   -- converter configuration / metadata
//
// Usage:
//   W3xToLniConverter converter(input_w3x, output_dir);
//   converter.SetOptions(options);
//   converter.SetProgressCallback(callback);
//   auto result = converter.Convert();
//
class W3xToLniConverter {
 public:
  // Constructs a converter that will read from |input_path| (a .w3x file or
  // an already-extracted directory) and write the LNI output to
  // |output_path|.
  W3xToLniConverter(std::filesystem::path input_path,
                    std::filesystem::path output_path);

  ~W3xToLniConverter() = default;

  // Non-copyable, movable.
  W3xToLniConverter(const W3xToLniConverter&) = delete;
  W3xToLniConverter& operator=(const W3xToLniConverter&) = delete;
  W3xToLniConverter(W3xToLniConverter&&) noexcept = default;
  W3xToLniConverter& operator=(W3xToLniConverter&&) noexcept = default;

  // ---------------------------------------------------------------------------
  // Configuration
  // ---------------------------------------------------------------------------

  // Sets the conversion options.  Must be called before Convert().
  void SetOptions(const W3xToLniOptions& options);

  // Returns the current options.
  const W3xToLniOptions& GetOptions() const;

  // Installs a progress callback.  The callback is invoked periodically
  // during Convert().  Returning false from the callback cancels the
  // conversion.
  void SetProgressCallback(ProgressCallback callback);

  // ---------------------------------------------------------------------------
  // Conversion
  // ---------------------------------------------------------------------------

  // Performs the conversion.  Returns an error if the input cannot be read,
  // the output directory cannot be created, or a conversion step fails.
  core::Result<void> Convert();

 private:
  // Individual conversion phases.
  core::Result<void> PrepareOutputDirectory();
  core::Result<void> ExtractMapFiles();
  core::Result<void> ConvertTableData();
  core::Result<void> ExtractTriggers();
  core::Result<void> WriteConfig();

  // Reports progress and checks for cancellation.
  // Returns false if the caller should abort (user cancelled).
  bool ReportProgress(std::string_view phase, std::size_t done,
                      std::size_t total, double overall);

  std::filesystem::path input_path_;
  std::filesystem::path output_path_;
  W3xToLniOptions options_;
  ProgressCallback progress_callback_;
  bool cancelled_ = false;
};

}  // namespace w3x_toolkit::converter

#endif  // W3X_TOOLKIT_CONVERTER_W3X_TO_LNI_H_
