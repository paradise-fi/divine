#include <string>
#include <limits>
#include <utap/utap.h>
#include <divine/timed/clocks.h>
#include <divine/timed/utils.h>
#include <divine/timed/ura.h>

#ifndef DIVINE_TIMED_EVAL_H
#define DIVINE_TIMED_EVAL_H

namespace divine {
namespace timed {

class Evaluator {
private:
    enum class Type {
        CLOCK, SCALAR, CHANNEL, ARRAY, PROCESS
    };

    enum class PrefixType {
        CONSTANT, LOCAL, NONE
    };

    struct VarData {
        Type type;
        PrefixType prefix;

        // offset gives the position in value vector of the variable,
        // or unique id of clock or channel
        int32_t offset;
        int elementsCount;

        std::pair< int, int > ranges;

        VarData( ) {}
        VarData( Type t,
                PrefixType p,
                int32_t off,
                const std::pair< int, int > r = std::make_pair( std::numeric_limits< int >::min(),
                                                                std::numeric_limits< int >::max() ) ) :
                type( t ), prefix( p ), offset( off ), ranges( r ) {}
        VarData(    Type t, PrefixType p, int off,
                    int elC, const std::pair< int, int > r = std::make_pair(
                                                                std::numeric_limits< int >::min(),
                                                                std::numeric_limits< int >::max() ) ) :
                type( t ),
                prefix( p ),
                offset( off ),
                elementsCount( elC ),
                ranges( r ) {}

        bool inRange( int32_t value ) const {
            return value >= ranges.first && value <= ranges.second;
        }
    };

    struct FuncData {
        // NOTE: _fun_ has to be a pointer as function_t does not copy instances properly
        const UTAP::function_t *fun;

        FuncData( ) {}
        FuncData( const UTAP::function_t &f ) : fun ( &f ) {}
    };

    struct Value {
        private:

        enum Kind {
            Int, Array
        };
        const Kind kind;

        // used for values not residing in the "memory"
        const int32_t num;
        const struct Array {
            // base_ptr is nullptr for clocks or channels and it points to
            //  the first element of underlying array/record
            //  otherwise
            int32_t *base_ptr;

            // index is equal to the offset for channel or clocks expressions
            //  and to the offset *inside* the top-level array of integers
            //  (i.e. for a[1][1], where type of _a_ is int[2][3],
            //  index==4)
            int index;

            // the "type" of this value; number of elements in the
            //  value, e.g. for simple integer len==1, for int[2][3] len==6
            int len;
        } _array;

        public:

        const struct Array& array() {
            assert( kind == Array );
            return _array;
        }

        const struct Array& array() const {
            assert( kind == Array );
            return _array;
        }

        Value( int32_t val ) : kind( Int ), num( val ), _array( { nullptr, 0, 0 } ) {}
        Value( int32_t *ptr, int i, int l ) : kind( Array ), num( 0 ), _array( { ptr, i, l } ) {}

        int32_t get_int() const {
            assert( kind == Int || ( kind == Array && array().len == 1 ) );
            if ( kind == Int )
                return num;
            else
                return array().base_ptr[ array().index ];
        }
    };


    struct NamedInstance : public UTAP::instance_t {
        std::string name;

        NamedInstance( const UTAP::instance_t& inst, const std::string& n ) : UTAP::instance_t( inst ), name( n ) {}
    };

    int getRecordElementIndex( int procId, const UTAP::type_t record_type, unsigned field_index );
    int getElementCount( int procId, const UTAP::type_t t );
    const VarData &getVarData( int procId, const UTAP::symbol_t & s ) const;
    const VarData &getVarData( int procId, UTAP::expression_t expr );
    const FuncData &getFuncData( int procId, const UTAP::symbol_t & s ) const;
    Clocks clocks;

    typedef uint32_t ura_id;
    typedef std::pair< UTAP::symbol_t, int > VariableIdentifier;
    typedef std::map< VariableIdentifier, VarData > SymTable;
    typedef std::map< VariableIdentifier, FuncData > FuncTable;
    SymTable vars;
    FuncTable funs;

