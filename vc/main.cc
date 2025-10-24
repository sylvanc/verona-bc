#include "lang.h"

#include <git2.h>
#include <trieste/driver.h>
#include <vbcc/bytecode.h>

int main(int argc, char** argv)
{
  using namespace vc;

  auto state = std::make_shared<Bytecode>();
  auto parse = vc::parser(state);
  auto struc = vc::structure(parse);

  Reader reader{
    "vc",
    {
      struc,
      sugar(),
      ident(),
      application(),
      operators(),
      anf(),
      reify(),
      vbcc::assignids(state),
      vbcc::validids(state),
      vbcc::liveness(state),
    },
    parse};

  struct Options : public trieste::Options
  {
    std::filesystem::path bytecode_file;
    bool strip = false;
    bool reproducible = false;

    void configure(CLI::App& cli) override
    {
      cli.add_option(
        "-b,--bytecode", bytecode_file, "Output bytecode to this file.");
      cli.add_flag(
        "-s,--strip", strip, "Strip debug information from the bytecode.");
      cli.add_flag(
        "-r,--reproducible", reproducible, "Make the build reproducible.");
    }
  };

  Options opts;
  Driver d(reader, &opts);

  git_libgit2_init();
  auto r = d.run(argc, argv);
  git_libgit2_shutdown();

  if (r != 0)
    return r;

  if (state->error)
    return -1;

  state->gen(opts.bytecode_file, opts.strip, opts.reproducible);
  return 0;
}
