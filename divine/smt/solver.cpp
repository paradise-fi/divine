#include <divine/smt/solver.hpp>
#include <divine/smt/builder.hpp>
#include <divine/vm/memory.hpp>
#include <brick-smt>
#include <brick-proc>
#include <brick-bitlevel>

using namespace divine::smt::builder;

namespace divine::smt::solver
{

namespace smt = brick::smt;
namespace proc = brick::proc;

bool match( const Constant &a, const Constant &b )
{
    const auto mask = brick::bitlevel::ones< uint64_t >( a.bitwidth() );
    return a.bitwidth() == b.bitwidth() && ( a.value & mask ) == ( b.value & mask );
}

Result SMTLib::solve()
{
    auto b = builder( 'z' - 'a' );
    auto q = b.constant( true );

    for ( auto clause : _asserts )
        q = builder::mk_bin( b, Op::And, 1, q, clause );

    auto r = brick::proc::spawnAndWait( proc::StdinString( _ctx.query( q ) ) | proc::CaptureStdout |
                                        proc::CaptureStderr, _opts );

    std::string_view result = r.out();
    if ( result.substr( 0, 5 ) == "unsat" )
        return Result::False;
    if ( result.substr( 0, 3 ) == "sat" )
        return Result::True;
    if ( result.substr( 0, 7 ) == "unknown" )
        return Result::Unknown;

    std::cerr << "E: The SMT solver produced an error: " << r.out() << std::endl
              << "E: The input formula was: " << std::endl
              << _ctx.query( q ) << std::endl;
    UNREACHABLE( "Invalid SMT reply" );
}

template< typename Core, typename Node >
Op equality( const Node& node ) noexcept
{
    if constexpr ( std::is_same_v< Core, STP > )
        return Op::Eq;
    else
        return node.is_bv() || node.is_bool() ? Op::Eq : Op::FpOEQ;
}

template< typename Core >
bool Simple< Core >::equal( vm::HeapPointer path, SymPairs &sym_pairs,
                            vm::CowHeap &h_1, vm::CowHeap &h_2 )
{
    this->reset();
    auto e_1 = this->extract( h_1, 1 ), e_2 = this->extract( h_2, 2 );
    auto b = this->builder();

    auto v_eq = b.constant( true );
    auto c_1_rpn = e_1.read_constraints( path ),
         c_2_rpn = e_2.read_constraints( path );
    auto c_1 = c_1_rpn.size() == 1 ? b.constant( true ) : evaluate( e_1, c_1_rpn ),
         c_2 = c_2_rpn.size() == 1 ? b.constant( true ) : evaluate( e_2, c_2_rpn );

    for ( auto [lhs, rhs] : sym_pairs )
    {
        auto f_1 = e_1.read( lhs );
        auto f_2 = e_2.read( rhs );

        auto v_1 = evaluate( e_1, f_1 );
        auto v_2 = evaluate( e_2, f_2 );

        Op op = equality< Core >( v_1 );
        auto pair_eq = mk_bin( b, op, 1, v_1, v_2 );
        v_eq = mk_bin( b, Op::And, 1, v_eq, pair_eq );
    }

    /* we already know that both constraint sets are sat */
    auto c_eq = mk_bin( b, Op::Eq, 1, c_1, c_2 ),
      pc_fail = mk_un(  b, Op::Not, 1, c_1 ),
       v_eq_c = mk_bin( b, Op::Or, 1, pc_fail, v_eq ),
           eq = mk_bin( b, Op::And, 1, c_eq, v_eq_c );

    this->add( mk_un( b, Op::Not, 1, eq ) );
    auto r = this->solve();
    this->reset();
    return r == Result::False;
}

template< typename Core >
bool Simple< Core >::feasible( vm::CowHeap & heap, vm::HeapPointer ptr )
{
    this->reset();
    auto e = this->extract( heap, 1 );
    auto b = this->builder();
    auto query = evaluate( e, e.read( ptr ) );
    this->add( mk_bin( b, Op::Eq, 1, query, b.constant( 1, 1 ) ) );
    return this->solve() != Result::False;
}

template< typename Core >
bool Caching< Core >::feasible( vm::CowHeap &heap, vm::HeapPointer ptr )
{
    if ( _sat.count( ptr ) )
        return _hits[ ptr ] ++, _sat[ ptr ];

    auto rv = Simple< Core >::feasible( heap, ptr );
    return _sat[ ptr ] = rv;
}

template< typename Core >
bool Incremental< Core >::feasible( vm::CowHeap &/*heap*/, vm::HeapPointer /*ptr*/ )
{
#if 0
    auto e = this->extract( heap );
    auto query = e.constant( true );
    std::unordered_set< vm::HeapPointer > in_context{ _inc.begin(), _inc.end() };
    auto head = ptr;

    while ( !ptr.null() && !in_context.count( ptr ) )
    {
        auto f = e.read( ptr );
        auto clause = e.convert( f->binary.left );
        query = mk_bin( e, Op::And, 1, query, clause );
        ptr = f->binary.right;
    }

    if ( head == ptr ) /* no new fragments */
        return true;

    while ( !_inc.empty() && _inc.back() != ptr )
    {
        _inc.pop_back();
        this->pop();
        this->pop();
    }

    this->push();
    this->add( query );
    auto result = this->solve();

    if ( result == Result::True )
    {
        this->push();
        _inc.push_back( head );
    }
    else
        this->pop();

    return result != Result::False;
    #endif
}

#if OPT_STP

void STP::pop()
{
    _mgr.Pop();
    clear();
}

void STP::push()
{
    _mgr.Push();
}

void STP::reset()
{
    while ( _mgr.getAssertLevel() )
        _mgr.Pop();
    clear();
}

Result STP::solve()
{
    stp::ASTNode top;
    auto vec = _mgr.GetAsserts();
    switch ( vec.size() )
    {
        case 0: top = _mgr.ASTTrue; break;
        case 1: top = vec[ 0 ]; break;
        default: top = _mgr.CreateNode( stp::AND, vec ); break;
    }
    switch ( _stp.TopLevelSTP( top, _mgr.ASTFalse ) )
    {
        case stp::SOLVER_UNSATISFIABLE: return Result::False;
        case stp::SOLVER_SATISFIABLE: return Result::True;
        default: return Result::Unknown;
    }
}

void STP::clear()
{
    _mgr.ClearAllTables();
    _stp.ClearAllTables();
}

#endif

template struct Simple< SMTLib >;
#if OPT_Z3
template struct Simple< Z3 >;
template struct Incremental< Z3 >;
template struct Caching< Z3 >;
#endif

#if OPT_STP
template struct Simple< STP >;
template struct Incremental< STP >;
template struct Caching< STP >;
#endif

}
