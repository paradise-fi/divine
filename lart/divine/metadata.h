#pragma once
#include <type_traits>
#include <brick-assert>

namespace lart::metadata
{

struct TypeSig {
    explicit TypeSig( const char *sig ) : sig( sig ) { }

    template< typename T, char C > struct TCode {
        using Type = T;
        static constexpr char code = C;
    };
    template< typename ... > struct TList { };
    template< typename T, typename ... Ts > struct TList< T, Ts... > {
        using Head = T;
        using Tail = TList< Ts... >;
        using Self = TList< T, Ts... >;
    };

  private:
    template< typename L1, typename L2 > struct Append_;
    template< typename ... Xs, typename ... Ys > struct Append_< TList< Xs... >, TList< Ys... > > {
        using T = TList< Xs..., Ys... >;
    };

  public:
    template< typename L1, typename L2 >
    using Append = typename Append_< L1, L2 >::T;

    // from Itanium ABI C++ mangling
    using ArgTypes = TList< TCode< bool, 'b' >
                       , TCode< char, 'c' >
                       , TCode< int16_t, 's' >
                       , TCode< int32_t, 'i' >
                       , TCode< int64_t, 'l' >
                       , TCode< __int128_t, 'n' >
//                       , TCode< float, 'f' >
//                       , TCode< double, 'd' >
//                       , TCode< long double, 'e' >
//                       , TCode< __float128, 'g' >
                       , TCode< void *, 'p' >
                       >;
    using Types = Append< ArgTypes, TList< TCode< void, 'v' > > >;


  private:
    template< char c, typename List = Types >
    struct DecodeOne_ {
        using T = typename DecodeOne_< c, typename List::Tail >::T;
    };

    template< char c, typename T_, typename... Ts >
    struct DecodeOne_< c, TList< TCode< T_, c >, Ts... > > {
        using T = T_;
    };

    template< typename T, typename List = Types >
    struct EncodeOne_ {
        static constexpr char value = EncodeOne_< T, typename List::Tail >::value;
    };

    template< char c, typename T, typename... Ts >
    struct EncodeOne_< T, TList< TCode< T, c >, Ts... > > {
        static constexpr char value = c;
    };

    template< typename T_, bool false_ >
    struct MakeSigned_ { using T = T_; };

    template< typename T_ >
    struct MakeSigned_< T_, true > { using T = std::make_signed_t< T_ >; };

    template< typename T >
    using MakeSigned = typename MakeSigned_< T,
                            std::is_integral< T >::value && !std::is_same< T, bool >::value
                        >::T;

  public:
    template< char c, typename List = Types >
    using DecodeOne = typename DecodeOne_< c, List >::T;

    template< typename T, typename List = Types >
    static constexpr char encodeOne = EncodeOne_<
                                        typename std::conditional<
                                            std::is_pointer< T >::value
                                            , void *
                                            , typename std::conditional<
                                                std::is_same< MakeSigned< T >, signed char >::value
                                                , char
                                                , MakeSigned< T >
                                                >::type
                                            >::type,
                                        List >::value;

    static_assert( std::is_same< Append< TList<>, TList< int > >, TList< int > >::value,
            "append [] [int]" );
    static_assert( std::is_same< Append< TList< int >, TList< long > >, TList< int, long > >::value,
            "append [int] [long]" );

    template< typename Yield, typename List = Types >
    constexpr static void decode( char c, Yield &&y, List = List() ) {
        if ( c == List::Head::code )
            y( TList< typename List::Head >() );
        else
            decode( c, std::forward< Yield >( y ), typename List::Tail() );
    }

    template< typename Yield >
    constexpr static void decode( char, Yield &&, TList<> ) {
#ifdef __divine__
        __dios_fault( _VM_F_NotImplemented, "Could not decode type signature" );
#else
        UNREACHABLE( "could not decode type signature" );
#endif
    }

    template< int limit, typename List = Types, typename Yield, typename Accum = TList<> >
    constexpr static auto decode( const char *c, Yield &&y, List = List(), Accum = Accum() )
        -> typename std::enable_if< ( limit > 0 ) >::type
    {
        if ( *c == 0 )
            y( Accum() );
        else
            decode( *c, [&]( auto x ) {
                decode< limit - 1 >( c + 1, std::forward< Yield >( y ), List(),
                        Append< Accum, decltype( x ) >() );
            }, List() );
    }

    template< int limit, typename List = Types, typename Yield, typename Accum = TList<> >
    constexpr static auto decode( const char *c, Yield &&y, List = List(), Accum = Accum() )
        -> typename std::enable_if< ( limit <= 0 ) >::type
    {
        if ( *c == 0 )
            y( Accum() );
    }

  private:
    template< typename >
    struct ToType_ { };
    template< typename T_, char c >
    struct ToType_< TCode< T_, c > > { using T = T_; };

    template< typename >
    struct ToTypes_ { };
    template< typename... Ts >
    struct ToTypes_< TList< Ts... > > {
        using T = TList< typename ToType_< Ts >::T... >;
    };

  public:
    template< typename T >
    using ToType = typename ToType_< T >::T;
    template< typename T >
    using ToTypes = typename ToTypes_< T >::T;

    static_assert( std::is_same< DecodeOne< 'i' >, int >::value, "Decode 'i' -> int" );
//    static_assert( std::is_same< DecodeOne< 'e' >, long double >::value, "DecodeOne 'e' -> long double" );
    static_assert( std::is_same< DecodeOne< 'c' >, char >::value, "Decode 'c' -> char" );
    static_assert( encodeOne< int > == 'i', "Encode int -> 'i'" );
//    static_assert( encodeOne< long double > == 'e', "Encode long double -> 'i'" );
    static_assert( encodeOne< char > == 'c', "Encode char -> 'c'" );

    static constexpr char encodeIntTy( int bitwidth ) {
        if ( bitwidth == 1 )
            return encodeOne< bool >;
        if ( bitwidth <= 8 )
            return encodeOne< int8_t >;
        if ( bitwidth <= 16 )
            return encodeOne< int16_t >;
        if ( bitwidth <= 32 )
            return encodeOne< int32_t >;
        if ( bitwidth <= 64 )
            return encodeOne< int64_t >;
        if ( bitwidth <= 128 )
            return encodeOne< __int128_t >;
        return unknownType;
    }

    static constexpr char unknownType = '?';

    TypeSig &operator++() { ++sig; return *this; }
    char operator[]( size_t ix ) const { return sig[ ix ]; }

    const char *sig;
};

}
