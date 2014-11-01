#include <divine/dve/parse.h>
#include <divine/dve/symtab.h>
#include <divine/dve/expression.h>
using namespace divine;
const std::string *dve::Token::tokenName = dve::tokenName;

using namespace divine::dve::parse;
using namespace brick::string;

void Declaration::fold( dve::SymTab* symtab )
{
    if ( sizeExpr.valid() ) {
        dve::Expression e( *symtab, sizeExpr );
        EvalContext ctx;
        setSize( e.evaluate( ctx ) );
    }

    initial.clear();
    for ( Expression &expr : initialExpr ) {
        dve::Expression e( *symtab, expr );
        EvalContext ctx;
        initial.push_back( e.evaluate( ctx ) );
    }

    if ( is_const ) {
        while ( initial.size() > static_cast< unsigned >( size ) )
            initial.push_back( 0 );
        symtab->constant( dve::NS::Variable, name, initial );
    }
}

void ChannelDeclaration::fold( dve::SymTab* symtab )
{
    if ( sizeExpr.valid() ) {
        dve::Expression e( *symtab, sizeExpr );
        EvalContext ctx;
        setSize( e.evaluate( ctx ) );
    }
}

void Automaton::Body::fold( dve::SymTab* parent )
{
    dve::SymTab symtab( parent );
    for ( Declaration &decl : decls )
        decl.fold( &symtab );
    for ( ChannelDeclaration &decl : chandecls )
        decl.fold( &symtab );
}


void System::fold()
{
    dve::SymTab symtab;
    for ( Declaration &decl : decls )
        decl.fold( &symtab );
    for ( ChannelDeclaration &decl : chandecls )
        decl.fold( &symtab );

    for ( Automaton &proc : processes )
        proc.fold( &symtab );
    for ( Automaton &proc : properties )
        proc.fold( &symtab );
}
