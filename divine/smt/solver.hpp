// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/heap.hpp>
#include <divine/smt/builder.hpp>
#include <divine/smt/extract.hpp>
#include <vector>

#if OPT_Z3
#include <z3++.h>
#endif

namespace divine::smt::solver
{

using namespace std::literals;

using SymPairs = std::vector< std::pair< vm::HeapPointer, vm::HeapPointer > >;
enum class Result { False, True, Unknown };

struct None
{
    bool equal( SymPairs &, vm::CowHeap &, vm::CowHeap & ) { UNREACHABLE( "no equality check" ); }
    bool feasible( vm::CowHeap &, vm::HeapPointer ) { return true; }
    void reset() {}
};

template< typename Core >
struct Simple : Core
{
    bool equal( SymPairs &sym_pairs, vm::CowHeap &h1, vm::CowHeap &h2 );
    bool feasible( vm::CowHeap & heap, vm::HeapPointer assumes );
};

template< typename Core >
struct Incremental : Simple< Core >
{
    bool feasible( vm::CowHeap & heap, vm::HeapPointer assumes );
    std::vector< vm::HeapPointer > _inc;
};

struct SMTLib
{
    using Options = std::vector< std::string >;
    SMTLib() : _opts{ "z3", "-in", "-smt2" } {}

    void reset() { _asserts.clear(); _ctx.clear(); }
    void add( brick::smt::Node p ) { _asserts.push_back( p ); }

    Result solve();

    auto builder( int id = 0 )
    {
        return builder::SMTLib2( _ctx, "_"s + char( 'a' + id ) );
    }

    auto extract( vm::CowHeap &h, int id = 0 )
    {
        return extract::SMTLib2( h, _ctx, "_"s + char( 'a' + id ) );
    }

    std::vector< brick::smt::Node > _asserts;
    brick::smt::Context _ctx;
    Options _opts;
};

#if OPT_Z3

struct Z3
{
    Z3() : _solver( _ctx ) { reset(); }
    Z3( const Z3 & ) : Z3() {}

    builder::Z3 builder( int = 0 ) { return builder::Z3( _ctx ); }
    extract::Z3 extract( vm::CowHeap &h, int = 0 ) { return extract::Z3( h, _ctx ); }

    Result solve()
    {
        switch ( _solver.check() )
        {
            case z3::check_result::unsat:   return Result::False;
            case z3::check_result::sat:     return Result::True;
            case z3::check_result::unknown: return Result::Unknown;
        }
    }

    void add( z3::expr e ) { _solver.add( e ); }
    void push()            { _solver.push();  }
    void pop()             { _solver.pop();   }
    void reset()           { _solver.reset(); }
private:
    z3::context _ctx;
    z3::solver _solver;
};

#endif

}

namespace divine::smt
{

using SMTLibSolver = solver::Simple< solver::SMTLib >;
using NoSolver = solver::None;

#if OPT_Z3
using Z3Solver = solver::Simple< solver::Z3 >;
#endif

}