    int32_t *data = NULL;
    int32_t *ura_id_ptr = NULL;
    bool extrapLU = true;
    bool extrapLB = true;
    bool extrapDiag = true;
    std::vector< int32_t > initValues;
    std::vector< int32_t > metaValues; // constants + local variables

    std::vector< NamedInstance > ProcessTable;
    std::vector< const UTAP::symbol_t* > ClockTable;
    std::vector< const UTAP::symbol_t* > ChannelTable;
    std::vector< int > ChannelPriorities;

    Locations locations;
    void computeLocalBounds();

    void assign( const UTAP::expression_t& lexp, const Value &val, int pId ) {
        assign( lexp, val.get_int(), pId );
    }
    void assign( const UTAP::expression_t& lexp, int32_t val, int pId );
    void assign( const UTAP::expression_t& lexp, const UTAP::expression_t& rexp, int pId );
    static UTAP::type_t getBasicType( const UTAP::type_t &type );
    void parseArrayValue(   const UTAP::expression_t &exp,
                            std::vector< int32_t > &output,
                            int procId ) ;
    void processFunctionDecl( const UTAP::function_t &f, int procId );
    void processSingleDecl( const UTAP::symbol_t &s,
                            const UTAP::expression_t &initializer,
                            int procId,
                            bool local = false );
    Value unop( int procId, const UTAP::Constants::kind_t op, const UTAP::expression_t &a );
    Value binop( int procId, const UTAP::Constants::kind_t &op, const UTAP::expression_t &a,
            const UTAP::expression_t &b );
    Value ternop( int procId, const UTAP::Constants::kind_t op, const UTAP::expression_t &a,
                    const UTAP::expression_t &b, const UTAP::expression_t &c );
    int32_t *getValue( const VarData &var );
    int32_t *getValue( int procId, const UTAP::symbol_t& s );
    bool inRange( int procId, const UTAP::symbol_t& s, int32_t value ) const;
    std::pair < int32_t, int32_t > getRange( int procId, const UTAP::expression_t &expr );
    void collectResets( int pId, const UTAP::expression_t &e, std::vector< bool > & out );

    typedef std::vector< std::pair< int32_t, int32_t > >  limitsVector;
    // location based clock bounds
    std::vector< std::map< int32_t, limitsVector> > locClockLimits;
    // clock bounds from diagonals and property
    std::vector< std::pair< int32_t, int32_t > > fixedClockLimits;
    // other bounds
    std::vector< std::pair< int32_t, int32_t > > generalClockLimits;
    void computeLocClockLimits();
    void computeChannelPriorities( UTAP::TimedAutomataSystem &sys );
	void finalize();

    struct StatementInfo {
        const UTAP::Statement *stmt;
        int iterCount;
        StatementInfo( ) { }
        StatementInfo( const UTAP::Statement* s, int ic = 0 ) : stmt( s ), iterCount( ic ) {}
    };

    void pushStatements( int, std::vector< StatementInfo > &, StatementInfo );
    void emitError( int code ) __attribute__((noreturn));
    void setClockLimits( int procId, const UTAP::expression_t &exp, std::vector< std::pair< int32_t, int32_t > > & );
    Value evalFunCall( int procId, const UTAP::expression_t &exp );
    int resolveId( int procId, const UTAP::expression_t& expr );

public:
    Evaluator(  ) {}

    Ura::ura ura;
    int &getUraStateId() {
        assert( ura_id_ptr );
        return *ura_id_ptr;
    }

    bool hasClocks() const {
        return !ClockTable.empty();
    }

    int getUraStateId() const {
        assert( ura_id_ptr );
        return *ura_id_ptr;
    }

    void finalizeUra() {
        ura.finalize();
    }

    const dbm::dbm_t getUraZone() const {
        ura_id uid = getUraStateId();
        assert( uid < ura.uppaal_dbm_rep.size() );
        return ura.uppaal_dbm_rep[ uid ];
    }

    // adds all clock comparisons to URA
    void parsePropClockGuard( const UTAP::expression_t &expr );

