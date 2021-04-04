# c-event-actors

This project is an experiment on my part to construct a simple embedded event (or asynchronous framework) wired together to build an actor framework.  This is all written in C.

The characteristics of this experimental framework are:

- "user" code is not executed in parallel,
- actors are able to send messages to each other,
- system operations, like reading a file, are all directed to actors which can perform this operation in parallel, and
- an event loop iterates through messages and forwards them to the appropriate actor

The reason why I do not want this framework to execute "user" code in parallel is that I do not want to add any concurrency control into "user" code.  At all times "user" code can execute knowing that it will not be interrupted.

