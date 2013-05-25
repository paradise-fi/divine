#include <divine/dve/preprocessor.h>

namespace divine {
namespace dve {
namespace preprocessor {

parse::Identifier getProcRef( parse::MacroNode& mnode, Definitions &defs, Macros &macros, SymTab &symtab, const Substitutions &substs )
{
    std::vector< int > values;
    for ( parse::Expression *e : mnode.params ) {
        Expression( defs, macros, *e, symtab, substs );
        dve::Expression ex( symtab, *e );
        EvalContext ctx;
        values.push_back( ex.evaluate( ctx ) );
    }
    std::stringstream str;
    str << mnode.name.name() << "(";
    bool tail = false;
    for ( int &v : values ) {
        if ( tail )
            str << ",";
        str << v;
        tail = true;
    }
    str << ")";
    return parse::Identifier( str.str(), mnode.context() );
}

}}}