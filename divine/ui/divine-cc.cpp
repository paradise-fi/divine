// -*- C++ -*- (c) 2016 Vladimír Štill

#include <brick-string>
#include <divine/cc/elf.hpp>
#include <divine/ui/cli.hpp>
#include <initializer_list>

namespace divine {
namespace ui {

using StringVec = std::vector< std::string >;

static void runld( std::string output, std::vector< std::string > args )
{
    auto out = output.empty() ? "a.out"s : output;

    cc::elf::linkObjects( out, args );
}

enum class OptionType { Option, File, LinkerPassThrough };

struct Option {
    Option( OptionType type, std::string value, std::vector< std::string > passThrough = { } ) :
        type( type ), value( value ), passThrough( passThrough )
    { }

    OptionType type;
    std::string value;
    std::vector< std::string > passThrough;
};

bool endsWithOneOf( std::string str, std::initializer_list< std::string > lst ) {
    for ( auto &x : lst )
        if ( brick::string::endsWith( str, x ) )
            return true;
    return false;
}

std::vector< Option > buildOptions( std::vector< std::string > args ) {
    using namespace brick::string;
    std::vector< Option > opts;

    for ( auto it = args.begin(), end = args.end(); it != end; ++it ) {

        if ( startsWith( *it, "-Xlinker" ) ) {
            opts.push_back( { OptionType::LinkerPassThrough, "", { *std::next( it ) } } );
            ++it; // argument is ignored by cc but passed to linker
        } else if ( startsWith( *it, "-Wl," ) ) {
            std::vector< std::string > pass;
            auto pos = it->find( ',' );
            do {
                auto start = pos + 1;
                pos = it->find( ',', start );
                auto end = pos == std::string::npos ? std::string::npos : pos - start - 1;
                pass.emplace_back( it->substr( start, end ) );
            } while ( pos != std::string::npos );
            opts.emplace_back( OptionType::LinkerPassThrough, "", pass );
        } else if ( *it == "-z" ) {
            opts.push_back( { OptionType::LinkerPassThrough, "", { *it, *std::next( it ) } } );
            ++it; // again, arg. ignored by cc
        } else if ( *it == "-s" )
            opts.push_back( { OptionType::LinkerPassThrough, "", { *it } } );
        else if ( startsWith( *it, "-l" ) || startsWith( *it, "-L" ) ) {
            if ( it->size() == 2 ) {
                opts.push_back( { OptionType::LinkerPassThrough, "", { *it, *std::next( it ) } } );
                ++it;
            } else
                opts.push_back( { OptionType::LinkerPassThrough, "", { *it } } );
        } else if ( startsWith( *it, "-" ) )
            opts.emplace_back( OptionType::Option, *it );
        // note: there should be no options which can have file as an
        // space-separated argument apart from -o and -l which are threated
        // specially
        else if ( brick::fs::exists( *it )
                  || endsWithOneOf( *it, { ".c", ".cc", ".cpp", ".C", ".bc", ".ll", ".i", ".ii", ".s", ".S", ".o" } ) )
            opts.emplace_back( OptionType::File, *it );
        else
            opts.emplace_back( OptionType::Option, *it );
    }

    return opts;
}

void DivineCc::run() {

    auto opts = buildOptions( _flags );
    std::vector< std::string > flags;
    int filesCount = 0;
    for ( auto &o : opts ) {
        if ( o.type == OptionType::Option )
            flags.emplace_back( o.value );
        if ( o.type == OptionType::File )
            ++filesCount;
    }

    cc::Compiler cc;

    if ( _dontLink ) {
        if ( !_output.empty() && filesCount > 1 )
            die( "Error: -o cannot be specified if multiple files are given together with -c" );
        for ( auto &o : opts ) {
            if ( o.type == OptionType::File ) {
                auto out = _output.empty() ? brick::fs::replaceExtension( o.value, "o" ) : _output;
                cc::elf::compile( cc, o.value, out, flags );
            }
        }
    } else {
        brick::fs::TempDir workdir( ".divine.compile.XXXXXX", brick::fs::AutoDelete::Yes,
                                    brick::fs::UseSystemTemp::Yes );
        std::vector< std::string > toLink;
        int i = 0;
        for ( auto &o : opts ) {
            if ( o.type == OptionType::File && !brick::string::endsWith( o.value, ".o" ) ) {
                auto out = brick::fs::joinPath( workdir, o.value + "."s + std::to_string( i ) + ".o"s );
                cc::elf::compile( cc, o.value, out, flags );
                o.passThrough = { out };
            }
        }

        std::vector< std::string > ldopts, tailldopts;
        std::tie( ldopts, tailldopts ) = cc::elf::getExecLinkOptions( _libPaths ); // TODO: detect static, needed crt files
        for ( auto &o : opts ) {
            if ( o.type == OptionType::LinkerPassThrough
                    || ( o.type == OptionType::File && !o.passThrough.empty() ) )
                std::copy( o.passThrough.begin(), o.passThrough.end(), std::back_inserter( ldopts ) );
            else if ( o.type == OptionType::File )
                ldopts.emplace_back( o.value );
        }
        std::copy( tailldopts.begin(), tailldopts.end(), std::back_inserter( ldopts ) );
        runld( _output, ldopts );
    }
}

void DivineLd::run() { runld( _output, _flags ); }

}
}

