# Interactive Debugging {#sim}

DIVINE comes with an interactive debugger, available as `divine sim`. The
debugger loads programs the same way `verify` and other DIVINE commands do, so
you can run it on standalone C or C++ source files directly, or you can compile
larger programs into bitcode and load that.

## Tutorial

Let's say you have a small C program which you wish to debug. We will refer to
this program as `program.c`. To load the program into the debugger, simply execute

    $ divine sim program.c

and DIVINE will take care of compiling and linking your program. It will load
the resulting bitcode but will not execute it immediately: instead, `sim` will
present a prompt to you, looking like this:

    # executing __boot at /divine/src/dios/dios.cpp:79
    >

The `__boot` function is common to all DIVINE-compiled programs and belongs to
DiOS, our minimalist operating system. When debugging user-mode programs, the
good first command to run is

    > start

which will start executing the program until it enters the `main` function:

    # a new program state was stored as #1
    # active threads: [0:0]
    # a new program state was stored as #2
    # active threads: [0:0]
    # executing main at program.c:14
    >

We can already see a few DIVINE-specific features of the debugger. First,
program states are stored and retained for future reference. Second, thread
switching is quite explicit -- every time a scheduling decision is made, `sim`
informs you of this fact. We will look at how to influence these decisions
later.

Now is a good time to familiarise ourselves with how to inspect the
program. There are two commands for listing the program itself: `source` and
`bitcode` and each will print the currently executing function in the
appropriate form (the original C source code and LLVM bitcode,
respectively). Additionally, when printing bitcode, the current values of all
LLVM registers are shown as inline comments:

      label %entry:
    >>  %01 = alloca [i32 1 d]                                  # [global* 0 0 uun]
        dbg.declare 
        %03 = getelementptr %01 [i32 0 d] [i32 0 d]             # [global* 0 0 uun]
        call @init %03 

Besides printing the currently executing function, both `source` and `bitcode`
can print code corresponding to other functions; in fact, by default they print
whatever function the debugger variable `$frame` refers to. To print the source
code of the current function's caller, you can issue

    > source caller
         97 void _start( int l, int argc, char **argv, char **envp ) {
    >>   98     int res = __execute_main( l, argc, argv, envp );
         99     exit( res );
        100 }

To inspect data, we can instead use `show` and `inspect`. We have mentioned
`$frame` earlier: there is, in fact, a number of variables set by `sim`. The
most interesting of those is `$_` which normally refers to the most interesting
object at hand, typically the executing frame. By default, `show` will print
the content of `$_` (like many other commands). However, when we pass an
explicit parameter to these commands, the difference between `show` and
`inspect` becomes apparent: the latter also sets `$_` to its parameter. This
makes `inspect` more suitable for exploring more complex data, while `show` is
useful for quickly glancing at nearby values:

    > step --count 2
    > show
    attributes:
        address: heap* 9cd25662 0+0
        shared:  0
        pc:      code* 2 2
        insn:    dbg.declare 
        location: program.c:16
        symbol:  main
    .x:
        type:    int[]
        .[0]:
            type: int
            value: [i32 0 u]
        .[1]:
            type: int
            value: [i32 0 u]
        .[2]:
            type: int
            value: [i32 0 u]
        .[3]:
            type: int
            value: [i32 0 u]
    related:     [ caller ]

This is how a frame is presented when we look at it with `show`.

## Collecting Information

Apart from `show` and `inspect` which simply print structured program data to
the screen, there are additional commands for data extraction. First,
`backtrace` will print the entire stack trace in one go, by default starting
with the currently executing frame. It is also possible to obtain *all* stack
traces reachable from a given heap variable, e.g.

    > backtrace $state
    # backtrace 1:
      __dios::_InterruptMask<true>::Without::Without(__dios::_InterruptMask<true>&)
          at /divine/include/libc/include/sys/interrupt.h:42
      _pthread_join(__dios::_InterruptMask<true>&, _DiOS_TLS*, void**)
          at /divine/include/libc/include/sys/interrupt.h:77
      pthread_join at /divine/src/libc/functions/sys/pthread.cpp:539
      main at test/pthread/2.mutex-good.c:22
      _start at /divine/src/libc/functions/sys/start.cpp:76
    # backtrace 2:
      __pthread_entry at /divine/src/libc/functions/sys/pthread.cpp:447

Another command to gather data is `call`, which allows you to call a function
defined in the program. The function must not take any parameters and will be
executed in *debug mode* (see [@sec:debugmode] -- the important caveat is that
any `dbg.call` instructions in your info function will be ignored). Execution
of the function will have no effect on the state of the simulated program. If
you have a program like this:

~~~ {.c}
#include <sys/divm.h>

void print_hello()
{
    __vm_trace( _VM_T_Text, "hello world" );
}

int main() {}
~~~

The `call` command works like this:

    > call print_hello
      hello world

Finally, the `info` command serves as an universal information gathering alias
-- you can set up your own commands that then become available as `info`
sub-commands:

    > info --setup "call print_hello" hello
    > info hello
      hello world

The `info` command also provides a built-in sub-command `registers` which
prints the current values of machine control registers (see also
[@sec:control]):

    > info registers
    Constants:    220000000
    Globals:      120000000
    Frame:        9cd2566220000000
    PC:           340000000
    Scheduler:    eb40000000
    State:        4d7b876d20000000
    IntFrame:     1020000000
    Flags:        0
    FaultHandler: b940000000
    ObjIdShuffle: faa6693f
    User1:        0
    User2:        201879b120000000
    User3:        6ca5bc2260000000
    User4:        0
