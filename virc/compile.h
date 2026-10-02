#pragma once

#include "model/compilation.h"

namespace virc
{
  struct CompileResult
  {
    trieste::ProcessResult process;
    std::shared_ptr<Compilation> state;

    explicit operator bool() const
    {
      return process.ok && !state->error;
    }

    const Compilation& compilation() const
    {
      return *state;
    }

    const trieste::Node& ast() const
    {
      return process.ast;
    }
  };

  std::vector<trieste::Pass> pipeline(std::shared_ptr<Compilation> state);
  CompileResult compile(trieste::Node reified_vir);

  namespace vbc
  {
    void emit(
      const Compilation& compilation,
      const std::filesystem::path& output,
      bool strip = false);
  }

  namespace llvm
  {
    bool emit(
      const Compilation& compilation, const std::filesystem::path& output);
  }
}
