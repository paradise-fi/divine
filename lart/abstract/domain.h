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

    namespace {
        using DomainData = std::tuple< std::string >;

        using brick::data::Bimap;
    } // anonymous namespace


    enum class DomainKind : uint8_t {
        scalar,
        pointer,
        content
    };


    struct Domain : DomainData {
        using DomainData::DomainData;

        inline std::string name() const noexcept { return std::get< std::string >( *this ); }

        inline auto meta( llvm::LLVMContext & ctx ) const noexcept {
            return meta::create( ctx, name() );
        }

        static inline Domain Concrete() { return Domain( "concrete" ); }
        static inline Domain Tristate() { return Domain( "tristate" ); }
        static inline Domain Unknown() { return Domain( "unknown" ); }

        static Domain get( llvm::Value * val ) noexcept {
            if ( auto str = meta::abstract::get( val ) ) {
                return Domain{ str.value() };
            }
            return Domain::Concrete();
        }

        static void set( llvm::Value * val, const Domain & dom ) {
            meta::abstract::set( val, dom.name() );
        }
    };


    struct DomainMetadata {
        DomainMetadata( llvm::GlobalVariable * glob )
            : glob( glob )
        {}

        inline bool scalar() const noexcept { return kind() == DomainKind::scalar; }
        inline bool pointer() const noexcept { return kind() == DomainKind::pointer; }
        inline bool content() const noexcept { return kind() == DomainKind::content; }

        Domain domain() const;
        DomainKind kind() const;
        llvm::Type * base_type() const;
        llvm::Value * default_value() const;

    private:
        static constexpr size_t base_type_offset = 0;

        llvm::GlobalVariable * glob;
    };


    struct CreateAbstractMetadata {
        void run( llvm::Module &m );
    };


    inline bool is_concrete( Domain dom ) {
        return dom == Domain::Concrete();
    }

    inline bool is_concrete( llvm::Value *val ) {
        return is_concrete( Domain::get( val ) );
    }

    bool forbidden_propagation_by_domain( llvm::Instruction * inst, Domain dom );

    bool is_duplicable( llvm::Instruction *inst );
    bool is_duplicable_in_domain( llvm::Instruction *inst, Domain dom );

    bool is_propagable_in_domain( llvm::Instruction *inst, Domain dom );

    bool is_transformable( llvm::Instruction *inst );
    bool is_transformable_in_domain( llvm::Instruction *inst, Domain dom );

    bool is_base_type( llvm::Module *m, llvm::Value *val );
    bool is_base_type_in_domain( llvm::Module *m, llvm::Value *val, Domain dom );

    std::vector< DomainMetadata > domains( llvm::Module & m );

    DomainMetadata domain_metadata( llvm::Module &m, const Domain & dom );

} // namespace lart::abstract
