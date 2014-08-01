#include <string>
#include <wibble/exception.h>
#include <wibble/test.h>

#ifndef DIVINE_UTILITY_DIE
#define DIVINE_UTILITY_DIE
namespace divine {
struct DieException : wibble::exception::Generic {
    DieException( std::string message, int exitcode = 1 ) :
        message( message ), exitcode( exitcode )
    { }

    const char* type() const throw() override {
        return "DieException";
    }
    std::string desc() const throw() override {
        return message;
    }

    std::string message;
    int exitcode;
};

inline static void die( std::string msg, int exitcode = 1 ) {
    throw DieException( msg, exitcode );
    assert_die();
}
}

#endif // DIVINE_UTILITY_DIE
