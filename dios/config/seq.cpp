#include <dios/config/context.hpp>

namespace __dios
{
    using Context = Upcall< WithFS< sched_null< Base > > >;
}

#include <dios/config/common.hpp>
