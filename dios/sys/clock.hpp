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
#include <rst/domains.h>

namespace __dios
{

    /* TODO do something about usec/nsec values */

    template< typename Next >
    struct Clock : Next
    {
        enum { Fixed, DetTick, NDetTick, Sym, Sloppy } _clock_mode = Fixed;
        int _clock_delta = 0;
        short _clock_ticks = 0, _clock_max_ticks = 3;

        time_t _clock_mbase = 1262277040, /* monotonic */
               _clock_rbase = _clock_mbase; /* real-time */

        struct timezone _clock_tz = { 0, 1 };

        void configure( SysOpts &opts )
        {
            auto sv = extract_opt( "clock-type", opts );
            auto mt = extract_opt( "clock-maxticks", opts );
            auto base = extract_opt( "clock-start", opts );

            if ( sv.empty() || sv == "fixed" )
                _clock_mode = Fixed;
            else if ( sv == "det" )
                _clock_mode = DetTick;
            else if ( sv == "ndet" )
                _clock_mode = NDetTick;
            else if ( sv == "sym" )
                _clock_mode = Sym;
            else if ( sv == "sloppy" )
                _clock_mode = Sloppy;
            else
                __dios_fault( _DiOS_F_Config, "invalid clock type" );

            if ( !mt.empty() )
                _clock_max_ticks = _DIVINE_strtol( mt.begin(), mt.size(), nullptr );
            if ( !base.empty() )
                _clock_mbase = _clock_rbase = _DIVINE_strtol( base.begin(), base.size(), nullptr );
        }

        template< typename Setup >
        void setup( Setup s )
        {
            traceAlias< Clock >( "{Clock}" );
            configure( s.opts );
            Next::setup( s );
        }

        int gettimeofday( struct timeval *tp, struct timezone *tzp )
        {
            if ( tp )
            {
                tp->tv_sec = _clock_rbase + _clock_delta;
                tp->tv_usec = 0;
                clock_tick();
            }

            if ( tzp )
                *tzp = _clock_tz;

            return 0;
        }

        int settimeofday( const struct timeval *tp, const struct timezone *tzp )
        {
            if ( tp )
                _clock_rbase = tp->tv_sec - _clock_delta;

            if ( tzp )
                _clock_tz = *tzp;

            return 0;
        }

        int clock_gettime( clockid_t id, struct timespec *sp )
        {
            sp->tv_nsec = 0;

            switch ( id )
            {
                case CLOCK_REALTIME:
                    sp->tv_sec = _clock_rbase + _clock_delta; break;
                case CLOCK_MONOTONIC:
                case CLOCK_PROCESS_CPUTIME_ID:
                case CLOCK_THREAD_CPUTIME_ID:
                case CLOCK_BOOTTIME:
                case CLOCK_UPTIME:
                    sp->tv_sec = _clock_mbase + _clock_delta; break;
                default:
                    *__dios_errno() = EINVAL; return -1;
            }

            clock_tick();
            return 0;
        }

        int clock_settime( clockid_t id, const struct timespec *sp )
        {
            if ( id != CLOCK_REALTIME )
                return *__dios_errno() = EINVAL, -1;

            if ( sp->tv_nsec < 0 || sp->tv_nsec > 1'000'000'000 )
                return *__dios_errno() = EINVAL, -1;

            _clock_rbase = sp->tv_sec - _clock_delta;
            return 0;
        }

        void clock_tick()
        {
            if ( _clock_mode == Fixed )
                return; /* no ticks */

            if ( _clock_ticks >= _clock_max_ticks )
                return; /* do nothing */

            if ( _clock_mode == NDetTick && __vm_choose( 2 ) == 0 )
                return;

            ++ _clock_ticks;

            if ( _clock_mode == Sym || _clock_mode == Sloppy )
            {
                __dios_fault( _VM_F_NotImplemented, "symbolic clocks" );
                /*
                int offset = __sym_val_i32();
                if ( offset < 0 )
                    __vm_cancel();
                _clock_delta += offset;
                */
            }
            else
                ++ _clock_delta;
            /*
            if ( _clock_mode == Sloppy )
            {
                _clock_rbase = __sym_val_i64();
                if ( _clock_rbase < 0 )
                    __vm_cancel;
            }
            */
        }
    };

}
