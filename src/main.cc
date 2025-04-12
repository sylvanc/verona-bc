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

  return 0;
}
