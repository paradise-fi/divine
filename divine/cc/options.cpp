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

#include <divine/cc/options.hpp>
#include <brick-string>

namespace divine::cc
{
    ParsedOpts parseOpts( std::vector< std::string > rawCCOpts )
    {
        using FT = FileType;
        FT xType = FT::Unknown;
        ParsedOpts po;

        for ( auto it = rawCCOpts.begin(), end = rawCCOpts.end(); it != end; ++it )
        {
            std::string isystem = "-isystem", inc = "-inc";
            if ( *it == "--help" )
            {
                po.hasHelp = true;
                return po;
            }
            if ( *it == "--version" )
            {
                po.hasVersion = true;
                return po;
            }
            if ( brick::string::startsWith( *it, "-I" ) )
            {
                std::string val;
                if ( it->size() > 2 )
                    val = it->substr( 2 );
                else
                    val = *++it;
                po.allowedPaths.emplace_back( val );
                po.opts.emplace_back( "-I" + val );
            }
            else if ( brick::string::startsWith( *it, isystem ) )
            {
                std::string val;
                if ( it->size() > isystem.size() )
                    val = it->substr( isystem.size() );
                else
                    val = *++it;
                po.allowedPaths.emplace_back( val );
                po.opts.emplace_back( isystem + val );
            }
            else if ( *it == "-include" )
                po.opts.emplace_back( *it ), po.opts.emplace_back( *++it );
            else if ( brick::string::startsWith( *it, "-x" ) )
            {
                std::string val;
                if ( it->size() > 2 )
                    val = it->substr( 2 );
                else
                    val = *++it;
                if ( val == "none" )
                    xType = FT::Unknown;
                else {
                    xType = typeFromXOpt( val );
                    if ( xType == FT::Unknown )
                        throw std::runtime_error( "-x value not recognized: " + val );
                }
                // this option is propagated to CC by xType, not directly
            }
            else if ( brick::string::startsWith( *it, "-l" ) )
            {
                std::string val = it->size() > 2 ? it->substr( 2 ) : *++it;
                po.files.emplace_back( Lib::InPlace(), val );
            }
            else if ( brick::string::startsWith( *it, "-L" ) )
            {
                std::string val = it->size() > 2 ? it->substr( 2 ) : *++it;
                po.allowedPaths.emplace_back( val );
                po.libSearchPath.emplace_back( std::move( val ) );
            }
            else if ( brick::string::startsWith( *it, "-o" ))
            {
                std::string val;
                if ( it->size() > 2 )
                    val = it->substr( 2 );
                else
                    val = *++it;
                po.outputFile = val;
            }
            else if ( !brick::string::startsWith( *it, "-" ) )
                po.files.emplace_back( File::InPlace(), *it, xType == FT::Unknown ? typeFromFile( *it ) : xType );
            else if ( *it == "-c" )
                po.toObjectOnly = true;
            else if ( *it == "-E" )
                po.preprocessOnly = true;
            else if ( *it == "-shared" )
                po.shared = true;
            else if ( *it == "--use-lld" )
                po.use_lld = true;
            else if ( *it == "-g" )
                po.opts.emplace_back( "-debug-info-kind=standalone" );
            else
                po.opts.emplace_back( *it );
        }

        return po;
    }
}
