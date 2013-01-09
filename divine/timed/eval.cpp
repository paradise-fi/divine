#include <iostream>
#include <cassert>
#include <algorithm>
#include <stdexcept>
#include <typeinfo>
#include "clocks.h"
#include "eval.h"
#include <utap/utap.h>
#include <dbm/dbm.h>

using namespace UTAP;
using namespace UTAP::Constants;
using namespace std;

std::pair< int, int > Evaluator::evalRange( int procId, const UTAP::type_t &type ) {
    auto range = type.getRange();
    return make_pair( eval( procId, range.first ), eval( procId, range.second ) );
}

void Evaluator::computeChannelPriorities( UTAP::TimedAutomataSystem &sys ) {
    ChannelPriorities.resize( ChannelTable.size(), sys.getTauPriority() );
    for ( auto& ch : sys.getChanPriorities() ) {
        if ( ch.chanElement.getType().isArray() ) {
            int size;
            auto arr = getArray( -1, ch.chanElement, &size );
            unsigned int begin = arr.first->offset + arr.second;
            unsigned int end = begin + size;
            for ( unsigned int i = begin; i < end; ++i ) {
                ChannelPriorities[ i ] = ch.chanPriority;
            }
        } else {
            ChannelPriorities[ evalChan( -1, ch.chanElement ) ] = ch.chanPriority;
        }
    }
}

void Evaluator::emitError( int code ) {
    assert( code != 0 );
    throw EvalError( code );
}

UTAP::type_t Evaluator::getBasicType( const UTAP::type_t &type ) {
    UTAP::type_t type_tmp = type;
    while ( type_tmp.isArray() || type_tmp.isRecord() ) {
        type_tmp = type_tmp[0];
    }
    return type_tmp;
}

int Evaluator::getArraySizes ( int procId, const type_t &type, vector< int > &output ) {
    type_t t = type;
    
    int size = 1;
    while ( t.isArray() || t.isRecord() ) {
        if ( t.isArray() ) {
            output.push_back( eval( procId, t.getArraySize().getRange().second ) + 1 );
            t = t.getSub();
        } else {
            output.push_back( t.getRecordSize() );
            size *= output.back();
            break;
        }
        size *= output.back();
    }

    return size;
}

const Evaluator::VarData &Evaluator::getVarData( int procId, const UTAP::symbol_t & s ) const {
    auto var = vars.find( make_pair( s, procId ) );    // local
    if ( var == vars.end() )
        var = vars.find( make_pair( s, -1 ) );        // global

    assert( var != vars.end() );
    return var->second;
}

const Evaluator::VarData &Evaluator::getVarData( int procId, const UTAP::expression_t &expr ) {
    if ( expr.getKind() == DOT ) {
        assert( procId == -1 );
        procId = resolveId( -1, expr[0] );
        auto proc = static_cast< const instance_t* >( expr[0].getSymbol().getData() ); // get instance
        symbol_t symbol = proc->templ->frame[ expr.getIndex() ];  // get referenced symbol
        auto var = vars.find( make_pair( symbol, procId ) );
        assert( var != vars.end() );
        return var->second;
    }
    assert( expr.getKind() == IDENTIFIER );
    return getVarData( procId, expr.getSymbol() );
}

const Evaluator::FuncData &Evaluator::getFuncData( int procId, const UTAP::symbol_t & s ) const {
    auto fun = funs.find( make_pair( s, procId ) );    // local
    if ( fun == funs.end() )
        fun = funs.find( make_pair( s, -1 ) );        // global

    assert( fun != funs.end() );
    return fun->second;
}

void Evaluator::parseArrayValue(   const expression_t &exp,
                        vector< int32_t > &output,
                        int procId ) {
    if ( exp.getType().isArray() ) {
        if ( exp.getKind() == LIST ) {
            int arraySize = eval( procId, exp.getType().getArraySize().getRange().second ) + 1;
            for ( int i = 0; i < arraySize; i++ )
                parseArrayValue( exp[i], output, procId );
        } else {
            symbol_t s = exp.getSymbol();
            int pSize;
            auto p = getArray( procId, exp, &pSize );
            const VarData &var = *(p.first);
            if ( var.prefix == PrefixType::CONSTANT ) {
                const int32_t *varContent = getValue( procId, s );
                if ( p.second+pSize > var.elementsCount ) {
                    emitError( EvalError::ARRAY_INDEX );
                }
                for ( int i = p.second; i < p.second + pSize; i++ )
                    output.push_back( varContent[ i ] );
            }
        }
    } else {
        output.push_back( eval( procId, exp ) );
    }
}

void Evaluator::processFunctionDecl( const function_t &f, int procId ) {
    funs[ make_pair( f.uid, procId ) ] = FuncData( f );
}
    
