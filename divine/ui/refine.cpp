// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2020 Lukáš Korenčik <xkorenc1@fi.muni.cz>
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


#include <divine/ui/cli.hpp>

#include <unordered_set>

#include <divine/ra/llvmrefine.hpp>

#include <divine/ui/sysinfo.hpp>

namespace divine::ui {

bool refine::is_valid()
{
    static const std::unordered_set< std::string > known = { "rewirecalls" };
    return known.count( _refinement ) != 0;
}


void refine::setup()
{

    if ( _refinement.empty() )
        die( "No refinement chosen.\n" );

    if ( !is_valid() )
        die( "Unknown refinement" );

    if ( _refinement == "rewirecalls" && _output_bc.empty() )
        die( "Output file not specified" );

    with_bc::setup();
}

void refine::run()
{
    if ( _refinement == "rewirecalls" )
        rewire_calls();
}

void refine::rewire_calls()
{
    auto bc = take_bc();
    divine::ra::indirect_calls_refinement_t rbc( std::move( bc->_ctx ),
                                                 std::move( bc->_module ),
                                                 _bc_opts );
    rbc.finish();
    if ( !_output_bc.empty() )
        brick::llvm::writeModule( rbc._m.get(), _output_bc );
}

} // namespace divine::ui
