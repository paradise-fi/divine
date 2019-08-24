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

#include <divine/mc/job.hpp>
#include <divine/mc/builder.hpp>

namespace divine::mc
{

    template< template< typename, typename > class Job_, typename Next >
    std::shared_ptr< Job > make_job( builder::BC bc, Next next )
    {
        if ( bc->is_symbolic() && bc->solver() != "none" )
        {
            auto solver = bc->solver();
#if OPT_Z3
            if ( solver == "z3" )
                return std::make_shared< Job_< Next, mc::Z3Builder > >( bc, next );
#endif
#if OPT_STP
            if ( solver == "stp" )
                return std::make_shared< Job_< Next, mc::STPBuilder > >( bc, next );
#endif
            if ( brick::string::startsWith( solver, "smtlib" ) )
            {
                std::vector< std::string > cmd;
                if ( solver == "smtlib" || solver == "smtlib:z3" )
                    cmd = { "z3", "-in", "-smt2" };
                else if ( solver == "smtlib:boolector" )
                    cmd = { "boolector", "--smt2" };
                else
                    cmd = { std::string( solver, 7, std::string::npos ) };
                return std::make_shared< Job_< Next, mc::SMTLibBuilder > >( bc, next, cmd );
            }
            UNREACHABLE( "unsupported solver", solver );
        }
        return std::make_shared< Job_< Next, mc::ExplicitBuilder > >( bc, next );
    }

}

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab ft=cpp
