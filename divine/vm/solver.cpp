#include <divine/vm/solver.hpp>
#include <divine/vm/formula.hpp>
#include <brick-smt>
#include <brick-proc>
#include <brick-bitlevel>

namespace divine::vm
{

namespace smt = brick::smt;
namespace proc = brick::proc;

using Result = Solver::Result;

namespace {

template< typename FormulaMap >
sym::Formula *stripAssumes( sym::Formula *f, FormulaMap &m )
{
    while ( f->op() == sym::Op::Assume )
        f = m.hp2form( f->assume.value );
    return f;
}

bool match( sym::Constant &a, sym::Constant &b )
{
    const auto mask = brick::bitlevel::ones< uint64_t >( a.type.bitwidth() );
    return a.type.bitwidth() == b.type.bitwidth() && (a.value & mask) == (b.value & mask);
}

#if OPT_Z3
static inline Result z3_solver_result( z3::check_result res )
{
    switch ( res )
    {
        case z3::check_result::unsat:   return Result::False;
        case z3::check_result::sat:     return Result::True;
        case z3::check_result::unknown: return Result::Unknown;
    }
}
#endif

} // anonymous namespace

Result SMTLibSolver::query( const std::string & formula ) const
{
    auto r = brick::proc::spawnAndWait( proc::StdinString( formula ) | proc::CaptureStdout |
                                        proc::CaptureStderr, options() );

    std::string_view result = r.out();
    if ( result.substr( 0, 5 ) == "unsat" )
        return Result::False;
    if ( result.substr( 0, 3 ) == "sat" )
        return Result::True;
    if ( result.substr( 0, 7 ) == "unknown" )
        return Result::Unknown;

    std::cerr << "E: The SMT solver produced an error: " << r.out() << std::endl
              << "E: The input formula was: " << std::endl
              << formula << std::endl;
    UNREACHABLE( "Invalid SMT reply" );
}

Result SMTLibSolver::equal( SymPairs &sym_pairs, CowHeap &h1, CowHeap &h2 ) const
{
    using FormulaMap = SMTLibFormulaMap;
    std::stringstream formula;
    std::unordered_set< int > indices;
    FormulaMap m1( h1, indices, formula, "_1" ), m2( h2, indices, formula, "_2" );

    for ( auto p : sym_pairs ) {
        m1.convert( p.first );
        m2.convert( p.second );
    }

    m1.pathcond();
    m2.pathcond();

    smt::Vector valeq;
    for ( auto p : sym_pairs )
    {
        // assumes are encoded in the path condition
        auto f1 = stripAssumes( m1.hp2form( p.first ), m1 ),
             f2 = stripAssumes( m2.hp2form( p.second ), m2 );

        if ( f1->op() == sym::Op::Variable && f2->op() == sym::Op::Variable )
        {
            if ( f1->var.id != f2->var.id )
                return Result::False;
            else
                continue;
        }

        if ( f1->op() == sym::Op::Constant && f2->op() == sym::Op::Constant )
        {
            if ( match( f1->con, f2->con ) )
                continue;
            else
                return Result::False;
        }

        valeq.emplace_back( smt::binop< smt::Op::Eq >( smt::symbol( m1[ p.first ] ),
                                                       smt::symbol( m2[ p.second ] ) ) );
    }

    formula << smt::assume( smt::unop< smt::Op::Not >(
                                   smt::binop< smt::Op::And >(
                                       smt::binop< smt::Op::Eq >( smt::symbol( "pathcond_1" ),
                                                                  smt::symbol( "pathcond_2" ) ),
                                       smt::binop< smt::Op::Implies >( smt::symbol( "pathcond_1" ),
                                                                       smt::bigand( valeq ) ) ) ) )
               << std::endl;

    formula << "(check-sat)" << std::endl;
    return query( formula.str() ) == Result::False ? Result::True : Result::False;
}

Result SMTLibSolver::feasible( CowHeap & heap, HeapPointer assumes ) const
{
    using FormulaMap = SMTLibFormulaMap;
    std::stringstream formula;
    std::unordered_set< int > indices;
    FormulaMap map( heap, indices, formula );

    while ( !assumes.null() )
    {
        value::Pointer constraint, next;
        heap.read_shift( assumes, constraint );
        heap.read( assumes, next );

        formula << smt::assume( smt::symbol( map.convert( constraint.cooked() ) ) ) << std::endl;
        assumes = next.cooked();
    }
    formula << "(check-sat)" << std::endl;
    return query( formula.str() );
}

#if OPT_Z3
Result Z3Solver::equal( SymPairs &sym_pairs, CowHeap &h1, CowHeap &h2 )
{
    using FormulaMap = Z3FormulaMap;

    FormulaMap m1( h1, ctx ), m2( h2, ctx );


    for ( const auto & p : sym_pairs ) {
        m1.convert( p.first );
        m2.convert( p.second );
    }

    auto pc1 = m1.pathcond();
    auto pc2 = m2.pathcond();

    z3::expr_vector valeq( ctx );
    for ( const auto& p : sym_pairs ) {
        auto f1 = stripAssumes( m1.hp2form( p.first ), m1 );
        auto f2 = stripAssumes( m2.hp2form( p.second ), m2 );

        if ( f1->op() == sym::Op::Variable && f2->op() == sym::Op::Variable )
        {
            if ( f1->var.id != f2->var.id )
                return Result::False;
            else
                continue;
        }

        if ( f1->op() == sym::Op::Constant && f2->op() == sym::Op::Constant )
        {
            if ( match( f1->con, f2->con ) )
                continue;
            else
                return Result::False;
        }

        valeq.push_back( m1[ p.first ] == m2[ p.second ] );
    }

    solver.reset();
    solver.add( !( ( pc1 == pc2 ) && z3::implies( pc1, z3::mk_and( valeq ) ) ) );
    return z3_solver_result( solver.check() ) == Result::False ? Result::True : Result::False;
}

Result Z3Solver::feasible( CowHeap &heap, HeapPointer assumes )
{
    using FormulaMap = Z3FormulaMap;
    FormulaMap map( heap, ctx );

    z3::expr pc = ctx.bool_val( true );

    try {
        while ( !assumes.null() ) {
            value::Pointer constraint, next;
            heap.read_shift( assumes, constraint );
            heap.read( assumes, next );
            auto c = map.convert( constraint.cooked() );

            if ( c.is_bv() ) {
                ASSERT_EQ( c.get_sort().bv_size(), 1 );
                pc = pc && ( c == ctx.bv_val( 1, 1 ) );
            } else {
                pc = pc && c;
            }

            assumes = next.cooked();
        }

        solver.reset();
        solver.add( pc );
        return z3_solver_result( solver.check() );
    }
    catch ( const z3::exception &e ) {
        std::cerr << "Cannot preform feasibility check: " << e.msg() << std::endl;
        throw e;
    }
}
#endif

}
