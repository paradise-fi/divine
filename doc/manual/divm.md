# DiVM: A Virtual Machine for Verification

Programs in DIVINE are executed by a virtual machine (called DiVM). The machine
code executed by this virtual machine is an extension of the LLVM bitcode. The
LLVM part of this “machine language” is described in detail in
the [LLVM Documentation] [1]. The extensions of the language and the semantics
specific to DiVM are the subject of this chapter.

[1]: http://llvm.org/docs/LangRef.html

## Activation Frames

Unlike in a ‘traditional’ implementations of C, there is no continuous stack in
DiVM. Instead, each activation record (frame) is allocated from the heap and
its size is fixed. Allocations that are normally done at runtime from the stack
are instead done from the heap, using the `alloca` LLVM
instruction. Additionally, since LLVM bitcode is in partial SSA form, what LLVM
calls 'registers' are objects quite different from traditional machine
registers. The registers used by a given function are bound to the frame of
that function (they cannot be used to pass values around and they don't need to
be explicitly saved across calls). In the VM, this is realized by storing
registers (statically allocated) in the activation record itself, along with
DiVM-specific "program counter" register (this is an actual register, but is
saved across calls automatically by the VM, see also [Control Registers] below)
and a pointer to the caller's activation frame. The header of the activation
record has a C-compatible representation, available as `struct _VM_Frame` in
`sys/divm.h`.

## Control Registers {#sec:control}

The state of the VM consists of two parts, a set of *control registers* and the
*heap* (structured memory). All available control registers are described by
`enum _VM_ControlRegister` defined in `sys/divm.h` and can be manipulated
through the `__vm_control` hypercall (see also [@sec:hypercalls]]
below). Please note that control registers and LLVM registers (SSA values) are
two different things. The control registers are of two types, holding either an
integer or a pointer. There only integer register, `_VM_CR_Flags`, is used as a
bitfield.

Four control registers govern address translation and normal execution:

  * `_VM_CR_Constants` contains the base address of the heap object (see [Heap]
    below) used by the VM to hold constants
  * `_VM_CR_Globals` is the base address of the heap object where global
    variables are stored
  * `_VM_CR_Frame` points to the currently executing activation frame
  * `_VM_CR_PC` is the program counter

Additional 4 registers are concerned with scheduling and interrupt control (for
details, see [@sec:scheduling] below):

  * `_VM_CR_Scheduler` is the entry address of the scheduler
  * `_VM_CR_State` is the object holding persistent state of the scheduler
  * `_VM_CR_IntFrame` the address of the interrupted frame (see also [@sec:scheduling])
  * `_VM_CR_Flags` is described in more detail below

Finally, there's `_VM_CR_FaultHandler` (the address of the fault handler, see
[@sec:faults]) and 4 user registers (`_VM_CR_User1` through `_VM_CR_User4`) of
unspecified types: they can hold either a 64 bit integer or a pointer. The VM
itself never looks at the content of those registers.

The flags register (`_VM_CR_Flags`) is further broken down into individual
bits, described by `enum _VM_ControlFlags`, again defined in
`sys/divm.h`. These are:

  * `_VM_CF_Mask`, if set, blocks *all* interrupts
  * `_VM_CF_Interrupted`, if set, causes an immediate interrupt (unless
    `_VM_CF_Mask` is also set, in which case the interrupt happens as soon as
    `_VM_CF_Mask` is lifted).
  * `_VM_CF_KernelMode` which indicates whether the VM is running in user or
    kernel mode; this bit is set by the VM when `__boot` or the scheduler is
    entered and whenever an interrupt happens; the bit can be cleared (but not
    set) via `__vm_control`

