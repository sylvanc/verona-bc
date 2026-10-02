#pragma once

#include <vir.h>

namespace virc
{
  trieste::Node err(const std::string& msg);
  trieste::Node err(trieste::Node node, const std::string& msg);
  trieste::Node errmsg(const std::string& msg);
  trieste::Node errloc(trieste::Node node);
}