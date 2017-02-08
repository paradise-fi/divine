#include <divine/vm/explore.hpp>
#include <divine/vm/formula.hpp>
#include <brick-smt>
#include <brick-proc>
#include <brick-bitlevel>

namespace divine {
namespace vm {
namespace explore {

namespace smt = brick::smt;
namespace proc = brick::proc;

sym::Formula *stripAssumes( sym::Formula *f, FormulaMap &m )
{
    while ( f->op == sym::Op::Assume )
        f = m.hp2form( f->assume.value );
    return f;
}

bool match( sym::Constant &a, sym::Constant &b )
{
    const auto mask = brick::bitlevel::ones< uint64_t >( a.type.bitwidth() );
    return a.type.bitwidth() == b.type.bitwidth() && (a.value & mask) == (b.value & mask);
}

enum class Result { False, True, Unknown };

Result runSolver( std::string input )
{
    auto r = brick::proc::spawnAndWait( proc::StdinString( input ) | proc::CaptureStdout |
                                        proc::CaptureStderr, "z3", "-in", "-smt2" );
    // alternatively: "boolector", "--smt2"

    std::string_view result = r.out();
    if ( result.substr( 0, 5 ) == "unsat" )
        return Result::False;
    if ( result.substr( 0, 3 ) == "sat" )
        return Result::True;
    if ( result.substr( 0, 7 ) == "unknown" )
        return Result::Unknown;

    std::cerr << "E: SMT solver produced error: " << r.out() << std::endl
              << "E: Input formula was: " << std::endl
              << input << std::endl;
    UNREACHABLE( "Invalid SMT reply" );
}

bool SymbolicHasher::smtEqual( SymPairs &symPairs ) const
{
    std::unordered_set< int > inputs;
    std::stringstream smtFormula;
    FormulaMap m1( h1, "_1", inputs, smtFormula ), m2( h2, "_2", inputs, smtFormula );

    for ( auto p : symPairs ) {
        m1.convert( p.first );
        m2.convert( p.second );
    }

    m1.pathcond();
    m2.pathcond();

    smt::Vector valeq;
    for ( auto p : symPairs )
    {
        // assumes are encoded in the path condition
        auto f1 = stripAssumes( m1.hp2form( p.first ), m1 ),
             f2 = stripAssumes( m2.hp2form( p.second ), m2 );

        if ( f1->op == sym::Op::Variable && f2->op == sym::Op::Variable )
        {
            if ( f1->var.id != f2->var.id )
                return false;
            else
                continue;
        }

        if ( f1->op == sym::Op::Constant && f2->op == sym::Op::Constant )
        {
            if ( match( f1->con, f2->con ) )
                continue;
            else
                return false;
        }

        valeq.emplace_back( smt::binop< smt::Op::Eq >( smt::symbol( m1[ p.first ] ),
                                                       smt::symbol( m2[ p.second ] ) ) );
    }

    smtFormula << smt::assume( smt::unop< smt::Op::Not >(
                                   smt::binop< smt::Op::And >(
                                       smt::binop< smt::Op::Eq >( smt::symbol( "pathcond_1" ),
                                                                  smt::symbol( "pathcond_2" ) ),
                                       smt::binop< smt::Op::Implies >( smt::symbol( "pathcond_1" ),
                                                                       smt::bigand( valeq ) ) ) ) )
               << std::endl;

    smtFormula << "(check-sat)" << std::endl;

    return runSolver( smtFormula.str() ) == Result::False;
}

void SymbolicContext::traceAlg( TraceAlg ta )
{
    std::unordered_set< int > inputs;
    std::stringstream smtFormula;
    FormulaMap map( heap(), "", inputs, smtFormula );

    ASSERT_EQ( ta.args.size(), 1 );

    HeapPointer assumes = ta.args[0];
    while ( !assumes.null() )
    {
        value::Pointer constraint, next;
        heap().read_shift( assumes, constraint );
        heap().read( assumes, next );

        smtFormula << smt::assume( smt::symbol( map.convert( constraint.cooked() ) ) ) << std::endl;
        assumes = next.cooked();
    }
    smtFormula << "(check-sat)" << std::endl;

    if ( runSolver( smtFormula.str() ) == Result::False )
        set( _VM_CR_Flags, get( _VM_CR_Flags ).integer | _VM_CF_Cancel );
}

}
}
}
