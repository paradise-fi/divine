// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/value.hpp>

/* This file implements the make_frame() function, which builds an empty call
 * frame for a given function, passing it down arguments (in form of value::*)
 * and calls it. Takes a context instance. */

namespace divine::vm
{
    template< typename Ctx >
    struct MakeFrame
    {
        Ctx &_ctx;
        typename Ctx::Program::Function *_fun = nullptr;
        CodePointer _pc;
        bool _incremental;

        MakeFrame( Ctx &ctx, CodePointer pc, bool inc = false )
            : _ctx( ctx ), _pc( pc ), _incremental( inc )
        {
            if ( _pc.function() )
                _fun = &ctx.program().function( pc );
        }

        void push( int i, HeapPointer )
        {
            if ( _incremental )
            {
                if ( i == _fun->argcount + _fun->vararg )
                    _incremental = false;
            }
            else
                ASSERT_EQ( _fun->argcount + _fun->vararg , i );
        }

        template< typename X, typename... Args >
        void push( int i, HeapPointer p, X x, Args... args )
        {
            _ctx.heap().write( p + _fun->instructions[ i ].result().offset, x );
            push( i + 1, p, args... );
        }

        template< typename... Args >
        void enter( PointerV parent, Args... args )
        {
            ASSERT( _fun );
            auto frameptr = _ctx.heap().make( _fun->framesize, _VM_PL_Code + 16 ).cooked();
            _ctx.set( _VM_CR_Frame, frameptr );
            _ctx.pc( _pc );
            _ctx.heap().write( frameptr, PointerV( _pc ) );
            _ctx.heap().write( frameptr + PointerBytes, parent );
            push( 0, frameptr, args... );
            _ctx.entered( _pc );
        }
    };

    template< typename Context, typename... Args >
    void make_frame( Context &ctx, CodePointer pc, PointerV parent, Args... args )
    {
        MakeFrame< Context > mkframe( ctx, pc );
        mkframe.enter( parent, args... );
    }
}
