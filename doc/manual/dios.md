# DiOS, A Small, Verification-Oriented Operating System {#dios}

Programs traditionally rely on a wide array of services provided by their
runtime environment (that is, a combination of libraries, the operating system
kernel, the hardware architecture and its peripherals and so on). When DIVINE
executes a program, it needs to provide these services to the program. Some of
those, especially library functions, can be obtained the same way they are in a
traditional (real) execution environment: the libraries can be compiled just
like other programs and the resulting bitcode files can be linked just as
easily.

The remaining services, though, must be somehow supplied by DIVINE, since they
are not readily available as libraries. Some of them are part of the virtual
machine, like memory management and interrupt control (cf. [The DIVINE Virtual
Machine]). The rest is provided by an "operating system". In principle, you can
write your own small operating system for use with DIVINE; however, to make
common verification tasks easier, DIVINE ships with a small OS that implements a
subset of POSIX interfaces that should cover the requirements of a typical
user-level program.

## DiOS Compared to Traditional Operating Systems

The main goal of DiOS is to provide a runtime environment for programs under
inspection, which should not be distinguishable from the real environment. To
achieve this goal, it presents an API for thread (and process in the future)
handling, faults and virtual machine configuration. DiOS API is then used to
implement the POSIX interface and supports the implementation of standard C and
C++ library.

However, there are a few differences to the real POSIX-compatible OSs the user
should be aware of:

- First of all, DiOS is trapped inside DIVINE VM, and therefore, it cannot
  perform any I/O operations with the outside world. All I/O has to be emulated.
- Consequently, DiOS cannot provide an access to a real file system, but
  supplies tools for capturing a file system snapshot, which can be used for
  emulation of file operations. See the file system section of this manual for
  further information.
- As the goal of the verification is to show that a program is safe no matter
  what scheduling choices are made, the thread scheduling differs from that of
  standard OSs. The user should not make any assumptions about it.
- DiOS currently does not cover the entire POSIX standard; however the support
  is commonly growing.

## Fault Handling And Error Traces

When DIVINE 4 VM performs an illegal operation (e.g. division by zero or null
pointer dereference), it performs a so-called fault and a user supplied function,
the fault handler, is called. This function can react to the error - collect
additional information or decide how the error should be handled. DiOS
provides its own fault handler, so that verified programs do not have to.

The DiOS fault handler prints a simple human readable stack trace together with
a short summary of the error. When a fault is triggered, it can either abort the
verification or continue execution -- depending on its configuration for a given
fault. Please see the next section for configuration details.

Consider the following simple C++ program:

```cpp
int main() {
    int *a = nullptr;
    *a = 42;
    return 0;
}

```

This program does nothing interesting, it just triggers a fault. If we execute
it using `./divine run -std=c++11 test.cpp`, we obtain the following output:

```
T: FAULT: null pointer dereference: [const* 0 0 ddp]
T: Fault in userspace: memory
T: Backtrace:
T:   1: main
T:   2: _start
```

By inspecting the trace, we can see that a fault was triggered. When VM triggers
a fault, it prints the reason -- here a null pointer dereference caused it. The
encounter of an error caused a DiOS fault handler to be called. The fault handler
first communicates whether the fault occurred in the DiOS kernel or in the user space --
in the actual verified program. This is followed by a simple backtrace. Note that
`_start` is a DiOS function, which is always at the bottom of a backtrace. It calls
all global constructors, initializes the standard libraries and calls `main` with
the right arguments.

If we run `./divine verify -std=c++11 test.cpp`, we obtain a more detailed
backtrace:

```
the error trace:
  FAULT: null pointer dereference: [const* 0 0 ddp]
  Fault in userspace: memory
  Backtrace:
    1: main
    2: _start

backtrace #1 [active stack]:
  @address: heap* 998dffb7 0+0
  @pc: code* 956 50
  @location: /divine/src/dios/fault.cpp:49
  @symbol: __dios::Fault::sc_handler(__dios::Context&, int*, void*, __va_list_tag*)

  @address: heap* 118ab8d1 0+0
  @pc: code* 95f 1
  @location: /divine/src/dios/fault.cpp:272
  @symbol: __sc::fault_handler(__dios::Context&, int*, void*, __va_list_tag*)

  @address: heap* e6dbccb9 0+0
  @pc: code* b11 16
  @location: /divine/include/dios/syscall.hpp:50
  @symbol: __dios::Syscall::handle(__dios::Context*)

  @address: heap* 11 0+0
  @pc: code* a75 26
  @location: /divine/include/dios/scheduling.hpp:185
  @symbol: void __dios::sched<false>()

backtrace #2:
  @address: heap* 9aeef2fc 0+0
  @pc: code* a8c 17
  @location: /divine/include/dios/syscall.hpp:43
  @symbol: __dios::Syscall::trap(int, int*, void*, __va_list_tag (&) [1])

  @address: heap* c149bfb0 0+0
  @pc: code* 982 b
  @location: /divine/src/dios/syscall.cpp:16
  @symbol: __dios_syscall

  @address: heap* 12 0+0
  @pc: code* 958 16
  @location: /divine/src/dios/fault.cpp:76
  @symbol: __dios::Fault::handler(_VM_Fault, _VM_Frame*, void (*)(), ...)

  @address: heap* 32250330 0+0
  @pc: code* c58 2
  @location: drt_dev/test_error.cpp:3
  @symbol: main

  @address: heap* ea18a3ba 0+0
  @pc: code* 96b 7
  @location: /divine/src/dios/main.cpp:152
  @symbol: _start

```

