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

#include <divine/vm/debug.hpp>
#include <divine/vm/print.hpp>
#include <divine/vm/eval.hpp>

namespace divine {
namespace vm {

std::pair< std::string, int > fileline( const llvm::Instruction &insn )
{
    auto loc = insn.getDebugLoc().get();
    if ( loc && loc->getNumOperands() )
        return std::make_pair( loc->getFilename().str(),
                               loc->getLine() );
    auto prog = llvm::getDISubprogram( insn.getParent()->getParent() );
    if ( prog )
        return std::make_pair( prog->getFilename().str(),
                               prog->getScopeLine() );
    return std::make_pair( "", 0 );
}

std::string location( const llvm::Instruction &insn )
{
    auto fl = fileline( insn );
    if ( fl.second )
        return fl.first + ":" + brick::string::fmt( fl.second );
    return "(unknown location)";
}

template< typename Prog, typename Heap >
using DNEval = Eval< Prog, ConstContext< Prog, Heap >, value::Void >;

template< typename Prog, typename Heap >
int DebugNode< Prog, Heap >::size()
{
    int sz = INT_MAX;
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    if ( _type )
        sz = _ctx.program().TD.getTypeAllocSize( _type );
    if ( !_address.null() )
        sz = std::min( sz, eval.ptr2sz( PointerV( _address ) ) - _offset );
    return sz;
}

template< typename Prog, typename Heap >
llvm::DIDerivedType *DebugNode< Prog, Heap >::di_derived( uint64_t tag, llvm::DIType *t )
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
llvm::DIDerivedType *DebugNode< Prog, Heap >::di_member( llvm::DIType *t )
{
    return di_derived( llvm::dwarf::DW_TAG_member, t );
}

template< typename Prog, typename Heap >
llvm::DIDerivedType *DebugNode< Prog, Heap >::di_pointer( llvm::DIType *t )
{
    return di_derived( llvm::dwarf::DW_TAG_pointer_type, t );
}

template< typename Prog, typename Heap >
llvm::DIType *DebugNode< Prog, Heap >::di_base( llvm::DIType *t )
{
    if ( di_member( t ) )
        return di_member( t )->getBaseType().resolve( _ctx.program().ditypemap );
    if ( di_pointer( t ) )
        return di_pointer( t )->getBaseType().resolve( _ctx.program().ditypemap );
    return nullptr;
}

template< typename Prog, typename Heap >
llvm::DICompositeType *DebugNode< Prog, Heap >::di_composite()
{
    return llvm::dyn_cast< llvm::DICompositeType >( di_base() ?: _di_type );
}

template< typename Prog, typename Heap >
int DebugNode< Prog, Heap >::width()
{
    if ( di_member() )
        return di_member()->getSizeInBits();
    return 8 * size();
}

template< typename Prog, typename Heap >
int DebugNode< Prog, Heap >::bitoffset()
{
    int rv = 0;
    if ( di_member() )
        rv = di_member()->getOffsetInBits() - 8 * _offset;
    ASSERT_LEQ( rv, 8 * size() );
    ASSERT_LEQ( rv + width(), 8 * size() );
    return rv;
}

template< typename Prog, typename Heap >
bool DebugNode< Prog, Heap >::valid()
{
    if ( _address.null() )
        return false;
    if ( _address.type() == PointerType::Heap && !_ctx.heap().valid( _address ) )
        return false;

    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    PointerV addr( _address );
    if ( !eval.boundcheck( addr, 1, false ) )
        return false;
    if ( !eval.boundcheck( addr, size(), false ) )
        return false;
    return true;
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::value( YieldAttr yield )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    if ( _type && _type->isIntegerTy() )
        eval.template type_dispatch< IsIntegral >(
            _type->getPrimitiveSizeInBits(), Prog::Slot::Integer,
            [&]( auto v )
            {
                auto raw = v.get( PointerV( _address + _offset ) );
                using V = decltype( raw );
                if ( bitoffset() || width() != size() * 8 )
                {
                    yield( "@raw_value", brick::string::fmt( raw ) );
                    auto val = raw >> value::Int< 32 >( bitoffset() );
                    val = val & V( bitlevel::ones< typename V::Raw >( width() ) );
                    ASSERT_LEQ( bitoffset() + width(), size() * 8 );
                    yield( "@value", brick::string::fmt( val ) );
                }
                else
                    yield( "@value", brick::string::fmt( raw ) );
            } );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::attributes( YieldAttr yield )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    Prog &program = _ctx.program();

    yield( "@address", brick::string::fmt( _address ) + "+" +
           brick::string::fmt( _offset ) );

    std::string typesuf;
    auto dit = _di_type, base = di_base();
    while (( dit = di_base( dit ) ))
        typesuf += di_pointer( dit ) ? "*" : "", base = dit;

    if ( base )
        yield( "@type", base->getName().str() + typesuf );

    if ( !valid() )
        return;

    auto hloc = eval.ptr2h( PointerV( _address ) );
    value( yield );

    yield( "@raw", print::raw( _ctx.heap(), hloc + _offset, size() ) );

    if ( _address.type() == PointerType::Const || _address.type() == PointerType::Global )
        yield( "@slot", brick::string::fmt( eval.ptr2s( _address ) ) );

    if ( _kind == DNKind::Frame )
    {
        yield( "@pc", brick::string::fmt( pc() ) );
        if ( pc().null() || pc().type() != PointerType::Code )
            return;
        auto *insn = &program.instruction( pc() );
        if ( insn->op )
        {
            eval._instruction = insn;
            yield( "@instruction", print::instruction( eval ) );
        }
        if ( !insn->op )
            insn = &program.instruction( pc() + 1 );
        ASSERT( insn->op );
        auto op = llvm::cast< llvm::Instruction >( insn->op );
        yield( "@location", location( *op ) );

        auto sym = op->getParent()->getParent()->getName().str();
        yield( "@symbol", print::demangle( sym ) );
    }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::bitcode( std::ostream &out )
{
    ASSERT_EQ( _kind, DNKind::Frame );
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    CodePointer iter = pc();
    iter.instruction( 0 );
    auto &instructions = _ctx.program().function( iter ).instructions;
    for ( auto &i : instructions )
    {
        eval._instruction = &i;
        out << ( iter == CodePointer( pc() ) ? ">>" : "  " );
        if ( i.op )
            out << print::instruction( eval, 2 ) << std::endl;
        else
        {
            auto iop = llvm::cast< llvm::Instruction >( instructions[ iter.instruction() + 1 ].op );
            out << print::value( eval, iop->getParent() ) << ":" << std::endl;
        }
        iter = iter + 1;
    }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::source( std::ostream &out )
{
    ASSERT_EQ( _kind, DNKind::Frame );
    out << print::source( subprogram(), _ctx.program(), pc() );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::related( YieldDN yield )
{
    if ( !valid() )
        return;

    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );

    PointerV ptr;
    auto hloc = eval.ptr2h( PointerV( _address ) );
    int hoff = hloc.offset();

    std::set< GenericPointer > ptrs;

    if ( _kind == DNKind::Frame )
        framevars( yield, ptrs );

    if ( _type && _di_type && _type->isPointerTy() )
    {
        PointerV addr;
        _ctx.heap().read( hloc + _offset, addr );
        ptrs.insert( addr.cooked() );
        auto kind = DNKind::Object;
        auto base = di_base( di_base() );
        if ( base && base->getName() == "_VM_Frame" )
            kind = DNKind::Frame;
        yield( "@deref", DebugNode( _ctx, _snapshot, addr.cooked(), 0, kind,
                                    _type->getPointerElementType(), di_base() ) );
    }

    if ( _type && _di_type && _type->isStructTy() )
        struct_fields( hloc, yield, ptrs );

    for ( auto ptroff : _ctx.heap().pointers( hloc, hoff + _offset, size() ) )
    {
        hloc.offset( hoff + _offset + ptroff->offset() );
        _ctx.heap().read( hloc, ptr );
        auto pp = ptr.cooked();
        if ( pp.type() == PointerType::Code || ptrs.find( pp ) != ptrs.end() )
            continue;
        pp.offset( 0 );
        yield( "@" + brick::string::fmt( ptroff->offset() ),
               DebugNode( _ctx, _snapshot, pp, 0, DNKind::Object, nullptr, nullptr ) );
    }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::struct_fields( HeapPointer hloc, YieldDN yield,
                                             std::set< GenericPointer > &ptrs )
{
    auto CT = llvm::dyn_cast< llvm::DICompositeType >( _di_type );
    if ( !CT )
    {
        auto DIT = llvm::cast< llvm::DIDerivedType >( _di_type );
        auto base = DIT->getBaseType().resolve( _ctx.program().ditypemap );
        CT = llvm::dyn_cast< llvm::DICompositeType >( base );
        ASSERT( CT );
    }
    auto ST = llvm::cast< llvm::StructType >( _type );
    auto STE = ST->element_begin();
    auto SLO = _ctx.program().TD.getStructLayout( ST );
    int idx = 0;
    for ( auto subtype : CT->getElements() )
        if ( auto CTE = llvm::dyn_cast< llvm::DIDerivedType >( subtype ) )
        {
            if ( idx + 1 < int( ST->getNumElements() ) &&
                 CTE->getOffsetInBits() >= 8 * SLO->getElementOffset( idx + 1 ) )
                idx ++, STE ++;

            int offset = SLO->getElementOffset( idx );
            if ( (*STE)->isPointerTy() )
            {
                PointerV ptr;
                _ctx.heap().read( hloc + offset, ptr );
                ptrs.insert( ptr.cooked() );
            }

            yield( CTE->getName().str(),
                   DebugNode( _ctx, _snapshot, _address, _offset + offset,
                              DNKind::Object, *STE, CTE ) );
        }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::localvar( YieldDN yield, llvm::DbgDeclareInst *DDI,
                                        std::set< GenericPointer > &ptrs )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );

    auto divar = DDI->getVariable();
    auto ditype = divar->getType().resolve( _ctx.program().ditypemap );
    auto var = DDI->getAddress();
    auto &vmap = _ctx.program().valuemap;
    if ( vmap.find( var ) == vmap.end() )
        return;

    PointerV ptr;
    _ctx.heap().read( eval.s2ptr( _ctx.program().valuemap[ var ].slot ), ptr );
    ptrs.insert( ptr.cooked() );

    auto type = var->getType()->getPointerElementType();
    yield( divar->getName().str(),
           DebugNode( _ctx, _snapshot, ptr.cooked(), 0, DNKind::Object, type, ditype ) );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::framevars( YieldDN yield, std::set< GenericPointer > &ptrs )
{
    PointerV fr( _address );
    _ctx.heap().skip( fr, PointerBytes );
    _ctx.heap().read( fr.cooked(), fr );
    if ( !fr.cooked().null() )
    {
        ptrs.insert( fr.cooked() );
        yield( "@parent", DebugNode( _ctx, _snapshot, fr.cooked(), 0,
                                     DNKind::Frame, nullptr, nullptr ) );
    }

    auto *insn = &_ctx.program().instruction( pc() );
    if ( !insn->op )
        insn = &_ctx.program().instruction( pc() + 1 );
    auto op = llvm::cast< llvm::Instruction >( insn->op );
    auto F = op->getParent()->getParent();

    for ( auto &BB : *F )
        for ( auto &I : BB )
            if ( auto DDI = llvm::dyn_cast< llvm::DbgDeclareInst >( &I ) )
                localvar( yield, DDI, ptrs );
}

template< typename Prog, typename Heap >
std::string DebugNode< Prog, Heap >::attribute( std::string key )
{
    std::string res = "-";
    attributes( [&]( auto k, auto v )
                {
                    if ( k == key )
                        res = v;
                } );
    return res;
}

template struct DebugNode< Program, CowHeap >;

}
}
