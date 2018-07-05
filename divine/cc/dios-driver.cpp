#include <divine/cc/driver.hpp>

namespace divine {
namespace cc {

void DiosDriver::linkEssentials()
{
    using namespace std::literals;

    for (auto arch : { "dios", "rst" } )
        linkEntireArchive( arch );
    // the _link_essentials modules are built from divine.link.always annotations
    for ( auto e : defaultDIVINELibs )
    {
        auto archive = getLib( e );
        auto modules = archive.modules();
        for ( auto it = modules.begin(); it != modules.end(); ++it )
            if ( it->getModuleIdentifier() == "_link_essentials.ll"s )
                linker->link_decls( it.take() );
    }
}

DiosDriver::DiosDriver( Options opts, std::shared_ptr< llvm::LLVMContext > ctx ) :
    Driver( opts, ctx )
{
    using brick::fs::joinPath;

    commonFlags.insert( commonFlags.end(),
                 { "-isystem", joinPath( includeDir, "libcxx/include" )
                 , "-isystem", joinPath( includeDir, "libcxxabi/include" )
                 , "-isystem", joinPath( includeDir, "libunwind/include" )
                 , "-isystem", includeDir
                 , "-D_POSIX_C_SOURCE=2008098L"
                 , "-D_LITTLE_ENDIAN=1234"
                 , "-D_BYTE_ORDER=1234"
                 } );
}

void DiosDriver::runCC( std::vector< std::string > rawCCOpts,
                        std::function< ModulePtr( ModulePtr &&, std::string ) > moduleCallback )
{
    Driver::runCC( rawCCOpts, moduleCallback );
    if ( linker->hasModule() ) {
        linkEssentials();
        linkLibs( defaultDIVINELibs );
    }
}

const std::vector< std::string > DiosDriver::defaultDIVINELibs = { "cxx", "cxxabi", "c" };

} // cc
} // divine