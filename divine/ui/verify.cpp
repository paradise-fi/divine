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

#include <divine/vm/explore.hpp>
#include <divine/vm/bitcode.hpp>
#include <divine/vm/debug.hpp>
#include <divine/ss/search.hpp>
#include <divine/ui/cli.hpp>

namespace divine {
namespace ui {

struct StContext
{
    using Heap = vm::explore::Context::Heap;
    using HeapPointer = Heap::Pointer;
    using PointerV = vm::value::Pointer< HeapPointer >;

    vm::Program _program;
    vm::explore::State _state;

    vm::Program &program() { return _program; }
    PointerV globals() { return _state.globals; }
    PointerV constants() { return _state.constants; }
    bool mask( bool = false ) { return false; }
    bool set_interrupted( bool = false ) { return false; }
    void check_interrupt() {}
    void cfl_interrupt( vm::CodePointer ) {}

    template< typename I >
    int choose( int, I, I ) { return 0; }

    PointerV frame() { NOT_IMPLEMENTED(); }
    void frame( PointerV p ) { NOT_IMPLEMENTED(); }
    bool isEntryFrame( HeapPointer fr ) { NOT_IMPLEMENTED(); }

    void fault( vm::Fault f, PointerV, vm::CodePointer ) { NOT_IMPLEMENTED(); }
    void trace( std::string s ) { std::cerr << "T: " << s << std::endl; }

    template< typename... Args >
    void enter( vm::CodePointer pc, PointerV parent, Args... args )
    {
        NOT_IMPLEMENTED();
    }

    bool sched( vm::CodePointer ) { return true; }
    bool fault_handler( vm::CodePointer ) { return true; }
    void setIfl( PointerV ) {}

    Heap &heap() { return *(_state.heap); }
    StContext( vm::explore::Context &_ctx, vm::explore::State &st )
        : _program( _ctx.program() ), _state( st )
    {}
};

void dump( vm::DebugNode< StContext > dn, std::set< vm::GenericPointer > &dumped )
{
    dumped.insert( dn._address );
    dn.attributes( []( std::string k, std::string v )
                     { std::cerr << k << std::endl << v << std::endl; } );
    dn.related( [&]( std::string k, auto rel )
                {
                    if ( dumped.count( dn._address ) )
                        return;
                    std::cerr << "---- " << k << " -------------------" << std::endl;
                    dump( rel, dumped );
                } );
}

void Verify::run()
{
    vm::Explore ex( _bc );
    int edgecount = 0, statecount = 0;

    ss::search( ss::Order::PseudoBFS, ex, 1, ss::passive_listen(
                    [&]( auto, auto, auto ) { ++edgecount; },
                    [&]( auto st )
                    {
                        StContext ctx( ex._ctx, st );
                        vm::DebugNode< StContext > dn( ctx, st.root, vm::DNKind::Object, nullptr );
                        std::set< vm::GenericPointer > _dumped;
                        std::cerr << "# new state" << std::endl;
                        dump( dn, _dumped );
                        ++statecount;
                    } ) );
    std::cerr << "found " << statecount << " states and " << edgecount << " edges" << std::endl;
}

}
}
