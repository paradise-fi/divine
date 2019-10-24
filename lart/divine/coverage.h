// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/CallSite.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/support/pass.h>
#include <lart/support/meta.h>

namespace lart::divine
{

struct Coverage {

    static PassMeta meta() {
        return passMeta< Coverage >( "coverage", "" );
    }


    template< typename Base >
    using LLVMUtil = abstract::LLVMUtil< Base >;

    struct IndicesBuilder : LLVMUtil< IndicesBuilder >
    {
        using indices_t = std::vector< llvm::Value * >;

        IndicesBuilder( llvm::Module * m ) : module( m ) {}

        indices_t create( int total, int index )
        {
            indices_t indices;
            indices.push_back( i32( total ) );
            indices.push_back( i32( index ) );
            for ( int i = 0; i < total - 1; ++i )
                indices.push_back( i32( 0 ) );
            return indices;
        }

        llvm::Module  * module;
    };

    llvm::Function * _choose;
    std::vector< llvm::CallSite > _chooses;

    void run( llvm::Module &m );

    void assign_indices( IndicesBuilder &ib );
};

} // namespace lart::divine