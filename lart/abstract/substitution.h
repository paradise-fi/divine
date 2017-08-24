// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/domains/zero.h>
#include <lart/abstract/domains/sym.h>

#include <lart/abstract/substituter.h>
#include <lart/abstract/data.h>

namespace lart {
namespace abstract {

template< typename TMap, typename Fns >
static auto make_sbuilder( TMap & tmap, Fns & fns, llvm::Module & m ) {
    auto s = Substituter< TMap, Fns >( tmap, fns );
    // TODO remove isPresent from domain implementeation
    s.addAbstraction( std::make_shared< TristateDomain >( tmap ) );
    if ( Zero::isPresent( m ) )
        s.addAbstraction( std::make_shared< Zero >( m, tmap ) );
    if ( Symbolic::isPresent( m ) )
        s.addAbstraction( std::make_shared< Symbolic >( m, tmap ) );
    return s;
}

struct Substitution {

    Substitution( PassData & data ) : data( data ) {}

    void run( llvm::Module & );

    PassData & data;
    ReMap< llvm::Function * > fns;
};


} // namespace abstract
} // namespace lart