void Evaluator::processSingleDecl( const symbol_t &s,
                        const expression_t &initializer,
                        int procId,
                        bool local ) {

    const type_t& type = s.getType();
    const type_t& basicType = getBasicType( type );

    PrefixType variablePrefix = local ? PrefixType::LOCAL : PrefixType::NONE;

    if ( type.is( REF ) && !basicType.is( CONSTANT ) ) {
        symbol_t ref_symbol = initializer.getSymbol();
    
        VarData new_vardata( getVarData( procId, ref_symbol ) ); 
        if ( ref_symbol.getType().isArray() && initializer.getKind() != IDENTIFIER ) {
            int pSize;
            auto p = getArray( procId, initializer, &pSize );

            for ( expression_t tmp = initializer; tmp.getKind() != IDENTIFIER; tmp = tmp[0] ) {
                int size = new_vardata.arraySizes.front();
                new_vardata.arraySizes.erase(new_vardata.arraySizes.begin());
                new_vardata.elementsCount /= size;
            }
            new_vardata.offset += p.second;
            assert(new_vardata.elementsCount == pSize);
        } // else nothing
        vars[ make_pair( s, procId ) ] = new_vardata;
    } else if ( basicType.isChannel() ) {
        if ( type.isArray() ) {
            vector< int > arrSizes;
            int size = getArraySizes( procId, type, arrSizes );
            vars[ make_pair( s, procId) ] = VarData ( Type::CHANNEL, variablePrefix,
                                                        ChannelTable.size(), arrSizes, size );
            for ( int i = 0; i < size; i++ )
                ChannelTable.push_back( &s );
        } else {
            vars[ make_pair(s, procId) ] = VarData( Type::CHANNEL, variablePrefix, ChannelTable.size() );
            ChannelTable.push_back( &s );
        }
    } else if ( basicType.isClock() ) {
        if ( s.getName() == "t(0)" ) // we dont need to save global timer
            return;
        if ( type.isArray() ) {
            vector< int > arrSizes;
            int size = getArraySizes( procId, type, arrSizes );
            vars[ make_pair( s, procId) ] = VarData ( Type::CLOCK, variablePrefix,
                                                        ClockTable.size(), arrSizes, size );
            for ( int i = 0; i < size; i++ )
                ClockTable.push_back( &s );
        } else {
            vars[ make_pair(s, procId) ] = VarData( Type::CLOCK, variablePrefix, ClockTable.size() );
            ClockTable.push_back( &s );
        }
    } else {
        pair< int, int > ranges;
        if ( basicType.is( UTAP::Constants::BOOL ) )
            ranges = make_pair( 0, 1 );
        else if ( basicType.is( CONSTANT ) ) 
            ranges = make_pair( numeric_limits< int >::min(), numeric_limits< int >::max() );
        else 
            ranges = evalRange( procId, basicType);

        int default_value;
        if ( ( ranges.first < 0 && ranges.second < 0 ) ||
             ( ranges.first > 0 && ranges.second > 0 ) )
            default_value = ranges.first;
        else
            default_value = 0;
        
        if ( type.isArray() ) {
            /*
             *  ARRAY value
             */

            PrefixType prefix = basicType.is( CONSTANT ) ? PrefixType::CONSTANT : variablePrefix;

            vector< int > arrSizes;
            int size = getArraySizes( procId, type, arrSizes );
            
            if ( prefix == PrefixType::NONE ) {
                vars.insert( make_pair( make_pair( s, procId ), 
                            VarData( Type::ARRAY, prefix, initValues.size(), arrSizes, size, ranges ) ) );
                if ( initializer.empty() )
                    initValues.insert( initValues.end(), size, default_value );
                else
                    parseArrayValue( initializer, initValues, procId );
            } else {
                decltype( vars )::iterator it = vars.find( make_pair( s, procId ) );
                if ( it == vars.end() ) {
                    vars.insert( it, make_pair ( make_pair ( s, procId ),
                                VarData( Type::ARRAY, prefix, metaValues.size(), arrSizes, size, ranges ) ) );
                    if ( initializer.empty() )
                        metaValues.insert( metaValues.end(), size, default_value );
                    else
                        parseArrayValue( initializer, metaValues, procId );
                } else {
                    vector< int32_t > vals;
                    if ( initializer.empty() )
                        vals.resize( size, default_value );
                    else
                        parseArrayValue( initializer, vals, procId );
                    
                    for ( int i = 0; i < size; i++ ) {
                        if ( !inRange( procId, s, vals[ i ] ) ) {
                            emitError( EvalError::OUT_OF_RANGE );
                        }
                        metaValues[ i + it->second.offset ] = vals[ i ];
                    }
                }
            }
        } else {
            /*
             *  SCALAR value
             */
            assert( basicType == type );
            PrefixType prefix = type.is( CONSTANT ) ? PrefixType::CONSTANT : variablePrefix;
            int32_t value = initializer.empty() ? default_value : eval( procId, initializer );

            if ( prefix == PrefixType::NONE ) {
                vars.insert( make_pair( make_pair( s, procId ),
                    VarData( Type::SCALAR, prefix, initValues.size(), ranges ) ) );
                if ( !inRange( procId, s, value ) )
                    emitError( EvalError::OUT_OF_RANGE );
                initValues.push_back( value );
            } else {
                auto it = vars.find( make_pair( s, procId ) );
                if ( it == vars.end() ) {
                    vars.insert( it, make_pair( make_pair( s, procId),
                                VarData( Type::SCALAR, prefix, metaValues.size(), ranges ) ) );
                    if ( !inRange( procId, s, value ) )
                        emitError( EvalError::OUT_OF_RANGE );
                    metaValues.push_back( value );
                } else {
                    if ( !inRange( procId, s, value ) )
                        emitError( EvalError::OUT_OF_RANGE );
                    metaValues[ it->second.offset ] = value;
                }
            }
        }
    }
}
    

