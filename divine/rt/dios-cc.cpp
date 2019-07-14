#include <divine/cc/codegen.hpp>
#include <divine/rt/dios-cc.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-fs>

namespace divine {
namespace rt {

using namespace cc;

void DiosCC::link_dios_config( std::string n )
{
    linkEntireArchive( "rst" );
    auto mod = load_object( find_object( "config/" + n ) );
    link( std::move( mod ) );
    for ( int i = 0; i < 3; ++i )
    {
        linkLib( "dios" );
        linkLib( "dios_divm" );
        linkLibs( rt::DiosCC::defaultDIVINELibs );
    }
}

void add_dios_header_paths( std::vector< std::string >& paths )
{
    using namespace brick::fs;

    paths.insert( paths.end(),
                 { "-isystem", joinPath( includeDir, "libcxx/include" )
                 , "-isystem", joinPath( includeDir, "libcxxabi/include" )
                 , "-isystem", joinPath( includeDir, "libunwind/include" )
                 , "-isystem", includeDir } );
}

void add_dios_defines( std::vector< std::string >& flags )
{
    flags.insert( flags.end(),
                  { "-D_LITTLE_ENDIAN=1234"
                  , "-D_BYTE_ORDER=1234"
                  , "-D__ELF__"
                  , "-D__unix__"
                  });
}

DiosCC::DiosCC( Options opts, std::shared_ptr< llvm::LLVMContext > ctx ) :
    Driver( opts, ctx )
{
    using brick::fs::joinPath;
    setupFS( rt::each );

    add_dios_header_paths( commonFlags );
    add_dios_defines( commonFlags );
}

void DiosCC::build( ParsedOpts po )
{
    Driver::build( po );

    if ( linker->hasModule() )
        linkLibs( defaultDIVINELibs );
}

NativeDiosCC::NativeDiosCC( const std::vector< std::string >& opts )
  :  Native( opts )
{
    add_dios_header_paths( _po.opts );
    add_dios_defines( _po.opts );
    divine::rt::each( [&]( auto path, auto c ) { _clang.mapVirtualFile( path, c ); } );
    _missing_bc_fatal = true;
}

auto NativeDiosCC::link_dios_native( bool cxx )
{
    using namespace brick::fs;
    TempDir tmpdir( ".divcc.XXXXXX", AutoDelete::Yes, UseSystemTemp::Yes );
    auto hostlib = tmpdir.path + "/libdios-host.a",
         cxxlib  = tmpdir.path + "/libc++.a",
         cxxabi  = tmpdir.path + "/libc++abi.a";

    if ( cxx )
    {
        _ld_args.push_back( "-stdlib=libc++" );
        _ld_args.push_back( "-L" + tmpdir.path );
        writeFile( cxxlib, rt::libcxx() );
        writeFile( cxxabi, rt::libcxxabi() );
    }

    writeFile( hostlib, rt::dios_host() );
    _ld_args.push_back( hostlib );

    return tmpdir;
}

std::unique_ptr< llvm::Module > NativeDiosCC::link_bitcode()
{
    auto drv = std::make_unique< DiosCC >( _clang.context() );
    std::unique_ptr< llvm::Module > m = Native::do_link_bitcode< rt::DiosCC >();
    drv->link( std::move( m ) );
    if ( !_po.shared )
        drv->linkLibs( DiosCC::defaultDIVINELibs );

    m = drv->takeLinked();

    if ( !_po.shared )
    {
        for ( auto& func : *m )
            if ( func.isDeclaration() && !whitelisted( func ) )
                throw cc::CompileError( "Symbol undefined (function): " + func.getName().str() );

        for ( auto& val : m->globals() )
            if ( auto G = dyn_cast< llvm::GlobalVariable >( &val ) )
                if ( !G->hasInitializer() && !whitelisted( *G ) )
                    throw cc::CompileError( "Symbol undefined (global variable): " + G->getName().str() );
    }

    verifyModule( *m );
    return m;
}

void NativeDiosCC::link()
{
    init_ld_args();
    auto tmpdir = link_dios_native( _cxx );
    Native::link();
}

const std::vector< std::string > DiosCC::defaultDIVINELibs = { "c++", "c++abi", "pthread", "c" };

} // cc
} // divine
