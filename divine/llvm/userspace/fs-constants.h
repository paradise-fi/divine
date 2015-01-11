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

namespace flags {

enum class Open {
    NoFlags     =  0,
    Read        =  1,
    Write       =  2,
    ReadWrite   =  3,
    Create      =  4,
    Excl        =  8,
    TmpFile     = 16,
};

enum class Access {
    OK          = 0,
    Read        = 1,
    Write       = 2,
    Execute     = 4,
};

} // namespace flags

using utils::operator|;
template< typename T >
using Flags = utils::StrongEnumFlags< T >;

} // namespace fs
} // namespace divine

#endif