int32_t Evaluator::unop( int procId, const Constants::kind_t op, const expression_t &a ) {
    int result;
    switch ( op ) {
    case UNARY_MINUS:   return -eval( procId, a );
    case NOT:           return !eval( procId, a );
    case PREINCREMENT:
        result = eval( procId, a );
        assign( a, result + 1, procId );
        return result + 1;
    case POSTINCREMENT:
        result = eval( procId, a );
        assign( a, result + 1, procId );
        return result;

    case PREDECREMENT:
        result = eval( procId, a );
        assign( a, result - 1, procId );
        return result - 1;

    case POSTDECREMENT:
        result = eval( procId, a );
        assign( a, result - 1, procId );
        return result;
    
    default: 
        cerr << "Unknown unop " << op << endl;
        assert( false );
        return 0;
    }
}


int32_t Evaluator::binop( int procId, const Constants::kind_t &op, const expression_t &a,
        const expression_t &b ) {

    if ( a.getType().isClock() || b.getType().isClock() ) {
        int clock_id, value;
        if ( a.getType().isClock() ) {
            clock_id = resolveId( procId, a );
            value = eval( procId, b );
            switch ( op ) {
            case LT:    return clocks.constrainBelow( clock_id, value, true );
            case LE:    return clocks.constrainBelow( clock_id, value, false );
            case GT:    return clocks.constrainAbove( clock_id, value, true );
            case GE:    return clocks.constrainAbove( clock_id, value, false );
            case EQ:    return clocks.constrainAbove( clock_id, value, false )
                            && clocks.constrainBelow( clock_id, value, false );
            default:
                cerr << "Unknown binop " << op << " on clocks" << endl;
                assert( false );
                return 0;
            }
        } else {
            clock_id = resolveId( procId, b );
            value = eval( procId, a );
            switch ( op ) {
            case GT:    return clocks.constrainBelow( clock_id, value, true ); 
            case GE:    return clocks.constrainBelow( clock_id, value, false );
            case LT:    return clocks.constrainAbove( clock_id, value, true ); 
            case LE:    return clocks.constrainAbove( clock_id, value, false );
            case EQ:    return clocks.constrainAbove( clock_id, value, false )
                            && clocks.constrainBelow( clock_id, value, false );
            default:
                cerr << "Unknown binop " << op << " on clocks" << endl;
                assert( false );
                return 0;
            }
        }

    } else if ( a.getType().isDiff() || b.getType().isDiff() ) {
        int clock_idL, clock_idR, value;
        if ( a.getType().isDiff() ) {
            clock_idL = resolveId( procId, a[0] );
            clock_idR = resolveId( procId, a[1] );
            value = eval( procId, b );
            switch ( op ) {
            case LT:    return clocks.constrainClocks( clock_idL, clock_idR, value, true );
            case LE:    return clocks.constrainClocks( clock_idL, clock_idR, value, false );
            case GT:    return clocks.constrainClocks( clock_idR, clock_idL, -value, true );
            case GE:    return clocks.constrainClocks( clock_idR, clock_idL, -value, false );
            case EQ:    return clocks.constrainClocks( clock_idL, clock_idR, value, false )
                            && clocks.constrainClocks( clock_idR, clock_idL, -value, false );
            default:
                cerr << "Unknown binop " << op << " on clocks" << endl;
                assert( false );
                return 0;
            }
        } else {
            clock_idL = resolveId( procId, b[0] );
            clock_idR = resolveId( procId, b[1] );
            value = eval( procId, a );
            switch ( op ) {
            case GT:    return clocks.constrainClocks( clock_idL, clock_idR, value, true ); 
            case GE:    return clocks.constrainClocks( clock_idL, clock_idR, value, false );
            case LT:    return clocks.constrainClocks( clock_idR, clock_idL, -value, true ); 
            case LE:    return clocks.constrainClocks( clock_idR, clock_idL, -value, false );
            case EQ:    return clocks.constrainClocks( clock_idL, clock_idR, value, false )
                            && clocks.constrainClocks( clock_idR, clock_idL, -value, false );
            default:
                cerr << "Unknown binop " << op << " on clocks" << endl;
                assert( false );
                return 0;
            }
        }
    }
    
    int32_t b_num;
    pair< const VarData*, int > p;
    switch ( op ) {
    case ASSPLUS:
    case PLUS:  return eval( procId, a ) + eval( procId, b );
    case ASSMINUS:
    case MINUS:
        return eval( procId, a ) - eval( procId, b );
    case ASSMULT:
    case MULT:  return eval( procId, a ) * eval( procId, b );
    case ASSDIV:
    case DIV:
        b_num = eval( procId, b );
        if ( b_num == 0 ) {
            emitError( EvalError::DIVIDE_BY_ZERO );
        }
        return eval( procId, a ) / b_num;
    case ASSMOD:
    case MOD:
        b_num = eval( procId, b );
        if ( b_num == 0 ) {
            emitError( EvalError::DIVIDE_BY_ZERO );
        }
        return eval( procId, a ) % b_num;

    case ASSAND:
    case BIT_AND:   return eval( procId, a ) & eval( procId, b );
    case ASSOR:
    case BIT_OR:    return eval( procId, a ) | eval( procId, b );
    case ASSXOR:
    case BIT_XOR:   return eval( procId, a ) ^ eval( procId, b );
    case ASSLSHIFT:
    case BIT_LSHIFT:
        b_num = eval( procId, b );
        if ( (b_num & 31) != b_num )
            emitError( EvalError::SHIFT );
        return eval( procId, a ) << b_num;
    case ASSRSHIFT:
    case BIT_RSHIFT:
        b_num = eval( procId, b );
        if ( (b_num & 31) != b_num )
            emitError( EvalError::SHIFT );
        return eval( procId, a ) >> b_num;

    case AND:   return eval( procId, a) && eval( procId, b);
    case OR:    return eval( procId, a) || eval( procId, b);

    case LT:    return eval( procId, a) < eval( procId, b);
    case LE:    return eval( procId, a) <= eval( procId, b);
    case GT:    return eval( procId, a) > eval( procId, b);
    case GE:    return eval( procId, a) >= eval( procId, b);

    case EQ:    return eval( procId, a) == eval( procId, b);
    case NEQ:   return eval( procId, a) != eval( procId, b);

    case MAX:   return max( eval( procId, a ), eval( procId, b ) );
    case MIN:   return min( eval( procId, a ), eval( procId, b ) );
    
    case ARRAY:
        p = getArray( procId, expression_t::createBinary( ARRAY, a, b ) );
        return getValue( *p.first )[ p.second ];

    case FORALL: {
        bool ret = true;
        auto range = evalRange( procId, a.getType() );
        for ( int val = range.first; val <= range.second; ++val )
            ret = ret && eval( procId, b.subst( a.getSymbol(), UTAP::expression_t::createConstant( val ) ) );
        return ret;
    }
    case EXISTS: {
        bool ret = false;
        auto range = evalRange( procId, a.getType() );
        for ( int val = range.first; val <= range.second; ++val ) {
            ret = ret || eval( procId, b.subst( a.getSymbol(), UTAP::expression_t::createConstant( val ) ) );
        }
        return ret;
    }

    default:
        cerr << "Unknown binop " << op << endl;
        assert( false );
    }
    return -1;
}

