// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
 * (c) 2018-2019 Henrich Lauko <xlauko@mail.muni.cz>
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

#include <variant>

#include <divine/smt/rpn.hpp>
#include <divine/vm/divm.h>
#include <brick-smt>

namespace divine::smt::builder
{
    using namespace divine::smt;

    template< typename Builder >
    auto mk_bin( Builder &bld, Op op, int bw, typename Builder::Node a, typename Builder::Node b )
        -> typename Builder::Node
    {
        ASSERT( brick::smt::is_binary( op ) );
        return bld.binary( { op, bw }, a, b );
    }

    template< typename Builder >
    auto mk_un( Builder &bld, Op op, int bw, typename Builder::Node a )
        -> typename Builder::Node
    {
        ASSERT( brick::smt::is_unary( op ) );
        return bld.unary( { op, bw }, a );
    }

} // namespace divine::smt::builder
