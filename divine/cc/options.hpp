#pragma once

/*
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

#include <divine/cc/filetype.hpp>
#include <brick-cmd>

namespace divine::cc
{

    // Driver options
    struct Options
    {
        brq::cmd_flag dont_link;
        bool verbose;
        Options() : Options( false, true ) {}
        Options( bool dont_link, bool verbose ) : dont_link( dont_link ), verbose( verbose ) {}
    };

    // Parsed CLI options
    struct ParsedOpts
    {
        std::vector< std::string > opts;
        std::vector< std::string > libSearchPath;
        std::vector< FileEntry > files;
        std::string outputFile;
        std::vector< std::string > allowedPaths;
        std::vector< std::string > cc1_args;
        std::vector< std::string > linker_args;
        bool toObjectOnly = false;
        bool preprocessOnly = false;
        bool hasHelp = false;
        bool hasVersion = false;
        bool use_lld = false;
        bool shared = false;
        bool pic = false;
    };

    ParsedOpts parseOpts( std::vector< std::string > rawCCOpts );
}