int32_t Evaluator::ternop( int procId, const Constants::kind_t op, const expression_t &a,
                const expression_t &b, const expression_t &c ) {
    switch ( op ) {
    case INLINEIF:
        return eval( procId, a ) ? eval( procId, b ) : eval( procId, c );
    default:
        cerr << "Unknown ternop " << op << endl;
        assert( false );
        return 0;
    }
}

// evaluates array expression, returns value identifier and index
// if the result is a value, returns index of that value and sets pSize to 1
// if the result is still an array, returns index to the first element of the array and pSize is set to its size
std::pair< const Evaluator::VarData*, int > Evaluator::getArray( int procId, const expression_t& e, int *pSize ) {
    assert( e.getKind() == ARRAY || e.getKind() == IDENTIFIER || e.getKind() == DOT );
    std::vector< uint32_t > indices;
    expression_t sub = e;
    while ( sub.getKind() == ARRAY ) {
        indices.push_back( eval( procId, sub[ 1 ] ) );
        sub = sub[ 0 ];
        int32_t offset = eval( procId, sub.getType().getArraySize().getRange().first );
        indices.back() -= offset;
    }
    const VarData& var = getVarData( procId, sub );
    int size = var.elementsCount;
    int index = 0;
    reverse( indices.begin(), indices.end() );
    for ( int i = 0; i < indices.size(); i++ ) {
        if ( indices[ i ] >= var.arraySizes[ i ] ) {
            emitError( EvalError::ARRAY_INDEX );
        }
        size /= var.arraySizes[ i ];
        index += size * indices[ i ];
    }
    if ( pSize )
        *pSize = size;
    return std::make_pair( &var, index );
}

void Evaluator::assign( const expression_t& lval, int32_t val, int pId ) {
    std::pair< const VarData*, int > var;
    if ( lval.getKind() == IDENTIFIER )
        var = make_pair( &getVarData( pId, lval.getSymbol() ), 0 );
    else if ( lval.getKind() == ARRAY )
        var = getArray( pId, lval );
    else
        assert( false );
    int index = var.first->offset + var.second;
    int32_t *data;
    if ( var.first->prefix == PrefixType::LOCAL )
        data = &metaValues[0];
    else
        data = Evaluator::data;
    
    if ( lval.getType().is( CLOCK ) )   // assign to clock
        clocks.set( index, val );
    else {      // assign to variable
        if ( !var.first->inRange( val ) ) {
            emitError( EvalError::OUT_OF_RANGE );
        }
        assert( data );
        data[ index ] = val;
    }
}

void Evaluator::assign( const expression_t& lexp, const expression_t& rexp, int pId ) {
    if ( rexp.getType().is( ARRAY ) ) { // assign array to array
        int size;
        auto arr = getArray( pId, lexp );
        int32_t *pDest = data + arr.first->offset + arr.second;
        arr = getArray( pId, rexp, &size );
        int32_t *data;
        if ( arr.first->prefix == PrefixType::LOCAL )
            data = &metaValues[0];
        else
            data = Evaluator::data;
        
        int32_t *pSrc = data + arr.first->offset + arr.second;
        int32_t *pEnd = pSrc + size;
        assert ( pEnd - pSrc <= arr.first->elementsCount );
        while ( pSrc != pEnd ) {
            if ( !arr.first->inRange( *pSrc ) ) {
                emitError( EvalError::OUT_OF_RANGE );
            }
            *pDest++ = *pSrc++;
        }
    } else {    // assign to variable
        assign( lexp, eval( pId, rexp ), pId );
    }

}

