// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-data>
#include <brick-llvm>

#include <lart/support/query.h>
#include <lart/abstract/meta.h>

namespace lart::abstract {

    enum class DomainKind : uint8_t {
        scalar,
        pointer,
        aggregate
    };

    std::string to_string( DomainKind kind );

} // namespace lart::abstract
