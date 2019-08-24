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
#include <divine/vm/eval.hpp>

namespace divine::vm
{
    template< typename Ctx >
    void Eval< Ctx >::implement_stacksave()
    {
        std::vector< PointerV > ptrs;
        collect_allocas( pc(), [&]( PointerV p, auto ) { ptrs.push_back( p ); } );
        auto r = makeobj( sizeof( IntV::Raw ) + ptrs.size() * PointerBytes );
        auto p = r;
        heap().write_shift( p, IntV( ptrs.size() ) );
        for ( auto ptr : ptrs )
            heap().write_shift( p, ptr );
        result( r );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_stackrestore()
    {
        auto r = operand< PointerV >( 0 );

        if ( !boundcheck( r, sizeof( int ), false ) )
            return;

        IntV count;
        heap().read_shift( r, count );
        if ( !count.defined() )
            fault( _VM_F_Hypercall ) << " stackrestore with undefined count";
        auto s = r;

        collect_allocas( pc(), [&]( auto ptr, auto slot )
        {
            bool retain = false;
            r = s;
            for ( int i = 0; i < count.cooked(); ++i )
            {
                PointerV p;
                heap().read_shift( r, p );
                auto eq = ptr == p;
                if ( !eq.defined() )
                {
                    fault( _VM_F_Hypercall ) << " undefined pointer at index "
                                                << i << " in stackrestore";
                    break;
                }
                if ( eq.cooked() )
                {
                    retain = true;
                    break;
                }
            }
            if ( !retain )
            {
                freeobj( ptr.cooked() );
                slot_write( slot, PointerV( nullPointer() ) );
            }
        } );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_intrinsic( int id )
    {
        auto _arith_with_overflow = [this]( auto wrap, auto impl, auto check ) -> void
        {
            return op< IsIntegral >( 1, [&]( auto v ) {
                    auto a = wrap( v.get( 1 ) ),
                            b = wrap( v.get( 2 ) );
                    auto r = impl( a, b );
                    BoolV over( check( a.cooked(), b.cooked() ) );
                    if ( !r.defined() )
                        over.defined( false );

                    slot_write( result(), r, 0 );
                    slot_write( result(), over, sizeof( typename decltype( a )::Raw ) );
                } );
        };

        auto _make_signed = []( auto x ) { return x.make_signed(); };
        auto _id = []( auto x ) { return x; };

        switch ( id )
        {
            case Intrinsic::vastart:
            {
                // writes a pointer to a monolithic block of memory that
                // contains all the varargs, successively assigned higher
                // addresses (going from left to right in the argument list) to
                // the argument of the intrinsic
                const auto &f = program().functions[ pc().function() ];
                if ( !f.vararg ) {
                    fault( _VM_F_Hypercall ) << "va_start called in a non-variadic function";
                    return;
                }
                auto vaptr_loc = s2ptr( f.instructions[ f.argcount ].result() );
                auto vaList = operand< PointerV >( 0 );
                if ( !boundcheck( vaList, operand( 0 ).size(), true ) )
                    return;
                heap().copy( vaptr_loc, ptr2h( vaList ), operand( 0 ).size() );
                return;
            }
            case Intrinsic::vaend: return;
            case Intrinsic::vacopy:
            {
                auto from = operand< PointerV >( 1 );
                auto to = operand< PointerV >( 0 );
                if ( !boundcheck( from, operand( 0 ).size(), false ) ||
                        !boundcheck( to, operand( 0 ).size(), true ) )
                    return;
                heap().copy( ptr2h( from ), ptr2h( to ), operand( 0 ).size() );
                return;
            }
            case Intrinsic::trap:
                fault( _VM_F_Control ) << "llvm.trap executed";
                return;
            case Intrinsic::umul_with_overflow:
                return _arith_with_overflow( _id, []( auto a, auto b ) { return a * b; },
                                                []( auto a, auto b ) { return a > _maxbound( a ) / b; } );
            case Intrinsic::uadd_with_overflow:
                return _arith_with_overflow( _id, []( auto a, auto b ) { return a + b; },
                                                []( auto a, auto b ) { return a > _maxbound( a ) - b; } );
            case Intrinsic::usub_with_overflow:
                return _arith_with_overflow( _id, []( auto a, auto b ) { return a - b; },
                                                []( auto a, auto b ) { return b > a; } );

            case Intrinsic::smul_with_overflow:
                return _arith_with_overflow( _make_signed, []( auto a, auto b ) { return a * b; },
                                            []( auto a, auto b )
                                            {
                                                return (a > _maxbound( a ) / b)
                                                    || (a < _minbound( a ) / b)
                                                    || ((a == -1) && (b == _minbound( a )))
                                                    || ((b == -1) && (a == _minbound( a ))); } );
            case Intrinsic::sadd_with_overflow:
                return _arith_with_overflow( _make_signed, []( auto a, auto b ) { return a + b; },
                                                []( auto a, auto b ) { return b > 0
                                                    ? a > _maxbound( a ) - b
                                                    : a < _minbound( a ) - b; } );
            case Intrinsic::ssub_with_overflow:
                return _arith_with_overflow( _make_signed, []( auto a, auto b ) { return a - b; },
                                                []( auto a, auto b ) { return b < 0
                                                    ? a > _maxbound( a ) + b
                                                    : a < _minbound( a ) + b; } );

            case Intrinsic::stacksave:
                return implement_stacksave();
            case Intrinsic::stackrestore:
                return implement_stackrestore();
            case Intrinsic::lifetime_start:
            {
                auto size = operand< IntV >( 0 );
                auto op = operand< PointerV >( 1 );
                if ( !heap().valid( op.cooked() ) )
                    heap().make( size.cooked(), op.cooked().object(), true );
                return;
            }
            case Intrinsic::lifetime_end:
            {
                auto op = operand< PointerV >( 1 );
                if ( !freeobj( op.cooked() ) )
                    fault( _VM_F_Memory ) << "invalid pointer passed to llvm.lifetime.end";
                return;
            }
            default:
                /* We lowered everything else in buildInfo. */
                // instruction().op->dump();
                UNREACHABLE( "unexpected intrinsic", id );
        }
    }
}
