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
    // Add a section called 'section_name' with data 'section_data' into the
    // object file at 'filepath'
    void add_section( std::string filepath, std::string section_name, const std::string &section_data )
    {
        using namespace brick::fs;

        TempDir workdir( ".divine.addSection.XXXXXX", AutoDelete::Yes,
                        UseSystemTemp::Yes );

        // Dump the section data into a temporary file, so that objcopy can load them
        auto secpath = joinPath( workdir, "sec" );
        std::ofstream secf( secpath, std::ios_base::out | std::ios_base::binary );
        secf << section_data;
        secf.close();

        auto r = brick::proc::spawnAndWait( brick::proc::CaptureStderr, "objcopy",
                                    "--remove-section", section_name, // objcopy can't override section
                                    "--add-section", section_name + "=" +  secpath,
                                    "--set-section-flags", section_name + "=noload,readonly",
                                    filepath );
        if ( !r )
            throw cc::CompileError( "could not add section " + section_name + " to " + filepath
                            + ", objcopy exited with " + to_string( r ) );
    }

    // Build CLI arguments for a linker invocation
    std::vector< std::string > ld_args( cc::ParsedOpts& po, PairedFiles& files )
    {
        std::vector< std::string > args;

	// Add compiler driver arguments
        for ( auto op : po.opts )
            args.push_back( op );
	// Add library search paths
        for ( auto path : po.libSearchPath )
            args.push_back( "-L" + path );
	// Set the output name
        if ( po.outputFile != "" )
        {
            args.push_back( "-o" );
            args.push_back( po.outputFile );
        }

	// Push the input files
        for ( auto file : files )
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
        add( args, po.linker_args );
        args.insert( args.begin(), "divcc" );

        return args;
    }
}
