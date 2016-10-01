DiOS, DIVINE's Operating System
===============================

Programs traditionaly rely on a wide array of services provided by their
runtime environment (that is, a combination of libraries, the operating system
kernel, the hardware architecture and its peripherals and so on). When DIVINE
executes a program, it needs to provide these services to the program. Some of
those, especially library functions, can be obtained the same way they are in a
traditional (real) execution environment: the libraries can be compiled just
like other programs and the resulting bitcode files can be linked just as
easily.

The remaining services, though, must be somehow provided by DIVINE, since they
are not readily available as libraries. Some of those are part of the virtual
machine, like memory management and interrupt control (cf. [The DIVINE Virtual
Machine]). The rest is provided by an "operating system". In principle, you can
write your own small operating system for use with DIVINE; however, to make
common verification tasks easier, DIVINE ships with a small OS that provides a
subset of POSIX interfaces that should cover the requirements of a typical
user-level program.
