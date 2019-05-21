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

namespace divine::smt
{
    using RPN = brick::smt::RPN< std::vector< uint8_t > >;
    using RPNView = brick::smt::RPNView< RPN >;

    using Op = brick::smt::Op;

    using Constant = RPNView::Constant;
    using Variable = RPNView::Variable;
    using UnaryOp = RPNView::UnaryOp;
    using BinaryOp = RPNView::BinaryOp;

    // TODO move to utils
    template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
    template<class... Ts> overload(Ts...) -> overload<Ts...>;

    template< typename Builder >
    auto evaluate( Builder &bld, const RPN &rpn ) -> typename Builder::Node
    {
        using Node = typename Builder::Node;
        std::vector< Node > stack;

        auto binary = [&] ( Op op, const auto& bin ) {
            auto a = stack.back();
            stack.pop_back();
            auto b = stack.back();
            stack.pop_back();
            stack.push_back( bld.binary( { op, bin.bitwidth }, a, b ) );
        };

        auto constrain = [&] ( const auto& con ) {
            if ( stack.size() == 1 ) {
                auto a = stack.back();
                stack.pop_back();
                auto b = bld.constant( true );
                stack.push_back( bld.binary( { Op::And, 1 }, a, b ) );
            } else {
                binary( Op::And, con );
            }
        };

        for ( const auto& term : RPNView{ rpn } ) {
            std::visit( overload {
                [&]( const Constant& con ) {
                    stack.push_back( bld.constant( con ) );
                },
                [&]( const Variable& var ) {
                    stack.push_back( bld.variable( var ) );
                },
                [&]( const UnaryOp& un ) {
                    auto arg = stack.back();
                    stack.pop_back();
                    stack.push_back( bld.unary( { un.op, un.bitwidth }, arg ) );
                },
                [&]( const BinaryOp& bin ) {
                    if ( bin.op == Op::Constraint ) {
                        constrain( bin );
                    } else {
                        binary( bin.op, bin );
                    }
                },
                [&]( const auto& ) {
                    UNREACHABLE_F( "Unsupported term type" );
                }
            }, term );
        }

        return stack.back();
    }

} // namespace divine::smt
