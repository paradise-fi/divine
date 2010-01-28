#include <tools/dvecompile.h>
#include <divine/generator/common.h>

using namespace wibble::str;

void dve_compiler::write_C(dve_expression_t & expr, std::ostream & ostr, std::string state_name)
{
    //DEBFUNC(cerr << "BEGIN of dve_expression_t::write" << endl;)
    dve_symbol_table_t * parent_table = expr.get_symbol_table();
    if (!parent_table) gerr << "Writing expression: Symbol table not set" << thr();
    switch (expr.get_operator())
    {
        case T_ID:
        { ostr<<state_name<<".";
            if(parent_table->get_variable(expr.get_ident_gid())->get_process_gid() != NO_ID)
            {
                ostr << parent_table->get_process(parent_table->get_variable(expr.get_ident_gid())->get_process_gid())->get_name(); //name of process
                ostr<<".";
            }
            ostr << parent_table->get_variable(expr.get_ident_gid())->get_name();
            break; }
        case T_FOREIGN_ID:
        { ostr << parent_table->get_process(parent_table->get_variable(expr.get_ident_gid())->get_process_gid())->get_name(); //name of process
            ostr<<"->";
            ostr << parent_table->get_variable(expr.get_ident_gid())->get_name();
            break; }
        case T_NAT:
            ostr << expr.get_value(); break;
        case T_PARENTHESIS:
        { ostr << "("; write_C(*expr.left(), ostr, state_name); ostr << ")"; break; }
        case T_SQUARE_BRACKETS:
        { ostr<<state_name<<".";
            if(parent_table->get_variable(expr.get_ident_gid())->get_process_gid() != NO_ID)
            {
                ostr << parent_table->get_process(parent_table->get_variable(expr.get_ident_gid())->get_process_gid())->get_name(); //name of process
                ostr<<".";
            }
            ostr << parent_table->get_variable(expr.get_ident_gid())->
                get_name(); ostr<<"["; write_C(*expr.left(), ostr, state_name); ostr<<"]" ;break; }
        case T_FOREIGN_SQUARE_BRACKETS:
        { ostr << parent_table->get_process(parent_table->get_variable(expr.get_ident_gid())->get_process_gid())->get_name(); //name of preocess
            ostr<<"->";
            ostr << parent_table->get_variable(expr.get_ident_gid())->get_name();
            ostr<<"["; write_C(*expr.left(), ostr, state_name); ostr<<"]" ;break; }
        case T_LT: { write_C(*expr.left(), ostr, state_name); ostr<<"<"; write_C(*expr.right(), ostr, state_name); break; }
        case T_LEQ: { write_C(*expr.left(), ostr, state_name); ostr<<"<="; write_C(*expr.right(), ostr, state_name); break; }
        case T_EQ: { write_C(*expr.left(), ostr, state_name); ostr<<"=="; write_C(*expr.right(), ostr, state_name); break; }
        case T_NEQ: { write_C(*expr.left(), ostr, state_name); ostr<<"!="; write_C(*expr.right(), ostr, state_name); break; }
        case T_GT: { write_C(*expr.left(), ostr, state_name); ostr<<">"; write_C(*expr.right(), ostr, state_name); break; }
        case T_GEQ: { write_C(*expr.left(), ostr, state_name); ostr<<">="; write_C(*expr.right(), ostr, state_name); break; }
        case T_PLUS: { write_C(*expr.left(), ostr, state_name); ostr<<"+"; write_C(*expr.right(), ostr, state_name); break; }
        case T_MINUS: { write_C(*expr.left(), ostr, state_name); ostr<<"-"; write_C(*expr.right(), ostr, state_name); break; }
        case T_MULT: { write_C(*expr.left(), ostr, state_name); ostr<<"*"; write_C(*expr.right(), ostr, state_name); break; }
        case T_DIV: { write_C(*expr.left(), ostr, state_name); ostr<<"/"; write_C(*expr.right(), ostr, state_name); break; }
        case T_MOD: { write_C(*expr.left(), ostr, state_name); ostr<<"%"; write_C(*expr.right(), ostr, state_name); break; }
        case T_AND: { write_C(*expr.left(), ostr, state_name); ostr<<"&"; write_C(*expr.right(), ostr, state_name); break; }
        case T_OR: { write_C(*expr.left(), ostr, state_name); ostr<<"|"; write_C(*expr.right(), ostr, state_name); break; }
        case T_XOR: { write_C(*expr.left(), ostr, state_name); ostr<<"^"; write_C(*expr.right(), ostr, state_name); break; }
        case T_LSHIFT: { write_C(*expr.left(), ostr, state_name); ostr<<"<<"; write_C(*expr.right(), ostr, state_name); break; }
        case T_RSHIFT: { write_C(*expr.left(), ostr, state_name); ostr<<">>"; write_C(*expr.right(), ostr, state_name); break; }
        case T_BOOL_AND: {write_C(*expr.left(), ostr, state_name); ostr<<" && ";write_C(*expr.right(), ostr, state_name); break;}
        case T_BOOL_OR: {write_C(*expr.left(), ostr, state_name); ostr<<" || "; write_C(*expr.right(), ostr, state_name); break;}
        case T_DOT:
        { ostr<<state_name<<".";
            ostr<<parent_table->get_process(parent_table->get_state(expr.get_ident_gid())->get_process_gid())->get_name(); ostr<<".state"<<" == ";
            ostr<<parent_table->get_state(expr.get_ident_gid())->get_lid(); break; }
        case T_IMPLY: { write_C(*expr.left(), ostr, state_name); ostr<<" -> "; write_C(*expr.right(), ostr, state_name); break; }
        case T_UNARY_MINUS: { ostr<<"-"; write_C(*expr.right(), ostr, state_name); break; }
        case T_TILDE: { ostr<<"~"; write_C(*expr.right(), ostr, state_name); break; }
        case T_BOOL_NOT: { ostr<<" ! ("; write_C(*expr.right(), ostr, state_name); ostr<< " )"; break; }
        case T_ASSIGNMENT: { write_C(*expr.left(), ostr, state_name); ostr<<" = "; write_C(*expr.right(), ostr, state_name); break; }
        default: { gerr << "Problem in expression - unknown operator"
                " number " << expr.get_operator() << psh(); }
    }
    //DEBFUNC(cerr << "END of dve_expression_t::write_C" << endl;)
}

