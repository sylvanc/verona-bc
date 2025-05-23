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
    {statements(),
     labels(),
     assignids(state),
     validids(state),
     liveness(state)},
    parser()};

  Driver d(reader, &options());
  auto r = d.run(argc, argv);

  if (r != 0)
    return r;

  if (state->error)
    return -1;

  wf::push_back(wfIR);
  state->gen();
  wf::pop_front();
  return 0;
}
