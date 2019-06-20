// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Jan Horáček <me@apophis.cz>
 * (c) 2017-2019 Zuzana Baranová <xbaranov@fi.muni.cz>
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

#include <divine/cc/codegen.hpp>
#include <divine/cc/driver.hpp>
#include <divine/cc/link.hpp>

DIVINE_RELAX_WARNINGS
#include "lld/Common/Driver.h"
DIVINE_UNRELAX_WARNINGS

#include <brick-proc>

namespace divine::cc
{
    void addSection( std::string filepath, std::string sectionName, const std::string &sectionData )
    {
        using namespace brick::fs;

        TempDir workdir( ".divine.addSection.XXXXXX", AutoDelete::Yes,
                        UseSystemTemp::Yes );
        auto secpath = joinPath( workdir, "sec" );
        std::ofstream secf( secpath, std::ios_base::out | std::ios_base::binary );
        secf << sectionData;
        secf.close();

        auto r = brick::proc::spawnAndWait( brick::proc::CaptureStderr, "objcopy",
                                    "--remove-section", sectionName, // objcopy can't override section
                                    "--add-section", sectionName + "=" +  secpath,
                                    "--set-section-flags", sectionName + "=noload,readonly",
                                    filepath );
        if ( !r )
            throw cc::CompileError( "could not add section " + sectionName + " to " + filepath
                            + ", objcopy exited with " + to_string( r ) );
    }

    std::vector< std::string > ld_args( cc::ParsedOpts& po, PairedFiles& objFiles )
    {
        std::vector< std::string > args;

        for ( auto op : po.opts )
            args.push_back( op );
        for ( auto path : po.libSearchPath )
            args.push_back( "-L" + path );
        if ( po.outputFile != "" )
        {
            args.push_back( "-o" );
            args.push_back( po.outputFile );
        }

        for ( auto file : objFiles )
        {
            if ( file.first == "lib" )
            {
                args.push_back( "-l" + file.second );
                continue;
            }

            if ( cc::is_object_type( file.first ) )
                args.push_back( file.first );
            else
                args.push_back( file.second );
        }
        args.insert( args.begin(), "divcc" );

        return args;
    }
}
