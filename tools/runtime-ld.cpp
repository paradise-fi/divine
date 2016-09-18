DIVINE_RELAX_WARNINGS
#include <brick-llvm>
DIVINE_UNRELAX_WARNINGS

/* usage: runtime-cc source.c output.bc [flags] */
int main( int argc, const char **argv )
{
    brick::llvm::Linker ld;
    llvm::LLVMContext ctx;

    for ( int i = 2; i < argc; ++i )
    {
        auto input = std::move( llvm::MemoryBuffer::getFile( argv[i] ).get() );
        auto parsed = llvm::parseBitcodeFile( input->getMemBufferRef(), ctx );
        ld.link( std::move( parsed.get() ) );
    }

    brick::llvm::writeModule( ld.get(), argv[1] );
    return 0;
}
