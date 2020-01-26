#include <dios/lava/mstring.hpp>
#include <dios/lava/constant.hpp>
#include <dios/lava/unit.hpp>
#include "semilattice.hpp"

namespace __lamp
{
    using namespace __lava;
    using mstring = __lava::mstring< constant, constant >;

    struct constring
    {
        using doms = domain_list< constant, mstring, unit >;
        using top = unit;

        using scalar_lift_dom = constant;
        using scalar_any_dom = unit;
        using array_lift_dom = mstring;
        using array_any_dom = mstring;

        static constexpr int join( int a, int b ) noexcept
        {
            auto mstring_idx = doms::idx< mstring >;
            auto unit_idx = doms::idx< unit >;

            if ( a == unit_idx || b == unit_idx )
                return unit_idx;

            if ( a == mstring_idx || b == mstring_idx )
                return mstring_idx;
            else
                return doms::idx< constant >;
        }
    };

    using meta_domain = semilattice< constring >;
}

#include "wrapper.hpp"
