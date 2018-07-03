#pragma once

#include <divine/cc/filetype.hpp>
#include <llvm/Support/Errc.h>

DIVINE_RELAX_WARNINGS
#include <clang/Tooling/Tooling.h> // ClangTool
#include <llvm/Support/Path.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-fs>
#include <brick-query>
#include <iostream>

namespace divine {
namespace cc {

enum class VFSError {
    InvalidIncludePath = 1000 // avoid clash with LLVM's error codes, they don't check category
};

struct VFSErrorCategory : std::error_category {
    const char *name() const noexcept override { return "DIVINE VFS error"; }
    std::string message( int condition ) const override {
        switch ( VFSError( condition ) ) {
            case VFSError::InvalidIncludePath:
                return "Invalid include path, not accessible in DIVINE";
        }
    }
};

static std::error_code make_error_code( VFSError derr ) {
    return std::error_code( int( derr ), VFSErrorCategory() );
}

} // cc
} // divine


namespace std {
    template<>
    struct is_error_code_enum< divine::cc::VFSError > : std::true_type { };
}


namespace divine {
namespace cc {

struct VFS : clang::vfs::FileSystem
{

    using Status = clang::vfs::Status;

  private:

    // adapt file map to the expectations of LLVM's VFS
    struct File : clang::vfs::File {
        File( llvm::StringRef content, Status stat ) :
            content( content ), stat( stat )
        { }

        llvm::ErrorOr< Status > status() override { return stat; }

        auto getBuffer( const llvm::Twine &/* path */, int64_t /* fileSize */ = -1,
                        bool /* requiresNullTerminator */ = true,
                        bool /* IsVolatile */ = false ) ->
            llvm::ErrorOr< std::unique_ptr< llvm::MemoryBuffer > > override
        {
            return { llvm::MemoryBuffer::getMemBuffer( content ) };
        }

        std::error_code close() override { return std::error_code(); }

      private:
        llvm::StringRef content;
        Status stat;
    };

    std::error_code setCurrentWorkingDirectory( const llvm::Twine &path ) override
    {
        _cwd = brick::fs::isAbsolute( path.str() ) ? path.str() : brick::fs::joinPath( _cwd, path.str() );
        return std::error_code();
    }

    llvm::ErrorOr< std::string > getCurrentWorkingDirectory() const override
    {
        return _cwd;
    }

    static auto doesNotExits() // forward to the real FS
    {
        return std::error_code( llvm::errc::no_such_file_or_directory );
    }

    static auto blockAccess( const llvm::Twine & )
    {
        return std::error_code( VFSError::InvalidIncludePath );
    };

    Status statpath( const std::string &path, clang::vfs::Status stat )
    {
        return Status::copyWithNewName( stat, path );
    }

  public:

    VFS() : _cwd( brick::fs::getcwd() ) {}

    std::string normal( std::string p )
    {
        auto abs = brick::fs::isAbsolute( p ) ? p : brick::fs::joinPath( _cwd, p );
        return brick::fs::normalize( abs);
    }

    auto status( const llvm::Twine &_path ) ->
        llvm::ErrorOr< clang::vfs::Status > override
    {
        auto path = normal( _path.str() );
        auto it = filemap.find( path );
        if ( it != filemap.end() )
            return statpath( path, it->second.second );
        else if ( allowed( path ) )
            return { doesNotExits() };
        return { blockAccess( path ) };
    }

    auto openFileForRead( const llvm::Twine &_path ) ->
        llvm::ErrorOr< std::unique_ptr< clang::vfs::File > > override
    {
        auto path = normal( _path.str() );

        auto it = filemap.find( path );
        if ( it != filemap.end() )
            return mkfile( it, statpath( path, it->second.second ) );
        else if ( allowed( path ) )
            return { doesNotExits() };
        return { blockAccess( path ) };
    }

    auto dir_begin( const llvm::Twine &_path, std::error_code & ) ->
        clang::vfs::directory_iterator override
    {
        std::cerr << "DVFS:dir_begin " << normal( _path.str() ) << std::endl;
        NOT_IMPLEMENTED();
    }

    void allowPath( std::string path ) {
        path = normal( path );
        allowedPrefixes.insert( path );
        auto parts = brick::fs::splitPath( path );
        addDir( parts.begin(), parts.end() );
    }

    void addFile( std::string name, std::string contents, bool allowOverride = false ) {
        storage.emplace_back( std::move( contents ) );
        addFile( name, llvm::StringRef( storage.back() ), allowOverride );
    }

    void addFile( std::string path, llvm::StringRef contents, bool allowOverride = false ) {
        ASSERT( allowOverride || !filemap.count( path ) );
        auto &ref = filemap[ path ];
        ref.first = contents;
        auto name = llvm::sys::path::filename( path );
        ref.second = clang::vfs::Status( name,
                        clang::vfs::getNextVirtualUniqueID(),
                        llvm::sys::TimePoint<>(), 0, 0, contents.size(),
                        llvm::sys::fs::file_type::regular_file,
                        llvm::sys::fs::perms::all_all );
        auto parts = brick::fs::splitPath( path );
        if ( !parts.empty() )
            addDir( parts.begin(), parts.end() - 1 );
    }

    template< typename It >
    void addDir( It begin, It end ) {
        for ( auto it = begin; it < end; ++it ) {
            auto path = brick::fs::joinPath( begin, std::next( it ) );
            filemap[ path ] = { "", clang::vfs::Status( *it,
                    clang::vfs::getNextVirtualUniqueID(),
                    llvm::sys::TimePoint<>(), 0, 0, 0,
                    llvm::sys::fs::file_type::directory_file,
                    llvm::sys::fs::perms::all_all ) };
        }
    }

    std::vector< std::string > filesMappedUnder( std::string path ) {
        auto prefix = brick::fs::splitPath( path );
        return brick::query::query( filemap )
            .filter( []( auto &pair ) { return !pair.second.second.isDirectory(); } )
            .map( []( auto &pair ) { return pair.first; } )
            .filter( [&]( auto p ) {
                    auto split = brick::fs::splitPath( p );
                    return split.size() >= prefix.size()
                           && std::equal( prefix.begin(), prefix.end(), split.begin() );
                } )
            .freeze();
    }

  private:

    bool allowed( std::string path ) {
        if ( path.empty() || brick::fs::isRelative( path ) )
            return true; // relative or .

        auto parts = brick::fs::splitPath( path );
        for ( auto it = std::next( parts.begin() ); it < parts.end(); ++it )
            if ( allowedPrefixes.count( brick::fs::joinPath( parts.begin(), it ) ) )
                return true;
        return false;
    }

    template< typename It >
    std::unique_ptr< clang::vfs::File > mkfile( It it, clang::vfs::Status stat ) {
        return std::make_unique< File >( it->second.first, stat );
    }

    std::map< std::string, std::pair< llvm::StringRef, clang::vfs::Status > > filemap;
    std::vector< std::string > storage;
    std::set< std::string > allowedPrefixes;
    std::string _cwd;
};

} // cc
} // divine
