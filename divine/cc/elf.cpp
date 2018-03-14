// -*- C++ -*- (c) 2016 Vladimír Štill

#include <divine/cc/elf.hpp>

#include <brick-fs>
#include <brick-proc>
DIVINE_RELAX_WARNINGS
#include <brick-llvm>
#include <brick-elf>
DIVINE_UNRELAX_WARNINGS
#include <brick-query>
#include <brick-string>

#include <fstream>
#include <streambuf>
#include <string>
#include <regex>

using namespace std::literals;

namespace divine {
namespace cc {
namespace elf {

static const std::string bcsec = "divine_bc";

using StringVec = std::vector< std::string >;
using Section = brick::elf::Section;

static StringVec defaultLibPath() {
    return { }; // TODO: detect from executable path
}

void addSection( std::string filepath, std::string sectionName, const std::string &sectionData )
{
    brick::fs::TempDir workdir( ".divine.addSection.XXXXXX", brick::fs::AutoDelete::Yes,
                                brick::fs::UseSystemTemp::Yes );
    auto secpath = brick::fs::joinPath( workdir, "sec" );
    std::ofstream secf( secpath, std::ios_base::out | std::ios_base::binary );
    secf << sectionData;
    secf.close();

    auto r = brick::proc::spawnAndWait( brick::proc::CaptureStderr, "objcopy",
                                  "--remove-section", sectionName, // objcopy can't override section
                                  "--add-section", sectionName + "=" +  secpath,
                                  "--set-section-flags", sectionName + "=noload,readonly",
                                  filepath );
    if ( !r )
        throw ElfError( "could not add section " + sectionName + " to " + filepath
                        + ", objcopy exited with " + to_string( r ) );
}

std::vector< std::string > getSections( std::string filepath, std::string sectionName ) {
    brick::elf::ElfObject elf( filepath );
    auto secs = elf.sectionsByName( sectionName );
    return std::vector< std::string >( secs.begin(), secs.end() );
}

static StringVec extractOpts( StringVec flagNames, StringVec args )
{
    StringVec out;
    for ( auto it = args.begin(), end = args.end(); it != end; ++it ) {
        for ( const auto &f : flagNames )
        if ( it->substr( 0, f.size() ) == f ) {
            if ( it->size() == f.size() )
                out.push_back( *++it );
            else
                out.push_back( it->substr( f.size() ) );
        }
    }
    return out;
}

static StringVec addLibsFromArgs( StringVec origSources, StringVec args )
{
    auto lpath = extractOpts( { "-L", "--library-path=" }, args );
    auto libs = extractOpts( { "-l", "--libaray=" }, args );
    auto dlpath = defaultLibPath();
    std::copy( dlpath.begin(), dlpath.end(), std::back_inserter( lpath ) );
    std::transform( libs.begin(), libs.end(), std::back_inserter( origSources ),
            [&]( std::string lib ) {
                for ( auto p : lpath ) {
                    for ( auto suf : { ".o", ".a", ".so" } ) {
                        auto libp = brick::fs::joinPath( p, "lib" + lib + suf );
                        if ( brick::fs::access( libp, F_OK ) )
                            return libp;
                    }
                }
                throw ElfError( "could not find library " + lib );
            } );
    return origSources;
}

std::vector< std::unique_ptr< llvm::Module > > extractModules( std::string file, llvm::LLVMContext &ctx )
{
    if ( file.substr( file.size() - 2 ) == ".a" ) {
        brick::fs::TempDir workdir( ".divine.archive.XXXXXX",
                                    brick::fs::AutoDelete::Yes,
                                    brick::fs::UseSystemTemp::Yes );
        brick::fs::ChangeCwd cwd( workdir );
        if ( brick::fs::isRelative( file ) )
            file = brick::fs::joinPath( cwd.oldcwd, file );
        auto r = brick::proc::spawnAndWait( "ar", "x", file );
        if ( !r )
            throw ElfError( "ar failed " + to_string( r ) );

        std::vector< std::unique_ptr< llvm::Module > > modules;
        brick::fs::traverseFiles( workdir, [&]( std::string f ) {
                    auto bcs = extractModules( f, ctx );
                    std::move( bcs.begin(), bcs.end(), std::back_inserter( modules ) );
                } );
        return modules;
    }

    auto secs = getSections( file, bcsec );
    std::vector< std::unique_ptr< llvm::Module > > modules;
    std::transform( secs.begin(), secs.end(), std::back_inserter( modules ),
        [&]( const std::string &section ) {
            auto input = llvm::MemoryBuffer::getMemBuffer( section );
            auto inputData = input->getMemBufferRef();
            auto parsed = parseBitcodeFile( inputData, ctx );
            if ( !parsed )
                throw ElfError( "Error parsing bitcode from ELF section; probably not an object produced by divine.cc" );
            return std::move( parsed.get() );
        } );
    return modules;
}

void linkObjects( std::string output, StringVec args )
{
    StringVec ldargs = { "ld", "-o", output, "--unique=" + bcsec };
    std::copy( args.begin(), args.end(), std::back_inserter( ldargs ) );

    auto r = brick::proc::spawnAndWait( brick::proc::ShowCmd, ldargs );
    if ( !r )
        throw ElfError( "could not link object files, ld returned " + to_string( r ) );

    {
        brick::elf::ElfObject eo( output );
        for ( int i = 0, end = eo.sectionsCount(); i != end; ++i ) {
            std::cout << "section " << i << ": " << eo.sectionName( i )
                      << " " << eo.sectionSize( i ) << std::endl;
        }
    }

    // extract bitcode for linked object files and link it with LLVM linker
    brick::llvm::Linker bcl;
    llvm::LLVMContext ctx;

    // try to link in the same order as LD
    for ( auto &m : brick::query::reversed( extractModules( output, ctx ) ) )
        bcl.link( std::move( m ) );

    addSection( output, bcsec, Compiler::serializeModule( *bcl.get() ) );
}

void compile( Compiler &cc, std::string path, std::string output, StringVec args ) {
    auto m = cc.compileModule( path, args );
    cc.emitObjFile( *m, output, args );
    addSection( output, bcsec, Compiler::serializeModule( *m ) );
}

struct SysPaths {

