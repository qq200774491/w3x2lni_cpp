// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include <cstdlib>
#include <exception>
#include <iostream>

#include "cli/cli_app.h"
#include "core/logger/logger.h"
#include "core/platform/platform.h"

int main(int argc, char **argv)
{
  // Configure the console for UTF-8 output (important on Windows).
  w3x_toolkit::core::Platform::SetupConsoleEncoding();

  // Initialize the logger early so all subsystems can use it.
  auto &logger = w3x_toolkit::core::Logger::Instance();
  logger.SetLevel(w3x_toolkit::core::LogLevel::kInfo);

  try
  {
    w3x_toolkit::cli::CliApp app;
    int exit_code = app.Run(argc, argv);

    // Flush any remaining log messages before exit.
    logger.Flush();
    return exit_code;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Fatal error: " << ex.what() << std::endl;
    logger.Critical("Unhandled exception: {}", ex.what());
    logger.Flush();
    return EXIT_FAILURE;
  }
  catch (...)
  {
    std::cerr << "Fatal error: unknown exception" << std::endl;
    logger.Critical("Unhandled unknown exception");
    logger.Flush();
    return EXIT_FAILURE;
  }
}
