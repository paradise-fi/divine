## DiOS VFS (12. 6. 2019)

DiOS has support to simulate operations with files and directories using an
virtual, in-memory filesystem. This allows verification of programs which read
or write to filesystem, provided all the inputs for the program can be created
beforehand. It can be useful for verification of parallel programs which use
filesystem primitives for communication (e.g., fifos).

VFS is part of the scheduler config stack (just under `Upcall` usually).

- main sources: `dios/vfs/*`
- aux sources:
  - `dios/include/sys/un.h`
  - `$BUILD_DIR/dios/include/sys/hostabi.h`

### Overview

The filesystem is modelled on Unix-like filesystems. Similarly, objects are
described by inodes (`vfs/inode.hpp`, `struct INode`). Unlike in Unix, `INode`
in DIVINE is actually a base type from which concrete types of files are derived
(`vfs/file.hpp`, `vfs/directory.hpp`), and each derived type also owns the
corresponding data.  INode implements `stat` which is used in the `stat`
syscall.  A `Node` is pointer to an `INode`.

An `error` helper, used in `INode` and throughout VFS is defined in
`vfs/flags.hpp`, it sets `ERRNO`. This file also defines flags for `open`.

### Filesystem Object Types

`vfs/file.hpp`, `vfs/directory.hpp`

Most filesystem types are base on Unix filesystems (symlinks, regular files,
directories, pipes), but there are some DiOS-specific object types:

- `VmTraceFile` -- write only, writes to trace/counterexample
- `VmBuffTraceFile` -- similar, but traces full lines
- `StandardInput` -- read-only, for stdin simulation

VFS has also support for Unix domain sockets, in particular stream and datagram
sockets: `vfs/socket.hpp`.

Overall read/write in VFS does not simulate short reads/writes.


### File Descriptors

`vfs/fd.hpp`

Reading from/writing to a file is done using a `FileDescriptor` struct, which
opens/closes the corresponding inode and keeps track of offset.

### VFS Manager

`dios/vfs/manager.h`

`template< typename Next > struct VFS : Syscall, Next`{.cpp}

This is the part of VFS which plugs into scheduler config stack. Let's
deconstruct it.

`dios/vfs/base.h`

- per-process data needed for the filesystem
  - umask, current directory, open file descriptors
- a base of the VFS which defines primitives on which VFS syscalls are build
- build VFS from an image

`dios/vfs/syscall.hpp` + `dios/vfs/syscalls.cpp`

- declaration and definition of filesystem-related syscalls.

Back to manager: `dios/vfs/manager.h`.

## Usage & Exercise

1.  Check which DIVINE options work with the filesystem.
2.  Create a program which reads a number from stdin and prints it to stdout. The
    program should fail if there is no input. Check program's behaviour in
    `divine check` & `divine exec -o nofail:malloc --virtual` with and without
    passing standard input to DIVINE.
