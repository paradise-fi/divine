#include <dios/config/context.hpp>

namespace __dios
{
    struct Context : Upcall< WithProc< Scheduler< Base > > > {};
}

#include <dios/config/common.hpp>
