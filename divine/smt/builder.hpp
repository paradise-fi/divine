// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
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

#include <divine/vm/heap.hpp>
#include <runtime/abstract/formula.h>
#include <brick-smt>

#if OPT_Z3
#include <z3++.h>
#endif

namespace divine::smt::builder
{

struct SMTLib2
{
    using Node = brick::smt::Node;
    SMTLib2( brick::smt::Context &ctx, std::string suff = "" ) : _ctx( ctx ), _suff( suff ) {}

    Node unary( sym::Unary un, Node n );
    Node binary( sym::Binary bin, Node a, Node b );
    Node constant( sym::Type t, uint64_t val );
    Node constant( bool );
    Node variable( sym::Type t, int32_t id );
    Node define( Node def );

    brick::smt::Context &_ctx;
    std::string _suff;
    int _def_counter = 0;
};

#if OPT_Z3
struct Z3
{
    using Node = z3::expr;
    Z3( z3::context &c ) : _ctx( c ) {}

    Node unary( sym::Unary un, Node n );
    Node binary( sym::Binary bin, Node a, Node b );
    Node constant( sym::Type t, uint64_t val );
    Node constant( bool );
    Node variable( sym::Type t, int32_t id );

    z3::context &_ctx;
};
#endif

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

}