    SysPaths() {
        for ( auto cc : { "cc", "gcc", "clang" } ) {
            try {
                auto r = spawnAndWait( brick::proc::CaptureStdout, cc, "-print-search-dirs" );
                if ( !r || r.out().empty() )
                    continue; // some error in CC
                std::stringstream ss( r.out() );
                std::string line;
                while ( getline( ss, line ) ) {
                    if ( brick::string::startsWith( line, "libraries:" ) )
                        _libDirs = parsePaths( line );
                    else if ( brick::string::startsWith( line, "programs:" ) )
                        _binDirs = parsePaths( line );
                }
                return;
            } catch ( brick::proc::ProcError & ) {
                // scan next
            }
        }
    }

    const StringVec &libDirs() const { return _libDirs; }
    const StringVec &binDirs() const { return _binDirs; }

  private:
    static StringVec parsePaths( std::string s ) {
        std::vector< std::string > out;
        std::regex sep(":");
        std::sregex_token_iterator it( s.begin(), s.end(), sep, -1 );
        // first is line type
        std::copy( std::next( it ), std::sregex_token_iterator(), std::back_inserter( out ) );
        for ( auto &p : out )
            while ( p[0] == ' ' || p[0] == '=' )
                p.erase( p.begin() );
        return out;
    }

    StringVec _binDirs = { "/usr/bin", "/bin" };
    StringVec _libDirs = { "/usr/lib", "/usr/lib64", "/lib", "/lib64" };
};

static SysPaths &syspaths() {
    static SysPaths sp;
    return sp;
}

std::string find( StringVec paths, StringVec alternatives ) {
    for ( auto a : alternatives ) {
        for ( auto p : paths ) {
            auto path = brick::fs::joinPath( p, a );
            if ( brick::fs::exists( path ) )
                return path;
        }
    }
    std::string msg = "Error: could not find any of: ";
    for ( auto a : alternatives )
        msg += "'"s + a + "', "s;
    msg.pop_back();
    msg.back() = '.';
    throw std::runtime_error( msg );
}

std::string findlib( std::string name ) {
    return find( syspaths().libDirs(), { "lib"s + name + ".so"s, "lib"s + name + ".a" } );
}

std::string findcrt( std::string name ) {
    return find( syspaths().libDirs(), { name + ".o"s } );
}

std::tuple< StringVec, StringVec > getExecLinkOptions( StringVec divineSearchPaths )
{
    StringVec ldopts =
            { "--eh-frame-hdr", "-m", "elf_x86_64",
              "-dynamic-linker", "/lib64/ld-linux-x86-64.so.2",
              findcrt( "crt1" ),
              findcrt( "crti" ),
              findcrt( "crtbegin" )
            };
    for ( auto ds : { divineSearchPaths, syspaths().libDirs() } )
        for ( auto d : ds )
            ldopts.push_back( "-L"s + d );

    return std::tuple< StringVec, StringVec > {
            // at the beginning
            ldopts,
            // at the end
            { "-lgcc",
              "--as-needed",
              "-lgcc_s",
              "--no-as-needed",
              "-lc",
              "-lgcc",
              "--as-needed",
              "-lgcc_s",
              "--no-as-needed",
              findcrt( "crtend" ),
              findcrt( "crtn" )
            }
        };
}

}
}
}
