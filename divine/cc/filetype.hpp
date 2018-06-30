#pragma once

#include <string>
#include <vector>

namespace divine {
namespace cc {

enum class FileType
{
	Unknown, C, Cpp, CPrepocessed, CppPreprocessed, IR, BC, Asm, Obj, Archive
};

FileType typeFromFile( std::string name );
FileType typeFromXOpt( std::string selector );
std::vector< std::string > argsOfType( FileType t );


template< typename A, typename B = std::initializer_list< std::decay_t<
                        decltype( *std::declval< A & >().begin() ) > > >
static void add( A &a, B b ) {
    std::copy( b.begin(), b.end(), std::back_inserter( a ) );
}

} // cc
} // divine
