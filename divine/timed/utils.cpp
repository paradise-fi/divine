#include <brick-string.h>
#include <divine/timed/utils.h>

namespace divine {
namespace timed {

UTAP::expression_t negIneq( const UTAP::expression_t& expr ) {
    ASSERT_EQ( expr.getSize(), 2u );
    UTAP::Constants::kind_t negOp;
    switch ( expr.getKind() ) {
    case UTAP::Constants::LT:
        negOp =  UTAP::Constants::GE;
        break;
    case UTAP::Constants::LE:
        negOp =  UTAP::Constants::GT;
        break;
    case UTAP::Constants::GT:
        negOp =  UTAP::Constants::LE;
        break;
    case UTAP::Constants::GE:
        negOp =  UTAP::Constants::LT;
        break;
    case UTAP::Constants::EQ:
        negOp =  UTAP::Constants::NEQ;
        break;
    case UTAP::Constants::NEQ:
        negOp =  UTAP::Constants::EQ;
        break;
    default:
        ASSERT_UNREACHABLE_F( "unexpected expression kind %d", expr.getKind() );
    }
    return UTAP::expression_t::createBinary( negOp, expr[ 0 ], expr[ 1 ], UTAP::position_t(), expr.getType() );
}

bool addCut( const UTAP::expression_t& expr, int pId, std::vector< Cut >& cuts ) {
    ASSERT_EQ( expr.getSize(), 2u );
    if ( expr.getKind() == UTAP::Constants::EQ || expr.getKind() == UTAP::Constants::NEQ ) {
        // we need two cuts to express equality or inequality
        bool ret1 =
            addCut( UTAP::expression_t::createBinary( UTAP::Constants::GE, expr[ 0 ], expr[ 1 ], UTAP::position_t(), expr.getType() ), pId, cuts );
        bool ret2 =
            addCut( UTAP::expression_t::createBinary( UTAP::Constants::LE, expr[ 0 ], expr[ 1 ], UTAP::position_t(), expr.getType() ), pId, cuts );
        return ret1 || ret2;
    }
    UTAP::expression_t neg = negIneq( expr );
    if ( neg.empty() )
        return false;
    auto cut = cuts.begin();
    for ( ; cut != cuts.end(); ++cut )  // look for duplicities (O(n^2), but called only when processing declarations)
        if ( expr.equal( cut->pos ) || expr.equal( cut->neg ) ) break;
    if ( cut == cuts.end() ) {  // no equal expression was found
        cuts.push_back( Cut( expr, neg, pId ) );
        return true;
    }
    return false;
}

}
}
