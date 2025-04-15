#include "lang.h"
#include "vbci.h"

#include <trieste/driver.h>

int main(int argc, char** argv)
{
  trieste::Driver d("vbcc", nullptr, vbcc::parser(), vbcc::passes());
  return d.run(argc, argv);
}
