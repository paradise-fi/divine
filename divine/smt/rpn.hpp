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

#include <brick-smt>

#include <cassert>

namespace divine::smt
{
    using RPN = brick::smt::RPN< std::vector< uint8_t > >;
    using RPNView = brick::smt::RPNView< RPN >;

    using Op = brick::smt::Op;

    using Bitwidth = brick::smt::Bitwidth;

    using Constant = RPNView::Constant;
    using Variable = RPNView::Variable;
    using CastOp = RPNView::CastOp;
    using UnaryOp = RPNView::UnaryOp;
    using BinaryOp = RPNView::BinaryOp;

    struct Unary : UnaryOp
    {
        Bitwidth bw;
    };

    struct Binary : BinaryOp
    {
        Bitwidth bw;
    };

    // TODO move to utils
    template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
    template<class... Ts> overload(Ts...) -> overload<Ts...>;

    constexpr Bitwidth bitwidth( Op op, Bitwidth abw, Bitwidth bbw ) noexcept
    {
        ASSERT_EQ( abw, bbw );
        if ( brick::smt::is_cmp( op ) )
            return 1;
        return abw;
    }

    template< typename Builder >
    auto evaluate( Builder &bld, const RPN &rpn ) -> typename Builder::Node
    {
        using Node = typename Builder::Node;
        std::vector< std::pair< Node, Bitwidth > > stack;

        auto pop = [&]()
        {
            auto v = stack.back();
            TRACE( "pop", v );
            stack.pop_back();
            return v;
        };

        auto push = [&]( Node n, Bitwidth bw )
        {
            TRACE( "push", n, "bw", bw );
            stack.emplace_back( n, bw );
        };

        TRACE( "evaluate", rpn );

        auto binary = [&] ( Op op, const auto& bin )
        {
            auto [ b, bbw ] = pop();
            auto [ a, abw ] = pop();

            auto bw = bitwidth( op, abw, bbw );
            push( bld.binary( { op, bw }, a, b ), bw );
        };

        for ( const auto& term : RPNView{ rpn } )
        {
            TRACE( "shift", term );
            std::visit( overload {
                [&]( const Constant& con )
                {
                    push( bld.constant( con ), con.bitwidth );
                },
                [&]( const Variable& var )
                {
                    push( bld.variable( var ), var.bitwidth );
                },
                [&]( const UnaryOp& un )
                {
                    auto [ arg, bw ] = pop();
                    push( bld.unary( { un, bw }, arg ), bw );
                },
                [&]( CastOp cast )
                {
                    auto [ arg, bw ] = pop();

                    if ( cast.op == Op::ZFit )
                    {
                        if ( cast.bitwidth < bw )
                            cast.op = Op::Trunc;
                        else
                            cast.op = Op::ZExt;
                    }

                    Unary op = { cast.op, cast.bitwidth };
                    push( bld.unary( op, arg ), cast.bitwidth );
                },
                [&]( const BinaryOp& bin )
                {
                    if ( bin.op == Op::Constraint )
                        binary( Op::And, bin );
                    else
                        binary( bin.op, bin );
                },
                [&]( const auto& )
                {
                    UNREACHABLE( "unsupported term type", term );
                }
            }, term );
        }

        return stack.back().first;
    }

} // namespace divine::smt