The first part of the output is the same as in the previous case, the rest are
full DIVINE backtraces for all the active threads. In this concrete example, we
see two backtraces even for a single threaded program. To see the reason for
this, let us first examine backtrace #2. If we go through the backtrace from
its end, we can see an invocation of `_start`, which called `main`. `main` was
then interrupted on line 3 because of the fault. Next, we can see a call to the
DiOS fault handler `__dios::Fault::handler`. As the handler is called from user
space and, consequently, is isolated from the internal state of DiOS, it performs a
syscall to jump into the kernel, as we can see from the first two entries of the
backtrace.

All DiOS kernel routines, including scheduling and syscalls, feature a separate
call stack -- this is the reason why we observe two backtraces. Therefore,
backtrace #1 contains a scheduler and syscall machinery after which fault
handler is called in kernel mode and can do further inspection.

## DiOS Configuration {#sec:dios.config}

DIVINE supports passing of boot parameters to the OS. Some of these parameters
are automatically generated by DIVINE (e.g. the name of the program, program
parameters or a snapshot of a file system), others can be supplied by the user.
These parameters can be specified using the command line option `-o {argument}`.

DiOS expects `{argument}` to be in the form `{command}:{argument}`. DiOS help
can be also invoked with the shortcut `-o help`. All arguments are processed
during the boot phase: if an invalid command or argument is passed, DiOS fails
to boot. DIVINE handles this state as unsuccessful verification and the output
contains a description of the error. DiOS supports the following commands:

- `debug`: print debug information during the boot. By default no information is
  printed during the boot. Supported arguments:
    - `help`: print help and abort the boot,
    - `machineparams`: print user specified machine parameters, e.g. the number
      of cpus,
    - `mainargs`: print `argv` and `envp`, which will be passed to the `main`
      function,
    - `faultcfg`: print fault and simfail configuration, which will be used for
      verification; note that if the configuration is not forced, the program
      under inspection can change the configuration.
- `trace` and `notrace`: report/do not report state of its argument back to VM;
  supported arguments:
    - `threads`: report all active threads back to the VM, so it can e.g. allow
      user to choose which thread to run. By default, threads are not traced.
- `ignore`, `report`, `abort` with variants prefixed with `force-`: configure
  handling of a given fault type. When `abort` is specified, DiOS passes the
  fault as an error back to the VM and the verification is terminated. Faults
  marked as `report` are reported back to the VM, but are not treated as errors
  -- the verification process may continue past the fault. When a fault is
  ignored, it is not reported back to the VM at all. If the execution after a
  fault continues, the instruction causing the fault is ignored or produces an
  undefined value. The following fault categories are present in DIVINE and
  these faults can be passed to the command:
    - `assert`: an `assert` call fails,
    - `arithmetic`: arithmetic errors -- e.g. division by 0,
    - `memory`: access to uninitialised memory or an invalid pointer dereference,
    - `control`: check the return value of a function and jump on undefined values,
    - `hypercall`: an invalid parameter to a VM hypercall was passed,
    - `notimplemented`: attempted to perform an unimplemented operation,
    - `diosassert`: an internal DiOS assertion was violated.
- `simfail` and `nofail`: simulate a possible failure of the given
  feature. E.g.  `malloc` can fail in a real system and therefore, when set to
  `simfail`, both success and failure of `malloc` are tested. Supported
  arguments:
    - `malloc`: simulate failure of memory allocation.
- `ncpus:<num>`: specify number of machine physical cores -- this has no direct
  impact on verification and affects only library calls which can return number
  of cores.
- `stdout`: specify how to treat the standard output of the program; the
  following options are supported:
    - `notrace`: the output is ignored.
    - `unbuffered`: each write is printed on a separate line,
    - `buffered`: each line of the output is printed (default)
- `stderr`: specify how to treat the standard error output; see also `stdout`.

## Virtual File System

DiOS provides a POSIX-compatible filesystem implementation. Since no real I/O
operations are allowed, the *Virtual File System* (VFS) operates on top of a
filesystem snapshot and the effects of operations performed on the VFS are not
propagated back to the host. The snapshots are created by DIVINE just before
DiOS boots; to create a snapshot of a directory, use the option `--capture
{vfsdir}`, where `{vfsdir}` consists of up to three `:`-separated components:

 * path to a directory (mandatory),
 * `follow` or `nofollow` -- specifies whether symlink targets should or should
   not be captured (optional, defaults to `follow`),
 * the “mount point” in the VFS -- if not specified, it is the same as the
   capture path

DIVINE can capture files, directories, symlinks and hardlinks. Additionally,
DiOS can also create pipes and UNIX sockets but these cannot be captured from
the host system by DIVINE.

The size of the snapshot is by default limited to 16 MiB. This prevents
accidental capture of a large snapshot. Example:

    divine verify --capture testdir:follow:/home/test/ --vfslimit 1kB test.cpp

Additionally, the stdin of the program can be provided in a file and used using
the DIVINE switch `--stdin {file}`. Finally, the standard and error output can
be handled in one of several ways:

 * it can be completely ignored
 * it is traced and becomes part of transition labels
    * in a line-buffered fashion (this is the default behaviour)
    * in an unbuffered way, where each `write` is printed as a single line of
      the trace

All of the above options can be specified via DiOS boot parameters. See
[@sec:dios.config] for more details.

The VFS implements basic POSIX syscalls -- `write`, `read`, etc. and standard C
functions like `printf`, `scanf` or C++ streams are implemented on top of
these. All functions which operate on the filesystem only modify the internal
filesystem snapshot.
