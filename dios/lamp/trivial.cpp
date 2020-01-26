#include <dios/lava/unit.hpp>
#include <dios/lava/constant.hpp>
#include "semilattice.hpp"

namespace __lamp
{
    using namespace __lava;

    struct trivial
    {
        using doms = domain_list< unit, constant >;

        using scalar_lift_dom = constant;
        using scalar_any_dom = unit;
        using array_lift_dom = unit;
        using array_any_dom = unit;

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
