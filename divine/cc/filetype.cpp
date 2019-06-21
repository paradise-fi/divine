/*
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <divine/cc/filetype.hpp>

#include <brick-fs>

namespace divine::cc
{
    FileType typeFromFile( std::string name )
    {
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

    FileType typeFromXOpt( std::string selector )
    {
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

    std::vector< std::string > argsOfType( FileType t )
    {
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

    bool is_type( std::string file, FileType type )
    {
        return cc::typeFromFile( file ) == type;
    }

    bool is_object_type( std::string file )
    {
        return is_type( file, FileType::Obj ) || is_type( file, FileType::Archive );
    }

}
