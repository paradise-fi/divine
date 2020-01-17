#include <dios/lava/unit.hpp>
#include <dios/lava/constant.hpp>
#include "semilattice.hpp"

namespace __lamp
{
    using namespace __lava;

    struct trivial
    {
        using doms = domain_list< unit, constant >;

        static constexpr int join( int a, int b ) noexcept
        {
            if ( a == doms::idx< constant > ) return b;
            if ( b == doms::idx< constant > ) return a;

            return doms::idx< unit >;
        }
    };

    using meta_domain = semilattice< trivial >;
}

#include "wrapper.hpp"
