#include <brick-string>

DIVINE_RELAX_WARNINGS
#include <brick-llvm-link>
DIVINE_UNRELAX_WARNINGS

int link_a( int argc, const char **argv )
{
    llvm::LLVMContext ctx;
    std::vector< std::unique_ptr< llvm::Module > > modules;

    modules.emplace_back( std::make_unique< llvm::Module >( "_link_essentials", ctx ) );
    llvm::Module &essentials = *modules.back();

    for ( int i = 2; i < argc; ++i )
    {
        auto input = std::move( llvm::MemoryBuffer::getFile( argv[i] ).get() );
        auto mod = llvm::parseBitcodeFile( input->getMemBufferRef(), ctx );
        if ( !mod )
            return 1;
        modules.emplace_back( std::move( mod.get() ) );
        llvm::Module &m = *modules.back();
        essentials.setDataLayout( m.getDataLayoutStr() );
        brick::llvm::enumerateFunctionsForAnno( "divine.link.always", m,
            [&]( llvm::Function *f ) {
                essentials.getOrInsertFunction( f->getName(),
                                                f->getFunctionType(),
                                                f->getAttributes() );
            } );
    }

    brick::llvm::writeArchive( modules.begin(), modules.end(), argv[1] );
    return 0;
}

int link_bc( int argc, const char **argv )
{
    auto ctx = std::make_shared< llvm::LLVMContext >();
    brick::llvm::Linker linker( ctx );

    for ( int i = 2; i < argc; ++i )
    {
        auto input = std::move( llvm::MemoryBuffer::getFile( argv[i] ).get() );
        if ( brq::ends_with( argv[i], ".bc" ) )
        {
            auto bc = std::move( llvm::parseBitcodeFile( input->getMemBufferRef(), *ctx ).get() );
            linker.link( std::move( bc ) );
        }

        if ( brq::ends_with( argv[i], ".a" ) )
        {
            auto archive = brick::llvm::ArchiveReader( std::move( input ), ctx );
            for ( auto &m : archive.modules() )
                if ( m.getModuleIdentifier() == "_link_essentials" )
                    linker.link( m ); /* TODO: link_decls here */
            linker.linkArchive( archive );
        }
    }

    auto mod = linker.take();
    brick::llvm::writeModule( mod.get(), argv[1] );
    return 0;
}

/* usage: runtime-ld output.a source1.bc [...] */
int main( int argc, const char **argv )
{
    std::string out = argv[1];

    if ( brq::ends_with( out, ".bc" ) )
        return link_bc( argc, argv );

    if ( brq::ends_with( out, ".a" ) )
        return link_a( argc, argv );

    return 1;
}
