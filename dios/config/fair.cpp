#include <dios/config/context.hpp>

namespace __dios
{
    using Context = Upcall< WithProc< FairScheduler< Base > > >;
}

#include <dios/config/common.hpp>
