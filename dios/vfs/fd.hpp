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
#include <memory>
#include <dirent.h>

#include <dios/vfs/flags.hpp>
#include <dios/vfs/inode.hpp>

namespace __dios::fs
{

struct FileDescriptor
{
    FileDescriptor( Node inode, OFlags fl, size_t offset = 0 ) :
        _inode( inode ),
        _flags( fl ),
        _offset( offset )
    {
        inode->open( *this );
    }

    ~FileDescriptor()
    {
        if ( _inode )
            _inode->close( *this );
    }

    FileDescriptor( const FileDescriptor & ) noexcept = default; /* used during fork */
    FileDescriptor( FileDescriptor && ) noexcept = default;
    FileDescriptor &operator=( FileDescriptor && ) noexcept = default;

    long long read( void *buf, size_t length )
    {
        if ( length == 0 )
            return 0;

        if ( _flags.nonblock() && !_inode->canRead() )
            return error( EAGAIN ), -1;

        char *dst = reinterpret_cast< char * >( buf );
        if ( !_inode->read( dst, _offset, length ) )
            return -1;

        _offset += length;
        return length;
    }

    long long write( const void *buf, size_t length, Node remote = nullptr )
    {
        if ( _flags.nonblock() && !_inode->canWrite( length, remote ) )
            return error( EAGAIN ), -1;

        if ( _flags.has( O_APPEND ) )
            _offset = _inode->size();

        const char *src = reinterpret_cast< const char * >( buf );

        if ( !_inode->write( src, _offset, length, remote ) )
            return -1;

        _offset += length;
        return length;
    }

    size_t offset() const { return _offset; }
    void offset( size_t off ) { _offset = off; }
    size_t size() { return _inode ? _inode->size() : 0; }
    Node inode() const { return _inode; }
    OFlags flags() const { return _flags; }
    OFlags &flags() { return _flags; }
    explicit operator bool() const { return bool( _inode );}

    void close()
    {
        _inode->close( *this );
        _inode = nullptr;
        _flags = 0;
        _offset = 0;
    }

    void ref() { ++_refcnt; }
    void unref() { if ( !--_refcnt ) close(); }

protected:
    Node _inode;
    OFlags _flags;
    size_t _offset;
    short _refcnt = 0;
};

}
