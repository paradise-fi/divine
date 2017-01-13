#include <divine/vm/explore.hpp>
#include <brick-smt>
#include <brick-proc>
#include <brick-bitlevel>

namespace divine {
namespace vm {
namespace explore {

struct FormulaMap {

    sym::Formula *hp2form( HeapPointer ptr ) {
        return reinterpret_cast< sym::Formula * >( heap.unsafe_bytes( ptr ).begin() );
    }

    FormulaMap( CowHeap &heap, std::string suff, std::unordered_set< int > &inputs, std::ostream &out ) :
        heap( heap ), inputs( inputs ), suff( suff ), out( out )
    { }

    static smt::Printer type( int bitwidth ) {
        return bitwidth == 1 ? smt::type( "Bool" ) : smt::bitvecT( bitwidth );
    }

    std::string_view convert( HeapPointer ptr ) {

        auto it = ptr2Sym.find( ptr );
        if ( it != ptr2Sym.end() )
            return it->second;

        auto formula = hp2form( ptr );
        int bw = formula->type.bitwidth();

        switch ( formula->op ) {
            case sym::Op::Variable: {
                it = ptr2Sym.emplace( ptr, "var_"s + std::to_string( formula->var.id ) ).first;
                if ( inputs.insert( formula->var.id ).second )
                    out << smt::declareConst( it->second, type( bw ) )
                        << std::endl;
                return it->second;
                }
            case sym::Op::Constant:
                it = ptr2Sym.emplace( ptr, to_string(
                             bw == 1 ? (formula->con.value ? smt::symbol( "true" ) : smt::symbol( "false" ))
                                     : smt::bitvec( formula->con.value, bw ) ) ).first;
                return it->second;
            case sym::Op::Assume:
                it = ptr2Sym.emplace( ptr, std::string( convert( formula->assume.value ) ) ).first;
                convert( formula->assume.constraint );
                pcparts.emplace( formula->assume.constraint );
                return it->second;
            default: ;
        };

        smt::Printer op;
        if ( sym::isUnary( formula->op ) ) {
            sym::Unary &unary = formula->unary;
            auto arg = smt::symbol( convert( unary.child ) );
            int childbw = hp2form( unary.child )->type.bitwidth();

            switch ( unary.op ) {
                case sym::Op::Trunc:
                    ASSERT_LT( bw, childbw );
                    op = smt::extract( bw - 1, 0, arg );
                    if ( bw == 1 )
                        op = smt::binop< smt::Op::Eq >( op, smt::bitvec( 1, 1 ) );
                    break;
                case sym::Op::ZExt:
                    ASSERT_LT( childbw, bw );
                    op = childbw > 1
                          ? smt::binop< smt::Op::Concat >( smt::bitvec( 0, bw - childbw ), arg )
                          : smt::ite( arg, smt::bitvec( 0, bw ), smt::bitvec( 1, bw ) );
                    break;
                case sym::Op::SExt:
                    ASSERT_LT( childbw, bw );
                    op = childbw > 1
                          ? smt::binop< smt::Op::Concat >(
                              smt::ite( smt::binop< smt::Op::Eq >( smt::bitvec( 1, 1 ),
                                                                   smt::extract( childbw - 1, childbw - 1, arg ) ),
                                        smt::unop< smt::Op::BvNot >( smt::bitvec( 0, bw - childbw ) ),
                                        smt::bitvec( 0, bw - childbw ) ),
                              arg )
                          : smt::ite( arg, smt::bitvec( 0, bw ), smt::unop< smt::Op::Not >( smt::bitvec( 0, bw ) ) );
                    break;
                case sym::Op::BoolNot:
                    ASSERT_EQ( childbw, bw );
                    ASSERT_EQ( bw, 1 );
                    op = smt::unop< smt::Op::Not >( arg );
                    break;
                default:
                    UNREACHABLE_F( "unknown unary operation %d", unary.op );
            }
        } else if ( sym::isBinary( formula->op ) ) {
            sym::Binary &binary = formula->binary;
            auto a = smt::symbol( convert( binary.left ) );
            auto b = smt::symbol( convert( binary.right ) );
            auto fa = hp2form( binary.left ), fb = hp2form( binary.right );
            int abw = fa->type.bitwidth(), bbw = fb->type.bitwidth();
            ASSERT_EQ( abw, bbw );

            if ( abw > 1 ) {
                switch ( binary.op ) {
#define MAP_OP_ARITH( OP ) case sym::Op::OP: \
                        ASSERT_EQ( bw, abw ); \
                        op = smt::binop< smt::Op::Bv ## OP >( a, b ); \
                        break
                    MAP_OP_ARITH( Add );
                    MAP_OP_ARITH( Sub );
                    MAP_OP_ARITH( Mul );
                    MAP_OP_ARITH( SDiv );
                    MAP_OP_ARITH( UDiv );
                    MAP_OP_ARITH( SRem );
                    MAP_OP_ARITH( URem );
                    MAP_OP_ARITH( Shl ); // NOTE: LLVM as well as SMT-LIB requre both args to shift to have same type
                    MAP_OP_ARITH( AShr );
                    MAP_OP_ARITH( LShr );
                    MAP_OP_ARITH( And );
                    MAP_OP_ARITH( Or );
                    MAP_OP_ARITH( Xor );
#undef MAP_OP_ARITH

#define MAP_OP_CMP( OP ) case sym::Op::OP: \
                        ASSERT_EQ( bw, 1 ); \
                        op = smt::binop< smt::Op::Bv ## OP >( a, b ); \
                        break
                    MAP_OP_CMP( ULE );
                    MAP_OP_CMP( ULT );
                    MAP_OP_CMP( UGE );
                    MAP_OP_CMP( UGT );
                    MAP_OP_CMP( SLE );
                    MAP_OP_CMP( SLT );
                    MAP_OP_CMP( SGE );
                    MAP_OP_CMP( SGT );
#undef MAP_OP_CMP
                    case sym::Op::EQ:
                        ASSERT_EQ( bw, 1 );
                        op = smt::binop< smt::Op::Eq >( a, b );
                        break;
                    case sym::Op::NE:
                        ASSERT_EQ( bw, 1 );
                        op = smt::unop< smt::Op::Not >( smt::binop< smt::Op::Eq >( a, b ) );
                        break;
                    default:
                        UNREACHABLE_F( "unknown binary operation %d", binary.op );
                }
            } else {
                ASSERT_EQ( abw, 1 );
                switch ( binary.op ) {
                    case sym::Op::Xor:
                    case sym::Op::Add:
                    case sym::Op::Sub:
                        op = smt::binop< smt::Op::Xor >( a, b );
                        break;
                    case sym::Op::And:
                    case sym::Op::Mul:
                        op = smt::binop< smt::Op::And >( a, b );
                        break;
                    case sym::Op::UDiv:
                    case sym::Op::SDiv:
                        op = a; // ?
                        break;
                    case sym::Op::URem:
                    case sym::Op::SRem:
                    case sym::Op::Shl:
                    case sym::Op::LShr:
                        op = smt::symbol( "false" );
                        break;
                    case sym::Op::AShr:
                        op = a;
                        break;
                    case sym::Op::Or:
                        op = smt::binop< smt::Op::Or >( a, b );
                        break;

                    case sym::Op::UGE:
                    case sym::Op::SLE:
                        op = smt::binop< smt::Op::Or >( a, smt::unop< smt::Op::Not >( b ) );
                        break;
                    case sym::Op::ULE:
                    case sym::Op::SGE:
                        op = smt::binop< smt::Op::Or >( b, smt::unop< smt::Op::Not >( a ) );
                        break;

                    case sym::Op::UGT:
                    case sym::Op::SLT:
                        op = smt::binop< smt::Op::And >( a, smt::unop< smt::Op::Not >( b ) );
                        break;
                    case sym::Op::ULT:
                    case sym::Op::SGT:
                        op = smt::binop< smt::Op::And >( b, smt::unop< smt::Op::Not >( a ) );
                        break;

                    case sym::Op::EQ:
                        op = smt::binop< smt::Op::Eq >( a, b );
                        break;
                    case sym::Op::NE:
                        op = smt::unop< smt::Op::Not >( smt::binop< smt::Op::Eq >( a, b ) );
                        break;
                    default:
                        UNREACHABLE_F( "unknown binary operation %d", binary.op );
                }
            }
        }

        ASSERT( op );
        auto name = "val_"s + std::to_string( valcount++ ) + suff;
        out << smt::defineConst( name, type( bw ), op ) << std::endl;
        return ptr2Sym.emplace( ptr, name ).first->second;
    }

