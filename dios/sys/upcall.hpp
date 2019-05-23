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
#include <dios/sys/syscall.hpp>

namespace __dios
{

    /* Implements calls that depend on the entire stack. This component is
     * always at the top of the stack and its functionality is in virtual
     * method overrides (and they are declared in `BaseContext`). */

    template< typename Conf >
    struct Upcall : Conf
    {
        using Process = typename Conf::Process;
        using BaseProcess = typename BaseContext::Process;

        /* This method is used by the kernel to force a reschedule in the
         * middle of a system call, usually because the task that is currently
         * running was killed. Do not confuse with __dios_reschedule() which is
         * instrumented into userspace code by LART to facilitate task
         * preemption. */

        virtual void reschedule() override
        {
            if ( this->check_final() )
                this->finalize();
            __dios_stack_free( __dios_parent_frame(), nullptr );
            __vm_suspend();
        }

        virtual BaseProcess *make_process( BaseProcess *bp ) override
        {
            if ( bp )
                return new Process( *static_cast< Process * >( bp ) );
            else
                return new Process();
        }
    };

}
