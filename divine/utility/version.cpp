#include <divine/utility/version.h>
#include <wibble/string.h>
#include <string>
#include <sstream>

#if OPT_MPI
#include <mpi.h>
#ifdef HAVE_OMPI_VERSION
#include <ompi/version.h>
#endif
#endif

extern const char *DIVINE_SOURCE_SHA, *DIVINE_BUILD_DATE,
                  *DIVINE_RELEASE_SHA, *DIVINE_VERSION;

namespace divine {

static std::string version;

const char *buildDateString() {
    return DIVINE_BUILD_DATE;
}

const char *versionString() {
    if ( std::string(DIVINE_RELEASE_SHA) == DIVINE_SOURCE_SHA )
        version = std::string( DIVINE_VERSION );
    else
        version = std::string( DIVINE_VERSION ) + "+" + DIVINE_SOURCE_SHA;
    return version.c_str();
}

std::vector< ReportLine > BuildInfo::report() const {
    std::stringstream ss;
#if OPT_MPI
        int vers, subvers;
        std::string impl = "unknown implementation";
        MPI::Get_version( vers, subvers );
#ifdef HAVE_OMPI_VERSION
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
