#include <dios/config/context.hpp>

namespace __dios
{
    struct Context : Upcall< WithProc< FairScheduler< Base > > > {};
}

#include <dios/config/common.hpp>
