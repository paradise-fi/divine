#include <dios/config/context.hpp>

namespace __dios
{
    struct Context : Upcall< WithFS< sched_null< Base > > > {};
}

#include <dios/config/common.hpp>
