#include <brick-cons>
#include <dios/lava/base.hpp>
#include <dios/lava/tristate.hpp>

namespace __lamp
{
    using bw = __lava::bitwidth_t;
    using tristate = __lava::tristate;

    template< typename... domains >
    struct domain_list : brq::cons_list_t< domains... >
    {
        template< typename type > static constexpr int idx = domain_list::template index_of< type >;
        template< int idx > using type = typename domain_list::template type_at< idx >;
    };

    template< typename sl >
    struct semilattice : __lava::tagged_array<>, __lava::domain_mixin< semilattice< sl > >
    {
        using __lava::tagged_array<>::tagged_array;
        using doms = typename sl::doms;
        using self = semilattice;

        template< typename dom_t >
        semilattice( const dom_t &v ) : tagged_array<>( v )
        {
            tag() = doms::template idx< dom_t >;
        }

        static constexpr int join( int a ) { return a; }

        template< typename... args_t >
        static constexpr int join( int a, int b, args_t... args )
        {
            return sl::join( a, join( b, args... ) );
        }

        template< typename to, typename from >
        static auto lift_to( const from &f )
        {
            if constexpr ( std::is_same_v< from, to > )
                return f;
            else if constexpr ( std::is_trivial_v< from > )
                return f;
            else
                return to::lift( f );
        }

        template< typename op_t, int idx = 0, typename... args_t >
        static auto in_domain( int dom, op_t op, const args_t & ... args )
        {
            if constexpr ( idx < doms::size )
            {
                if ( idx == dom )
                {
                    constexpr int joined = join( idx, doms::template idx< args_t > ... );
                    static_assert( joined >= 0 );
                    static_assert( ( ( doms::template idx< args_t > >= 0 ) && ... ) );

                    if constexpr ( joined == idx )
                        return op( lift_to< typename doms::template type< idx > >( args ) ... );
                    else
                        __builtin_trap();
                }
                else
                    return in_domain< op_t, idx + 1 >( dom, op, args... );
            }
            else
                return in_domain< op_t, 0 >( ( __builtin_trap(), dom ), op, args... );
        }

        template< typename op_t, int idx = 0 >
        static auto cast_one( op_t op, const semilattice &v )
        {
            if constexpr ( idx < doms::size )
            {
                using to_type = const typename doms::template type< idx > &;
                using via = const tagged_array<> &;
                if ( v.tag() == idx )
                    return op( static_cast< to_type >( static_cast< via >( v ) ) );
                else
                    return cast_one< op_t, idx + 1 >( op, v );
            }
            return cast_one< op_t, 0 >( ( __builtin_trap(), op ), v );
        }

        template< typename op_t >
        static auto cast( op_t op ) { return op(); }

        template< typename op_t, typename arg_t, typename... args_t >
        static auto cast( op_t op, const arg_t &a, const args_t & ... args )
        {
            auto rec = [&]( const auto &c )
            {
                return cast( [&]( const auto & ... cs ) { return op( c, cs... ); }, args... );
            };

            return cast_one( rec, a );
        }

        template< typename op_t, typename... args_t >
        static auto op( op_t o, const args_t & ... args )
        {
            int dom = join( args.tag() ... );
            TRACE( "domain join gave", dom );

            auto downcasted = [&]( const auto & ... args )
            {
                return in_domain( dom, o, args... );
            };

            return cast( downcasted, args... );
        }

        static constexpr auto add = []( auto a ) { return decltype( a )::op_add; };
        static constexpr auto sub = []( auto a ) { return decltype( a )::op_sub; };
        static constexpr auto mul = []( auto a ) { return decltype( a )::op_mul; };

        static constexpr auto sdiv = []( auto a ) { return decltype( a )::op_sdiv; };
        static constexpr auto udiv = []( auto a ) { return decltype( a )::op_udiv; };
        static constexpr auto srem = []( auto a ) { return decltype( a )::op_srem; };
        static constexpr auto urem = []( auto a ) { return decltype( a )::op_urem; };

