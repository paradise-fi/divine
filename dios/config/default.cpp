#include <dios/config/context.hpp>

namespace __dios
{
    using Context = Upcall< WithProc< Scheduler< Base > > >;
}

#include <dios/config/common.hpp>
