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

#include <divine/vm/dbg-context.hpp>
#include <brick-string>

namespace divine::vm::dbg
{

enum class DNKind { Globals, Frame, Object };
using DNKey = std::tuple< GenericPointer, int, DNKind, llvm::DIType * >;

static std::ostream &operator<<( std::ostream &o, DNKind dnk )
{
    switch ( dnk )
    {
        case DNKind::Frame: return o << "frame";
        case DNKind::Globals: return o << "frame";
        case DNKind::Object: return o << "object";
    }
}

template< typename Program, typename Heap >
struct Node
{
    using Snapshot = typename Heap::Snapshot;

    DNContext< Heap > _ctx;

    GenericPointer _address;
    int _offset;
    Snapshot _snapshot;
    DNKind _kind;

    std::map< std::string, int > _related_count;
    std::set< GenericPointer > _related_ptrs;

    using YieldAttr = std::function< void( std::string, std::string ) >;
    using YieldDN   = std::function< void( std::string, Node< Program, Heap > ) >;

    /* applies only to Objects */
    llvm::Type *_type;
    llvm::DIType *_di_type;
    llvm::DIVariable *_di_var;

    using PointerV = value::Pointer;

    void init()
    {
        _type = nullptr;
        _di_type = nullptr;
        _di_var = nullptr;
        _kind = DNKind::Object;
        _offset = 0;
    }

    void di_var( llvm::DIVariable *var )
    {
        _di_var = var;
        _di_type = var->getType().resolve( _ctx.debug().typemap() );
    }

    void di_type( llvm::DIType *type )
    {
        _di_var = nullptr;
        _di_type = type;
    }

    void type( llvm::Type *type ) { _type = type; }

    void offset( int off ) { _offset = off; }
    void address( DNKind k, GenericPointer l )
    {
        _address = l;
        _kind = k;
        if ( _kind == DNKind::Frame )
        {
            _ctx.set( _VM_CR_Frame, _address );
            _ctx.set( _VM_CR_PC, pc() );
        }
        if ( _kind == DNKind::Globals )
            _ctx.set( _VM_CR_Globals, _address );
    }

    template< typename Context >
    Node( Context ctx, Snapshot s )
        : _ctx( ctx.program(), ctx.debug(), ctx.heap() ), _snapshot( s )
    {
        _ctx.heap().restore( s );
        _ctx.set( _VM_CR_Globals, ctx.globals() );
        _ctx.set( _VM_CR_Constants, ctx.constants() );
        init();
    }

    Node( DNContext< Heap > ctx, Snapshot s )
        : _ctx( ctx ), _snapshot( s )
    {
        init();
    }

    Node( const Node &o ) = default;

    void relocate( typename Heap::Snapshot s )
    {
        _ctx.load( s );
        address( _kind, _address );
    }

    DNKind kind() { return _kind; }
    GenericPointer address() { return _address; }
    Snapshot snapshot() { return _snapshot; }

    GenericPointer pc()
    {
        ASSERT_EQ( kind(), DNKind::Frame );
        PointerV pc;
        if ( boundcheck( PointerV( _address ), PointerBytes ) )
            _ctx.heap().read( _address, pc );
        return pc.cooked();
    }

    llvm::DISubprogram *subprogram()
    {
        ASSERT_EQ( kind(), DNKind::Frame );
        return llvm::getDISubprogram( _ctx.debug().function( pc() ) );
    }

    DNKey sortkey() const
    {
        return std::make_tuple( _address, _offset, _kind,
                                _kind == DNKind::Frame ? nullptr : _di_type );
    }

    bool valid();
    bool boundcheck( PointerV ptr, int size );

    void value( YieldAttr yield );
    void attributes( YieldAttr yield );
    std::string attribute( std::string key );
    int size();
    llvm::DIDerivedType *di_derived( uint64_t tag, llvm::DIType *t = nullptr );
    llvm::DIDerivedType *di_member( llvm::DIType *t = nullptr );
    llvm::DIDerivedType *di_pointer( llvm::DIType *t = nullptr );
    llvm::DIType *di_base( llvm::DIType *t = nullptr );
    llvm::DIType *di_resolve( llvm::DIType *t = nullptr );
    std::string di_name( llvm::DIType * = nullptr, bool in_alias = false );
    std::string di_scopename( llvm::DIScope * = nullptr );
    llvm::DICompositeType *di_composite( uint64_t tag, llvm::DIType *t = nullptr );

    int width();
    int bitoffset();

    void bitcode( std::ostream &out );
    void source( std::ostream &out );
    void format( std::ostream &out, int depth = 1, int derefs = 0, int indent = 0 );

    void components( YieldDN yield );
    void related( YieldDN yield, bool anon = true );
    void struct_fields( HeapPointer hloc, YieldDN yield );
    void array_elements( YieldDN yield );
    void localvar( YieldDN yield, llvm::DbgDeclareInst *DDI );
    void localvar( YieldDN yield, llvm::DbgValueInst *DDV );
    void framevars( YieldDN yield );
    void globalvars( YieldDN yield );

    void dump( std::ostream &o );
    void dot( std::ostream &o );
};

}
