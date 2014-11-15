#include <string>
#include <stdexcept>

#include <bricks/brick-assert.h>

#ifndef DIVINE_UTILITY_DIE
#define DIVINE_UTILITY_DIE
namespace divine {
struct DieException : std::runtime_error {
    DieException( std::string message, int exitcode = 1 ) :
        std::runtime_error( message ), exitcode( exitcode )
    { }

    int exitcode;
};

inline static void die( std::string msg, int exitcode = 1 ) {
    throw DieException( msg, exitcode );
}
}

#endif // DIVINE_UTILITY_DIE