int32_t *Evaluator::getValue( const VarData &var ) {
    int32_t *data;
    if ( var.prefix == PrefixType::NONE ) {   // get variable
        data = Evaluator::data;
    } else {                                  // get constant or local variable
        assert( metaValues.size() > 0 ); 
        data = &metaValues[0];
    }

    assert( data );
    return data + var.offset;
}
    
int32_t *Evaluator::getValue( int procId, const symbol_t& s ) {
    const VarData& var = getVarData( procId, s );
    return getValue( var );
}

bool Evaluator::inRange( int procId, const symbol_t& s, int32_t value ) const {
    return getVarData( procId, s ).inRange( value );
}

bool Evaluator::isChanUrgent( int ch ) const {
    return ChannelTable[ch]->getType().is( URGENT );
}

bool Evaluator::isChanBcast( int ch ) const {
    return ChannelTable[ch]->getType().is( BROADCAST );
}

void Evaluator::processDeclGlobals( UTAP::TimedAutomataSystem &sys ) {
    assert( ProcessTable.empty() );
    
    for ( const function_t &f : sys.getGlobals().functions )
        // global functions
        processFunctionDecl( f, -1 );
    for ( const variable_t &v : sys.getGlobals().variables ) {
        // global variables
        processSingleDecl( v.uid, v.expr, -1 );
    }
    computeChannelPriorities( sys );
}

void Evaluator::processDecl( const vector< instance_t > &procs ) {
    for ( int pId = 0; pId < procs.size(); ++pId ) {
        const instance_t &p = procs[pId];

        // save the symbol so it can be used in property
        auto type = p.uid.getType();
        if ( type.isProcessSet() ) { // partial instances are represended as arrays of processes
            vector< int > arrSizes;
            int size = 1;
            for ( int i = 0; i < type.size(); ++i ) {
                auto r = evalRange( -1, type[ i ] );
                arrSizes.push_back( r.second - r.first + 1 );
                size *= arrSizes.back();
            }
            // insert array of processes or do nothing if we have already seen this uid
            vars.insert( make_pair( make_pair( p.uid, -1 ), VarData( Type::PROCESS, PrefixType::NONE, pId, arrSizes, size ) ) );
        } else {
            vars[ make_pair( p.uid, -1 ) ] = VarData ( Type::PROCESS, PrefixType::NONE, pId );
        }

        // template parametres
        for ( const pair< symbol_t, expression_t > &m : p.mapping )
            processSingleDecl( m.first, m.second, ProcessTable.size() );

        // fuctions
        for ( const function_t &f : p.templ->functions )
            processFunctionDecl( f, ProcessTable.size() );

        // local variables
        assert( p.templ );
        for ( const variable_t &v : p.templ->variables )
            processSingleDecl( v.uid, v.expr, ProcessTable.size() );
        
        ProcessTable.push_back( p );
    }

    // compute clock limits

    clocks.resize( ClockTable.size() );
    fixedClockLimits.resize( ClockTable.size(), make_pair( -dbm_INFINITY, -dbm_INFINITY ) );

    computeLocClockLimits( procs );
}

// after collectResets, out[c] is true iff the clock _c_ is reseted on the edge _e_
void Evaluator::collectResets( int pId, const template_t &templ, const edge_t &e, vector< bool > & out ) {
    out.resize( ClockTable.size(), false );
    expression_t tmp = e.assign;
    while ( tmp.getKind() == COMMA ) {
        if ( tmp[0].getKind() == ASSIGN && tmp[0][0].getType().isClock() ) {
            out[ resolveId( pId, tmp[0][0] ) ] = true;
        }
        tmp = tmp[1];
    } 
    if ( tmp.getKind() == ASSIGN && tmp[0].getType().isClock() )
        out[ resolveId( pId, tmp[0] ) ] = true;
}

void Evaluator::computeLocClockLimits( const vector< instance_t > &procs ) {
    locClockLimits.resize( ProcessTable.size() );

    int i = 0;
    for ( const instance_t &p : procs ) {
        for ( state_t &s : p.templ->states ) {
            limitsVector limits;
            limits.resize( ClockTable.size(), make_pair( -dbm_INFINITY, -dbm_INFINITY ) );
            setClockLimits( i, s.invariant, limits );
            locClockLimits[ i ][ s.locNr ] = limits;
        }
        for ( edge_t &e : p.templ->edges ) {
            symbol_t src = e.src->uid;
            limitsVector &limits = locClockLimits[ i ][ e.src->locNr ];
            assert( limits.size() == ClockTable.size() );
            setClockLimits( i, e.guard, limits );
        }
        ++i;
    }

    bool change = true;
    while ( change ) {
        // bounds propagation
        change = false;
        i = 0;
        for ( const instance_t &p : procs ) {
            for ( edge_t &e : p.templ->edges ) {
                vector< bool > reseted;
                collectResets( i, *p.templ, e, reseted );
                for ( int clk = 0; clk < ClockTable.size(); clk++ ) {
                    if ( reseted[ clk ] )
                        continue;
                    auto& srcLim = locClockLimits[ i ][ e.src->locNr ][ clk ];
                    auto& dstLim = locClockLimits[ i ][ e.dst->locNr ][ clk ];
                    if ( srcLim.first < dstLim.first ) {
                        srcLim.first = dstLim.first;
                        change = true;
                    }
                    if ( srcLim.second < dstLim.second ) {
                        srcLim.second = dstLim.second;
                        change = true;
                    }
                }
            }
            ++i;
        }

    }
}

