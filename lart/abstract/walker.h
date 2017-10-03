// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <deque>
#include <lart/abstract/value.h>

namespace lart {
namespace abstract {

using RootsSet = std::set< AbstractValue >;
using ArgDomains = std::vector< Domain >;
struct FunctionRoots {
    RootsSet annRoots;                         // annotation roots
    std::map< ArgDomains, RootsSet > argRoots; // argument dependent roots
};

using Reached = std::map< llvm::Function *, FunctionRoots >;

struct Parent;

using ParentPtr = std::shared_ptr< Parent >;

struct Parent {
    explicit Parent( llvm::CallSite cs, ParentPtr p, RootsSet & r )
        : callsite( cs ), parent( p ), roots( r ) {}

    llvm::CallSite callsite;
    ParentPtr parent;
    RootsSet & roots;
};

inline ParentPtr make_parent( llvm::CallSite cs, ParentPtr pp, RootsSet & rs ) {
    return std::make_shared< Parent >( cs, pp, rs );
}

struct Propagate {
    explicit Propagate( AbstractValue v, RootsSet & r, ParentPtr p )
        : value( v ), roots( r ), parent( p ) {}

    AbstractValue value;        // propagated value
    RootsSet& roots;            // roots in which is value propagated
    ParentPtr parent;           // parent from which was the function called
};

inline bool operator==( const Propagate & a, const Propagate & b ) {
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

using Task = std::variant< Propagate, StepIn, StepOut >;

// ValuesPropagationAnalysis
struct VPA {
    // Returns pairs of funcions with reached roots
    Reached run( llvm::Module & m );

private:
    void record( llvm::Function * fn );

    void dispach( Task && );
    void preprocess( llvm::Function * );
    void propagate( const Propagate & );

    void stepIn( const StepIn & );
    void stepOut( const StepOut & );

    Domain returns( llvm::Function *, const RootsSet & );

    std::deque< Task > tasks;

    Reached reached;
};

template< typename A, typename B >
static inline std::set< AbstractValue > unionRoots( const A& a, const B& b ) {
    std::set< AbstractValue > u;
    using std::set_union;
    set_union( a.begin(), a.end(), b.begin(), b.end(), std::inserter( u, u.begin() ) );
    return u;
}

} // namespace abstract
} // namespace lart
