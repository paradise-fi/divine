// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/pointer.hpp>
#include <divine/dbg/info.hpp>
#include <divine/dbg/print.hpp>
#include <divine/vm/program.hpp>
#include <divine/vm/value.hpp>
#include <divine/vm/setup.hpp>
#include <set>

namespace divine::dbg
{

template< typename Context >
struct Stepper
{
    enum Verbosity { Quiet, TraceOnly, PrintSource, PrintInstructions, TraceInstructions };
    using Snapshot = typename Context::Heap::Snapshot;
    using YieldState = std::function< Snapshot( Snapshot ) >;
    using CallBack = std::function< bool() >;
    using Breakpoint = std::function< bool( vm::CodePointer, bool ) >;

    vm::GenericPointer _frame, _frame_cur, _parent_cur;
    llvm::Instruction *_insn_last;
    Components _ff_components;
    bool _stop_on_fault, _stop_on_error, _stop_on_accept, _booting, _break;
    bool _sigint;
    std::pair< int, int > _lines, _instructions, _states, _jumps;
    std::pair< llvm::StringRef, int > _line;

    Breakpoint _breakpoint;
    YieldState _yield_state;
    CallBack _callback;

    Stepper()
        : _frame( vm::nullPointer() ),
          _frame_cur( vm::nullPointer() ),
          _parent_cur( vm::nullPointer() ),
          _insn_last( nullptr ),
          _stop_on_fault( false ), _stop_on_error( true ), _stop_on_accept( false ), _booting( false ),
          _sigint( false ),
          _lines( 0, 0 ), _instructions( 0, 0 ),
          _states( 0, 0 ), _jumps( 0, 0 ),
          _line( "", 0 )
    {}

    void lines( int l ) { _lines.second = l; }
    void instructions( int i ) { _instructions.second = i; }
    void states( int s ) { _states.second = s; }
    void jumps( int j ) { _jumps.second = j; }
    void frame( vm::GenericPointer f ) { _frame = f; }
    auto frame() { return _frame; }

    void add( std::pair< int, int > &p )
    {
        if ( _frame.null() || _frame == _frame_cur )
            p.first ++;
    }

    bool _check( std::pair< int, int > &p )
    {
        return p.second && p.first >= p.second;
    }

    bool check_location( vm::CodePointer pc, const llvm::Instruction *op )
    {
        bool dbg_changed = !_insn_last || _insn_last->getDebugLoc() != op->getDebugLoc();

        if ( dbg_changed )
        {
            auto l = dbg::fileline( *op );
            if ( _line.second && l != _line )
                add( _lines );
            if ( _frame.null() || _frame == _frame_cur )
                _line = l;
        }
        if ( _breakpoint )
            return _breakpoint( pc, dbg_changed );
        return false;
    }

    template< typename Eval >
    bool check( Context &ctx, Eval &eval, vm::CodePointer oldpc, bool moved )
    {
        if ( moved && check_location( eval.pc(), ctx.debug().find( nullptr, oldpc ).first ) )
            return true;
        if ( !_frame.null() && !ctx.heap().valid( _frame ) )
            return true;
        if ( _check( _jumps ) )
            return true;
        if ( !_frame.null() && _frame_cur != _frame )
            return false;
        return _check( _lines ) || _check( _instructions ) || _check( _states );
    }

    void instruction()
    {
        add( _instructions );
    }

    void state() { add( _states ); }

    template< typename Heap >
    void in_frame( vm::GenericPointer next, Heap &heap );

    bool schedule( Context &ctx )
    {
        if ( !ctx.frame().null() )
            return false; /* nothing to be done */

        if ( _stop_on_error && ( ctx.flags_any( _VM_CF_Error ) ) )
            return false; /* can't schedule if there is an error and we should stop */

        if ( _booting && ctx.frame().null() && !vm::setup::postboot_check( ctx ) )
            return false; /* boot failed */

        if ( ctx.flags_any( _VM_CF_Cancel ) )
            return true;

        _booting = false;
        if ( _yield_state )
            ctx.load( _yield_state( ctx.snapshot() ) );
        if ( ctx.scheduler().type() == vm::PointerType::Code )
            vm::setup::scheduler( ctx );
        else
            return false;
        return true;
    }

    void run( Context &ctx, Verbosity verb );
};

}