The remaining 3 flags indicate properties of the resulting edge in the state
space (see also [State Space of a Program]). These may be set by the program
itself or by a special monitor automaton, a feature of DiOS which enables
modular specification of non-trivial (sequence-dependent) properties. These 3
flags are reset by the VM upon entering the scheduler (see
[@sec:scheduling]). The edge-specific flags are:

  * `_VM_CF_Error` indicates that an error -- a safety violation -- ought to be
    reported (a good place to set this is the *fault handler*, see [@sec:faults]),
  * `_VM_CF_Accepting` indicates that the edge is accepting, under a Büchi
    acceptance condition (see also [ω-Regular Properties and LTL]).
  * `_VM_CF_Cancel` indicates that this edge should be abandoned (it will not
    become a part of the state space and neither will its target state, unless
    also reachable some other way)

## Heap

The entire *persistent* state of the VM is stored in the heap. The heap is
represented as a directed graph of objects, where pointers stored in those
objects act as the edges of the graph. For each object, in addition to the
memory corresponding to that object, a supplemental information area is
allocated transparently by the VM for tracking metadata, like which bytes in
the object are initialised (defined) and a list of addresses in the object
where pointers are stored.

Activation frames, global variables and even constants are all stored in the
heap. The heap is also stored in a way that makes it quite efficient (both
time- and memory-wise) for the VM to take snapshots and store them. This is how
model checking and reversible debugging is realized in DIVINE.

## The Hypercall Interface

The interface between the program and the VM is based on a small set of
*hypercalls* (a list is provided in [@tbl:hypercalls]). This way, unlike pure
LLVM, the DiVM language is capable of encoding an operating system, along with
a syscall interface and all the usual functionality included in system
libraries.

| Hypercall       | Description                                                |
|:----------------|:-----------------------------------------------------------|
| `obj_make`      | Create a new object in the memory graph of the program     |
| `obj_free`      | Explicitly destroys an object in the memory graph          |
| `obj_size`      | Obtain the current size of an object                       |
| `obj_resize`    | Efficiently resize an object (optional)                    |
| `obj_shared`    | Mark an object as *shared* for τ reduction (optional)      |
| `trace`         | Attach a piece of data to an edge in the execution graph   |
| `interrupt_mem` | Mark a memory-access-related interrupt point               |
| `interrupt_cfl` | Mark a control-flow-related interrupt point                |
| `choose`        | Non-deterministic choice (a fork in the execution graph)   |
| `control`       | Read or manipulate machine control registers               |

