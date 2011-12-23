#include <tools/compile.h>
#include <tools/dvecompile.h>
#include <divine/generator/common.h>

using namespace wibble::str;

namespace divine {
const char *compile_defines_str = "\
#define assert_eq(a,b) assert(a == b)\n\
#define assert_neq(a,b) assert(a != b);\n\
#define assert_leq(a,b) assert(a <= b);\n\
#define assert_die() assert(false);\n\
#define DIVINE_EMBED\n";
}

void dve_compiler::write_C(dve_expression_t & expr, std::ostream & ostr, std::string state_name)
{
    std::map< int, const char * > op;

    op[ T_LT ] = "<"; op[ T_LEQ ] = "<=";
    op[ T_EQ ] = "=="; op[ T_NEQ ] = "!=";
    op[ T_GT ] = ">"; op[ T_GEQ ] = ">=";

    op[ T_PLUS ] = "+"; op[ T_MINUS ] = "-";
    op[ T_MULT ] = "*"; op[ T_DIV ] = "/"; op[ T_MOD ] = "%";

    op[ T_AND ] = "&"; op[ T_OR ] = "|"; op[ T_XOR ] = "^";
    op[ T_LSHIFT ] = "<<"; op[ T_RSHIFT ] = ">>";

    op[ T_BOOL_AND ] = "&&"; op[ T_BOOL_OR ] = "||";

    op[ T_ASSIGNMENT ] = "=";

    dve_symbol_table_t * parent_table = expr.get_symbol_table();
    if (!parent_table) gerr << "Writing expression: Symbol table not set" << thr();
    switch (expr.get_operator())
    {
        case T_ID:
            if (!(parent_table->get_variable(expr.get_ident_gid())->is_const()))
                ostr<<state_name<<".";
            if(parent_table->get_variable(expr.get_ident_gid())->get_process_gid() != NO_ID)
            {
                ostr << parent_table->get_process(parent_table->get_variable(expr.get_ident_gid())->
                                                  get_process_gid())->get_name(); //name of process
                ostr<<".";
            }
            ostr << parent_table->get_variable(expr.get_ident_gid())->get_name();
            break;
        case T_FOREIGN_ID:
            ostr << parent_table->get_process(parent_table->get_variable(expr.get_ident_gid())->
                                              get_process_gid())->get_name(); //name of process
            ostr<<"->";
            ostr << parent_table->get_variable(expr.get_ident_gid())->get_name();
            break;
        case T_NAT:
            ostr << expr.get_value();
            break;
        case T_PARENTHESIS:
            ostr << "(";
            write_C(*expr.left(), ostr, state_name);
            ostr << ")";
            break;
        case T_SQUARE_BRACKETS:
            if (!(parent_table->get_variable(expr.get_ident_gid())->is_const()))
                ostr<<state_name<<".";
            if(parent_table->get_variable(expr.get_ident_gid())->get_process_gid() != NO_ID)
            {
                ostr << parent_table->get_process(parent_table->get_variable(expr.get_ident_gid())->
                                                  get_process_gid())->get_name(); //name of process
                ostr<<".";
            }
            ostr << parent_table->get_variable(expr.get_ident_gid())->
                get_name(); ostr<<"["; write_C(*expr.left(), ostr, state_name); ostr<<"]" ;
            break;
        case T_FOREIGN_SQUARE_BRACKETS:
            ostr << parent_table->get_process(parent_table->get_variable(expr.get_ident_gid())->
                                              get_process_gid())->get_name(); //name of preocess
            ostr<<"->";
            ostr << parent_table->get_variable(expr.get_ident_gid())->get_name();
            ostr<<"["; write_C(*expr.left(), ostr, state_name); ostr<<"]";
            break;

        case T_LT: case T_LEQ: case T_EQ: case T_NEQ: case T_GT: case T_GEQ:
        case T_PLUS: case T_MINUS: case T_MULT: case T_DIV: case T_MOD:
        case T_AND: case T_OR: case T_XOR: case T_LSHIFT: case T_RSHIFT:
        case T_BOOL_AND: case T_BOOL_OR: case T_ASSIGNMENT:
            write_C( *expr.left(), ostr, state_name );
            ostr << " " << op[ expr.get_operator() ] << " ";
            write_C( *expr.right(), ostr, state_name );
            break;

        case T_DOT:
            ostr << in_state(
                parent_table->get_state(expr.get_ident_gid())->get_process_gid(),
                parent_table->get_state(expr.get_ident_gid())->get_lid(), state_name );
            break;

        case T_IMPLY:
            write_C(*expr.left(), ostr, state_name);
            ostr<<" -> "; // FIXME this looks wrong, -> in C is dereference
            write_C(*expr.right(), ostr, state_name);
            break;
        case T_UNARY_MINUS:
            ostr<<"-";
            write_C(*expr.right(), ostr, state_name);
            break;
        case T_TILDE:
            ostr<<"~";
            write_C(*expr.right(), ostr, state_name);
            break;
        case T_BOOL_NOT:
            ostr<<" ! (";
            write_C(*expr.right(), ostr, state_name);
            ostr<< " )";
            break;
        default:
            gerr << "Problem in expression - unknown operator"
                 << " number " << expr.get_operator() << psh();
    }
}

