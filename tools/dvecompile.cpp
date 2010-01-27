#include <tools/dvecompile.h>

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

namespace divine {
extern const char *pool_h_str;
extern const char *circular_h_str;
}

void dve_compiler::print_header(ostream & ostr)
{
  ostr << "#include <stdio.h>" <<endl;
  ostr << "#include <stdint.h>" <<endl;
  ostr << "#include <string.h>" <<endl;
  //ostr << "#include "<<'"' <<"hash_kernel.cu"<<'"'<<endl;
  ostr << endl;
  //ostr << "#define EXPLICIT_STORAGE_HASH_SEED 0xBA9A9ABA"<<endl;
  ostr << endl;
  ostr << "#define c_state (*p_state_struct)" <<endl; 
  ostr << "typedef uint64_t ulong_long_int_t;" <<endl; 
  ostr << "typedef int64_t slong_long_int_t;" <<endl; 
  ostr << "typedef uint32_t ulong_int_t;" <<endl; 
  ostr << "typedef int32_t slong_int_t;" <<endl; 
  ostr << "typedef uint16_t ushort_int_t;" <<endl; 
  ostr << "typedef int16_t sshort_int_t;" <<endl; 
  ostr << "typedef uint8_t byte_t;" <<endl; 
  ostr << "typedef uint8_t ubyte_t;" <<endl; 
  ostr << "typedef int8_t sbyte_t;" <<endl; 
  ostr << "typedef size_t size_int_t;" <<endl;
  ostr << endl;
  ostr << divine::pool_h_str;
  ostr << endl;
  ostr << divine::circular_h_str;
  ostr << endl;
  ostr << "static inline char *pool_alloc( char *p, int size ) { \n\
                 return reinterpret_cast< divine::Pool * >( p )->allocate( size ); }";
  ostr << endl;
}

void dve_compiler::print_state_struct(ostream & ostr)
{
 for (size_int_t i=0; i!=glob_var_count; i++)
 {
  dve_symbol_t * var = get_symbol_table()->get_variable(get_global_variable_gid(i));
  if (var->is_const())
  {
    ostr << "const ";
    if (var->is_byte()) ostr << "byte_t ";
       else ostr << "sshort_int_t ";
    ostr << var->get_name();
   if (var->is_vector())
    {
     ostr << "[" << var->get_vector_size() << "]";
     if (var->get_init_expr_count()) ostr << " = {";
     for (size_int_t j=0; j!=var->get_init_expr_count(); j++)
      {
       ostr << var->get_init_expr(j)->to_string();
       if (j!=(var->get_init_expr_count()-1)) ostr << ", ";
       else ostr << "}";
      }
    }
    else if (var->get_init_expr())
    { ostr << " = " << var->get_init_expr()->to_string(); }
   ostr << ";" << endl;
  }
 }
 ostr << endl;

 bool global = true;
 string spaces = "  ";
 string orig_spaces = spaces;
 string name;
 string process_name = "UNINITIALIZED";
 ostr << "struct state_struct_t" << endl;
 ostr << " {" << endl;
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
           ostr << spaces << "byte_t ";
         else if (state_creators[i].var_type==VAR_INT)
           ostr << spaces << "sshort_int_t ";
         else gerr << "Unexpected error" << thr();
         ostr << name << "[" <<state_creators[i].array_size<< "];" << endl;
        }
       else
        {
         if (state_creators[i].var_type==VAR_BYTE)
           ostr << spaces << "byte_t " << name << ";" << endl;
         else if (state_creators[i].var_type==VAR_INT)
           ostr << spaces << "sshort_int_t " << name << ";" << endl;
         else gerr << "Unexpected error" << thr();
        }
      }
     break;
     case state_creator_t::PROCESS_STATE:
      {
       if (global)
        {
         spaces += "  ";
         global = false;
        }
       else
        {
         ostr << orig_spaces << " } __attribute__((__packed__)) " << process_name << ";" << endl;
        }
       ostr << orig_spaces << "struct" << endl;
       ostr << orig_spaces << " {" << endl;
       process_name=
         get_symbol_table()->get_process(state_creators[i].gid)->get_name();
       ostr << spaces << "ushort_int_t state;" << endl;
      }
     break;
     case state_creator_t::CHANNEL_BUFFER:
      {
       name=get_symbol_table()->get_channel(state_creators[i].gid)->get_name();
       ostr << spaces << "struct" << endl;
       ostr << spaces << " {" << endl;
       ostr << spaces << "  ushort_int_t number_of_items;" << endl;
       ostr << spaces << "  struct" << endl;
       ostr << spaces << "   {" << endl;
       string extra_spaces = spaces + "    ";
       dve_symbol_t * symbol =
         get_symbol_table()->get_channel(state_creators[i].gid);
       size_int_t item_count = symbol->get_channel_type_list_size();
       for (size_int_t j=0; j<item_count; ++j)
         if (symbol->get_channel_type_list_item(j)==VAR_BYTE)
           ostr << extra_spaces << "byte_t x" << j << ";" << endl;
         else if (symbol->get_channel_type_list_item(j)==VAR_INT)
           ostr << extra_spaces << "sshort_int_t x" << j << ";" << endl;
         else gerr << "Unexpected error" << thr();
       ostr << spaces << "   } __attribute__((__packed__)) content["<< symbol->get_channel_buffer_size() << "];"<< endl;
       ostr << spaces << " } __attribute__((__packed__)) " << name << ";" <<endl;
      }
     break;
     default: gerr << "Unexpected error" << thr();
     break;
    };
  }
 if (!global)
  {
   ostr << orig_spaces << " } __attribute__((__packed__)) " << process_name << ";" << endl;
  }
 ostr << " } __attribute__((__packed__));" << endl;
 ostr << endl;
 ostr << "int state_size = sizeof(state_struct_t);" << endl;
 ostr << endl ;
}


