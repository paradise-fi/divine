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
#include <divine/vm/pointer.hpp>

#include <list>
#include <cassert>
#include <optional>

namespace divine::smt
{
    //using RPN = brick::smt::RPN< Array< uint8_t > >;
    using RPN = brick::smt::RPN< std::vector< uint8_t > >; // no append ! RPN::extend
    using RPNView = brick::smt::RPNView< RPN >;

    using Op = brick::smt::Op;

    using Bitwidth = brick::smt::Bitwidth;

    using Constant = RPNView::Constant;
    using Variable = RPNView::Variable;
    using CastOp = RPNView::CastOp;
    using UnaryOp = RPNView::UnaryOp;
    using BinaryOp = RPNView::BinaryOp;
    using CallOp = RPNView::CallOp;

    struct Unary  : UnaryOp  { Bitwidth bw; };
    struct Binary : BinaryOp { Bitwidth bw; };

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

    // decompose the input RPN into smaller RPNs
    // which have dependent variables, for later caching
    // TODO: output iterator
    static std::vector< RPN > decompose( const RPN &rpn )
    {
        using VarID = RPN::VarID;

        std::map< VarID, RPN > decomp;
        std::vector< RPN > constant_RPNs;

        std::optional< VarID > current_var;
        RPN active_RPN;
        bool is_constraint = false;

        std::list< std::pair< RPN, RPNView::Iterator > > control_stack;

        union_find< VarID > uf;
        std::vector< union_find< VarID >::Node* > shadow;

        auto pop = [&]( auto& s )
        {
            auto v = s.back();
            s.pop_back();
            return v;
        };

        auto push_control = [&]( RPN&& rpn )
        {
            control_stack.emplace_back( std::move( rpn ), RPNView::Iterator() );
            control_stack.back().second = RPNView{ control_stack.back().first }.begin();
        };

        auto append_rpn = [&]( VarID var, RPN& rpn )
        {
            auto it = decomp.find( var );
            if( it != decomp.end() )
            {
                auto& r = (*it).second;
                r.insert( r.end(), rpn.begin(), rpn.end() );
                (*it).second.apply< Op::And >();
            }
            decomp.emplace( var, rpn );
        };

        auto handle_binary = [&] ( BinaryOp bin )
        {
            if ( bin.op == Op::Constraint )
            {
                bin.op = Op::And;
                shadow.pop_back();
                shadow.pop_back();
                is_constraint = true;
            }
            else 
            {
                auto n1 = pop( shadow );
                auto n2 = pop( shadow );
                shadow.emplace_back( uf.union_( n1, n2 ) );
            }
        };

        auto handle_unary = [&]( const UnaryOp &un ){};
        auto handle_cast = [&]( CastOp cast ){};

        auto handle_call = [&]( CallOp call )
        {
            // we only need to go in if we haven't met a variable
            /*
            vm::HeapPointer ptr( call.objid );
            RPN rpn = bld.read( ptr );
            push_control( RPN( rpn ) );
            */
        };

        auto handle_term = overload
        {
            [&]( const Constant& con )
            { shadow.emplace_back( nullptr ); },
            [&]( const Variable& var )
            {
                if ( !current_var )
                    current_var = var.id;
                shadow.emplace_back( uf.make_set( var.id ) );
            },
            handle_unary, handle_cast, handle_binary, handle_call,
            [&]( const auto &term ) { UNREACHABLE( "unsupported term type", term ); }
        };

        push_control( RPN( rpn ) );

        while ( !control_stack.empty() )
        {
            auto& cs = control_stack.back();
            RPNView view{ cs.first };

            RPNView v{ rpn };
            // we have the first Constraint, so the rpn begins with a "true"?? FIXME
            if ( cs.second == v.begin() )
                ++cs.second;

            auto term = *cs.second;            

            std::visit( handle_term, term );

            if( is_constraint )
            {
                if( !current_var ) // only constants
                    constant_RPNs.push_back( active_RPN );
                else
                    append_rpn( *current_var, active_RPN );

                active_RPN.clear();
                current_var.reset();
            }
            else
                std::visit( [&]( auto t ) { active_RPN.insert( active_RPN.end(), bytes_begin(t), bytes_end(t) ); }, term );

            ++cs.second;

            if ( cs.second == view.end() )
                control_stack.pop_back();
        }

        //re-assign based on union-find variable decomposition
        for( auto it = decomp.begin(); it != decomp.end(); )
        {
            auto& [var, rpn] = *it;
            auto repr = uf.get_representant( var );
            ASSERT( repr.has_value() );
            if ( var != *repr )
            {
                append_rpn( *repr, rpn );
                decomp.erase( it++ );
            }
            else
                ++it;
        }

        // add RPNs composed of constants-only
        std::vector< RPN > ret = constant_RPNs;
        for( auto [var,rpn] : decomp )
            ret.push_back( rpn );

        return ret;
    }

    template< typename Builder >
    auto evaluate( Builder &bld, const RPN &rpn, bool *is_constraint = nullptr )
        -> typename Builder::Node
    {
        using Node = typename Builder::Node;
        std::vector< std::pair< Node, Bitwidth > > stack;
        std::list< std::pair< RPN, RPNView::Iterator > > control_stack;

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

        auto push_control = [&]( RPN&& rpn )
        {
            control_stack.emplace_back( std::move( rpn ), RPNView::Iterator() );
            control_stack.back().second = RPNView{ control_stack.back().first }.begin();
        };

        auto handle_binary = [&] ( BinaryOp bin )
        {
            if ( bin.op == Op::Constraint )
            {
                bin.op = Op::And;
                if ( is_constraint ) *is_constraint = true;
            }

            auto [ b, bbw ] = pop();
            auto [ a, abw ] = pop();

            auto bw = bitwidth( bin.op, abw, bbw );
            push( bld.binary( { bin.op, bw }, a, b ), bw );
        };

        auto handle_unary = [&]( const UnaryOp &un )
        {
            auto [ arg, bw ] = pop();
            push( bld.unary( { un, bw }, arg ), bw );
        };

        auto handle_cast = [&]( CastOp cast )
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
        };

        auto handle_call = [&]( CallOp call )
        {
            vm::HeapPointer ptr( call.objid );
            RPN rpn = bld.read( ptr );
            push_control( RPN( rpn ) );
        };

        auto handle_term = overload
        {
            [&]( const Constant& con ) { push( bld.constant( con ), con.bitwidth() ); },
            [&]( const Variable& var ) { push( bld.variable( var ), var.bitwidth() ); },
            handle_unary, handle_cast, handle_binary, handle_call,
            [&]( const auto &term ) { UNREACHABLE( "unsupported term type", term ); }
        };

        TRACE( "evaluate", rpn );

        push_control( RPN( rpn ) );

        while ( !control_stack.empty() )
        {
            auto& cs = control_stack.back();
            RPNView view{ cs.first };
            auto term = *cs.second;
            ++cs.second;

            if ( cs.second == view.end() )
                control_stack.pop_back();

            if ( is_constraint ) *is_constraint = false;
            std::visit( handle_term, term );
        }

        return stack.back().first;
    }
}
