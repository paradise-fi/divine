#pragma once

#include <cstdint>
#include <dios.h>

#include <rst/lart.h>
#include <util/array.hpp>
#include <rst/tristate.hpp>

#include <experimental/type_traits>

#define _NOTHROW __attribute__((__nothrow__))

#define _UNREACHABLE_F(...) do { \
        __dios_trace_f( __VA_ARGS__ ); \
        __dios_fault( _VM_F_Assert, "unreachable called" ); \
        __builtin_unreachable(); \
    } while ( false )

#define _EXPECT(expr,...) \
    if (!(expr)) \
        _UNREACHABLE_F( __VA_ARGS__ );

#define _LART_INLINE \
    __attribute__((__always_inline__, __flatten__))

#define _LART_NOINLINE \
    __attribute__((__noinline__))

#define _LART_NOINLINE_WEAK \
    __attribute__((__noinline__,__weak__))

#define _LART_INTERFACE \
    __attribute__((__used__,__nothrow__, __noinline__, __flatten__)) __invisible

#define _LART_OPTNONE \
    __attribute__((optnone))

#define _LART_SCALAR __attribute__((__annotate__("lart.abstract.return.scalar")))
#define _LART_AGGREGATE __attribute__((__annotate__("lart.abstract.return.aggregate")))

extern uint64_t __tainted;

extern "C" bool __vm_test_taint_addr( bool (*tainted) ( bool, void * addr ), bool, void * addr );

extern "C" bool __vm_test_taint_byte( bool (*tainted) ( bool, char val ), bool, char val );

namespace __dios::rst::abstract {

    namespace detail {
        template < class Default, class AlwaysVoid,
                   template< class... > class Op, class... Args >
        struct detector {
            using value_t = std::false_type;
            using type = Default;
        };

        template < class Default, template< class... > class Op, class... Args >
        struct detector< Default, std::void_t< Op< Args... > >, Op, Args... > {
            using value_t = std::true_type;
            using type = Op< Args... >;
        };
    } // namespace detail

    struct Nonesuch {
        ~Nonesuch() = delete;
        Nonesuch( Nonesuch const& ) = delete;
        void operator=( Nonesuch const& ) = delete;
    };

    template < template< class... > class Op, class... Args >
    using is_detected = typename detail::detector< Nonesuch, void, Op, Args... >::value_t;

    template< template< class... > class Op, class... Args >
    inline constexpr bool is_detected_v = is_detected< Op, Args... >::value;

    template < template< class... > class Op, class... Args >
    using detected_t = typename detail::detector< Nonesuch, void, Op, Args... >::type;

    template < typename T, int PT = _VM_PT_Heap >
    using Array = __dios::Array< T, PT >;

    template< typename It >
    struct Range {
        using iterator = It;
        using const_iterator = It;

        Range( It begin, It end ) noexcept : _begin( begin ), _end( end ) { }
        Range( const Range & ) noexcept = default;

        _LART_INLINE
        iterator begin() const noexcept { return _begin; }

        _LART_INLINE
        iterator end() const noexcept { return _end; }

      private:
        It _begin, _end;
    };

    template< typename It >
    _LART_INLINE
    static Range< It > range( It begin, It end ) noexcept { return Range< It >( begin, end ); }

    template< typename T >
    struct Abstracted { };

    static constexpr bool PointerBase = true;

    using bitwidth_t = int8_t;

    template< typename T >
    _LART_INLINE T taint() noexcept
    {
        static_assert( std::is_arithmetic_v< T > || std::is_pointer_v< T >,
                       "Cannot taint a non-arithmetic or non-pointer value." );
        if constexpr ( std::is_arithmetic_v< T > )
            return static_cast< T >( __tainted );
        else
            return reinterpret_cast< T >( __tainted );
    }

    template< typename T >
    _LART_INLINE T taint( T val ) noexcept
    {
        static_assert( std::is_arithmetic_v< T > || std::is_pointer_v< T >,
                       "Cannot taint a non-arithmetic or non-pointer value." );
        if constexpr ( std::is_arithmetic_v< T > )
            return val + static_cast< T >( __tainted );
        else
            return  reinterpret_cast< T >( reinterpret_cast< uintptr_t >( val ) + __tainted );
    }

