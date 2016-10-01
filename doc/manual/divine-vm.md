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
be explicitly saved across calls).

Control Registers
-----------------

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

