// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Vladimír Štill <xstill@fi.muni.cz>
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

#include <divine/sim/cli.hpp>
#include <optional>

namespace divine::sim
{

struct Stack
{
    Stack( CLI &i, std::optional< DN > top = std::nullopt ) :
        _i( &i ),
        _top( top.value_or( _i->get( "$top" ) ) )
    { }

    // returns invalid DN if there is none
    DN raw_up( DN frame )
    {
        return _i->get( "caller", true, std::make_unique< DN >( std::move( frame ) ) );
    }

    DN top() { return _top; }

    DN bottom()
    {
        DN i = _top, prev = _top;
        while ( i.valid() ) {
            prev = i;
            i = raw_up( i );
        }
        return prev;
    }

    bool is_kernel()
    {
        auto sched_pc = _i->_ctx.get( _VM_CR_Scheduler ).pointer;
        auto bottom_pc = bottom().pc();
        return sched_pc.object() == bottom_pc.object();
    }

    std::optional< Stack > get_userspace_stack()
    {
        auto int_frame = _i->_ctx.get( _VM_CR_IntFrame ).pointer;
        if ( !is_kernel() || int_frame.null() )
            return std::nullopt;

        DN other_top = _top;
        other_top.address( dbg::DNKind::Frame, int_frame );
        return Stack( *_i, other_top );
    }

    private:
    CLI *_i;
    DN _top;
};

DN CLI::frame_up( DN frame )
{
    Stack stack( *this, frame );
    DN up = stack.raw_up( frame );

    // switch to userspace if we hit the bottom of the kernel stack
    if ( stack.is_kernel() && !up.valid() )
    {
        auto nstack = stack.get_userspace_stack();
        if ( nstack )
        {
            stack = nstack.value();
            up = stack.top();
        }
    }

    return up;
}

}
