#include <dios/config/context.hpp>
#include <dios/proxy/passthru.h>

namespace __dios
{
    using Context = Upcall< Fault< Scheduler < fs::PassThrough < Base > > > >;
}

#include <dios/config/common.hpp>
