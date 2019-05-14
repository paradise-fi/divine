#include <dios/config/context.hpp>

namespace __dios
{
    using Context = Upcall< fs::VFS< Fault< SyncScheduler< Base > > > >;
}

#include <dios/config/common.hpp>
