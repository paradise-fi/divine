#include <divine/cc/clang.hpp>
#include <llvm/Bitcode/ReaderWriter.h>
#include <iostream>

using namespace divine;

/* usage: runtime-cc srcdir bindir source.c output.bc [flags] */
int main( int argc, const char **argv )
{
    try {
        cc::Compiler clang;
        clang.allowIncludePath( argv[1] );
        clang.allowIncludePath( argv[2] );
        std::vector< std::string > opts;
        std::copy( argv + 5, argv + argc, std::back_inserter( opts ) );
        auto mod = clang.compileModule( argv[3], opts );
        std::error_code err;
        llvm::raw_fd_ostream outs( argv[4], err, llvm::sys::fs::F_None );
        llvm::WriteBitcodeToFile( mod.get(), outs );
        return 0;
    } catch ( cc::CompileError &err ) {
        std::cerr << err.what() << std::endl;
        return 1;
    }
}
