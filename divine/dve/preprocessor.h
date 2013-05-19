// -*- C++ -*- (c) 2013 Jan Kriho

#ifndef DIVINE_DVE_PREPROCESSOR_H
#define DIVINE_DVE_PREPROCESSOR_H

#include <divine/dve/parse.h>
#include <string>
#include <wibble/string.h>
#include <memory>
#include <unordered_map>

#include <divine/utility/buchi.h>

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
    typedef std::vector< std::pair< int, int > > BATrans;

    parse::Property LTL2Process( parse::LTL &prop ) {
        parse::Property propBA;

        std::string ltl = prop.property.data.substr( 1, prop.property.data.length() - 2 );

        Buchi ba;
        std::vector< Buchi::DNFClause > clauses;
        ba.build( ltl,
                  [&]( Buchi::DNFClause &cls )
                  { clauses.push_back( cls ); return clauses.size() - 1; } );

        propBA.name = prop.name;
        propBA.ctx = prop.ctx;
        for ( int i = 0; i < ba.size(); i++ ) {
            propBA.states.push_back( parse::Identifier( "q" + wibble::str::fmt( i ), prop.context() ) );

            if ( ba.isAccepting( i ) )
                propBA.accepts.push_back( parse::Identifier( "q" + wibble::str::fmt( i ), prop.context() ) );

            const BATrans &batrans = ba.transitions( i );
            for ( std::pair< int, int > bt : batrans ) {
                parse::Transition t;
                t.ctx = prop.ctx;
                t.proc = prop.name;
                t.from = parse::Identifier( "q" + wibble::str::fmt( i ), prop.context() );
                t.to = parse::Identifier( "q" + wibble::str::fmt( bt.first ), prop.context() );
                for ( std::pair< bool, std::string > &item : clauses[ bt.second ] ) {
                    t.guards.push_back(
                        parse::Expression(
                            ( item.first ? "" : "not ") + item.second + "()",
                            prop.context() ) );
                }
                propBA.trans.push_back( t );
            }
        }
        propBA.inits.push_back( parse::Identifier( "q" + wibble::str::fmt( ba.getInitial() ), prop.context() ) );

        return propBA;
    }

    void processInput( parse::Declaration & decl ) {
        if ( defs.count( decl.name ) ) {
            Initialiser init( decl.context().createChild( *defs[ decl.name ].lexer, defs[ decl.name ].var ) );
            decl.initial = init.initial;
            decl.is_const = true;
            decl.is_input = false;
        }
        else if ( decl.initial.size() ) {
            decl.is_const = true;
            decl.is_input = false;
        }
    }

    void process( parse::System & ast ) {
        for ( parse::Declaration & decl : ast.decls ) {
            if ( decl.is_input ) {
                processInput( decl );
            }
        }
        for ( parse::LTL & ltl : ast.ltlprops ) {
            ast.properties.push_back( LTL2Process( ltl ) );
        }
    }

    System( std::unordered_map< std::string, Definition > &defs ) : defs( defs ) {}
};

}

}
}

#endif