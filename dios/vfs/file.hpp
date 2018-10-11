// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2015 Jiří Weiser
 * (c) 2018 Petr Ročkai <code@fixp.eu>
 *
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

#pragma once
#include <algorithm>
#include <signal.h>
#include <dios.h>
#include <dios/sys/memory.hpp>
#include <dios/sys/trace.hpp>
#include <rst/common.h>
#include <sys/fault.h>
#include <sys/trace.h>

#include "utils.h"
#include <dios/vfs/inode.hpp>
#include <dios/vfs/fd.hpp>
#include "storage.h"

#define FS_CHOICE_GOAL          0

namespace __dios::fs
{

struct SymLink : INode
{
    SymLink( std::string_view target = "" ) : _target( target )
    {
        if ( _target.size() > PATH_LIMIT )
            throw Error( ENAMETOOLONG );
    }

    size_t size() const override { return _target.size(); }
    std::string_view target() const { return _target; }
    void target( String s ) { _target = std::move( s ); }
    void target( std::string_view s ) { _target = String( s ); }

private:
    String _target;
};

struct RegularFile : INode
{
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

    size_t size() const override { return _size; }
    bool canRead() const override { return true; }
    bool canWrite() const override { return true; }

    bool read( char *buffer, size_t offset, size_t &length ) override
    {
        if ( offset >= _size ) {
            length = 0;
            return true;
        }
        const char *source = _isSnapshot() ?
                          _roContent + offset :
                          _content.data() + offset;
        if ( offset + length > _size )
            length = _size - offset;
        std::copy( source, source + length, buffer );
        return true;
    }

    bool write( const char *buffer, size_t offset, size_t &length ) override
    {
        if ( _isSnapshot() )
            _copyOnWrite();

        if ( _content.size() < offset + length )
            resize( offset + length );

        std::copy( buffer, buffer + length, _content.begin() + offset );
        return true;
    }

    void clear() override
    {
        if ( !_size )
            return;

        _snapshot = false;
        resize( 0 );
    }

    void resize( size_t length )
    {
        _content.resize( length );
        _size = _content.size();
    }

    void content( std::string_view s )
    {
        _content.resize( s.size() );
        std::copy( s.begin(), s.end(), _content.begin() );
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
    __dios::Vector< char > _content;
};

struct VmTraceFile : INode
{
    bool canWrite() const override { return true; }

    __attribute__(( __annotate__( "divine.debugfn" ) ))
    bool write( const char *buffer, size_t, size_t &length ) override
    {
        if ( buffer[ length - 1 ] == 0 )
            __dios::traceInternal( 0, "%s", buffer );
        else
        {
            char buf[ length + 1 ];
            std::copy( buffer, buffer + length, buf );
            buf[ length ] = 0;
            __dios::traceInternal( 0, "%s", buf );
        }
        return true;
    }
};

struct VmBuffTraceFile : INode
{
    bool canWrite() const override { return true; }

    __debugfn void do_write( const char *data, size_t &length ) noexcept
    {
        auto &buf = get_debug().trace_buf[ abstract::weaken( __dios_this_task() ) ];
        buf.insert( buf.length(), data, length );
        auto nl = buf.find_last_of( "\n" );
        if ( nl != std::string::npos )
        {
            __dios::traceInternal( 0, "%s", buf.substr( 0, nl ).c_str() );
            buf.erase( 0, nl + 1 );
        }
        get_debug().persist();
        get_debug().persist_buffers();
    }

    __debugfn void do_flush() noexcept
    {
        for ( auto &b : get_debug().trace_buf )
        {
            if ( !b.second.empty() )
                __dios::traceInternal( 0, "%s", b.second.c_str() );
            b.second.clear();
        }
        get_debug().persist();
        get_debug().persist_buffers();
    }

    bool write( const char *data, size_t, size_t & length ) override
    {
        do_write( data, length );
        return true;
    }

    ~VmBuffTraceFile() { do_flush(); }
};

struct StandardInput : INode
{
    StandardInput() :
        _content( nullptr ),
        _size( 0 )
    {}

    StandardInput( const char *content, size_t size ) :
        _content( content ),
        _size( size )
    {}

    size_t size() const override { return _size; }

    bool canRead() const override
    {
        // simulate user drinking coffee
        if ( _size )
            return FS_CHOICE( 2 ) == FS_CHOICE_GOAL;
        return false;
    }

    bool read( char *buffer, size_t offset, size_t &length ) override
    {
        if ( offset >= _size ) {
            length = 0;
            return true;
        }
        const char *source = _content + offset;
        if ( offset + length > _size )
            length = _size - offset;
        std::copy( source, source + length, buffer );
        return true;
    }

private:
    const char *_content;
    size_t _size;
};

struct Pipe : INode
{
    Pipe() :
        _stream( PIPE_SIZE_LIMIT ),
        _reader( false ),
        _writer( false )
    {}

    Pipe( bool r, bool w ) :
        _stream( PIPE_SIZE_LIMIT ),
        _reader( r ),
        _writer( w )
    {}

    size_t size() const override { return _stream.size(); }
    bool canRead() const override { return size() > 0; }
    bool canWrite() const override { return size() < PIPE_SIZE_LIMIT; }

    void open( FileDescriptor &fd ) override
    {
        __dios_assert( fd.inode().get() == this );

        if ( fd.flags().read() && fd.flags().write() )
            __dios_fault( _VM_Fault::_VM_F_Assert, "Pipe is opened both for reading and writing" );

        if ( fd.flags().read() )
        {
            if ( !fd.flags().has( O_NONBLOCK ) && !writer() )
               __vm_cancel();
            assignReader();
        }

        if ( fd.flags().write() )
        {
            if ( !fd.flags().has( O_NONBLOCK ) && !reader() )
               __vm_cancel();
            assignWriter();
        }

        fd.flags().clear( O_NONBLOCK );
    }

    void close( FileDescriptor &fd ) override
    {
        __dios_assert( fd.inode().get() == this );
        if ( fd.flags().read() )
            releaseReader();
    }

    bool read( char *buffer, size_t, size_t &length ) override
    {
        if ( length == 0 )
            return true;

        // progress or deadlock
        while ( ( length = _stream.pop( buffer, length ) ) == 0 )
            __vm_cancel();

        return true;
    }

    bool write( const char *buffer, size_t, size_t &length ) override
    {
        if ( !_reader ) {
            raise( SIGPIPE );
            throw Error( EPIPE );
        }

        // progress or deadlock
        while ( ( length = _stream.push( buffer, length ) ) == 0 )
            __vm_cancel();

        return true;
    }

    void clear() override { throw Error( EINVAL ); }
    void releaseReader() { _reader = false; }
    bool reader() const { return _reader; }
    bool writer() const { return _writer; }

    void assignReader()
    {
        if ( _reader )
            __dios_fault( _VM_Fault::_VM_F_Assert, "Pipe is opened for reading again." );
        _reader = true;
    }

    void assignWriter()
    {
        if ( _writer )
            __dios_fault( _VM_Fault::_VM_F_Assert, "Pipe is opened for writing again." );
        _writer = true;
    }

private:
    storage::Stream _stream;
    bool _reader;
    bool _writer;
};

}