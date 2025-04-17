#include "bytecode.h"
#include "lang.h"

#include <trieste/driver.h>

int main(int argc, char** argv)
{
  using namespace trieste;
  using namespace vbcc;

  auto state = std::make_shared<State>();
  Reader reader{
    "vbcc",
    {statements(), labels(), assignids(state), validids(state)},
    parser()};
  Driver d(reader);
  auto r = d.run(argc, argv);

  if (r != 0)
    return r;

  if (state->error)
    return -1;

  wf::push_back(wfPassLabels);
  state->gen();
  wf::pop_front();
  return 0;
}