std::string dve_compiler::cexpr( dve_expression_t & expr, std::string state )
{
    std::stringstream str;
    str << "(";
    write_C( expr, str, state.c_str() );
    str << ")";
    return str.str();
}

void dve_compiler::gen_header()
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

    line( divine::pool_h_str );
    line();

    line( divine::blob_h_str );
    line();

    line( "using namespace divine;" );

    line( divine::generator_custom_api_h_str );
    line();
}

const char *typeOf( int tid ) {
    if ( tid == 0 ) return "byte_t";
    if ( tid == 1 ) return "sshort_int_t";
    return 0;
}

int sizeOf( int tid ) {
    if ( tid == 0 ) return sizeof(byte_t);
    if ( tid == 1 ) return sizeof(sshort_int_t);
    return 0;
}

void dve_compiler::gen_constants()
{
    for (size_int_t i=0; i!=glob_var_count; i++)
    {
        dve_symbol_t * var = get_symbol_table()->get_variable(get_global_variable_gid(i));
        if (!var->is_const())
            continue;

        append( "const " );
        append( typeOf( var->get_var_type() ) );
        append( " " );
        append( var->get_name() );

        if (var->is_vector())
        {
            append( "[" + fmt( var->get_vector_size() ) + "]" );

            if ( var->get_init_expr_count() ) append( " = {" );
            for (size_int_t j=0; j!=var->get_init_expr_count(); j++)
            {
                append( var->get_init_expr(j)->to_string() );
                if (j!=(var->get_init_expr_count()-1))
                    append( ", " );
                else
                    append( "}" );
            }
        } else if ( var->get_init_expr() ) {
            append( string( " = " ) + var->get_init_expr()->to_string() );
        }
        line( ";" );
    }
    line();
}

void dve_compiler::ensure_process() {
    if ( !in_process || !process_empty )
        return;
    line( "struct" );
    block_begin();
    process_empty = false;
}

void dve_compiler::start_process() {
    in_process = true;
    process_empty = true;
}

void dve_compiler::end_process( std::string name ) {
    in_process = false;
    if ( process_empty )
        return;
    block_end();
    line( "__attribute__((__packed__)) " + name + ";" );
}

void dve_compiler::gen_state_struct()
{
    string name;
    string process_name = "UNINITIALIZED";
    line( "struct state_struct_t" );
    block_begin();

    line( "struct" ); block_begin();
    int total_bits = 0;
    for ( size_int_t i=0; i!=state_creators_count; ++i )
        if ( state_creators[i].type == state_creator_t::PROCESS_STATE ) {
            dve_symbol_t *sym = get_symbol_table()->get_process(state_creators[i].gid);
            dve_process_t *proc =
                dynamic_cast< dve_process_t * >( get_process(state_creators[i].gid) );
            int max = proc->get_state_count();
            int bits = 1;
            while ( max /= 2 ) ++ bits;
            total_bits += bits;
            declare( "uint16_t", std::string( sym->get_name() ) + ":" + fmt( bits ) );
        }

    block_end();
    line( "__attribute__((__packed__)) _control;" );

    for (size_int_t i=0; i!=state_creators_count; ++i)
    {
        int gid = state_creators[i].gid;

        switch (state_creators[i].type)
        {
            case state_creator_t::VARIABLE:
            {
                ensure_process();
                name=get_symbol_table()->get_variable(gid)->get_name();
                declare( typeOf( state_creators[i].var_type ), name,
                         state_creators[i].array_size );
            }
            break;
            case state_creator_t::PROCESS_STATE:
            {
                end_process( process_name );
                process_name =
                    get_symbol_table()->get_process(gid)->get_name();
                start_process();
            }
            break;
            case state_creator_t::CHANNEL_BUFFER:
            {
                ensure_process();
                name=get_symbol_table()->get_channel(gid)->get_name();
                line( "struct" );
                block_begin();
                declare( "ushort_int_t",  "number_of_items" );
                line( "struct" );
                block_begin();
                dve_symbol_t * symbol =
                    get_symbol_table()->get_channel(gid);
                size_int_t item_count = symbol->get_channel_type_list_size();

                for (size_int_t j=0; j<item_count; ++j) {
                    declare( typeOf( symbol->get_channel_type_list_item(j) ), "x" + fmt( j ) );
                }
                block_end();
                line( "content[" + fmt( symbol->get_channel_buffer_size() ) + "];" );
                block_end();
                line( "__attribute__((__packed__)) " + name + ";" );
            }
            break;
            default: gerr << "Unexpected error" << thr();
                break;
        };
    }

    end_process( process_name );

    block_end();
    line( "__attribute__((__packed__));" );

    line( "int state_size = sizeof(state_struct_t);" );
    line();
}

