// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/cleanup.h>

#include <lart/abstract/domain.h>

#include <lart/support/cleanup.h>
#include <lart/support/query.h>

namespace lart {
namespace abstract {

void Cleanup::run( llvm::Module &m ) {
    std::map< Domain, llvm::Function * > cleanups;

    for ( const auto & meta : domains( m ) ) {
        auto name = "__" + meta.domain().name() + "_cleanup";
        auto cleanup_function = m.getFunction( name );

        if ( !cleanup_function ) {
            throw std::runtime_error( "missing function in domain: " + name );
        }

        cleanups.emplace( meta.domain(), cleanup_function );
    }

    for ( auto &fn : m ) {
        if ( meta::abstract::roots( &fn ) ) {
            auto domains = query::query( meta::enumerate( fn ) )
                .map( Domain::get )
                .freeze();

            auto it = std::unique( domains.begin(), domains.end() );
            domains.resize( std::distance( domains.begin(), it ) );

            cleanup::atExits( fn, [&] ( const auto& exit ) {
                llvm::IRBuilder<> irb(exit);
                for (const auto& dom : domains) {
                    irb.CreateCall( cleanups.at( dom ) );
                }
            } );
        }
    }
}

} // namespace abstract
} // namespace lart
