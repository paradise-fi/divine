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

        static constexpr const char * prefix = "__vm_test_taint";

        explicit Taint( llvm::Instruction * inst, Type type )
            : type( type ), inst( inst )
        {
            meta::set( inst, meta::tag::taint::type, Placeholder::TypeTable[ type ] );
        }

        static bool is( llvm::Instruction * inst )
        {
            return meta::has( inst, meta::tag::taint::type );
        }

        Type type;
        llvm::Instruction * inst;
    };

    struct Tainting {
        void run( llvm::Module & m );
        Taint dispach( const Placeholder & ph ) const;
    };

} // namespace lart::abstract
