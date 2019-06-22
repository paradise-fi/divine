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
    auto mod = load_object( "config/" + n );
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

DiosCC::DiosCC( Options opts, std::shared_ptr< llvm::LLVMContext > ctx ) :
    Driver( opts, ctx )
{
    using brick::fs::joinPath;
    setupFS( rt::each );

    add_dios_header_paths( commonFlags );
    commonFlags.insert( commonFlags.end(),
                 { "-D_LITTLE_ENDIAN=1234"
                 , "-D_BYTE_ORDER=1234"
                 , "-D__ELF__"
                 , "-D__unix__"
                 } );
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
    rt::add_dios_header_paths( _po.opts );
    divine::rt::each( [&]( auto path, auto c ) { _clang.mapVirtualFile( path, c ); } );
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
        _ld_args.push_back( "--driver-mode=g++" );
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
    return cc::link_bitcode< rt::DiosCC, true >( _files, _clang, _po.libSearchPath );
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
