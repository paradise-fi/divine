DIVINE_RELAX_WARNINGS
#include <brick-llvm>
DIVINE_UNRELAX_WARNINGS

/* usage: runtime-ld output.a source1.bc [...] */
int main( int argc, const char **argv )
{
    llvm::LLVMContext ctx;
    std::vector< std::unique_ptr< llvm::Module > > modules;

    for ( int i = 2; i < argc; ++i )
    {
        auto input = std::move( llvm::MemoryBuffer::getFile( argv[i] ).get() );
        modules.emplace_back( std::move(
                    llvm::parseBitcodeFile( input->getMemBufferRef(), ctx ).get() ) );
    }

    brick::llvm::writeArchive( modules.begin(), modules.end(), argv[1] );
    return 0;
}
