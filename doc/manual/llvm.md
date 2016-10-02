Model Checking C and C++ Code via LLVM Bitcode
==============================================

The traditional "explicit-state" model checking practice is not widely adopted
in the programming community, and vice-versa, use of mainstream programming
languages is not a common practice in the model checking community. Hence,
model checking of systems expressed as C programs comes with some novelties for
both kinds of users.

First of all, the current main application of DIVINE is verification of safety
(and some liveness) properties of asynchronous, shared-memory programs. The
typical realisation of such asynchronous parallelism is programming with
threads and shared memory. Often for performance and/or familiarity reasons,
programming with threads, shared memory and locking is the only viable
alternative, even though the approach is fraught with difficulties and
presents many pitfalls that can catch even expert programmers unaware -- not to
say novices. Sadly, resource locking is inherently non-compositional, hence
there is virtually no way to provide a reliable yet powerful abstraction, all
that while retaining speed and scalability.

Despite all its shortcomings, lock-based programming (or alternatively,
lock-free shared memory programming, which is yet more difficult) is becoming
more prevalent. Model checking provides a powerful tool to ascertain
correctness of programs written with locks around concurrent access to shared
memory. Most programmers will agree that bugs that show up rarely and are
nearly impossible to reproduce are the worst kind to deal with. Sadly, most
concurrency bugs are of this nature, since they arise from subtle interactions
of nondeterministically scheduled threads of execution. A test-case may work 99
times out of 100, yet the 100th time die due to an unfathomable invalid memory
access. Even sophisticated modern debugging tools like `valgrind` are often
powerless in this situation.

This is where DIVINE can help, since it systematically and efficiently explores
all relevant execution orderings, discovering even the subtlest race
conditions, restoring a crucially important property of bugs: reproducibility.
If you have ever changed a program and watched the test-case run in a loop for
hundreds of iterations, wondering if the bug is really fixed, or it just
stubbornly refuses to crash the program... well, DIVINE is the tool you have
been waiting for.

Of course, there is a catch. Model checking is computationally intensive and
memory-hungry. While this is usually not a problem with comparably small unit
tests, applying model checking to large programs may not be feasible --
depending on your use-case, and on the amount of memory and time that you have.

With the universal LLVM backend, DIVINE can support a wide range of compiled
programming languages. However, out of the box, language-specific support is
only provided for C and C++. A fairly complete ISO C runtime library is
provided as part of DIVINE, with appropriate hooks into DIVINE, as well as an
implementation of the `pthread` specification, i.e. the POSIX threading
interface. Additionally, an implementation of the standard C++ library is
bundled with DIVINE. Besides the standard library, DIVINE also provides an
adapted version of the *runtime* library required by C++ programs to implement
runtime type identification and exception handling; both are fully supported by
DIVINE.

If your language interfaces with the C library, the `libc` part of language
support can be re-used transparently. However, currently no other platform glue
is provided for other languages. Your mileage may vary. Data structure and
algorithmic code can be very likely processed with at most trivial additions to
the support code, in any language that can be compiled into LLVM bitcode.

Compiling Programs
------------------

The first step to take when you want to use DIVINE for C/C++ verification is to
compile your program into LLVM bitcode and link it against the DIVINE-provided
runtime libraries. When you issue `divine cc program.c`, `divine` will compile
the runtime support and your program and link them together automatically,
using a builtin clang-based compiler.  The `cc` subcommand accepts a wide array
of traditional C compiler flags like `-I`, `-std`, `-W` and so on.

Alternatively, you can pass a single-file C or C++ program directly to other
`divine` commands, like `verify` or `sim`, in which case the program will be
compiled and linked transparently.

Limitations
-----------

When DIVINE interprets your program, it does so in a very strictly controlled
environment, so that every step is fully reproducible. Hence, no real IO is
allowed; the program can only see its own memory.

While the `pthread` interface is fully supported, the `fork` system call is
not: DIVINE cannot verify multi-process systems at this time. Likewise, C++
exceptions are well-supported but the C functions `setjmp` and `longjmp` are
not yet implemented.

The interpreter simulates an "in order" CPU architecture, hence any possible
detrimental effects of instruction and memory access reordering won't be
detected. Again, an improvement in this area is planned for a future release.

State Space of a Program
------------------------

Internally, DIVINE constructs the state space of your program -- an oriented
graph, whose vertices represent various states your program can reach, i.e. the
values of all its mapped memory locations, the values in all machine registers,
and so on. DIVINE normally doesn't store (or construct) all the possible states
-- only a relevant subset. In a multi-threaded program, it often happens that
more than one thread can run at once, i.e. if your program is in a given state,
the next state it will get into is determined by chance -- it all hinges on
which thread gets to run. Hence, some (in fact, most) states in a parallel
program can proceed to multiple different configurations, depending on a
scheduling choice. In such cases, DIVINE has to explore both such "variant"
successors to determine the behaviour of the program in all possible scenarios.

You can visualise the state space DIVINE explores by using `divine draw` --
this will show how the "future" of your program branches through various
configurations, and how it converges back into a common point -- or, if its
behaviour had changed depending on scheduling, diverges into multiple different
outcomes.

Non-Deterministic Choice
------------------------

Scheduling is not the only source of non-determinism in typical
programs. Interactions with the environment can have different outcomes, even
if the internal state of the program is identical. A typical example would be
memory allocation: the same call in the same context could either succeed or
fail, depending on the conditions outside the control of the program. In
addition to failures, the normal input of the program (files, network, user
input on the terminal) are all instances of non-determinism due to external
influences. The sum of all such external behaviours that affect the outcome of
the program is called the environment (this includes the scheduling of threads
done by the operating system and/or CPU). When testing, the environment is
controlled to some degree: the inputs are usually fixed, but eg. thread
scheduling is (basically) random -- it changes with every test
execution. Resource exhaustion can be simulated in a test environment to some
degree. In a model checker, the environment is controlled much more strictly:
when a test case is run through a model checker, it will come out the same
every time.

ω-Regular Properties and LTL
----------------------------
