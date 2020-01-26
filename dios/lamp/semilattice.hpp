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

    template< typename type, typename tag >
    struct tagged : type, tag
    {
        using type::type;
        using unwrap = type;
    };

    struct index_tag {};
    struct scalar_tag {};

    template< typename dom_t > using index_t  = tagged< dom_t, index_tag >;
    template< typename dom_t > using scalar_t = tagged< dom_t, scalar_tag >;

    template< typename sl >
    struct semilattice : __lava::tagged_array<>, __lava::domain_mixin< semilattice< sl > >
    {
        using base = __lava::tagged_array<>;
        using doms = typename sl::doms;
        using self = semilattice;
        using sref = const self &;

        template< int idx > using dom_type = typename doms::template type< idx >;
        template< typename type > static constexpr int dom_idx = doms::template idx< type >;

        semilattice( void *v, __dios::construct_shared_t s ) : base( v, s ) {}

        template< typename dom_t, typename = std::enable_if_t< doms::template idx< dom_t > >= 0 > >
        semilattice( dom_t &&v ) : base( v.disown(), construct_shared )
        {
            tag() = doms::template idx< dom_t >;
        }

        struct index_w  : self { using self::self; index_w( const self &v ) : self( v ) {} };
        struct scalar_w : self { using self::self; scalar_w( const self &v ) : self( v ) {} };

        static constexpr int join( int a ) { return a; }

        template< typename... args_t >
        static constexpr int join( int a, int b, args_t... args )
        {
            if ( a >= 0 && b >= 0 )
                return sl::join( a, join( b, args... ) );
            else
                return join( a >= 0 ? a : b, args... );
        }

        template< typename to, typename from >
        static auto lift_to( const from &f )
        {
            if constexpr ( std::is_base_of_v< index_tag, from > )
            {
                using orig = typename from::unwrap;
                constexpr int goal = join( dom_idx< orig >, dom_idx< typename to::index_dom > );
                static_assert( goal >= 0 );
                return self::lift_to< dom_type< goal >, orig >( f );
            }
            else if constexpr ( std::is_base_of_v< scalar_tag, from > )
            {
                using orig = typename from::unwrap;
                constexpr int orig_idx = dom_idx< orig >;
                ASSERT_EQ( f.tag(), orig_idx );
                constexpr int goal = join( orig_idx, dom_idx< typename to::scalar_dom > );
                static_assert( goal >= 0 );
                return self::lift_to< dom_type< goal >, orig >( f );
            }
            else if constexpr ( std::is_same_v< from, to > )
                return f;
            else if constexpr ( std::is_trivial_v< from > )
                return f;
            else
                return to::lift( f );
        }

        template< typename op_t, int idx = 0, typename... args_t >
        static self in_domain( int dom, op_t op, const args_t & ... args )
        {
            if constexpr ( idx < doms::size )
            {
                constexpr int joined = join( idx, doms::template idx< args_t > ... );
                static_assert( joined >= 0 );
                if ( idx == dom )
                    if constexpr ( joined == idx )
                        return op( lift_to< typename doms::template type< idx > >( args ) ... );

                return in_domain< op_t, idx + 1 >( dom, op, args... );
            }
            else __builtin_unreachable();
        }

        template< typename op_t, typename sl_t, int idx = 0 >
        static auto cast_one( op_t op, const sl_t &v )
        {
            if constexpr ( idx < doms::size )
            {
                constexpr bool is_idx = std::is_same_v< sl_t, index_w >;
                constexpr bool is_scl = std::is_same_v< sl_t, scalar_w >;

                using to_type = typename doms::template type< idx >;
                using index_type = index_t< to_type >;
                using scalar_type = scalar_t< to_type >;
                using coerce1_t = std::conditional_t< is_idx, index_type, to_type >;
                using coerce_t = std::conditional_t< is_scl, scalar_type, coerce1_t >;

                if ( v.tag() == idx )
                {
                    coerce_t coerce( v.ptr(), v.construct_shared );
                    auto rv = op( coerce );
                    coerce.disown();
                    return rv;
                }
                else
                    return self::cast_one< op_t, sl_t, idx + 1 >( op, v );
            }
            return self::cast_one< op_t, sl_t, 0 >( ( __builtin_trap(), op ), v );
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
        static self op( op_t operation, const args_t & ... args )
        {
            int dom = join( args.tag() ... );

            auto downcasted = [&]( const auto & ... args )
            {
                return in_domain( dom, operation, args... );
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

        static constexpr auto zext  = []( auto a ) { return decltype( a )::op_zext; };
        static constexpr auto sext  = []( auto a ) { return decltype( a )::op_sext; };
        static constexpr auto trunc = []( auto a ) { return decltype( a )::op_trunc; };

        static constexpr auto ne   = []( auto a ) { return decltype( a )::op_ne; };
        static constexpr auto eq   = []( auto a ) { return decltype( a )::op_eq; };
        static constexpr auto slt  = []( auto a ) { return decltype( a )::op_slt; };
        static constexpr auto sgt  = []( auto a ) { return decltype( a )::op_sgt; };
        static constexpr auto ult  = []( auto a ) { return decltype( a )::op_ult; };
        static constexpr auto ugt  = []( auto a ) { return decltype( a )::op_ugt; };

        static constexpr auto shl  = []( auto a ) { return decltype( a )::op_shl; };
        static constexpr auto lshr = []( auto a ) { return decltype( a )::op_lshr; };
        static constexpr auto ashr = []( auto a ) { return decltype( a )::op_ashr; };

        static constexpr auto load = []( auto a ) { return decltype( a )::op_load; };

        static constexpr auto gep = []( auto a )
        {
            return []( const auto & ... x ) { return decltype( a )::op_gep( x... ); };
        };

        static constexpr auto store = []( auto a )
        {
            return []( const auto & ... x ) { return decltype( a )::op_store( x... ); };
        };

        static constexpr auto _assume = []( auto a ) { return decltype( a )::assume; };
        static constexpr auto _tristate = []( auto a ) { return decltype( a )::to_tristate; };

        static constexpr auto strcmp = []( auto a ) { return decltype( a )::fn_strcmp; };
        static constexpr auto strlen = []( auto a ) { return decltype( a )::fn_strlen; };
        static constexpr auto strcat = []( auto a ) { return decltype( a )::fn_strcat; };
        static constexpr auto strcpy = []( auto a ) { return decltype( a )::fn_strcpy; };

        static constexpr auto strchr = []( auto a )
        {
            return []( const auto & ... x ) { return decltype( a )::fn_strchr( x... ); };
        };

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
        static self lift( const val_t &val ) { return sl::scalar_lift_dom::lift( val ); }
        static self lift( __lava::array_ref arr ) { return sl::array_lift_dom::lift( arr ); }

        template< typename val_t >
        static self any() { return sl::scalar_any_dom::template any< val_t >(); }

        static self assume( sref a, sref b, bool c ) { return op( wrap( _assume, c ), a, b ); }
        static tristate to_tristate( sref a )
        {
            return cast( [&]( auto v ) { return decltype( v )::to_tristate( v ); }, a );
        }

        static self op_add( sref a, sref b ) { return op( wrap( add ), a, b ); }
        static self op_sub( sref a, sref b ) { return op( wrap( sub ), a, b ); }
        static self op_mul( sref a, sref b ) { return op( wrap( mul ), a, b ); }

        static self op_sdiv( sref a, sref b ) { return op( wrap( sdiv ), a, b ); }
        static self op_udiv( sref a, sref b ) { return op( wrap( udiv ), a, b ); }
        static self op_srem( sref a, sref b ) { return op( wrap( srem ), a, b ); }
        static self op_urem( sref a, sref b ) { return op( wrap( urem ), a, b ); }

        static self op_zext ( sref a, bw b ) { return op( wrap( zext,  b ), a ); }
        static self op_sext ( sref a, bw b ) { return op( wrap( sext,  b ), a ); }
        static self op_trunc( sref a, bw b ) { return op( wrap( trunc, b ), a ); }

        static self op_eq ( sref a, sref b ) { return op( wrap( eq  ), a, b ); }
        static self op_ne ( sref a, sref b ) { return op( wrap( ne  ), a, b ); }
        static self op_slt( sref a, sref b ) { return op( wrap( slt ), a, b ); }
        static self op_sgt( sref a, sref b ) { return op( wrap( sgt ), a, b ); }
        static self op_ult( sref a, sref b ) { return op( wrap( ult ), a, b ); }
        static self op_ugt( sref a, sref b ) { return op( wrap( ugt ), a, b ); }

        static self op_shl ( sref a, sref b ) { return op( wrap( shl  ), a, b ); }
        static self op_lshr( sref a, sref b ) { return op( wrap( lshr ), a, b ); }
        static self op_ashr( sref a, sref b ) { return op( wrap( ashr ), a, b ); }

        static self op_gep( sref a, sref b, uint64_t s )
        {
            return op( wrap( gep, s ), a, index_w( b ) );
        }

        static self op_store( sref a, sref b, bw w )
        {
            return op( wrap( store, w ), a, scalar_w( b ) );
        }

        static self op_load( sref a, bw w ) { return op( wrap( load, w ), a ); }

        static self fn_strcmp( sref a, sref b ) { return op( wrap( strcmp ), a, b ); }
        static self fn_strcat( sref a, sref b ) { return op( wrap( strcat ), a, b ); }
        static self fn_strcpy( sref a, sref b ) { return op( wrap( strcpy ), a, b ); }
        static self fn_strchr( sref a, sref b ) { return op( wrap( strchr ), a, scalar_w( b ) ); }
        static self fn_strlen( sref a ) { return op( wrap( strlen ), a ); }
    };

}
