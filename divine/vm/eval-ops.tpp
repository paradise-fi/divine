// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2018 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/eval.hpp>

namespace divine::vm
{
    template< typename Ctx >
    void Eval< Ctx >::implement_extractvalue()
    {
        auto off = gep( instruction().subcode, 1, instruction().argcount() );
        ASSERT( off.defined() );
        slot_copy( s2ptr( operand( 0 ), off.cooked() ), result(), result().size() );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_insertvalue()
    {
        /* first copy the original */
        slot_copy( s2ptr( operand( 0 ) ), result(), result().size() );
        auto off = gep( instruction().subcode, 2, instruction().argcount() );
        ASSERT( off.defined() );
        /* write the new value over the selected field */
        slot_copy( s2ptr( operand( 1 ) ), result(), operand( 1 ).size(), off.cooked() );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_indirectBr()
    {
        local_jump( operandCk< PointerV >( 0 ) );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_call( bool invoke )
    {
        auto targetV = operand< PointerV >( invoke ? -3 : -1 );

        if ( !targetV.defined() )
        {
            fault( _VM_F_Control ) << "invalid call on an uninitialised pointer " << targetV;
            return;
        }

        if ( ! targetV.cooked().null() && targetV.cooked().type() != PointerType::Code )
        {
            fault( _VM_F_Control ) << "invalid call on a pointer which does not point to code: "
                                    << targetV;
            return;
        }
        CodePointer target = targetV.cooked();

        ASSERT( !(!invoke && target.function()) || instruction().subcode == Intrinsic::not_intrinsic );

        if ( !target.function() )
        {
            if ( instruction().subcode != Intrinsic::not_intrinsic )
                return implement_intrinsic( instruction().subcode );
            fault( _VM_F_Control ) << "invalid call on an invalid code pointer: "
                                    << target;
            return;
        }

        const auto &function = program().function( target );

        if ( target != program().entry( target ) )
        {
            fault( _VM_F_Control ) << "invalid call on a code pointer which does not point "
                                    << "to a function entry: " << target;
            return;
        }


        /* report problems with the call before pushing the new stackframe */
        const int argcount = instruction().argcount() - ( invoke ? 3 : 1 );

        if ( !function.vararg && argcount > function.argcount )
        {
            fault( _VM_F_Control ) << "too many arguments given to a call: "
                                    << function.argcount << " expected but "
                                    << argcount << " given";
            return;
        }

        if ( argcount < function.argcount )
        {
            fault( _VM_F_Control ) << "too few arguments given to a call: "
                                    << function.argcount << " expected but "
                                    << argcount << " given";
            return;
        }

        auto frameptr = makeobj( program().function( target ).framesize );
        auto p = frameptr;
        heap().write_shift( p, PointerV( target ) );
        heap().write_shift( p, PointerV( frame() ) );

        /* Copy arguments to the new frame. */
        for ( int i = 0; i < argcount && i < int( function.argcount ); ++i )
            heap().copy( s2ptr( operand( i ) ),
                            s2ptr( function.instructions[ i ].result(), 0, frameptr.cooked() ),
                            function.instructions[ i ].result().size() );

        if ( function.vararg )
        {
            int size = 0;
            for ( int i = function.argcount; i < argcount; ++i )
                size += operand( i ).size();
            auto vaptr = size ? makeobj( size ) : nullPointerV();
            auto vaptr_loc = s2ptr( function.instructions[ function.argcount ].result(),
                                    0, frameptr.cooked() );
            heap().write( vaptr_loc, vaptr );
            for ( int i = function.argcount; i < argcount; ++i )
            {
                auto op = operand( i );
                heap().copy( s2ptr( op ), vaptr.cooked(), op.size() );
                heap().skip( vaptr, int( op.size() ) );
            }
        }

        ASSERT_NEQ( instruction().opcode, OpCode::PHI );
        context().sync_pc();
        context().set( _VM_CR_Frame, frameptr.cooked() );
        context().set( _VM_CR_PC, target );
        context().entered( target );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_ret()
    {
        PointerV fr( frame() );
        heap().skip( fr, sizeof( typename PointerV::Raw ) );
        PointerV parent, br;
        heap().read( fr.cooked(), parent );

        collect_frame_locals( pc(), [&]( auto ptr, auto ) { freeobj( ptr.cooked() ); } );

        if ( parent.cooked().null() )
        {
            if ( context().flags_any( _VM_CF_AutoSuspend ) )
            {
                context().flags_set( 0, _VM_CF_KeepFrame | _VM_CF_Stop );
                _final_frame = frame();
                context().set( _VM_CR_Frame, parent.cooked() );
            }
            else
                fault( _VM_F_Control ) << "trying to return without a caller";
            return;
        }

        PointerV caller_pc;
        heap().read( parent.cooked(), caller_pc );
        const auto &caller = program().instruction( caller_pc.cooked() );
    /* TODO
        if ( caller.opcode != OpCode::Invoke && caller.opcode != OpCode::Call )
        {
            fault( _VM_F_Control, parent.cooked(), caller_pc.cooked() )
                << "Illegal return to a non-call location.";
            return;
        }
    */
        if ( instruction().argcount() ) /* return value */
        {
            if ( !caller.has_result() ) {
                fault( _VM_F_Control, parent.cooked(), caller_pc.cooked() )
                    << "Function which was called as void returned a value";
                return;
            } else if ( caller.result().size() != operand( 0 ).size() ) {
                fault( _VM_F_Control, parent.cooked(), caller_pc.cooked() )
                    << "The returned value is bigger than expected by the caller";
                return;
            } else if ( !heap().copy( s2ptr( operand( 0 ) ),
                                    s2ptr( caller.result(), 0, parent.cooked() ),
                                    caller.result().size() ) )
            {
                fault( _VM_F_Memory, parent.cooked(), caller_pc.cooked() )
                    << "Could not return a value";
                return;
            }
        } else if ( caller.result().size() > 0 ) {
            fault( _VM_F_Control, parent.cooked(), caller_pc.cooked() )
                << "Function called as non-void did not return a value";
            return;
        }

        auto retpc = pc();
        context().set( _VM_CR_Frame, parent.cooked() );
        context().set( _VM_CR_PC, caller_pc.cooked() );

        if ( caller.opcode == OpCode::Invoke )
        {
            auto rv = s2ptr( caller.operand( -2 ), 0, parent.cooked() );
            heap().read( rv, br );
            local_jump( br );
        }
        freeobj( fr.cooked() );
        context().left( retpc );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_br()
    {
        if ( instruction().argcount() == 1 )
            local_jump( operandCk< PointerV >( 0 ) );
        else
        {
            auto cond = operand< BoolV >( 0 );
            auto target = operandCk< PointerV >( cond.cooked() ? 2 : 1 );
            if ( cond.defbits() & 1 )
                local_jump( target );
            else
                fault( _VM_F_Control, frame(), target.cooked() )
                    << " conditional jump depends on an undefined value";
        }
    }
}
