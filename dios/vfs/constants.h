// -*- C++ -*- (c) 2015 Jiří Weiser

#include "utils.h"
#include "storage.h"

#ifndef _FS_CONSTANTS_H_
#define _FS_CONSTANTS_H_

namespace __dios {
namespace fs {

enum class FileTrace {
    NOTRACE,     /* ignore write in file */
    UNBUFFERED,  /* trace whenever possible */
    TRACE        /* trace on newline */
};

enum class Seek {
    Undefined,
    Set,
    Current,
    End
};

enum class SocketType {
    Stream,
    Datagram,
};

const int CURRENT_DIRECTORY = -100;
const int PATH_LIMIT = 1023;
const int FILE_NAME_LIMIT = 255;
const int FILE_DESCRIPTOR_LIMIT = 1024;
const int PIPE_SIZE_LIMIT = 1024;

namespace flags {

enum class Open {
    NoFlags     =    0,
    Read        =    1,
    Write       =    2,
    Create      =    4,
    Excl        =    8,
//    TmpFile     =   16,
    Truncate    =   32,
    NoAccess    =   64,
    Append      =  128,
    SymNofollow =  256,
    NonBlock    =  512,
    Directory   = 1024,
    FifoWait    = 2048,
    Invalid     = 4096,
};

enum class Message {
    NoFlags = 0,
    DontWait = 1,
    Peek = 2,
    WaitAll = 4,
};


} // namespace flags

using storage::operator|;
template< typename T >
using Flags = storage::StrongEnumFlags< T >;

} // namespace fs
} // namespace __dios

#endif
