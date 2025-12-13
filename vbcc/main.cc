#include "bytecode.h"
#include "lang.h"

#include <trieste/driver.h>

int main(int argc, char** argv)
{
  using namespace trieste;
  using namespace vbcc;

  auto state = std::make_shared<Bytecode>();
  Reader reader{
    "vbcc",
    {statements(),
     labels(),
     assignids(state),
     validids(state),
     liveness(state)},
    parser(state)};


  struct Options : public trieste::Options
  {
    std::filesystem::path bytecode_file;
    bool strip = false;
    bool reproducible = false;
    bool build = false;

    void configure(CLI::App& cli) override
    {
      cli.add_option(
        "-b,--bytecode", bytecode_file, "Output bytecode to this file.");
      cli.add_flag(
        "-s,--strip", strip, "Strip debug information from the bytecode.");
      cli.add_flag(
        "-r,--reproducible", reproducible, "Make the build reproducible.");
      cli.callback([this, &cli]()
      {
        build = cli.parsed();
      });
    }
  };

  Options opts;
  Driver d(reader, &opts);
  auto r = d.run(argc, argv);

  if (r != 0)
    return r;

  if (state->error)
    return -1;

  if (opts.build)
    state->gen(opts.bytecode_file, opts.strip, opts.reproducible);

  return 0;
}
