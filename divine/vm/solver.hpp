// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
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
#include <divine/vm/formula.hpp>
#include <vector>

#if OPT_Z3
#include <z3++.h>
#endif

namespace divine::vm
{

using SymPairs = std::vector< std::pair< HeapPointer, HeapPointer > >;

struct Solver
{
    enum class Result { False, True, Unknown };
};

struct NoSolver
{
    using Result = Solver::Result;
    Result equal( SymPairs &, CowHeap &, CowHeap & ) const { UNREACHABLE( "no equality check" ); }
    Result feasible( CowHeap &, HeapPointer ) const { return Result::True; }
};

struct SMTLibSolver : Solver
{
    using Solver::Solver;

    using Options = std::vector< std::string >;

    SMTLibSolver( Options && opts ) : _opts( std::move( opts ) ) {}

    Result equal( SymPairs &sym_pairs, CowHeap &h1, CowHeap &h2 ) const;
    Result feasible( CowHeap & heap, HeapPointer assumes ) const;
private:
    Result query( const std::string & formula ) const;

    Options options() const { return _opts; }

    const Options _opts;
};

struct Z3SMTLibSolver : SMTLibSolver
{
    Z3SMTLibSolver() : SMTLibSolver( { "z3", "-in", "-smt2" } ) {}
};

struct BoolectorSMTLib : SMTLibSolver
{
    BoolectorSMTLib() : SMTLibSolver( { "boolector", "--smt2" } ) {}
};

#if OPT_Z3
struct Z3Solver : Solver
{
    using Solver::Solver;

    Z3Solver() : solver( ctx ) {}
    Z3Solver( const Z3Solver & ) : ctx(), solver( ctx ) {}

    Result equal( SymPairs &sym_pairs, CowHeap &h1, CowHeap &h2 );
    Result feasible( CowHeap & heap, HeapPointer assumes );
private:
    z3::context ctx;
    z3::solver solver;
};
#else
using Z3Solver = Z3SMTLibSolver;
#endif

struct SymbolicConfig
{
    SymbolicConfig( std::string solver ) : solver( solver ) {}
    const std::string solver;
};

}
