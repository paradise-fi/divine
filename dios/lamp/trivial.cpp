#include <rst/unit.hpp>
#include <rst/constant.hpp>
#include <rst/semilattice.hpp>

namespace __rst
{
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

#include <rst/wrapper.cpp>