void dve_compiler::gen_initial_state()
{
    setAllocator( new Allocator );

    append( "extern \"C\" void get_initial( CustomSetup *setup, Blob *out )" );

    block_begin();
    line( "Blob b( *(setup->pool), state_size + setup->slack );" );
    line( "b.clear();" );
    line( "state_struct_t &_out = b.get< state_struct_t >( setup->slack );");

    std::string process = "_out";
    for ( size_int_t i=0; i!=state_creators_count; ++i ) {
        state_creator_t &creator = state_creators[i];
        int gid = creator.gid;
        std::string var;
        switch (creator.type)
        {
            case state_creator_t::VARIABLE:
                var = process + "." + get_symbol_table()->get_variable(gid)->get_name();
                if ( creator.array_size ) {
                    for ( int i = 0; i < initial_values_counts[gid]; ++ i )
                        assign( var + "[" + fmt(i) + "]", fmt( initial_values[gid].all_values[i] ) );
                    // the remaining items are 0
                } else {
                    if ( initial_values_counts[gid] )
                        assign( var, fmt( initial_values[gid].all_value ) );
                    // 0 otherwise
                }
                break;
            case state_creator_t::PROCESS_STATE:
                process = std::string( "_out." ) +
                          get_symbol_table()->get_process(gid)->get_name();
                assign( process_state( gid, "_out" ), fmt( initial_states[gid] ) );
                break;
            case state_creator_t::CHANNEL_BUFFER: break; /* channels are empty */
           }
    }
    line( "*out = b;" );
    block_end();

    line();
}

void dve_compiler::analyse_transition(
    dve_transition_t * transition,
    vector<ext_transition_t> &ext_transition_vector )
{
    if(!transition->is_sync_ask())
    {
        // transition not of type SYNC_ASK
        if(!have_property)
        {
            // no properties, just add to ext_transition vector
            ext_transition_t ext_transition;
            ext_transition.synchronized = false;
            ext_transition.first = transition;
            ext_transition_vector.push_back(ext_transition);
        }
        else
        {
            // this transition is not a property, but there are properties
            // forall properties, add this transition to ext_transition_vector
            for(iter_property_transitions = property_transitions.begin();
                iter_property_transitions != property_transitions.end();
                iter_property_transitions++)
            {
                ext_transition_t ext_transition;
                ext_transition.synchronized = false;
                ext_transition.first = transition;
                ext_transition.property = (*iter_property_transitions);
                ext_transition_vector.push_back(ext_transition);
            }
        }
    }
    else
    {
        // transition of type SYNC_ASK
        iter_channel_map = channel_map.find(transition->get_channel_gid());
        if(iter_channel_map != channel_map.end())
        {
            // channel of this transition is found
            // (strange test, no else part for if statement)
            // assume: channel should always be present
            // forall transitions that also use this channel, add to ext_transitions
            for(iter_transition_vector  = iter_channel_map->second.begin();
                iter_transition_vector != iter_channel_map->second.end();
                iter_transition_vector++)
            {
                if (transition->get_process_gid() != (*iter_transition_vector)->get_process_gid() ) //not synchronize with yourself
                {
                    if(!have_property)
                    {
                        // system has no properties, so add only once without property
                        ext_transition_t ext_transition;
                        ext_transition.synchronized = true;
                        ext_transition.first = transition;
                        ext_transition.second = (*iter_transition_vector);
                        ext_transition_vector.push_back(ext_transition);
                    }
                    else
                    {
                        // system has properties, so forall properties, add the combination if this transition,
                        // the transition that also uses this channel and the property
                        for(iter_property_transitions = property_transitions.begin();
                            iter_property_transitions != property_transitions.end();
                            iter_property_transitions++)
                        {
                            ext_transition_t ext_transition;
                            ext_transition.synchronized = true;
                            ext_transition.first = transition;
                            ext_transition.second = (*iter_transition_vector);
                            ext_transition.property = (*iter_property_transitions);
                            ext_transition_vector.push_back(ext_transition);
                        }
                    }
                }
            }
        }
    }
}

