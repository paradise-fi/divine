#include <string>
#include <utap/utap.h>
#include <limits>
#include "clocks.h"
#include "utils.h"

#ifndef DIVINE_TIMED_EVAL_H
#define DIVINE_TIMED_EVAL_H

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
        int32_t offset; // used by variables
        std::vector< int > arraySizes;
        int elementsCount;

        std::pair< int, int > ranges;
       
        VarData( ) {}
        VarData( Type t,
                PrefixType p,
                int32_t off,
                const std::pair< int, int > r = std::make_pair( std::numeric_limits< int >::min(),
                                                                std::numeric_limits< int >::max() ) ) :
                type( t ), prefix( p ), offset( off ), ranges( r ) {}
        VarData(    Type t, PrefixType p, int off, const std::vector< int > &aS,
                    int elC, const std::pair< int, int > r = std::make_pair( 
                                                                std::numeric_limits< int >::min(),
                                                                std::numeric_limits< int >::max() ) ) :
                type( t ),
                prefix( p ),
                offset( off ),
                arraySizes( aS ),
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

    const VarData &getVarData( int procId, const UTAP::symbol_t & s ) const;
    const VarData &getVarData( int procId, const UTAP::expression_t &expr );
    const FuncData &getFuncData( int procId, const UTAP::symbol_t & s ) const;
    std::pair< const VarData*, int > getArray( int procId, const UTAP::expression_t& e, int *pSize = NULL ) ;
    Clocks clocks;

    typedef std::pair< UTAP::symbol_t, int > VariableIdentifier;
    typedef std::map< VariableIdentifier, VarData > SymTable;
    typedef std::map< VariableIdentifier, FuncData > FuncTable;
    SymTable vars;
    FuncTable funs;

    int32_t *data;
    std::vector< int32_t > initValues;
    std::vector< int32_t > metaValues; // constants + local variables

    std::vector< UTAP::instance_t > ProcessTable;
    std::vector< const UTAP::symbol_t* > ClockTable;
    std::vector< const UTAP::symbol_t* > ChannelTable;
    std::vector< int > ChannelPriorities;

    Locations locations;
    void computeLocalBounds();

    /*
     *  getArraySizes returns the size of the whole array (i.e. for a[2][3] 
     *  returns 6). Moreover, vector _output_ is filled with the size for each dimension.
     */

    int getArraySizes( int procId, const UTAP::type_t &type, std::vector< int > &output );
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
    int32_t unop( int procId, const UTAP::Constants::kind_t op, const UTAP::expression_t &a );
    int32_t binop( int procId, const UTAP::Constants::kind_t &op, const UTAP::expression_t &a,
            const UTAP::expression_t &b );
    int32_t ternop( int procId, const UTAP::Constants::kind_t op, const UTAP::expression_t &a,
                    const UTAP::expression_t &b, const UTAP::expression_t &c );
    int32_t *getValue( const VarData &var );
    int32_t *getValue( int procId, const UTAP::symbol_t& s );
    bool inRange( int procId, const UTAP::symbol_t& s, int32_t value ) const;
    void collectResets( int pId, const UTAP::template_t &templ, const UTAP::edge_t &e, std::vector< bool > & out );

    typedef std::vector< std::pair< int32_t, int32_t > >  limitsVector;
    std::vector< std::map< int32_t, limitsVector> > locClockLimits;
    std::vector< std::pair< int32_t, int32_t > > fixedClockLimits;
    void computeLocClockLimits( const std::vector< UTAP::instance_t > &procs );
    void computeChannelPriorities( UTAP::TimedAutomataSystem &sys );
   
    struct StatementInfo {
        const UTAP::Statement *stmt;
        int iterCount;
        StatementInfo( ) { }
        StatementInfo( const UTAP::Statement* s, int ic = 0 ) : stmt( s ), iterCount( ic ) {}
    };
    void pushStatements( int, std::vector< StatementInfo > &, StatementInfo );
    void emitError( int code );
    void setClockLimits( int procId, const UTAP::expression_t &exp, std::vector< std::pair< int32_t, int32_t > > & );
    int32_t evalFunCall( int procId, const UTAP::expression_t &exp );
    int resolveId( int procId, const UTAP::expression_t& expr );

public:
    Evaluator(  ) {}

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
    int32_t eval( int procId, const UTAP::expression_t& expr );

    // run command
    void evalCmd( int procId, const UTAP::expression_t& expr );

    // read inequalities from an expression and update global clock limits if necessary; used for properties
    void setClockLimits( int procId, const UTAP::expression_t &exp );

    // evaluate boolean expression
    bool evalBool( int procId, const UTAP::expression_t& expr ) {
        assert( (computeLocalBounds(), true) ); // computeLocalBounds() has to be called so asserts in Clocks::constrain* work
        return eval( procId, expr );
    }

    // set variables and clocks to their initial values
    void initial() {
        memcpy( data, &initValues[0], initValues.size() * sizeof(int32_t) );
        clocks.initial();
    }

    friend std::ostream& operator<<( std::ostream& o, Evaluator& e );

    // perform clock "up" operation (release upper constraints)
    void up() {
        clocks.up();
    }

    // return number of bytes required to store variable values and clock zone
    unsigned int getReqSize() {
        return initValues.size() * sizeof(int32_t) + clocks.getReqSize();
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
        if ( p == -1)
            return "global";
        else
            return ProcessTable[p].uid.getName();
    }

    enum { ERR_DIVIDE_BY_ZERO = 100, ERR_ARRAY_INDEX, ERR_OUT_OF_RANGE, ERR_SHIFT, ERR_NEG_CLOCK };

    // error flag; can be set by eval* calls and is reset to 0 when setData is called
    int error;
};

#endif

