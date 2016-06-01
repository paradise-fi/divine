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

namespace divine {
namespace vm {

struct RunContext
{
    using Heap = MutableHeap;
    using HeapPointer = Heap::Pointer;
    using PointerV = value::Pointer< HeapPointer >;

    struct Control
    {
        Heap &_heap;
        Program &_program;
        PointerV _constants, _globals, _frame, _entry_frame, _ifl;
        CodePointer _sched, _fault;
        bool _mask;
        std::mt19937 _rand;

        Control( Heap &h, Program &p ) : _heap( h ), _program( p ), _rand( 0 ) {}

        PointerV frame() { return _frame; }
        void frame( PointerV p ) { _frame = p; }
        PointerV globals() { return _globals; }
        PointerV constants() { return _constants; }

        void push( PointerV ) {}

        template< typename X, typename... Args >
        void push( PointerV p, X x, Args... args )
        {
            _heap.shift( p, x );
            push( p, args... );
        }

        template< typename... Args >
        void enter( CodePointer pc, PointerV parent, Args... args )
        {
            int datasz = _program.function( pc ).datasize;
            auto frameptr = _heap.make( datasz + 2 * PointerBytes );
            _frame = frameptr;
            if ( parent.v() == nullPointer() )
                _entry_frame = _frame;
            _heap.shift( frameptr, PointerV( pc ) );
            _heap.shift( frameptr, parent );
            push( frameptr, args... );
        }

        void interrupt()
        {
            HeapPointer ifl = _ifl.v();
            if ( _ifl.v() != nullPointer() )
                _heap.write( _ifl.v(), _frame );
            _frame = _entry_frame;
            _ifl = PointerV();
            _mask = true;
        }

        void fault( Fault f )
        {
            if ( _fault == CodePointer( nullPointer() ) )
                doublefault();
            else
                enter( _fault, _frame, value::Int< 32 >( f ) );
        }

        bool mask( bool n )  { bool o = _mask; _mask = n; return o; }
        bool mask() { return _mask; }

        template< typename I >
        int choose( int o, I, I )
        {
            std::uniform_int_distribution< int > dist( 0, o - 1 );
            return dist( _rand );
        }

        bool isEntryFrame( HeapPointer fr ) { return HeapPointer( _entry_frame.v() ) == fr; }
        void setSched( CodePointer p ) { _sched = p; _sched.instruction( 1 ); }
        void setFault( CodePointer p ) { _fault = p; }
        void setIfl( PointerV p ) { _ifl = p; }

        void doublefault()
        {
            std::cerr << "E: Double fault, program terminated." << std::endl;
            _frame = nullPointer();
        }
        void trace( std::string s ) { std::cerr << "T: " << s << std::endl; }
    };

    std::vector< std::tuple< std::string, Program::SlotRef > > _env;

    Heap _heap;
    Control _control;
    Program &_program;

    Heap &heap() { return _heap; }
    Control &control() { return _control; }

    /* must be called before setup() */
    void putenv( std::string str )
    {
        Program::Slot s( str.size() + 1, Program::Slot::Constant );
        s.type = Program::Slot::Aggregate;
        auto sref = _program.allocateSlot( s );
        _env.push_back( std::make_tuple( str, sref ) );
    }

    void setup_env()
    {
        auto &heap = _program._ccontext.heap();
        for ( auto e : _env )
        {
            auto str = std::get< 0 >( e );
            auto ptr = PointerV( _program.s2hptr( std::get< 1 >( e ).slot ) );
            std::for_each( str.begin(), str.end(),
                           [&]( char c )
                           {
                               heap.shift( ptr, value::Int< 8 >( c ) );
                           } );
            heap.shift( ptr, value::Int< 8 >( 0 ) );
        }
    }

    template< typename C >
    void setup( const C &env )
    {
        _program.setupRR();
        _program.computeRR();

        for ( auto e : env )
            putenv( e );

        _program.computeStatic();
        setup_env();
        std::tie( _control._constants, _control._globals ) = _program.exportHeap( heap() );

        auto ipc = _program.functionByName( "__sys_init" );
        auto envptr = _program.globalByName( "__sys_env" );
        ipc.instruction( 1 );
        _control.enter( ipc, nullPointer(), PointerV( envptr ) );
    }

    RunContext( Program &p )
        : _program( p ), _control( _heap, p )
    {}
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

    while ( state.v() != vm::nullPointer() )
    {
        _ctx.control().enter(
            _ctx._control._sched, vm::nullPointer(),
            Eval::IntV( eval.heap().size( state.v() ) ), state );
        _ctx.control().mask( true );
        eval.run();
        state = eval._result;
    }
}

}
}