void dve_compiler::analyse()
{
    dve_transition_t * transition;
    have_property = get_with_property();

    // obtain transition with synchronization of the type SYNC_EXCLAIM and property transitions
    for(size_int_t i = 0; i < get_trans_count(); i++)
    {
        transition = dynamic_cast<dve_transition_t*>(get_transition(i));
        if(transition->is_sync_exclaim())
        {
            iter_channel_map = channel_map.find(transition->get_channel_gid());
            if(iter_channel_map == channel_map.end()) //new channel
            {
                vector<dve_transition_t*> transition_vector;
                transition_vector.push_back(transition);
                channel_map.insert(pair<size_int_t,vector<dve_transition_t*> >(
                                       transition->get_channel_gid(),transition_vector));
            }
            else{
                iter_channel_map->second.push_back(transition);
            }
        }

        if( is_property( transition->get_process_gid() ) )
            property_transitions.push_back(transition);
    }

    // obtain map of transitions
    for(size_int_t i = 0; i < get_trans_count(); i++)
    {
        transition = dynamic_cast<dve_transition_t*>(get_transition(i));
        if(!transition->is_sync_exclaim() && !is_property( transition->get_process_gid() ) )
        {
            // not syncronized sender without buffer and not a property transition
            iter_transition_map = transition_map.find(transition->get_process_gid());

            //new process it means that new state in process is also new
            if( iter_transition_map == transition_map.end())
            {
                // new process, add to transition map
                map<size_int_t,vector<ext_transition_t> >  process_transition_map;
                vector<ext_transition_t> ext_transition_vector;

                analyse_transition( transition, ext_transition_vector );

                // for this process state, add the ext transitions
                process_transition_map.insert(pair<size_int_t,vector<ext_transition_t> >(
                                                  transition->get_state1_lid(),ext_transition_vector));
                // then add this vector to the transition map for this process
                transition_map.insert(pair<size_int_t,map<size_int_t,vector<ext_transition_t> > >(
                                          transition->get_process_gid(),process_transition_map));
            } else {
                // existing process, find process_transition_map
                iter_process_transition_map =
                    iter_transition_map->second.find(transition->get_state1_lid());

                //new state in current process
                if( iter_process_transition_map == iter_transition_map->second.end())
                {
                    vector<ext_transition_t> ext_transition_vector;
                    analyse_transition( transition, ext_transition_vector );

                    // and reinsert result
                    iter_transition_map->second.insert(
                        pair<size_int_t,vector<ext_transition_t> >(
                            transition->get_state1_lid(),ext_transition_vector) );
                } else analyse_transition( transition, iter_process_transition_map->second );
            }
        }
    }
}

void dve_compiler::transition_guard( ext_transition_t *et, std::string in )
{
    if_begin( false );
    if_cexpr_clause( et->first->get_guard(), in );

    if( et->synchronized )
    {
        if_clause( in_state( et->second->get_process_gid(),
                             et->second->get_state1_lid(), in ) );
        if_cexpr_clause( et->second->get_guard(), in );
    }
    else
    {
        int chan = et->first->get_channel_gid();
        if(et->first->get_sync_mode() == SYNC_EXCLAIM_BUFFER)
            if_clause( relate( channel_items( chan, in ), "!=",
                               fmt( channel_capacity( chan ) ) ) );

        if(et->first->get_sync_mode() == SYNC_ASK_BUFFER)
            if_clause( relate( channel_items( chan, in ), "!=", "0" ) );
    }
    if(have_property)
    {
        if_clause( in_state( et->property->get_process_gid(),
                             et->property->get_state1_lid(), in ) );
        if_cexpr_clause( et->property->get_guard(), in );
    }

    if_end();
}

