An Introduction to Using DIVINE
===============================

In this section, we will give a short example on how to invoke DIVINE and its
various functions. We will use a small C program (consisting of a single
compile unit) as an example, along with a few simple properties.

## Basics of Program Analysis

We will start with the following simple C program:

```{.c .numberLines}
#include <stdio.h>
#include <assert.h>

void foo( int *array ) {
    for ( int i = 0; i <= 4; ++i ) {
        printf( "writing at index %d\n", i );
        array[i] = 42;
    }
}

int main() {
    int x[4];
    foo( x );
    assert( x[3] == 42 );
}
```

The above code contains a bug, an access out of bounds of `array` for `i == 4`; we
will see how this is presented by DIVINE.

The program as it is can be compiled by your system's C compiler and run. If you
do so, it will probably run OK despite the out of bounds access (this is an example
of a stack buffer overflow – the program will incorrectly overwrite an adjacent
value on the stack which, in most cases, does not interfere with its execution).
We can proceed to verify the program using DIVINE:

```{.bash}
$ divine verify program.c
```

DIVINE will now compile your program and run the verifier on the compiled
code. After a short while, it will produce the following output:

```
compiling /tmp/test.c
loading bitcode … LART … RR … constants … done
booting … done
found 183 states in 0:00, averaging 358.8

state count: 183
states per second: 358.824
version: 4.0.3+d6ac9355109

architecture: Intel(R) Core(TM) i7-3520M CPU @ 2.90GHz
memory used: 759940
physical memory used: 385836
user time: 3.020000
system time: 0.060000
wall time: 3.123273

timers:
  lart: 0.463
  loader: 0.796
  boot: 0.12
  search: 0.51
  ce: 0.272
error found: yes
error trace: |
  writing at index 0
  writing at index 1
  writing at index 2
  writing at index 3
  writing at index 4
  FAULT: access of size 1 at [heap* 712567f3 10 ddp] is 1 bytes out of bounds
  Fault in userspace: memory
  Backtrace:
    1: foo
    2: main
    3: _start

choices made: 0^182
error state:
  backtrace 1: # active stack
    - address: heap* 567b80fb 0+0
      pc: code* 129 52
      location: /divine/src/dios/core/fault.cpp:49
      symbol: __dios::Fault::sc_handler(__dios::Context&, int*, void*, __va_list_tag*)

    - address: heap* c25416f1 0+0
      pc: code* 132 1
      location: /divine/src/dios/core/fault.cpp:274
      symbol: __sc::fault_handler(__dios::Context&, int*, void*, __va_list_tag*)

    - address: heap* dc1c5195 0+0
      pc: code* 11b 18
      location: /divine/include/dios/core/syscall.hpp:39
      symbol: __dios::Syscall::handle(__dios::Context*)

    - address: heap* 11 0+0
      pc: code* fd 32
      location: /divine/include/dios/core/scheduling.hpp:198
      symbol: void __dios::sched<false>()

  backtrace 2:
    - address: heap* cfbb6a51 0+0
      pc: code* c0d 17
      location: /divine/src/libc/functions/unistd/syscall.c:14
      symbol: __dios_trap

    - address: heap* 6637de5b 0+0
      pc: code* c0e c
      location: /divine/src/libc/functions/unistd/syscall.c:27
      symbol: __dios_syscall

    - address: heap* 12 0+0
      pc: code* 12b 16
      location: /divine/src/dios/core/fault.cpp:76
      symbol: __dios::Fault::handler(_VM_Fault, _VM_Frame*, void (*)(), ...)

    - address: heap* 4a0081dc 0+0
      pc: code* 1 10
      location: /tmp/test.c:8
      symbol: foo

    - address: heap* bf24efc5 0+0
      pc: code* 2 3
      location: /tmp/test.c:14
      symbol: main

    - address: heap* 14f75f5a 0+0
      pc: code* 143 7
      location: /divine/src/dios/core/main.cpp:152
      symbol: _start
```

The output begins with compile- and load-time report messages,
followed by some statistics. Then, starting with `error found: yes`, the detected error
is introduced. The error-related information contains:

*   `error trace` -- shows the output that the program printed on its run
    from the beginning of the execution to the point of the error.
    A description of the error concludes this section.

*   `choices made` -- indicates the values of nondeterministic choices DIVINE made
    in the run.[^choices] This information is mostly useful as an identifier of the
    error trace.

*   `error state` -- this field contains backtraces of all stacks present in
    the program. For a single-threaded program in an error state, there are two stacks --
    the stack of the program's main thread (with `_start`, followed by
    `main`, at the bottom) and the stack of DIVINE's runtime (DiOS, see later),
    which contains the fault handler responsible for terminating the
    verification.

[^choices]: this information is run-length encoded (e.g. `0^2 2^4` means the first two
    choices returned 0 and the following four returned 2);

In our case, the most important information is `FAULT: access of size 1 at
[heap* 712567f3 10 ddp] is 1 bytes out of bounds` which indicates the error is
caused by an invalid memory access. The other crucial information is the line of code
which caused it (this can be found on the second stack, in the entry for the `foo`
function):

```
- address: heap* 4a0081dc 0+0
  pc: code* 1 10
  location: /tmp/program.c:8
  symbol: foo
```

Hence we can see that the problem is caused by an invalid memory access on line 8 in
our program.

*Note*: one might notice that the addresses in DIVINE are printed in the form `
[heap* 712567f3 10 ddp]`; the meaning of which is: the pointer in question is a heap
pointer (other types of pointers are constant pointers and global pointers; stack
pointers are not distinguished from heap pointers); the object id (in hexadecimal, assigned
to the allocation) is `712567f3`; the offset (again in hexadecimal) is `10` and the value is a
defined pointer (`ddp`, i.e. it is not an uninitialised value).

