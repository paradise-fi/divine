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

#include <divine/dbg/info.hpp>
#include <divine/dbg/node.hpp>
#include <divine/dbg/print.hpp>
#include <divine/vm/xg-code.hpp>
#include <divine/vm/eval.hpp>
#include <divine/vm/eval.tpp>
#include <divine/smt/extract.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/CFG.h>
DIVINE_UNRELAX_WARNINGS
#include <brick-llvm>
#include <queue>

namespace divine::dbg
{

namespace xg = vm::xg;
namespace lx = vm::lx;
using namespace std::literals;

template< typename Heap >
using DNEval = vm::Eval< DNContext< Heap > >;

template< typename Prog, typename Heap >
vm::GenericPointer Node< Prog, Heap >::pc()
{
    ASSERT_EQ( kind(), DNKind::Frame );
    ASSERT_EQ( _offset, 0 );
    PointerV pc;
    if ( boundcheck( 0, vm::PointerBytes ) )
        _ctx.heap().read( _address, pc );
    return pc.cooked();
}

template< typename Prog, typename Heap >
int Node< Prog, Heap >::size()
{
    int sz = INT_MAX;
    DNEval< Heap > eval( _ctx );
    if ( _type && _type->isSized() )
        sz = _ctx.program().TD.getTypeAllocSize( _type );
    if ( !_address.null() )
        sz = std::min( sz, eval.ptr2sz( PointerV( _address ) ) - _offset );
    return sz;
}

template< typename Prog, typename Heap >
llvm::DIDerivedType *Node< Prog, Heap >::di_derived( uint64_t tag, llvm::DIType *t )
{
    t = t ?: _di_type;
    if ( !t )
        return nullptr;
    auto derived = llvm::dyn_cast< llvm::DIDerivedType >( t );
    if ( derived && derived->getTag() == tag )
        return derived;
    return nullptr;
}

template< typename Prog, typename Heap >
llvm::DIDerivedType *Node< Prog, Heap >::di_member( llvm::DIType *t )
{
    return di_derived( llvm::dwarf::DW_TAG_member, t );
}

template< typename Prog, typename Heap >
llvm::DIDerivedType *Node< Prog, Heap >::di_pointer( llvm::DIType *t )
{
    return di_derived( llvm::dwarf::DW_TAG_pointer_type, t );
}

template< typename Prog, typename Heap >
llvm::DIType *Node< Prog, Heap >::di_base( llvm::DIType *t )
{
    t = t ?: di_resolve();
    if ( !t )
        return nullptr;
    if ( auto deriv = llvm::dyn_cast< llvm::DIDerivedType >( t ) )
        return deriv->getBaseType().resolve();
    if ( auto comp = llvm::dyn_cast< llvm::DICompositeType >( t ) )
        return comp->getBaseType().resolve();
    return nullptr;
}

template< typename Prog, typename Heap >
llvm::DICompositeType *Node< Prog, Heap >::di_composite( uint64_t tag, llvm::DIType *t )
{
    t = t ?: di_resolve();
    if ( t && t->getTag() == tag )
        return llvm::dyn_cast< llvm::DICompositeType >( t );
    return nullptr;
}

template< typename Prog, typename Heap >
int Node< Prog, Heap >::width()
{
    if ( di_member() )
        return di_member()->getSizeInBits();
    return 8 * size();
}

template< typename Prog, typename Heap >
int Node< Prog, Heap >::bitoffset()
{
    int rv = 0;
    if ( di_member() )
        rv = di_member()->getOffsetInBits() - 8 * _offset;
    ASSERT_LEQ( rv, 8 * size() );
    ASSERT_LEQ( rv + width(), 8 * size() );
    return rv;
}

template< typename Prog, typename Heap >
bool Node< Prog, Heap >::boundcheck( PointerV ptr, int size )
{
    DNEval< Heap > eval( _ctx );
    return eval.boundcheck( []( auto ) { return std::stringstream(); }, ptr, size, false );
}

template< typename Prog, typename Heap >
bool Node< Prog, Heap >::boundcheck( int off, int size )
{
    if ( _bound && off + size >= _bound )
        return false;
    return boundcheck( PointerV( _address ) + _offset + off, size );
}

template< typename Prog, typename Heap >
bool Node< Prog, Heap >::valid()
{
    if ( _address.null() )
        return false;
    if ( _address.type() == vm::PointerType::Heap && !_ctx.heap().valid( _address ) )
        return false;

    DNEval< Heap > eval( _ctx );
    PointerV addr( _address );
    if ( !boundcheck( addr, 1 ) )
        return false;
    if ( !boundcheck( addr, size() ) )
        return false;

    if ( _kind == DNKind::Frame )
        if ( pc().type() != vm::PointerType::Code || !_ctx.program().valid( pc() ) )
            return false;
    return true;
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::value( YieldAttr yield )
{
    if ( !_type )
        return;

    DNEval< Heap > eval( _ctx );
    PointerV loc( _address + _offset );

    if ( _type->isIntegerTy() )
        eval.template type_dispatch< vm::IsIntegral >(
            xg::type( _type ),
            [&]( auto v )
            {
                auto raw = v.get( loc );
                using V = decltype( raw );
                if ( bitoffset() || width() != size() * 8 )
                {
                    yield( "raw_value", brick::string::fmt( raw ) );
                    auto val = raw >> vm::value::Int< 32 >( bitoffset() );
                    val = val & V( brick::bitlevel::ones< typename V::Raw >( width() ) );
                    ASSERT_LEQ( bitoffset() + width(), size() * 8 );
                    yield( "value", brick::string::fmt( val ) );
                }
                else
                    yield( "value", brick::string::fmt( raw ) );
            } );

    if ( _type->isFloatingPointTy() )
        eval.template type_dispatch< vm::IsFloat >(
            xg::type( _type ),
            [&]( auto v )
            {
                yield( "value", brick::string::fmt( v.get( loc ) ) );
            } );

    if ( _di_type && ( di_name() == "char*" ||  di_name() == "const char*" ) )
    {
        PointerV str_v;
        auto hloc = heap_address() + _offset;
        _ctx.heap().read( hloc, str_v );
        auto str = eval.ptr2h( str_v );
        if ( _ctx.heap().valid( str ) )
                yield( "string", "\"" + _ctx.heap().read_string( str ) + "\"" ); /* TODO escape */
    }

    if ( _type->isPointerTy() )
        eval.template type_dispatch< vm::Any >(
            xg::type( _type ),
            [&]( auto v ) { yield( "value", brick::string::fmt( v.get( loc ) ) ); } );
}

template< typename Prog, typename Heap >
std::string Node< Prog, Heap >::di_scopename( llvm::DIScope *scope )
{
    std::string n;
    if ( !scope )
        scope = _di_var->getScope();

    auto parent = scope->getScope().resolve();

    if ( parent && llvm::isa< llvm::DINamespace >( parent ) )
        n = di_scopename( parent ) + "::";

    if ( auto ns = llvm::dyn_cast< llvm::DINamespace >( scope ) )
        n += ns->getName() == "" ? "<anon>" : ns->getName();
    else if ( auto file = llvm::dyn_cast< llvm::DICompileUnit >( scope ) )
        n += "<static in " + file->getFilename().str() + ">";
    else
        n += scope->getName();

    return n;
}

template< typename Prog, typename Heap >
std::string Node< Prog, Heap >::di_name( llvm::DIType *t, bool in_alias, bool prettify )
{
    if ( !t )
        t = _di_type;
    if ( di_member( t ) )
        return di_name( di_base( t ) );

    if ( auto subr = llvm::dyn_cast< llvm::DISubroutineType >( t ) )
    {
        auto types = subr->getTypeArray();
        std::stringstream fmt;
        if ( auto rvt = types[0].resolve() )
            fmt << di_name( rvt ) << "(";
        else
            fmt << "void" << "(";
        for ( unsigned i = 1; i < types.size(); ++i )
        {
            auto subt = types[i].resolve(); // null means ellipsis here
            fmt << ( subt ? di_name( subt ) : "..." ) << ( i + 1 < types.size() ? ", " : "" );
        }
        fmt << ")";
        return fmt.str();
    }

    const auto& fnMap = _ctx.debug()._typenamemap;
    std::string name;
    if ( prettify && fnMap.find( t ) != fnMap.end() )
        name = fnMap.find( t )->second;
    else
        name = t->getName().str();

    if ( di_pointer( t ) && !di_base( t ) ) /* special case */
        return "void *";

    if ( !di_base( t ) || di_composite( llvm::dwarf::DW_TAG_enumeration_type ) )
        return name.empty() ? "<anon>" : name;

    if ( di_derived( llvm::dwarf::DW_TAG_typedef, t ) )
    {
        std::string def = di_name( di_base( t ), true );
        if ( !def.empty() )
            name += " = " + def;
        if ( in_alias )
            return name;
        else
            return "(" + name + ")";
    }

    if ( di_derived( llvm::dwarf::DW_TAG_inheritance, t ) )
        return "<" + di_name( di_base( t ) ) + ">";
    if ( di_derived( llvm::dwarf::DW_TAG_ptr_to_member_type, t ) )
        return di_name( di_base( t ) ) + "::*";
    if ( di_pointer( t ) )
        return di_name( di_base( t ) ) + "*";
    if ( di_derived( llvm::dwarf::DW_TAG_reference_type, t ) )
        return di_name( di_base( t ) ) + "&";
    if ( di_derived( llvm::dwarf::DW_TAG_rvalue_reference_type, t ) )
        return di_name( di_base( t ) ) + "&&";
    if ( di_derived( llvm::dwarf::DW_TAG_const_type, t ) )
        return "const " + di_name( di_base( t ) );
    if ( di_derived( llvm::dwarf::DW_TAG_restrict_type, t ) )
        return di_name( di_base( t ) ) + " restrict";
    if ( di_derived( llvm::dwarf::DW_TAG_volatile_type, t ) )
        return "volatile " + di_name( di_base( t ) );
    if ( di_composite( llvm::dwarf::DW_TAG_array_type, t ) )
        return di_name( di_base( t ) ) + "[]";
    UNREACHABLE( "unexpected debuginfo metadata:", t );
}

template< typename Prog, typename Heap >
vm::HeapPointer Node< Prog, Heap >::heap_address()
{
    DNEval< Heap > eval( _ctx );
    return eval.ptr2h( PointerV( _address ) );
}

template< typename Prog, typename Heap >
std::string Node< Prog, Heap >::formula( bool peek, int offset )
{
    brick::smt::Context smt_ctx;
    smt::extract::SMTLib2 smt_value( _ctx.heap(), smt_ctx );
    vm::GenericPointer addr;
    if ( peek )
    {
        auto p = _ctx.heap().peek( heap_address() + _offset + offset, 0 );
        if ( p.defined() )
            addr.object( p.cooked() );
    }
    else
        addr = _address;
    if ( !addr.object() )
        return "";
    auto n = smt_value.convert( addr );
    auto str = smt_ctx.print( n ) + " ";
    if ( offset )
        return "[+" + std::to_string( offset ) + "] " + str;
    else
        return str;
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::attributes( YieldAttr yield )
{
    DNEval< Heap > eval( _ctx );
    Prog &program = _ctx.program();
    Info &dbgInfo = _ctx.debug();

    yield( "address", brick::string::fmt( _address ) + "+" +
           brick::string::fmt( _offset ) );

    if ( _di_type )
        yield( "type", dbgInfo.makePretty( di_name() ) );

    if ( !valid() )
        return;

    auto hloc = heap_address();
    yield( "size", brick::string::fmt( size() ) );
    value( yield );

    yield( "raw", print::raw( _ctx.heap(), hloc + _offset, size() ) );

    if ( _address.type() == vm::PointerType::Global )
        yield( "slot", brick::string::fmt( eval.ptr2s( _address ) ) );


    std::stringstream formulas;

    if ( _address.type() == vm::PointerType::Marked )
        formulas << "[ptr] " << formula( false ) << " ";

    for ( int i = 0; i < size(); ++i )
    {
        vm::CharV tainted;
        _ctx.heap().read( hloc + _offset, tainted );
        if ( tainted.taints() )
            formulas << formula( true, i );
    }

    if ( !formulas.str().empty() )
        yield( "formula", formulas.str() );

    if ( _di_var )
    {
        yield( "scope", di_scopename() );
        yield( "definition", _di_var->getFilename().str() + ":" +
                              std::to_string( _di_var->getLine() ) );
    }

    if ( _kind == DNKind::Frame )
    {
        yield( "pc", brick::string::fmt( pc() ) );
        if ( pc().null() || pc().type() != vm::PointerType::Code )
            return;
        auto *insn = &program.instruction( pc() );
        ASSERT_EQ( eval.pc(), vm::CodePointer( pc() ) );
        if ( insn->opcode != lx::OpBB && insn->opcode != lx::OpArg )
        {
            eval._instruction = insn;
            yield( "insn", print( eval ).instruction( 0, 1000 ) );
        }

        auto op = _ctx.debug().find( nullptr, active_pc() ).first;
        yield( "location", location( *op ) );

        auto sym = op->getParent()->getParent()->getName().str();
        yield( "symbol", dbgInfo.makePretty( print::demangle( sym ) ) );
    }
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::bitcode( std::ostream &out )
{
    if ( _kind != DNKind::Frame )
        throw brick::except::Error( "cannot display bitcode, not a stack frame" );
    DNEval< Heap > eval( _ctx );
    vm::CodePointer iter = pc(), origpc = pc();
    auto &prog = _ctx.program();
    for ( iter.instruction( 0 ); prog.valid( iter ); iter = iter + 1 )
    {
        auto &i = prog.instruction( iter );
        eval._instruction = &i;
        _ctx.set( _VM_CR_PC, iter );
        if ( i.opcode == lx::OpArg || !i.opcode )
            continue;

        out << ( iter == active_pc() ? ">>" : "  " );
        if ( i.opcode == lx::OpBB )
        {
            auto iop = _ctx.debug().find( nullptr, iter + 1 ).first;
            out << print( eval ).value( iop->getParent() ) << ":" << std::endl;
        }
        else
            out << "  " << print( eval ).instruction( 4 ) << std::endl;
    }
    _ctx.set( _VM_CR_PC, origpc );
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::source( std::ostream &out, std::function< std::string( std::string ) > pp )
{
    if ( _kind != DNKind::Frame )
        throw brick::except::Error( "cannot display source code, not a stack frame" );
    if ( !valid() )
        throw brick::except::Error( "cannot display source code for a null/invalid frame" );
    auto s = subprogram();
    if ( !s )
        throw brick::except::Error( "cannot display source code, no debugging information found" );
    out << print::source( _ctx.debug(), s, _ctx.program(), active_pc(), pp );
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::components( YieldDN yield )
{
    if ( !valid() )
        return;

    if ( _kind == DNKind::Frame )
        framevars( yield );

    if ( _kind == DNKind::Globals )
        globalvars( yield );

    DNEval< Heap > eval( _ctx );

    if ( _type && _di_type &&
         ( di_composite( llvm::dwarf::DW_TAG_structure_type ) ||
           di_composite( llvm::dwarf::DW_TAG_class_type ) ) &&
         _type->isStructTy() )
        struct_fields( heap_address(), yield );

    if ( _type && _di_type && di_composite( llvm::dwarf::DW_TAG_array_type ) )
        array_elements( yield );
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::related( YieldDN yield, bool anon )
{
    if ( !valid() )
        return;

    DNEval< Heap > eval( _ctx );

    PointerV ptr;
    auto hloc = heap_address();
    int hoff = hloc.offset();

    if ( _type && _di_type && _type->isPointerTy() &&
         boundcheck( PointerV( hloc + _offset ), vm::PointerBytes ) )
    {
        PointerV addr;
        _ctx.heap().read( hloc + _offset, addr );
        _related_ptrs.insert( addr.cooked() );
        auto kind = DNKind::Object;
        if ( di_name() == "_VM_Frame*" )
            kind = DNKind::Frame;
        Node rel( _ctx, _snapshot );
        rel.address( kind, addr.cooked() );
        rel.type( _type->getPointerElementType() );
        rel.di_type( di_base() );
        yield( "deref", rel );
    }

    if ( _kind == DNKind::Frame && boundcheck( 0, 2 * vm::PointerBytes ) )
    {
        PointerV fr( _address );
        _ctx.heap().skip( fr, vm::PointerBytes );
        _ctx.heap().read( fr.cooked(), fr );
        if ( !fr.cooked().null() )
        {
            _related_ptrs.insert( fr.cooked() );
            Node caller( _ctx, _snapshot );
            caller.address( DNKind::Frame, fr.cooked() );
            yield( "caller", caller );
        }
    }

    if ( !anon )
        return;

    for ( auto ptroff : _ctx.heap().pointers( hloc, _offset, size() ) )
    {
        hloc.offset( hoff + _offset + ptroff->offset() );
        if ( ptroff->offset() + ptroff->size() > size() )
            continue;
        _ctx.heap().read( hloc, ptr );
        auto pp = ptr.cooked();
        if ( pp.type() == vm::PointerType::Code || pp.null() )
            continue;
        if ( _related_ptrs.find( pp ) != _related_ptrs.end() )
            continue;
        pp.offset( 0 );
        Node deref( _ctx, _snapshot );
        deref.address( DNKind::Object, pp );
        yield( brick::string::fmt( ptroff->offset() ), deref );
    }
}

template< typename Prog, typename Heap >
llvm::DIType *Node< Prog, Heap >::di_resolve( llvm::DIType *t )
{
    llvm::DIType *base = t ?: _di_type;
    llvm::DIDerivedType *DT = nullptr;

    while ( base )
        if ( ( DT = llvm::dyn_cast< llvm::DIDerivedType >( base ) ) &&
             ( DT->getTag() == llvm::dwarf::DW_TAG_typedef ||
               DT->getTag() == llvm::dwarf::DW_TAG_member ||
               DT->getTag() == llvm::dwarf::DW_TAG_restrict_type ||
               DT->getTag() == llvm::dwarf::DW_TAG_volatile_type ||
               DT->getTag() == llvm::dwarf::DW_TAG_const_type ) )
            base = DT->getBaseType().resolve();
        else return base;
    return t;
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::struct_fields( vm::HeapPointer hloc, YieldDN yield )
{
    auto CT = llvm::cast< llvm::DICompositeType >( di_resolve() );
    auto ST = llvm::cast< llvm::StructType >( _type );
    if ( ST->isOpaque() )
        return;
    auto STE = ST->element_begin();
    auto SLO = _ctx.program().TD.getStructLayout( ST );
    int idx = 0, anon = 0, base = 0;
    for ( auto subtype : CT->getElements() )
        if ( auto CTE = llvm::dyn_cast< llvm::DIDerivedType >( subtype ) )
        {
            if ( idx + 1 < int( ST->getNumElements() ) &&
                 CTE->getOffsetInBits() >= 8 * SLO->getElementOffset( idx + 1 ) )
                idx ++, STE ++;

            int offset = SLO->getElementOffset( idx );
            if ( (*STE)->isPointerTy() && boundcheck( PointerV( hloc + offset ), vm::PointerBytes ) )
            {
                PointerV ptr;
                _ctx.heap().read( hloc + offset, ptr );
                _related_ptrs.insert( ptr.cooked() );
            }

            Node field( _ctx, _snapshot );
            field.address( DNKind::Object, _address );
            field.bounds( _offset + offset ); /* TODO bound? */
            field.type( *STE );
            field.di_type( CTE );
            std::string name = CTE->getName().str();
            if ( name.empty() && CTE->getTag() == llvm::dwarf::DW_TAG_inheritance )
            {
                name = "<base"s + (base++ ? "$" + std::to_string( base ) : "") + ">";
                field.di_type( di_base( CTE ) );
            }
            if ( name.empty() )
                name = "<anon"s + (anon++ ? "$" + std::to_string( base ) : "") + ">";
            yield( name, field );
        }
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::array_elements( YieldDN yield )
{
    // variable-length arrays are allocated as pointers to the element type,
    // not as sequential types
    auto type = _type;
    if ( llvm::isa< llvm::SequentialType >( type ) )
        type = _type->getSequentialElementType();
    int size = _ctx.program().TD.getTypeAllocSize( type );

    for ( int idx = 0; boundcheck( idx * size, size ); ++ idx )
    {
        Node elem( _ctx, _snapshot );
        elem.address( DNKind::Object, _address );
        elem.bounds( _offset + idx * size, size );
        elem.type( type );
        elem.di_type( di_base() );
        yield( "[" + std::to_string( idx ) + "]", elem );
    }
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::localvar( YieldDN yield, llvm::DbgDeclareInst *DDI )
{
    DNEval< Heap > eval( _ctx );

    auto divar = DDI->getVariable();
    auto ditype = divar->getType().resolve();
    auto var = DDI->getAddress();
    auto &vmap = _ctx.program().valuemap;
    if ( vmap.find( var ) == vmap.end() )
        return;

    PointerV ptr;
    _ctx.heap().read( eval.s2ptr( _ctx.program().valuemap[ var ] ), ptr );
    _related_ptrs.insert( ptr.cooked() );

    auto type = var->getType()->getPointerElementType();
    auto name = divar->getName().str();

    if ( _related_count[ name ] ++ )
        return;

    Node lvar( _ctx, _snapshot );
    lvar.address( DNKind::Object, ptr.cooked() );
    lvar.type( type );
    lvar.di_type( ditype );
    yield( name, lvar );
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::localvar( YieldDN yield, llvm::DbgValueInst *DDV )
{
    DNEval< Heap > eval( _ctx );

    auto divar = DDV->getVariable();
    auto var = DDV->getValue();
    auto &vmap = _ctx.program().valuemap;
    if ( vmap.find( var ) == vmap.end() )
        return;

    auto slot = _ctx.program().valuemap[ var ];
    int bound = 0;

    vm::GenericPointer ptr;
    if ( _ctx.program()._addr.has_addr( var ) && slot.location != Prog::Slot::Local )
        ptr = _ctx.program().addr( var );
    else if ( slot.location != Prog::Slot::Invalid )
        ptr = eval.s2ptr( slot ), bound = slot.size();

    PointerV deref;

    if ( slot.size() >= vm::PointerBytes && boundcheck( PointerV( ptr ), vm::PointerBytes ) )
        _ctx.heap().read( eval.ptr2h( PointerV( ptr ) ), deref );
    if ( deref.pointer() )
        _related_ptrs.insert( deref.cooked() );

    auto type = var->getType();
    auto name = divar->getName().str();

    if ( _related_count[ name ] ++ )
        return;

    Node lvar( _ctx, _snapshot );
    lvar.address( DNKind::Object, ptr );
    lvar.bounds( 0, bound );
    lvar.type( type );
    lvar.di_var( divar );
    yield( name, lvar );
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::localvar( YieldDN yield, llvm::Argument *arg )
{
    DNEval< Heap > eval( _ctx );

    auto slot = _ctx.program().valuemap[ arg ];
    auto ptr = eval.s2ptr( slot );
    auto name = arg->getName().str();

    if ( _related_count[ name ] ++ )
        return;

    Node lvar( _ctx, _snapshot );
    lvar.address( DNKind::Object, ptr );
    lvar.type( arg->getType() );
    yield( name, lvar );
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::framevars( YieldDN yield )
{
    if ( pc().type() != vm::PointerType::Code )
        return;

    std::queue< llvm::BasicBlock * > q;
    std::set< llvm::BasicBlock * > seen;

    auto npc = _ctx.program().nextpc( pc() );
    auto op = _ctx.debug().find( nullptr, npc ).first;
    auto start = op->getParent();
    q.push( start );

    while ( !q.empty() )
    {
        auto BB = q.front();
        q.pop();

        bool skip = BB == op->getParent();

        for ( auto iter = BB->rbegin(); iter != BB->rend(); ++ iter )
        {
            if ( skip && &*iter == op ) skip = false;
            if ( skip ) continue;
            if ( auto DDI = llvm::dyn_cast< llvm::DbgDeclareInst >( &*iter ) )
                localvar( yield, DDI );
            if ( auto DDV = llvm::dyn_cast< llvm::DbgValueInst >( &*iter ) )
                localvar( yield, DDV );
        }

        for ( auto P : llvm::predecessors( BB ) )
            if ( !seen.count( BB ) )
                q.push( P ), seen.insert( BB );
    }

    for ( auto &a : start->getParent()->args() )
        localvar( yield, &a );
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::globalvars( YieldDN /*yield*/ )
{
    NOT_IMPLEMENTED();
#if 0 /* needs rewrite for 4.0 */
    DNEval< Heap > eval( _ctx );
    llvm::DebugInfoFinder finder;
    finder.processModule( *_ctx.program().module );
    std::map< std::string, int > disamb;

    for ( auto GV : finder.global_variables() )
    {
        auto var = GV->getVariable();
        if ( !var || !llvm::isa< llvm::GlobalVariable >( var ) )
            continue;
        auto ptr = _ctx.program().addr( var );
        if ( ptr.null() )
            continue;

        PointerV deref;
        if ( boundcheck( PointerV( ptr ), vm::PointerBytes ) )
            _ctx.heap().read( eval.ptr2h( PointerV( ptr ) ), deref );
        if ( deref.pointer() )
            _related_ptrs.insert( deref.cooked() );

        Node dn( _ctx, _snapshot );
        dn.address( DNKind::Object, ptr );
        dn.di_var( GV );
        dn.type( var->getType()->getPointerElementType() );
        std::string name = GV->getName();
        if ( llvm::isa< llvm::DINamespace >( GV->getScope() ) )
            name = dn.di_scopename() + "::" + name;
        if ( disamb[ name ] )
            name += ( "$" + std::to_string( disamb[ name ] ++ ) );
        else
            disamb[ name ] = 1;
        yield( name, dn );
    }
#endif
}

static std::string rightpad( std::string s, int i )
{
    while ( int( s.size() ) < i )
        s += ' ';
    return s;
}

template< typename Prog, typename Heap >
void Node< Prog, Heap >::format( std::ostream &out, int depth, int derefs, int indent )
{
    std::string ind_attr( indent + 4, ' ' ), ind( indent, ' ' );
    std::set< std::string > ck{ "value", "type", "location", "symbol", "scope", "formula" };

    if ( !indent )
        out << ind << "attributes:" << std::endl;

    attributes(
        [&]( std::string k, auto v )
        {
            if ( k == "raw" || ( indent && ck.count( k ) == 0 ) )
                return;
            auto i = indent ? ind : ind_attr;
            out << i << rightpad( k + ": ", 13 - i.size() ) << v << std::endl;
        } );

    int col = 0, relrow = 0, relcnt = 0;

    std::stringstream rels;
    auto addrel =
        [&]( std::string n )
        {
            if ( relrow > 3 )
                return;

            if ( !relcnt++ )
                rels << "[ ";
            else
                rels << ", ";
            if ( indent + col + n.size() >= 68 )
            {
                if ( relrow <= 3 )
                    col = 0, rels << std::endl << ind
                                  << rightpad( "", 13 - indent )
                                  << ( relrow == 3 ? "..." : "" );
                ++ relrow;
            }
            if ( relrow <= 3 )
            {
                rels << n;
                col += n.size() + 2;
            }
        };

    components(
        [&]( std::string n, auto sub )
        {
            std::stringstream str;
            if ( depth )
            {
                sub.format( str, depth - 1, derefs, indent + 4 );
                out << ind << "." << n << ":" << std::endl << str.str();
            }
            else addrel( n );
        } );

    related(
        [&]( std::string n, auto rel )
        {
            if ( derefs > 0 && n == "deref" )
            {
                std::stringstream str;
                rel.format( str, depth, derefs - 1, indent + 4 );
                out << ind << n << ":" << std::endl << str.str();
                return;
            }
            else addrel( n );
        } );

    if ( relcnt )
        out << ind << rightpad( "related:", 13 - indent ) << rels.str() << " ]" << std::endl;
}

template< typename Prog, typename Heap >
std::string Node< Prog, Heap >::attribute( std::string key )
{
    std::string res = "-";
    attributes( [&]( auto k, auto v )
                {
                    if ( k == key )
                        res = v;
                } );
    return res;
}

template< typename Prog, typename Heap >
Node< Prog, Heap > Node< Prog, Heap >::related( std::string key )
{
    Node< Prog, Heap > res( _ctx, _snapshot );
    related( [&]( auto k, auto v )
             {
                 if ( k == key )
                     res = v;
             } );
    return res;
}

template struct Node< vm::Program, vm::CowHeap >;

}
