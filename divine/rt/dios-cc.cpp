#include <divine/rt/dios-cc.hpp>

namespace divine {
namespace rt {

using namespace cc;

void DiosCC::linkEssentials()
{
    using namespace std::literals;

    for ( auto arch : { "dios", "rst" } )
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

DiosCC::DiosCC( Options opts, std::shared_ptr< llvm::LLVMContext > ctx ) :
    Driver( opts, ctx )
{
    using brick::fs::joinPath;
    setupFS( rt::each );

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

void DiosCC::build( ParsedOpts po )
{
    Driver::build( po );
    if ( linker->hasModule() ) {
        linkEssentials();
        linkLibs( defaultDIVINELibs );
    }
}

const std::vector< std::string > DiosCC::defaultDIVINELibs = { "cxx", "cxxabi", "c" };

} // cc
} // divine