    // returns mask of clocks that was reseted in _effect_
    unsigned getResetedMask( int procId, const UTAP::expression_t effect );

    // contains all inequalities involving clocks differences; filled by processDecl and setClockLimits; not internally used
    std::vector< Cut > clockDiffrenceExprs;

    // finalize zones
    void extrapolate();

    // set data and locations to work on, resets error flag
    void setData( char *d, Locations );

    // read global system declarations
    void processDeclGlobals( UTAP::TimedAutomataSystem &sys );

    // read individual process instances; processDeclGlobals has to be called first
    void processDecl( const std::vector< UTAP::instance_t > &procs );

    // evaluate expression and return its value
    Value eval( int procId, const UTAP::expression_t& expr );

    // run command
    void evalCmd( int procId, const UTAP::expression_t& expr );

    // read inequalities from an expression and update global clock limits if necessary; used for properties
    void setClockLimits( int procId, const UTAP::expression_t &exp );

    // evaluate boolean expression
    bool evalBool( int procId, const UTAP::expression_t& expr ) {
        assert( (computeLocalBounds(), true) ); // computeLocalBounds() has to be called so asserts in Clocks::constrain* work
        return eval( procId, expr ).get_int();
    }

    // set variables and clocks to their initial values
    void initial() {
        memcpy( data, &initValues[0], initValues.size() * sizeof(int32_t) );
        clocks.initial();
        ura_id_ptr = data + getReqSize() - sizeof( int32_t );
        getUraStateId() = 0;
    }

    friend std::ostream& operator<<( std::ostream& o, Evaluator& e );

    // perform clock "up" operation (release upper constraints)
    void up() {
        assert( !clocks.isEmpty() );
        clocks.up();
    }

    Federation getFederation() const {
        return clocks.createFed();
    }

    Federation zoneIntersection( Federation fed ) const {
        return clocks.intersection( fed );
    }

    dbm::dbm_t dbmIntersection( dbm::dbm_t d ) const {
        return clocks.intersection( d );
    }

    void assignZone( const raw_t* src ) {
        clocks.assignZone( src );
    }

    void assignZone( const dbm::dbm_t &src ) {
        clocks.assignZone( src );
    }

    // return number of bytes required to store variable values and clock zone
    unsigned int getReqSize() const {
        return initValues.size() * sizeof(int32_t) + clocks.getReqSize() + sizeof(ura_id);
    }

    // returns range of the given type
    std::pair< int, int > evalRange( int procId, const UTAP::type_t &type );

    int evalChan( int procId, const UTAP::expression_t& expr ) {
        // evaluate expression and return unique channel identifier that can be used
        // to call isChanUrgent, isChanBcast and getChanPriority
        assert( expr.getType().is( UTAP::Constants::CHANNEL ) );
        return resolveId( procId, expr );
    }

    bool isChanUrgent( int ch ) const;
    bool isChanBcast( int ch ) const;
    int getChanPriority( int ch ) const {
        return ChannelPriorities[ ch ];
    };

    std::string getProcessName( int p ) const {
        if ( p == -1 )
            return "";
        else
            return ProcessTable[p].name;
    }

    void addSymbol( const UTAP::symbol_t &s, int procId = -1, const UTAP::expression_t &init = UTAP::expression_t() ) {
        processSingleDecl( s, init, procId );
		finalize();
    }

    void enableLU( bool enable ) {
        extrapLU = enable;
    }

    bool usesLU() const {
        return extrapLU;
    }

    void enableLB( bool enable ) {
        extrapLB = enable;
    }

    bool usesLB() const {
        return extrapLB;
    }

    void enableDiag( bool enable ) {
        extrapDiag = enable;
    }

    bool usesDiag() const {
        return extrapDiag;
    }

    // returns DBM offset and size of each row
    std::pair< unsigned int, unsigned int > splitPoints() const {
        unsigned int row = ClockTable.size() + 1;
        assert( clocks.getReqSize() == row * row * 4 );
        return std::make_pair( getReqSize() - clocks.getReqSize(), row * 4 );
    }
};

}
}

#endif