void Evaluator::computeLocalBounds() {
    vector< pair< int32_t, int32_t > > bounds = fixedClockLimits;
    assert( fixedClockLimits.size() == ClockTable.size() );
    for ( int proc = 0; proc < ProcessTable.size(); ++proc ) {
        for ( int clk = 0; clk < ClockTable.size(); ++clk ) {
            auto &b = locClockLimits[ proc ][ locations[ proc ] ][ clk ];
            bounds[ clk ].first = max( b.first, bounds[ clk ].first );
            bounds[ clk ].second = max( b.second, bounds[ clk ].second );
        }
    }

    for ( int clk = 0; clk < ClockTable.size(); ++clk ) {
        clocks.setUpperBound( clk, bounds[clk].first );
        clocks.setLowerBound( clk, bounds[clk].second );
    }
}

void Evaluator::evalCmd( int procId, const expression_t& expr ) {
    if ( expr.getKind() == COMMA ) {
        evalCmd( procId, expr[0] );
        evalCmd( procId, expr[1] );
        return;
    }
    if ( expr.getSize() < 2 ) {
        eval( procId, expr );
        return;
    }

    symbol_t dest_sym = expr[0].getSymbol();
    if ( dest_sym.getType().isClock() ) {
        int value =  eval( procId, expr[ 1 ] );
        if ( value < 0 ) {
            emitError( EvalError::NEG_CLOCK );
        }
        clocks.set( resolveId( procId, expr[0]), value );
        return;
    }

    switch ( expr.getKind() ) {
    case( ASSIGN ):
        assign( expr[0], expr[1], procId );
        break;
    case( ASSPLUS ):
    case( ASSMINUS ): 
    case( ASSDIV ):
    case( ASSMOD ):
    case( ASSMULT ):
    case( ASSAND ):
    case( ASSOR ):
    case( ASSXOR ):
    case( ASSLSHIFT ):
    case( ASSRSHIFT ):
        assign( expr[0], binop( procId, expr.getKind(), expr[ 0 ], expr[ 1 ] ), procId );
        break;
    default:
        eval( procId, expr );
        break;
    }
}

void Evaluator::pushStatements( int procId, vector< StatementInfo > &stack, StatementInfo block ) {
    const BlockStatement *blk_stmt = dynamic_cast< const BlockStatement* >( block.stmt );
    if ( blk_stmt ) {
        for ( auto &var : blk_stmt->variables ) {
            processSingleDecl( var.uid, var.expr, procId, true );
        }
        // NOTE: BlockStatement does not provide a size() method
        int blockSize = blk_stmt->end() - blk_stmt->begin();
        stack.resize( stack.size() + blockSize );
        reverse_copy( blk_stmt->begin(), blk_stmt->end(), stack.end() - blockSize );
    } else {
        // special case when block consists of only one statement
        // 
        // in this case the are no variables in the block
        stack.push_back( block );
    }
}

