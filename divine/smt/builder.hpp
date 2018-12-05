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

#if OPT_Z3
#include <z3++.h>
#endif

#if OPT_STP
#include <stp/STPManager/STPManager.h>
#endif

namespace divine::smt::builder
{

struct SMTLib2
{
    using Node = brick::smt::Node;
    SMTLib2( brick::smt::Context &ctx, std::string suff = "", bool defs = true )
        : _ctx( ctx ), _suff( suff ), _use_defs( defs ) {}

    Node unary( sym::Unary un, Node n );
    Node binary( sym::Binary bin, Node a, Node b );
    Node constant( sym::Type t, uint64_t val );
    Node constant( bool );
    Node variable( sym::Type t, int32_t id );
    Node define( Node def );

    brick::smt::Context &_ctx;
    std::string _suff;
    bool _use_defs = true;
    int _def_counter = 0;
};

#if OPT_STP
struct STP
{
    using Node = stp::ASTNode;

    Node unary( sym::Unary un, Node n );
    Node binary( sym::Binary bin, Node a, Node b );
    Node constant( sym::Type t, uint64_t val );
    Node constant( int w, uint64_t val );
    Node constant( int val );
    Node constant( bool );
    Node variable( sym::Type t, int32_t id );

    STP( stp::STPMgr &stp ) : _stp( stp ) {}
    stp::STPMgr &_stp;
};
#endif

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

}

