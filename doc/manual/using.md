An Introduction to Using DIVINE
===============================

In this section, we will give a short example on how to invoke DIVINE and its
various functions. We will use a small C program (consisting of a single
compilation unit) as an example, along with a few simple properties.

## Basics of Program Analysis

We will start with the following simple C program:

```{.c .numberLines}
#include <stdio.h>
#include <assert.h>

void init( int *array )
{
    for ( int i = 0; i <= 4; ++i )
    {
        printf( "writing at index %d\n", i );
        array[i] = i;
    }
}

int main()
{
    int x[4];
    init( x );
    assert( x[3] == 3 );
}
```

The above code contains a bug: an out of bounds access to `array` at index `i
== 4`; we will see how this is presented by DIVINE.

The program can be compiled by your system's C compiler and executed. If you do
so, it will probably run OK despite the out-of-bounds access (this is an
example of a stack buffer overflow –- the program will incorrectly overwrite an
adjacent value on the stack which, in most cases, does not interfere with its
execution).  We can proceed to check the program using DIVINE:

    $ divine check program.c

DIVINE will compile your program and run the verifier on the compiled
code. After a short while, it will produce the following output:

    compiling program.c
    loading bitcode … LART … RR … constants … done
    booting … done
    states per second: 419.192
    state count: 83

    error found: yes
    error trace: |
      [0] writing at index 0
      [0] writing at index 1
      [0] writing at index 2
      [0] writing at index 3
      [0] writing at index 4
      FAULT: access of size 4 at [heap* 295cbfc3 10h ddp] is 4 bytes out of bounds
      [0] FATAL: memory error in userspace

    active stack:
      - symbol: void {Fault}::handler<{Context} >(_VM_Fault, _VM_Frame*, void (*)(), ...)
        location: /divine/include/dios/core/fault.hpp:189
      - symbol: init
        location: program.c:9
      - symbol: main
        location: program.c:16
      - symbol: _start
        location: /divine/src/libc/functions/sys/start.cpp:77
    a report was written to program.report.mtjmlg

The output begins with compile- and load-time report messages, followed by some
statistics. Starting with `error found: yes`, the detected error is
introduced. The error-related information contains:

 * `error trace` -- shows the output that the program printed during its
   execution, until the point of the error; a description of the error
   concludes this section,

 * `active stack` -- this field contains the stack trace of the thread
   responsible for the error (with a fault handler at the top)

In our case, the most important information is `FAULT: access of size 4 at
[heap* 295cbfc3 10h ddp] is 4 bytes out of bounds` which indicates the error was
caused by an invalid memory access. The other crucial information is the line
of code which caused the error:

      - symbol: init
        location: program.c:9

Hence we can see that the problem is caused by an invalid memory access on line
9 in our program.

*Note:* one might notice that the addresses in DIVINE are printed in the form
`[heap* 295cbfc3 10h ddp]`; the meaning of which is: the pointer in question is
a heap pointer (other types of pointers are constant pointers and global
pointers; stack pointers are not distinguished from heap pointers); the object
identifier (in hexadecimal, assigned by the VM) is `295cbfc3`; the offset
(again in hexadecimal) is `10` and the value is a defined pointer (`ddp`,
i.e. it is not an uninitialised value).

## Debugging Counterexamples with the Interactive Simulator

Now that we have found a bug in our program, it might be useful to inspect the
error in more detail. For this, we can use DIVINE's simulator.

    $ divine sim program.c

After compilation, we will land in an interactive debugger environment:

```
Welcome to 'divine sim', an interactive debugger. Type 'help' to get started.
# executing __boot at /divine/src/dios/core/dios.cpp:315
>
```

There are a few commands we could use in this situation. For instance, the
`start` command brings the program to the beginning of the `main` function
(fast-forwarding through the internal program initialisation process). Another
alternative is to invoke `sim` with `--load-report` (where the name of the
report is printed at the very end of the `check` output), like this:

    $ divine sim --load-report program.report.mtjmlg

The simulator now prints identifiers of program states along the violating
execution, together with the output of the program (prefixed with `T:`). The
replay stops at the error that was found by `divine check`.

One can use the `up` and `down` commands to move through the active stack, to
examine the context of the error. To examine local variables, we can use the
`show`, including the value of `i`:

    .i$1:
        type:    int
        value:   [i32 4 d]

The entry suggests that `i` is an `int`{.c} variable and its value is
represented as `[i32 4 d]`: meaning it is a 32 bit integer with value 4 and it
is fully defined (`d`). If we go one frame `up` and use `show` again, we can
see the entry for `x`:

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

We see that `x` is an `int`{.c} array and that it contains 4 values: the access
at `x[4]` is clearly out of bounds.

More details about the simulator can be found in the section
on [interactive debugging](#sim).

## Controlling the Execution Environment

Programs in DIVINE run in an environment provided by [DiOS](#dios),
DIVINE's operating system, and by runtime libraries (including C and C++
standard libraries and `pthreads`). The behaviour of this runtime can be configured
using the `-o` option. To get the list of options, run `divine info program.c`:

    $ divine info program.c
    compiling program.c

    DIVINE 4.0.22

    Available options for test/c/2.trivial.c are:
    - [force-]{ignore|report|abort}: configure the fault handler
      arguments:
       - assert
       - arithmetic
       - memory
    [...]
    - config: run DiOS in a given configuration
      arguments:
       - default: async threads, processes, vfs
       - passthrough: pass syscalls to the host OS
       - replay: re-use a trace recorded in passthrough mode
       - synchronous: for use with synchronous systems
    [...]
    use -o {option}:{value} to pass these options to the program

It is often convenient to assume that malloc never fails: this can be achieved by
passing the `-o nofail:malloc` option to DiOS. Other important options are those controlling
the fatalness of errors (the default option is `abort` -- if an error of type `abort` is
encountered, the verification ends with an error report; on the other hand, the verifier
will attempt to continue when it detects an error that was marked as `ignore`).

Furthermore, it is possible to pass arguments to the `main` function of the
program by appending them after the name of the source file (e.g. `divine verify
program.c main-arg-1 main-arg-2`), and to add environment variables for the
program using the `-DVAR=VALUE` option.

## Compilation Options and Compilation of Multiple Files

Supposing you wish to verify something that is not just a simple C program,
you may have to pass compilation options to DIVINE. In some cases, it is sufficient
to pass the following options to `divine verify`:

 * `-std=<version>`, (e.g. `-std=c++14`) option sets the standard of C/C++ to
   be used and is directly forwarded to the compiler;

 * other options can be passed using the `-C` option, i.e. `-C,-O3` to enable
   optimisation, or `-C,-I,include-path` to add `include-path` to the
   directories in which compiler looks for header files.

However, if you need to pass more options or if your program consists of more
than one source file, it might be more practical to compile it to LLVM bitcode
first and pass this bitcode to `divine verify`:

    $ divine cc -Iinclude-path -O1 -std=c++14 -DFOO=bar program.c other.c
    $ divine verify program.bc

`divine cc` is a wrapper for the clang compiler and it is possible to pass most
of clang's options to it directly.
