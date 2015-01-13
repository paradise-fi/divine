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

namespace flags {

enum class Open {
    NoFlags     =  0,
    Read        =  1,
    Write       =  2,
    ReadWrite   =  3,
    Create      =  4,
    Excl        =  8,
    TmpFile     = 16,
    Truncate    = 32,
};

enum class Access {
    OK          = 0,
    Read        = 1,
    Write       = 2,
    Execute     = 4,
};

enum class At {
    Undefined   = 0,
    NoFlag      = 1,
    RemoveDir   = 2,
};

} // namespace flags

using utils::operator|;
template< typename T >
using Flags = utils::StrongEnumFlags< T >;

} // namespace fs
} // namespace divine

#endif