struct MyStateAllocator : StateAllocator {
    virtual state_t duplicate_state( const state_t &state ) {};
    virtual state_t new_state( std::size_t size ) { state_t x; x.ptr = new char[size]; x.size = size; memset(x.ptr,0,size); return x; }
    virtual void delete_state( state_t &st ) {};
    virtual ~MyStateAllocator() {}
};

void dve_compiler::print_initial_state(ostream & ostr)
{
  setAllocator(new MyStateAllocator);
  state_t initial_state =  dve_explicit_system_t::get_initial_state();
  ostr << "char initial_state[] = {";
  for(int i = 0; i < initial_state.size; i++)
  {
   ostr << (unsigned int)(unsigned char)initial_state.ptr[i];
   if(i != initial_state.size - 1)
    ostr << ", ";
  }
  ostr<<"};"<<endl;
  ostr<<endl;
}

void dve_compiler::print_generator(ostream & ostr)
{
  map<size_int_t,map<size_int_t,vector<ext_transition_t> > > transition_map;
  map<size_int_t,vector<dve_transition_t*> > channel_map;

  vector<dve_transition_t*> property_transitions;
  vector<dve_transition_t*>::iterator iter_property_transitions;

  map<size_int_t,vector<dve_transition_t*> >::iterator iter_channel_map;
  vector<dve_transition_t*>::iterator iter_transition_vector;
  map<size_int_t,vector<ext_transition_t> >::iterator iter_process_transition_map;
  map<size_int_t,map<size_int_t,vector<ext_transition_t> > >::iterator iter_transition_map;
  vector<ext_transition_t>::iterator iter_ext_transition_vector;

  string state_name = "c_state";

  dve_transition_t * transition;
  bool sytem_with_property = this->get_with_property();

  // obtain transition with synchronization of the type SYNC_EXCLAIM and property transitions
  for(size_int_t i = 0; i < this->get_trans_count(); i++)
  {
    transition = dynamic_cast<dve_transition_t*>(this->get_transition(i));
    if(transition->is_sync_exclaim())
    {
      iter_channel_map = channel_map.find(transition->get_channel_gid());
      if(iter_channel_map == channel_map.end()) //new channel
      {
         vector<dve_transition_t*> transition_vector;
         transition_vector.push_back(transition);
         channel_map.insert(pair<size_int_t,vector<dve_transition_t*> >(transition->get_channel_gid(),transition_vector));
      }
      else{
       iter_channel_map->second.push_back(transition);
      }
    }

    if(sytem_with_property && transition->get_process_gid() == this->get_property_gid())
    {
      property_transitions.push_back(transition);
    }
  }

  // obtain map of transitions
  for(size_int_t i = 0; i < this->get_trans_count(); i++)
  {
    transition = dynamic_cast<dve_transition_t*>(this->get_transition(i));
    if(!transition->is_sync_exclaim() && (!sytem_with_property || transition->get_process_gid() != this->get_property_gid()) )
    {
       // not syncronized sender without buffer and not a property transition
      iter_transition_map = transition_map.find(transition->get_process_gid());
      if( iter_transition_map == transition_map.end()) //new process it means that new state in process is also new
      {
         // new process, add to transition map
         map<size_int_t,vector<ext_transition_t> >  process_transition_map;
         vector<ext_transition_t> ext_transition_vector;
         if(!transition->is_sync_ask())
         {
           // transition not of type SYNC_ASK
           if(!sytem_with_property)
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
             for(iter_property_transitions = property_transitions.begin();iter_property_transitions != property_transitions.end();
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
             // channel of this transition is found (strange test, no else part for if statement)
             // assume: channel should always be present
             // forall transitions that also use this channel, add to ext_transitions
             for(iter_transition_vector = iter_channel_map->second.begin();iter_transition_vector !=
                 iter_channel_map->second.end();iter_transition_vector++)
             {
               if (transition->get_process_gid() != (*iter_transition_vector)->get_process_gid() ) //not synchronize with yourself
               {
                 if(!sytem_with_property)
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
                   for(iter_property_transitions = property_transitions.begin();iter_property_transitions != property_transitions.end();
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
         // for this process state, add the ext transitions
         process_transition_map.insert(pair<size_int_t,vector<ext_transition_t> >(transition->get_state1_lid(),ext_transition_vector));
         // then add this vector to the transition map for this process
         transition_map.insert(pair<size_int_t,map<size_int_t,vector<ext_transition_t> > >(transition->get_process_gid(),process_transition_map));
      }
      else{
        // existing process, find process_transition_map
         iter_process_transition_map = iter_transition_map->second.find(transition->get_state1_lid());
         if( iter_process_transition_map == iter_transition_map->second.end()) //new state in current process
         {
           vector<ext_transition_t> ext_transition_vector;
           if(!transition->is_sync_ask())
           {
             // transition is not SYNC_ASK
             if(!sytem_with_property)
             {
               // no properties
               ext_transition_t ext_transition;
               ext_transition.synchronized = false;
               ext_transition.first = transition;
               ext_transition_vector.push_back(ext_transition);
             }
             else
             {
               // forall properties, add this transition
               for(iter_property_transitions = property_transitions.begin();iter_property_transitions != property_transitions.end();
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
            // transition is SYNC_ASK -> add all transitions that fill the channel
             iter_channel_map = channel_map.find(transition->get_channel_gid());
             if(iter_channel_map != channel_map.end())
             {
               for(iter_transition_vector = iter_channel_map->second.begin();iter_transition_vector !=
                   iter_channel_map->second.end();iter_transition_vector++)
               {
                 if (transition->get_process_gid() != (*iter_transition_vector)->get_process_gid() ) //not synchronize with yourself
                 {
                   if(!sytem_with_property)
                   {
                     // no properties, just add all transitions that fill the channel
                     ext_transition_t ext_transition;
                     ext_transition.synchronized = true;
                     ext_transition.first = transition;
                     ext_transition.second = (*iter_transition_vector);
                     ext_transition_vector.push_back(ext_transition);
                   }
                   else
                   {
                     // for all properties, add the transitions that fill the channel
                     for(iter_property_transitions = property_transitions.begin();iter_property_transitions != property_transitions.end();
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
           // and reinsert result
           iter_transition_map->second.insert(pair<size_int_t,vector<ext_transition_t> >(transition->get_state1_lid(),ext_transition_vector));
         }
         else{
           // existing state
           if(!transition->is_sync_ask())
           {
             // NOT SYNC_ASK
             if(!sytem_with_property)
             { 
               // no properties, just add transition
               ext_transition_t ext_transition;
               ext_transition.synchronized = false;
               ext_transition.first = transition;
               iter_process_transition_map->second.push_back(ext_transition);
             }
             else{
              // forall properties, add transition
               for(iter_property_transitions = property_transitions.begin();iter_property_transitions != property_transitions.end();
                   iter_property_transitions++)
               {
                 ext_transition_t ext_transition;
                 ext_transition.synchronized = false;
                 ext_transition.first = transition;
                 ext_transition.property = (*iter_property_transitions);
                 iter_process_transition_map->second.push_back(ext_transition);
               }
             }
           }
           else
           {
             // lookup channel in channel_map
             iter_channel_map = channel_map.find(transition->get_channel_gid());
             if(iter_channel_map != channel_map.end())
             {
               // assume channel is found
               // forall transitions with SYNC_EXCLAIM (in channel_map)
               for(iter_transition_vector = iter_channel_map->second.begin();iter_transition_vector !=
                   iter_channel_map->second.end();iter_transition_vector++)
               {
                 if (transition->get_process_gid() != (*iter_transition_vector)->get_process_gid() ) //not synchronize with yourself
                 {
                   if(!sytem_with_property)
                   {
                     // no property, just add transition * all transitions with SYNC_EXCLAIM
                     ext_transition_t ext_transition;
                     ext_transition.synchronized = true;
                     ext_transition.first = transition;
                     ext_transition.second = (*iter_transition_vector);
                     iter_process_transition_map->second.push_back(ext_transition);
                   }
                   else{
                     // system has properties, forall properties, add transition * all transitions with SYNC_EXCLAIM
                     for(iter_property_transitions = property_transitions.begin();iter_property_transitions != property_transitions.end();
                         iter_property_transitions++)
                     {
                        ext_transition_t ext_transition;
                        ext_transition.synchronized = true;
                        ext_transition.first = transition;
                        ext_transition.second = (*iter_transition_vector);
                        ext_transition.property = (*iter_property_transitions);
                        iter_process_transition_map->second.push_back(ext_transition);
                     }
                   }
                 }
               }
             }
           }
         }
      }
    }
  }

  // get_succ
  string space = "   ";
  int label = 1;
  bool some_commited_state = false;
  ostr << "extern "<< '"' << "C" << '"' << " int get_successor(int next_state, char* from, char* to)" <<endl;
  ostr << " {"<<endl;
  ostr << space << "bool processes_in_deadlock = false;"<<endl;
  ostr << space << "state_struct_t *p_state_struct = (state_struct_t*)from;"<<endl;
  ostr << space << "state_struct_t *p_new_c_state = (state_struct_t*)to;"<<endl;
  ostr << space << "*p_new_c_state = c_state;"<<endl;
  ostr << space << "goto switch_state ;"<<endl;
 
  ostr << space << "l"<<label << ": if( ";
  label++;
  for(size_int_t i = 0; i < this->get_process_count(); i++)
   for(size_int_t j = 0; j < dynamic_cast<dve_process_t*>(this->get_process(i))->get_state_count(); j++)
   {
     if(dynamic_cast<dve_process_t*>(this->get_process(i))->get_commited(j))
     {
       if(some_commited_state)
         ostr << " || ";
       else
         some_commited_state = true;
       ostr << state_name << "." << get_symbol_table()->get_process(i)->get_name() << ".state" << " == "<<j;
     }
   }
   if(some_commited_state) 
    ostr << " )" << endl;
   else
    ostr << "false )" << endl;
   ostr << space << " { " << endl;  // in commited state
   space = space + "    ";

   for(size_int_t i = 0; i < this->get_process_count(); i++)
   {
     if(!sytem_with_property || i != this->get_property_gid())
     {
       //ostr << space <<"switch ( "<< state_name <<"." << get_symbol_table()->get_process(i)->get_name() << ".state )" <<endl;
       //ostr << space <<" {"<<endl;
       if(transition_map.find(i) != transition_map.end())
         for(iter_process_transition_map = transition_map.find(i)->second.begin();
             iter_process_transition_map != transition_map.find(i)->second.end();iter_process_transition_map++)
         {
           if(dynamic_cast<dve_process_t*>(this->get_process(i))->get_commited(iter_process_transition_map->first))
           {
             ostr << space <<"l"<< label << ": if( " << state_name <<"." << get_symbol_table()->get_process(i)->get_name() << ".state == " <<iter_process_transition_map->first<<" )"<<endl;
             label++;
             ostr << space << "  {"<<endl;
             for(iter_ext_transition_vector = iter_process_transition_map->second.begin();
               iter_ext_transition_vector != iter_process_transition_map->second.end();iter_ext_transition_vector++)
             {
               if( !iter_ext_transition_vector->synchronized || 
                  dynamic_cast<dve_process_t*>(this->get_process(iter_ext_transition_vector->second->get_process_gid()))->
                  get_commited(iter_ext_transition_vector->second->get_state1_lid()) ) // !! jak je to s property synchronizaci v comitted stavech !!
               {
                 ostr << space << "        if( ";
                 bool has_guard = false;
                 if(iter_ext_transition_vector->first->get_guard()!= 0 )
                 {
                   ostr << "( ";
                   write_C(*iter_ext_transition_vector->first->get_guard(), ostr, state_name);
                   has_guard = true;
                   ostr << ") ";
                 }
                 if(iter_ext_transition_vector->synchronized)
                 {
                   if(has_guard)
                     ostr <<" && "; 
                   else
                     has_guard = true;
                   ostr << state_name << "." << get_symbol_table()->get_process(iter_ext_transition_vector->second->get_process_gid())->get_name()
                        << ".state == "<< iter_ext_transition_vector->second->get_state1_lid();
                   if(iter_ext_transition_vector->second->get_guard()!= 0 )
                   {
                     if(has_guard)
                       ostr <<" && "; 
                     else
                       has_guard = true;
                    ostr << "( ";
                    write_C(*iter_ext_transition_vector->second->get_guard(), ostr, state_name);
                    ostr << ") ";
                   }
                 }
                 else
                 {
                   if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_EXCLAIM_BUFFER)
                   {
                     if(has_guard)
                       ostr <<" && "; 
                     else
                       has_guard = true;
                     ostr << "( ";
                     ostr << state_name << "." <<get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                          << "." <<"number_of_items != "
                          << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_channel_buffer_size();
                     ostr << ") ";
                   }
                   if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_ASK_BUFFER)
                   {
                     if(has_guard)
                       ostr <<" && "; 
                     else
                       has_guard = true;
                     ostr << "( ";
                     ostr << state_name << "." <<get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                          << "." <<"number_of_items != 0";
                     ostr << ") ";
                   }
                 }
                 if(sytem_with_property)
                 {
                   if(has_guard)
                     ostr <<" && "; 
                   else
                     has_guard = true;
                   ostr << state_name << "." << get_symbol_table()->get_process(iter_ext_transition_vector->property->get_process_gid())->get_name()
                        << ".state == "<< iter_ext_transition_vector->property->get_state1_lid();
                   if(iter_ext_transition_vector->property->get_guard()!= 0 )
                   {
                     if(has_guard)
                       ostr <<" && "; 
                     else
                      has_guard = true;
                    ostr << "( ";
                    write_C(*iter_ext_transition_vector->property->get_guard(), ostr, state_name);
                    ostr << ") ";
                   }
                 }
                 if(has_guard)
                   ostr <<" )"<<endl; 
                 else
                   ostr <<"true )"<<endl;
                 ostr << space << "          {" <<endl;

                 //synchronization effect
                 if(iter_ext_transition_vector->synchronized)
                 {
                    for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                    {
                      ostr << space << "             ";
                      write_C(*iter_ext_transition_vector->first->get_sync_expr_list_item(s), ostr, "(*p_new_c_state)");
                      ostr << " = ";
                      write_C(*iter_ext_transition_vector->second->get_sync_expr_list_item(s), ostr, "(c_state)");
                      ostr << ";" <<endl;
                    }
                 }
                 else
                 {
                   if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_EXCLAIM_BUFFER)
                   {
                     for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                     {
                        ostr << space << "             ";
                        ostr << "(*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name()
                             << ".content[(c_state)."<< get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                             << ".number_of_items - 1].x" << s << " = ";
                        write_C(*iter_ext_transition_vector->first->get_sync_expr_list_item(s), ostr, "(c_state)");
                        ostr << ";" << endl;
                     }
                     ostr << space << "             ";
                     ostr << "(*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                          << ".number_of_items++;"<<endl;
                   }
                   if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_ASK_BUFFER)
                   {
                     for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                     {
                        ostr << space << "             ";
                        write_C(*iter_ext_transition_vector->first->get_sync_expr_list_item(s), ostr, "(*p_new_c_state)");
                        ostr << " = (c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name()
                             << ".content[0].x" << s <<";" <<endl;
                     }
                     ostr << space << "             ";
                     ostr << "(*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                          << ".number_of_items--;"<<endl;
                     ostr << space << "             ";
                     ostr << "for(size_int_t i = 1 ; i <= (*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                          << ".number_of_items; i++)"<<endl;
                     ostr << space << "               {" <<endl;
                     for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                     {
                       ostr << space << "                 ";
                       ostr <<  "(*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() << ".content[i-1].x" << s
                            << " = (c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() << ".content[i].x" << s << ";" <<endl;
                       ostr << space << "                 ";
                       ostr <<  "(*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() << ".content[i].x" << s
                            << " = 0;" <<endl;
                     }
                     ostr << space << "               }" <<endl;
                   }
                 }
                 //first transition effect 
                 ostr << space << "             (*p_new_c_state)."<<get_symbol_table()->get_process(iter_ext_transition_vector->first->get_process_gid())->get_name()
                      << ".state = "<< iter_ext_transition_vector->first->get_state2_lid()<< ";" <<endl;
                 for(size_int_t e = 0;e < iter_ext_transition_vector->first->get_effect_count();e++)
                 {
                    ostr << space << "             ";
                    write_C(*iter_ext_transition_vector->first->get_effect(e), ostr, "(*p_new_c_state)");
                    ostr <<";"<< endl;
                 }
                 if(iter_ext_transition_vector->synchronized) //second transiton effect
                 {
                   ostr << space << "             (*p_new_c_state)."<<get_symbol_table()->get_process(iter_ext_transition_vector->second->get_process_gid())->get_name()
                        << ".state = "<< iter_ext_transition_vector->second->get_state2_lid()<< ";" <<endl;
                   for(size_int_t e = 0;e < iter_ext_transition_vector->second->get_effect_count();e++)
                   {
                      ostr << space << "             ";
                      write_C(*iter_ext_transition_vector->second->get_effect(e), ostr, "(*p_new_c_state)");
                      ostr <<";"<< endl;
                   }
                 }
                 if(sytem_with_property) //change of the property process state
                 {
                   ostr << space << "             (*p_new_c_state)."<<get_symbol_table()->get_process(iter_ext_transition_vector->property->get_process_gid())->get_name()
                        << ".state = "<< iter_ext_transition_vector->property->get_state2_lid()<< ";" <<endl;
                 }
                 ostr << space << "          }" <<endl;
               }
             }
             ostr << space << "        return " << label << ";"<<endl;
             ostr << space << "      }"<<endl;
           }
         }
       //ostr << space <<" }"<<endl;
     }
   }
   space = "   ";
   ostr << space << " } " <<endl;


   ostr << space << " else" << endl; // no in commited state
   ostr << space << " {" << endl;
   space = space + "    ";

   for(size_int_t i = 0; i < this->get_process_count(); i++)
   {
     if(!sytem_with_property || i != this->get_property_gid())
     {
       if(transition_map.find(i) != transition_map.end())
         for(iter_process_transition_map = transition_map.find(i)->second.begin();
             iter_process_transition_map != transition_map.find(i)->second.end();iter_process_transition_map++)
         {
           ostr << space << "l"<<label << ": if( " << state_name <<"." << get_symbol_table()->get_process(i)->get_name() << ".state == " <<iter_process_transition_map->first<<" )"<<endl;
           label++;
           ostr << space << "    {"<<endl;
           for(iter_ext_transition_vector = iter_process_transition_map->second.begin();
               iter_ext_transition_vector != iter_process_transition_map->second.end();iter_ext_transition_vector++)
           {
             ostr << space << "l" <<label << ":     if( ";
             label++;
             bool has_guard = false;
             if(iter_ext_transition_vector->first->get_guard()!= 0 )
             {
               ostr << "( ";
               write_C(*iter_ext_transition_vector->first->get_guard(), ostr, state_name);
               has_guard = true;
               ostr << " ) ";
             }
             if(iter_ext_transition_vector->synchronized)
             {
               if(has_guard)
                 ostr <<" && "; 
               else
                 has_guard = true;
               ostr << state_name << "." << get_symbol_table()->get_process(iter_ext_transition_vector->second->get_process_gid())->get_name()
                    << ".state == "<< iter_ext_transition_vector->second->get_state1_lid();
               if(iter_ext_transition_vector->second->get_guard()!= 0 )
               {
                 if(has_guard)
                   ostr <<" && "; 
                 else
                   has_guard = true;
                 ostr << "( ";
                 write_C(*iter_ext_transition_vector->second->get_guard(), ostr, state_name);
                 ostr << ") ";
               }
             }
             else
             {
               if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_EXCLAIM_BUFFER)
               {
                 if(has_guard)
                   ostr <<" && "; 
                 else
                   has_guard = true;
                 ostr << "( ";
                 ostr << state_name << "." <<get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                      << "." <<"number_of_items != "
                      << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_channel_buffer_size();
                 ostr << ") ";
               }
               if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_ASK_BUFFER)
               {
                 if(has_guard)
                   ostr <<" && "; 
                 else
                   has_guard = true;
                 ostr << "( ";
                 ostr << state_name << "." <<get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                      << "." <<"number_of_items != 0";
                 ostr << ") ";
               }
             }
             if(sytem_with_property)
             {
               if(has_guard)
                 ostr <<" && "; 
               else
                 has_guard = true;
               ostr << state_name << "." << get_symbol_table()->get_process(iter_ext_transition_vector->property->get_process_gid())->get_name()
                    << ".state == "<< iter_ext_transition_vector->property->get_state1_lid();
               if(iter_ext_transition_vector->property->get_guard()!= 0 )
               {
                 if(has_guard)
                   ostr <<" && "; 
                 else
                   has_guard = true;
                ostr << "( ";
                write_C(*iter_ext_transition_vector->property->get_guard(), ostr, state_name);
                ostr << ") ";
               }
             }
             if(has_guard)
               ostr <<" )"<<endl; 
             else
               ostr <<"true )"<<endl;
             ostr << space << "          {" <<endl;

             ostr << space << "             processes_in_deadlock = false;" <<endl;
             //synchronization effect
             if(iter_ext_transition_vector->synchronized)
             {
                for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                {
                  ostr << space << "             ";
                  write_C(*iter_ext_transition_vector->first->get_sync_expr_list_item(s), ostr, "(*p_new_c_state)");
                  ostr << " = ";
                  write_C(*iter_ext_transition_vector->second->get_sync_expr_list_item(s), ostr, "(c_state)");
                  ostr << ";" <<endl;
                }
             }
             else
             {
               if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_EXCLAIM_BUFFER)
               {
                 for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                 {
                    ostr << space << "             ";
                    ostr << "(*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name()
                         << ".content[(c_state)."<< get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                         << ".number_of_items].x" << s << " = ";
                    write_C(*iter_ext_transition_vector->first->get_sync_expr_list_item(s), ostr, "(c_state)");
                    ostr << ";" << endl;
                 }
                 ostr << space << "             ";
                 ostr << "(*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                         << ".number_of_items++;"<<endl;
               }
               if(iter_ext_transition_vector->first->get_sync_mode() == SYNC_ASK_BUFFER)
               {
                 for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                 {
                    ostr << space << "             ";
                    write_C(*iter_ext_transition_vector->first->get_sync_expr_list_item(s), ostr, "(*p_new_c_state)");
                    ostr << " = (c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name()
                         << ".content[0].x" << s <<";" <<endl;
                 }
                 ostr << space << "             ";
                 ostr << "(*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                      << ".number_of_items--;"<<endl;
                 ostr << space << "             ";
                 ostr << "for(size_int_t i = 1 ; i <= (*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() 
                      << ".number_of_items; i++)"<<endl;
                 ostr << space << "               {" <<endl;
                 for(size_int_t s = 0;s < iter_ext_transition_vector->first->get_sync_expr_list_size();s++)
                 {
                   ostr << space << "                 ";
                   ostr <<  "(*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() << ".content[i-1].x" << s
                        << " = (c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() << ".content[i].x" << s << ";" <<endl;
                   ostr << space << "                 ";
                   ostr <<  "(*p_new_c_state)." << get_symbol_table()->get_channel(iter_ext_transition_vector->first->get_channel_gid())->get_name() << ".content[i].x" << s
                        << " = 0;" <<endl;
                 }
                 ostr << space << "               }" <<endl;
               }
             }
             //first transition effect 
             ostr << space << "             (*p_new_c_state)."<<get_symbol_table()->get_process(iter_ext_transition_vector->first->get_process_gid())->get_name()
                  << ".state = "<< iter_ext_transition_vector->first->get_state2_lid()<< ";" <<endl;
             for(size_int_t e = 0;e < iter_ext_transition_vector->first->get_effect_count();e++)
             {
                ostr << space << "             ";
                write_C(*iter_ext_transition_vector->first->get_effect(e), ostr, "(*p_new_c_state)");
                ostr <<";"<< endl;
             }
             if(iter_ext_transition_vector->synchronized) //second transiton effect
             {
               ostr << space << "             (*p_new_c_state)."<<get_symbol_table()->get_process(iter_ext_transition_vector->second->get_process_gid())->get_name()
                    << ".state = "<< iter_ext_transition_vector->second->get_state2_lid()<< ";" <<endl;
               for(size_int_t e = 0;e < iter_ext_transition_vector->second->get_effect_count();e++)
               {
                  ostr << space << "             ";
                  write_C(*iter_ext_transition_vector->second->get_effect(e), ostr, "(*p_new_c_state)");
                  ostr <<";"<< endl;
               }
             }
             if(sytem_with_property) //change of the property process state
             {
               ostr << space << "             (*p_new_c_state)."<<get_symbol_table()->get_process(iter_ext_transition_vector->property->get_process_gid())->get_name()
                    << ".state = "<< iter_ext_transition_vector->property->get_state2_lid()<< ";" <<endl;
             }
             //ostr << space << "              print_state(*p_new_c_state,cout);"<<endl;
             ostr << space << "              return " <<label <<";"<<endl;
             ostr << space << "          }" <<endl;
           }
           ostr << space << "      }"<<endl;
         }
         //ostr << space <<" }"<<endl;
     }
   }
   ostr << space <<" }"<<endl;
   ostr << space << "l" << label << ": if( processes_in_deadlock )" <<endl;
   label++;
   ostr << space << " {" <<endl;
   for(iter_property_transitions = property_transitions.begin();iter_property_transitions != property_transitions.end();
       iter_property_transitions++)
   {
        ostr << space << "l" << label << ": if( ";
        label++;
        ostr << state_name << "." << get_symbol_table()->get_process((*iter_property_transitions)->get_process_gid())->get_name()
                    << ".state == "<< (*iter_property_transitions)->get_state1_lid();
        if( (*iter_property_transitions)->get_guard()!= 0 )
        {
          ostr << space << " && ( ";
          write_C(*(*iter_property_transitions)->get_guard(), ostr, state_name);
          ostr << " )"<<endl;
        }
        ostr << " )"<<endl;

        ostr << space << "    {" <<endl;
        ostr << space << "      (*p_new_c_state)."<<get_symbol_table()->get_process((*iter_property_transitions)->get_process_gid())->get_name()
                    << ".state = "<< (*iter_property_transitions)->get_state2_lid()<< ";" <<endl;
        ostr << space << "      return " <<label <<";"<<endl;
        ostr << space << "    }"<<endl;
   }
   ostr << space << " }" <<endl;


  ostr << space << "l" << label << ": return 0;" <<endl;
  ostr << "  switch_state :"<<endl;
  ostr << space << "switch(next_state)" <<endl;
  ostr << space << " {" <<endl;  
  for(int i=1; i<=label; i++)
   if (i==1)
    ostr<< space << "  case " << i << " : {processes_in_deadlock = true; goto l" << i << ";}" << endl;
   else
    ostr<< space << "  case " << i << " : goto l" << i << ";" << endl; 
  ostr << space << " }" <<endl;
  ostr << space << "}" <<endl;

  ostr<<endl;

  ostr << "extern "<< '"' << "C" << '"' << " bool is_accepting(char* state, int size)" <<endl; // only Buchi acceptance condition
  ostr << " {"<<endl;
  if(sytem_with_property)
  {
    ostr << space << "state_struct_t *p_state_struct = (state_struct_t*)state;"<<endl;
    for(size_int_t i = 0; i < dynamic_cast<dve_process_t*>(this->get_process((this->get_property_gid())))->get_state_count(); i++)
    {
      if (dynamic_cast<dve_process_t*>(this->get_process((this->get_property_gid())))->get_acceptance(i, 0, 1) )
      {
         ostr << space << "if(p_state_struct->" << get_symbol_table()->get_process(this->get_property_gid())->get_name() << ".state == " 
              <<  i  <<" ) return true;" << endl;
      }
    }
    ostr << space << "return false;" << endl;
  }
  else{
  ostr << "    return false;"<<endl;
  }
  ostr << " }"<<endl;

  ostr<<endl;

  ostr << "extern "<< '"' << "C" << '"' << " int get_state_size()" <<endl;
  ostr << " {"<<endl;
  ostr << "   return state_size; "<<endl;
  ostr << " }"<<endl;

  ostr<<endl;

  ostr << "extern "<< '"' << "C" << '"' << " void get_initial_state(char* to)" <<endl;
  ostr << " {"<<endl;
  ostr << "   memcpy(to,initial_state,state_size); "<<endl;
  ostr << " }"<<endl;
}