void dve_compiler::transition_effect( ext_transition_t *et, std::string in, std::string out )
{
    if(et->synchronized)
    {
        for(size_int_t s = 0;s < et->first->get_sync_expr_list_size();s++)
            assign( cexpr( *et->first->get_sync_expr_list_item(s), out ),
                    cexpr( *et->second->get_sync_expr_list_item(s), in ) );
    }
    else
    {
        int chan = et->first->get_channel_gid();
        if(et->first->get_sync_mode() == SYNC_EXCLAIM_BUFFER)
        {
            for(size_int_t s = 0;s < et->first->get_sync_expr_list_size();s++)
            {
                assign( channel_item_at( chan, channel_items( chan, in ), s, out ),
                        cexpr( *et->first->get_sync_expr_list_item( s ), in ) );
            }
            line( channel_items( chan, out ) + "++;" );
        }
        if(et->first->get_sync_mode() == SYNC_ASK_BUFFER)
        {
            for(size_int_t s = 0;s < et->first->get_sync_expr_list_size();s++)
                assign( cexpr( *et->first->get_sync_expr_list_item(s), out ),
                        channel_item_at( chan, "0", s, in ) );
            line( channel_items( chan, out ) + "--;" );

            line( "for(size_int_t i = 1 ; i <= " + channel_items( chan, out ) + "; i++)" );
            block_begin();
            for(size_int_t s = 0;s < et->first->get_sync_expr_list_size();s++)
            {
                assign( channel_item_at( chan, "i-1", s, out ), channel_item_at( chan, "i", s, in ) );
                assign( channel_item_at( chan, "i", s, out ), "0" );
            }
            block_end();
        }
    }

    //first transition effect
    assign( process_state( et->first->get_process_gid(), out ),
            fmt( et->first->get_state2_lid() ) );

    for(size_int_t e = 0;e < et->first->get_effect_count();e++)
        print_cexpr( *et->first->get_effect(e), out );

    if(et->synchronized) //second transiton effect
    {
        assign( process_state( et->second->get_process_gid(), out ),
                fmt( et->second->get_state2_lid() ) );
        for(size_int_t e = 0;e < et->second->get_effect_count();e++)
            print_cexpr( *et->second->get_effect(e), out );
    }

    if(have_property) //change of the property process state
        assign( process_state( et->property->get_process_gid(), out ),
                fmt( et->property->get_state2_lid() ) );
}

void dve_compiler::new_output_state() {
    line( "divine::Blob blob_out( *(setup->pool), setup->slack + state_size );" );
    line( "state_struct_t *out = &blob_out.get< state_struct_t >( setup->slack );" );
    line( "blob_out.clear( 0, setup->slack );" );
    line( "*out = *in;" );
}

void dve_compiler::yield_state() {
    if ( many ) {
        line( "if (buf_out->space() < 2) {" );
        line( "    buf_out->unadd( states_emitted );" );
        line( "    return;" );
        line( "}");
        line( "buf_out->add( (*buf_in)[ 0 ] );" );
        line( "buf_out->add( blob_out );" );
        line( "++states_emitted;" );
    } else {
        line( "*to = blob_out;" );
        line( "return " + fmt( current_label ) + ";" );
    }
}

