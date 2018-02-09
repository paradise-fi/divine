#include <divine/smt/solver.hpp>
#include <divine/smt/builder.hpp>
#include <brick-smt>
#include <brick-proc>
#include <brick-bitlevel>

namespace divine::smt::solver
{

namespace smt = brick::smt;
namespace proc = brick::proc;

bool match( sym::Constant &a, sym::Constant &b )
{
    const auto mask = brick::bitlevel::ones< uint64_t >( a.type.bitwidth() );
    return a.type.bitwidth() == b.type.bitwidth() && ( a.value & mask ) == ( b.value & mask );
}

Result SMTLib::solve()
{
    auto b = builder( 'z' - 'a' );
    auto q = b.constant( true );

    for ( auto clause : _asserts )
        builder::mk_bin( b, sym::Op::And, 1, q, clause );

    std::stringstream str;
    str << "(assert " << _ctx.str( q ) << ")" << std::endl << "(check-sat)";

    auto r = brick::proc::spawnAndWait( proc::StdinString( str.str() ) | proc::CaptureStdout |
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
              << str.str() << std::endl;
    UNREACHABLE( "Invalid SMT reply" );
}

template< typename Extract >
auto get_pc( Extract &e, vm::HeapPointer ptr )
{
    auto query = e.constant( true );

    while ( !ptr.null() )
    {
        auto f = e.read( ptr );
        auto clause = e.convert( f->binary.left );
        query = builder::mk_bin( e, sym::Op::And, 1, query, clause );
        ptr = f->binary.right;
    }

    return query;
}

template< typename Core >
bool Simple< Core >::equal( SymPairs &sym_pairs, vm::CowHeap &h_1, vm::CowHeap &h_2 )
{
    this->reset();
    auto e_1 = this->extract( h_1, 1 ), e_2 = this->extract( h_2, 2 );
    auto b = this->builder();

    auto v_eq = b.constant( true );
    auto c_1 = e_1.constant( true ), c_2 = e_2.constant( true );
    bool constraints_found = false;

    using namespace builder;

    for ( auto p : sym_pairs )
    {
        auto f_1 = e_1.read( p.first ), f_2 = e_2.read( p.second );
        if ( f_1->op() == sym::Op::Constraint )
        {
            ASSERT( !constraints_found );
            ASSERT_EQ( int( f_2->op() ), int( sym::Op::Constraint ) );
            constraints_found = true;
            c_1 = get_pc( e_1, p.first );
            c_2 = get_pc( e_2, p.second );
        }
        else
        {
            auto v_1 = e_1.convert( p.first ), v_2 = e_2.convert( p.second );
            auto pair_eq = mk_bin( b, sym::Op::EQ, 1, v_1, v_2 );
            v_eq = mk_bin( b, sym::Op::And, 1, v_eq, pair_eq );
        }
    }


    /* we already know that both constraint sets are sat */
    auto c_eq = mk_bin( b, sym::Op::EQ, 1, c_1, c_2 ),
      pc_fail = mk_un( b, sym::Op::BoolNot, 1, c_1 ),
       v_eq_c = mk_bin( b, sym::Op::Or, 1, pc_fail, v_eq ),
           eq = mk_bin( b, sym::Op::And, 1, c_eq, v_eq_c );

    this->add( mk_un( b, sym::Op::BoolNot, 1, eq ) );
    auto r = this->solve();
    this->reset();
    return r == Result::False;
}

template< typename Core >
bool Simple< Core >::feasible( vm::CowHeap & heap, vm::HeapPointer ptr )
{
    this->reset();
    auto e = this->extract( heap );
    auto query = get_pc( e, ptr );
    this->add( query );
    return this->solve() != Result::False;
}

template< typename Core >
bool Incremental< Core >::feasible( vm::CowHeap &heap, vm::HeapPointer ptr )
{
    auto e = this->extract( heap );
    auto query = e.constant( true );
    std::unordered_set< vm::HeapPointer > in_context{ _inc.begin(), _inc.end() };
    auto head = ptr;

    while ( !ptr.null() && !in_context.count( ptr ) )
    {
        auto f = e.read( ptr );
        auto clause = e.convert( f->binary.left );
        query = mk_bin( e, sym::Op::And, 1, query, clause );
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
}

template struct Simple< SMTLib >;
#if OPT_Z3
template struct Simple< Z3 >;
template struct Incremental< Z3 >;
#endif

}
