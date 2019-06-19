## System call proxies (19. 6. 2019)

Apart from VFS, DiOS provides two other related ways of handling file system
(though not limited to these) system calls: pass-through mode and replay mode.
The former leverages the host operating system to handle system calls, the
latter reproduces the results of a prior pass-through run. Both are implemented
as a part of DiOS configuration stack and reside under the scheduler component.

### The journey of a system call

Recall how one gets from a library function (e.g. `close`) to the actual
implementation in DiOS:

1. `close` -- libc function (dios/libc/unistd/syswrap.cpp)
2. (some unpadding)
3. `__dios::SysProxy::close` (declared in include/sys/syscall.h, implemented
   in config/common.hpp (included by all DiOS configurations), trap happens)
4. (unpadding to `ctx->close(...)`, where `ctx` is the configuration stack)
5. `dios::{stack component}::close`

### Pass-through mode
(dios/proxy/passthru.h)

In this mode, every system call has two distinct effects. First, every system
call made by the executed program is translated to a corresponding system call
to the host operating system (e.g. your computer's Linux). Second, the name of
the system call, along with all its parameters and results is recorded in a
binary format to a file called `passthrough.out`. The file can be later used in
replay mode.

As DiOS is actually a part of the interpreted bitcode, it cannot “escape” from
the interpreter (DiVM) to make system calls to the host operating system.
This is therefore facilitated by DiVM directly by providing a `__vm_syscall`
hypercall (basically a non-standard instruction of the bitcode). In this
hypercall's variable number of parameters, all necessary information is encoded
in a generic way so that DiVM does not need to know anything about specific
system calls. This information naturally includes number of the (host) system
call and the parameters, but also type of every parameter. With this knowledge,
DiVM can pass long parameters (such as strings) from the program's memory to the
host kernel and back (e.g. in the read and write system calls).

Note than the numbers of system calls do not need to match between DiOS and the
host operating system. Therefore, when translating the system calls, DiOS needs
to know which system calls have which numbers in the host kernel. That (and
other host-specific constants) are defined in a `hostabi.h` file. You will not
find that in the repository, however, as it is generated in compilation time by
the `hostabi.pl` script.

An example of how `__vm__syscall` is used can be found in proxy/trace.cpp, where
it is called directly to handle the `passthrough.out` file. Note that the system
call's return value is passed as an output parameter, while `__vm_syscall`'s
return value is the `errno` value after the system call has finished.

`fs::PassThrough` in proxy/passthru.h does the actual translation for
`__vm_syscall`. Its system-call-implementing functions take parameters wrapped
in dummy “tagging” structures, which are then used by the myriad of `parse`
functions to emit respective arguments for `__vm_syscall`. These tags include
`Mem<T>` for parameters passed by a pointer to a zero-terminated string (and
translates to the `_VM_SC_Mem` flag) or `Count<T>` for parameters passed also by
a pointer, but followed by another parameter stating the data's length (think
`write`).

On DiVM's side, these arguments are then translated to an appropriate call to
the libc `syscall` function with some data transfer from and to the interpreted
program's memory. See divine/vm/eval.cpp for implementation.

Pass-though mode is the default for `divine exec`. You can enforce it elsewhere
with `--dios-config proxy`, but things will probably break.

### Replay mode
(dios/proxy/replay.h)

When a correct `passthrough.out` file is available, one can use the replay mode.
After starting in this mode, DiOS loads the recorded system calls and any time
a system call is invoked, the replay proxy checks whether there exists an
appropriate record. If so, the proxy recreates its effects without affecting the
host. Each system call may be used in this manner only once per run.

You can enable this mode with the `--dios-config replay` flag.
