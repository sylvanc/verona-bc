# Execution Model

`Program` owns one loaded VBC image and its decoded classes, functions, types,
libraries, symbols, strings, memo slots, and debug data. `Thread` owns logical
frames, locals, argument state, finalization queues, and the current program
counter for one executing behavior.

`Program::run()` loads the image, initializes strings and arguments, starts the
scheduler, runs library initializers and memo initializers, queues `main`, and
runs finalizers after scheduler completion. The process exit code is shared
through runtime support.