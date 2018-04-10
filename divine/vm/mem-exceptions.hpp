// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Adam Matoušek <xmatous3@fi.muni.cz>
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

#include <brick-types>
#include <divine/vm/divm.h>

namespace divine::vm::mem
{

template< typename ExceptionType, typename Loc_ >
struct ExceptionMap
{
    using Loc = Loc_;
    using Internal = typename Loc::Internal;
    using ExcMap = std::map< Loc, ExceptionType >;
    using Lock = std::lock_guard< std::mutex >;

    ExceptionMap &operator=( const ExceptionType & o ) = delete;

    ExceptionType &at( Internal obj, int wpos )
    {
        ASSERT_EQ( wpos % 4, 0 );

        Lock lk( _mtx );

        auto it = _exceptions.find( Loc( obj, 0, wpos ) );
        ASSERT( it != _exceptions.end() );
        return it->second;
    }

    void set( Internal obj, int wpos, const ExceptionType &exc )
    {
        ASSERT_EQ( wpos % 4, 0 );

        Lock lk( _mtx );
        _exceptions[ Loc( obj, 0, wpos ) ] = exc;
    }

    void free( Internal obj )
    {
        Lock lk( _mtx );

        auto lb = _exceptions.lower_bound( Loc( obj, 0, 0 ) );
        auto ub = _exceptions.upper_bound( Loc( obj, 0, (1 << _VM_PB_Off) - 1 ) );
        _exceptions.erase( lb, ub );
    }

    template< typename OM >
    void copy( OM &from_m, typename OM::Loc from, Loc to, int sz )
    {
        Lock lk( _mtx );
        typename OM::Loc from_p( from.object, from.objid, from.offset + sz );

        int delta = to.offset - from.offset;

        auto lb = from_m._exceptions.lower_bound( from );
        auto ub = from_m._exceptions.upper_bound( from_p );
        std::transform( lb, ub, std::inserter( _exceptions, _exceptions.begin() ), [&]( auto x )
        {
            auto fl = x.first;
            Loc l( to.object, fl.objid, fl.offset + delta );
            return std::make_pair( l, x.second );
        } );
    }

    bool empty()
    {
        Lock lk( _mtx );
        return std::all_of( _exceptions.begin(), _exceptions.end(),
                            []( const auto & e ) { return ! e.second.valid(); } );
    }

    ExcMap _exceptions;
    mutable std::mutex _mtx;
};

}
