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

#include <divine/vm/types.hpp>
#include <divine/smt/builder.hpp>
#include <divine/smt/extract.hpp>
#include <vector>
#include <brick-except>
#include <brick-timer>

#if OPT_STP
#include <stp/STPManager/STPManager.h>
#include <stp/STPManager/STP.h>
#include <stp/ToSat/AIG/ToSATAIG.h>
#endif

#if OPT_Z3
#include <z3++.h>
#endif

namespace divine::smt
{
    struct feasibility_timer_tag;
    struct equality_timer_tag;

    using feasibility_timer = brq::timer< feasibility_timer_tag >;
    using equality_timer    = brq::timer< equality_timer_tag >;
}

namespace divine::smt::solver
{

using namespace std::literals;

using SymPairs = std::vector< std::pair< vm::HeapPointer, vm::HeapPointer > >;
enum class Result { False, True, Unknown };

struct None
{
    bool equal( vm::HeapPointer, SymPairs &, vm::CowHeap &, vm::CowHeap & ) { return true; }

    bool feasible( vm::CowHeap &, vm::HeapPointer a )
    {
        brq::raise() << "Cannot evaluate an assumption without a solver.\n"
                     << "Did you mean to use --symbolic?";
        return true;
    }

    void reset() {}
};

template< typename Core >
struct Simple : Core
{
    using Core::Core;
    bool equal( vm::HeapPointer path, SymPairs &sym_pairs, vm::CowHeap &h1, vm::CowHeap &h2 );
    bool feasible( vm::CowHeap & heap, vm::HeapPointer assumes );
};

template< typename Core >
struct Incremental : Simple< Core >
{
    using Simple< Core >::Simple;
    bool feasible( vm::CowHeap & heap, vm::HeapPointer assumes );
    std::vector< vm::HeapPointer > _inc;
    void reset() { _inc.clear(); Simple< Core >::reset(); }
};

template< typename Core >
struct Caching : Simple< Core >
{
    using Simple< Core >::Simple;
    bool feasible( vm::CowHeap & heap, vm::HeapPointer assumes );
    std::unordered_map< vm::HeapPointer, bool > _sat;
    std::unordered_map< vm::HeapPointer, int > _hits;
};

struct SMTLib
{
    using Options = std::vector< std::string >;
    SMTLib( const Options &opts ) : _opts{ opts } {}

    void reset() { _asserts.clear(); _ctx.clear(); }
    void add( brq::smtlib_node p ) { _asserts.push_back( p ); }

    Result solve();

    auto builder( int id = 0 )
    {
        return builder::SMTLib2( _ctx, "_"s + char( 'a' + id ) );
    }

    auto extract( vm::CowHeap &h, int id = 0 )
    {
        return extract::SMTLib2( h, _ctx, "_"s + char( 'a' + id ) );
    }

    std::vector< brq::smtlib_node > _asserts;
    brq::smtlib_context _ctx;
    Options _opts;
};

#if OPT_STP

struct STP
{
    STP() : _simp( &_mgr ), _at( &_mgr, &_simp ),
            _ts( &_mgr, &_at ), _ce( &_mgr, &_simp, &_at ),
            _stp( &_mgr, &_simp, &_at, &_ts, &_ce )
    {
        reset();
    }

    STP( const STP & ) : STP () {}

    void reset();
    void clear();
    void push();
    void pop();
    void add( stp::ASTNode n ) { _mgr.AddAssert( n ); }

    Result solve();
    builder::STP builder( int = 0 ) { return builder::STP( _mgr ); }
    extract::STP extract( vm::CowHeap &h, int = 0 ) { return extract::STP( h, _mgr ); }

    stp::STPMgr _mgr;
    stp::Simplifier _simp;
    stp::ArrayTransformer _at;
    stp::ToSATAIG _ts; /* how about AIG? */
    stp::AbsRefine_CounterExample _ce;
    stp::STP _stp;
};

#endif

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

#if OPT_STP
using STPSolver = solver::Simple< solver::STP >; // TODO incremental
#endif

}
