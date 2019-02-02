// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
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
#include <divine/dbg/node.hpp>

namespace divine::dbg
{

using DNSet = std::set< DNKey >;

template< typename BT, typename Fmt, typename DN >
void backtrace( BT bt, Fmt fmt, DN dn, DNSet &visited, int &stacks, int maxdepth )
{
    if ( visited.count( dn.sortkey() ) && dn.address().type() != vm::PointerType::Global )
        return;
    visited.insert( dn.sortkey() );

    if ( !maxdepth )
        return;

    if ( dn.kind() == DNKind::Frame && dn.valid() )
        fmt( dn );

    auto follow =
        [&]( std::string k, auto rel )
        {
            if ( rel.kind() == DNKind::Frame && k != "caller" &&
                 rel.address().type() == vm::PointerType::Heap && rel.valid() &&
                 !visited.count( rel.sortkey() ) && maxdepth > 1 )
                bt( ++stacks );
            backtrace( bt, fmt, rel, visited, stacks, k == "caller" ? maxdepth - 1 : maxdepth );
        };

    dn.related( follow, false );
    dn.components( follow );
}

}
