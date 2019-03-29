#pragma once
#include <dios.h>
#include <utility>
#include <new>

#include <util/array.hpp>
#include <sys/metadata.h>
#include <sys/bitcode.h>

#ifdef __divine__
#include <sys/divm.h>
#endif

#define _NOTHROW __attribute__((__nothrow__))
#define _ROOT __attribute__((__annotate__("divine.link.always")))

#define _UNREACHABLE_F(...) do { \
        __dios_trace_f( __VA_ARGS__ ); \
        __dios_fault( _VM_F_Assert, "unreachable called" ); \
        __builtin_unreachable(); \
    } while ( false )

#define _LART_INLINE __attribute__((__always_inline__, __flatten__))
#define _LART_NOINLINE __attribute__((__noinline__))
#define _LART_INTERFACE __attribute__((__nothrow__, __noinline__, __flatten__)) __invisible extern "C"

extern uint64_t __tainted;
extern void * __tainted_ptr;

namespace abstract {

    template< typename T >
    using Array = __dios::Array< T >;

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

    template< typename T, typename ... Args >
    static T *__new( _VM_PointerType pt, Args &&...args )
    {
        void *ptr = __vm_obj_make( sizeof( T ), pt );
        new ( ptr ) T( std::forward< Args >( args )... );
        return static_cast< T * >( ptr );
    }

    template< typename T >
    static T taint()
    {
        static_assert( std::is_arithmetic_v< T > || std::is_pointer_v< T >,
                       "Cannot taint a non-arithmetic or non-pointer value." );
        if constexpr ( std::is_arithmetic_v< T > )
            return static_cast< T >( __tainted );
        else
            return static_cast< T >( __tainted_ptr );
    }

    template< typename T >
    static T taint( T val )
    {
        static_assert( std::is_arithmetic_v< T > || std::is_pointer_v< T >,
                       "Cannot taint a non-arithmetic or non-pointer value." );
        if constexpr ( std::is_arithmetic_v< T > )
            return val + static_cast< T >( __tainted );
        else
            return  reinterpret_cast< T >( reinterpret_cast< uintptr_t >( val ) + __tainted );
    }

    extern "C" bool __vm_test_taint_addr( bool (*tainted) ( bool, void * addr ), bool, void * addr );

    static bool tainted( void * addr )
    {
        return __vm_test_taint_addr( [] ( bool, void * ) { return true; }, false, addr );
    }

    extern "C" bool __vm_test_taint_byte( bool (*tainted) ( bool, char val ), bool, char val );

    static bool tainted( char val )
    {
        return __vm_test_taint_byte( [] ( bool, char ) { return true; }, false, val );
    }

    template< typename T >
    static T * peek_object( void * addr ) noexcept
    {
        struct { uint32_t off = 0, obj; } ptr;
        ptr.obj = __vm_peek( addr, _VM_ML_User );

        T * obj;
        memcpy( &obj, &ptr, sizeof( T * ) );
        return obj;
    }

    template< typename T >
    static void poke_object( T * obj, void * addr ) noexcept
    {
        struct { uint32_t off, obj; } ptr;
        memcpy( &ptr, &obj, sizeof( T * ) );
        __vm_poke( addr, _VM_ML_User, ptr.obj );
    }

    template< typename T >
    inline T * object( T * ptr ) noexcept
    {
        return reinterpret_cast< T * >( reinterpret_cast< uintptr_t >( ptr ) & _VM_PM_Obj );
    }

    template< typename T >
    inline uint32_t offset( T * ptr ) noexcept
    {
        return static_cast< uint32_t >( reinterpret_cast< uintptr_t >( ptr ) & _VM_PM_Off );
    }

    __invisible static inline uint64_t ignore_fault() noexcept {
        return __vm_ctl_flag( 0, _DiOS_CF_IgnoreFault );
    }

    __invisible static inline void restore_fault( uint64_t flags ) noexcept {
        if ( ( flags & _DiOS_CF_IgnoreFault ) == 0 )
            __vm_ctl_flag( _DiOS_CF_IgnoreFault, 0 );
    }

    __invisible static inline bool marked( void * addr ) noexcept {
        return __dios_pointer_get_type( addr ) == _VM_PT_Marked;
    }

    __invisible static inline bool weak( void * addr ) noexcept {
        return __dios_pointer_get_type( addr ) == _VM_PT_Weak;
    }

    template< typename Object, typename Release, typename Check >
    void cleanup( _VM_Frame * frame, Release release, Check check ) noexcept {
        auto *meta = __md_get_pc_meta( frame->pc );
        auto *inst = meta->inst_table;
        auto base = reinterpret_cast< uint8_t * >( frame );

        auto flags = ignore_fault();
        for ( int i = 0; i < meta->inst_table_size; ++i, ++inst ) {
            if ( inst->val_width == 8 ) {
                auto addr = *reinterpret_cast< void ** >( base + inst->val_offset );

                if ( inst->opcode == OpCode::Alloca )  {
                    auto size = __vm_obj_size( addr );
                    for (int off = 0; off < size; ++off ) {
                        auto obj = static_cast< char * >( addr ) + off;
                        if ( abstract::tainted( *( obj ) ) ) {
                            if ( auto * ptr = peek_object< Object >( obj ) ) {
                                release( ptr );
                            }
                        }
                    }
                }

                auto op = inst->opcode;
                bool call = ( op == OpCode::Call || op == OpCode::OpHypercall );
                if ( call && ( marked( addr ) || weak( addr ) ) ) {
                    auto * obj = static_cast< Object * >( addr );
                    release( obj );
                    check( obj );
                }

            }
        }
        restore_fault( flags );
    }

    template< typename Object, typename Cleanup >
    void cleanup_check( Object * obj, Cleanup cleanup ) noexcept {
        if ( !obj || obj->refcount() )
            return;
        cleanup( obj );
    }

} // namespace abstract
