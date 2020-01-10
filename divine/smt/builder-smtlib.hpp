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

#include <divine/smt/builder-common.hpp>
#include <brick-smtlib>

namespace divine::smt::builder
{

struct SMTLib2
{
    using Node = brick::smt::Node;
    SMTLib2( brick::smt::Context &ctx, std::string suff = "", bool defs = true )
        : _ctx( ctx ), _suff( suff ), _use_defs( defs ) {}

    Node unary( Unary op, Node n );
    Node binary( Binary op, Node a, Node b );
    Node constant( smt::Bitwidth bw, uint64_t value );
    Node constant( Constant con );
    Node constant( bool );
    Node variable( Variable var );
    Node define( Node def );

    brick::smt::Context &_ctx;
    std::string _suff;
    bool _use_defs = true;
    int _def_counter = 0;
};

} // namespace divine::smt::builder
