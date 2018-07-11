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
#include <unistd.h>
#include <fcntl.h>

#include "utils.h"
#include "storage.h"

namespace __dios::fs
{

struct Flags
{
    int _value = 0;
    constexpr Flags() = default;
    constexpr Flags( int o ) : _value( o ) {} /* implicit */
    explicit constexpr operator bool() const { return _value; }
    constexpr int to_i() const { return _value; }
};

template< typename Self >
struct FlagOps : Flags
{
    using Flags::Flags;
    constexpr bool has( Self s ) const { return _value & s._value; }
    constexpr void clear( Self s ) { _value &= ~s._value; }
    constexpr void set( Self s ) { _value |= s._value; }
};

struct OFlags : FlagOps< OFlags >
{
    using FlagOps< OFlags >::FlagOps;
    static constexpr int NOACC = O_WRONLY | O_RDWR;

    constexpr bool read() const     { return !( _value & O_WRONLY ); }
    constexpr bool write() const    { return _value & ( O_WRONLY | O_RDWR ); }
    constexpr bool noaccess() const { return ( _value & NOACC ) == NOACC; }
    constexpr bool nonblock() const { return _value & O_NONBLOCK; }
    constexpr bool follow() const   { return !( _value & O_NOFOLLOW ) && !( _value & O_EXCL ) ; }
};

#undef O_RDONLY
#undef O_WRONLY
#undef O_RDWR
#undef O_CREAT
#undef O_EXCL
#undef O_NOCTTY
#undef O_TRUNC
#undef O_APPEND
#undef O_NONBLOCK
#undef O_NDELAY
#undef O_DIRECTORY
#undef O_NOFOLLOW

constexpr OFlags O_RDONLY    = _HOST_O_RDONLY;
constexpr OFlags O_WRONLY    = _HOST_O_WRONLY;
constexpr OFlags O_RDWR      = _HOST_O_RDWR;
constexpr OFlags O_CREAT     = _HOST_O_CREAT;
constexpr OFlags O_EXCL      = _HOST_O_EXCL;
constexpr OFlags O_NOCTTY    = _HOST_O_NOCTTY;
constexpr OFlags O_TRUNC     = _HOST_O_TRUNC;
constexpr OFlags O_APPEND    = _HOST_O_APPEND;
constexpr OFlags O_NONBLOCK  = _HOST_O_NONBLOCK;
constexpr OFlags O_DIRECTORY = _HOST_O_DIRECTORY;
constexpr OFlags O_NOFOLLOW  = _HOST_O_NOFOLLOW;
constexpr OFlags O_NDELAY    = O_NONBLOCK;

template< typename F >
constexpr std::enable_if_t< std::is_base_of_v< Flags, F >, F > operator &( F a, F b )
{
    return F( a._value & b._value );
}

template< typename F >
constexpr std::enable_if_t< std::is_base_of_v< Flags, F >, F > operator |( F a, F b )
{
    return F( a._value | b._value );
}

template< typename F >
constexpr std::enable_if_t< std::is_base_of_v< Flags, F >, F > operator ~( F a )
{
    return F( ~a._value );
}

template< typename F >
constexpr std::enable_if_t< std::is_base_of_v< Flags, F >, F > &operator |= ( F &a, F b )
{
    a._value |= b._value;
    return a;
}

template< typename F >
constexpr std::enable_if_t< std::is_base_of_v< Flags, F >, F > &operator &= ( F &a, F b )
{
    a._value &= b._value;
    return a;
}

template< typename F >
constexpr std::enable_if_t< std::is_base_of_v< Flags, F >, bool > operator == ( F a, F b )
{
    return a._value == b._value;
}

template< typename F >
constexpr std::enable_if_t< std::is_base_of_v< Flags, F >, bool > operator != ( F a, F b )
{
    return a._value != b._value;
}

enum class FileTrace {
    NOTRACE,     /* ignore write in file */
    UNBUFFERED,  /* trace whenever possible */
    TRACE        /* trace on newline */
};

enum class Seek {
    Undefined,
    Set,
    Current,
    End
};

enum class SocketType {
    Stream,
    Datagram,
};

const int CURRENT_DIRECTORY = -100;
const int PATH_LIMIT = 1023;
const int FILE_NAME_LIMIT = 255;
const int FILE_DESCRIPTOR_LIMIT = 1024;
const int PIPE_SIZE_LIMIT = 1024;

namespace flags {

enum class Message {
    NoFlags = 0,
    DontWait = 1,
    Peek = 2,
    WaitAll = 4,
};


} // namespace flags

using storage::operator|;
template< typename T >
using LegacyFlags = storage::StrongEnumFlags< T >;

}
