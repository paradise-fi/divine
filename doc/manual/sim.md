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
