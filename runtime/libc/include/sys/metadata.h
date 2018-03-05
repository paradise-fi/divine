#ifdef __divine__
#include <sys/divm.h>
#include <dios.h>
#endif
#include <stdint.h>

#ifndef __DIVINE_METADATA_H__
#define __DIVINE_METADATA_H__

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define CPP_END }
#if __cplusplus >= 201103L
#define NOTHROW noexcept
#else
#define NOTHROW throw()
#endif
#else
#define EXTERN_C
#define CPP_END
#define NOTHROW __attribute__((__nothrow__))
#endif
#define _ROOT __attribute__((__annotate__("divine.link.always")))

EXTERN_C

typedef struct {
    /* LLVM instruction opcode (use divine/opcodes.h, enum OpCode to decode) */
    int opcode;
    /* suboperation for cmp, armw, and ivoke (in case of invoke it is the
     * offset of landing block from the beginning of the function (that is,
     * instruction index of landing block)) */
    int subop;
    /* offset of return value in frame, do not use directly, use
     * __md_get_register_info instead */
    int val_offset;
    /* width of return value in frame, do not use directly, use
     * __md_get_register_info instead */
    int val_width;
} _MD_InstInfo;

typedef struct {
    char *name;
    void (*entry_point)( void );
    int frame_size;
    int arg_count;
    int is_variadic;
    char *type; /* similar to Itanium ABI mangle, first return type, then
                *  arguments, all pointers are under p, all integers except for
                *  char are set as unsigned, all chars are 'c' (char) */
    int inst_table_size;
    _MD_InstInfo *inst_table;
    void (*ehPersonality)( void );
    void *ehLSDA; /* language-specific exception handling tables */
    int is_nounwind;
    int is_trap;
} _MD_Function;

typedef struct {
    void *start;
    int width;
} _MD_RegInfo;

typedef struct {
    char *name;
    void *address;
    long size;
    int is_constant;
} _MD_Global;

/* Query function metadata by function name, this is the mangled name in case
 * of C++ or other languages which employ name mangling */
// const _MD_Function *__md_get_function_meta( const char *name ) NOTHROW _ROOT;

/* Query function metadata program counter value */
const _MD_Function *__md_get_pc_meta( _VM_CodePointer pc ) NOTHROW _ROOT;

/* Given function frame, program counter and metadata extracts pointer to
 * register corresponding to instruction with given PC from the frame.
 *
 * If the PC is out of range, or any argument is invalid return { nullptr, 0 },
 * while for void values (registers of instructions which do no return) returns
 * { ptr-somewhere-to-the-frame, 0 }.
 *
 * Be aware that registers can be reused/overlapping if they do not interfere
 * or are guaranteed to have same value any time they are both defined in a
 * valid run.
 * Examples of safe use:
 * * writing landingpad's register before jumping to the instruction after the
 *   landingpad
 * * reading register of value of call/invoke
 * */
_MD_RegInfo __md_get_register_info( struct _VM_Frame *frame, _VM_CodePointer pc, const _MD_Function *funMeta ) NOTHROW _ROOT;

const _MD_Global *__md_get_global_meta( const char *name ) NOTHROW _ROOT;

CPP_END

#if __cplusplus >= 201402L && !defined( _PDCLIB_BUILD )

#include <type_traits>
#ifndef __divine__
#include <brick-assert>
#endif

namespace metadata {

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
#endif

#undef EXTERN_C
#undef CPP_END
#undef NOTHROW
#undef _ROOT

#endif // __DIVINE_METADATA_H__
