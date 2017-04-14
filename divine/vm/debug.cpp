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
#include <divine/vm/formula.hpp>

namespace divine {
namespace vm {

using namespace std::literals;

std::pair< llvm::StringRef, int > fileline( const llvm::Instruction &insn )
{
    auto loc = insn.getDebugLoc().get();
    if ( loc && loc->getNumOperands() )
        return std::make_pair( loc->getFilename(),
                               loc->getLine() );
    auto prog = llvm::getDISubprogram( insn.getParent()->getParent() );
    if ( prog )
        return std::make_pair( prog->getFilename(),
                               prog->getScopeLine() );
    return std::make_pair( "", 0 );
}

std::string location( const llvm::Instruction &insn )
{
    return location( fileline( insn ) );
}

std::string location( std::pair< llvm::StringRef, int > fl )
{
    if ( fl.second )
        return fl.first.str() + ":" + brick::string::fmt( fl.second );
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
    t = t ?: di_resolve();
    if ( !t )
        return nullptr;
    if ( auto deriv = llvm::dyn_cast< llvm::DIDerivedType >( t ) )
        return deriv->getBaseType().resolve( _ctx.program().ditypemap );
    if ( auto comp = llvm::dyn_cast< llvm::DICompositeType >( t ) )
        return comp->getBaseType().resolve(  _ctx.program().ditypemap );
    return nullptr;
}

template< typename Prog, typename Heap >
llvm::DICompositeType *DebugNode< Prog, Heap >::di_composite( uint64_t tag, llvm::DIType *t )
{
    t = t ?: di_resolve();
    if ( t && t->getTag() == tag )
        return llvm::dyn_cast< llvm::DICompositeType >( t );
    return nullptr;
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
bool DebugNode< Prog, Heap >::boundcheck( PointerV ptr, int size )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    return eval.boundcheck( []( auto ) { return std::stringstream(); }, ptr, size, false );
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
    if ( !boundcheck( addr, 1 ) )
        return false;
    if ( !boundcheck( addr, size() ) )
        return false;
    return true;
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::value( YieldAttr yield )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    PointerV loc( _address + _offset );

    int bitw = 0;
    if ( _type )
        bitw = _type->getPrimitiveSizeInBits();

    if ( _type && _type->isIntegerTy() )
        eval.template type_dispatch< IsIntegral >(
            bitw, Prog::Slot::Integer,
            [&]( auto v )
            {
                auto raw = v.get( loc );
                using V = decltype( raw );
                if ( bitoffset() || width() != size() * 8 )
                {
                    yield( "raw_value", brick::string::fmt( raw ) );
                    auto val = raw >> value::Int< 32 >( bitoffset() );
                    val = val & V( bitlevel::ones< typename V::Raw >( width() ) );
                    ASSERT_LEQ( bitoffset() + width(), size() * 8 );
                    yield( "value", brick::string::fmt( val ) );
                }
                else
                    yield( "value", brick::string::fmt( raw ) );
            } );

    if ( _type && _type->isFloatingPointTy() )
        eval.template type_dispatch< IsFloat >(
            bitw, Prog::Slot::Float,
            [&]( auto v )
            {
                yield( "value", brick::string::fmt( v.get( loc ) ) );
            } );

    if ( _type && _di_type && ( di_name() == "char*" ||  di_name() == "const char*" ) )
    {
        PointerV str_v;
        auto hloc = eval.ptr2h( PointerV( _address ) ) + _offset;
        _ctx.heap().read( hloc, str_v );
        auto str = eval.ptr2h( str_v );
        if ( _ctx.heap().valid( str ) )
                yield( "string", "\"" + _ctx.heap().read_string( str ) + "\"" ); /* TODO escape */
    }

    if ( _type && _type->isPointerTy() )
        eval.template type_dispatch< Any >(
            PointerBytes, Prog::Slot::Pointer,
            [&]( auto v ) { yield( "value", brick::string::fmt( v.get( loc ) ) ); } );

}

template< typename Prog, typename Heap >
std::string DebugNode< Prog, Heap >::di_scopename( llvm::DIScope *scope )
{
    std::string n;
    if ( !scope )
        scope = _di_var->getScope();

    auto parent = scope->getScope().resolve( _ctx.program().ditypemap );

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
std::string DebugNode< Prog, Heap >::di_name( llvm::DIType *t, bool in_alias )
{
    if ( !t )
        t = _di_type;
    if ( di_member( t ) )
        return di_name( di_base( t ) );

    auto &ditmap = _ctx.program().ditypemap;
    if ( auto subr = llvm::dyn_cast< llvm::DISubroutineType >( t ) )
    {
        auto types = subr->getTypeArray();
        std::stringstream fmt;
        if ( auto rvt = types[0].resolve( ditmap ) )
            fmt << di_name( rvt ) << "(";
        else
            fmt << "void" << "(";
        for ( unsigned i = 1; i < types.size(); ++i )
        {
            auto subt = types[i].resolve( ditmap ); // null means ellipsis here
            fmt << ( subt ? di_name( subt ) : "..." ) << ( i + 1 < types.size() ? ", " : "" );
        }
        fmt << ")";
        return fmt.str();
    }

    std::string name = t->getName().str();

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
    t->dump();
    UNREACHABLE( "unexpected debuginfo metadata" );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::attributes( YieldAttr yield )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    Prog &program = _ctx.program();

    yield( "address", brick::string::fmt( _address ) + "+" +
           brick::string::fmt( _offset ) );

    if ( _di_type )
        yield( "type", di_name() );

    if ( !valid() )
        return;

    auto hloc = eval.ptr2h( PointerV( _address ) );
    value( yield );

    yield( "raw", print::raw( _ctx.heap(), hloc + _offset, size() ) );

    if ( _address.type() == PointerType::Const || _address.type() == PointerType::Global )
        yield( "slot", brick::string::fmt( eval.ptr2s( _address ) ) );
    else if ( _address.type() == PointerType::Heap )
        yield( "shared", brick::string::fmt( _ctx.heap().shared( _address ) ) );

    if ( _address.type() == PointerType::Marked )
    {
        std::stringstream out, key;
        std::unordered_set< int > inputs;
        FormulaMap map( _ctx.heap(), "", inputs, out );
        map.convert( _address );
        yield( "formula", out.str() );
    }

    if ( _di_var )
    {
        yield( "scope", di_scopename() );
        yield( "definition", _di_var->getFilename().str() + ":" +
                              std::to_string( _di_var->getLine() ) );
    }

    if ( _kind == DNKind::Frame )
    {
        yield( "pc", brick::string::fmt( pc() ) );
        if ( pc().null() || pc().type() != PointerType::Code )
            return;
        auto *insn = &program.instruction( pc() );
        ASSERT_EQ( eval.pc(), CodePointer( pc() ) );
        if ( insn->opcode != OpArgs && insn->opcode != OpBB )
        {
            eval._instruction = insn;
            yield( "insn", print::instruction( eval, 0, 1000 ) );
        }
        auto npc = program.nextpc( pc() );
        auto op = program.find( nullptr, npc ).first;
        yield( "location", location( *op ) );

        auto sym = op->getParent()->getParent()->getName().str();
        yield( "symbol", print::demangle( sym ) );
    }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::bitcode( std::ostream &out )
{
    ASSERT_EQ( _kind, DNKind::Frame );
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    CodePointer iter = pc(), origpc = pc();
    auto &prog = _ctx.program();
    for ( iter.instruction( 0 ); prog.valid( iter ); iter = iter + 1 )
    {
        auto &i = prog.instruction( iter );
        if ( i.opcode == OpArgs )
            continue;
        eval._instruction = &i;
        _ctx.set( _VM_CR_PC, iter );
        out << ( iter == CodePointer( pc() ) ? ">>" : "  " );
        if ( i.opcode == OpBB )
        {
            auto iop = _ctx.program().find( nullptr, iter + 1 ).first;
            out << print::value( eval, iop->getParent() ) << ":" << std::endl;
        }
        else
            out << "  " << print::instruction( eval, 4 ) << std::endl;
    }
    _ctx.set( _VM_CR_PC, origpc );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::source( std::ostream &out )
{
    ASSERT_EQ( _kind, DNKind::Frame );
    out << print::source( subprogram(), _ctx.program(), pc() );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::components( YieldDN yield )
{
    if ( _kind == DNKind::Frame )
        framevars( yield );

    if ( _kind == DNKind::Globals )
        globalvars( yield );

    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    auto hloc = eval.ptr2h( PointerV( _address ) );

    if ( _type && _di_type &&
         di_composite( llvm::dwarf::DW_TAG_structure_type ) &&
         _type->isStructTy() )
        struct_fields( hloc, yield );

    if ( _type && _di_type && di_composite( llvm::dwarf::DW_TAG_array_type ) )
        array_elements( yield );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::related( YieldDN yield, bool anon )
{
    if ( !valid() )
        return;

    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );

    PointerV ptr;
    auto hloc = eval.ptr2h( PointerV( _address ) );
    int hoff = hloc.offset();

    if ( _type && _di_type && _type->isPointerTy() )
    {
        PointerV addr;
        _ctx.heap().read( hloc + _offset, addr );
        _related_ptrs.insert( addr.cooked() );
        auto kind = DNKind::Object;
        if ( di_name() == "_VM_Frame*" )
            kind = DNKind::Frame;
        DebugNode rel( _ctx, _snapshot );
        rel.address( kind, addr.cooked() );
        rel.type( _type->getPointerElementType() );
        rel.di_type( di_base() );
        yield( "deref", rel );
    }

    if ( _kind == DNKind::Frame )
    {
        PointerV fr( _address );
        _ctx.heap().skip( fr, PointerBytes );
        _ctx.heap().read( fr.cooked(), fr );
        if ( !fr.cooked().null() )
        {
            _related_ptrs.insert( fr.cooked() );
            DebugNode caller( _ctx, _snapshot );
            caller.address( DNKind::Frame, fr.cooked() );
            yield( "caller", caller );
        }
    }

    if ( !anon )
        return;

    for ( auto ptroff : _ctx.heap().pointers( hloc, hoff + _offset, size() ) )
    {
        hloc.offset( hoff + _offset + ptroff->offset() );
        if ( ptroff->offset() + ptroff->size() > size() )
            continue;
        _ctx.heap().read( hloc, ptr );
        auto pp = ptr.cooked();
        if ( pp.type() == PointerType::Code || pp.null() )
            continue;
        if ( _related_ptrs.find( pp ) != _related_ptrs.end() )
            continue;
        pp.offset( 0 );
        DebugNode deref( _ctx, _snapshot );
        deref.address( DNKind::Object, pp );
        yield( brick::string::fmt( ptroff->offset() ), deref );
    }
}

template< typename Prog, typename Heap >
llvm::DIType *DebugNode< Prog, Heap >::di_resolve( llvm::DIType *t )
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
            base = DT->getBaseType().resolve( _ctx.program().ditypemap );
        else return base;
    return t;
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::struct_fields( HeapPointer hloc, YieldDN yield )
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
            if ( (*STE)->isPointerTy() && boundcheck( PointerV( hloc + offset ), PointerBytes ) )
            {
                PointerV ptr;
                _ctx.heap().read( hloc + offset, ptr );
                _related_ptrs.insert( ptr.cooked() );
            }

            DebugNode field( _ctx, _snapshot );
            field.address( DNKind::Object, _address );
            field.offset( _offset + offset );
            field.type( *STE );
            field.di_type( CTE );
            std::string name = CTE->getName().str();
            if ( name.empty() && CTE->getTag() == llvm::dwarf::DW_TAG_inheritance )
                name = "<base"s + (base++ ? "$" + std::to_string( base ) : "") + ">";
            if ( name.empty() )
                name = "<anon"s + (anon++ ? "$" + std::to_string( base ) : "") + ">";
            yield( name, field );
        }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::array_elements( YieldDN yield )
{
    // variable-length arrays are allocated as pointers to the element type,
    // not as sequential types
    auto type = _type;
    if ( llvm::isa< llvm::SequentialType >( type ) )
        type = _type->getSequentialElementType();
    int size = _ctx.program().TD.getTypeAllocSize( type );
    PointerV addr( _address + _offset );

    for ( int idx = 0; boundcheck( addr + idx * size, size ); ++ idx )
    {
        DebugNode elem( _ctx, _snapshot );
        elem.address( DNKind::Object, _address );
        elem.offset( _offset + idx * size );
        elem.type( type );
        elem.di_type( di_base() );
        yield( "[" + std::to_string( idx ) + "]", elem );
    }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::localvar( YieldDN yield, llvm::DbgDeclareInst *DDI )
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
    _related_ptrs.insert( ptr.cooked() );

    auto type = var->getType()->getPointerElementType();
    auto name = divar->getName().str();

    if ( divar->getScope() != subprogram() )
        name += "$" + brick::string::fmt( ++ _related_count[ name ] );

    DebugNode lvar( _ctx, _snapshot );
    lvar.address( DNKind::Object, ptr.cooked() );
    lvar.type( type );
    lvar.di_type( ditype );
    yield( name, lvar );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::localvar( YieldDN yield, llvm::DbgValueInst *DDV )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );

    auto divar = DDV->getVariable();
    auto var = DDV->getValue();
    auto &vmap = _ctx.program().valuemap;
    if ( vmap.find( var ) == vmap.end() )
        return;

    auto sref = _ctx.program().valuemap[ var ];
    auto ptr = sref.slot.location == Prog::Slot::Local ?
               eval.s2ptr( sref.slot ) :
               _ctx.program().s2ptr( sref );
    PointerV deref;
    if ( boundcheck( PointerV( ptr ), PointerBytes ) )
        _ctx.heap().read( eval.ptr2h( PointerV( ptr ) ), deref );
    if ( deref.pointer() )
        _related_ptrs.insert( deref.cooked() );

    auto type = var->getType();
    auto name = divar->getName().str();

    if ( divar->getScope() != subprogram() )
        name += "$" + brick::string::fmt( ++ _related_count[ name ] );

    DebugNode lvar( _ctx, _snapshot );
    lvar.address( DNKind::Object, ptr );
    lvar.type( type );
    lvar.di_var( divar );
    yield( name, lvar );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::framevars( YieldDN yield )
{
    if ( pc().type() != PointerType::Code )
        return;

    auto npc = _ctx.program().nextpc( pc() );
    auto op = _ctx.program().find( nullptr, npc ).first;
    auto F = op->getParent()->getParent();

    for ( auto &BB : *F )
        for ( auto &I : BB )
            if ( auto DDI = llvm::dyn_cast< llvm::DbgDeclareInst >( &I ) )
                localvar( yield, DDI );
            else if ( auto DDV = llvm::dyn_cast< llvm::DbgValueInst >( &I ) )
                localvar( yield, DDV );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::globalvars( YieldDN yield )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    llvm::DebugInfoFinder finder;
    finder.processModule( *_ctx.program().module );
    auto &map = _ctx.program().globalmap;
    std::map< std::string, int > disamb;

    for ( auto GV : finder.global_variables() )
    {
        auto var = GV->getVariable();
        if ( !map.count( var ) )
            continue;
        auto ptr = _ctx.program().s2ptr( map[ var ] );

        PointerV deref;
        if ( boundcheck( PointerV( ptr ), PointerBytes ) )
            _ctx.heap().read( eval.ptr2h( PointerV( ptr ) ), deref );
        if ( deref.pointer() )
            _related_ptrs.insert( deref.cooked() );

        DebugNode dn( _ctx, _snapshot );
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
}

static std::string rightpad( std::string s, int i )
{
    while ( int( s.size() ) < i )
        s += ' ';
    return s;
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::format( std::ostream &out, int depth, int derefs, int indent )
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
