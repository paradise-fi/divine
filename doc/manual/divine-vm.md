The DIVINE Virtual Machine
==========================

Programs in DIVINE are executed by a virtual machine (DIVINE VM or DiVM for
short). The machine code executed by this virtual machine is the LLVM
bitcode. The details of the "machine language" are, therefore, described in the
[LLVM Documentation] [1].

[1]: http://llvm.org/docs/LangRef.html

Activation Frames
-----------------

Unlike in 'traditional' implementations of C, there is no continuous stack in
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
`divine.h`.

Control Registers
-----------------

The state of the VM consists of two parts, a set of *control registers* and the
*heap* (structured memory). All available registers are described by `enum
_VM_ControlRegister` defined in `divine.h` and can be manipulated through the
`__vm_control` hypercall (see also [The Hypercall Interface] below). The
registers are of two types, holding either an integer or a pointer. There are 2
integer registers, `_VM_CR_Flags`, which is used as a bitfield, and
`_VM_CR_User1` (the latter is not used or examined by the VM itself). The flags
register is further broken down into individual bits, described by `enum
_VM_ControlFlags`, again defined in `divine.h`. These are:

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
flags are reset by the VM upon entering the scheduler (see [Scheduling]). The
edge-specific flags are:

  * `_VM_CF_Error` indicates that an error -- a safety violation -- ought to be
    reported (a good place to set this is the *fault handler*, see [Faults]),
  * `_VM_CF_Accepting` indicates that the edge is accepting, under a Büchi
    acceptance condition (see also [ω-Regular Properties and LTL]).
  * `_VM_CF_Cancel` indicates that this edge should be abandoned (it will not
    become a part of the state space and neither will its target state, unless
    also reachable some other way)

Heap
----

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

Scheduling
----------

The DIVINE VM has no intrinsic concept of threads or processes. Instead, it
relies on an "operating system" to implement abstractions like those and the VM
itself only provides the minimum support neccessary. Unlike with "real"
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
swapped, usually entailing that the control is transferred back to the
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

Faults
------

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
    jumps, indirect calls a null function, wrong number of arguments in a
    `call` instruction, `select` or `switch` on an undefined value or attempt
    to execute the `unreachable` LLVM instruction
  * `_VM_F_Hypercall` is raised when an invalid hypercall is attempted (wrong
    number or type of parameters, undefined parameter values)

When a fault is raised, control is transferred to a user-defined *fault
handler* (a function the address of which is held in the `_VM_CR_FaultHandler`
control register). Out of the box, DiOS (see [DiOS, DIVINE's Operating System])
provides a configurable fault handler. If a fault handler is set, faults are
not fatal (the only exception is a double fault, that is, a fault that occurs
while the fault handler itself is active). The fault handler, possibly with
cooperation from the scheduler (see [Scheduling] above), can terminate the
program, or raise the `_VM_CF_Error` flag, or take other appropriate actions.

The handler can also choose to continue with execution despite the fault, by
transferring control to the activation frame and program counter value that are
provided by the VM for this purpose. (Note: this is neccessarry, because the
fault might occur in the middle of evaluating a control flow instruction, in
which case, the VM could not finish its evaluation. The continuation passed to
the fault handler is the best estimate by the VM on where the execution should
resume. The fault handler is free to choose a different location.)

Boot Sequence
-------------

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

The Hypercall Interface
-----------------------