### Debugging Counterexamples with the Interactive Simulator

We have found a bug in our program. While in this case the cause might be visible
directly, it is useful to be able to inspect the error in more detail. For
this, we can use DIVINE's simulator.

```{.bash}
divine sim program.c
```

After the compilation, we will land in an interactive debugger environment:

```
Welcome to 'divine sim', an interactive debugger. Type 'help' to get started.
# executing __boot at /divine/src/dios/core/dios.cpp:315
>
```

There are a few commands we could use in this situation. For instance,
the `start` command brings the program to the beginning of the `main`
function (skipping the internal program initialization process). However, since
we have the error trace, we will instead jump to the end of the trace with `trace 0^182`
(the sequence of numbers after the `trace` keyword should be replaced by the value taken from
`choices made` from the output of `divine verify`). The simulator now outputs
identifiers of the produced states, together with outputs the program printed so far
(prefixed with `T:`), and ends just after the last choice before the error. We
can now let the simulator run the program until the error is found (or a new
state is produced) with `stepa`.

At this point, the simulator should have stopped in the fault handler which is
executed by DIVINE when an error is found. We can use the `up` command to step
from the fault handler to the frame of the function in which the error occurred
(generally, `up` can be used to move from the callee to the caller on the stack,
`down` can be used to move in the opposite direction). As we should be in the
frame of the `foo` function, we can use the `show` command to print local
variables present in this function. In the output there should be an entry for
`i`:

```
.i$1:
    type:    int
    value:   [i32 4 d]
```

The entry suggests that `i` is an `int`{.c} variable. It is represented as
`[i32 4 d]`, meaning it is a 32 bit integer with value 4 and it is fully defined
(`d`). If we jump one frame `up` and use `show` again, we can see the entry for `x`:

```
.x:
    type:    int[]
    .[0]:
        type: int
        value: [i32 42 d]
    .[1]:
        type: int
        value: [i32 42 d]
    .[2]:
        type: int
        value: [i32 42 d]
    .[3]:
        type: int
        value: [i32 42 d]
```

Here, we see that `x` is an `int`{.c} array and that it contains 4 values,
therefore, access at `x[4]` is out of bounds of this array.

More details about the simulator can be found in the [interactive debugging
section](#sim).

### Controlling the Execution Environment

Programs in DIVINE run in an environment provided by [DiOS](#dios),
DIVINE's operating system, and by runtime libraries (including C and C++
standard libraries and `pthreads`). The behaviour of this runtime can be configured
using the `-o` option. To get the list of options, run `divine info program.c`:

```
$ divine info program.c
compiling program.c

DIVINE 4.0.5

Available options for program.c are:
I: DiOS boot info:
I:   help:
I:     supported commands:
I:       - "debug":
I:           description: print debug information during boot
I:           arguments:
I:             - help - help and exit
I:             - machineparams - specified by user, e.g. number of cpus
I:             - mainargs - argv and envp
I:             - faultcfg - fault and simfail configuration
I:       - "{trace|notrace}":
I:           description: report/not report item back to VM
I:           arguments:
I:             - threads - thread info during execution
I:       - "[force-]{ignore|report|abort}":
I:           description: configure fault, force disables program override
I:           arguments:
I:             - assert
I:             - arithmetic
I:             - memory
I:             - control
I:             - locking
I:             - hypercall
I:             - notimplemented
I:             - diosassert
I:       - "{nofail|simfail}":
I:           description: enable/disable simulation of failure
I:           arguments:
I:             - malloc
I:       - "ncpus":
I:           description: specify number of cpu units (default 0)
I:           arguments:
I:             - <num>
I:       - "{stdout|stderr}":
I:           description: specify how to treat program output
I:           arguments:
I:             - notrace - ignore the stream
I:             - unbuffered - trace each write
I:             - trace - trace after each newline (default)
I:       - "syscall":
I:           description: specify how to treat standard syscalls
I:           arguments:
I:             - simulate - simulate syscalls, use virtual file system (can be used with verify and run)
I:             - passthrough - use syscalls from the underlying host OS (cannot be used with verify)
use -o {option}:{value} to pass these options to the program
```

It is often convenient to assume that malloc never fails: this can be achieved by
passing the `-o nofail:malloc` option to DiOS. Other important options are those controlling
the fatalness of errors (the default option is `abort` -- if an error of type `abort` is
encountered, the verification ends with an error report; on the other hand, the verifier
will attempt to continue when it detects an error that was marked as `ignore`).

Furthermore, it is possible to pass arguments to the `main` function of the
program by appending them after the name of the source file (e.g. `divine verify
program.c main-arg-1 main-arg-2`), and to add environment variables for the
program using the `-DVAR=VALUE` option.


### Compilation Options and Compilation of Multiple Files

Supposing you wish to verify something that is not just a simple C program,
you may have to pass compilation options to DIVINE. In some cases, it is sufficient
to pass the following options to `divine verify`:

*   `-std=<version>`, (e.g. `-std=c++14`) option sets the standard of C/C++ to
    be used and is directly forwarded to the compiler;

*   other options can be passed using the `-C` option, i.e. `-C,-O3` to enable
    optimizations, or `-C,-I,include-path` to add `include-path` to the
    directories in which compiler looks for header files.

However, if you need to pass more options or if your program consists of more
than one source file, it might be more practical to compile it to LLVM bitcode
first and pass this bitcode to `divine verify`:

```{,bash}
$ divine cc -Iinclude-path -O1 -std=c++14 -DFOO=bar program.c other.c
$ divine verify program.bc
```

`divine cc` is a wrapper for the clang compiler and it is possible to pass most
of clang's options to it directly.
