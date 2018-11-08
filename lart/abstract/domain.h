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

namespace lart::abstract {

enum class DomainKind : uint8_t {
    scalar,
    pointer,
    string,
    custom
};

namespace {
    using DomainData = std::tuple< std::string >;

    using brick::data::Bimap;

} // anonymous namespace

static constexpr char abstract_tag[] = "lart.abstract";
static constexpr char abstract_domain_tag[]   = "lart.abstract.domain.tag";
static constexpr char abstract_domain_kind[]  = "lart.abstract.domain.kind";

struct Domain : DomainData {
    using DomainData::DomainData;

    inline std::string name() const noexcept { return std::get< std::string >( *this ); }

    static Domain Concrete() { return Domain( "concrete" ); }
    static Domain Tristate() { return Domain( "tristate" ); }
    static Domain Unknown() { return Domain( "unknown" ); }
};

struct DomainMetadata {

    DomainMetadata( llvm::GlobalVariable * glob )
        : glob( glob )
    {}

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

struct MetadataBuilder {
    MetadataBuilder( llvm::LLVMContext &ctx )
        : ctx( ctx )
    {}

    llvm::MDNode* domain_node( llvm::StringRef dom );
    llvm::MDNode* domain_node( Domain dom );
private:
    llvm::LLVMContext &ctx;
};

struct FunctionTag {
    static constexpr char ignore_ret[] = "lart.transform.ignore.ret";
    static constexpr char ignore_arg[] = "lart.transform.ignore.arg";
    static constexpr char forbidden[] = "lart.transform.forbidden";

    static inline bool ignore_call_of_function( llvm::Function * fn ) {
        return fn->getMetadata( ignore_arg );
    }

    static inline bool ignore_return_of_function( llvm::Function * fn ) {
        return fn->getMetadata( ignore_ret );
    }

    static inline bool forbidden_function( llvm::Function * fn ) {
        return fn->getMetadata( forbidden );
    }
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
    Domain get_arg_domain( unsigned i );

    void clear();
private:

    static constexpr char tag[] = "lart.function.domains";

    llvm::Function *fn;
};

void make_duals( llvm::Instruction *a, llvm::Instruction *b );
llvm::Value* get_dual( llvm::Instruction *i );

std::vector< ValueMetadata > abstract_metadata( llvm::Module &m );
std::vector< ValueMetadata > abstract_metadata( llvm::Function *fn );

bool has_abstract_metadata( llvm::Instruction *inst );

llvm::MDNode * get_abstract_metadata( llvm::Instruction *inst );

void add_abstract_metadata( llvm::Instruction *inst, Domain dom );



Domain get_domain( llvm::Type *type );

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


bool is_transformable_in_domain( llvm::Instruction *inst, Domain dom );

bool is_base_type( llvm::Value * val );
bool is_base_type_in_domain( llvm::Value * val, Domain dom );

template< typename Yield >
auto global_variable_walker( llvm::Module &m, Yield yield ) {
    brick::llvm::enumerateAnnosInNs< llvm::GlobalVariable >( abstract_domain_tag, m, yield );
}

DomainMetadata domain_metadata( llvm::Module &m, Domain dom );

} // namespace lart::abstract
