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

Scheduling
----------

Faults
------

Heap
----

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

