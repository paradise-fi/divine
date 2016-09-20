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

DIVINE_RELAX_WARNINGS
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/IntrinsicInst.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/vm/context.hpp>
#include <brick-string>

namespace divine {
namespace vm {

enum class DNKind { Frame, Object };

static std::ostream &operator<<( std::ostream &o, DNKind dnk )
{
    switch ( dnk )
    {
        case DNKind::Frame: return o << "frame";
        case DNKind::Object: return o << "object";
    }
}

std::pair< std::string, int > fileline( const llvm::Instruction &insn );
std::string location( const llvm::Instruction &insn );

template< typename Program, typename Heap >
struct DebugNode
{
    using Snapshot = typename Heap::Snapshot;

    ConstContext< Program, Heap > _ctx;

    GenericPointer _address;
    int _offset;
    Snapshot _snapshot;
    DNKind _kind;

    using YieldAttr = std::function< void( std::string, std::string ) >;
    using YieldDN   = std::function< void( std::string, DebugNode< Program, Heap > ) >;

    /* applies only to Objects */
    llvm::Type *_type;
    llvm::DIType *_di_type;

    using PointerV = value::Pointer;

    void init()
    {
        if ( _kind == DNKind::Frame )
            _ctx.set( _VM_CR_Frame, _address );
    }

    template< typename Context >
    DebugNode( Context ctx, Snapshot s, GenericPointer l, int off,
               DNKind k, llvm::Type *t, llvm::DIType *dit )
        : _ctx( ctx.program(), ctx.heap() ), _address( l ), _offset( off ),
          _snapshot( s ), _kind( k ), _type( t ), _di_type( dit )
    {
        _ctx.heap().restore( s );
        _ctx.set( _VM_CR_Globals, ctx.globals() );
        _ctx.set( _VM_CR_Constants, ctx.constants() );
        init();
    }

    DebugNode( ConstContext< Program, Heap > ctx, Snapshot s,
               GenericPointer l, int off, DNKind k, llvm::Type *t, llvm::DIType *dit )
        : _ctx( ctx ), _address( l ), _offset( off ),
          _snapshot( s ), _kind( k ), _type( t ), _di_type( dit )
    {
        init();
    }

    DebugNode( const DebugNode &o ) = default;

    void relocate( typename Heap::Snapshot s )
    {
        _ctx.heap().restore( s );
    }

    DNKind kind() { return _kind; }
    GenericPointer address() { return _address; }
    Snapshot snapshot() { return _snapshot; }

    GenericPointer pc()
    {
        ASSERT_EQ( kind(), DNKind::Frame );
        PointerV pc;
        _ctx.heap().read( _address, pc );
        return pc.cooked();
    }

    llvm::Function *llvmfunction()
    {
        ASSERT_EQ( kind(), DNKind::Frame );
        return _ctx.program().llvmfunction( pc() );
    }

    llvm::DISubprogram *subprogram()
    {
        return llvm::getDISubprogram( llvmfunction() );
    }

    auto sortkey() { return std::make_tuple( _address, _offset,
                                             _kind == DNKind::Frame ? nullptr : _di_type ); }

    bool valid();
    void value( YieldAttr yield );
    void attributes( YieldAttr yield );
    std::string attribute( std::string key );
    int size();
    llvm::DIDerivedType *di_derived( uint64_t tag, llvm::DIType *t = nullptr );
    llvm::DIDerivedType *di_member( llvm::DIType *t = nullptr );
    llvm::DIDerivedType *di_pointer( llvm::DIType *t = nullptr );
    llvm::DIType *di_base( llvm::DIType *t = nullptr );
    llvm::DICompositeType *di_composite();

    int width();
    int bitoffset();

    void bitcode( std::ostream &out );
    void source( std::ostream &out );
    void format( std::ostream &out, int depth = 1, bool compact = false, int indent = 0 );

    void related( YieldDN yield );
    void struct_fields( HeapPointer hloc, YieldDN yield, std::set< GenericPointer > &ptrs );
    void localvar( YieldDN yield, llvm::DbgDeclareInst *DDI,
                   std::set< GenericPointer > &ptrs );
    void framevars( YieldDN yield, std::set< GenericPointer > &ptrs );

    void dump( std::ostream &o );
    void dot( std::ostream &o );
};

using ProcInfo = std::vector< std::pair< std::pair< int, int >, int > >;

template< typename Program, typename Heap >
struct DebugContext : Context< Program, Heap >
{
    std::vector< std::string > _trace;
    std::deque< int > _choices;
    ProcInfo _proc;

    llvm::DIType *_state_di_type;
    llvm::Type   *_state_type;

    DebugContext( Program &p )
        : Context< Program, Heap >( p ),
          _state_di_type( nullptr ), _state_type( nullptr )
    {}

    template< typename I >
    int choose( int count, I, I )
    {
        if ( _choices.empty() )
            _choices.emplace_back( 0 );

        ASSERT_LT( _choices.front(), count );
        ASSERT_LEQ( 0, _choices.front() );
        if ( !_proc.empty() )
            _proc.clear();
        auto rv = _choices.front();
        _choices.pop_front();
        return rv;
    }

    void trace( vm::TraceText tt )
    {
        _trace.push_back( this->heap().read_string( tt.text ) );
    }

    void trace( vm::TraceSchedInfo ) { NOT_IMPLEMENTED(); }

    void trace( vm::TraceSchedChoice tsc )
    {
        auto ptr = tsc.list;
        int size = this->heap().size( ptr.cooked() );
        if ( size % 12 )
            return; /* invalid */
        for ( int i = 0; i < size / 12; ++i )
        {
            vm::value::Int< 32, true > pid, tid, choice;
            this->heap().read_shift( ptr, pid );
            this->heap().read_shift( ptr, tid );
            this->heap().read_shift( ptr, choice );
            _proc.emplace_back( std::make_pair( pid.cooked(), tid.cooked() ), choice.cooked() );
        }
    }

    void state_type( llvm::DIVariable *di )
    {
        auto ptrtype = llvm::cast< llvm::DIDerivedType >(
            di->getType().resolve( this->program().ditypemap ) );
        _state_di_type = ptrtype->getBaseType().resolve( this->program().ditypemap );
    }

    void trace( vm::TraceStateType s )
    {
        _state_type = s.stateptr->getType()->getPointerElementType();
        for ( auto &BB : *llvm::cast< llvm::Instruction >( s.stateptr )->getParent()->getParent() )
            for ( auto &I : BB )
            {
                if ( auto DVI = llvm::dyn_cast< llvm::DbgValueInst >( &I ) )
                    if ( DVI->getValue() == s.stateptr )
                        state_type( DVI->getVariable() );
                if ( auto DDI = llvm::dyn_cast< llvm::DbgDeclareInst >( &I ) )
                    if ( DDI->getAddress() == s.stateptr )
                        state_type( DDI->getVariable() );
            }
    }
};

}
}
