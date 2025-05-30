#include "lang.h"

#include <trieste/driver.h>
#include <vbcc.h>
#include <vbcc/bytecode.h>

int main(int argc, char** argv)
{
  using namespace vc;

  // TODO: assignids, validids, liveness.
  auto state = std::make_shared<Bytecode>();
  Reader reader{"vc", {structure()}, parser()};

  struct Options : public trieste::Options
  {
    std::filesystem::path bytecode_file;
    bool strip = false;

    void configure(CLI::App& cli) override
    {
      cli.add_option(
        "-b,--bytecode", bytecode_file, "Output bytecode to this file.");
      cli.add_flag(
        "-s,--strip", strip, "Strip debug information from the bytecode.");
    }
  };

  Options opts;
  Driver d(reader, &opts);
  auto r = d.run(argc, argv);

  if (r != 0)
    return r;

  // if (state->error)
  //   return -1;

  // wf::push_back(wfIR);
  // state->gen(opts.bytecode_file, opts.strip);
  // wf::pop_front();
  return 0;
}
