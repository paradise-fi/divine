// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domain.h>
#include <lart/abstract/placeholder.h>

namespace lart::abstract {

    struct Taint
    {
        using Type = Placeholder::Type;

        explicit Taint( llvm::Instruction * inst, Type type )
            : type( type ), inst( inst )
        {
            meta::set( inst, meta::tag::taint::type, Placeholder::TypeTable[ type ] );
        }

        Type type;
        llvm::Instruction * inst;
    };

    struct TaintBuilder
    {
        Taint construct( const Placeholder & ph );

    private:
        template< Taint::Type T >
        Taint construct( const Placeholder & ph );
    };

    struct Tainting {
        void run( llvm::Module & m );
    };

} // namespace lart::abstract
