// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Petr Ročkai <code@fixp.eu>
 * (c) 2015 Jiří Weiser
 * (c) 2014 Vladimír Štill
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
#include <util/array.hpp>
#include <algorithm>

namespace __dios::fs
{

    struct Stream
    {

        Stream( size_t capacity ) :
            _data( capacity ),
            _head( 0 ),
            _occupied( 0 )
        {}

        size_t size() const { return _occupied; }
        bool empty() const { return _occupied == 0; }
        size_t capacity() const { return _data.size(); }

        size_t push( const char *data, size_t length )
        {
            if ( _occupied + length > capacity() )
                length = capacity() - _occupied;

            if ( !length )
                return 0;

            size_t usedLength = std::min( length, capacity() - ( _head + _occupied ) % capacity() );
            std::copy( data, data + usedLength, end() );
            _occupied += usedLength;

            if ( usedLength < length )
            {
                std::copy( data + usedLength, data + length, end() );
                _occupied += length - usedLength;
            }
            return length;
        }

        size_t pop( char *data, size_t length )
        {
            if ( _occupied < length )
                length = _occupied;

            if ( !length )
                return 0;

            size_t usedLength = std::min( length, capacity() - _head );
            std::copy( begin(), begin() + usedLength, data );
            _head = ( _head + usedLength ) % capacity();

            if ( usedLength < length )
            {
                std::copy( begin(), begin() + length - usedLength, data + usedLength );
                _head = ( _head + length - usedLength ) % capacity();
            }
            _occupied -= length;
            return length;
        }

        size_t peek( char *data, size_t length )
        {
            size_t head = _head;
            size_t occupied = _occupied;
            size_t r = pop( data, length );
            _head = head;
            _occupied = occupied;
            return r;
        }

        bool resize( size_t newCapacity )
        {
            if ( newCapacity < size() )
                return false;

            Array< char > newData( newCapacity );

            _occupied = pop( &newData.front(), _occupied );
            _head = 0;
            _data.swap( newData );
            return true;
        }

    private:
        using Iterator = Array< char >::iterator;

        Iterator begin() { return _data.begin() + _head; }
        Iterator end() { return _data.begin() + ( _head + _occupied ) % capacity(); }

        Array< char > _data;
        unsigned _head;
        unsigned _occupied;
    };

}
