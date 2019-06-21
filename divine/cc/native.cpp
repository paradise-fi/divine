// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Zuzana Baranová <xbaranov@fi.muni.cz>
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

#include <divine/cc/driver.hpp>
#include <divine/cc/codegen.hpp>
#include <divine/cc/link.hpp>
#include <divine/cc/native.hpp>
#include <divine/cc/options.hpp>

DIVINE_RELAX_WARNINGS
#include "lld/Common/Driver.h"
DIVINE_UNRELAX_WARNINGS

#include <brick-proc>

namespace divine::cc
{
    int Native::compile_files()
    {
        for ( auto file : _files )
        {
            if ( file.first == "lib" )
                continue;
            if ( cc::is_object_type( file.first ) )
                continue;
            auto mod = _clang.compile( file.first, _po.opts );
            cc::emit_obj_file( *mod, file.second );
        }
        return 0;
    }

    void Native::init_ld_args()
    {
        if ( _ld_args.empty() )
            _ld_args = cc::ld_args( _po, _files );
    }

    void Native::link()
    {
        init_ld_args();

        auto drv = std::make_unique< cc::Driver >( _clang.context() );
        std::vector< const char * > ld_args_c;

        ld_args_c.reserve( _ld_args.size() );
        for ( size_t i = 0; i < _ld_args.size(); ++i )
            ld_args_c.push_back( _ld_args[i].c_str() );

        auto ld_job = drv->getJobs( ld_args_c ).back();
        if ( _po.use_system_ld )
        {
            ld_job.args.insert( ld_job.args.begin(), ld_job.name );
            auto r = brick::proc::spawnAndWait( brick::proc::CaptureStderr, ld_job.args );
            if ( !r )
                throw cc::CompileError( "failed to link, ld exited with " + to_string( r ) );
        }
        else
        {
            ld_job.args.insert( ld_job.args.begin(), "divcc" );
            std::vector< const char * > lld_job_c;
            lld_job_c.reserve( ld_job.args.size() );
            for ( size_t i = 0; i < ld_job.args.size(); ++i )
                lld_job_c.push_back( ld_job.args[i].c_str() );

            bool linked = lld::elf::link( lld_job_c, false );
            if ( !linked )
                throw cc::CompileError( "lld failed, not linked" );
        }
    }

    Native::~Native()
    {
        if ( !_po.toObjectOnly )
            for ( auto file : _files )
            {
                if ( cc::is_object_type( file.first ) )
                    continue;
                std::string ofn = file.second;
                unlink( ofn.c_str() );
            }
    }
}
