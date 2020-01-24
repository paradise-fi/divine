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
    using Node = brq::smtlib_node;
    using op_t = brq::smt_op;

    SMTLib2( brq::smtlib_context &ctx, std::string suff = "", bool defs = true )
        : _ctx( ctx ), _suff( suff ), _use_defs( defs ) {}

    Node unary( brq::smt_op op, Node n, int bw );
    Node extract( Node n, std::pair< int, int > );
    Node binary( brq::smt_op op, Node a, Node b, int bw );
    Node constant( uint64_t value, int bw );
    Node constant( bool );
    Node variable( int id, int bw );
    Node define( Node def );

    brq::smtlib_context &_ctx;
    std::string _suff;
    bool _use_defs = true;
    int _def_counter = 0;
};

} // namespace divine::smt::builder
