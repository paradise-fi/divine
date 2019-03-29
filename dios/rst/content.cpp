// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/content.h>

#include <rst/common.h>
#include <rst/lart.h>

#define _CONTENT __attribute__((__annotate__("lart.abstract.return.content")))

namespace abstract::content {

    using abstract::__new;

    __content * __content_lift( void * addr ) {
        auto obj = __new< __content >( _VM_PT_Marked );
        obj->data = static_cast< Memory::Data >( addr );
        return obj;
    }

    extern "C" {
        _CONTENT void * __content_val( void * addr ) _LART_IGNORE_ARG {
            auto val = __content_lift( addr );
            __lart_stash( reinterpret_cast< uintptr_t >( val ) );
            return abstract::taint< void * >( addr );
        }

        void __content_store( char /*byte*/, __content * /*obj*/ ) {
            _UNREACHABLE_F( "Not implemented." );
        }

        char __content_load( __content * /*obj*/ ) {
            _UNREACHABLE_F( "Not implemented." );
        }

        __content * __content_gep( __content * /*obj*/, uint64_t /*idx*/ ) {
            _UNREACHABLE_F( "Not implemented." );
        }

        void __content_stash( __content * str ) {
            __lart_stash( reinterpret_cast< uintptr_t >( str ) );
        }

        __content * __content_unstash() {
            return reinterpret_cast< __content * >( __lart_unstash() );
        }

        void __content_freeze( __content * str, void * addr ) {
            if ( str ) {
                poke_object< __content >( str, addr );
            }
        }

        __content * __content_thaw( void * addr ) {
            return peek_object< __content >( addr );
        }
    }

} // namespace abstract::content
