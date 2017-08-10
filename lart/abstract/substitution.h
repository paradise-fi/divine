// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/domains/zero.h>
#include <lart/abstract/domains/sym.h>

#include <lart/abstract/substituter.h>

namespace lart {
namespace abstract {

struct Substitution {
    void run( llvm::Module & );

    void init( llvm::Module & m ) {
        if ( Zero::isPresent( m ) )
            subst.registerAbstraction( std::make_shared< Zero >( m ) );
        if ( Symbolic::isPresent( m ) )
            subst.registerAbstraction( std::make_shared< Symbolic >( m ) );
    }

    void process( llvm::Value * );
    void changeReturn( llvm::Function * );
    Substituter subst;
};

} // namespace abstract
} // namespace lart

