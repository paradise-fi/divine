#include <tools/compile.h>
#include <divine/dve/compiler.h>
#include <divine/generator/common.h>
//*
using namespace wibble::str;

namespace divine {
const char *compile_defines_str = "\
#define assert_eq(a,b) assert(a == b)\n\
#define assert_neq(a,b) assert(a != b);\n\
#define assert_leq(a,b) assert(a <= b);\n\
#define assert_die() assert(false);\n\
#define DIVINE_EMBED\n\
#define O_POOLS\n"; // required

namespace dve {

namespace compiler {

void DveCompiler::write_C( parse::LValue & expr, std::ostream & ostr, std::string state_name, std::string context  ) {
    ostr << state_name << "." << getVariable( expr.ident.name(), context );
    if ( expr.idx.valid() ) {
        ostr << "[ ";
        write_C( expr.idx, ostr, state_name, context );
        ostr << " ]";
    }
}

void DveCompiler::write_C( parse::RValue & expr, std::ostream & ostr,
                           std::string state_name, std::string context  )
{
    if ( expr.ident.valid() ) {
        ostr << state_name << "." << getVariable( expr.ident.name(), context );
        if ( expr.idx && expr.idx->valid() ) {
            ostr << "[ ";
            write_C( *expr.idx, ostr, state_name, context );
            ostr << " ]";
        }
    } else {
        ostr << expr.value.value;
    }
}

void DveCompiler::write_C( parse::Expression & expr, ostream& ostr, string state_name, std::string context )
{
    std::map< TI::TokenId, const char * > op;

    op[ TI::Lt ] = "<"; op[ TI::LEq ] = "<=";
    op[ TI::Eq ] = "=="; op[ TI::NEq ] = "!=";
    op[ TI::Gt ] = ">"; op[ TI::GEq ] = ">=";

    op[ TI::Plus ] = "+"; op[ TI::Minus ] = "-";
    op[ TI::Mult ] = "*"; op[ TI::Div ] = "/"; op[ TI::Mod ] = "%";

    op[ TI::And ] = "&"; op[ TI::Or ] = "|"; op[ TI::Xor ] = "^";
    op[ TI::LShift ] = "<<"; op[ TI::RShift ] = ">>";

    op[ TI::Bool_And ] = "&&"; op[ TI::Bool_Or ] = "||";

    op[ TI::Assignment ] = "=";

    op[ TI::Bool_Not ] = "!"; op[ TI::Tilde ] = "~";
    op[ TI::Invalid ] = "XXX";

    assert( op.count( expr.op.id ) );
    if ( expr.lhs && expr.rhs ) {
        ostr << "(";
        switch ( expr.op.id ) {
            case TI::Period:
            {
                parse::Process &p = getProcess( expr.lhs->rval->ident.name() );
                ostr << in_state( p, getStateId( p, expr.rhs->rval->ident.name()),
                                  state_name );
            }
                break;
            case TI::Arrow:
                write_C( *expr.rhs, ostr, state_name, expr.lhs->rval->ident.name() );
                break;
            case TI::Imply:
                ostr << "!( ";
                write_C( *expr.lhs, ostr, state_name, context );
                ostr << " ) || ( ";
                write_C( *expr.rhs, ostr, state_name, context );
                ostr << " )";
                break;
            default:
                write_C( *expr.lhs, ostr, state_name, context );
                ostr << " " << op[ expr.op.id ] << " ";
                write_C( *expr.rhs, ostr, state_name, context );
        }
        ostr << ")";
    }
    else if ( expr.lhs ) {
        ostr << op[ expr.op.id ] << "(";
        write_C( *expr.lhs, ostr, state_name, context );
        ostr << " )";
    }
    else
        write_C( *expr.rval, ostr, state_name, context );

}

std::string DveCompiler::cexpr( divine::dve::parse::Expression& expr, string state, string context )
{
    std::stringstream str;
    str << "(";
    write_C( expr, str, state.c_str(), context );
    str << ")";
    return str.str();
}

std::string DveCompiler::cexpr( divine::dve::parse::LValue& expr, std::string state, string context )
{
    std::stringstream str;
    str << "(";
    write_C( expr, str, state.c_str(), context );
    str << ")";
    return str.str();
}


void DveCompiler::gen_header()
{
    line( "#include <stdio.h>" );
    line( "#include <string.h>" );
    line( "#include <stdint.h>" );
    line();

    line( "typedef uint64_t ulong_long_int_t;" );
    line( "typedef int64_t slong_long_int_t;" );
    line( "typedef uint32_t ulong_int_t;" );
    line( "typedef int32_t slong_int_t;" );
    line( "typedef uint16_t ushort_int_t;" );
    line( "typedef int16_t sshort_int_t;" );
    line( "typedef uint8_t byte_t;" );
    line( "typedef uint8_t ubyte_t;" );
    line( "typedef int8_t sbyte_t;" );
    line( "typedef size_t size_int_t;" );
    line();

    line( compile_defines_str );

    line( divine::cesmi_usr_cesmi_h_str );
    line();
}

const char *typeOf( int size ) {
    if ( size == 1 ) return "uint8_t";
    if ( size == 2 ) return "int16_t";
    return 0;
}

int sizeOf( int tid ) {
    if ( tid == 0 ) return sizeof(uint8_t);
    if ( tid == 1 ) return sizeof(int16_t);
    return 0;
}

void DveCompiler::gen_constants()
{
    for ( parse::Declaration &decl : ast->decls ) {
        if ( !decl.is_const )
            continue;
        append( "const" );
        append( typeOf( decl.width ) );
        append( " " );
        append( decl.name );
        if ( decl.is_array ) {
            append( "[" + fmt( decl.size ) + "]" );
            if ( decl.initial.size() > 0 ) {
                append( " = {" );
                bool first = true;
                for ( auto it = decl.initial.begin(); it != decl.initial.end(); it++ ) {
                    if ( !first )
                        append( ", " );
                    else
                        first = false;
                    std::stringstream strstr;
                    it->dump( strstr );
                    append( strstr.str() );
                }
                append( "}" );
            }
        }
        else if ( decl.initial.size() > 0 ) {
            std::stringstream strstr;
            decl.initial.begin()->dump( strstr );
            append( string( " = " ) + strstr.str() );
        }
        line( ";" );
    }
    line();
}

void DveCompiler::ensure_process() {
    if ( !in_process || !process_empty )
        return;
    line( "struct" );
    block_begin();
    process_empty = false;
}

void DveCompiler::start_process() {
    in_process = true;
    process_empty = true;
}

void DveCompiler::end_process( std::string name ) {
    in_process = false;
    if ( process_empty )
        return;
    block_end();
    line( "__attribute__((__packed__)) " + name + ";" );
}

void DveCompiler::gen_state_struct()
{
    string name;
    string process_name = "UNINITIALIZED";
    line( "struct state_struct_t" );
    block_begin();

    line( "struct" ); block_begin();
    int total_bits = 0;

    for ( parse::Process &p : ast->processes ) {
        int max = p.states.size();
        int bits = 1;
        while ( max /= 2 ) ++ bits;
        total_bits += bits;
        declare( "uint16_t", p.name.name() + ":" + fmt( bits ) );
    }

    block_end();
    line( "__attribute__((__packed__)) _control;" );

    for ( parse::Declaration &decl : ast->decls ) {
        if ( !decl.is_const ) {
            declare( typeOf( decl.width ), decl.name, ( decl.is_array ? decl.size : 0 ) );
        }
    }

    for ( parse::ChannelDeclaration &chandecl : ast->chandecls ) {
        if ( !chandecl.is_buffered )
            continue;
        line( "struct" );
        block_begin();
        declare( "uint16_t", "number_of_items" );
        line( "struct" );
        block_begin();
        for ( size_t j = 0; j < chandecl.components.size(); j++ ) {
            declare( typeOf( chandecl.components[j] ), "x" + fmt( j ) );
        }
        block_end();
        line( "content[" + fmt( chandecl.size ) + "];" );
        block_end();
        line( "__attribute__((__packed__)) " + chandecl.name + ";" );
    }

    for ( parse::Process &p : ast->processes ) {
        start_process();
        for ( parse::Declaration &decl : p.decls ) {
            ensure_process();
            declare( typeOf( decl.width ), decl.name, ( decl.is_array ? decl.size : 0 ) );
        }
        end_process( p.name.name() );
    }

    block_end();
    line( "__attribute__((__packed__));" );

    line( "int state_size = sizeof(state_struct_t);" );
    line();
}

void DveCompiler::gen_initial_state()
{
    //setAllocator( new Allocator );

    append( "int get_initial( cesmi_setup *setup, int handle, cesmi_node *out )" );

    block_begin();
    line( "if ( handle != 1 ) return 0;" );
    line( "*out = setup->make_node( setup->allocation_handle, state_size );" );
    line( "memset( out->memory, 0, state_size );" );
    line( "state_struct_t &_out = *reinterpret_cast< state_struct_t * >( out->memory );");

    std::string process = "_out";
    for ( parse::Declaration &decl : ast->decls ) {
        std::string var = process + "." + decl.name;
        if ( decl.is_array ) {
            for ( size_t i = 0; i < decl.initial.size(); i++ ) {
                std::stringstream strstr;
                decl.initial[ i ].dump( strstr );
                assign( var + "[" + fmt( i ) + "]", strstr.str() );
            }
        }
        else {
            if ( decl.initial.size() > 0 ) {
                std::stringstream strstr;
                decl.initial[ 0 ].dump( strstr );
                assign( var, strstr.str() );
            }
        }
    }

    for ( parse::Process &p : ast->processes ) {
        process = std::string( "_out." ) + p.name.name();
        for ( parse::Declaration &decl : ast->decls ) {
            std::string var = process + "." + decl.name;
            if ( decl.is_array ) {
                for ( size_t i = 0; i < decl.initial.size(); i++ ) {
                    std::stringstream strstr;
                    decl.initial[ i ].dump( strstr );
                    assign( var + "[" + fmt( i ) + "]", strstr.str() );
                }
            }
            else {
                if ( decl.initial.size() > 0 ) {
                    std::stringstream strstr;
                    decl.initial[ 0 ].dump( strstr );
                    assign( var, strstr.str() );
                }
            }
        }
        for ( int i = 0; i < p.states.size(); i++ ) {
            if ( p.states[ i ].name() == p.inits[ 0 ].name() ) {
                assign( process_state( p, "_out" ), fmt( i ) );
            }
        }
    }
    line( "return 2;" );
    block_end();

    line();
}

void DveCompiler::analyse_transition(
    parse::Transition * t,
    vector< ExtTransition > &ext_transition_vector )
{
    if ( !t->syncexpr.valid() || t->syncexpr.write || chanIsBuffered( t->syncexpr.chan ) ) {
        // transition not of type SYNC_ASK
        if ( !have_property ) {
            // no properties, just add to ext_transition vector
            ExtTransition extTrans;
            extTrans.synchronized = false;
            extTrans.first = t;
            ext_transition_vector.push_back( extTrans );
        }
        else {
            // this transition is not a property, but there are properties
            // forall properties, add this transition to ext_transition_vector
            for( iter_property_transitions = property_transitions.begin();
                iter_property_transitions != property_transitions.end();
                iter_property_transitions++ )
            {
                ExtTransition extTrans;
                extTrans.synchronized = false;
                extTrans.first = t;
                extTrans.property = (*iter_property_transitions);
                ext_transition_vector.push_back( extTrans );
            }
        }
    }
    else {
        // transition of type SYNC_ASK
        iter_channel_map = channel_map.find( t->syncexpr.chan.name() );
        if ( iter_channel_map != channel_map.end() ) {
            // channel of this transition is found
            // (strange test, no else part for if statement)
            // assume: channel should always be present
            // forall transitions that also use this channel, add to ext_transitions
            for ( iter_transition_vector  = iter_channel_map->second.begin();
                iter_transition_vector != iter_channel_map->second.end();
                iter_transition_vector++ )
            {
                if ( t->proc.name() != (*iter_transition_vector)->proc.name() ) {
                    if ( !have_property ) {
                        // system has no properties, so add only once without property
                        ExtTransition extTrans;
                        extTrans.synchronized = true;
                        extTrans.first = t;
                        extTrans.second = (*iter_transition_vector);
                        ext_transition_vector.push_back( extTrans );
                    }
                    else {
                        // system has properties, so forall properties, add the combination if this transition,
                        // the transition that also uses this channel and the property
                        for(iter_property_transitions = property_transitions.begin();
                            iter_property_transitions != property_transitions.end();
                            iter_property_transitions++)
                        {
                            ExtTransition extTrans;
                            extTrans.synchronized = true;
                            extTrans.first = t;
                            extTrans.second = (*iter_transition_vector);
                            extTrans.property = (*iter_property_transitions);
                            ext_transition_vector.push_back( extTrans );
                        }
                    }
                }
            }
        }
    }
}

bool DveCompiler::chanIsBuffered( parse::Identifier chan ) {
    for ( parse::ChannelDeclaration &chandecl : ast->chandecls ) {
        if ( chandecl.name == chan.name() ) {
            return chandecl.is_buffered;
        }
    }
    std::cerr << "Internal error" << std::endl;
    exit( -1 );
}

void DveCompiler::analyse()
{
    parse::Transition * transition;
    have_property = ast->property.valid();

    // obtain transition with synchronization of the type SYNC_EXCLAIM and property transitions
    for ( parse::Process &p : ast->processes ) {
        for ( parse::Transition &t : p.trans ) {
            if ( t.syncexpr.valid() && t.syncexpr.write && !chanIsBuffered( t.syncexpr.chan ) ) {
                iter_channel_map = channel_map.find( t.syncexpr.chan.name() );
                if ( iter_channel_map == channel_map.end() ) {
                    vector< parse::Transition * > transitionVector;
                    transitionVector.push_back( &t );
                    channel_map.insert( pair< std::string, vector< parse::Transition * > >(
                        t.syncexpr.chan.name(), transitionVector
                    ) );
                }
                else {
                    iter_channel_map->second.push_back( &t );
                }
            }

            if ( p.name.name() == ast->property.name() ) {
                property_transitions.push_back( &t );
            }
        }
    }
    for ( parse::Process &p : ast->processes ) {
        for ( parse::Transition &t : p.trans ) {
            if (
                    (!t.syncexpr.valid() || !t.syncexpr.write || chanIsBuffered( t.syncexpr.chan ) ) 
                    && ( p.name.name() != ast->property.name() )
            ) {
                // not syncronized sender without buffer and not a property transition
                iter_transition_map = transition_map.find( p.name.name() );

                //new process it means that new state in process is also new
                if ( iter_transition_map == transition_map.end() ) {
                    // new process, add to transition map
                    map< std::string, vector< ExtTransition > > processTransitionMap;
                    vector< ExtTransition > extTransitionVector;

                    analyse_transition( &t, extTransitionVector );

                    // for this process state, add the ext transitions
                    processTransitionMap.insert(
                        pair< std::string, vector< ExtTransition > >(
                            t.from.name(), extTransitionVector
                        )
                    );
                    // then add this vector to the transition map for this process
                    transition_map.insert(
                        pair< std::string, map< std::string, vector< ExtTransition > > >(
                            p.name.name(), processTransitionMap
                        )
                    );
                }
                else {
                    // existing process, find process_transition_map
                    iter_process_transition_map = iter_transition_map->second.find( t.from.name() );

                    //new state in current process
                    if ( iter_process_transition_map == iter_transition_map->second.end() ) {
                        vector< ExtTransition > extTransitionVector;
                        analyse_transition( &t, extTransitionVector );

                        // and reinsert result
                        iter_transition_map->second.insert(
                            pair< std::string, vector< ExtTransition > >(
                                t.from.name(), extTransitionVector
                            )
                        );
                    }
                    else
                        analyse_transition( &t, iter_process_transition_map->second );
                }
            }
        }
    }

}

void DveCompiler::transition_guard( ExtTransition * et, std::string in )
{
    if_begin( false );
    for ( parse::Expression &exp : et->first->guards )
        if_cexpr_clause( &exp, in, et->first->proc.name() );

    if( et->synchronized )
    {
        if_clause( in_state( getProcess( et->second->proc.name() ),
                             getStateId( getProcess( et->second->proc.name() ), et->second->from.name() ), in ) );
        for ( parse::Expression &exp : et->second->guards )
            if_cexpr_clause( &exp, in, et->second->proc.name() );
    }
    else
    {
        if ( et->first->syncexpr.valid() ) {
            std::string chan = et->first->syncexpr.chan.name();
            if ( et->first->syncexpr.write ) {
                if_clause( relate( channel_items( chan, in ), "<", fmt( channel_capacity( chan ) ) ) );
            }
            else {
                if_clause( relate( channel_items( chan, in ), ">", fmt( 0 ) ) );
            }
        }
    }
    if(have_property)
    {
        parse::Process &property = getProcess( ast->property.name() );
        if_clause( in_state( property,
                             getStateId( property, et->property->from.name() ), in ) );
        for ( parse::Expression &exp : et->property->guards )
            if_cexpr_clause( &exp, in, et->property->proc.name() );
    }

    if_end();
}

void DveCompiler::transition_effect( ExtTransition * et, string in, string out )
{
    if(et->synchronized)
    {
        for ( size_t s = 0; s < et->first->syncexpr.lvallist.lvlist.size(); s++ )
            assign(
                cexpr( et->first->syncexpr.lvallist.lvlist[ s ], out , et->first->proc.name() ),
                cexpr( et->second->syncexpr.exprlist.explist[ s ], in, et->second->proc.name() )
            );
    }
    else if ( et->first->syncexpr.valid() )
    {
        std::string chan = et->first->syncexpr.chan.name();
        if( et->first->syncexpr.write )
        {
            for ( size_t s = 0; s < et->first->syncexpr.exprlist.explist.size(); s++ ) {
                assign(
                    channel_item_at( chan, channel_items( chan, in ), s, out ),
                    cexpr( et->first->syncexpr.exprlist.explist[ s ], in, et->first->proc.name() )
                );
            }
            line( channel_items( chan, out ) + "++;" );
        }
        else
        {
            for ( size_t s = 0; s < et->first->syncexpr.lvallist.lvlist.size(); s++ ) {
                assign(
                    cexpr( et->first->syncexpr.lvallist.lvlist[ s ], out, et->first->proc.name() ),
                    channel_item_at( chan, "0", s, in )
                );
            }
            line( channel_items( chan, out ) + "--;" );

            line( "for(size_t i = 1 ; i <= " + channel_items( chan, out ) + "; i++)" );
            block_begin();
            for(size_t s = 0;s < et->first->syncexpr.lvallist.lvlist.size() ;s++)
            {
                assign( channel_item_at( chan, "i-1", s, out ), channel_item_at( chan, "i", s, in ) );
                assign( channel_item_at( chan, "i", s, out ), "0" );
            }
            block_end();
        }
    }

    //first transition effect
    assign(
        process_state( getProcess( et->first->proc.name() ), out ),
        fmt( getStateId( getProcess( et->first->proc.name() ), et->first->to.name() ) )
    );

    for ( parse::Assignment &effect : et->first->effects ) {
        assign( cexpr( effect.lhs, out, et->first->proc.name() ), cexpr( effect.rhs, out, et->first->proc.name() ) );
    }

    if( et->synchronized ) //second transiton effect
    {
        assign(
            process_state( getProcess( et->second->proc.name() ), out ),
            fmt( getStateId( getProcess( et->second->proc.name() ), et->second->to.name() ) )
        );

        for ( parse::Assignment &effect : et->second->effects ) {
            assign( cexpr( effect.lhs, out, et->second->proc.name() ), cexpr( effect.rhs, out, et->second->proc.name() ) );
        }
    }

    if( have_property ) //change of the property process state
        assign(
            process_state( getProcess( et->property->proc.name() ), out ),
            fmt( getStateId( getProcess( et->property->proc.name() ), et->property->to.name() ) )
        );
}

void DveCompiler::new_output_state() {
    line( "cesmi_node blob_out = setup->make_node( setup->allocation_handle, state_size );" );
    line( "state_struct_t *out = reinterpret_cast< state_struct_t * >( blob_out.memory );" );
    line( "*out = *in;" );
}

void DveCompiler::yield_state() {
    line( "*to = blob_out;" );
    line( "return " + fmt( current_label ) + ";" );
}

void DveCompiler::gen_successors()
{
    string in = "(*in)", out = "(*out)", space = "";

    new_label();
    if_begin( true );

    for ( parse::Process &p : ast->processes ) {
        for ( size_t i = 0; i < p.states.size(); i++ ) {
            if ( isCommited( p, i ) )
                if_clause( in_state( p, i, in ) );
        }
    }
    if_end(); block_begin(); // committed states

    for ( parse::Process &p : ast->processes ) {
        if ( transition_map.find( p.name.name() ) != transition_map.end() && !is_property( p ) ) {
            for (
                    iter_process_transition_map = transition_map.find( p.name.name() )->second.begin();
                    iter_process_transition_map != transition_map.find( p.name.name() )->second.end();
                    iter_process_transition_map++
            ) {
                if ( isCommited( p, iter_process_transition_map->first ) ) {
                    int stateID = getStateId( p, iter_process_transition_map->first );

                    new_label();

                    if_begin( true );
                    if_clause( in_state( p, stateID, in ) );
                    if_end(); block_begin();

                    for (
                            iter_ext_transition_vector = iter_process_transition_map->second.begin();
                            iter_ext_transition_vector != iter_process_transition_map->second.end();
                            iter_ext_transition_vector++
                    ) {
                        // !! jak je to s property synchronizaci v comitted stavech !!
                        if ( !iter_ext_transition_vector->synchronized
                            || isCommited( getProcess( iter_ext_transition_vector->second->proc.name() ),
                                iter_ext_transition_vector->second->from.name()
                            )
                        ) {
                            new_label();

                            transition_guard( &*iter_ext_transition_vector, in );
                            block_begin();
                            new_output_state();
                            transition_effect( &*iter_ext_transition_vector, in, out );
                            yield_state();
                            block_end();
                        }
                    }

                    block_end();
                }
            }
        }
    }

    new_label(); // Trick. : - )
    line( ";" );

    block_end();
    line( "else" );
    block_begin();

    for ( parse::Process &p : ast->processes ) {
        if ( transition_map.find( p.name.name() ) != transition_map.end() && !is_property( p ) ) {
            for (
                    iter_process_transition_map = transition_map.find( p.name.name() )->second.begin();
                    iter_process_transition_map != transition_map.find( p.name.name() )->second.end();
                    iter_process_transition_map++
            ) {
                int stateID = getStateId( p, iter_process_transition_map->first );

                new_label();

                if_begin( true );
                if_clause( in_state( p, stateID, in ) );
                if_end(); block_begin();

                for (
                            iter_ext_transition_vector = iter_process_transition_map->second.begin();
                            iter_ext_transition_vector != iter_process_transition_map->second.end();
                            iter_ext_transition_vector++
                ) {
                    new_label();

                    transition_guard( &*iter_ext_transition_vector, in );
                    block_begin();
                    new_output_state();
                    transition_effect( &*iter_ext_transition_vector, in, out );
                    line( "system_in_deadlock = false;" );
                    yield_state();
                    block_end();
                }
                block_end();
            }
        }
    }
    block_end();

    new_label();

    if_begin( true );
    if_clause( "system_in_deadlock" );
    if_end(); block_begin();

    parse::Process &property = getProcess( ast->property.name() );

    for(iter_property_transitions = property_transitions.begin();
        iter_property_transitions != property_transitions.end();
        iter_property_transitions++)
    {
        new_label();
        if_begin( false );

        if_clause( in_state( property, getStateId( property, (*iter_property_transitions)->from.name() ), in ) );
        for ( parse::Expression &exp : (*iter_property_transitions)->guards )
            if_cexpr_clause( &exp, in, ast->property.name() );

        if_end(); block_begin();
        new_output_state();

        assign( process_state( property, out ), fmt( getStateId( property, (*iter_property_transitions)->to.name() ) ) );

        yield_state();
        block_end();
    }
    block_end();
}

void DveCompiler::gen_is_accepting()
{
    // not compulsory, so don't bother if not needed
    if(!have_property)
        return;

    line( "uint64_t get_flags( cesmi_setup *setup, cesmi_node n )" );
    block_begin();

    line( "state_struct_t &state = *reinterpret_cast< state_struct_t * >( n.memory );" );
    size_t i;

    parse::Process &property = getProcess( ast->property.name() );

    for ( i = 0; i < property.states.size(); i++ ) {
       if ( isAccepting( property, i ) ) {
           if_begin( true );
           if_clause( in_state( property, i, "state" ) );
           if_end();
           line( "    return cesmi_accepting;" );
       }
    }

    line( "return 0;" );
    block_end();

    line();
}

void DveCompiler::print_generator()
{
    gen_header();
    gen_constants();
    gen_state_struct();
    gen_initial_state();

    line( "void setup( cesmi_setup *setup ) {" );
    line( "    setup->property_count = 1 + " + fmt( have_property ) + ";" );
    line( "}" );

    many = false;
    current_label = 1;

    gen_is_accepting();

    line( "int get_successor( cesmi_setup *setup, int next_state, cesmi_node from, cesmi_node *to ) " );
    block_begin();
    line( "const state_struct_t *in = reinterpret_cast< state_struct_t * >( from.memory );" );
    line( "bool system_in_deadlock = false;" );
    line( "goto switch_state;" );

    gen_successors();

    new_label();
    line( "return 0;" );

    line( "switch_state: switch( next_state )" );
    block_begin();
    for(int i=1; i < current_label; i++)
        if (i==1)
            line( "case " + fmt( i ) + ": system_in_deadlock = true; goto l" + fmt( i ) + ";" );
        else
            line( "case " + fmt( i ) + ": goto l" + fmt( i ) + ";" );
    block_end();

    block_end();
}

}

}

}