    _LART_INLINE static bool tainted( void * addr ) noexcept
    {
        return __vm_test_taint_addr( [] ( bool, void * ) { return true; }, false, addr );
    }

    _LART_INLINE static bool tainted( char val ) noexcept
    {
        return __vm_test_taint_byte( [] ( bool, char ) { return true; }, false, val );
    }

    template< typename T, _VM_PointerType PT = _VM_PT_Heap, typename ... Args >
    _LART_INLINE T *__new( Args &&...args )
    {
        void *ptr = __vm_obj_make( sizeof( T ), PT );
        new ( ptr ) T( std::forward< Args >( args )... );
        return static_cast< T * >( ptr );
    }

    template< typename concrete_t, typename abstract_t >
    _LART_INLINE concrete_t stash_abstract_value( abstract_t value )
    {
        using abstract_value_t = void *;
        __lart_stash( static_cast< abstract_value_t >( value ) );
        return taint< concrete_t >();
    }

    template< typename concrete_t, typename abstract_t >
    _LART_INLINE auto make_abstract() noexcept
    {
        return stash_abstract_value< concrete_t >( abstract_t::lift_any( Abstracted< concrete_t >{} ) );
    }

    template< typename concrete_t, typename abstract_t, typename ...args_t >
    _LART_INLINE auto make_abstract( args_t && ...args ) noexcept
    {
        return stash_abstract_value< concrete_t >( abstract_t::lift_any( std::forward< args_t >( args )... ) );
    }

    template< typename abstract_t, typename concrete_t >
    _LART_INLINE auto lift_one( concrete_t c ) noexcept
    {
        static_assert( std::is_integral_v< concrete_t > || std::is_pointer_v< concrete_t >,
                       "Cannot lift a non-arithmetic or non-pointer value." );
        if constexpr ( sizeof( concrete_t ) == 8 )
            return abstract_t::lift_one_i64( c );
        if constexpr ( sizeof( concrete_t ) == 4 )
            return abstract_t::lift_one_i32( c );
        if constexpr ( sizeof( concrete_t ) == 2 )
            return abstract_t::lift_one_i16( c );
        if constexpr ( sizeof( concrete_t ) == 1 )
            return abstract_t::lift_one_i8( c );
    }
    template< typename abstract_t >
    _LART_INLINE auto lift_one( float c ) noexcept { return abstract_t::lift_one_float( c ); }
    template< typename abstract_t >
    _LART_INLINE auto lift_one( double c ) noexcept { return abstract_t::lift_one_double( c ); }

    template< typename concrete_t, typename abstract_t >
    _LART_INLINE concrete_t make_abstract( concrete_t c ) noexcept
    {
        return stash_abstract_value< concrete_t >( lift_one< abstract_t >( c ) );
    }

    template< typename T >
    _LART_INLINE T * peek_object( void * addr ) noexcept
    {
        struct { uint32_t off = 0, obj; } ptr;
        ptr.obj = __vm_peek( addr, _VM_ML_User );

        T * obj;
        memcpy( &obj, &ptr, sizeof( T * ) );
        return obj;
    }

    template< typename T >
    _LART_INLINE void poke_object( T obj, void * addr ) noexcept
    {
        struct { uint32_t off, obj; } ptr;
        memcpy( &ptr, &obj, sizeof( T * ) );
        __vm_poke( addr, _VM_ML_User, ptr.obj );
    }

    struct object_t
    {
        using ptr_t = void *;

        uint32_t off = 0;
        uint32_t obj = 0;

        ptr_t pointer() const noexcept
        {
            ptr_t ptr;
            memcpy( &ptr, this, sizeof( ptr_t ) );
            return ptr;
        }
    };

    template< typename T  = void >
    _LART_INLINE T * object( T * ptr ) noexcept
    {
        return reinterpret_cast< T * >( reinterpret_cast< uintptr_t >( ptr ) & _VM_PM_Obj );
    }

    _LART_INLINE
    static void * object_pointer( uint32_t objid ) noexcept
    {
        object_t object;
        object.obj = objid;
        return object.pointer();
    }

