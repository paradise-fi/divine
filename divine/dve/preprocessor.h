// -*- C++ -*- (c) 2013 Jan Kriho

#ifndef DIVINE_DVE_PREPROCESSOR_H
#define DIVINE_DVE_PREPROCESSOR_H

#include <divine/dve/parse.h>
#include <string>
#include <memory>
#include <unordered_map>

namespace divine {
namespace dve {

namespace preprocessor {

struct Definition {
    std::string var, value;
    std::shared_ptr< dve::Lexer< dve::IOStream > > lexer;
    std::shared_ptr< dve::IOStream > stream;
    std::shared_ptr< std::stringstream > realstream;

    Definition( std::string def ) {
        size_t pos = def.find( "=" );
        if ( pos == std::string::npos ) {
            throw std::string( "Invalid definition: " + def );
        }
        var = def.substr( 0, pos );
        value = def.substr( pos + 1, std::string::npos );

        realstream.reset( new std::stringstream() );
        (*realstream) << value;
        stream.reset( new dve::IOStream( *realstream ) );
        lexer.reset( new dve::Lexer< dve::IOStream >( *stream ) );
    }

    Definition() {}
};

struct Initialiser : Parser {
    std::vector< parse::Expression > initial;

    void initList() {
        list< parse::Expression >( std::back_inserter( initial ),
                                   Token::BlockOpen, Token::Comma,
                                   Token::BlockClose );
    }

    Initialiser( Context &c ) : Parser( c ) {
        if ( maybe( &Initialiser::initList ) )
            return;
        initial.push_back( parse::Expression( context() ) );
    }
};

struct System {
    std::unordered_map< std::string, Definition > & defs;

    void process( parse::System & ast ) {
        for ( parse::Declaration & decl : ast.decls ) {
            if ( decl.is_input ) {
                if ( defs.count( decl.name ) ) {
                    Initialiser init( ast.context().createChild( *defs[ decl.name ].lexer, defs[ decl.name ].var ) );
                    decl.initial = init.initial;
                    decl.is_const = true;
                    decl.is_input = false;
                }
                else if ( decl.initial.size() ) {
                    decl.is_const = true;
                    decl.is_input = false;
                }
            }
        }
    }

    System( std::unordered_map< std::string, Definition > &defs ) : defs( defs ) {}
};

}

}
}

#endif