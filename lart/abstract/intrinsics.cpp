// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/intrinsics.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

#include <brick-assert>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

inline std::string intrinsic_prefix( Instruction *i, Domain d ) {
    return "lart." + DomainTable[ d ] + "." + i->getOpcodeName();
}

using Predicate = CmpInst::Predicate;
const std::unordered_map< Predicate, std::string > predicate = {
    { Predicate::ICMP_EQ, "eq" },
    { Predicate::ICMP_NE, "ne" },
    { Predicate::ICMP_UGT, "ugt" },
    { Predicate::ICMP_UGE, "uge" },
    { Predicate::ICMP_ULT, "ult" },
    { Predicate::ICMP_ULE, "ule" },
    { Predicate::ICMP_SGT, "sgt" },
    { Predicate::ICMP_SGE, "sge" },
    { Predicate::ICMP_SLT, "slt" },
    { Predicate::ICMP_SLE, "sle" }
};

std::string intrinsic_name( Instruction *i, Domain d ) {
    auto pref = intrinsic_prefix( i, d );

    if ( auto icmp = dyn_cast< ICmpInst >( i ) )
        return pref + "_" + predicate.at( icmp->getPredicate() )
                    + "." + llvm_name( icmp->getOperand( 0 )->getType() );

    if ( isa< BinaryOperator >( i ) )
        return pref + "." + llvm_name( i->getType() );

    if ( auto ci = dyn_cast< CastInst >( i ) )
        return pref + "." + llvm_name( ci->getSrcTy() )
                    + "." + llvm_name( ci->getDestTy() );

    UNREACHABLE( "Unhandled intrinsic." );
}

} // anonymous namespace

bool is_lart_intr( Value * ) { NOT_IMPLEMENTED(); }

bool is_lower( Value * ) { NOT_IMPLEMENTED(); }

bool is_assume( Value * ) { NOT_IMPLEMENTED(); }

Function* get_intrinsic( Instruction *i, Domain d ) {
    auto m = getModule( i );
    auto fty = FunctionType::get( i->getType(), types_of( i->operands() ), false );
    auto name = intrinsic_name( i, d );
    return cast< Function >( m->getOrInsertFunction( name, fty ) );
}

} // namespace abstract
} // namespace lart
