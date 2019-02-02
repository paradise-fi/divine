// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/cleanup.h>

#include <lart/abstract/domain.h>

#include <lart/support/cleanup.h>
#include <lart/support/query.h>

namespace lart {
namespace abstract {

void Cleanup::run( llvm::Module &m ) {
    std::map< Domain, llvm::Function * > cleanups;

    global_variable_walker( m, [&] ( const auto& glob, const auto& ) {
        auto dom = DomainMetadata( glob ).domain();

        auto name = "__" + dom.name() + "_cleanup";
        auto cleanup_function = m.getFunction( name );

        if ( !cleanup_function ) {
            throw std::runtime_error( "missing function in domain: " + name );
        }

        cleanups.emplace( dom, cleanup_function );
    } );

    for ( auto &fn : m ) {
        if ( fn.getMetadata( meta::tag::roots ) ) {
            auto domains = query::query( abstract_metadata( &fn ) )
                .map( [] ( auto & mdv ) { return mdv.domain(); } )
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
