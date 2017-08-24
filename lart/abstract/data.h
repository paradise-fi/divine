// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

using TypeLift = Lift< llvm::Type * >;
using ValueLift = Lift< llvm::Value * >;

struct TMap : LiftMap< TypeLift, llvm::Type * > {
    bool isAbstract( llvm::Type * type ) {
        return ::lart::abstract::isAbstract( type, *this );
    }
};

struct PassData {
    TMap tmap;
};

} // namespace abstract
} // namespace lart
