// A fini callback runs while the scheduler is alive, so work it schedules must
// complete before shutdown. Main sets 7, then scheduled teardown work sets 99.
// If teardown runs after scheduler shutdown, the final exit code remains 7.
// Expected exit code: 99.

marker {}

schedule_fini(): none
{
  let done = when ()
  {
    :::set_exit_code(99);
    marker
  };
}

use
{
  init(): any
  {
    :::init_set_exit_code(1);
    { fini_schedules_when::schedule_fini(); }
  }

  init_set_exit_code = "set_exit_code"(i32): none;
  set_exit_code = "set_exit_code"(i32): none;
}

main(): none
{
  :::set_exit_code(7);
}
