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
    explicit Parent( ParentPtr p, RootsSet & r ) : parent( p ), roots( r ) {}

    ParentPtr parent;
    RootsSet & roots;
};

inline ParentPtr make_parent( ParentPtr pp, RootsSet & rs ) {
    return std::make_shared< Parent >( pp, rs );
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

using Task = std::variant< Propagate >;

// ValuesPropagationAnalysis
struct VPA {
    // Returns pairs of funcions with reached roots
    Reached run( llvm::Module & m );

private:
    void record( llvm::Function * fn );

    void dispach( Task && );
    void preprocess( llvm::Function * );
    void propagate( const Propagate & );

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
