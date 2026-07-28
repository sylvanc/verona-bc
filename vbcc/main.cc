#include "bytecode.h"
#include "lang.h"

#include <trieste/driver.h>

#include <string>

int main(int argc, char** argv)
{
  using namespace trieste;
  using namespace vbcc;

  enum class OutputFormat
  {
    VBC,
    LLVMIR,
  };

  auto state = std::make_shared<Bytecode>();
  Reader reader{
    "vbcc",
    {statements(),
     labels(),
     memo(),
     assignids(state),
     validids(state),
     typecheck(state),
     optimize(state),
     liveness(state)},
    parser()};

  struct Options : public trieste::Options
  {
    std::filesystem::path path;
    std::filesystem::path output_file;
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
      cli.add_flag(
        "-s,--strip", strip, "Strip debug information from the bytecode.");

      cli.callback([this, &cli]() {
        build = cli.parsed();
        path = cli.get_option("path")->as<std::filesystem::path>();

        if (!path.has_filename())
          path = path.parent_path();

        output_format = output_format_name == "vbc" ? OutputFormat::VBC :
                                                      OutputFormat::LLVMIR;
        std::string extension =
          output_format == OutputFormat::VBC ? ".vbc" : ".ll";

        if (!path.empty() && output_file.empty())
          output_file = path.stem().replace_extension(extension);

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
  auto r = d.run(argc, argv);

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
      state->gen_vbc(opts.output_file, opts.strip);
      break;

    case OutputFormat::LLVMIR:
#if defined(VERONA_ENABLE_LLVM_BACKEND)
      if (!state->gen_llvm(opts.output_file))
        return -1;
#else
      logging::Error() << "vbcc was built without LLVM backend support"
                       << std::endl;
      return -1;
#endif
      break;
  }

  return 0;
}
