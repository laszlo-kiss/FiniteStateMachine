# FiniteStateMachine

Simple state machine classes in C++.

The state machine may have any number of states and events. There can be two types
of Events, external and internal. External events are generated by the 'outside world'
or users of the state machine. Internal events may be generated by the state machinery
itself and are considered high priority (thus serviced before external events). Their
use is optional.

Each state may have an optional entry and exit method in addition to the event handler
methods.

See: https://en.wikipedia.org/wiki/Finite-state_machine

## Install
Place in a convenient include directory and include in a C++11 source file.

##Features
* Header only.
* No dependencies.
* Supports both synchronous and asynchronous use (examples included).

## Tested on:
* Mac OS X 10.10 (Xcode 7 - Apple LLVM version 7.0.0 (clang-700.1.76))

## Alternatives
* https://github.com/makulik/sttcl
* https://github.com/eglimi/cppfsm
* http://www.boost.org/doc/libs/1_58_0/libs/statechart/doc/index.html
* http://www.boost.org/doc/libs/1_58_0/libs/msm/doc/HTML/index.html

## License
* MIT