        static constexpr auto zext = []( auto a ) { return decltype( a )::op_zext; };
        static constexpr auto sext = []( auto a ) { return decltype( a )::op_sext; };

        static constexpr auto ne   = []( auto a ) { return decltype( a )::op_ne; };
        static constexpr auto eq   = []( auto a ) { return decltype( a )::op_eq; };
        static constexpr auto slt  = []( auto a ) { return decltype( a )::op_slt; };
        static constexpr auto sgt  = []( auto a ) { return decltype( a )::op_sgt; };
        static constexpr auto ult  = []( auto a ) { return decltype( a )::op_ult; };
        static constexpr auto ugt  = []( auto a ) { return decltype( a )::op_ugt; };

        static constexpr auto shl  = []( auto a ) { return decltype( a )::op_shl; };
        static constexpr auto lshr = []( auto a ) { return decltype( a )::op_lshr; };
        static constexpr auto ashr = []( auto a ) { return decltype( a )::op_ashr; };

        static constexpr auto trunc = []( auto a ) { return decltype( a )::op_trunc; };

        static constexpr auto _assume = []( auto a ) { return decltype( a )::assume; };
        static constexpr auto _tristate = []( auto a ) { return decltype( a )::to_tristate; };

        static constexpr auto run = []( const auto &op, const auto & ... bind  )
        {
            return [=]( const auto &arg, const auto & ... args )
            {
                return op( arg )( arg, args..., bind... );
            };
        };

        static constexpr auto wrap = []( const auto &op, const auto & ... bind  )
        {
            return [=]( const auto &arg, const auto & ... args )
            {
                return self( op( arg )( arg, args..., bind... ) );
            };
        };

        template< typename val_t >
        static self lift( const val_t &val ) { return sl::lift_dom::lift( val ); }

        template< typename val_t >
        static self any() { return sl::any_dom::template any< val_t >(); }

        static self assume( self a, self b, bool c ) { return op( wrap( _assume, c ), a, b ); }
        static tristate to_tristate( self a ) { return op( run( _tristate ), a ); }

        static self op_add( self a, self b ) { return op( wrap( add ), a, b ); }
        static self op_sub( self a, self b ) { return op( wrap( sub ), a, b ); }
        static self op_mul( self a, self b ) { return op( wrap( mul ), a, b ); }

        static self op_sdiv( self a, self b ) { return op( wrap( sdiv ), a, b ); }
        static self op_udiv( self a, self b ) { return op( wrap( udiv ), a, b ); }
        static self op_srem( self a, self b ) { return op( wrap( srem ), a, b ); }
        static self op_urem( self a, self b ) { return op( wrap( urem ), a, b ); }

        static self op_zext ( self a, bw b ) { return op( wrap( zext,  b ), a ); }
        static self op_sext ( self a, bw b ) { return op( wrap( sext,  b ), a ); }
        static self op_trunc( self a, bw b ) { return op( wrap( trunc, b ), a ); }

        static self op_eq ( self a, self b ) { return op( wrap( eq  ), a, b ); }
        static self op_ne ( self a, self b ) { return op( wrap( ne  ), a, b ); }
        static self op_slt( self a, self b ) { return op( wrap( slt ), a, b ); }
        static self op_sgt( self a, self b ) { return op( wrap( sgt ), a, b ); }
        static self op_ult( self a, self b ) { return op( wrap( ult ), a, b ); }
        static self op_ugt( self a, self b ) { return op( wrap( ugt ), a, b ); }

        static self op_shl ( self a, self b ) { return op( wrap( shl  ), a, b ); }
        static self op_lshr( self a, self b ) { return op( wrap( lshr ), a, b ); }
        static self op_ashr( self a, self b ) { return op( wrap( ashr ), a, b ); }
    };

}
