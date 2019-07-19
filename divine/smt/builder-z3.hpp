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

#if OPT_Z3
#include <divine/smt/builder-common.hpp>
#include <z3++.h>

namespace divine::smt::builder
{

#if 0
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

struct Z3
{
    using Node = z3::expr;
    Z3( z3::context &c ) : _ctx( c ) {}

    Node unary( Unary, Node ) { UNREACHABLE_F( "NOT IMPLEMENTED" ); }
    Node binary( Binary, Node, Node ) { UNREACHABLE_F( "NOT IMPLEMENTED" ); }
    Node constant( Constant ) { UNREACHABLE_F( "NOT IMPLEMENTED" ); }
    Node constant( bool ) { UNREACHABLE_F( "NOT IMPLEMENTED" ); }
    Node variable( Variable ) { UNREACHABLE_F( "NOT IMPLEMENTED" ); }

    z3::context &_ctx;
};

} // namespace divine::smt::builder
#endif
