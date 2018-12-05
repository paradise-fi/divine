// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
 * (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
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

#include <lart/lart.h>
#include <brick-smt>

namespace divine::smt { namespace sym = lart::sym; }
namespace divine::smt::builder
{

template< typename B >
auto mk_bin( B &bld, sym::Op op, int bw, typename B::Node a, typename B::Node b )
{
    return bld.binary( sym::Binary( op, sym::Type( sym::Type::Int, bw ), vm::nullPointer(),
                                    vm::nullPointer() ),
                       a, b );
}

template< typename B >
auto mk_un( B &bld, sym::Op op, int bw, typename B::Node a )
{
    return bld.unary( sym::Unary( op, sym::Type( sym::Type::Int, bw ), vm::nullPointer() ), a );
}

} // namespace divine::smt::builder
