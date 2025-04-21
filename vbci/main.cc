// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include "program.h"
#include "types.h"

#include <CLI/CLI.hpp>
#include <verona.h>

int main(int argc, char** argv)
{
  using namespace vbci;

  CLI::App app;
  std::filesystem::path file;
  app.add_option("path", file, "File to execute.")->required();

  std::string log_level;
  app
    .add_option(
      "-l,--log_level",
      log_level,
      "Set Log Level to one of "
      "Trace, Debug, Info, "
      "Warning, Output, Error, "
      "None")
    ->check(logging::set_log_level_from_string);

  try
  {
    app.parse(argc, argv);
  }
  catch (const CLI::ParseError& e)
  {
    return app.exit(e);
  }

  auto program = Program::get();

  if (!program.load(file))
    return -1;

  if (!program.parse())
    return -1;

  return program.run();
}
