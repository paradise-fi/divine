// -*- C++ -*- (c) 2016-2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/dfa.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
#include <llvm/Transforms/Utils.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/lowerselect.h>
#include <lart/abstract/domain.h>
#include <lart/abstract/util.h>

#include <brick-llvm>

namespace lart::abstract {

    using LatticeValue = DataFlowAnalysis::LatticeValue;

    struct AddAbstractMetaVisitor : llvm::InstVisitor< AddAbstractMetaVisitor >
    {
        AddAbstractMetaVisitor( const LatticeValue &lattice )
            : _lattice( lattice )
        {}

        static constexpr char op_prefix[] = "lart.abstract.op_";

        void add_meta( llvm::Instruction *val, const std::string& operation )
        {
            meta::abstract::set( val, to_string( DomainKind::scalar ) );

            auto m = val->getModule();
            auto op = m->getFunction( operation );
            auto index = op->getMetadata( meta::tag::operation::index );
            val->setMetadata( meta::tag::operation::index, index );
        }

        void visitCmpInst( llvm::CmpInst &cmp )
        {
            auto op = std::string( op_prefix )
                    + PredicateTable.at( cmp.getPredicate() );
            add_meta( &cmp, op );
        }

        void visitCastInst( llvm::CastInst &cast )
        {
            auto op = std::string( op_prefix )
                    + std::string( cast.getOpcodeName() );
            add_meta( &cast, op );
        }

        void visitBinaryOperator( llvm::BinaryOperator &bin )
        {
            auto op = std::string( op_prefix )
                    + std::string( bin.getOpcodeName() );
            add_meta( &bin, op );
        }

        const LatticeValue &_lattice;
    };

    bool DataFlowAnalysis::join( llvm::Value * lhs, llvm::Value * rhs ) noexcept
    {
        return interval( lhs ).join( interval( rhs ) );
    }

    void DataFlowAnalysis::propagate( llvm::Value * val ) noexcept
    {
        for ( auto u : val->users() ) {
            if ( join( u, val ) )
                push( [=] { propagate( u ); } );
        }
    }

    void DataFlowAnalysis::run( llvm::Module &m )
    {
        for ( auto * val : meta::enumerate( m ) ) {
            // TODO preprocessing

            interval( val ).to_bottom();
            push( [=] { propagate( val ); } );
        }

        while ( !_tasks.empty() ) {
            _tasks.front()();
            _tasks.pop_front();
        }

        for ( const auto & [ val, in ] : _intervals ) {
            // TODO decide what to annotate
            add_meta( val, in );
        }
    }

    void DataFlowAnalysis::add_meta( llvm::Value *v, const LatticeValue& l ) noexcept
    {
        auto inst = llvm::cast< llvm::Instruction >( v );
        AddAbstractMetaVisitor visitor( l );
        visitor.visit( inst );
    }

} // namespace lart::abstract