    std::string_view operator[]( HeapPointer p ) {
        auto it = ptr2Sym.find( p );
        ASSERT( it != ptr2Sym.end() );
        return it->second;
    }

    void pathcond() {
        smt::Vector args;
        for ( auto ptr : pcparts )
            args.emplace_back( smt::symbol( (*this)[ ptr ] ) );

        out << smt::defineConst( "pathcond" + suff, smt::type( "Bool" ), smt::bigand( args ) )
            << std::endl;
    }

    CowHeap &heap;
    std::unordered_map< HeapPointer, std::string > ptr2Sym;
    std::unordered_set< HeapPointer > pcparts;
    std::unordered_set< int > &inputs;
    std::string suff;
    int valcount = 0;
    std::ostream &out;
};

sym::Formula *stripAssumes( sym::Formula *f, FormulaMap &m ) {
    while ( f->op == sym::Op::Assume )
        f = m.hp2form( f->assume.value );
    return f;
}

bool match( sym::Constant &a, sym::Constant &b ) {
    const auto mask = brick::bitlevel::ones< uint64_t >( a.type.bitwidth() );
    return a.type.bitwidth() == b.type.bitwidth() && (a.value & mask) == (b.value & mask);
}

enum class Result { False, True, Unknown };

Result runSolver( std::string input ) {
    auto r = brick::proc::spawnAndWait( proc::StdinString( input ) | proc::CaptureStdout | proc::CaptureStderr,
//                  "boolector", "--smt2" );
                  "z3", "-in", "-smt2" );
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

bool SymbolicHasher::smtEqual( SymPairs &symPairs ) const {
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
    for ( auto p : symPairs ) {
        // assumes are encoded in the path condition
        auto f1 = stripAssumes( m1.hp2form( p.first ), m1 ),
             f2 = stripAssumes( m2.hp2form( p.second ), m2 );

        if ( f1->op == sym::Op::Variable && f2->op == sym::Op::Variable ) {
            if ( f1->var.id != f2->var.id )
                return false;
            else
                continue;
        }
        if ( f1->op == sym::Op::Constant && f2->op == sym::Op::Constant ) {
            if ( match( f1->con, f2->con ) )
                continue;
            else
                return false;
        }
        valeq.emplace_back( smt::binop< smt::Op::Eq >( smt::symbol( m1[ p.first ] ), smt::symbol( m2[ p.second ] ) ) );
    }

    smtFormula << smt::assume( smt::unop< smt::Op::Not >( smt::binop< smt::Op::And >(
                smt::binop< smt::Op::Eq >( smt::symbol( "pathcond_1" ), smt::symbol( "pathcond_2" ) ),
                smt::binop< smt::Op::Implies >( smt::symbol( "pathcond_1" ), smt::bigand( valeq ) ) ) ) )
        << std::endl;

    smtFormula << "(check-sat)" << std::endl;

    return runSolver( smtFormula.str() ) == Result::False;
}

void SymbolicContext::traceAlg( TraceAlg ta ) {

    std::unordered_set< int > inputs;
    std::stringstream smtFormula;
    FormulaMap map( heap(), "", inputs, smtFormula );

    ASSERT_EQ( ta.args.size(), 1 );

    HeapPointer assumes = ta.args[0];
    while ( !assumes.null() ) {
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
