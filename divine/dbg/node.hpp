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

#include <divine/dbg/context.hpp>
#include <divine/dbg/print.hpp>
#include <brick-string>

namespace divine::dbg
{

enum class DNKind { Globals, Frame, Object };
using DNKey = std::tuple< vm::GenericPointer, int, DNKind, llvm::DIType * >;

template< typename stream >
static auto operator<<( stream &o, DNKind dnk ) -> decltype( o << "" )
{
    switch ( dnk )
    {
        case DNKind::Frame: return o << "frame";
        case DNKind::Globals: return o << "frame";
        case DNKind::Object: return o << "object";
    }
    UNREACHABLE( "impossible debug node kind" );
}

template< typename Program, typename Heap >
struct Node
{
    using Snapshot = typename Heap::Snapshot;
    using Context = DNContext< Heap >;

    Context _ctx;

    vm::GenericPointer _address;
    int _offset, _bound;
    Snapshot _snapshot;
    DNKind _kind;
    bool _executing;
    brick::mem::RefCnt< typename Context::RefPool::Master > _ref;

    std::map< std::string, int > _related_count;
    std::set< vm::GenericPointer > _related_ptrs;

    using YieldAttr = std::function< void( std::string, std::string ) >;
    using YieldDN   = std::function< void( std::string, Node< Program, Heap > ) >;

    /* applies only to Objects */
    llvm::Type *_type;
    llvm::DIType *_di_type;
    llvm::DIVariable *_di_var;
    llvm::Value *_var_loc;

    using PointerV = vm::value::Pointer;

    void di_var( llvm::DIVariable *var )
    {
        _di_var = var;
        _di_type = var->getType().resolve();
    }

    void di_type( llvm::DIType *type )
    {
        _di_var = nullptr;
        _di_type = type;
    }

    void type( llvm::Type *type ) { _type = type; }

    void bounds( int off, int bound = 0 ) { _offset = off; _bound = bound; }
    void address( DNKind k, vm::GenericPointer l, bool exec = false )
    {
        _address = l;
        _kind = k;
        if ( _kind == DNKind::Frame && _address.heap() )
        {
            _ctx.set( _VM_CR_Frame, _address );
            _ctx.set( _VM_CR_PC, pc() );
            _executing = exec;
        }
        if ( _kind == DNKind::Globals && _address.heap() )
            _ctx.set( _VM_CR_Globals, _address );
    }

    template< typename E >
    auto print( E &eval ) { return printer( _ctx.debug(), eval ); }

    Node( const DNContext< Heap > &ctx, Snapshot s )
        : _ctx( ctx ), _snapshot( s ), _ref( _ctx._refcnt, s )
    {
        _type = nullptr;
        _di_type = nullptr;
        _di_var = nullptr;
        _var_loc = nullptr;
        _kind = DNKind::Object;
        _offset = 0;
        _bound = 0;

        _ctx.debug().initPretty( [&]( llvm::DIType *di ) {
            static const int offset = strlen( "typeinfo name for " );
            if ( auto dic = llvm::dyn_cast< llvm::DICompositeType >( di ) )
                return print::demangle( dic->getIdentifier().str() ).substr( offset );
            return di_name( di, false, false );
        } );
    }

    Node( const Node &o ) = default;

    void relocate( typename Heap::Snapshot s )
    {
        _ctx.load( s );
        _ref = decltype( _ref )( _ctx._refcnt, s );
        address( _kind, _address, _executing );
    }

    DNKind kind() { return _kind; }
    vm::GenericPointer address() { return _address; }
    vm::HeapPointer heap_address();
    Snapshot snapshot() { return _snapshot; }

    vm::GenericPointer pc();
    vm::CodePointer active_pc()
    {
        if ( _executing )
            return _ctx.program().advance( pc() );
        else
            return _ctx.program().nextpc( pc() );
    }

    llvm::DISubprogram *subprogram()
    {
        ASSERT_EQ( kind(), DNKind::Frame );
        return _ctx.debug().function( pc() )->getSubprogram();
    }

    DNKey sortkey() const
    {
        return std::make_tuple( _address, _offset, _kind,
                                _kind == DNKind::Frame ? nullptr : _di_type );
    }

    bool valid();
    bool boundcheck( PointerV ptr, int size );
    bool boundcheck( int off, int size );

    void value( YieldAttr yield );
    std::string formula( bool peek, int offset = 0 );
    void attributes( YieldAttr yield );
    std::string attribute( std::string key );
    int size();
    llvm::DIDerivedType *di_derived( uint64_t tag, llvm::DIType *t = nullptr );
    llvm::DIDerivedType *di_member( llvm::DIType *t = nullptr );
    llvm::DIDerivedType *di_pointer( llvm::DIType *t = nullptr );
    llvm::DIType *di_base( llvm::DIType *t = nullptr );
    llvm::DIType *di_resolve( llvm::DIType *t = nullptr );
    std::string di_name( llvm::DIType * = nullptr, bool in_alias = false, bool prettify = true );
    std::string di_scopename( llvm::DIScope * = nullptr );
    llvm::DICompositeType *di_composite( uint64_t tag, llvm::DIType *t = nullptr );

    int width();
    int bitoffset();

    void bitcode( std::ostream &out );
    void source( std::ostream &out, std::function< std::string( std::string ) > pp );
    void format( brq::string_builder &out, int depth = 1, int derefs = 0, int indent = 0 );

    void components( YieldDN yield );
    void related( YieldDN yield, bool anon = true );
    Node related( std::string name );
    void struct_fields( vm::HeapPointer hloc, YieldDN yield );
    void array_elements( YieldDN yield );
    void localvar( YieldDN yield, llvm::DbgDeclareInst *DDI );
    void localvar( YieldDN yield, llvm::DbgValueInst *DDV );
    void localvar( YieldDN yield, llvm::Argument *arg );
    void framevars( YieldDN yield );
    void globalvars( YieldDN yield );

    void dump( std::ostream &o );
    void dot( std::ostream &o );
};

template< typename Program, typename Heap >
void diff( std::ostream &out, Node< Program, Heap > a, Node< Program, Heap > b );

}
