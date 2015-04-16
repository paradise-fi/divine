// -*- C++ -*- (c) 2015 Jiří Weiser

#include <algorithm>
#include <divine.h>
#include <divine/problem.h>

#include "fs-utils.h"
#include "fs-inode.h"

#ifndef _FS_FILE_H_
#define _FS_FILE_H_

namespace divine {
namespace fs {

struct Link : DataItem {

    Link( utils::String target ) :
        _target( std::move( target ) )
    {
        if ( _target.size() > PATH_LIMIT )
            throw Error( ENAMETOOLONG );
    }

    size_t size() const override {
        return _target.size();
    }

    const utils::String &target() const {
        return _target;
    }

private:
    utils::String _target;
};


struct File : DataItem {

    virtual bool read( char *, size_t, size_t & ) = 0;
    virtual bool write( const char *, size_t, size_t ) = 0;

    virtual void clear() = 0;

};

struct RegularFile : File {

    RegularFile( const char *content, size_t size ) :
        _snapshot( bool( content ) ),
        _size( content ? size : 0 ),
        _roContent( content )
    {}

    RegularFile() :
        _snapshot( false ),
        _size( 0 ),
        _roContent( nullptr )
    {}

    RegularFile( const RegularFile &other ) = default;
    RegularFile( RegularFile &&other ) = default;
    RegularFile &operator=( RegularFile ) = delete;

    size_t size() const override {
        return _size;
    }

    bool read( char *buffer, size_t offset, size_t &length ) override {
        const char *source = _isSnapshot() ?
                          _roContent + offset :
                          _content.data() + offset;
        if ( offset + length > _size )
            length = _size - offset;
        std::copy( source, source + length, buffer );
        return true;
    }

    bool write( const char *buffer, size_t offset, size_t length ) override {
        if ( _isSnapshot() )
            _copyOnWrite();

        if ( _content.size() < offset + length )
            resize( offset + length );

        std::copy( buffer, buffer + length, _content.begin() + offset );
        return true;
    }

    void clear() override {
        if ( !_size )
            return;

        _snapshot = false;
        resize( 0 );
    }

    void resize( size_t length ) {
        _content.resize( length );
        _size = _content.size();
    }

private:

    bool _isSnapshot() const {
        return _snapshot;
    }

    void _copyOnWrite() {
        const char *roContent = _roContent;
        _content.resize( _size );

        std::copy( roContent, roContent + _size, _content.begin() );
        _snapshot = false;
    }

    bool _snapshot;
    size_t _size;
    const char *_roContent;
    utils::Vector< char > _content;
};

struct WriteOnlyFile : public File {

    size_t size() const override {
        return 0;
    }
    bool read( char *, size_t, size_t & ) override {
        return false;
    }
    bool write( const char*, size_t, size_t ) override {
        return true;
    }
    void clear() override {
    }
};

struct Pipe : public File {

    Pipe() :
        _reader( false ),
        _writer( false )
    {}

    Pipe( bool r, bool w ) :
        _reader( r ),
        _writer( w )
    {}

    size_t size() const override {
        return _content.size();
    }

    bool read( char *buffer, size_t, size_t &length ) override {
        if ( length > _content.size() )
            length = _content.size();
        if ( length == 0 )
            return true;

        auto b = _content.begin();
        auto e = b + length;
        std::copy( b, e, buffer );

        _content.erase( b, e );
        return true;
    }

    bool write( const char *buffer, size_t, size_t length ) override {
        if ( !_reader )
            /// TODO: raise SIGPIPE signal
            throw Error( EPIPE );
        size_t offset = _content.size();
        _content.resize( offset + length );
        std::copy( buffer, buffer + length, _content.begin() + offset );
        return true;
    }

    void clear() override {
    }

    void releaseReader() {
        _reader = false;
    }

    bool reader() const {
        return _reader;
    }
    bool writer() const {
        return _writer;
    }

    void assignReader() {
        if ( _reader )
            __divine_problem( Other, "Pipe is opened for reading again." );
        _reader = true;
    }
    void assignWriter() {
        if ( _writer )
            __divine_problem( Other, "Pipe is opened for writing again." );
        _writer = true;
    }

private:
    utils::Vector< char > _content;
    bool _reader;
    bool _writer;
};

struct Socket : public File {
    // TODO: implement
};

} // namespace fs
} // namespace divine

#endif
