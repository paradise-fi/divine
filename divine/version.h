#include <divine/context.sha1.h>

// Set the version information here.
#define DIVINE_VERSION_ID "1.2.1"
// #define DIVINE_BRANCH 
#define DIVINE_RELEASE

// Defined in terms of above. Do not touch.
#ifdef DIVINE_BRANCH
#define DIVINE_BRANCH_P DIVINE_BRANCH "+"
#else
#define DIVINE_BRANCH_P ""
#endif

#ifdef DIVINE_RELEASE
#define DIVINE_VERSION DIVINE_VERSION_ID
#else
#define DIVINE_VERSION DIVINE_VERSION_ID "+" DIVINE_BRANCH_P DIVINE_CONTEXT_SHA
#endif
