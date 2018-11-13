#include <divine/cc/filetype.hpp>

#include <brick-fs>

namespace divine {
namespace cc {

FileType typeFromFile( std::string name ) {
    auto ext = brick::fs::takeExtension( name );
    if ( ext == ".c" )
        return FileType::C;
    for ( auto &x : { ".cpp", ".cc", ".C", ".CPP", ".c++", ".cp", ".cxx" } )
        if ( ext == x )
            return FileType::Cpp;
    if ( ext == ".i" )
        return FileType::CPrepocessed;
    if ( ext == ".ii" )
        return FileType::CppPreprocessed;
    if ( ext == ".bc" )
        return FileType::BC;
    if ( ext == ".ll" )
        return FileType::IR;
    if ( ext == ".S" || ext == ".s" )
        return FileType::Asm;
    if ( ext == ".o" )
        return FileType::Obj;
    if ( ext == ".a" )
        return FileType::Archive;
    return FileType::Unknown;
}

FileType typeFromXOpt( std::string selector ) {
    if ( selector == "c++" )
        return FileType::Cpp;
    if ( selector == "c" )
        return FileType::C;
    if ( selector == "c++cpp-output" )
        return FileType::CppPreprocessed;
    if ( selector == "cpp-output" )
        return FileType::CPrepocessed;
    if ( selector == "ir" )
        return FileType::IR;
    return FileType::Unknown;
}

std::vector< std::string > argsOfType( FileType t ) {
    std::vector< std::string > out { "-x" };
    switch ( t )
    {
        case FileType::Cpp:
            add( out, { "c++" } );
            break;
        case FileType::C:
            add( out, { "c" } );
            break;
        case FileType::CppPreprocessed:
            add( out, { "c++cpp-output" } );
            break;
        case FileType::CPrepocessed:
            add( out, { "cpp-output" } );
            break;
        case FileType::BC:
        case FileType::IR:
            add( out, { "ir" } );
            break;
        case FileType::Asm:
            add( out, { "assembler-with-cpp" } );
            break;
        case FileType::Unknown:
        default:
            UNREACHABLE( "Unknown file type" );
    }
    return out;
}

} // cc
} // divine
