#include "lang.h"
#include "vbci.h"

#include <trieste/driver.h>

int main(int argc, char** argv)
{
  using namespace trieste;
  using namespace vbcc;

  Reader reader{"vbcc", {statements(), labels(), bytecode()}, parser()};
  Driver d(reader);
  return d.run(argc, argv);
}
