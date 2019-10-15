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

#include <divine/mc/ctx-assume.hpp>
#include <divine/mc/exec.hpp>
#include <divine/mc/search.hpp>
#include <divine/mc/bitcode.hpp>
#include <divine/mc/machine.hpp>
#include <divine/mc/weaver.hpp>
#include <divine/vm/eval.tpp>

namespace divine::mc
{
    template< typename next >
    struct print_trace : next
    {
        using next::trace;
        void trace( std::string s ) { std::cout << s << std::endl; }
        void trace( vm::TraceInfo ti ) { trace( this->heap().read_string( ti.text ) ); }
    };

    namespace ctx = vm::ctx;
    struct ctx_exec : brq::compose_stack< ctx_assume, ctx_choice, brq::module< print_trace >,
                                          ctx::with_debug, ctx::with_tracking,
                                          ctx::common< vm::Program, vm::CowHeap > > {};

    template< typename next >
    struct backtrack_ : next
    {
        using typename next::tq;
        using typename next::task_choose;

        std::stack< task_choose > _stack;

        bool feasible( tq q ) override
        {
            if ( next::feasible( q ) )
                return true;
            if ( !this->context().flags_any( _VM_CF_Stop ) && !_stack.empty() )
            {
                TRACE( "encountered an infeasible path, backtracking", _stack.top() );
                push< task_choose >( q, _stack.top() );
                _stack.pop();
            }
            return false;
        }

        void choose( tq q, task_choose &c )
        {
            c.total = -c.total;
            if ( c.total < 0 )
            {
                _stack.push( c );
                c.cancel();
            }
            else
                next::choose( q, c );
        };

        ~backtrack_()
        {
            while ( !_stack.empty() )
                this->snap_put( _stack.top().snap ), _stack.pop();
        }
    };

    struct backtrack : brq::module< backtrack_ > {};

    struct mach_exec : brq::compose_stack< machine::search_dispatch, backtrack,
                                           machine::compute,
                                           machine::tree_search,
                                           machine::choose_random, machine::choose,
                                           machine::with_context< ctx_exec >,
                                           machine::base< smt::NoSolver > > {};

    struct search : mc::Search< mc::State, mc::Label > {};

    void Exec::run()
    {
        mach_exec m;
        m.bc( _bc );
        m.context().enable_debug();
        weave( m ).extend( search() ).start();
    }
}

