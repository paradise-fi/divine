# DiOS, DIVINE's Operating System

Programs traditionally rely on a wide array of services provided by their
runtime environment (that is, a combination of libraries, the operating system
kernel, the hardware architecture and its peripherals and so on). When DIVINE
executes a program, it needs to provide these services to the program. Some of
those, especially library functions, can be obtained the same way they are in a
traditional (real) execution environment: the libraries can be compiled just
like other programs and the resulting bitcode files can be linked just as
easily.

The remaining services, though, must be somehow provided by DIVINE, since they
are not readily available as libraries. Some of them are part of the virtual
machine, like memory management and interrupt control (cf. [The DIVINE Virtual
Machine]). The rest is provided by an "operating system". In principle, you can
write your own small operating system for use with DIVINE; however, to make
common verification tasks easier, DIVINE ships with a small OS that provides a
subset of POSIX interfaces that should cover the requirements of a typical
user-level program.

## DiOS Compared to Traditional Operating Systems

The main goal of DiOS is to provide a runtime environment for programs under
inspection, which should not be distinguishable from the real environment. To
achieve this goal, it provides an API for threads (and processes in the future)
handling, faults and virtual machine configuration. DiOS API is then used to
implement the POSIX interface and supports the implementation of standard C and
C++ library.

However, there are a few differences to the real POSIX-compatible OSs the user
should be aware of:

- First of all, DiOS is trapped inside DIVINE VM, and therefore, it cannot
  perform any I/O operations with the outside world. All I/O has to be emulated.
- Consequently, DiOS cannot provide an access to a real file system, but
  supplies a tools for capturing a file system snapshot, which can be used for
  emulation of file operations. See the file system section of this manual for
  further information.
- As the goal of the verification is to show that a program is safe no matter
  what scheduling choices are made, the thread scheduling differs from that of
  standard OSs. The user should not make any assumptions about it.
- DiOS currently does not cover the entire POSIX standard; however the support
  is commonly growing.

## DiOS Fault Handling And Error Traces

When DIVINE 4 VM performs an illegal operation (e.g. division by zero or null
pointer dereference), it performs so-called fault and a user supplied function,
fault handler, is called. This function can react to error - e.g. collect
additional information, or decide, how the error should be handled. DiOS
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
a fault, it prints the reason -- here a null pointer dereference caused it. Then
a DiOS fault handler was called. First, it printed whether the fault occurred in
DiOS kernel or in the user space -- in the actual verified program. Then a
simple backtrace was printed. Note that `_start` is a DiOS function, which is
always at the bottom of a backtrace. It calls all global constructors,
initializes the standard libraries and calls `main` with the right arguments.

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

The first part of the output is the same as in the previous case. The rest are
full DIVINE backtraces for all the active threads. In this concrete example, we
see two backtraces even for a single threaded program. To see the reason for
this, let us first examine backtrace #2. If we go through the backtrace from
its end, we can see an invocation of `_start`, which called `main`. `main` was
then interrupted on line 3 because of the fault. Next, we can see a call to the
DiOS fault handler `__dios::Fault::handler`. As the handler is called from user
space and therefore, it is isolated from DiOS internal state, it performs a
syscall to jump in the kernel, as we can see from the first two entries of the
backtrace.

All DiOS kernel routines, including scheduling and syscalls, feature a separate
call stack -- this is the reason why we observe two backtraces. Therefore,
backtrace #1 contains a scheduler and syscall machinery after which fault
handler is called in kernel mode and can do further inspection.

## DiOS Configuration

DIVINE supports passing of boot parameters to the OS. Some of these parameters
are automatically generated by DIVINE (e.g. name of the program, program
parameters or a snapshot of a file system), others can be specified by the user.
These parameters can be specified using command line option `-o {argument}`.

DiOS expects `{argument}` to be in the form `{command}:{argument}`. DiOS help
can be also invoked with a shortcut `-o help`. All arguments are processed
during the boot. If an invalid command or argument is passed, DiOS fails to
boot. DIVINE handles this state as unsuccessful verification and the output
contains a description of the error. DiOS supports the following commands:

- `debug`: print debug information during the boot. By default no information is
  printed during the boot. Supported arguments:
    - `help`: printed help and abort the boot.
    - `machineparams`: printed user specified machine parameters, e.g. number of
      cpus.
    - `mainargs`: printed `argv` and `envp`, which will be passed to the `main`
      function.
    - `faultcfg`: printed fault and simfail configuration, which will be used
      for verification. Note that if the configuration is not forced, program
      under inspection can change it during its run.
- `trace` and `notrace`: report/do not report state of its argument back to VM.
  Supported arguments:
    - `threads`: reports all active threads back to VM, so it can e.g. allow
      user to choose which thread to run. By default, threads are not traced.
- `ignore`, `report`, `abort` with variant prefixed with `force-`: configure
  handling of a given fault. When `abort` is specified, DiOS passes the fault as
  an error back to the VM and the verification is terminated. Faults marked as
  `report` are reported back to the VM (i.e. the fault is recorded to
  backtrace), but are not treated as errors -- the verification process
  continues. When fault is ignored it is not reported back to the VM at all. If
  the execution after fault continues, instruction producing the fault is
  ignored or produces undefined value. There are following fault categories in
  DIVINE and these faults can be passed to this command:
    - `assert`: C-assert is violated. Default abort.
    - `arithmetic`: arithmetic error occurs -- e.g. division by 0. Default
      abort.
    - `memory`: access to uninitialized memory or invalid pointer dereference
      Default abort.
    - `control`: check return value of function and jumps on undefined values.
      Default abort.
    - `hypercall`: invalid parameter to VM hypercall was passed. Default abort.
    - `notimplemented`: unimplemented operation should be performed. Default
      abort.
    - `diosassert`: internal DiOS assert was violated, can occur in case of
      invalid DiOS API usage. Default abort.
- `simfail` and `nofail`: simulate possible failure of given feature. E.g.
  `malloc` can fail in a real system and therefore, when simfailing, both
  success and failure of `malloc` are tested. Supported arguments:
    - `malloc`: simulate failure of memory allocation.
- `ncpus:<num>`: specify number of machine physical cores. Has no direct impact
  on verification, affects only library calls, which can return number of cores.
- `stdout`: specify how to treat program output on standard output. Following
  options are supported:
    - `notrace`: the output is ignored.
    - `unbuffered`: each write is printed. Can lead to an unexpected formatting
      of the backtrace.
    - `buffered`: each line of output is printed (i.e. formatting of backtrace
      should be reasonable). Default option.
- `stderr`: specify how to treat program output on standard error output. For
  options, see `stdout`.

## DiOS Virtual File System