: A list of hypercalls provided by DiVM. {#tbl:hypercalls}

## Scheduling {#sec:scheduling}

The DIVINE VM has no intrinsic concept of threads or processes. Instead, it
relies on an "operating system" to implement such abstractions and the VM
itself only provides the minimum support necessary. Unlike with "real"
computers, a system required to operate DiVM can be extremely simple,
consisting of just 2 C functions (one of them is `__boot`, see [Boot Sequence]
below). The latter of those is the scheduler, the responsibility of which is to
organize interleaving of threads in the program to be verified. However, the
program may not use threads but some other form of concurrency -- it is up to
the scheduler, which may be provided by the user, to implement the correct
abstractions.

From the point of view of the state space (cf. [State Space of a Program]), the
scheduler decides what the successors of a given state are. When DIVINE needs
to construct successors to a particular state, it executes the scheduler in
that state; the scheduler decides which thread to run (usually with the help of
the non-deterministic choice operator) and transfers control to that thread (by
changing the value of the `_VM_CR_Frame` control register, i.e. by instructing
DIVINE to execute a particular activation frame). The VM then continues
execution in the activation frame that the scheduler has chosen, until it
encounters an *interrupt*. When DIVINE loads a program, it annotates the
bitcode with *interrupt points*, that is, locations in the program where
threads may need to be re-scheduled. When such a point is encountered, the VM
sets the `_VM_CF_Interrupted` bit in `_VM_CR_Flags` and unless `_VM_CF_Mask` is
in effect, an interrupt is raised immediately.

Upon an interrupt, the values of `_VM_CR_IntFrame` and `_VM_CR_Frame` are
swapped, which usually means that the control is transferred back to the
scheduler, which can then read the address of the interrupted frame from
`_VM_CR_IntFrame` (this may be a descendant or a parent of the frame that the
scheduler originally transferred control to, or may be a null pointer if the
activation stack became empty).

Of course, the scheduler needs to store its state -- for this purpose, it must
use the `_VM_CR_State` register, which is set by `__boot` to point to a
particular heap object. This heap object can be resized by calling
`__vm_obj_resize` if needed, but the register itself is read-only after
`__boot` returns. The object can be used to, for example, store pointers to
activation frames corresponding to individual threads (but of course, those may
also be stored indirectly, behind a pointer to another heap object). In other
words, the object pointed to by `_VM_CR_State` serves as the *root* of the
heap.

## Faults {#sec:faults}

An important role of DIVINE is to detect errors -- various types of safety
violations -- in the program. For this reason, it needs to interpret the
bitcode as strictly as possible and report any problems back to the
user. Specifically, any dangerous operations that would normally lead to a
crash (or worse, a security vulnerability) are caught and reported as *faults*
by the VM. The fault types that can arise are the following (enumerated in
`enum _VM_Fault` in `divine.h`):

  * `_VM_F_Arithmetic` is raised when the program attempts to divide by zero
  * `_VM_F_Memory` is raised on attempts at illegal memory access and related
    errors (out-of-bounds loads or writes, double free, attempts to dereference
    undefined pointers)
  * `_VM_F_Control` is raised on control flow errors -- undefined conditional
    jumps, invalid call of a null or invalid function pointer, wrong number of
    arguments in a `call` instruction, `select` or `switch` on an undefined
    value or attempt to execute the `unreachable` LLVM instruction
  * `_VM_F_Hypercall` is raised when an invalid hypercall is attempted (wrong
    number or type of parameters, undefined parameter values)

When a fault is raised, control is transferred to a user-defined *fault
handler* (a function the address of which is held in the `_VM_CR_FaultHandler`
control register). Out of the box, DiOS (see [DiOS, DIVINE's Operating System])
provides a configurable fault handler. If a fault handler is set, faults are
not fatal (the only exception is a double fault, that is, a fault that occurs
while the fault handler itself is active). The fault handler, possibly with
cooperation from the scheduler (see [@sec:scheduling]), can terminate the
program, or raise the `_VM_CF_Error` flag, or take other appropriate actions.

The handler can also choose to continue with execution despite the fault, by
transferring control to the activation frame and program counter value that are
provided by the VM for this purpose. (Note: this is necessary, because the
fault might occur in the middle of evaluating a control flow instruction, in
which case, the VM could not finish its evaluation. The continuation passed to
the fault handler is the best estimate by the VM on where the execution should
resume. The fault handler is free to choose a different location.)

## Boot Sequence

The virtual machine explicitly recognizes two modes of execution: privileged
(kernel) mode and normal, unprivileged user mode. When the VM is started, it
looks up a function named `__boot` in the bitcode file and starts executing
this function, in kernel mode. The responsibility of this function is to set up
the operating system and set up the VM state for execution of the user
program. There are only two mandatory steps in the boot process: set the
`_VM_CR_Scheduler` and the `_VM_CR_State` control registers (see above). An
optional (but recommended) step is to inform the VM (or more specifically, any
debugging or verification tools outside the VM) about the C/C++ *type* (or
DWARF type, to be precise, as this is also possible for non-C languages)
associated with the OS state. This is accomplished by an appropriate
`__vm_trace` call (see also below).

## Memory Management Hypercalls

Since LLVM bitcode is not tied to a memory representation, its apparatus for
memory management is quite limited. Just like in C, `malloc`, `free`, and
related functions are provided by libraries, but ultimately based on some
lower-level mechanism, like, for example, the `mmap` system call. This is often
the case in POSIX systems targeting machines with a flat-addressed virtual
memory system: `mmap` is tailored to allocate comparatively large, contiguous
chunks of memory (the requested size must be an integer multiple of hardware
page size) and management of individual objects is done entirely in user-level
code. Lack of any per-object protections is also a source of many common
programming errors, which are often hard to detect and debug.

It is therefore highly desirable that a single object obtained from `malloc`
corresponds to a single VM-managed and properly isolated object. This way,
object boundaries can easily be enforced by the model checker, and any
violations reported back to the user. This means that, instead of subdividing
memory obtained from `mmap`, the `libc` running in DiVM uses `obj_make` to
create a separate object for each memory allocation. The `obj_make` hypercall
obtains the object size as a parameter and writes the address of the newly
created object into the corresponding LLVM register (LLVM registers are stored
in memory, and therefore participate in the graph structure; this is described
in more detail in [@sec:frames]). Therefore, the newly created object is
immediately and atomically connected to the rest of the memory graph.

The standard counterpart to `malloc` is `free`, which returns memory, which is
no longer needed by the program, into the pool used by `malloc`. Again, in
DiVM, there is a hypercall -- `obj_free` -- with a role similar to that of
standard `free`. In particular, `obj_free` takes a pointer as an argument, and
marks the corresponding object as *invalid*. Any further access to this object
is a *fault* (faults are described in more detail in [@sec:faults]). The
remaining hypercalls in the `obj_` family exist to simplify bookkeeping and are
not particularly important to the semantics of the language.

## Non-deterministic Choice and Counterexamples {#sec:nondeterminism}

It is often the case that the behaviour of a program depends on outside
influences, which cannot be reasonably described in a deterministic fashion and
wired into the SUT. Such influences are collectively known as the
*environment*, and the effects of the environment translate into
non-deterministic behaviour. A major source of this non-determinism is thread
interleaving -- or, equivalently, the choice of which thread should run next
after an interrupt.

In our design, all non-determinism in the program (and the operating system) is
derived from uses of the `choose` hypercall (which non-deterministically
returns an integer between 0 and a given number). Since everything else in the
SUT is completely deterministic, the succession of values produced by calls to
`choose` specifies an execution trace unambiguously. This trait makes it quite
simple to store counterexamples and other traces in a tool-neutral,
machine-readable fashion. Additionally, hints about which interrupts fired can
be included in case the counterexample consumer does not wish to reproduce the
exact interrupt semantics of the given VM implementation.

Finally, the `trace` hypercall serves to attach additional information to
transitions in the execution graph. In particular, this information then
becomes part of the counterexample when it is presented to the user. For
example, the `libc` provided by DIVINE uses the `trace` hypercall in the
implementation of standard IO functions. This way, if a program prints
something to its standard output during the violating run, this output becomes
visible in the counterexample.

## Debug Mode {#sec:debugmode}

The virtual machine has a special *debug* mode which allows instrumentation of
the program under test with additional tracing or other information gathering
functionality. This is achieved by a special `dbg.call` instruction, which is
emitted by the bitcode loader whenever it encounters an LLVM `call` instruction
that targets a function annotated with `divine.debugfn`. For instance, the DiOS
tracing functions (`__dios_trace*`) carry this annotation. The virtual machine
has two operation modes differentiated by how they treat `dbg.call`
instructions. In the *debug allowed* mode, the instruction is executed and for
the duration of the call, the VM enters a *debug mode*. In the other mode
(debug forbidden), the instruction is simply ignored.

Typically, verification (state space generation) would be done in the *debug
forbidden* operation mode, while the counter-example trace would be obtained or
replayed in the *debug allowed* mode. To make this approach feasible, there are
certain limitations on the behaviour of a function called using `dbg.call`:

   * when in *debug mode* (i.e. when already executing a `dbg.call`
     instruction), all further `dbg.call` instructions are *ignored*
   * faults in debug mode always cause the execution of `dbg.call` to be
     abandoned and the program continues executing as if the `dbg.call`
     returned normally
   * interrupts and the `vm_choose` hypercall are forbidden in `dbg.call` and
     both will cause a fault (and hence abandonment of the call)
