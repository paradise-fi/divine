#include <divine/version.h>
#include <string>

extern const char *DIVINE_SOURCE_SHA;
extern const char *DIVINE_BUILD_DATE;

// You need to update the version number *and* the SHA to current release one.
#define DIVINE_VERSION "2.5"
#define DIVINE_RELEASE_SHA "55fa350eea497c79291f524bb429d8fa71a1961f"

namespace divine {

static std::string version;

const char *buildDateString() {
    return DIVINE_BUILD_DATE;
}

const char *versionString() {
    if ( std::string(DIVINE_RELEASE_SHA) == DIVINE_SOURCE_SHA )
        return DIVINE_VERSION;
    version = std::string( DIVINE_VERSION "+" ) + DIVINE_SOURCE_SHA;
    return version.c_str();
}

}