    _LART_INLINE
    static uint32_t objid( void * ptr ) noexcept
    {
        object_t res;
        memcpy( &res, &ptr, sizeof( void * ) );
        return res.obj;
    }

    template< typename T >
    _LART_INLINE uint32_t offset( T * ptr ) noexcept
    {
        object_t res;
        memcpy( &res, &ptr, sizeof( void * ) );
        return res.off;
    }

    template< typename Object, typename Offset >
    struct abstract_object : object_t
    {
        operator ptr_t() noexcept
        {
            ptr_t ptr;
            memcpy( &ptr, this, sizeof( ptr_t ) );
            return ptr;
        }

        Offset offset() const noexcept
        {
            return static_cast< Offset >( object_pointer( this->off ) );
        }

        Object objid() const noexcept
        {
            return static_cast< Object >( object_pointer( this->obj ) );
        }
    };

    _LART_INLINE static uint64_t ignore_fault() noexcept
    {
        return __vm_ctl_flag( 0, _DiOS_CF_IgnoreFault );
    }

    _LART_INLINE static void restore_fault( uint64_t flags ) noexcept
    {
        if ( ( flags & _DiOS_CF_IgnoreFault ) == 0 )
            __vm_ctl_flag( _DiOS_CF_IgnoreFault, 0 );
    }

    _LART_INLINE static bool marked( void * addr ) noexcept
    {
        return __dios_pointer_get_type( addr ) == _VM_PT_Marked;
    }

    _LART_INLINE static bool weak( void * addr ) noexcept
    {
        return __dios_pointer_get_type( addr ) == _VM_PT_Weak;
    }

    _LART_INLINE static void fault_idiv_by_zero() noexcept
    {
        __dios_fault( _VM_Fault::_VM_F_Integer, "division by zero" );
    }

    _LART_INLINE static void fault_fdiv_by_zero() noexcept
    {
        __dios_fault( _VM_Fault::_VM_F_Float, "division by zero" );
    }

    template< typename T >
    _LART_INLINE static constexpr bitwidth_t bitwidth() noexcept
    {
        return std::numeric_limits< T >::digits + std::numeric_limits< T >::is_signed;
    }

    namespace op {

        template< typename T = void >
        struct shift_left;

        template< typename T >
        struct shift_left
        {
          inline constexpr bool operator()( const T& x, const T& y ) const { return x << y; }
        };

        template<>
        struct shift_left< void >
        {
            template < typename T, typename U >
            inline constexpr auto operator()( T&& t, U&& u ) const
                    noexcept( noexcept( std::forward< T >( t ) << std::forward< U >( u ) ) )
                -> decltype( std::forward< T >( t ) << std::forward< U >( u ) )
            { return std::forward< T >( t ) << std::forward< U >( u ); }

            typedef void is_transparent;
        };


        template< typename T = void >
        struct shift_right;

        template< typename T >
        struct shift_right
        {
          inline constexpr bool operator()( const T& x, const T& y ) const { return x >> y; }
        };

        template<>
        struct shift_right< void >
        {
            template < typename T, typename U >
            inline constexpr auto operator()( T&& t, U&& u ) const
                    noexcept( noexcept( std::forward< T >( t ) >> std::forward< U >( u ) ) )
                -> decltype( std::forward< T >( t ) >> std::forward< U >( u ) )
            { return std::forward< T >( t ) >> std::forward< U >( u ); }

            typedef void is_transparent;
        };


        template< typename T = void >
        struct arithmetic_shift_right;

        template< typename T >
        struct arithmetic_shift_right
        {
          inline constexpr bool operator()( const T& x, const T& y ) const
          {
              return x >> y;
          }
        };

        template<>
        struct arithmetic_shift_right< void >
        {
            template < typename T, typename U >
            inline constexpr auto operator()( T&& t, U&& u ) const
                    noexcept( noexcept( std::forward< T >( t ) >> std::forward< U >( u ) ) )
                -> decltype( std::forward< T >( t ) >> std::forward< U >( u ) )
            { return std::forward< T >( t ) >> std::forward< U >( u ); }

            typedef void is_transparent;
        };

    } // namespace op

} // namespace __dios::rst::abstract
