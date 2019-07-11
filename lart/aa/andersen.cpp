// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/CallSite.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/aa/andersen.h>
#include <iostream>
#include <brick-assert>
#include <brick-llvm>
#include <brick-string>

namespace lart {
namespace aa {

void Andersen::push( Node *n ) {
    if ( n->queued )
        return;

    _worklist.push_back( n );
    n->queued = true;
}

Andersen::Node *Andersen::pop() {
    Node *n = _worklist.front();
    _worklist.pop_front();
    n->queued = false;
    return n;
}

template< typename Cin, typename Cout >
void copyout( Cin &in, Cout &out ) {
    std::copy( in.begin(), in.end(), std::inserter( out, out.begin() ) );
}

void Andersen::solve( Constraint c ) {
    std::set< Node * > updated = c.left->_pointsto;

    std::cerr << (c.left->aml ? 'm' : 'i') << c.left << " {";

    for ( auto p : c.left->_pointsto )
        std::cerr << " " << (p->aml ? 'm' : 'i') << p;
    std::cerr << " }" << std::endl;

    std::cerr <<  (c.right->aml ? 'm' : 'i') << c.right << " {";
    for ( auto p : c.right->_pointsto )
        std::cerr << " " << (p->aml ? 'm' : 'i') << p;
    std::cerr << " }" << std::endl;

    switch ( c.t ) {
        case Constraint::Ref:
            std::cerr << "ref" << std::endl;
            ASSERT( c.right->aml );
            updated.insert( c.right );
            break;
        case Constraint::Deref:
            std::cerr << "deref" << std::endl;
            for ( auto x : c.right->_pointsto )
                copyout( x->_pointsto, updated );
            break;
        case Constraint::Copy:
            std::cerr << "copy" << std::endl;
            copyout( c.right->_pointsto, updated );
            break;
        case Constraint::Store:
            for ( auto x : c.left->_pointsto ) {
                auto s = x->_pointsto.size();
                copyout( c.right->_pointsto, x->_pointsto );
                if ( s < x->_pointsto.size() ) {
                    std::cerr << "updated " << x << ":";
                    for ( auto p: x->_pointsto )
                        std::cerr << " " << (p->aml ? 'm' : 'i') << p;
                    std::cerr << std::endl;
                    push( x );
                }
            }
            updated = c.left->_pointsto; // XXX
            std::cerr << "store" << std::endl;
            break;
        default: UNREACHABLE( "switch fell through" );
    }

    for ( auto n: updated )
        ASSERT( n->aml );

    if ( updated != c.left->_pointsto ) {
        std::cerr << "updated:";
        for ( auto p: updated )
            std::cerr << " " << (p->aml ? 'm' : 'i') << p;
        std::cerr << std::endl << std::endl;
        c.left->_pointsto = updated;
        push( c.left );
    }
    std::cerr << std::endl;
}

void Andersen::solve( Node *n ) {
    /* TODO: optimize */
    std::vector< Constraint >::iterator i;
    for ( i = _constraints.begin(); i != _constraints.end(); ++i )
        if ( i->left == n || i->right == n )
            solve( *i );
}

void Andersen::solve()
{
    for ( auto i : _nodes )
        push( i.second );
    for ( auto i : _amls )
        push( i );

    while ( !_worklist.empty() ) {
        std::cerr << "worklist: " << _worklist.size() << std::endl;
        solve( pop() );
    }
}

void Andersen::constrainReturns( llvm::Function *f, llvm::Value *r )
{
    for ( auto &b: *f )
        for ( auto &i : b )
            if ( llvm::isa< llvm::ReturnInst >( i ) )
                constraint( Constraint::Copy, r, i.getOperand( 0 ) );
}

void Andersen::build( llvm::Instruction &i ) {

    if ( llvm::isa< llvm::AllocaInst >( i ) ) {
        _amls.push_back( new Node );
        _amls.back()->aml = true;
        constraint( Constraint::Ref, &i, _amls.back() );
    }

    if ( llvm::isa< llvm::StoreInst >( i ) )
        constraint( Constraint::Store, i.getOperand( 1 ), i.getOperand( 0 ) );

    if ( llvm::isa< llvm::LoadInst >( i ) )
    {
        TRACE( i );
        constraint( Constraint::Deref, &i, i.getOperand( 0 ) );
        // std::cerr << _nodes[ &i ] << " <- " << _nodes[ i.getOperand( 0 ) ] << std::endl;
    }

    if ( llvm::isa< llvm::BitCastInst >( i ) ||
         llvm::isa< llvm::IntToPtrInst >( i ) ||
         llvm::isa< llvm::PtrToIntInst >( i ) ||
         llvm::isa< llvm::GetElementPtrInst >( i ) )
        constraint( Constraint::Copy, &i, i.getOperand( 0 ) );

    if ( llvm::isa< llvm::CallInst >( i ) ) {
        llvm::CallSite cs( &i );
        if ( auto *F = cs.getCalledFunction() ) {
            if ( F->getName().str() == "__divine_malloc" ) { /* FIXME */
                _amls.push_back( new Node );
                _amls.back()->aml = true;
                constraint( Constraint::Ref, &i, _amls.back() );
            } else {
                int idx = 0;
                for ( auto &arg : F->args() )
                {
                    ASSERT( idx <= int( i.getNumOperands() ) );
                    constraint( Constraint::Copy, &arg, i.getOperand( idx ) );
                    ++ idx;
                }
                if ( !i.getType()->isVoidTy() )
                    constrainReturns( F, &i );
            }
        }
    }

    /*
     * TODO: We need to support LLVM intrinsics here *somehow*. Lowering them
     * might be an option, but we don't want to actually write out a version of
     * the bitcode in that form. Lowering is a one-way process, though. To this
     * end, we may need to make a working copy of the module, establish a
     * mapping between those two copies, and then run lowering on one of them,
     * maintaining the mapping somehow.
     *
     * Another option would be to do the lowering and just write it out, as a
     * first step of LART as a whole. This does not impair code correctness,
     * only efficiency: the code generator can no longer insert the efficient
     * arch-specific code for those intrinsics.
     */

    if ( llvm::isa< llvm::PHINode >( i ) )
        for ( int j = 0; j < int( i.getNumOperands() ); ++j )
            constraint( Constraint::Copy, &i, i.getOperand( j ) );
}

void Andersen::build( llvm::Module &m ) {
    for ( auto &v : m.globals() )
    {
        if ( !v.hasInitializer() )
            continue;
        constraint( Constraint::Ref, &v, v.getInitializer() );
        _nodes[ v.getInitializer() ]->aml = true;
    }

    for ( auto &f : m )
        for ( auto &b: f )
            for ( auto &i : b )
                build( i );
}

typedef llvm::ArrayRef< llvm::Metadata * > MDsRef;

llvm::MDNode *Andersen::annotate( Node *n, std::set< Node * > &seen )
{
    llvm::Module &m = *_module;
    llvm::MDNode *mdn;

    /* already converted */
    if ( _mdnodes.count( n ) )
        return _mdnodes.find( n )->second;

    /* loop has formed, insert a temporary node and let the caller handle this */
    if ( seen.count( n ) ) {
        if ( _mdtemp.count( n ) )
            return _mdtemp.find( n )->second;
        mdn = llvm::MDNode::getTemporary( m.getContext(), MDsRef() ).get();
        _mdtemp.insert( std::make_pair( n, mdn ) );
        return mdn;
    }

    seen.insert( n );
    int id = seen.size();

    /* make the points-to set first */
    {
        std::set< Node * > &pto = n->_pointsto;
        llvm::Metadata **v = new llvm::Metadata *[ pto.size() ], **vi = v;

        for ( Node *p : pto )
            *vi++ = annotate( p, seen );

        mdn = llvm::MDNode::get( m.getContext(), MDsRef( v, pto.size() ) );
    }

    /* now make the AML node */
    if ( n->aml ) {
        llvm::Metadata **v = new llvm::Metadata *[3];
        v[0] = llvm::ValueAsMetadata::get( llvm::ConstantInt::get( m.getContext(), llvm::APInt( 32, id ) ) );
        v[1] = _rootctx;
        v[2] = mdn;
        MDsRef vals( v, 3 );
        mdn = llvm::MDNode::get( m.getContext(), vals );
    }

    _mdnodes.insert( std::make_pair( n, mdn ) );

    /* close the cycles, if any */
    if ( _mdtemp.count( n ) ) {
        _mdtemp.find( n )->second->replaceAllUsesWith( mdn );
        _mdtemp.erase( n );
    }

    return mdn;
}

void Andersen::annotate( llvm::GlobalValue *, std::set< Node * > & /* seen */ )
{
}

void Andersen::annotate( llvm::Instruction *insn, std::set< Node * > &seen )
{
    if ( _nodes.count( insn ) )
        insn->setMetadata( "aa_def", annotate( _nodes[ insn ], seen ) );

    int size = 0;
    std::vector< Node * > ops;
    for ( int i = 0; i < int( insn->getNumOperands() ); ++i ) {
        llvm::Value *op = insn->getOperand( i );
        if ( !_nodes.count( op ) )
            continue;
        ops.push_back( _nodes[ op ] );
        size += ops.back()->_pointsto.size();
    }

    llvm::Metadata **v = new llvm::Metadata *[ size ], **vi = v;
    for ( Node *n : ops )
        for ( Node *p : n->_pointsto )
            *vi++ = annotate( p, seen );

    insn->setMetadata(
        "aa_use", llvm::MDNode::get( _module->getContext(), MDsRef( v, size ) ) );
}

void Andersen::annotate( llvm::Module &m ) {
    _module = &m;

    // llvm::NamedMDNode *global = m.getOrInsertNamedMetadata( "lart.aa_global" );

    MDsRef ctxv(
        llvm::MDString::get( m.getContext(), "lart.aa-root-context" ) );
    _rootctx = llvm::MDNode::get( m.getContext(), ctxv );

    std::set< Node * > seen;

    for ( auto &v : m.globals() )
        annotate( &v, seen );

    for ( auto &f : m )
        for ( auto &b: f )
            for ( auto &i : b )
                annotate( &i, seen );
    ASSERT( _mdtemp.empty() );
}

}
}
