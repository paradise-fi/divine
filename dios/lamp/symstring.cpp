#include <dios/lava/mstring.hpp>
#include <dios/lava/term.hpp>
#include "semilattice.hpp"

namespace __lamp
{
    using namespace __lava;
    using mstring = __lava::mstring< term, term >;

    struct symstring
    {
        using doms = domain_list< term, mstring >;

        using scalar_lift_dom = term;
        using scalar_any_dom = term;
        using array_lift_dom = mstring;
        using array_any_dom = mstring;

        static constexpr int join( int a, int b ) noexcept
        {
            auto mstring_idx = doms::idx< mstring >;

            if ( a == mstring_idx || b == mstring_idx )
                return mstring_idx;
            else
                return doms::idx< term >;
        }
    };

    using meta_domain = semilattice< symstring >;
}

#include "wrapper.hpp"
