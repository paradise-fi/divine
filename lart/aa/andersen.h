// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>

#include <vector>
#include <deque>
#include <set>
#include <map>

namespace lart {
namespace aa {

struct Andersen {
    struct Node {
        bool queued:1, aml:1;
        std::set< Node * > _pointsto;
        Node() : queued( false ), aml( false ) {}
    };

    struct Constraint {
        enum Type {
            Ref,   //  left = &right (alloc)
            Copy,  //  left =  right (bitcast, inttoptr &c.)
            Deref, //  left = *right (load)
            Store, // *left =  right  (store)
            GEP    //  left =  right + offset (getelementptr)
        };
        /* TODO: GEP needs a representation for offsets */

        // ?left \subseteq ?right (with ? depending on Type)
        Node *left, *right;
        Type t;
    };

    /* each llvm::Value can have (at most) one associated Node */
    std::map< llvm::Value *, Node * > _nodes;
    std::vector< Node * > _amls; // abstract memory locations
    std::vector< Constraint > _constraints;
    std::deque< Node * > _worklist;

    std::map< Node *, llvm::MDNode * > _mdnodes;
    std::map< Node *, llvm::MDNode * > _mdtemp;
    llvm::MDNode *_rootctx;
    llvm::Module *_module;

    Node *pop();
    void push( Node *n );

    void constraint( Constraint::Type t, Node *l, Node *r )
    {
        Constraint c;
        c.t = t;
        c.left = l;
        c.right = r;
        _constraints.push_back( c );
    }

    llvm::Value *fixGlobal( llvm::Value *v ) {
        if ( auto gv = llvm::dyn_cast< llvm::GlobalVariable >( v ) ) {
            if ( gv->hasInitializer() )
                return gv->getInitializer();
        }
        return v;
    }

    void constraint( Constraint::Type t, llvm::Value *l, Node *r ) {
        l = fixGlobal( l );
        if ( !_nodes[ l ] )
            _nodes[ l ] = new Node;
        return constraint( t, _nodes[ l ], r );
    }

    void constraint( Constraint::Type t, llvm::Value *l, llvm::Value *r ) {
        l = fixGlobal( l );
        r = fixGlobal( r );
        if ( !_nodes[ l ] )
            _nodes[ l ] = new Node;
        if ( !_nodes[ r ] )
            _nodes[ r ] = new Node;
        return constraint( t, _nodes[ l ], _nodes[ r ] );
    }

    void constrainReturns( llvm::Function *f, llvm::Value *r );

    void build( llvm::Instruction &i ); // set up _nodes and _constraints
    void build( llvm::Module &m ); // set up _nodes and _constraints
    void solve( Constraint c ); // process the effect of a single constraint
    void solve( Node *n ); // process the effect of a single node
    void solve(); // compute points-to sets for all nodes
    void annotate( llvm::Module &m ); // build up metadata nodes

    llvm::MDNode *annotate( Node *n, std::set< Node * > &seen );
    void annotate( llvm::Instruction *i, std::set< Node * > &seen );
    void annotate( llvm::GlobalValue *v, std::set< Node * > &seen );
};

}
}
