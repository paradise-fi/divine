An Introduction to Using DIVINE
===============================

In this section, we will give a short example on how to invoke DIVINE and its
various functions. We will use a small C program (consisting of a single
compile unit) as an example in this section, along with a few simple
properties.

System Modelling Basics
-----------------------

We will start with a simple C program with 2 threads and a single shared
variable:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.C .numberLines}
#include <pthread.h>
#include <assert.h>

int i = 33;

void* thread( void *x )
{
    i ++;
    return NULL;
}

int main()
{
    pthread_t tid;
    pthread_create( &tid, NULL, thread, NULL );

    i ++;

    pthread_join( tid, NULL );
    assert( i == 35 );
    return i;
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

(Yes, this program clearly has a bug in it, a race condition on the update of
`i`. We will deal with that later.)

The program as it is can be compiled using your system's C compiler just fine
(assuming access to `pthread.h`) and you can try running it. To use DIVINE, we
will need to compile it a little differently:

    $ divine cc program.c

After a short while, it will produce a file, `program.bc`. This file contains
LLVM bitcode of our program, and of the runtime environment. We can learn a
little more about the program using `divine info`:

    $ divine info program.bc
    (TODO: divine info is not yet implemented in DIVINE 4)

We can analyse the bitcode file a little more, actually exploring its entire
state space, measuring it (we won't be asking about any properties of the
program yet):

    $ divine metrics program.bc
    (TODO: divine metrics is not yet implemented in DIVINE 4)

We have learned that our program has NN unique states and there are NN
transitions between these states. By default, DIVINE tries to use the most
efficient reduction available which is still exact (i.e. it will not use
hash-compaction or other approximation techniques unless explicitly enabled).
We can try turning off those reductions to see what happens:

    $ divine metrics program.bc --no-reduce
    (TODO: divine metrics is not yet implemented in DIVINE 4)

Property Verification & Counter-Example Analysis
------------------------------------------------

Now let's turn to some more interesting properties. With the above program, the
first thing that comes to mind is asking DIVINE whether the assertion in the
program can ever be violated:

    $ divine verify program.bc
    loading /lib/libc.bc... linking... done
    loading /lib/libcxxabi.bc... linking... done
    loading /lib/libcxx.bc... linking... done
    loading /lib/libdios.bc... linking... done
    compiling program.c
    annotating bitcode... done
    computing RR... constants... done
    found 181 states and 314 edges in 0:00, averaging inf states/s
    
    found an error:
    Dios started!
    Pthread_join
    FAULT: __vm_fault called
    VM Fault
    Backtrace:
    1: main
    2: __dios_main
    
    choices made: 0 0 0 1 1 0 0 1 0 0 1 1 0 0 0 0 0 0 1 1 0 0
    backtraces in the error state:
    @address: heap* 902b4 0+0
    @pc: code* 1d4 4f
    @location: /divine/src/dios/fault.cpp:34
    @symbol: __dios::Fault::handler(_VM_Fault, _VM_Frame*, void (*)())
    
    @address: heap* 901e1 0+0
    @pc: code* 234 11
    @location: program.c:20
    @symbol: main
    
    @address: heap* 901bb 0+0
    @pc: code* 1de 26
    @location: /divine/src/dios/main.cpp:111
    @symbol: __dios_main

Here, as we have anticipated, DIVINE tells us that in fact, the assertion *can*
be violated and gave us an indication of what exactly went wrong. In
particular, the backtrace reveals that `program.c:20` was active during the
fault, which is exactly our assertion.

To understand what led to the assertion failure, however, we will need to
analyse the counterexample in more detail. For this purpose, DIVINE provides an
interactive debugger, available as `divine sim`.
