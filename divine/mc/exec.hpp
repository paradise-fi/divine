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

#include <random>
#include <divine/vm/context.hpp>
#include <divine/dbg/context.hpp>
#include <divine/vm/program.hpp>
#include <divine/vm/memory.hpp>
#include <divine/mc/types.hpp>

namespace divine::mc
{

struct BitCode;

template< typename Super >
struct ExecContext_ : Super
{
    std::mt19937 _rand;
    using Super::Super;
    using typename Super::Heap;
    std::vector< std::string > _trace;

    template< typename I >
    int choose( int o, I, I )
    {
        std::uniform_int_distribution< int > dist( 0, o - 1 );
        return dist( _rand );
    }

    void doublefault()
    {
        std::cerr << "E: Double fault, program terminated." << std::endl;
        this->set( _VM_CR_Frame, vm::nullPointer() );
        this->flags_set( 0, _VM_CF_Cancel );
    }

    using Super::trace;
    void trace( std::string s ) { std::cout << s << std::endl; }
    void trace( vm::TraceText tt ) { trace( this->heap().read_string( tt.text ) ); }
    void trace( vm::TraceSchedInfo ) { NOT_IMPLEMENTED(); }
    void trace( vm::TraceSchedChoice ) {}
    void trace( vm::TraceStateType ) {}
    void trace( vm::TraceInfo ti )
    {
        std::cout << this->heap().read_string( ti.text ) << std::endl;
    }
    void trace( vm::TraceAssume ) {}
    void trace( vm::TraceTypeAlias ) {}
    void trace( vm::TraceTaskID ) {}
    void trace( vm::TraceDebugPersist ) {} /* noop, since snapshots are not
                                            * restored here */
};

using ExecContext  = ExecContext_< vm::Context< vm::Program, vm::MutableHeap > >;
using TraceContext = ExecContext_< dbg::Context< vm::MutableHeap > >;

struct Exec
{
    using BC = std::shared_ptr< BitCode >;
    using Env = std::vector< std::string >;

    BC _bc;
    PoolStats _ps;

    Exec( BC bc ) : _bc( bc ) {}

    template< typename solver_t, template< typename > typename exec_t, typename tactic_t >
    void do_run();
    void run( bool exhaustive, std::string_view tactic = "none" );

    void trace();

    PoolStats poolstats() { return _ps; }
};

}
