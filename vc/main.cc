#include "lang.h"

#include <git2.h>
#include <trieste/driver.h>
#include <virc/compile.h>
#include <virc/vbc/emitter.h>

int main(int argc, char** argv)
{
  using namespace vc;

  auto state = std::make_shared<Compilation>();
  auto parse = vc::parser();
  auto struc = vc::structure(parse);

  std::vector<Pass> passes{
    struc,
    ident(),
    sugar(),
    functype(),
    dot(),
    application(),
    anf(),
    infer(),
    reify(),
  };
  auto virc_passes = virc::pipeline(state);
  passes.insert(passes.end(), virc_passes.begin(), virc_passes.end());

  Reader reader{"vc", passes, parse};

  struct Options : public trieste::Options
  {
    std::filesystem::path path;
    std::filesystem::path bytecode_file;
    bool strip = false;
    bool build = false;

    void configure(CLI::App& cli) override
    {
      cli.add_option(
        "-b,--bytecode", bytecode_file, "Output bytecode to this file.");
      cli.add_flag(
        "-s,--strip", strip, "Strip debug information from the bytecode.");

      cli.callback([this, &cli]() {
        path = cli.get_option("path")->as<std::filesystem::path>();

        if (!path.has_filename())
          path = path.parent_path();

        // Check is the path is a directory, if not this is an error.
        if (!std::filesystem::is_directory(path))
        {
          std::cerr << "Error: path is not a directory: " << path << std::endl;
          std::exit(1);
        }

        if (!path.empty() && bytecode_file.empty())
          bytecode_file = path.stem().replace_extension(".vbc");

        auto pass = cli.get_option_no_throw("--pass");

        if (
          !pass || pass->count() == 0 || pass->as<std::string>() == "optimize")
          build = true;
      });
    }
  };

  Options opts;
  Driver d(reader, &opts);

  git_libgit2_init();
  state->add_path(argv[0]);
  auto r = d.run(argc, argv);
  git_libgit2_shutdown();

  if (r != 0)
    return r;

  if (!opts.build)
    return 0;

  if (state->error)
    return -1;

  if (!opts.path.empty())
    state->add_path(opts.path);

  virc::vbc::emit(*state, opts.bytecode_file, opts.strip);
  return 0;
}
