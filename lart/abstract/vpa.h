// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <deque>
#include <lart/abstract/value.h>
#include <lart/abstract/util.h>
#include <lart/abstract/fieldtrie.h>
namespace lart {
namespace abstract {

using RootsSet = std::set< AbstractValue >;
using RootsSetPtr = std::unique_ptr< RootsSet >;

namespace {

template< typename A, typename B >
static inline std::set< AbstractValue > unionRoots( const A& a, const B& b ) {
    std::set< AbstractValue > u;
    std::set_union( a->begin(), a->end(), b->begin(), b->end(), std::inserter( u, u.begin() ) );
    return u;
}

} // anonymous namespace

struct FunctionRoots {

    RootsSet * annotations() const {
        return annRoots.get();
    }

    RootsSet * argDepRoots( const ArgDomains & doms ) {
        if ( !argRoots.count( doms ) )
            argRoots[ doms ] = std::make_unique< RootsSet >();
        return argRoots[ doms ].get();
    }

    RootsSet roots( const ArgDomains & doms ) const {
        return unionRoots( annRoots, argRoots.at( doms ).get() );
    }

    RootsSet roots( RootsSet * rs ) const {
        return unionRoots( annRoots, rs );
    }

    void insert( AbstractValue av, RootsSet * rs ) {
        if ( auto a = av.get< llvm::Argument >() ) {
            if ( rs == annRoots.get() ) {
                auto fn = getFunction( a );
                auto doms = argDomains( fn, filterA< llvm::Argument >( *rs ) );
                rs = argRoots[ doms ].get();
                annRoots->insert( av );
            }

            auto roots = std::find_if( argRoots.begin(), argRoots.end(),
                [&] ( const auto & r ) { return r.second.get() == rs; } );

            assert( roots != argRoots.end() );
            if ( roots->first[ a->getArgNo() ] == Domain::LLVM ) {
                ArgDomains doms = roots->first;
                doms[ a->getArgNo() ] = av.domain;
                argRoots.insert( { doms, std::move( roots->second ) } );
                argRoots.erase( roots );
            }
        }
        rs->insert( av );
    }

    void insert( AbstractValue av ) {
        assert( !av.isa< llvm::Argument >() );
        annRoots->insert( av );
    }

    void init( size_t argSize ) {
        if ( !annRoots )
            annRoots = std::make_unique< RootsSet >();
        auto key = ArgDomains( argSize, Domain::LLVM );
        argRoots[ key ] = std::make_unique< RootsSet >();
    }


    bool has( const ArgDomains & doms ) const {
        return argRoots.count( doms );
    }

    Domain returns( const ArgDomains & doms ) const {
        // TODO cache return results
        auto rs = roots( doms );
        auto rf = reachFrom( { rs.begin(), rs.end() } );
        for ( auto & v : lart::util::reverse( rf ) )
            if ( v.isa< llvm::ReturnInst >() )
                return v.domain;
        return Domain::LLVM;
    }


    using ArgRootsSets = std::map< ArgDomains, RootsSetPtr >;
    using RootsIterator = ArgRootsSets::iterator;

    class iterator: public std::iterator< std::bidirectional_iterator_tag, RootsSet > {
    public:
        explicit iterator( RootsIterator roots, FunctionRoots & froots )
            : roots( roots ), froots( froots ) {}

        iterator& operator++() { roots++; return *this; }
        iterator operator++( int ) { iterator retval = *this; ++(*this); return retval; }

        bool operator==( iterator other ) const { return roots == other.roots; }
        bool operator!=( iterator other ) const { return !( *this == other ); }
        value_type operator*() const { return unionRoots( roots->second, froots.annRoots ); }
    private:
        RootsIterator roots;
        FunctionRoots & froots;
    };

