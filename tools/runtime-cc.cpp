#include <divine/cc/cc1.hpp>
#include <divine/rt/paths.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/Bitcode/BitcodeWriter.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/divine/debugpaths.hpp>
#include <brick-string>
#include <brick-fs>
#include <brick-llvm>
#include <brick-sha2>
#include <iostream>

using namespace divine;
using namespace brick;

#define VERSION "4" /* bump this to force rebuilds despite .fp matches */

/* usage: runtime-cc srcdir bindir source.c output.bc [flags] */
int main( int argc, const char **argv )
{
    try {
        cc::CC1 clang;
        std::string srcDir = argv[1], binDir = argv[2];
        clang.allowIncludePath( srcDir );
        clang.allowIncludePath( binDir );
        std::vector< std::string > opts;
        std::copy( argv + 5, argv + argc, std::back_inserter( opts ) );

        /* Compile only if SHA-2 hash of preprocessed file differs */
        auto prec = clang.preprocess( argv[3], opts );
        std::string fpFilename = std::string( argv[ 4 ] ) + ".fp";
        std::string oldFp = brq::read_file_or( fpFilename, {} );
        std::string newFp = VERSION ":" + sha2::to_hex( sha2_512( prec ) );
        if ( oldFp == newFp && brq::file_exists( argv[ 4 ] ) )
        {
            brq::touch( argv[ 4 ] );
            return 0;
        }
        auto mod = clang.compile( argv[3], opts );

        TRACE( "bindir =", binDir, "srcdir =", srcDir );
        lart::divine::rewriteDebugPaths( *mod, [=]( auto p )
        {
            auto n = p;
            if ( brq::starts_with( p, srcDir ) )
                n = p.substr( srcDir.size() );
            if ( brq::starts_with( p, binDir ) )
                n = p.substr( binDir.size() );
            TRACE( "rewrite", p, "to", n );
            return n;
        } );

        brick::llvm::writeModule( mod.get(), argv[4] );
        brq::write_file( fpFilename, newFp );

        return 0;
    } catch ( cc::CompileError &err ) {
        std::cerr << err.what() << std::endl;
        return 1;
    }
}