void dve_compiler::gen_successors()
{
    string in = "(*in)", out = "(*out)", space = "";

    new_label();
    if_begin( true );

    for(size_int_t i = 0; i < get_process_count(); i++)
        for(size_int_t j = 0; j < dynamic_cast<dve_process_t*>(get_process(i))->get_state_count(); j++)
            if(dynamic_cast<dve_process_t*>(get_process(i))->get_commited(j))
                if_clause( in_state( i, j, in ) );

    if_end(); block_begin(); // committed states

    for(size_int_t i = 0; i < this->get_process_count(); i++)
    {
        if( transition_map.find(i) != transition_map.end() && !is_property( i ) )
            for(iter_process_transition_map = transition_map.find(i)->second.begin();
                iter_process_transition_map != transition_map.find(i)->second.end();
                iter_process_transition_map++)
            {
                if(dynamic_cast<dve_process_t*>(get_process(i))->get_commited(
                       iter_process_transition_map->first))
                {
                    new_label();

                    if_begin( true );
                    if_clause( in_state( i, iter_process_transition_map->first, in ) );
                    if_end(); block_begin();


                    for(iter_ext_transition_vector = iter_process_transition_map->second.begin();
                        iter_ext_transition_vector != iter_process_transition_map->second.end();
                        iter_ext_transition_vector++)
                    {
                        // !! jak je to s property synchronizaci v comitted stavech !!
                        if( !iter_ext_transition_vector->synchronized ||
                            dynamic_cast<dve_process_t*>(
                                get_process(iter_ext_transition_vector->second->get_process_gid()))->
                            get_commited(iter_ext_transition_vector->second->get_state1_lid()) )
                        {
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

    new_label(); // Trick. : - )
    line( ";" );

    block_end();
    line( "else" );
    block_begin();

    for(size_int_t i = 0; i < get_process_count(); i++)
    {
        if(transition_map.find(i) != transition_map.end() && !is_property( i ))
            for(iter_process_transition_map = transition_map.find(i)->second.begin();
                iter_process_transition_map != transition_map.find(i)->second.end();
                iter_process_transition_map++)
            {
                new_label();
                if_begin( true );
                if_clause( in_state( i, iter_process_transition_map->first, in ) );

                if_end(); block_begin();

                for(iter_ext_transition_vector = iter_process_transition_map->second.begin();
                    iter_ext_transition_vector != iter_process_transition_map->second.end();
                    iter_ext_transition_vector++)
                {
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
    block_end();

    new_label();

    if_begin( true );
    if_clause( "system_in_deadlock" );
    if_end(); block_begin();

    for(iter_property_transitions = property_transitions.begin();
        iter_property_transitions != property_transitions.end();
        iter_property_transitions++)
    {
        new_label();
        if_begin( false );

        if_clause( in_state( (*iter_property_transitions)->get_process_gid(),
                             (*iter_property_transitions)->get_state1_lid(), in ) );
        if_cexpr_clause( (*iter_property_transitions)->get_guard(), in );

        if_end(); block_begin();
        new_output_state();

        assign( process_state( (*iter_property_transitions)->get_process_gid(), out ),
                fmt( (*iter_property_transitions)->get_state2_lid() ) );

        yield_state();
        block_end();
    }
    block_end();
}

void dve_compiler::gen_is_accepting()
{
    // not compulsory, so don't bother if not needed
    if(!have_property)
        return;

    line( "extern \"C\" bool is_accepting( CustomSetup *setup, Blob b, int size )" );
    block_begin();

    line( "state_struct_t &state = b.get< state_struct_t >( setup->slack );" );
    for(size_int_t i = 0; i < dynamic_cast<dve_process_t*>(get_process((get_property_gid())))->get_state_count(); i++)
    {
        if (dynamic_cast<dve_process_t*>(get_process((get_property_gid())))->get_acceptance(i, 0, 1) )
        {
            if_begin( true );
            if_clause( in_state( get_property_gid(), i, "state" ) );
            if_end();
            line( "    return true;" );
        }
    }
    line( "return false;" );
    block_end();

    line();
}

void dve_compiler::print_generator()
{
    gen_header();
    gen_constants();
    gen_state_struct();
    gen_initial_state();

    line( "extern \"C\" void setup( CustomSetup *setup ) {" );
    line( "    setup->state_size = state_size;" );
    line( "    setup->has_property = " + fmt( get_with_property() ) + ";" );
    line( "}" );

    many = false;
    current_label = 1;

    gen_is_accepting();

    line( "extern \"C\" int get_successor( CustomSetup *setup, int next_state, Blob from, Blob *to ) " );
    block_begin();
    line( "const state_struct_t *in = &from.get< state_struct_t >( setup->slack );" );
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

    // many = true;
    // current_label = 0;
#if 0
    line( "extern \"C\" void get_many_successors( int slack, char *_pool, char *," );
    line( "                                       char *_buf_in, char *_buf_outf, char *_buf_outs ) " );
    block_begin();
    line( "divine::Pool *pool = (divine::Pool *) _pool;" );
    line( "typedef divine::Circular< divine::Blob, 0 > Buffer;" );
    line( "Buffer *buf_in = (Buffer *) _buf_in;" );
    line( "Buffer *buf_out = (Buffer *) _buf_out;" );
    line( "int states_emitted;" );
    line( "bool system_in_deadlock;" );
    line( "state_struct_t *in;" );

    line( "next:" );
    line( "system_in_deadlock = true;" );
    line( "states_emitted = 0;" );
    line( "in = (state_struct_t*) ((*buf_in)[ 0 ].data() + slack);" );
    gen_successors();
    line( "buf_in->drop( 1 );" );
    line( "if ( buf_in->empty() ) return;" );
    line( "goto next;" );
    block_end();
#endif
}
