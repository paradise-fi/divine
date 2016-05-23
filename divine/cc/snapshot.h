// -*- C++ -*- (c) 2015 Jiří Weiser

#include <fstream>
#include <cctype>
#include <algorithm>
#include <iterator>

#include <brick-fs.h>

#ifndef TOOLS_COMPILE_SNAPSHOT_H
#define TOOLS_COMPILE_SNAPSHOT_H

namespace divine {
namespace compile {

struct Snapshot {

    static void writeFile( std::ostream &file, const std::string &dir, const std::string &_stdin )
    {
        file << "#include <fs-manager.h>\n"
             << "namespace divine{ namespace fs {\n"
             << "VFS vfs{\n";

        if ( !_stdin.empty() ) {
            std::ifstream in( _stdin, std::ios::binary );
            in >> std::noskipws;
            std::string content = brick::fs::readFile( in );
            file << '"' << _stringify( content ) << "\"," << content.size() << ",{\n";
        }

        if ( !dir.empty() ) {
            brick::fs::traverseDirectoryTree( dir,
                [&]( std::string path ) {
                    auto shrinked = brick::fs::distinctPaths( dir, path );
                    if ( !shrinked.empty() ) {
                        auto st = brick::fs::stat( path );
                        file << "{\"" << _stringify( shrinked ) << "\", Type::Directory, " << st->st_mode << ", nullptr, 0 },\n";
                    }
                    return true;
                },
                []( std::string ){},
                [&]( std::string path ) {
                    auto st = brick::fs::lstat( path );
                    const char *type = _resolveType( st->st_mode );
                    file << "{\"" << _stringify( brick::fs::distinctPaths( dir, path ) ) << "\", Type::" << type << ", "<< st->st_mode << ", ";

                    if ( _file == type ) {
                        std::ifstream in( path, std::ios::binary );
                        in >> std::noskipws;
                        std::string content = brick::fs::readFile( in );
                        file << '"' << _stringify( content ) << "\"," << content.size();
                    }
                    else if ( _pipe == type ) {
                        file << "\"\", 0";
                    }
                    else if ( _symLink == type ) {
                        std::string link( st->st_size, '-' );
                        ::readlink( path.c_str(), &link.front(), st->st_size );
                        file << "\"" << _stringify( link ) << "\", " << st->st_size;
                    }
                    else
                        file << "nullptr, 0";
                    file << "},\n";
                }
            );
        }
        file << "{nullptr, Type::Nothing, 0, nullptr, 0}";
        if ( !_stdin.empty() )
            file << "}";
        file << "};}}" << std::endl;
    }
private:

    static constexpr const char *_file = "File";
    static constexpr const char *_symLink = "SymLink";
    static constexpr const char *_pipe = "Pipe";

    static const char *_resolveType( unsigned mode ) {
        if ( ( mode & S_IFLNK ) == S_IFLNK )
            return _symLink;
        if ( ( mode & S_IFREG ) == S_IFREG )
            return _file;
        if ( ( mode & S_IFIFO ) == S_IFIFO )
            return _pipe;
        return nullptr;
    }

    using uchar = unsigned char;

    static std::string _encode( uchar c ) {
        std::string result = "\\x";

        if ( ( (c >> 4) & 15 ) < 10 )
            result += ( (c >> 4) & 15 ) + '0';
        else
            result += ( (c >> 4) & 15 ) - 10 + 'a';

        if ( ( c & 15 ) < 10 )
            result += ( c & 15 ) + '0';
        else
            result += ( c & 15 ) - 10 + 'a';

        return result;
    }

    static std::string _stringify( const std::string &str ) {
        std::string result;
        for ( char c : str )
            result += _encode( uchar( c ) );
        return result;
    }

};

} // namespace compile
} // namespace divine

#endif
