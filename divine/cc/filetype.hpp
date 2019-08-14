#pragma once

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

#include <string>
#include <vector>

#include <brick-types>

// Utilities used to detect a file type based on its filename or CLI xopt

namespace divine::cc
{
    enum class FileType
    {
        Unknown, C, Cpp, CPrepocessed, CppPreprocessed, IR, BC, Asm, Obj, Archive, Shared
    };

    struct File
    {
        using InPlace = brick::types::InPlace< File >;

        File() = default;
        explicit File( std::string name, FileType type = FileType::Unknown ) :
            name( std::move( name ) ), type( type )
        { }

        std::string name;
        FileType type = FileType::Unknown;
    };

    struct Lib
    {
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

    // Append contents of B to the end of A
    template< typename A, typename B = std::initializer_list< std::decay_t<
                            decltype( *std::declval< A & >().begin() ) > > >
    static void add( A &a, B b )
    {
        std::copy( b.begin(), b.end(), std::back_inserter( a ) );
    }
} // divine
