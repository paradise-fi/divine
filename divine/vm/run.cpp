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

#include <divine/vm/run.hpp>
#include <divine/vm/bitcode.hpp>
#include <divine/vm/heap.hpp>
#include <divine/vm/eval.hpp>
#include <divine/vm/context.hpp>
#include <divine/vm/control.hpp>

namespace divine {
namespace vm {

struct RunControl : Control< MutableHeap >
{
    std::mt19937 _rand;
    using Control::Control;

    template< typename I >
    int choose( int o, I, I )
    {
        std::uniform_int_distribution< int > dist( 0, o - 1 );
        return dist( _rand );
    }

    void doublefault()
    {
        std::cerr << "E: Double fault, program terminated." << std::endl;
        _frame = nullPointer();
    }

    void trace( std::string s ) { std::cerr << "T: " << s << std::endl; }
};

struct RunContext : Context< RunControl, MutableHeap >
{
    RunContext( Program &p ) : Context( p ) {}
};

void Run::run()
{
    using Eval = vm::Eval< Program, RunContext, RunContext::PointerV >;
    auto &program = _bc->program();
    vm::RunContext _ctx( program );
    _ctx.setup( _env );
    Eval eval( program, _ctx );
    _ctx.control().mask( true );
    eval.run();
    auto state = eval._result;

    while ( state.cooked() != vm::nullPointer() )
    {
        _ctx.control().enter(
            _ctx._control._sched, vm::nullPointer(),
            Eval::IntV( eval.heap().size( state.cooked() ) ), state );
        _ctx.control().mask( true );
        eval.run();
        state = eval._result;
    }
}

}
}
