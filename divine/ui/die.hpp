#pragma once
#include <brick-except>

namespace divine {

struct DieException : brick::except::Error { using brick::except::Error::Error; };

inline static void die( std::string msg, int exitcode = 1 )
{
    throw DieException( msg, exitcode );
}
}
