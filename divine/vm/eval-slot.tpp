// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
/*
 * (c) 2012-2019 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once
#include <divine/vm/dispatch.hpp>
#include <divine/vm/eval.hpp>
#include <brick-tuple>

namespace divine::vm
{
    using brick::tuple::pass;

    template< typename Ctx, typename _T, typename... Args >
    struct V
    {
        using T = _T;
        Eval< Ctx > *ev;
        std::tuple< Args... > args;

        V( Eval< Ctx > *ev, Args... args ) : ev( ev ), args( args... ) {}

        T get( lx::Slot s )
        {
            T result;
            pass( result, &T::setup, args );
            ev->slot_read( s, result );
            return result;
        }

        T get( PointerV p )
        {
            T result;
            pass( result, &T::setup, args );
            ev->heap().read( ev->ptr2h( p ), result );
            return result;
        }

        T get( int v ) { return get( ev->instruction().value( v ) ); }
        T arg( int v ) { return get( v + 1 ); }

        template< typename R = T, typename... CArgs > R construct( CArgs &&... cargs )
        {
            using namespace brick::tuple;
            R result( std::forward< CArgs... >( cargs )... );
            pass( result, &R::setup, args );
            return result;
        }

        void set( int v, T t )
        {
            ev->slot_write( ev->instruction().value( v ), t );
        }
    };

    template< typename Ctx > template< template< typename > class Guard, typename Op >
    void Eval< Ctx >::type_dispatch( typename lx::Slot::Type type, Op _op, lx::Slot slot )
    {
        using lx::Slot;
        switch ( type )
        {
            case Slot::I1: return op< Guard, value::Int<  1 > >( _op );
            case Slot::I8: return op< Guard, value::Int<  8 > >( _op );
            case Slot::I16: return op< Guard, value::Int< 16 > >( _op );
            case Slot::I32: return op< Guard, value::Int< 32 > >( _op );
            case Slot::I64: return op< Guard, value::Int< 64 > >( _op );
            case Slot::I128: return op< Guard, value::Int< 128 > >( _op );
            case Slot::IX:
                ASSERT( slot.width() );
                return op< Guard, value::DynInt<> >( _op, slot.width() );
            case Slot::Ptr: case Slot::PtrA: case Slot::PtrC:
                return op< Guard, PointerV >( _op );
            case Slot::F32:
                return op< Guard, value::Float< float > >( _op );
            case Slot::F64:
                return op< Guard, value::Float< double > >( _op );
            case Slot::F80:
                return op< Guard, value::Float< long double > >( _op );
            case Slot::Void:
                return;
            default:
                // instruction().op->dump();
                UNREACHABLE_F( "an unexpected dispatch type %d", type );
        }
    }

    template< typename Ctx >
    template< template< typename > class Guard, typename T, typename Op, typename... Args >
    auto Eval< Ctx >::op( Op _op, Args... args ) -> typename std::enable_if< Guard< T >::value >::type
    {
        // std::cerr << "op called on type " << typeid( T ).name() << std::endl;
        // std::cerr << instruction() << std::endl;
        _op( V< Ctx, T, Args... >( this, args... ) );
    }

    template< typename Ctx >
    template< template< typename > class Guard, typename T, typename... Args >
    void Eval< Ctx >::op( NoOp, Args... )
    {
        // instruction().op->dump();
        UNREACHABLE_F( "invalid operation on %s", typeid( T ).name() );
    }

}
