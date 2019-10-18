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
    void Eval< Ctx >::implement_hypercall_syscall()
    {
        int idx = 0;

        std::vector< long > args;
        std::vector< bool > argt;
        std::vector< std::unique_ptr< char[] > > bufs;

        std::vector< int >  actions;

        auto type = [&]( int action ) { return action & ~( _VM_SC_In | _VM_SC_Out ); };
        auto in   = [&]( int action ) { return action & _VM_SC_In; };
        auto out  = [&]( int action ) { return action & _VM_SC_Out; };
        auto next = [&]( int action ) { idx += type( action ) == _VM_SC_Mem ? 2 : 1; };

        auto prepare = [&]( int action )
        {
            if ( type( action ) < 0 || type( action ) > 2 || ( !in( action ) && !out( action ) ) )
            {
                fault( _VM_F_Hypercall ) << "illegal syscall parameter no " << idx << ": " << action;
                return false;
            }

            if ( in( action ) || type( action ) == _VM_SC_Mem )
                argt.push_back( type( action ) != _VM_SC_Int32 );

            if ( type( action ) == _VM_SC_Int32 && in( action ) )
                args.push_back( operandCk< IntV >( idx ).cooked() );
            else if ( type( action ) == _VM_SC_Int64 && in( action ) )
                args.push_back( operandCk< PtrIntV >( idx ).cooked() );
            else if ( type( action ) == _VM_SC_Mem )
            {
                int size = operandCk< IntV >( idx ).cooked();
                bufs.emplace_back( size ? new char[ size ] : nullptr );
                args.push_back( long( bufs.back().get() ) );
                auto ptr = operand< PointerV >( idx + 1 );
                CharV ch;
                if ( !ptr.cooked().null() && !boundcheck( ptr, size, out( action ) ) )
                    return false;
                ptr = PointerV( ptr2h( ptr ) );
                if ( in( action ) )
                    for ( int i = 0; i < size; ++i )
                    {
                        heap().read_shift( ptr, ch );
                        if ( !ch.defined() )
                        {
                            fault( _VM_F_Hypercall ) << "uninitialised byte in __vm_syscall: argument "
                                                    << idx << ", offset " << i << ", action 0x"
                                                    << std::hex << action << ", size " << std::dec << size;
                            return false;
                        }
                        bufs.back()[ i ] = ch.cooked();
                    }
            }
            else if ( out( action ) )
            {
                auto ptr = operand< PointerV >( idx );
                if ( !ptr.cooked().null() &&
                        !boundcheck( ptr, type( action ) == _VM_SC_Int32 ? 4 : 8, true ) )
                    return false;
            }
            return true;
        };

        int buf_idx = 0;

        auto process = [&]( int action, long val )
        {
            if ( type( action ) == _VM_SC_Mem )
            {
                if ( out( action ) )
                {
                    int size = operand< IntV >( idx ).cooked();
                    auto ptr = PointerV( ptr2h( operand< PointerV >( idx + 1 ) ) );
                    for ( int i = 0; i < size; ++i )
                        heap().write_shift( ptr, CharV( bufs[ buf_idx ][ i ] ) );
                }
                ++ buf_idx;
            }
            else if ( out( action ) )
            {
                auto ptr = operand< PointerV >( idx );
                if (!ptr.cooked().null()) {
                    if ( type( action ) == _VM_SC_Int32 )
                        heap().write( ptr2h(ptr), IntV( val ) );
                    else
                        heap().write( ptr2h(ptr), PtrIntV( val ) );
                }
            }
        };

        if ( instruction().argcount() < 2 )
        {
            fault( _VM_F_Hypercall ) << "at least 2 arguments are required for __vm_syscall";
            return;
        }

        IntV id = operandCk< IntV >( idx++ );

        while ( idx < instruction().argcount() - 1 )
        {
            actions.push_back( operandCk< IntV >( idx++ ).cooked() );
            if ( !prepare( actions.back() ) )
                return;
            next( actions.back() );
        }

        auto rv = syscall_helper( id.cooked(), args, argt );
        result( IntV( errno ) );

        idx = 2;
        process( actions[ 0 ], rv );
        next( actions[ 0 ] );

        for ( int i = 1; i < int( actions.size() ); ++i )
        {
            idx ++; // skip action
            process( actions[ i ], args[ i - 1 ] );
            next( actions[ i ] );
        }
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_hypercall_clone()
    {
        auto ptr = operandCk< PointerV >( 0 ).cooked();
        auto block = operandCk< PointerV >( 1 );
        std::map< HeapPointer, HeapPointer > visited;

        if ( !block.cooked().null() )
        {
            if ( !ptr.heap() || !heap().valid( ptr ) )
            {
                fault( _VM_F_Hypercall ) << "invalid block pointer "
                                        << ptr << " passed to __vm_obj_clone";
                return;
            }

            while ( boundcheck_nop( block, PointerBytes, false ) )
            {
                PointerV blocked;
                heap().read_shift( block, blocked );
                visited.emplace( blocked.cooked(), nullPointer() );
            }
        }

        if ( !ptr.heap() || !heap().valid( ptr ) )
            fault( _VM_F_Hypercall ) << "invalid pointer " << ptr << " passed to __vm_obj_clone";
        else
            result( PointerV( mem::clone( heap(), heap(), ptr, visited, mem::CloneType::All ) ) );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_ctl_set_frame()
    {
        if ( instruction().argcount() > 3 )
        {
            fault( _VM_F_Hypercall ) << "too many arguments to __vm_ctl_set";
            return;
        }

        auto ptr = operandCk< PointerV >( 1 );

        if ( !ptr.cooked().null() && !boundcheck_nop( ptr, 2 * PointerBytes, true ) )
        {
            fault( _VM_F_Hypercall ) << "invalid target frame in __vm_ctl_set";
            return;
        }

        bool free = true;
        context().sync_pc();

        if ( context().flags_all( _VM_CF_KeepFrame ) )
        {
            context().flags_set( _VM_CF_KeepFrame, 0 );
            free = false;
        }
        else if ( ptr.cooked() == GenericPointer( frame() ) )
        {
            fault( _VM_F_Hypercall, GenericPointer(), CodePointer() )
                << " cannot target current frame without _VM_CF_KeepFrame";
            return;
        }

        auto caller_pc = pc();
        auto maybe_free = [&]
        {
            if ( !free ) return;
            collect_frame_locals( caller_pc, [&]( auto ptr, auto ) { freeobj( ptr.cooked() ); } );
            freeobj( frame() );
        };

        if ( ptr.cooked().null() )
        {
            maybe_free();
            context().flags_set( 0, _VM_CF_Stop );
            return context().set( _VM_CR_Frame, ptr.cooked() );
        }

        PointerV oldpc;
        heap().read( ptr.cooked(), oldpc );
        context().set( _VM_CR_PC, oldpc.cooked() );

        if ( instruction().argcount() == 3 )
        {
            auto target = operandCk< PointerV >( 2 );
            maybe_free();
            context().set( _VM_CR_Frame, ptr.cooked() );
            long_jump( target );
        }
        else if ( !ptr.cooked().null() )
        {
            auto opcode = [&]( int off ) { return program().instruction( oldpc.cooked() + off ).opcode; };

            if ( opcode( 0 ) == OpCode::PHI )
            {
                fault( _VM_F_Hypercall ) << "cannot transfer control into the middle of a phi block";
                return;
            }

            if ( opcode( 0 ) == lx::OpBB && opcode( 1 ) == OpCode::PHI )
            {
                fault( _VM_F_Hypercall ) << "cannot transfer control directly to a basic block w/ phi nodes";
                return;
            }

            maybe_free();
            context().set( _VM_CR_Frame, ptr.cooked() );
        }

        update_shuffle();
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_ctl_set()
    {
        auto reg = _VM_ControlRegister( operandCk< IntV >( 0 ).cooked() );

        switch ( reg ) /* permission check */
        {
            case _VM_CR_Flags:
            case _VM_CR_Globals:
                if ( !assert_flag( _VM_CF_KernelMode, "cannot change register in user mode" ) )
                    return;
                else break;
            case _VM_CR_State:
            case _VM_CR_Scheduler:
            case _VM_CR_FaultHandler:
                if ( !assert_flag( _VM_CF_Booting, "can only change register during boot" ) )
                    return;
                else break;
            case _VM_CR_Constants:
            case _VM_CR_ObjIdShuffle:
                fault( _VM_F_Access ) << "attempted to change (immutable) control register " << reg;
                return;
            default: break;
        }

        if ( reg == _VM_CR_Frame )
            return implement_ctl_set_frame();

        if ( instruction().argcount() > 2 )
        {
            fault( _VM_F_Hypercall ) << "too many arguments to __vm_ctl_set";
            return;
        }

        if ( reg == _VM_CR_Flags )
        {
            uint64_t want = operandCk< PtrIntV >( 1 ).cooked();
            uint64_t change = context().flags() ^ want;
            if ( change & _VM_CF_DebugMode )
            {
                fault( _VM_F_Access ) << "debug mode cannot be changed";
                return;
            }
            context().set( reg, operandCk< PtrIntV >( 1 ).cooked() );
        }
        else
            context().set( reg, operandCk< PointerV >( 1 ).cooked() );

    }

    template< typename Ctx >
    void Eval< Ctx >::implement_ctl_get()
    {
        auto reg = _VM_ControlRegister( operandCk< IntV >( 0 ).cooked() );

        switch ( reg ) /* permission check */
        {
            case _VM_CR_Globals:
            case _VM_CR_Constants:
            case _VM_CR_State:
            case _VM_CR_Scheduler:
            case _VM_CR_ObjIdShuffle:
                if ( !assert_flag( _VM_CF_KernelMode, "register " + brick::string::fmt( reg ) +
                                " only readable in kernel mode" ) )
                    return;
                else break;
            default: break;
        }

        /* TODO mask out privileged bits from _VM_CR_Flags? */
        if ( reg == _VM_CR_Flags )
            result( PtrIntV( context().flags() ) );
        else
            result( PointerV( context().get_ptr( reg ) ) );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_ctl_flag()
    {
        uint64_t clear = operandCk< PtrIntV >( 0 ).cooked(),
                set = operandCk< PtrIntV >( 1 ).cooked(),
                change = clear | set;

        if ( set & _VM_CF_KernelMode )
            if ( !program().traps( pc() ) )
            {
                fault( _VM_F_Access ) << "cannot enter kernel mode here";
                return;
            }

        if ( set & _VM_CF_Booting )
        {
            fault( _VM_F_Access ) << "the 'booting' flag cannot be changed";
            return;
        }

        if ( change & _VM_CF_DebugMode )
        {
            fault( _VM_F_Access ) << "the 'debug' flag cannot be changed";
            return;
        }

        if ( change & _VM_CF_Error )
        {
            if ( !assert_flag( _VM_CF_KernelMode, "the error flag can be only changed in kernel mode" ) )
                return;
            if ( context().fault_str().size() )
                context().trace( "FAULT: " + context().fault_str() );
            context().fault_clear();
        }

        result( PtrIntV( context().flags() ) );
        context().flags_set( clear, set );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_test_loop()
    {
        auto counter = operandCk< IntV >( 0 ).cooked();

        if ( context().test_loop( pc(), counter ) )
        {
            TRACE( "test_loop: loop closed at", counter );
            context().sync_pc();
            CodePointer h = operandCk< PointerV >( 1 ).cooked();
            make_frame( context(), h, PointerV( frame() ) );
        }
        else
            TRACE( "test_loop: not a loop", counter );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_test_crit()
    {
        auto ptr = operand< PointerV >( 0 );

        if ( ptr.cooked().type() == PointerType::Global && ptr2s( ptr.cooked() ).location == Slot::Const )
            return;

        auto size = operandCk< IntV >( 1 ).cooked();

        if ( !boundcheck_nop( ptr, size, false ) )
        {
            if ( !context().flags_any( _VM_CF_Error ) )
                boundcheck( ptr, size, false );
            return;
        }

        auto at = operandCk< IntV >( 2 ).cooked();

        if ( !context().test_crit( pc(), ptr.cooked(), size, at ) )
            return;

        context().sync_pc();
        CodePointer h = operandCk< PointerV >( 3 ).cooked();
        make_frame( context(), h, PointerV( frame() ) );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_test_taint()
    {
        CodePointer tf = operandCk< PointerV >( 0 ).cooked();
        CodePointer nf;

        if ( operand( 1 ).type == Slot::PtrC )
            nf = operandCk< PointerV >( 1 ).cooked();

        uint8_t taints = 0;
        for ( int i = 2; i < instruction().argcount(); ++i )
            op< Any >( i + 1, [&]( auto v ) { taints |= v.arg( i ).taints(); } );

        context().sync_pc();
        auto oldframe = frame();

        MakeFrame< Ctx > mkframe( context(), taints ? tf : nf, true );

        if ( taints )
            mkframe.enter( PointerV( frame() ) );
        else if ( nf.function() ) /* what to call for non-tainted */
            mkframe.enter( PointerV( frame() ) );
        else
        {
            slot_copy( s2ptr( operand( 1 ) ), result(), result().size() );
            return;
        }

        auto newframe = frame();

        for ( int i = 2; i < instruction().argcount(); ++i )
            op< Any >( i + 1, [&]( auto v )
            {
                context().set( _VM_CR_Frame, oldframe );
                auto arg = v.arg( i );
                context().set( _VM_CR_Frame, newframe );
                auto taint_v = value::Int< 8 >( arg.taints() );
                if ( taints )
                    mkframe.push( (i - 2) * 2, frame(), taint_v, arg );
                else
                    mkframe.push( (i - 2), frame(), arg );
            } );
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_peek()
    {
        auto ptr = operandCk< PointerV >( 0 );
        if ( !ptr.defined() || !boundcheck( ptr, 1, false ) )
            return;
        int key = operandCk< IntV >( 1 ).cooked();
        auto loc = heap().loc( ptr2h( ptr ) );
        switch ( key )
        {
            case _VM_ML_Pointers:
            {
                auto ptrs = heap().pointers( loc, 1 );
                GenericPointer::ObjT obj = 0;

                for ( auto p : ptrs ) /* there's at most 1 */
                    switch ( p.size() )
                    {
                        case 1: obj = p.fragment(); break;
                        default: NOT_IMPLEMENTED();
                    }
                return result( IntV( obj ) );
            }
            default:
                result( heap().peek( loc, key - _VM_ML_User ) );
        }
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_poke()
    {
        auto ptr = operandCk< PointerV >( 0 );
        if ( !ptr.defined() || !boundcheck( ptr, 1, true ) )
            return;
        HeapPointer where = ptr2h( ptr );
        int layer = operandCk< IntV >( 1 ).cooked();
        auto value = operandCk< value::Int< 32, false > >( 2 );
        auto loc = heap().loc( where );

        switch ( layer )
        {
            case _VM_ML_Taints:
            {
                IntV data;
                heap().read( where, data );
                data.taints( value.cooked() );
                heap().write( where, data );
                break;
            }
            default:
                ASSERT( layer >= _VM_ML_User );
                heap().poke( loc, layer - _VM_ML_User, value );
        }
    }

    template< typename Ctx >
    void Eval< Ctx >::implement_hypercall()
    {
        switch( instruction().subcode )
        {
            case lx::HypercallChoose:
            {
                int options = operandCk< IntV >( 0 ).cooked();
                std::vector< int > p;
                for ( int i = 1; i < int( instruction().argcount() ); ++i )
                    p.push_back( operandCk< IntV >( i ).cooked() );
                if ( !p.empty() && int( p.size() ) != options )
                    fault( _VM_F_Hypercall );
                else
                {
                    int choice = context().choose( options, p.begin(), p.end() );
                    if ( choice >= 0 )
                        result( IntV( choice ) );
                }
                return;
            }

            case lx::HypercallCtlSet:  return implement_ctl_set();
            case lx::HypercallCtlGet:  return implement_ctl_get();
            case lx::HypercallCtlFlag: return implement_ctl_flag();

            case lx::HypercallTestLoop: return implement_test_loop();
            case lx::HypercallTestCrit: return implement_test_crit();
            case lx::HypercallTestTaint: return implement_test_taint();

            case lx::HypercallPeek: return implement_peek();
            case lx::HypercallPoke: return implement_poke();

            case lx::HypercallSyscall:
                return implement_hypercall_syscall();

            case lx::HypercallTrace:
            {
                _VM_Trace t = _VM_Trace( operandCk< IntV >( 0 ).cooked() );
                switch ( t )
                {
                    case _VM_T_Text:
                        context().trace( TraceText{ ptr2h( operandCk< PointerV >( 1 ) ) } );
                        return;
                    case _VM_T_Fault:
                    {
                        auto arg = ptr2h( operandCk< PointerV >( 1 ) );
                        context().trace( TraceFault{ heap().read_string( arg ) } );
                        return;
                    }
                    case _VM_T_StateType:
                        context().trace( TraceStateType{ pc() } );
                        return;
                    case _VM_T_SchedChoice:
                        context().trace( TraceSchedChoice{
                                PointerV( ptr2h( operandCk< PointerV >( 1 ) ) ) } );
                        return;
                    case _VM_T_SchedInfo:
                        context().trace( TraceSchedInfo{ operandCk< IntV >( 1 ).cooked(),
                                                            operandCk< IntV >( 2 ).cooked() } );
                        return;
                    case _VM_T_TaskID:
                        context().trace( TraceTaskID{ operandCk< PointerV >( 1 ).cooked() } );
                        return;
                    case _VM_T_Info:
                        context().trace( TraceInfo{ ptr2h( operandCk< PointerV >( 1 ) ) } );
                        return;
                    case _VM_T_Assume:
                        context().trace( TraceAssume{ ptr2h( operandCk< PointerV >( 1 ) ) } );
                        return;
                    case _VM_T_LeakCheck:
                        context().trace( TraceLeakCheck() );
                        return;
                    case _VM_T_TypeAlias:
                        context().trace( TraceTypeAlias{ pc(), ptr2h( operandCk< PointerV >( 2 ) ) } );
                        return;
                    case _VM_T_DebugPersist:
                        context().trace( TraceDebugPersist{ operandCk< PointerV >( 1 ).cooked() } );
                        return;
                    default:
                        fault( _VM_F_Hypercall ) << "invalid __vm_trace type " << t;
                }
                return;
            }

            case lx::HypercallFault:
                fault( Fault( operandCk< IntV >( 0 ).cooked() ) ) << "__vm_fault called";
                return;

            case lx::HypercallObjMake:
            {
                int64_t size = operandCk< IntV >( 0 ).cooked();
                auto type = PointerType( operandCk< IntV >( 1 ).cooked() );
                if ( size >= ( 2ll << _VM_PB_Off ) || size < 1 )
                {
                    result( nullPointerV() );
                    fault( _VM_F_Hypercall ) << "invalid size " << size
                                                << " passed to __vm_obj_make";
                    return;
                }
                result( size ? makeobj( size, type ) : nullPointerV() );
                return;
            }
            case lx::HypercallObjFree:
            {
                auto op = operand< PointerV >( 0 );
                auto ptr = op.cooked();
                if ( !op.defined() )
                    fault( _VM_F_Memory ) << "undefined pointer passed to __vm_obj_free";
                else if ( !ptr.heap() )
                    fault( _VM_F_Memory ) << "non-heap pointer passed to __vm_obj_free";
                else if ( !freeobj( ptr ) )
                    fault( _VM_F_Memory ) << "invalid pointer passed to __vm_obj_free";
                return;
            }
            case lx::HypercallObjResize:
            {
                auto ptr = operandCk< PointerV >( 0 ).cooked();
                if ( !ptr.heap() )
                    fault( _VM_F_Memory ) << "non-heap pointer passed to __vm_obj_resize";
                else if ( !heap().resize( ptr, operandCk< IntV >( 1 ).cooked() ) )
                    fault( _VM_F_Memory ) << "invalid pointer passed to __vm_obj_resize";
                return;
            }
            case lx::HypercallObjSize:
            {
                auto ptr = operandCk< PointerV >( 0 ).cooked();
                if ( !ptr.heap() || !heap().valid( ptr ) )
                    fault( _VM_F_Hypercall ) << "invalid pointer " << ptr << " passed to __vm_obj_size";
                else
                    result( IntV( heap().size( ptr ) ) );
                return;
            }
            case lx::HypercallObjClone:
                return implement_hypercall_clone();
            default:
                UNREACHABLE( "unknown hypercall", instruction().subcode );
        }
    }
}
