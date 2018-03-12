// -*- C++ -*- (c) 2015 Jiří Weiser
//             (c) 2014 Vladimír Štill
/*
 *  This file contains couple of (sometimes a little bit modified)
 *  functions from bricks/brick-fs.h.
 */
#include "fs-utils.h"

#ifndef _FS_PATH_H_
#define _FS_PATH_H_

namespace __dios {
namespace fs {
namespace path {

#if defined( _WIN32 )
        const char pathSeparators[] = { '\\', '/' };
#elif defined( __MAC_OS_X_VERSION_MAX_ALLOWED )
        const char pathSeparators[] = { '/' };
#else
        const char pathSeparators[] = { '/' };
#endif

inline bool isPathSeparator( char c ) {
    for ( auto sep : pathSeparators )
        if ( sep == c )
            return true;
    return false;
}

inline std::pair< __dios::String, __dios::String > absolutePrefix( __dios::String path ) {
#ifdef _WIN32 /* this must go before general case, because \ is prefix of \\ */
    if ( path.size() >= 3 && path[ 1 ] == ':' && isPathSeparator( path[ 2 ] ) )
        return std::make_pair( path.substr( 0, 3 ), path.substr( 3 ) );
    if ( path.size() >= 2 && isPathSeparator( path[ 0 ] ) && isPathSeparator( path[ 1 ] ) )
        return std::make_pair( path.substr( 0, 2 ), path.substr( 2 ) );
#endif
    // this is absolute path in both windows and unix
    if ( path.size() >= 1 && isPathSeparator( path[ 0 ] ) )
        return std::make_pair( path.substr( 0, 1 ), path.substr( 1 ) );
    return std::make_pair( __dios::String(), path );
}

inline bool isAbsolute( __dios::String path ) {
    return absolutePrefix( std::move( path ) ).first.size() != 0;
}

inline bool isRelative( __dios::String path ) {
    return !isAbsolute( std::move( path ) );
}

inline std::pair< __dios::String, __dios::String > splitFileName( __dios::String path ) {
    auto begin = path.rbegin();
    while ( isPathSeparator( *begin ) )
        ++begin;
    auto length = &*begin - &path.front() + 1;
    auto pos = std::find_if( begin, path.rend(), &isPathSeparator );
    if ( pos == path.rend() )
        return std::make_pair( __dios::String(), path.substr( 0, length ) );
    auto count = &*pos - &path.front();
    length -= count + 1;
    return std::make_pair( path.substr( 0, count ), path.substr( count + 1, length ) );
}

inline __dios::String joinPath( __dios::Vector< __dios::String > paths, bool /*normalize*/ = false ) {
    if ( paths.empty() )
        return "";
    auto it = ++paths.begin();
    __dios::String out = paths[0];

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
inline __dios::String joinPath( FilePaths &&...paths ) {
    return joinPath( __dios::Vector< __dios::String >{ std::forward< FilePaths >( paths )... } );
}

template< typename Result = __dios::Vector< __dios::String > >
inline Result splitPath( const __dios::String &path, bool normalize = false ) {
    Result out;
    auto last = path.begin();
    while ( true ) {
        auto next = std::find_if( last, path.end(), &isPathSeparator );
        if ( next == path.end() ) {
            __dios::String s( last, next );
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
            __dios::String s( last, next );
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

inline __dios::String normalize( __dios::String path ) {
    auto abs = absolutePrefix( path );
    return joinPath( abs.first, joinPath( splitPath( abs.second, true ) ) );
}

} // namespace path
} // namespace fs
} // namespace __dios

#endif