std::string dve_compiler::cexpr( dve_expression_t & expr, std::string state )
{
    std::stringstream str;
    str << "(";
    write_C( expr, str, state.c_str() );
    str << ")";
    return str.str();
}

namespace divine {
extern const char *pool_h_str;
extern const char *circular_h_str;
}

void dve_compiler::print_header()
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

    line( divine::pool_h_str );
    line();

    line( divine::circular_h_str );
    line();

    line( "static inline char *pool_alloc( char *p, int size ) {" );
    line( "     return reinterpret_cast< divine::Pool * >( p )->allocate( size ); }" );
    line();
}

void dve_compiler::print_state_struct()
{
    for (size_int_t i=0; i!=glob_var_count; i++)
    {
        dve_symbol_t * var = get_symbol_table()->get_variable(get_global_variable_gid(i));
        if (var->is_const())
        {
            append( "const " );
            if ( var->is_byte() )
                append( "byte_t " );
            else
                append( "sshort_int_t " );

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
    }
    line();

    bool global = true;
    string name;
    string process_name = "UNINITIALIZED";
    line( "struct state_struct_t" );
    block_begin();
    for (size_int_t i=0; i!=state_creators_count; ++i)
    {
        switch (state_creators[i].type)
        {
            case state_creator_t::VARIABLE:
            {
                name=get_symbol_table()->get_variable(state_creators[i].gid)->get_name();
                if (state_creators[i].array_size)
                {
                    if (state_creators[i].var_type==VAR_BYTE)
                        append( "byte_t " );
                    else if (state_creators[i].var_type==VAR_INT)
                        append( "sshort_int_t " );
                    else gerr << "Unexpected error" << thr();
                    line( name + "[" + fmt( state_creators[i].array_size ) + "];" );
                }
                else
                {
                    if (state_creators[i].var_type==VAR_BYTE)
                        line( "byte_t " + name + ";" );
                    else if (state_creators[i].var_type==VAR_INT)
                        line( "sshort_int_t" + name + ";" );
                    else gerr << "Unexpected error" << thr();
                }
            }
            break;
            case state_creator_t::PROCESS_STATE:
            {
                if (global)
                {
                    global = false;
                }
                else
                {
                    block_end();
                    line( "__attribute__((__packed__)) " + process_name + ";" );
                }
                line( "struct" );
                block_begin();

                process_name=
                    get_symbol_table()->get_process(state_creators[i].gid)->get_name();
                line( "ushort_int_t state;" );
            }
            break;
            case state_creator_t::CHANNEL_BUFFER:
            {
                name=get_symbol_table()->get_channel(state_creators[i].gid)->get_name();
                line( "struct" );
                block_begin();
                line( "ushort_int_t number_of_items;" );
                line( "struct" );
                block_begin();
                dve_symbol_t * symbol =
                    get_symbol_table()->get_channel(state_creators[i].gid);
                size_int_t item_count = symbol->get_channel_type_list_size();

                for (size_int_t j=0; j<item_count; ++j)
                    if (symbol->get_channel_type_list_item(j)==VAR_BYTE)
                        line( "byte_t x" + fmt( j ) + ";" );
                    else if (symbol->get_channel_type_list_item(j)==VAR_INT)
                        line( "sshort_int_t x" + fmt( j ) + ";" );
                    else gerr << "Unexpected error" << thr();
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
    if (!global)
    {
        block_end();
        line( "__attribute__((__packed__)) " + process_name + ";" );
    }
    block_end();
    line( "__attribute__((__packed__));" );

    line( "int state_size = sizeof(state_struct_t);" );
    line();
}


void dve_compiler::print_initial_state()
{
    setAllocator( new generator::Allocator );
    state_t initial_state =  dve_explicit_system_t::get_initial_state();
    append( "char initial_state[] = {" );
    for(int i = 0; i < initial_state.size; i++)
    {
        append( fmt( (unsigned int)(unsigned char)initial_state.ptr[i] ) );
        if(i != initial_state.size - 1)
            append( ", " );
    }
    line( "};" );
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

void dve_compiler::print_generator()
{
    string in = "(*in)", out = "(*out)", space = "";
    int current_label = 1;
    bool some_commited_state = false;

    line( "#define p_new_c_state out" );
    line( "#define p_state_struct in" );
    line();

    line( "extern \"C\" int get_successor(int next_state, char* from, char* to) " );
    block_begin();
    line( "bool system_in_deadlock = false;" );
    line( "state_struct_t *in = (state_struct_t*)from;" );
    line( "state_struct_t *out = (state_struct_t*)to;" );
    line( "*out = *in;" );
    line( "goto switch_state;" );

    label( current_label );
    if_begin( true );

    current_label++;

    for(size_int_t i = 0; i < get_process_count(); i++)
        for(size_int_t j = 0; j < dynamic_cast<dve_process_t*>(get_process(i))->get_state_count(); j++)
            if(dynamic_cast<dve_process_t*>(get_process(i))->get_commited(j))
                if_clause( in_state( i, j, in ) );

    if_end(); block_begin();

    for(size_int_t i = 0; i < this->get_process_count(); i++)
    {
        if(!have_property || i != this->get_property_gid())
        {
            if(transition_map.find(i) != transition_map.end())
                for(iter_process_transition_map = transition_map.find(i)->second.begin();
                    iter_process_transition_map != transition_map.find(i)->second.end();iter_process_transition_map++)
                {
                    if(dynamic_cast<dve_process_t*>(get_process(i))->get_commited(iter_process_transition_map->first))
                    {
                        label( current_label );
                        current_label++;

                        if_begin( true );
                        if_clause( in_state( i, iter_process_transition_map->first, in ) );

                        if_end(); block_begin();

                        for(iter_ext_transition_vector = iter_process_transition_map->second.begin();
                            iter_ext_transition_vector != iter_process_transition_map->second.end();iter_ext_transition_vector++)
                        {
                            if( !iter_ext_transition_vector->synchronized ||
                                dynamic_cast<dve_process_t*>(get_process(iter_ext_transition_vector->second->get_process_gid()))->
                                get_commited(iter_ext_transition_vector->second->get_state1_lid()) ) // !! jak je to s property synchronizaci v comitted stavech !!
                            {
                                if_begin( false );
                                if_cexpr_clause( iter_ext_transition_vector->first->get_guard(), in );

                                if(iter_ext_transition_vector->synchronized)
                                {
                                    if_clause( in_state( iter_ext_transition_vector->second->get_process_gid(),
                                                         iter_ext_transition_vector->second->get_state1_lid(), in ) );
                                    if_cexpr_clause( iter_ext_transition_vector->second->get_guard(), in );
                                }
                                else
                                {
                                    if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_EXCLAIM_BUFFER)
                                    {
                                        std::stringstream str;
                                        str << in << "." <<get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name()
                                            << "." <<"number_of_items != "
                                            << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_channel_buffer_size();
                                        if_clause( str.str() );
                                    }
                                    if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_ASK_BUFFER)
                                    {
                                        std::stringstream str;
                                        str << in << "." <<get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name()
                                             << "." <<"number_of_items != 0";
                                        if_clause( str.str() );
                                    }
                                }
                                if(have_property)
                                {
                                    if_clause( in_state( iter_ext_transition_vector->property->get_process_gid(),
                                                         iter_ext_transition_vector->property->get_state1_lid(), in ) );
                                    if_cexpr_clause( iter_ext_transition_vector->property->get_guard(), in );
                                }

                                if_end(); block_begin();

                                //synchronization effect
                                if(iter_ext_transition_vector->synchronized)
                                {
                                    for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                                    {
                                        assign( cexpr( *iter_ext_transition_vector->first->get_sync_expr_list_item(s), out ),
                                                cexpr( *iter_ext_transition_vector->second->get_sync_expr_list_item(s), in ) );
                                    }
                                }
                                else
                                {
                                    int chan = iter_ext_transition_vector->first->get_channel_gid();
                                    if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_EXCLAIM_BUFFER)
                                    {
                                        for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                                            assign( channel_item_at( chan, channel_items( chan, in ) + " - 1", s, out ),
                                                    cexpr( *iter_ext_transition_vector->first->get_sync_expr_list_item(s), in ) );

                                        line( channel_items( chan, out ) + "++;" );
                                    }

                                    if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_ASK_BUFFER)
                                    {
                                        for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                                        {
                                            assign( cexpr( *iter_ext_transition_vector->first->get_sync_expr_list_item(s), out ),
                                                    channel_item_at( chan, "0", s, in ) );
                                        }
                                        line( channel_items( chan, out ) + "--;" );

                                        line( "for(size_int_t i = 1 ; i <= " + channel_items( chan, out ) + "; i++)" );
                                        block_begin();

                                        for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                                        {
                                            assign( channel_item_at( chan, "i-1", s, out ), channel_item_at( chan, "i", s, in ) );
                                            assign( channel_item_at( chan, "i", s, out ), "0" );
                                        }
                                        block_end();
                                    }
                                }
                                //first transition effect
                                assign( process_state( iter_ext_transition_vector->first->get_process_gid(), out ),
                                        fmt( iter_ext_transition_vector->first->get_state2_lid() ) );

                                for(size_int_t e = 0;e < iter_ext_transition_vector->first->get_effect_count();e++)
                                    print_cexpr( *iter_ext_transition_vector->first->get_effect(e), out );

                                if(iter_ext_transition_vector->synchronized) //second transiton effect
                                {
                                    assign( process_state( iter_ext_transition_vector->second->get_process_gid(), out ),
                                            fmt( iter_ext_transition_vector->second->get_state2_lid() ) );
                                    for(size_int_t e = 0;e < iter_ext_transition_vector->second->get_effect_count();e++)
                                        print_cexpr( *iter_ext_transition_vector->second->get_effect(e), out );
                                }
                                if(have_property) //change of the property process state
                                    assign( process_state( iter_ext_transition_vector->property->get_process_gid(), out ),
                                            fmt( iter_ext_transition_vector->property->get_state2_lid() ) );
                                block_end();
                            }
                        }
                        line( "return" + fmt( current_label ) + ";" );
                        block_end();
                    }
                }
        }
    }

    block_end();
    line( "else" );
    block_begin();

    for(size_int_t i = 0; i < get_process_count(); i++)
    {
        if(!have_property || i != get_property_gid())
        {
            if(transition_map.find(i) != transition_map.end())
                for(iter_process_transition_map = transition_map.find(i)->second.begin();
                    iter_process_transition_map != transition_map.find(i)->second.end();iter_process_transition_map++)
                {
                    label( current_label );
                    if_begin( true );
                    if_clause( in_state( i, iter_process_transition_map->first, in ) );

                    if_end(); block_begin();

                    current_label++;

                    for(iter_ext_transition_vector = iter_process_transition_map->second.begin();
                        iter_ext_transition_vector != iter_process_transition_map->second.end();iter_ext_transition_vector++)
                    {
                        label( current_label );
                        current_label++;
                        if_begin( false );
                        if_cexpr_clause( iter_ext_transition_vector->first->get_guard(), in );

                        if(iter_ext_transition_vector->synchronized)
                        {
                            if_clause( in_state( iter_ext_transition_vector->second->get_process_gid(),
                                                 iter_ext_transition_vector->second->get_state1_lid(), in ) );
                            if_cexpr_clause( iter_ext_transition_vector->second->get_guard(), in );
                        }
                        else
                        {
                            int chan = iter_ext_transition_vector->first->get_channel_gid();
                            if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_EXCLAIM_BUFFER)
                                if_clause( relate( channel_items( chan, in ), "!=", fmt( channel_capacity( chan ) ) ) );

                            if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_ASK_BUFFER)
                                if_clause( relate( channel_items( chan, in ), "!=", "0" ) );
                        }
                        if(have_property)
                        {
                            if_clause( in_state( iter_ext_transition_vector->property->get_process_gid(),
                                                 iter_ext_transition_vector->property->get_state1_lid(), in ) );
                            if_cexpr_clause( iter_ext_transition_vector->property->get_guard(), in );
                        }

                        if_end(); block_begin();

                        line( "system_in_deadlock = false;" );

                        //synchronization effect
                        if(iter_ext_transition_vector->synchronized)
                        {
                            for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                                assign( cexpr( *iter_ext_transition_vector->first->get_sync_expr_list_item(s), out ),
                                        cexpr( *iter_ext_transition_vector->second->get_sync_expr_list_item(s), in ) );
                        }
                        else
                        {
                            int chan = iter_ext_transition_vector->first->get_channel_gid();
                            if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_EXCLAIM_BUFFER)
                            {
                                for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                                {
                                    assign( channel_item_at( chan, channel_items( chan, in ), s, out ),
                                            cexpr( *iter_ext_transition_vector->first->get_sync_expr_list_item( s ), in ) );
                                }
                                line( channel_items( chan, out ) + "++;" );
                            }
                            if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_ASK_BUFFER)
                            {
                                for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                                    assign( cexpr( *iter_ext_transition_vector->first->get_sync_expr_list_item(s), out ),
                                            channel_item_at( chan, "0", s, in ) );
                                line( channel_items( chan, out ) + "--;" );

                                line( "for(size_int_t i = 1 ; i <= " + channel_items( chan, out ) + "; i++)" );
                                block_begin();
                                for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                                {
                                    assign( channel_item_at( chan, "i-1", s, out ), channel_item_at( chan, "i", s, in ) );
                                    assign( channel_item_at( chan, "i", s, out ), "0" );
                                }
                                block_end();
                            }
                        }

                        //first transition effect
                        assign( process_state( iter_ext_transition_vector->first->get_process_gid(), out ),
                                fmt( iter_ext_transition_vector->first->get_state2_lid() ) );

                        for(size_int_t e = 0;e < iter_ext_transition_vector->first->get_effect_count();e++)
                            print_cexpr( *iter_ext_transition_vector->first->get_effect(e), out );

                        if(iter_ext_transition_vector->synchronized) //second transiton effect
                        {
                            assign( process_state( iter_ext_transition_vector->second->get_process_gid(), out ),
                                    fmt( iter_ext_transition_vector->second->get_state2_lid() ) );
                            for(size_int_t e = 0;e < iter_ext_transition_vector->second->get_effect_count();e++)
                                print_cexpr( *iter_ext_transition_vector->second->get_effect(e), out );
                        }

                        if(have_property) //change of the property process state
                            assign( process_state( iter_ext_transition_vector->property->get_process_gid(), out ),
                                    fmt( iter_ext_transition_vector->property->get_state2_lid() ) );

                        line( "return " + fmt( current_label ) + ";" );
                        block_end();
                    }
                    block_end();
                }
        }
    }
    block_end();

    label( current_label ); current_label ++;

    if_begin( true );
    if_clause( "system_in_deadlock" );
    if_end(); block_begin();

    for(iter_property_transitions = property_transitions.begin();iter_property_transitions != property_transitions.end();
        iter_property_transitions++)
    {
        label( current_label ); current_label ++;
        if_begin( false );

        if_clause( in_state( (*iter_property_transitions)->get_process_gid(),
                             (*iter_property_transitions)->get_state1_lid(), in ) );
        if_cexpr_clause( (*iter_property_transitions)->get_guard(), in );

        if_end(); block_begin();

        assign( process_state( (*iter_property_transitions)->get_process_gid(), in ),
                fmt( (*iter_property_transitions)->get_state2_lid() ) );

        line( "return " + fmt( current_label ) + ";" );
        block_end();
    }
    block_end();

    label( current_label );
    line( "return 0;" );

    line( "switch_state: switch( next_state )" );
    block_begin();

    for(int i=1; i<=current_label; i++)
        if (i==1)
            line( "case " + fmt( i ) + ": system_in_deadlock = true; goto l" + fmt( i ) + ";" );
        else
            line( "case " + fmt( i ) + ": goto l" + fmt( i ) + ";" );
    block_end();
    block_end();

    line();

    if(have_property)
    {
        line( "extern \"C\" bool is_accepting( char *_state, int size )" );
        block_begin();

        line( "state_struct_t &state = * (state_struct_t*) _state;" );
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
    }

    line();

    line( "extern \"C\" int get_state_size() {" );
    line( "    return state_size;" );
    line( "}" );

    line();

    line( "extern \"C\" void get_initial_state( char *to ) {" );
    line( "    memcpy(to, initial_state, state_size);" );
    line( "}" );
}
