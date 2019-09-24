DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/abstract/domain.h>
#include <lart/driver.h>

#include <lart/divine/stubs.h>
#include <bricks/brick-bitlevel>

#include <sstream>

namespace lart::divine {

struct AbstractStubs
{
    std::string _domain;

    AbstractStubs( std::string opt )
    {
        if ( ! opt.empty() ) {
            std::stringstream ss( opt );
            std::getline( ss, _domain );
        } else {
            _domain = "unit";
        }
    }

    static PassMeta meta()
    {
        return passMetaO< AbstractStubs >(
                "AbstractStubs",
                "options: [<domain>]\n\n"
                "Stub empty declarations with abstract values." );
    }

    void run( llvm::Module &m )
    {
        auto &ctx = m.getContext();
        llvm::Function *aValCtor[4];
        std::string aValStem = "__" + _domain + "_val_i";

        int i = 4;
        while ( i --> 0 )
        {
            int width = 8 << i;
            aValCtor[ i ] = m.getFunction( aValStem + std::to_string( width ) );
            if ( !aValCtor[ i ] )
            aValCtor[i] = llvm::cast< llvm::Function >(
                    m.getOrInsertFunction( aValStem + std::to_string( width ),
                    llvm::IntegerType::get( ctx, width ) ) );
            ASSERT( aValCtor[ i ] );
            lart::abstract::meta::set( aValCtor[ i ], lart::abstract::meta::tag::abstract_return );
            lart::abstract::meta::set( aValCtor[ i ], lart::abstract::meta::tag::abstract );
        }

        long count = 0;
        for ( auto &fn : m ) {
            if ( !fn.isDeclaration() || fn.isIntrinsic() || fn.getName().startswith( "__vm_" ) ||
                    fn.getName().startswith( "__" + _domain ) )
                continue;
            auto retType = fn.getReturnType();
            if ( !retType->isIntegerTy() )
                continue;
            auto bb = llvm::BasicBlock::Create( ctx, "", &fn );
            llvm::IRBuilder<> irb( bb );
            auto call = irb.CreateCall( aValCtor[ brick::bitlevel::MSB( retType->getIntegerBitWidth() ) - 3 ] );
            irb.CreateRet( call );
            lart::abstract::meta::set( call, lart::abstract::meta::tag::abstract_return );
            lart::abstract::meta::abstract::set( call, to_string( lart::abstract::DomainKind::scalar ) );
            ++count;
            fn.setName( "__stub_" + fn.getName() );
        }
        lart::Driver::assertValid( &m );
        if ( count )
            std::cerr << "INFO: abstrubbed " << count << " declarations using " << _domain << std::endl;
    }
};

PassMeta abstractStubsPass() {
    return passMetaC< DropEmptyDecls, AbstractStubs >( "abstrubs",
            "options: [<domain>]\n\n"
            "Remove unused function declarations and abstract the rest.",

            []( PassVector &mgr, std::string opt ) {
                DropEmptyDecls::meta().create( mgr, "" );
                AbstractStubs::meta().create( mgr, opt );
            } );
}

} /* lart::divine */