int32_t Evaluator::evalFunCall( int procId, const UTAP::expression_t &exp ) {
    const symbol_t &sym_f = exp[0].getSymbol();
    const FuncData &fdata = getFuncData( procId, sym_f );
    assert( fdata.fun );
    int arity = fdata.fun->uid.getType().size() - 1;
    for ( int i = 0; i < arity; ++i ) {
        // parameters are stored in the frame and apparently nowhere else
        processSingleDecl( fdata.fun->body->getFrame()[i], exp[ i + 1 ], procId, true );
    }
    for ( auto &var : fdata.fun->variables )
        processSingleDecl( var.uid, var.expr, procId, true );

    // stack of statements to be evaluated
    vector< StatementInfo > statementStack;

    statementStack.push_back( StatementInfo( fdata.fun->body, 0 ) );

    while ( !statementStack.empty() ) {
        StatementInfo stmt_info = statementStack.back();
        const Statement *stmt = stmt_info.stmt;
        statementStack.pop_back();

        assert( stmt );

        if ( dynamic_cast< const EmptyStatement *>( stmt ) ) {
        
        } else if ( dynamic_cast< const ExprStatement* >( stmt ) ) {
            evalCmd( procId, dynamic_cast< const ExprStatement* >( stmt )->expr );
        
        } else if ( dynamic_cast< const ForStatement* >( stmt ) ) {
            const ForStatement *for_stmt = dynamic_cast< const ForStatement* >( stmt );
            if ( stmt_info.iterCount == 0 )
                evalCmd( procId, for_stmt->init );
            else
                evalCmd( procId, for_stmt->step );
            if ( eval( procId, for_stmt->cond ) ) {
                ++stmt_info.iterCount;
                pushStatements( procId, statementStack, stmt_info );
                pushStatements( procId, statementStack, StatementInfo( for_stmt->stat ) );
            }

        } else if ( dynamic_cast< const IterationStatement* >( stmt ) ) {
            const IterationStatement *iter_stmt = dynamic_cast< const IterationStatement* >( stmt );
            auto r = evalRange( procId, iter_stmt->symbol.getType() );
            int range_len = r.second - r.first + 1;
            if ( stmt_info.iterCount < range_len ) {
                const VarData &var = getVarData( procId, iter_stmt->symbol );
                if ( !inRange( procId, iter_stmt->symbol, stmt_info.iterCount + r.first ) ) {
                    emitError( EvalError::OUT_OF_RANGE );
                }
                metaValues[ var.offset ] = stmt_info.iterCount + r.first;
                ++stmt_info.iterCount;
                pushStatements( procId, statementStack, stmt_info );
                pushStatements( procId, statementStack, StatementInfo( iter_stmt->stat ) );
            }

        } else if ( dynamic_cast< const WhileStatement* >( stmt ) ) {
            const WhileStatement *while_stmt = dynamic_cast< const WhileStatement* >( stmt );
            if ( eval( procId, while_stmt->cond) ) {
                assert( while_stmt->stat );
                ++stmt_info.iterCount;
                pushStatements( procId, statementStack, stmt_info );
                pushStatements( procId, statementStack, StatementInfo( while_stmt->stat ) );
            }
        
        } else if ( dynamic_cast< const DoWhileStatement* >( stmt ) ) {
            const DoWhileStatement *dowhile_stmt = dynamic_cast< const DoWhileStatement* >( stmt );
            ++stmt_info.iterCount;
            assert( dowhile_stmt->stat );
            if ( stmt_info.iterCount == 0 || eval( procId, dowhile_stmt->cond ) ) {
                pushStatements( procId, statementStack, stmt_info );
                pushStatements( procId, statementStack, StatementInfo( dowhile_stmt->stat ) );
            }
        
        } else if ( dynamic_cast< const BlockStatement* >( stmt ) ) {
            const BlockStatement *blk = dynamic_cast< const BlockStatement* >( stmt );
            pushStatements( procId, statementStack, StatementInfo( blk ) );
        
        } else if ( dynamic_cast< const SwitchStatement* >( stmt ) ) {
            // UTAP does not know switch, case, default keywords
            cerr << "Switch statement is not supported" << endl;
            assert( false );
            return 0;

        } else if ( dynamic_cast< const CaseStatement* >( stmt ) ) {
            cerr << "Case statement is not supported" << endl;
            assert( false );
            return 0;
        
        } else if ( dynamic_cast< const DefaultStatement* >( stmt ) ) {
            cerr << "Default statement is not supported" << endl;
            assert( false );
            return 0;
        
        } else if ( dynamic_cast< const IfStatement* >( stmt ) ) {
            const IfStatement *eval_stmt = dynamic_cast< const IfStatement* >( stmt );
            bool cond_value = eval( procId, eval_stmt->cond );
            if ( cond_value ) {
                assert( eval_stmt->trueCase );
                pushStatements( procId, statementStack, StatementInfo( eval_stmt->trueCase ) );
            } else if ( eval_stmt->falseCase ) {
                assert( eval_stmt->falseCase );
                pushStatements( procId, statementStack, StatementInfo( eval_stmt->falseCase ) );
            }
        
        } else if ( dynamic_cast< const BreakStatement* >( stmt ) ) {
            // NOTE: UTAP library declares BreakStatement and ContinueStatement, but UPPAAL does not
            //      know break nor continue keyword...
            // delete current block + one more statement (while/for "header")
            cerr << "Break statement is not supported" << endl;
            assert( false );
            return 0;

        } else if ( dynamic_cast< const ContinueStatement* >( stmt ) ) {
            cerr << "Continue statement is not supported" << endl;
            assert( false );
            return 0;
        
        } else if ( dynamic_cast< const ReturnStatement* >( stmt ) ) {
            return eval( procId, dynamic_cast< const ReturnStatement* >( stmt )->value );
        
        } else {
            cerr << "Unknown statement type" << std::endl;
            assert( false );
        
        }
    }
    
    return 0;
}

int32_t Evaluator::eval( int procId, const expression_t& expr ) {
    if ( expr.empty() )
        return 1;

    assert( !expr.getType().isClock() );
    assert( !expr.getType().isChannel() );
    assert( !expr.getType().is( LIST ) );

    if ( expr.getKind() == FUNCALL )
        return evalFunCall( procId, expr );

    switch ( expr.getSize() ) {
        case 3:
            return  ternop( procId, expr.getKind(), expr[0], expr[1], expr[2] );
        case 2:
            return  binop( procId, expr.getKind(), expr[0], expr[1] );
        case 1:
            if ( expr.getKind() == DOT ) {
                int pId = resolveId( -1, expr[0] );
                auto proc = static_cast< const instance_t* >( expr[0].getSymbol().getData() ); // get instance
                symbol_t symb = proc->templ->frame[ expr.getIndex() ];  // get referenced symbol
                if ( symb.getType().isLocation() ) {
                    auto location = static_cast< const state_t* >( symb.getData() );   // get location
                    return locations[ pId ] == location->locNr;
                } else {
                    return *getValue( getVarData( procId, expr ) );
                }
            }
            return unop( procId, expr.getKind(), expr[0] ) ;
        case 0:
            switch ( expr.getKind() ) {
                case IDENTIFIER:
                    return *getValue( procId, expr.getSymbol() );
                case CONSTANT:
                    return expr.getValue();
                default:
                    cerr << "unknown nulop" << expr.getKind() << endl;
                    assert( false );
                    return -1;
            }
    }
    assert( false );
    return 0;
}

