#include "lang.h"

#include <git2.h>
#include <trieste/driver.h>
#include <virc/compile.h>
#include <virc/vbc/emitter.h>

#if defined(VERONA_ENABLE_LLVM_BACKEND)
#  include <virc/llvm/emit.h>
#endif

int main(int argc, char** argv)
{
  using namespace vc;

  enum class OutputFormat
  {
    VBC,
    LLVMIR,
  };

  auto state = std::make_shared<virc::Compilation>();
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
    std::filesystem::path output_file;
    std::filesystem::path bytecode_file;
    std::string output_format_name = "vbc";
    OutputFormat output_format = OutputFormat::VBC;
    bool strip = false;
    bool build = false;

    void configure(CLI::App& cli) override
    {
      cli
        .add_option(
          "--emit",
          output_format_name,
          "Output format: vbc or llvm-ir. Defaults to vbc.")
        ->check(CLI::IsMember({"vbc", "llvm-ir"}));
      cli.add_option(
        "--output-file", output_file, "Output file for the selected format.");
      cli.add_option(
        "-b,--bytecode", bytecode_file, "Output VBC to this file.");
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

        output_format = output_format_name == "vbc" ? OutputFormat::VBC :
                                                      OutputFormat::LLVMIR;

#if !defined(VERONA_ENABLE_LLVM_BACKEND)
        if (output_format == OutputFormat::LLVMIR)
        {
          throw CLI::ValidationError(
            "--emit", "vc was built without LLVM backend support");
        }
#endif

        auto bytecode_option = cli.get_option_no_throw("--bytecode");
        auto output_option = cli.get_option_no_throw("--output-file");
        if (
          bytecode_option && bytecode_option->count() > 0 && output_option &&
          output_option->count() > 0)
        {
          throw CLI::ValidationError(
            "--bytecode", "cannot be combined with --output-file");
        }

        if (
          output_format == OutputFormat::LLVMIR && bytecode_option &&
          bytecode_option->count() > 0)
        {
          throw CLI::ValidationError(
            "--bytecode", "cannot be used with --emit llvm-ir");
        }

        if (!bytecode_file.empty())
          output_file = bytecode_file;

        std::string extension =
          output_format == OutputFormat::VBC ? ".vbc" : ".ll";

        if (!path.empty() && output_file.empty())
        {
          output_file = path.stem();
          output_file += extension;
        }

        if (!output_file.empty() && output_file.extension() != extension)
        {
          throw CLI::ValidationError(
            "--output-file",
            "output format requires the " + extension + " extension");
        }

        if (strip && output_format != OutputFormat::VBC)
        {
          throw CLI::ValidationError(
            "--strip", "is only supported for VBC output");
        }

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

  switch (opts.output_format)
  {
    case OutputFormat::VBC:
      virc::vbc::emit(*state, opts.output_file, opts.strip);
      break;

    case OutputFormat::LLVMIR:
#if defined(VERONA_ENABLE_LLVM_BACKEND)
      if (!virc::llvm::emit(*state, opts.output_file))
        return -1;
#else
      return -1;
#endif
      break;
  }

  return 0;
}
