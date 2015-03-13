// -*- C++ -*- (c) 2015 Jiří Weiser

#include "fs-utils.h"

#ifndef _FS_CONSTANTS_H_
#define _FS_CONSTANTS_H_

namespace divine {
namespace fs {

enum class Seek {
    Undefined,
    Set,
    Current,
    End
};

const int CURRENT_DIRECTORY = -4;
const int PATH_LIMIT = 1023;
const int FILE_NAME_LIMIT = 255;
const int FILE_DESCRIPTOR_LIMIT = 1024;

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
    Invalid     = 4096,
};

enum class Access {
    OK          = 0,
    Execute     = 1,
    Write       = 2,
    Read        = 4,
    Invalid     = 8,
};

enum class At {
    NoFlags     =  0,
    Invalid     =  1,
    RemoveDir   =  2,
    EffectiveID =  4,
    SymFollow   =  8,
    SymNofollow = 16,
};

} // namespace flags

using utils::operator|;
template< typename T >
using Flags = utils::StrongEnumFlags< T >;

} // namespace fs
} // namespace divine

#endif
