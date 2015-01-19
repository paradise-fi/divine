#include "fs-utils.h"

#ifndef _FS_PATH_H_
#define _FS_PATH_H_

namespace divine {
namespace fs {
namespace path {

#if defined( __unix )
const char pathSeparators[] = { '/' };
#elif defined( _WIN32 )
const char pathSeparators[] = { '\\', '/' };
#endif

inline bool isPathSeparator( char c ) {
    for ( auto sep : pathSeparators )
        if ( sep == c )
            return true;
    return false;
}

inline std::pair< utils::String, utils::String > absolutePrefix( utils::String path ) {
#ifdef _WIN32 /* this must go before general case, because \ is prefix of \\ */
    if ( path.size() >= 3 && path[ 1 ] == ':' && isPathSeparator( path[ 2 ] ) )
        return std::make_pair( path.substr( 0, 3 ), path.substr( 3 ) );
    if ( path.size() >= 2 && isPathSeparator( path[ 0 ] ) && isPathSeparator( path[ 1 ] ) )
        return std::make_pair( path.substr( 0, 2 ), path.substr( 2 ) );
#endif
    // this is absolute path in both windows and unix
    if ( path.size() >= 1 && isPathSeparator( path[ 0 ] ) )
        return std::make_pair( path.substr( 0, 1 ), path.substr( 1 ) );
    return std::make_pair( utils::String(), path );
}

inline bool isAbsolute( utils::String path ) {
    return absolutePrefix( std::move( path ) ).first.size() != 0;
}

inline bool isRelative( utils::String path ) {
    return !isAbsolute( std::move( path ) );
}

inline std::pair< utils::String, utils::String > splitFileName( utils::String path ) {
    auto begin = path.rbegin();
    while ( isPathSeparator( *begin ) )
        ++begin;
    auto length = &*begin - &path.front() + 1;
    auto pos = std::find_if( begin, path.rend(), &isPathSeparator );
    if ( pos == path.rend() )
        return std::make_pair( utils::String(), path.substr( 0, length ) );
    auto count = &*pos - &path.front();
    length -= count + 1;
    return std::make_pair( path.substr( 0, count ), path.substr( count + 1, length ) );
}

inline utils::String joinPath( utils::Vector< utils::String > paths, bool normalize = false ) {
    if ( paths.empty() )
        return "";
    auto it = ++paths.begin();
    utils::String out = paths[0];

    for ( ; it != paths.end(); ++it ) {
        if ( isAbsolute( *it ) || out.empty() )
            out = *it;
        else if ( !out.empty() && isPathSeparator( out.back() ) )
            out += *it;
        else
            out += pathSeparators[0] + *it;
    }
    return out;
}

template< typename... FilePaths >
inline utils::String joinPath( FilePaths &&...paths ) {
    return joinPath( utils::Vector< utils::String >{ std::forward< FilePaths >( paths )... } );
}

template< typename Result = utils::Vector< utils::String > >
inline Result splitPath( utils::String path, bool normalize = false ) {
    Result out;
    auto last = path.begin();
    while ( true ) {
        auto next = std::find_if( last, path.end(), &isPathSeparator );
        if ( next == path.end() ) {
            utils::String s( last, next );
            if ( normalize && s == "." );
            else if ( normalize && s == ".." ) {
                if ( !out.empty() && out.back() != ".." )
                    out.pop_back();
                else
                    out.emplace_back( std::move( s ) );
            }
            else
                out.emplace_back( std::move( s ) );
            return out;
        }
        if ( last != next ) {
            utils::String s( last, next );
            if ( normalize && s == "." );
            else if ( normalize && s == ".." ) {
                if ( !out.empty() && out.back() != ".." )
                    out.pop_back();
                else
                    out.emplace_back( std::move( s ) );
            }
            else
                out.emplace_back( std::move( s ) );
        }
        last = ++next;
    }
}

inline utils::String normalize( utils::String path ) {
    auto abs = absolutePrefix( path );
    return joinPath( abs.first, joinPath( splitPath( abs.second, true ) ) );
}

} // namespace path
} // namespace fs
} // namespace divine

#endif
