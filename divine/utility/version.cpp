#include <divine/utility/version.h>
#include <string>
#include <sstream>

#ifdef O_MPI
#include <mpi.h>
#ifdef O_OMPI_VERSION
#include <ompi/version.h>
#endif
#endif

extern const char *DIVINE_SOURCE_SHA;
extern const char *DIVINE_BUILD_DATE;

// You need to update the version number *and* the SHA to current release one.
#define DIVINE_VERSION "3.0.90"
#define DIVINE_RELSTATE " (3.1 alpha 1)"
#define DIVINE_RELEASE_SHA "a3052703fc73ab72ec4645cb38a04edabd77d86f"

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

std::vector< ReportLine > BuildInfo::report() const {
    std::stringstream ss;
#ifdef O_MPI
        int vers, subvers;
        std::string impl = "unknown implementation";
        MPI::Get_version( vers, subvers );
#ifdef OMPI_VERSION
        impl = std::string( "OpenMPI " ) + OMPI_VERSION;
#endif
        ss << vers << "." << subvers << " (" << impl << ")";
#else
        ss << "n/a";
#endif

    return { { "Version", versionString() },
             { "Build-Date", buildDateString() },
             { "Pointer-Width", std::to_string( 8 * sizeof( void* ) ) },
#ifdef NDEBUG
             { "Debug", "disabled" },
#else
             { "Debug", "enabled" },
#endif
             { "Compile-Flags", divineCompileFlags },
             { "MPI-Version", ss.str() }
           };
}

}
