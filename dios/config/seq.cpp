#include <dios/config/context.hpp>
#include <dios/sys/sched_null.hpp>

namespace __dios
{
    using Context = Upcall< WithFS< sched_null< Base > > >;
}

#include <dios/config/common.hpp>
