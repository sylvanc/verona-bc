// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include "logging.h"
#include "program.h"
#include "types.h"

#include <CLI/CLI.hpp>
#include <uv.h>

int main(int argc, char** argv)
{
  using namespace vbci;

  CLI::App app;
  app.allow_extras();

  std::filesystem::path file;
  app.add_option("path", file, "File to execute.")->required();

  size_t num_threads = uv_available_parallelism();
  app.add_option("-t,--threads", num_threads, "Scheduler threads.");

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
    argv = uv_setup_args(argc, argv);
    app.parse(argc, argv);
  }
  catch (const CLI::ParseError& e)
  {
    return app.exit(e);
  }

  LOG(Info) << "Running with " << num_threads << " threads";
  return Program::get().run(file, num_threads, app.remaining());
}