void Evaluator::setData( char *d, Locations l ) {
    data = reinterpret_cast< int32_t* >( d );
    clocks.setData( d + getReqSize() - clocks.getReqSize() );
    locations = l;
}

void Evaluator::extrapolate() {
    computeLocalBounds();
    clocks.extrapolate( clockDiffrenceExprs.empty() );
}

/*
 *  resolveId() 
 */
int Evaluator::resolveId( int procId, const expression_t& expr ) { 
    assert( expr.getType().is( CHANNEL ) || expr.getType().is( CLOCK ) || expr.getType().is( PROCESS ) );
    if ( expr.getKind() != ARRAY ) {
        return getVarData( procId, expr ).offset;
    } else {
        auto p = getArray( procId, expr );
        return p.first->offset + p.second;
    }
}

std::ostream& operator<<( std::ostream& o, Evaluator& e ) {
    for ( auto var : e.vars ) {
        if ( var.second.prefix == Evaluator::PrefixType::NONE ) {
            // VARIABLE
            if ( var.second.type == Evaluator::Type::SCALAR) {
                o << e.getProcessName( var.first.second ) << ".";
                o << var.first.first.getName() << " = " << *e.getValue( var.first.second, var.first.first ) << endl;
            } else if ( var.second.type == Evaluator::Type::ARRAY ) {
                for ( int i = 0; i < var.second.elementsCount; i++ ) {
                    o << e.getProcessName( var.first.second ) << ".";
                    o << var.first.first.getName() << "[" << i
                        << "] = " << e.getValue( var.first.second, var.first.first )[ i ] << endl;
                }
            }
        }
    }
    o << e.clocks;
    
    return o;
}

void Evaluator::setClockLimits( int procId, const UTAP::expression_t &expr ) {
    setClockLimits( procId, expr, fixedClockLimits );
}
void Evaluator::setClockLimits( int procId, const UTAP::expression_t &expr, vector< pair< int32_t, int32_t > > &out ) {
    if ( expr.empty() )
        return;
    switch ( expr.getSize() ) {
        case 3:
            setClockLimits( procId, expr[0], out );
            setClockLimits( procId, expr[1], out );
            setClockLimits( procId, expr[2], out );
        case 2:
            if (expr[0].getKind() == ARRAY && expr[0].getType().isClock() ) {
                const VarData &var = getVarData( procId, expr[0].getSymbol() );
                int32_t limit = eval( procId, expr[1] );

                for ( int clockIndex = var.offset; clockIndex < var.offset + var.elementsCount; ++clockIndex ) {
                    assert( clockIndex < ClockTable.size() );
                    assert( ClockTable.size() == out.size() );
                    if ( out[ clockIndex ].first < limit )
                        out[ clockIndex ].first = limit;
                    if ( out[ clockIndex ].second < limit )
                        out[ clockIndex ].second = limit;
                }
            } else if ( ( expr[0].getType().isDiff() || expr[1].getType().isDiff() ) ) {
                addCut( expr, procId, clockDiffrenceExprs );
                int32_t limit;
                int clockIndexl, clockIndexr;
                if ( expr[0].getType().isDiff() ) {
                    limit = eval( procId, expr[1] );
                    clockIndexl = resolveId( procId, expr[0][0] );
                    clockIndexr = resolveId( procId, expr[0][1] );
                } else {
                    limit = eval( procId, expr[0] );
                    clockIndexl = resolveId( procId, expr[1][0] );
                    clockIndexr = resolveId( procId, expr[1][1] );
                }
                if ( fixedClockLimits[ clockIndexl ].first < limit )
                    fixedClockLimits[ clockIndexl ].first = limit;
                if ( fixedClockLimits[ clockIndexr ].first < limit )
                    fixedClockLimits[ clockIndexr ].first = limit;
                if ( fixedClockLimits[ clockIndexl ].second < limit )
                    fixedClockLimits[ clockIndexl ].second = limit;
                if ( fixedClockLimits[ clockIndexr ].second < limit )
                    fixedClockLimits[ clockIndexr ].second = limit;
            } else if ( expr[0].getType().isClock() || expr[1].getType().isClock() ) {
                bool upper = expr.getKind() == LE || expr.getKind() == LT;
                int clockIndex;
                int32_t limit;
                if ( expr[1].getType().isClock() ) {
                    clockIndex = resolveId( procId, expr[1] );
                    limit = eval( procId, expr[0] );
                    upper = !upper;
                } else {
                    clockIndex = resolveId( procId, expr[0] );
                    limit = eval( procId, expr[1] );
                }
                if ( expr.getKind() == EQ ) {
                    if ( out[ clockIndex ].first < limit )
                        out[ clockIndex ].first = limit;
                    if ( out[ clockIndex ].second < limit )
                        out[ clockIndex ].second = limit;
                } else if ( upper ) {
                    if ( out[ clockIndex ].first < limit )
                        out[ clockIndex ].first = limit;
                } else {
                    if ( out[ clockIndex ].second < limit )
                        out[ clockIndex ].second = limit;
                }
            } else {
                setClockLimits( procId, expr[0], out );
                setClockLimits( procId, expr[1], out );
            }

            break;
        case 1:
            setClockLimits( procId, expr[0], out );
            break;
        case 0:
            break;
    }
}

