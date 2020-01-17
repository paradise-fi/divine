#include <brick-cons>
#include "constant.hpp"
#include "unit.hpp"

namespace __dios::rst::abstract
{
    template< typename... domains >
    struct domain_list : brq::cons_list_t< domains... >
    {
        template< typename type > static constexpr int idx = domain_list::template index_of< type >;
        template< int idx > using type = typename domain_list::template type_at< idx >;
    };

    template< typename sl >
    struct semilattice : tagged_array<> /* FIXME _VM_PT_Heap vs _VM_PT_Marked */
    {
        using doms = typename sl::doms;

        semilattice() noexcept : tagged_array<>( tagged_abstract_domain_t( -1 ) ) {}

        template< typename dom_t >
        semilattice( const dom_t &v ) noexcept : tagged_array<>( v )
        {
            tag() = doms::template idx< dom_t >;
        }

        static constexpr int join( int a ) noexcept { return a; }

        template< typename... args_t >
        static constexpr int join( int a, int b, args_t... args ) noexcept
        {
            return sl::join( a, join( b, args... ) );
        }

        template< typename to, typename from >
        static auto lift( const from &f ) noexcept
        {
            if constexpr ( std::is_same_v< from, to > )
                return f;
            else
                return to::lift( f );
        }

        template< typename op_t, int idx = 0, typename... args_t >
        static void in_domain( int dom, op_t op, const args_t & ... args ) noexcept
        {
            if constexpr ( idx < doms::size )
            {
                if ( idx == dom )
                {
                    if constexpr ( join( idx, doms::template idx< args_t > ... ) == idx )
                        return op( lift< typename doms::template type< idx > >( args ) ... );
                    else
                        __builtin_trap();
                }
                else
                    return in_domain< op_t, idx + 1 >( dom, op, args... );
            }
            else
                __builtin_trap();
        }

        template< typename op_t, int idx = 0 >
        static void cast_one( op_t op, const semilattice &v ) noexcept
        {
            if constexpr ( idx < doms::size )
            {
                using to_type = typename doms::template type< idx >;
                if ( v.tag() == idx )
                    op( static_cast< const to_type & >( static_cast< const tagged_array<> & >( v ) ) );
                else
                    cast_one< op_t, idx + 1 >( op, v );
            }
        }

        template< typename op_t >
        static void cast( op_t op ) noexcept { op(); }

        template< typename op_t, typename arg_t, typename... args_t >
        static void cast( op_t op, const arg_t &a, const args_t & ... args ) noexcept
        {
            auto rec = [&]( const auto &c ) noexcept
            {
                cast( [&]( const auto & ... cs ) noexcept { op( c, cs... ); }, args... );
            };

            cast_one( rec, a );
        }

        template< typename op_t, typename... args_t >
        static semilattice op( op_t o, const args_t & ... args ) noexcept
        {
            semilattice rv;
            int dom = join( args.tag() ... );
            TRACE( "domain join gave", dom );

            auto run_op = [&]( const auto &arg, const auto & ... args ) noexcept
            {
                rv = o( arg )( arg, args... );
            };

            auto downcasted = [&]( const auto & ... args ) noexcept
            {
                in_domain( dom, run_op, args... );
            };

            cast( downcasted, args... );
            return rv;
        }

        static constexpr auto add = []( auto a ) noexcept { return decltype( a )::op_add; };
        static constexpr auto sub = []( auto a ) noexcept { return decltype( a )::op_sub; };
        static constexpr auto mul = []( auto a ) noexcept { return decltype( a )::op_mul; };

        static constexpr auto sdiv = []( auto a ) noexcept { return decltype( a )::op_sdiv; };
        static constexpr auto udiv = []( auto a ) noexcept { return decltype( a )::op_udiv; };
        static constexpr auto srem = []( auto a ) noexcept { return decltype( a )::op_srem; };
        static constexpr auto urem = []( auto a ) noexcept { return decltype( a )::op_urem; };

        /* ... */

        using self = semilattice;

        static self op_add( self a, self b ) noexcept { return op( add, a, b ); }
        static self op_sub( self a, self b ) noexcept { return op( sub, a, b ); }
        static self op_mul( self a, self b ) noexcept { return op( mul, a, b ); }

        static self op_sdiv( self a, self b ) noexcept { return op( sdiv, a, b ); }
        static self op_udiv( self a, self b ) noexcept { return op( udiv, a, b ); }
        static self op_srem( self a, self b ) noexcept { return op( srem, a, b ); }
        static self op_urem( self a, self b ) noexcept { return op( urem, a, b ); }

        /* ... */
    };

    struct sl1
    {
        using doms = domain_list< unit, constant >;

        static constexpr int join( int a, int b ) noexcept
        {
            if ( a == doms::idx< constant > ) return b;
            if ( b == doms::idx< constant > ) return a;

            return doms::idx< unit >;
        }
    };

    using sl1_domain = semilattice< sl1 >;
}
