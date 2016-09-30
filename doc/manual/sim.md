# Interactive Debugging

DIVINE comes with an interactive debugger, available as `divine sim`. The
debugger loads programs the same way `verify` and other DIVINE commands do, so
you can run it on standalone C or C++ source files directly, or you can compile
larger programs into bitcode and load that.

## Tutorial

Let's say you have a small C program which you wish to debug. We will refer to
this program as `program.c`. To load the program into the debugger, simply execute

    $ divine sim program.c

and DIVINE will take care of compiling and linking your program and load it,
but will not execute it. Instead, `sim` will present a prompt to you, looking
like this:

    # executing __boot at /divine/src/dios/dios.cpp:79
    >

The `__boot` function is common to all DIVINE-compiled programs and belongs to
DiOS, our minimalist operating system. When debugging user-mode programs, the
good first command to run is

    > start

which will start executing the program until it enters the `main` function:

    # a new program state was stored as #1
    # active threads: [1:0]
    T: starting program.c...
    # a new program state was stored as #2
    # active threads: [1:0]
    # executing main at program.c:13
    >

Here, we can already see a few DIVINE-specific features of the debugger. First,
program states are stored and retained for future reference. Second, thread
switching is quite explicit -- every time a scheduling decision is made, `sim`
informs you of this fact. We will look at how to influence these decisions
later.

Now is a good time to familiarise ourselves with how to inspect the
program. There are two commands for listing the program itself: `source` and
`bitcode`, each will print the currently executing function in the appropriate
form (the original C source code and LLVM bitcode, respectively). Additionally,
when printing bitcode, the current values of all LLVM registers are shown
as inline comments:

    >>label %entry:
        %01 = alloca [i32 1 d]                                  # [const* 0 0 uu]
        %02 = call @pthread_create %01 [const* 0 0 dd]          # [i32 0 u]
                   [code* 233 0 dd] [const* 0 0 dd] 
        call @__vm_interrupt_mem [global* 2d 0 dd] [i32 1 d] 
        %04 = load [global* 2d 0 dd]                            # [i32 0 u]

Besides printing the currently executing function, both `source` and `bitcode`
can print code corresponding to other functions; in fact, by default they print
whatever function the debugger variable `$frame` refers to. To print the source
code of the current function's caller, you can issue

    > source @caller
        103 void __dios_main( int l, int argc, char **argv, char **envp ) noexcept {
        104     __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask );
        105     __dios_trace_t( "Dios started!" );
        106     __dios::runCtors();
        107     int res;
        108     switch (l) {
        109     case 0:
        110         __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_None );
    >>  111         res = main();
        112         break;
        113     case 2:
        114         __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_None );
        115         res = main( argc, argv );
        116         break;
        [...]

To inspect data, we can instead use `show` and `inspect`. We have mentioned
`$frame` earlier: there is, in fact, a number of variables set by `sim`. The
most interesting of those is `$_` which normally refers to the (usually) most
interesting object at hand, typically the executing frame. By default, `show`
will print the content of `$_` (like many other commands). However, when we
pass an explicit parameter to these commands, the difference between `show` and
`inspect` becomes apparent: the latter also sets `$_` to its parameter. This
makes `inspect` more suitable for exploring more complex data, while `show` is
useful for quickly glancing at nearby values:

    > show
    @address:     heap* 901e2 0+0
    @pc:          code* 234 0
    @location:    program.c:13
    @symbol:      main
    tid:
        @type:    pthread_t
    related:      @caller

This is how a frame is presented when we look at it with `show`.
