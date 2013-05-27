#include <divine/utility/version.h>
#include <string>

#ifdef O_MPI
#include <mpi.h>
#ifdef O_OMPI_VERSION
#include <ompi/version.h>
#endif
#endif

extern const char *DIVINE_SOURCE_SHA;
extern const char *DIVINE_BUILD_DATE;

// You need to update the version number *and* the SHA to current release one.
#define DIVINE_VERSION "2.96"
#define DIVINE_RELSTATE " (3.0 RC 1)"
#define DIVINE_RELEASE_SHA "6d9241dffcbc3cbf5b5934427a7fb53e472934a7"

namespace divine {

static std::string version;

const char *buildDateString() {
    return DIVINE_BUILD_DATE;
}

const char *versionString() {
    if ( std::string(DIVINE_RELEASE_SHA) == DIVINE_SOURCE_SHA )
        version = std::string( DIVINE_VERSION ) + DIVINE_RELSTATE;
    else
        version = std::string( DIVINE_VERSION "+" ) + DIVINE_SOURCE_SHA;
    return version.c_str();
}

std::ostream &operator<<( std::ostream &o, BuildInfo )
{
        o << "Version: " << versionString() << std::endl;
        o << "Build-Date: " << buildDateString() << std::endl;
        o << "Pointer-Width: " << 8 * sizeof( void* ) << std::endl;
#ifdef NDEBUG
        o << "Debug: disabled" << std::endl;
#else
        o << "Debug: enabled" << std::endl;
#endif
        o << "Compile-Flags: " << divineCompileFlags << std::endl;

#ifdef O_MPI
        int vers, subvers;
        std::string impl = "unknown implementation";
        MPI::Get_version( vers, subvers );
#ifdef OMPI_VERSION
        impl = std::string( "OpenMPI " ) + OMPI_VERSION;
#endif
        o << "MPI-Version: " << vers << "." << subvers << " (" << impl << ")" << std::endl;
#else
        o << "MPI-Version: n/a" << std::endl;
#endif
        return o;
}

}
