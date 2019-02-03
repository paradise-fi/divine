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
    content
};

namespace {
    using DomainData = std::tuple< std::string >;

    using brick::data::Bimap;
} // anonymous namespace


struct Domain : DomainData {
    using DomainData::DomainData;

    inline std::string name() const noexcept { return std::get< std::string >( *this ); }

    inline auto meta( llvm::LLVMContext & ctx ) const noexcept {
        return meta::value::create( ctx, name() );
    }

    static Domain Concrete() { return Domain( "concrete" ); }
    static Domain Tristate() { return Domain( "tristate" ); }
    static Domain Unknown() { return Domain( "unknown" ); }
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


struct ValueMetadata {
    ValueMetadata( llvm::Metadata * md )
        : _md( llvm::cast< llvm::ValueAsMetadata >( md ) )
    {}

    ValueMetadata( llvm::Value * v )
        : _md( llvm::LocalAsMetadata::get( v ) )
    {}

    std::string name() const noexcept;
    llvm::Value * value() const noexcept;
    Domain domain() const noexcept;
private:
    llvm::ValueAsMetadata  *_md;
};


struct CreateAbstractMetadata {
    void run( llvm::Module &m );
};

struct ArgMetadata {
    ArgMetadata( llvm::Metadata *data )
        : data( llvm::cast< llvm::MDNode >( data ) )
    {}

    Domain domain() const;
private:
    llvm::MDNode *data;
};


struct FunctionMetadata {
    FunctionMetadata( llvm::Function * fn )
        : fn( fn )
    {}

    void set_arg_domain( unsigned i, Domain dom );
    Domain get_arg_domain( unsigned i ) const;

    bool has_arg_domain( unsigned i ) const;

    void clear();
private:

    static constexpr char meta[] = "lart.function.meta";

    llvm::Function *fn;
};

void make_duals( llvm::Instruction *a, llvm::Instruction *b );
llvm::Value* get_dual( llvm::Instruction *i );

std::vector< ValueMetadata > abstract_metadata( llvm::Module &m );
std::vector< ValueMetadata > abstract_metadata( llvm::Function *fn );

bool has_abstract_metadata( llvm::Value *val );
bool has_abstract_metadata( llvm::Argument *arg );
bool has_abstract_metadata( llvm::Instruction *inst );

llvm::MDNode * get_abstract_metadata( llvm::Instruction *inst );

void add_abstract_metadata( llvm::Instruction *inst, Domain dom );

inline Domain get_domain( llvm::Argument *arg ) {
    auto fmd = FunctionMetadata( arg->getParent() );
    return fmd.get_arg_domain( arg->getArgNo() );
}

inline Domain get_domain( llvm::Instruction *inst ) {
    return ValueMetadata( inst ).domain();
}

inline Domain get_domain( llvm::Value *val ) {
    if ( llvm::isa< llvm::Constant >( val ) )
        return Domain::Concrete();
    else if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
        return get_domain( arg );
    else
        return get_domain( llvm::cast< llvm::Instruction >( val ) );
}

inline bool is_concrete( Domain dom ) {
    return dom == Domain::Concrete();
}

inline bool is_concrete( llvm::Value *val ) {
    return is_concrete( get_domain( val ) );
}

bool forbidden_propagation_by_domain( llvm::Instruction * inst, Domain dom );

bool is_duplicable( llvm::Instruction *inst );
bool is_duplicable_in_domain( llvm::Instruction *inst, Domain dom );

bool is_propagable_in_domain( llvm::Instruction *inst, Domain dom );

bool is_transformable( llvm::Instruction *inst );
bool is_transformable_in_domain( llvm::Instruction *inst, Domain dom );

bool is_base_type( llvm::Module *m, llvm::Value *val );
bool is_base_type_in_domain( llvm::Module *m, llvm::Value *val, Domain dom );

template< typename Yield >
auto global_variable_walker( llvm::Module &m, Yield yield ) {
    brick::llvm::enumerateAnnosInNs< llvm::GlobalVariable >( meta::tag::domain::name, m, yield );
}

DomainMetadata domain_metadata( llvm::Module &m, Domain dom );

} // namespace lart::abstract