    iterator begin() { return iterator( argRoots.begin(), *this ); }
    iterator end() { return iterator( argRoots.end(), *this ); }

private:
    RootsSetPtr annRoots; // annotation roots
    std::map< ArgDomains, RootsSetPtr > argRoots; // argument dependent roots
};

using Reached = std::map< llvm::Function *, FunctionRoots >;

struct Parent;

using ParentPtr = std::shared_ptr< Parent >;

struct Parent {
    explicit Parent( llvm::CallSite cs, ParentPtr p, RootsSet * r )
        : callsite( cs ), parent( p ), roots( r ) {}

    llvm::CallSite callsite;
    ParentPtr parent;
    RootsSet * roots;
};

inline ParentPtr make_parent( llvm::CallSite cs, ParentPtr pp, RootsSet * rs ) {
    return std::make_shared< Parent >( cs, pp, rs );
}

struct PropagateDown {
    explicit PropagateDown( AbstractValue v, RootsSet * r, ParentPtr p )
        : value( v ), roots( r ), parent( p ) {}

    AbstractValue value;        // propagated value
    RootsSet * roots;           // roots in which is value propagated
    ParentPtr parent;           // parent from which was the function called
};

inline bool operator==( const PropagateDown & a, const PropagateDown & b ) {
    return std::tie( a.value, a.roots, a.parent ) == std::tie( b.value, b.roots, b.parent );
}

struct StepIn {
    explicit StepIn( ParentPtr p ) : parent( p ) {}

    ParentPtr parent;
};

inline bool operator==( const StepIn & a, const StepIn & b ) {
    // TODO parent comparison
    return a.parent == b.parent;
}

struct StepOut {
    explicit StepOut( llvm::Function * f, Domain d, ParentPtr p )
        : function( f ), domain( d ), parent( p ) {}

    llvm::Function * function;
    Domain domain;
    ParentPtr parent;
};

inline bool operator==( const StepOut & a, const StepOut & b) {
    return std::tie( a.domain, a.parent ) == std::tie( b.domain, b.parent );
}

struct PropagateUp {
    explicit PropagateUp( llvm::Argument * a, RootsSet * r, ParentPtr p )
        : arg( a ), roots( r ), parent( p ) {}

    llvm::Argument * arg;
    RootsSet * roots;
    ParentPtr parent;
};

inline bool operator==( const PropagateUp & a, const PropagateUp & b) {
    return std::tie( a.arg, a.roots, a.parent ) == std::tie( b.arg, b.roots, b.parent );
}

struct PropagateGlobal {
    explicit PropagateGlobal( llvm::GlobalValue * v ) : value( v ) {}
    llvm::GlobalValue * value;
};

inline bool operator==( const PropagateGlobal & a, const PropagateGlobal & b) {
    return a.value == b.value;
}

using Task = std::variant< PropagateDown, PropagateUp, StepIn, StepOut, PropagateGlobal >;

// ValuesPropagationAnalysis
struct VPA {
    using Globals = RootsSet;
    using Roots = std::tuple< Reached, Globals >;
    // Returns pairs of funcions with reached roots
    Roots run( llvm::Module & m );

private:
    void record( llvm::Function * fn );

    void dispach( Task && );
    void preprocess( llvm::Function * );
    void propagateDown( const PropagateDown & );
    void propagateIntDown( const PropagateDown & );
    void propagatePtrOrStructDown( const PropagateDown & );
    void propagateUp( const PropagateUp & );
    void propagateFromGEP( llvm::GetElementPtrInst * gep, Domain dom, RootsSet * roots, ParentPtr parent );

    void stepIn( const StepIn & );
    void stepOut( const StepOut & );

    void markGlobal( llvm::GlobalValue * value );

    llvm::Value * origin( llvm::Value * value );

    std::deque< Task > tasks;

    AbstractFields< llvm::Value * > fields;

    Reached reached;
    Globals globals;

    std::map< RootsSet *, std::set< llvm::Value * > > seen;
};

} // namespace abstract
} // namespace lart
