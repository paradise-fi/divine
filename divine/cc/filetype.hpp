#pragma once

#include <string>
#include <vector>

#include <brick-types>

namespace divine {
namespace cc {

enum class FileType
{
    Unknown, C, Cpp, CPrepocessed, CppPreprocessed, IR, BC, Asm, Obj, Archive
};

struct File {
    using InPlace = brick::types::InPlace< File >;

    File() = default;
    explicit File( std::string name, FileType type = FileType::Unknown ) :
        name( std::move( name ) ), type( type )
    { }

    std::string name;
    FileType type = FileType::Unknown;
};

struct Lib {
    using InPlace = brick::types::InPlace< Lib >;

    Lib() = default;
    explicit Lib( std::string name ) : name( std::move( name ) ) { }
    std::string name;
};

using FileEntry = brick::types::Union< File, Lib >;

FileType typeFromFile( std::string name );
FileType typeFromXOpt( std::string selector );
std::vector< std::string > argsOfType( FileType t );
bool is_type( std::string file, FileType type );
bool is_object_type( std::string file );

template< typename A, typename B = std::initializer_list< std::decay_t<
                        decltype( *std::declval< A & >().begin() ) > > >
static void add( A &a, B b ) {
    std::copy( b.begin(), b.end(), std::back_inserter( a ) );
}

} // cc
} // divine
