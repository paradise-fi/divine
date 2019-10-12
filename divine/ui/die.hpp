#pragma once
#include <brick-except>

namespace divine {

struct DieException : brq::error { using brq::error::error; };

inline static void die( std::string msg, int exitcode = 1 )
{
    throw DieException( msg, exitcode );
}
}
