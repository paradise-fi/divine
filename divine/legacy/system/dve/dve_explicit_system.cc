#include "system/dve/dve_explicit_system.hh"
#include "system/dve/dve_system.hh"
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "system/dve/syntax_analysis/dve_grammar.hh"
#include "system/dve/dve_process_decomposition.hh"
#include "system/state.hh"
#include "common/bit_string.hh"
#include "common/error.hh"
#include "common/deb.hh"

#include <divine/datastruct.h>

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif
using std::ostream;
using std::vector;


//#define DEBFUNC(x) x

////FUNCTIONS for evaluation in explicit system
inline
all_values_t ES_eval_id_compact
  (const void * params, const size_int_t & gid, bool & eval_err)
{
 if (ES_p_parameters_t(params)->var_types[gid] == VAR_INT)
   return state_pos_to<sshort_int_t>(
      ES_p_parameters_t(params)->state,
      ES_p_parameters_t(params)->state_positions_var[gid]);
 else //(aux_var_type == VAR_BYTE)
   return state_pos_to<byte_t>(
      ES_p_parameters_t(params)->state,
      ES_p_parameters_t(params)->state_positions_var[gid]);
}

all_values_t ES_eval_square_bracket_compact
  (const void * params, const size_int_t  & gid,
   const size_int_t & array_index, bool & eval_err)
{
  DEBFUNC(cerr<<" eval[]: gid="<<gid<<" index:"<<array_index<<endl;);
  if (array_index<ES_p_parameters_t(params)->array_sizes[gid])
    {
      if (ES_p_parameters_t(params)->var_types[gid] == VAR_INT)
	return state_pos_to<sshort_int_t>
	  (ES_p_parameters_t(params)->state,
	   ES_p_parameters_t(params)->state_positions_var[gid] +
           array_index*2);
      else //(aux_var_type == VAR_BYTE)
	return state_pos_to<byte_t>
	  (ES_p_parameters_t(params)->state,
	   ES_p_parameters_t(params)->state_positions_var[gid] +
           array_index);
    }
  else
    {
      eval_err = true;  //in case of bad array index
      return 0;
    }
}

all_values_t ES_eval_dot_compact
  (const void * params, const dve_symbol_table_t *symbol_table, const size_int_t & gid,  bool & eval_err)
{
  DEBFUNC(cerr << "CALL of ES_eval_dot" << endl;)
    return (state_pos_to<dve_state_int_t>
	    (ES_p_parameters_t(params)->state,
	     ES_p_parameters_t(params)->state_positions_state[gid])
	    == ES_p_parameters_t(params)->state_lids[gid]);
}

all_values_t ES_eval_id
  (const void * params, const dve_expression_t & subexpr, bool & eval_err)
{
 if (ES_p_parameters_t(params)->var_types[subexpr.get_ident_gid()] == VAR_INT)
   return state_pos_to<sshort_int_t>(
      ES_p_parameters_t(params)->state,
      ES_p_parameters_t(params)->state_positions_var[subexpr.get_ident_gid()]);
 else //(aux_var_type == VAR_BYTE)
   return state_pos_to<byte_t>(
      ES_p_parameters_t(params)->state,
      ES_p_parameters_t(params)->state_positions_var[subexpr.get_ident_gid()]);
}

all_values_t ES_eval_square_bracket
  (const void * params, const dve_expression_t & subexpr, const size_int_t & array_index,
   bool & eval_err)
{
 if (array_index<ES_p_parameters_t(params)->array_sizes[subexpr.get_ident_gid()])
  {
   if (ES_p_parameters_t(params)->var_types[subexpr.get_ident_gid()] == VAR_INT)
     return state_pos_to<sshort_int_t>(
        ES_p_parameters_t(params)->state,
        ES_p_parameters_t(params)->state_positions_var[subexpr.get_ident_gid()] +
           array_index*2);
   else //(aux_var_type == VAR_BYTE)
     return state_pos_to<byte_t>(
        ES_p_parameters_t(params)->state,
        ES_p_parameters_t(params)->state_positions_var[subexpr.get_ident_gid()] +
           array_index);
  }
 else
  {
   eval_err = true;  //in case of bad array index
   return 0;
  }
}

all_values_t ES_eval_dot
   (const void * params, const dve_expression_t & subexpr, bool & eval_err)
{
 DEBFUNC(cerr << "CALL of ES_eval_dot" << endl;)
 return (state_pos_to<dve_state_int_t>(ES_p_parameters_t(params)->state,
               ES_p_parameters_t(params)->state_positions_state[subexpr.get_ident_gid()])
         == ES_p_parameters_t(params)->state_lids[subexpr.get_ident_gid()]);
}


////METHODS of explicit_system:
dve_explicit_system_t::dve_explicit_system_t(error_vector_t & evect):
  system_t(evect), explicit_system_t(evect), dve_system_t(evect)
{
 //initialization of the data:
 aux_enabled_trans = 0; aux_enabled_trans2 = 0;
 aux_succ_container =0; max_succ_count = 0;
 channels = 0;
 state_positions_var = 0; state_positions_state = 0; state_positions_proc = 0;
 state_positions_channel = 0; channel_buffer_size = 0; channel_element_size = 0;
 channel_item_type=0; channel_item_pos=0;
 state_sizes_var = 0;
 process_positions = 0;
 array_sizes = 0;
 property_decomposition = 0;
 get_abilities().system_can_decompose_property = true;
}

dve_explicit_system_t::~dve_explicit_system_t()
{
 DEB(cerr << "BEGIN of destructor of dve_explicit_system_t" << endl;)
 if (parameters) delete ES_p_parameters_t(parameters);
 safe_delete_array(channels);
 safe_delete_array(state_positions_var);
 safe_delete_array(state_positions_state);
 safe_delete_array(state_positions_proc);
 safe_delete_array(state_positions_channel);
 safe_delete_array(channel_buffer_size);
 safe_delete_array(channel_element_size);
 safe_delete_array(channel_item_type);
 safe_delete_array(channel_item_pos);
 safe_delete_array(state_sizes_var);
 safe_delete_array(process_positions);
 safe_delete_array(array_sizes);
 safe_delete(aux_enabled_trans);
 safe_delete(aux_enabled_trans2);
 safe_delete(aux_succ_container);
 safe_delete(property_decomposition);
 for (size_int_t gf_i=0; gf_i<glob_filters.size(); ++gf_i)
   free(glob_filters[gf_i]);
 //delete [] gone; //only for combined_succs
 DEB(cerr << "END of destructor of dve_explicit_system_t" << endl;)
}

slong_int_t dve_explicit_system_t::read(const char * const filename,  bool do_expr_compaction)
{
 int result = dve_system_t::read(filename);
 if (result==0)
   {
     another_read_stuff(do_expr_compaction);
     if (get_with_property())
       {
	 property_decomposition = new dve_process_decomposition_t(*this);
	 property_decomposition->parse_process(get_property_gid());
       }
   }
 return result;
}

void dve_explicit_system_t::process_variable(const size_int_t gid)
{
 const dve_symbol_t * cp_symb = psymbol_table->get_variable(gid);
 if (!cp_symb->is_const())
  {
   state_positions_var[gid] = space_sum;
   if (!cp_symb->is_vector())
    {
     array_sizes[gid] = 0;
     if (cp_symb->is_byte()) { state_sizes_var[gid] = 1; }
     else if (cp_symb->is_int()) { state_sizes_var[gid]= 2; }
     else terr <<"Unexpected error: unrecognized type of variable" << thr();
     space_sum += state_sizes_var[gid];
    }
   else
    {
     array_sizes[gid] = cp_symb->get_vector_size();
     if (cp_symb->is_byte())
       { state_sizes_var[gid] = 1*cp_symb->get_vector_size(); }
     else if (cp_symb->is_int())
       { state_sizes_var[gid] = 2*cp_symb->get_vector_size(); }
     else terr <<"Unexpected error: unrecognized type of variable" << thr();
     space_sum += state_sizes_var[gid];
    }
   
   state_creators.push_back(
      state_creator_t(state_creator_t::VARIABLE,cp_symb->get_var_type(),
                      cp_symb->get_vector_size(),gid));
   state_creators_count++;
  }
}

void dve_explicit_system_t::process_channel(const size_int_t gid)
{
 const dve_symbol_t * cp_symb = psymbol_table->get_channel(gid);
 if (cp_symb->get_channel_typed() && cp_symb->get_channel_buffer_size())
  {
   state_positions_channel[gid] = space_sum;
   space_sum += 2; //16 bits for buffer size (ushort_int_t)
   channel_buffer_size[gid] = cp_symb->get_channel_buffer_size();
   size_int_t types_count = cp_symb->get_channel_type_list_size();
   channel_item_pos[gid] = new size_int_t[types_count];
   channel_item_type[gid] = new dve_var_type_t[types_count];
   size_int_t element_size = 0;
   for (size_int_t i=0; i!=types_count; ++i)
    {
     dve_var_type_t var_type = cp_symb->get_channel_type_list_item(i);
     channel_item_type[gid][i] = var_type;
     channel_item_pos[gid][i] = element_size;
     if (var_type==VAR_BYTE) element_size += 1; 
     else if (var_type==VAR_INT) element_size += 2;
     else terr <<"Unexpected error: unrecognized type of channel item" << thr();
    }
   channel_element_size[gid] = element_size;
   space_sum += element_size*cp_symb->get_channel_buffer_size();
   state_creators.push_back(
      state_creator_t(state_creator_t::CHANNEL_BUFFER,VAR_BYTE,0,gid));
   state_creators_count++;
  }
 else
  {
   state_positions_channel[gid] = MAX_SIZE_INT;
   channel_buffer_size[gid] = 0;
   channel_element_size[gid] = 0;
   channel_item_pos[gid] = 0;
   channel_item_type[gid] = 0;
  }
}

size_int_t dve_explicit_system_t::channel_content_count(const state_t & state,
                                                        const size_int_t gid)
{ return state_pos_to<ushort_int_t>(state, state_positions_channel[gid]); }

bool dve_explicit_system_t::channel_is_empty(const state_t & state,
                                             const size_int_t gid)
{ return (!bool(channel_content_count(state,gid))); }

bool dve_explicit_system_t::channel_is_full(const state_t & state,
                                            const size_int_t gid)
{ return (channel_content_count(state,gid) == channel_buffer_size[gid]); }

void dve_explicit_system_t::push_back_channel(state_t & state,
                                              const size_int_t gid)
{ 
 const size_int_t count = channel_content_count(state,gid);
 set_to_state_pos<ushort_int_t>(state, state_positions_channel[gid], count+1);
}

void dve_explicit_system_t::pop_front_channel(state_t & state,
                                              const size_int_t gid)
{
 const size_int_t count = channel_content_count(state,gid);
 set_to_state_pos<ushort_int_t>(state, state_positions_channel[gid], count-1);
 //move the queue
 memmove(state.ptr+state_positions_channel[gid]+2,
         state.ptr+state_positions_channel[gid]+2+channel_element_size[gid],
         (count-1)*channel_element_size[gid]);
 //fill released space with zeros
 memset(state.ptr+state_positions_channel[gid]+2+
        (count-1)*channel_element_size[gid],0,channel_element_size[gid]);
}

void dve_explicit_system_t::write_to_channel(state_t & state,
                                             const size_int_t gid,
                                             const size_int_t item_index,
                                             const all_values_t value)
{
 const size_int_t count = channel_content_count(state,gid);
 if (channel_item_type[gid][item_index]==VAR_BYTE)
   byte_to_state_pos(state,
       state_positions_channel[gid]+2+(count-1)*channel_element_size[gid]+
       channel_item_pos[gid][item_index],
       byte_t(value));
 else if (channel_item_type[gid][item_index]==VAR_INT)
   int_to_state_pos(state,
       state_positions_channel[gid]+2+(count-1)*channel_element_size[gid]+
       channel_item_pos[gid][item_index],
       sshort_int_t(value));
 else gerr << "Unexpected error: Unrecognized channel item type" << thr();
}

all_values_t dve_explicit_system_t::read_from_channel(const state_t & state,
                                                    const size_int_t gid,
                                                    const size_int_t item_index,
                                                    const size_int_t elem_index)
 //defautly elem_index==0;                                                    
{
 if (channel_item_type[gid][item_index]==VAR_BYTE)
   return state_pos_to<byte_t>(state,
       state_positions_channel[gid]+2+channel_item_pos[gid][item_index]+
       elem_index*channel_element_size[gid]);
 else if (channel_item_type[gid][item_index]==VAR_INT)
   return state_pos_to<sshort_int_t>(state,
       state_positions_channel[gid]+2+channel_item_pos[gid][item_index]+
       elem_index*channel_element_size[gid]);
 else
  { gerr << "Unexpected error: Unrecognized channel item type" << thr();
    return 0; /*unreachable*/ }
}

void dve_explicit_system_t::another_read_stuff(bool do_expr_compaction)
{
 DEBFUNC(cerr << "BEGIN of another_read_stuff()" << endl;)
 glob_count = get_global_variable_count();
 eval_id = ES_eval_id;
 eval_square_bracket = ES_eval_square_bracket;
 eval_dot = ES_eval_dot;
 eval_id_compact = ES_eval_id_compact;
 eval_square_bracket_compact = ES_eval_square_bracket_compact;
 eval_dot_compact = ES_eval_dot_compact;

 safe_delete_array(channels);
 channels = new channels_t[get_channel_count()];
 for (size_int_t i = 0; i!=get_channel_count(); ++i)
   channels[i].allocate(get_channel_freq(i));
// gone = new size_int_t[process_field_size];  //only for combined_succs
 
 /* now we will create a lists of state positions for each GID */
 space_sum =0;
 state_creators_count = 0;
 process_count = processes.size();

 safe_delete_array(state_positions_var);
 state_positions_var = new size_int_t[var_count];

 safe_delete_array(state_positions_state);
 state_positions_state = new size_int_t[state_count];

 safe_delete_array(state_positions_proc);
 state_positions_proc = new size_int_t[processes.size()];

 safe_delete_array(state_positions_channel);
 state_positions_channel = new size_int_t[get_channel_count()];

 safe_delete_array(channel_buffer_size);
 channel_buffer_size = new size_int_t[get_channel_count()];

 safe_delete_array(channel_element_size);
 channel_element_size = new size_int_t[get_channel_count()];

 safe_delete_array(channel_item_type);
 channel_item_type = new dve_var_type_t*[get_channel_count()];

 safe_delete_array(channel_item_pos);
 channel_item_pos = new size_int_t*[get_channel_count()];

 safe_delete_array(state_sizes_var);
 state_sizes_var = new size_int_t[var_count];

 safe_delete_array(array_sizes);
 array_sizes = new size_int_t[var_count];

 if (parameters) delete ES_p_parameters_t(parameters);
 parameters = (void *)(new ES_parameters_t);
 ES_p_parameters_t(parameters)->state_positions_var = state_positions_var;
 ES_p_parameters_t(parameters)->state_positions_state = state_positions_state;
 ES_p_parameters_t(parameters)->state_positions_proc = state_positions_proc;
 ES_p_parameters_t(parameters)->initial_values = initial_values;
 ES_p_parameters_t(parameters)->initial_states = initial_states;
 ES_p_parameters_t(parameters)->state_lids = state_lids;
 ES_p_parameters_t(parameters)->var_types = var_types;
 ES_p_parameters_t(parameters)->array_sizes = array_sizes;

 safe_delete_array(process_positions);
 process_positions = new process_position_t[process_count];

 trans_count = dve_system_t::get_trans_count();
 max_succ_count = trans_count;
 for (size_int_t i=0; i!=get_channel_count(); i++)
   if (get_channel_freq(i)>1)
     max_succ_count += get_channel_freq(i)*get_channel_freq(i)-1;
 if (get_with_property())
     max_succ_count *= processes[prop_gid]->get_trans_count();

 safe_delete(aux_enabled_trans);
 aux_enabled_trans = new enabled_trans_container_t(*this);

 safe_delete(aux_enabled_trans2);
 aux_enabled_trans2 = new enabled_trans_container_t(*this);

 safe_delete(aux_succ_container);
 aux_succ_container = new succ_container_t();
 if (get_with_property())
   property_trans.extend(pproperty->get_trans_count());

 global_position.begin=space_sum;
 size_int_t var_count = get_global_variable_count();
 for (size_int_t j = 0; j!=var_count; ++j)
   process_variable(get_global_variable_gid(j));
 
 size_int_t channel_count = get_channel_count();
 for (size_int_t i = 0; i!=channel_count; ++i)
   process_channel(i);
   
 global_position.size = space_sum - global_position.begin;
 
 for (size_int_t i = 0; i!=process_count; ++i)
  {
   const dve_process_t * cp_process = processes[i];
   process_positions[i].begin = space_sum;
   
   //Processing the place for state of process:
   state_positions_proc[i] = space_sum;
   size_int_t state_count = cp_process->get_state_count();
   /* also all states of this process will have the same state position: */
   for (size_int_t j = 0; j!=state_count; ++j)
     state_positions_state[cp_process->get_state_gid(j)] = space_sum;
   space_sum += sizeof(dve_state_int_t);
   state_creators.push_back(
     state_creator_t(state_creator_t::PROCESS_STATE,VAR_BYTE,0,i));
   state_creators_count++;
   
   //Processing variables:
   size_int_t var_count = cp_process->get_variable_count();
   for (size_int_t j = 0; j!=var_count; ++j)
     process_variable(cp_process->get_variable_gid(j));
   process_positions[i].size =
     space_sum - process_positions[i].begin;
  }
 
 for (size_int_t gf_i=0; gf_i<glob_filters.size(); ++gf_i)
   free(glob_filters[gf_i]);
 glob_filters.clear();
 
 if (get_with_property())
  {
   property_position = state_positions_proc[pproperty->get_gid()];
   prop_begin = process_positions[prop_gid].begin;
   prop_size = process_positions[prop_gid].size;
  }
 
 size_int_t trans_count = this->get_trans_count();

 for (size_int_t j = 0; j != trans_count; ++j)
  {
   dve_transition_t * transition =dynamic_cast<dve_transition_t*>(get_transition(j));
       
   byte_t * glob_filter = (byte_t *)(calloc(global_position.size,sizeof(byte_t)));
   const bit_string_t & b_str = transition->get_glob_mask();
   if (b_str.get_bit_count()!=glob_count)
     terr << "Unexpected error: <bit_string_t>.get_bits()!=glob_count"
          << thr();
   for (size_int_t var_index=0; var_index!=glob_count; ++var_index)
    {
     if (b_str.get_bit(var_index))
      {
       size_int_t gid = get_global_variable_gid(var_index);
       size_int_t begin = state_positions_var[gid];
       size_int_t upper_bound = state_sizes_var[gid] + begin;
       
       for (size_int_t byte_index=begin; byte_index!=upper_bound;
            ++byte_index)
         glob_filter[byte_index] |= 255;
      }
    }
   glob_filters.push_back(glob_filter);   
  }

 if (do_expr_compaction)
   {
     //cerr<<"Using compacted expressions."<<endl;
     DEBFUNC(cerr<<"Compaction of expressions ...";);
     for (size_int_t j = 0; j != trans_count; ++j)
       {
	 dve_transition_t * transition =dynamic_cast<dve_transition_t*>(get_transition(j));
	 if (transition->get_guard())
	   {
	     
	     DEBFUNC(cerr<<" GUARD:"<<transition->get_guard()->to_string()<<endl;);
	     transition->get_guard()->compaction();
	   }
	 for (size_t k=0; k<transition->get_effect_count(); k++)
	   {
	     DEBFUNC(cerr<<" EFFECT:"<<transition->get_effect(k)->to_string()<<endl;);
	     (transition->get_effect(k))->compaction();
	   }
       }
     DEBFUNC(cerr<<" done."<<endl;);
   }

 DEBFUNC(cerr << "END of another_read_stuff()" << endl;)
};

size_int_t dve_explicit_system_t::get_space_sum() const
{ return space_sum; }

state_t dve_explicit_system_t::get_initial_state()
{
 DEB(cerr << "BEGIN of get_initial_state() " << endl;)
 state_t state = new_state(get_space_sum());
 size_int_t values_count;
 DEB(DBG_print_state_CR(state,cerr);)
 for (size_int_t i = 0; i!=state_creators_count; ++i)
 {
//  DEB(cerr << "Taking next symbol in consideration" << endl;)
  state_creator_t & state_creator = state_creators[i];
  if (state_creator.type == state_creator_t::VARIABLE)
   {
    if (state_creator.array_size) 
     { //symbol is vector
      values_count = initial_values_counts[state_creator.gid];
      DEB(cerr << "Symbol is a vector of length " << values_count << endl;)
      if (state_creator.var_type == VAR_BYTE)
        for (size_int_t j=0; j!=values_count; ++j)
         {
          byte_to_state_pos(state,state_positions_var[state_creator.gid]+j,
                            initial_values[state_creator.gid].all_values[j]);
          DEB(DBG_print_state_CR(state,cerr);)
         }
      else //state_creator.var_type == VAR_INT
        for (size_int_t j=0; j!=values_count; ++j)
         {
          int_to_state_pos(state,state_positions_var[state_creator.gid]+j*2,
                           initial_values[state_creator.gid].all_values[j]);
          DEB(DBG_print_state_CR(state,cerr);)
         }
     }
    else //symbol is scalar
     {
      DEB(cerr << "Symbol is scalar" << endl;)
      if (initial_values_counts[state_creator.gid])
       {
        DEB(cerr << "Symbol is initialized to " << initial_values[state_creator.gid].all_value)
        DEB(     << endl;)
        if (state_creator.var_type == VAR_BYTE)
          byte_to_state_pos(state,state_positions_var[state_creator.gid],
                            initial_values[state_creator.gid].all_value);
        else //state_creator.var_type == VAR_INT
          int_to_state_pos(state,state_positions_var[state_creator.gid],
                           initial_values[state_creator.gid].all_value);
        DEB(DBG_print_state_CR(state,cerr);)
       }
      else
       { DEB(cerr << "NOCHANGE" << endl;)
        /* nothing because new_state() zeroes the space */ }
     }
   }
  else if (state_creator.type==state_creator_t::PROCESS_STATE)
   {// it's in a state of process
    DEB(cerr << "Symbol is procname" << endl;)
    set_to_state_pos<dve_state_int_t>(state,state_positions_proc[state_creator.gid],
         dve_state_int_t(initial_states[state_creator.gid]));
    DEB(DBG_print_state_CR(state,cerr);)
   }
  else if (state_creator.type==state_creator_t::CHANNEL_BUFFER)
   {
    set_to_state_pos<ushort_int_t>(state,
                                   state_positions_channel[state_creator.gid],
                                   0);//buffers are initially empty
   }
 }
 
 DEB(cerr << "END of get_initial_state() " << endl;)
 return state;
}


bool dve_explicit_system_t::set_state_creator_value_of_var_type
  (state_t state, const size_int_t var_gid, const dve_var_type_t var_type,
   const all_values_t v, const size_int_t index)
{
 DEB(cerr << "Setting value "<< v << " to var ")
 DEB(     << psymbol_table->get_variable(var_gid)->get_name() << endl;)
 if (var_type == VAR_BYTE)
  {
   DEB(cerr << "Setting it to byte" << endl;)
   byte_to_state_pos(state,state_positions_var[var_gid]+index*(sizeof(byte_t)),
                     byte_t(v));
   if (v > MAX_BYTE || v < MIN_BYTE)
     return true;  //unsuccesfull (breaking bounds)
   else
     return false; //result OK
  }
 else //it is int
  {
   DEB(cerr << "Setting to int variable" << endl;)
   int_to_state_pos(state,state_positions_var[var_gid]+index*(sizeof(sshort_int_t)),
                    sshort_int_t(v));
   if (v > MAX_SSHORT_INT || v < MIN_SSHORT_INT)
     return true;  //unsuccesfull (breaking bounds)
   else
     return false; //result OK
  }
}


all_values_t dve_explicit_system_t::get_state_creator_value_of_var_type
  (const state_t state, size_int_t var_gid, const dve_var_type_t var_type,
   const size_int_t index)
{
 if (var_type == VAR_BYTE) //it is byte
  return state_pos_to_byte(state,
                           state_positions_var[var_gid]+index*sizeof(byte_t));
 else //it is int (var_type == VAR_INT)
  return state_pos_to_int(state,
                          state_positions_var[var_gid]+index*sizeof(sshort_int_t));
}

size_int_t dve_explicit_system_t::get_state_of_process
  (const state_t state, size_int_t process_id) const
{
 return state_pos_to<dve_state_int_t>
   (state,state_positions_proc[processes[process_id]->get_gid()]);
}

int dve_explicit_system_t::get_async_enabled_trans
      (state_t state, enabled_trans_container_t & enb_trans)
{
 int result = SUCC_NORMAL;
 bool only_commited = is_commited(state);
 p_enabled_trans = &enb_trans;
 p_enabled_trans->clear();
 prepare_channels_info();
 if (get_with_property())
  {
   property_trans.clear();
   DEB(cerr << "computing enabled transitions in a property process")
   DEB(     << prop_gid << endl;)
   if (compute_enabled_of_property(state))
     result |= SUCC_ERROR;
   p_enabled_trans->set_property_succ_count(property_trans.size());
  }
 else p_enabled_trans->set_property_succ_count(0);
 
 size_int_t process_index;
 for (process_index = 0; process_index!=processes.size(); ++process_index)
   //we do not test whether process is "property process", because it should
   //not have synchonizations through ?-channel
   if (compute_enabled_stage1(process_index, channels, state, only_commited))
     result |= SUCC_ERROR;

 for (process_index = 0; process_index!=processes.size(); ++process_index)
  {
   if (!((get_with_property()) && process_index == prop_gid))
     if (compute_enabled_stage2(process_index, channels, state, only_commited))
       result |= SUCC_ERROR;
   p_enabled_trans->set_next_begin(process_index,p_enabled_trans->size());
  }
 
 if (p_enabled_trans->size()==0) {
   if (get_with_property())
    {
     //systems with property are in deadlock only if property process
     //is not in a deadlock
     if (property_trans.size()>0)
      {
       result |= SUCC_DEADLOCK;
       DEB(enabled_trans_container_t::iterator safe = p_enabled_trans->begin();)
       p_enabled_trans->extend(property_trans.size());
       DEB(if (safe!=p_enabled_trans->begin()) cerr << "WARNING: reallocation!!!" << endl;)
       enabled_trans_container_t::iterator iter=p_enabled_trans->begin();
       for (size_int_t i=0; i!=property_trans.size(); i++, iter++)
        {
         iter->set_count(1);
         (*iter)[0] = property_trans[i].trans;
         iter->set_erroneous(property_trans[i].error);
        }
       //property process is the only owner of enabled transitions:
       p_enabled_trans->set_next_begin(get_property_gid(),property_trans.size());
      }
     //else simply return empty list of successors without any special value
    }
   else
     //when system is does nto contain property, then 
     //`p_enabled_trans->size()==0' implies deadlock
     result |= SUCC_DEADLOCK; 
 }
 
 
 return result;
}

int dve_explicit_system_t::get_sync_enabled_trans(const state_t state,
                                        enabled_trans_container_t & enb_trans)
{
 aux_succ_container->clear();
 return get_sync_succs(state, *aux_succ_container, enb_trans);
}

int dve_explicit_system_t::get_enabled_ith_trans(const state_t state,
                                                   const size_int_t i,
                                                   enabled_trans_t & enb_trans)
{
 int result = get_enabled_trans(state,*aux_enabled_trans);
 enb_trans = (*aux_enabled_trans)[i];
 return result;
}
                                  
int dve_explicit_system_t::get_enabled_trans_count(const state_t state,
                                                     size_int_t & count)
{
 int result = get_enabled_trans(state,*aux_enabled_trans);
 count = aux_enabled_trans->size();
 return result;
}

int dve_explicit_system_t::get_ith_succ(state_t state, const int i,
                                    state_t & succ)
{
 dve_enabled_trans_t trans;
 int result = get_enabled_ith_trans(state,i,trans);
 if (get_async_enabled_trans_succ(state,trans,succ))
   return (result|SUCC_ERROR);
 else
   return result;
}


int dve_explicit_system_t::get_async_succs_internal
      (const state_t state, succ_container_t & succs)
{
 int result = get_async_enabled_trans(state,(*p_enabled_trans));
 if (get_async_enabled_trans_succs(state,succs,(*p_enabled_trans)))
   return (result|SUCC_ERROR);
 else
   return result;
}

void dve_explicit_system_t::set_state_creator_value_extended(
   const state_t & state, const state_t & new_state,
   const dve_expression_t & to_assign, const all_values_t & val, bool & error)
{
 size_int_t array_index = 0;
 if (to_assign.get_operator() == T_SQUARE_BRACKETS)
   array_index = eval_expr(to_assign.left(),state,error);
 
 //if it is not an array or array index doens't break the bounds
 size_int_t array_size = array_sizes[to_assign.get_ident_gid()];
 if ( array_size==0  ||  (array_index>=0 && array_index<array_size) )
   error = error || //Lazy evaluation!!!
           set_state_creator_value(new_state,to_assign.get_ident_gid(),
                                   val,array_index);
 else error = true;
}

bool dve_explicit_system_t::get_async_enabled_trans_succ_without_property
     (const state_t state, const enabled_trans_t & enabled,
      state_t & new_state)
{
 bool error = enabled.get_erroneous();
 new_state = duplicate_state(state);
 
 dve_transition_t * const sync_trans = get_receiving_trans(enabled),
                  * const trans = get_sending_or_normal_trans(enabled);
 
 //Evaluation of synchronization:
 if (sync_trans)
  {
   dve_symbol_t * p_channel =
     psymbol_table->get_channel(sync_trans->get_channel_gid());
   bool typed_transmission = p_channel->get_channel_typed();
   
   const size_int_t answ_expr_count = trans->get_sync_expr_list_size();
   if (answ_expr_count != 0)
    {
     const size_int_t ask_expr_count = sync_trans->get_sync_expr_list_size();
     if (ask_expr_count == answ_expr_count) //but it is also checked during
      {                                     //parsing of DVE source
       for (size_int_t i=0; i!=ask_expr_count; ++i)
        {
         //take i-th item of transmitted value and receiving variable
         const dve_expression_t & answ_expr =
           *trans->get_sync_expr_list_item(i);
         const dve_expression_t & ask_expr =
           *sync_trans->get_sync_expr_list_item(i);
         //compute transmitted value
         all_values_t val = eval_expr(&answ_expr,new_state,error);
         if (typed_transmission)
           val = retype(p_channel->get_channel_type_list_item(i), val);
         //store computed value to the variable given by `ask_expr'
         set_state_creator_value_extended(state, new_state, ask_expr,
                                          val, error);
        }
      }
     else error = true;
    }
   //Lazy evaluation!!!
   error = error || apply_transition_effects(new_state,sync_trans);
  }

 if (trans) //normal transition/send/buffered send/buffered receive
  {
   error = error || apply_transition_effects(new_state,trans); //Lazy!!!
   if (trans->get_sync_mode()==SYNC_EXCLAIM_BUFFER) //buffered send
    {
     const size_int_t answ_expr_count = trans->get_sync_expr_list_size();
     push_back_channel(new_state,trans->get_channel_gid());
     for (size_int_t i=0; i!=answ_expr_count; ++i)
      {
       all_values_t val = eval_expr(trans->get_sync_expr_list_item(i),
                                    new_state,error);
       write_to_channel(new_state,trans->get_channel_gid(),i,val);
      }
    }
   else if (trans->get_sync_mode()==SYNC_ASK_BUFFER) //buffered receive
    {
     const size_int_t ask_expr_count = trans->get_sync_expr_list_size();
     for (size_int_t i=0; i!=ask_expr_count; ++i)
      {
       all_values_t val =
           read_from_channel(new_state,trans->get_channel_gid(),i);
       const dve_expression_t & ask_expr = *trans->get_sync_expr_list_item(i);
       set_state_creator_value_extended(state, new_state, ask_expr,
                                        val, error);
      }
     pop_front_channel(new_state,trans->get_channel_gid());
    }
  }
 if (error) go_to_error(new_state);
 
 return error;
}

bool dve_explicit_system_t::get_sync_enabled_trans_succ
    (const state_t state, const enabled_trans_t & enabled, state_t & new_state)
{
 bool error = enabled.get_erroneous();
 new_state = duplicate_state(state);
 
 //It only applies all effects of all transitions to the given state.
 //!!!!Transmitting of values through channels is ignored!!!!
 for (size_int_t i=0; i!=enabled.get_count(); ++i)
   error = error || //Lazy evaluation!!!
     apply_transition_effects(new_state,dynamic_cast<dve_transition_t*>(enabled[i]));
 
 return error;
}

bool dve_explicit_system_t::get_async_enabled_trans_succ
     (const state_t state, const enabled_trans_t & enabled,
      state_t & new_state)
{
 bool error = 
    get_async_enabled_trans_succ_without_property(state,enabled,new_state);
 dve_transition_t * const prop_trans = get_property_trans(enabled);
 if (prop_trans) //Lazy evaluation of || !!!
   error = error || apply_transition_effects(new_state,prop_trans);
 return error;
}

bool dve_explicit_system_t::get_async_enabled_trans_succ
     (const state_t state, const enabled_trans_t & enabled,
      state_t & new_state, const state_t property_state)
{
 bool error=get_async_enabled_trans_succ_without_property(state,enabled,new_state);
 dve_transition_t * const prop_trans = get_property_trans(enabled);
 if (prop_trans)
   memcpy((byte_t *)(new_state.ptr)+prop_begin,
          (byte_t *)(property_state.ptr)+prop_begin,
          prop_size);
 return error;
}

bool dve_explicit_system_t::get_async_enabled_trans_succs
     (const state_t state, succ_container_t & succs,
      const enabled_trans_container_t & enabled_trans)
{
 DEBFUNC(cerr << "BEGIN of get_async_enabled_trans_succs" << endl;)
 succs.clear();
 
 //successors of enabled transitions:
 bool error=false;
 state_t new_state;
 DEB(cerr << "Creating " << enabled_trans.get_property_succ_count())
 DEB(     << " successors with property." << endl;)
 
 
 if (!property_has_synchronization)
  {
   //Optimization: precomputation of successors created by transitions of
   //property process. Created states are stored to the list of successors,
   //because of reflexiveness of trasition relation. Later parts of these
   //states are used to create a product of system without property process
   //with property process
   for (size_int_t i=0; i!=enabled_trans.get_property_succ_count(); i++)
    {
     bool trans_err =
         get_async_enabled_trans_succ(state,enabled_trans[i],new_state);
     error = error || trans_err;
     succs.push_back(new_state);
    }
   
   DEB(cerr << "Creating " << (enabled_trans.size()-enabled_trans.get_property_succ_count()))
   DEB(     << " successors." << endl;)
   //decide, whether to make a product with property process or not
   if ((!error) && (get_with_property()))
     //make a product using precomputed successors 
     for (size_int_t i=enabled_trans.get_property_succ_count(), j=0;
          i!=enabled_trans.size(); i++,j++)
      {
       //get_async_enabled_trans_succ can use precomputed succs[j] to create
       //a new state that is a combination of a state got without property
       //and a state succs[j] (got using only property process).
       bool trans_err =
          get_async_enabled_trans_succ(state,enabled_trans[i],new_state,
                                       succs[j]);
       error = error || trans_err;
       succs.push_back(new_state);
      }
   else //we cannot use precomputed successors for creating a product with
         //property - they are either erroneous or they is no property process
      for (size_int_t i=enabled_trans.get_property_succ_count();
          i!=enabled_trans.size(); i++)
       {
        bool trans_err =
           get_async_enabled_trans_succ(state,enabled_trans[i],new_state);
        error = error || trans_err;
        succs.push_back(new_state);
       }
  }
 else
  {//if the property has a synchronization in its syntax, then successors has
   //to be generated from enabled transitions in a simpler (and potentially
   //slightly slower) way.
   for (size_int_t i=0; i!=enabled_trans.size(); i++)
    {
     bool trans_err =
         get_async_enabled_trans_succ(state,enabled_trans[i],new_state);
     error = error || trans_err;
     succs.push_back(new_state);
    }
  }
  
  
 DEB(if (succs.size()!=enabled_trans.size()) terr << "Unexpected error: ")
 DEB(  << "succs.size()!=enabled_trans->size()" << thr();)
 DEB(if (succs.size()>max_succ_count))
 DEB(  terr<<"Error in estimate of count of asynchronous successors" << thr();)
 DEBFUNC(cerr << "END of get_async_enabled_trans_succs" << endl;)
 return error;
}

bool dve_explicit_system_t::get_sync_enabled_trans_succs
     (const state_t state, succ_container_t & succs,
      const enabled_trans_container_t & enabled_trans)
{
 bool error = false;
 for (size_int_t i=enabled_trans.get_property_succ_count();
      i!=enabled_trans.size(); i++)
  {
   state_t new_state;
   error = error || //Lazy evaluation!!!
     get_sync_enabled_trans_succ(state,enabled_trans[i],new_state);
   succs.push_back(new_state);
  }
 return error;
}


bool dve_explicit_system_t::not_in_glob_conflict(size_int_t * trans_indexes,
                                             size_int_t * bounds)
{
 bit_string_t aux_string(glob_count);
 aux_string.clear();
 for (size_int_t i=0; i!=processes.size(); ++i)
  {
   if (bounds[i])
    {
     dve_transition_t * trans =
       dynamic_cast<dve_transition_t*>
         ((*p_enabled_trans->get_enabled_transition(i,trans_indexes[i]))[0]);
     const bit_string_t & str = trans->get_glob_mask();
     if (!(str & aux_string))
      {
       DEB(cerr<<"not_in_glob_conflict returned false."<<endl;)
       return false;
      }
     aux_string.add(str);
     DEB(cerr << "accumulated global usage:" << endl; aux_string.DBG_print();)
    }
  }
 return true;
}

/* This method works in a following way:
 * It creates a state (synchronnous successor) using successors created by
 * single asynchronous transition.
 *
 * This method expect to have a lists of sucessors created by single
 * processes stored sequentially in `succs' (first successors created
 * using transitions of 1st process, then 2nd process, 3rd process, ...)
 * bound[i] = number of successors in `succs' created by transitions of
 *            i-th process
 *
 * For i-th process this methods takes trans_indexes[i]-th successor
 * created by transition of this process and copies a part influenced
 * by this transition to the new state.
 *
 * After the method does it for all processes, we get the state, that
 * is the same like we simply aplied corresponding transition of
 * each processes to `initial_state'.
 *
 * This approach is faster, because we do not need to compute all
 * effects every time, but we compute them once and get resulting
 * successors by copying of relevant parts of computed states
 * together.
 */ 
state_t dve_explicit_system_t::create_state_by_composition
    (state_t initial_state, enabled_trans_t * const p_enabled,
     succ_container_t & succs, size_int_t * trans_indexes,
     size_int_t * bounds)
{
 state_t result = duplicate_state(initial_state);
 size_int_t first_state = 0; //index (to succs) of first state generated
                              //by transition of i-th process
 //pointer to the global part of the state:
 byte_t * global_result_begin = (byte_t *)(result.ptr)
                                  + global_position.begin;
 if (p_enabled) p_enabled->set_count(processes.size());
 for (size_int_t i=0; i!=processes.size(); ++i)
  {
   process_position_t & fpos = process_positions[i];
   state_t state = succs[first_state + trans_indexes[i]];
   //copy the part influenced by i-th process excluding global variables
   memcpy((byte_t *)(result.ptr)+fpos.begin,
          (byte_t *)(state.ptr)+fpos.begin,
          fpos.size);
   dve_transition_t * const trans = //taking a transition of process
     dynamic_cast<dve_transition_t*>
       ((*p_enabled_trans->get_enabled_transition(i,trans_indexes[i]))[0]);
   //(receiving and property parts are guarantied to be empty due to
   // implementation of compute_successors_without_sync(), which created
   // p_enabled_trans)

   //storing a part of enabled transitions
   if (p_enabled) (*p_enabled)[i] = trans;
   
   //if there are any global variables:
   if (glob_count)
     //look on every byte of global variables and project a new state
     //according a mask stored in glob_filters
     for (size_int_t j=0; j!=global_position.size; ++j)
       if (((byte_t *)glob_filters[trans->get_gid()])[j])
         global_result_begin[j] =
            ((byte_t *)(state.ptr) + global_position.begin)[j];
   first_state += bounds[i]; //skip to states generated by transitions
                             //of next process
  }
 return result;
}
                                      

int dve_explicit_system_t::get_sync_succs_internal
     (state_t state, succ_container_t & succs,
      enabled_trans_container_t * const etc)
{
 DEB(cerr << "BEGIN of get_sync_succs" << endl;)
 int result = SUCC_NORMAL;
 etc->clear();
 //we use aux_enabled_trans and aux_succ_container and thus it is not necessary
 //to allocate the memory for these containers in every call
 p_enabled_trans = aux_enabled_trans;
 p_enabled_trans->clear(); //clear() does not deallocate memory
 //there can be at most as many absolutelly asynchronous successors
 //as the count of all transitions in all processes
 //Therefore aux_succ_container is sufficiently large.
 succ_container_t & no_sync_succs = *aux_succ_container;
 no_sync_succs.clear(); //clear() does not deallocate memory
 succs.clear(); //clear() does not deallocate memory
 size_int_t process_index;
 
 size_int_t current[processes.size()], //vector of indexes of successors
             bound[processes.size()];   //bounds for items of current
 size_int_t previous_size=0;
 
 /* first we get the successors of all transitions of
  * every process (synchronization is ignored) and store them to no_sync_succs*/
 for (process_index = 0; process_index!=processes.size();
     ++process_index)
  {
   current[process_index] = 0;
   if (compute_successors_without_sync(process_index, no_sync_succs, state))
     result |= SUCC_ERROR;
   p_enabled_trans->set_next_begin(process_index,p_enabled_trans->size());
   DEB(cerr << "no_sync_succs.size() = " << no_sync_succs.size() << endl;)
   DEB(cerr << "p_enabled_trans->size() = " << p_enabled_trans->size() << endl;)
   bound[process_index] = no_sync_succs.size()-previous_size;
   if (previous_size==no_sync_succs.size())
    {//process with ID `process_index' has no enabled transtions
     //so we delete the states
     succ_container_t::iterator end = no_sync_succs.end();
     for (succ_container_t::iterator i=no_sync_succs.begin(); i!=end; ++i)
       delete_state(*i);
     
     //and return that system is in deadlock
     result |= SUCC_DEADLOCK;
     return result;
    }
   else
     previous_size = no_sync_succs.size();
  }
 
 size_int_t last_bound = bound[processes.size()-1]; //Optimization-value stored
 DEB(cerr << "no_sync_succs.size() == " << no_sync_succs.size() << endl;)
 DEB(cerr << "bound=|";)
 DEB(for (size_int_t i=0;i!=processes.size();++i))
 DEB(  cerr << bound[i] << '|';)
 DEB(cerr << endl;)
 
 if (no_sync_succs.size()) //if we are not in a deadlock
  {
   /* second we combine these successors together (we make a product of
    * all processes) */
   //We iterate throught all vectors current, where
   //current[i]<bound[i] for all i. We begin with current = {0,0,0,0,...}
   while (current[processes.size()-1]!=last_bound)
    {
     DEB(cerr << "current=|";)
     DEB(for (size_int_t i=0;i!=processes.size();++i))
     DEB(  cerr << current[i] << '|';)
     DEB(cerr << endl;)
     
     enabled_trans_t * p_enabled;
     if (etc) { etc->extend(1); p_enabled=&etc->back(); }
     else p_enabled = 0;
     //compose states selected by a vector current
     succs.push_back(
      create_state_by_composition(state,p_enabled,no_sync_succs,current,bound));
     
     if (!not_in_glob_conflict(current,bound))
      {//conflicting transitions lead to error state
       DEB(cerr << "In conflict!!!" << endl;)
       go_to_error(succs.back());
       result |= SUCC_ERROR;
      }
       
     /* we have to increase values in `current' - we do it in lexicographical
      * manner: */
     size_int_t inc_index = 0;
     for (; inc_index!=(processes.size()-1) &&
              (current[inc_index]+1)>=bound[inc_index];
           ++inc_index)
       { current[inc_index] = 0; }
     ++current[inc_index]; //we increase the value on the first place where
                           //it is possible
    }
  }
 
 succ_container_t::iterator end = no_sync_succs.end();
 for (succ_container_t::iterator i=no_sync_succs.begin(); i!=end; ++i)
   delete_state(*i);
 
 DEB(cerr << "END of get_sync_succs" << endl;)
 return result;
}

bool dve_explicit_system_t::apply_effect(const state_t state, 
                                        const dve_expression_t * const effect)
{
 DEBFUNC(cerr << "BEGIN of apply_effect" << endl;)
 const dve_expression_t & assign_to = *effect->left();
 const dve_expression_t & assign_what = *effect->right();
 bool eval_err=false;
 
 all_values_t value = eval_expr(&assign_what,state,eval_err);
 DEB(cerr << "go_through: value = " << value << endl;)
 
 size_int_t array_index = 0;
 if (assign_to.get_operator() == T_SQUARE_BRACKETS)
  {
    array_index = eval_expr(assign_to.left(),state,eval_err);
    DEB(cerr << "Assignment to array with array_index = " << array_index << endl;)
  }
 
 //if it is not an array or array index doens't break the bounds
 size_int_t array_size = array_sizes[assign_to.get_ident_gid()];
 if (array_size==0 ||
     (array_index>=0 && array_index<array_size))
   eval_err = eval_err || //Lazy evalution !!!
             set_state_creator_value(state,assign_to.get_ident_gid(),value,array_index);
 else
   eval_err = true;
 DEBFUNC(cerr << "END of apply_effect returning "<<(eval_err?"TRUE":"FALSE"))
 DEBFUNC(     << endl;)
 return eval_err;
}

bool dve_explicit_system_t::apply_transition_effects
  (const state_t state,
   const dve_transition_t * const trans)
{
 bool trans_err = false;
 set_to_state_pos<dve_state_int_t>
    (state,state_positions_proc[trans->get_process_gid()],
     trans->get_state2_lid());
 size_int_t effect_count = trans->get_effect_count();
 for (size_int_t i=0; i!=effect_count; i++)
   trans_err = trans_err || apply_effect(state, trans->get_effect(i));//Lazy!!!
 return trans_err;
}

void dve_explicit_system_t::go_to_error(state_t state)
{
 DEB(cerr << "go_to_error called" << endl;)
 
 clear_state(state);
 set_to_state_pos<dve_state_int_t>(state,
     state_positions_proc[processes[0]->get_gid()],//at least 1 process exists
     processes[0]->get_state_count());
}

state_t dve_explicit_system_t::create_error_state()
{
 DEB(cerr << "create_error_state called" << endl;)
 
 state_t state = new_state(get_space_sum());
 set_to_state_pos<dve_state_int_t>(state,
     state_positions_proc[0],  //at least 1 process exists
     processes[0]->get_state_count());
  return state;
}

bool dve_explicit_system_t::compute_enabled_of_property(const state_t state)
{
 DEB(cerr << "BEGIN of compute_enabled_of_property" << endl;)
 dve_state_int_t state1 =
   state_pos_to<dve_state_int_t>(state,state_positions_proc[pproperty->get_gid()]);
 bool error=false;
 //only non-synchronized transitions (no trantition in property should be
 //synchronized)
 size_int_t count = pproperty->get_trans_count(SYNC_NO_SYNC);
 
 for (size_int_t i=0; i!=count; ++i)
  {
   bool trans_err = false;
   dve_transition_t * const t = pproperty->get_transition(SYNC_NO_SYNC,i);
   if (passed_through(state,t,state1,trans_err))
     { property_trans.extend(1); property_trans.back().assign(t,trans_err); }
   error = error || trans_err;
  }
 return error;
 DEB(cerr << "END of compute_enabled_of_property" << endl;)
}

bool dve_explicit_system_t::compute_successors_without_sync
  (const size_int_t process_number,
   succ_container_t & succs, const state_t state)
{
 DEBFUNC(cerr << "BEGIN of compute_successors_without_sync" << endl;)
 bool trans_err = false;
 bool exists_err = false;
 dve_process_t * const process = processes[process_number];

 dve_state_int_t state1 =
   state_pos_to<dve_state_int_t>(state,state_positions_proc[process->get_gid()]);
 state_t new_state;
 for (int mode = 0; mode<=2; ++mode)
  {
   size_int_t count = process->get_trans_count(sync_mode_t(mode));
   for (size_int_t i=0; i!=count; ++i)
    {
     trans_err = false;
     dve_transition_t * const t = process->get_transition(sync_mode_t(mode),i);
     if (passed_through(state,t,state1,trans_err))
      {
       new_state = duplicate_state(state);
       trans_err = trans_err || apply_transition_effects(new_state,t);//Lazy!!!
       if (trans_err) { go_to_error(new_state); exists_err = true; }
       p_enabled_trans->extend(1);
       p_enabled_trans->back().set_count(1);
       p_enabled_trans->back().set_erroneous(trans_err);
       p_enabled_trans->back()[0] = t;
       succs.push_back(new_state);
      }
    }
  }
 return exists_err;
 DEBFUNC(cerr << "END of compute_successors_without_sync" << endl;)
}

bool dve_explicit_system_t::compute_enabled_stage1
  (const size_int_t process_number,
   channels_t * channels, const state_t state, const bool only_commited)
{
 bool trans_err;
 bool exists_err = false;
 //searching enabled synchronized transitions
 const dve_process_t * const process = processes[process_number];
 size_int_t process_state_pos =
    state_positions_proc[process->get_gid()];
 dve_state_int_t state1 = state_pos_to<dve_state_int_t>(state,process_state_pos);
 
 DEB(cerr << "BEGIN of compute_enabled_stage1" <<endl;)
 if (!only_commited || process->get_commited(state1))
  {//run only if `state' is a normal state or
   //`state1' is commited state
   size_int_t count = process->get_trans_count(SYNC_ASK);
   for (size_int_t i=0; i!=count; ++i)
    {
     DEB(cerr << "cycle: asking transition to channel ";)
     /* alias of i-th transition */
     trans_err = false;
     const dve_transition_t * const t = process->get_transition(SYNC_ASK,i);
     if (passed_through(state,t,state1,trans_err))
      {
       channels_t & channel_item = channels[t->get_channel_gid()];
       channel_item.push_back(proc_and_trans_t(process_number,i));
       channel_item.error = channel_item.error || trans_err;
      }
     exists_err = exists_err || trans_err;
    }
  }

 DEB(cerr << "END of compute_enabled_stage1" <<endl;)
 return exists_err;
}

void dve_explicit_system_t::append_new_enabled(dve_transition_t * const t_answ,
                                          dve_transition_t * const t_ask,
                                          const bool trans_err)
{
 if (get_with_property())
  {
   size_int_t first = p_enabled_trans->size();
   p_enabled_trans->extend(property_trans.size());
   enabled_trans_container_t::iterator iter=p_enabled_trans->begin() + first;
   for (size_int_t i=0;
        i!=property_trans.size(); i++, iter++) {
     if (t_ask)
      {
       iter->set_count(3);
       (*iter)[1] = t_ask;
       (*iter)[2]= property_trans[i].trans;
      }
     else { iter->set_count(2); (*iter)[1]= property_trans[i].trans; }
     iter->set_erroneous(trans_err||property_trans[i].error);
     (*iter)[0]=t_answ;
   }
  }
 else
  {
   p_enabled_trans->extend(1);
   if (t_ask)
    {
     p_enabled_trans->back().set_count(2);
     p_enabled_trans->back()[1] = t_ask;
    }
   else p_enabled_trans->back().set_count(1);
   p_enabled_trans->back()[0] = t_answ;
   p_enabled_trans->back().set_erroneous(trans_err);
  }
}

void dve_explicit_system_t::append_new_enabled_prop_sync
                                         (dve_transition_t * const t_answ,
                                          dve_transition_t * const t_prop,
                                          const bool trans_err)
{
 p_enabled_trans->extend(1);
 enabled_trans_t & last_enabled_trans = (*p_enabled_trans).back();
 last_enabled_trans.set_count(2);
 last_enabled_trans[0] = t_answ;
 last_enabled_trans[1] = t_prop;
 last_enabled_trans.set_erroneous(trans_err);
}

bool dve_explicit_system_t::compute_enabled_stage2
  (const size_int_t process_number,
   channels_t * channels, const state_t state, const bool only_commited)
{
 DEB(cerr << "BEGIN of compute_enabled_stage2" << endl;)
 dve_process_t * const process = processes[process_number];
 
 dve_state_int_t state1 =
    state_pos_to<dve_state_int_t>(state,state_positions_proc[process->get_gid()]);
 bool exists_err = false;
 if (!only_commited || process->get_commited(state1))
  {
   //non-synchronized transitions
   bool trans_err;
   
   size_int_t count = process->get_trans_count(SYNC_NO_SYNC);
   for (size_int_t i=0; i!=count; ++i)
    {
     trans_err = false;
     dve_transition_t * const trans = process->get_transition(SYNC_NO_SYNC,i);
     if (passed_through(state,trans,state1,trans_err))
      {//transition is enabled => adding to p_enabled_trans
       exists_err = exists_err || trans_err;
       append_new_enabled(trans,0,trans_err);
      }
    }
   //asynchronous receives:
   count = process->get_trans_count(SYNC_ASK_BUFFER);
   for (size_int_t i=0; i!=count; ++i)
    {
     trans_err = false;
     dve_transition_t * const trans =process->get_transition(SYNC_ASK_BUFFER,i);
     if (passed_through(state,trans,state1,trans_err) &&
         !channel_is_empty(state,trans->get_channel_gid()))
      {//transition is enabled => adding to p_enabled_trans
       exists_err = exists_err || trans_err;
       append_new_enabled(trans,0,trans_err);
      }
    }
   //asynchronous sends:
   count = process->get_trans_count(SYNC_EXCLAIM_BUFFER);
   for (size_int_t i=0; i!=count; ++i)
    {
     trans_err = false;
     dve_transition_t * const trans =
       process->get_transition(SYNC_EXCLAIM_BUFFER,i);
     if (passed_through(state,trans,state1,trans_err) &&
         !channel_is_full(state,trans->get_channel_gid()))
      {//transition is enabled => adding to p_enabled_trans
       exists_err = exists_err || trans_err;
       append_new_enabled(trans,0,trans_err);
      }
    }
   
  
   //synchonized transitions
   DEB(cerr << "SECOND PART of compute_enabled_stage2" << endl;)
   count = process->get_trans_count(SYNC_EXCLAIM);
   for (size_int_t i=0; i!=count; ++i)
    {
     dve_transition_t * const t_answ = process->get_transition(SYNC_EXCLAIM,i);
     channels_t & channel_info = channels[t_answ->get_channel_gid()];
     trans_err = channel_info.get_error();
     if (passed_through(state,t_answ,state1,trans_err))
      {
       for (size_int_t j = 0; j!=channel_info.count; ++j)
        {
         const proc_and_trans_t & pat = channel_info.list[j];
         if (pat.proc != process_number)
          {
           dve_transition_t * const t_ask =
              processes[pat.proc]->get_transition(SYNC_ASK,pat.trans);
           
           if (get_with_property() && get_property_gid()==pat.proc)
             //PROPERTY WITH SYNCHRONIZATION:
             append_new_enabled_prop_sync(t_answ, t_ask, trans_err);
           else
            {
             if (!not_in_glob_conflict(t_answ,t_ask))
              {
               trans_err = true;
               DEB(cerr << "Non-disjunct processes: ")
               DEB(<< process_number << " & " << pat.proc << endl;)
              }
             exists_err = exists_err || trans_err;
             append_new_enabled(t_answ,t_ask,trans_err);
            }

          }
        }
      }
  
    }
  }
 DEBFUNC(cerr << "END of compute_enabled_stage2" << endl;)
 return exists_err;
}

dve_transition_t * dve_explicit_system_t::get_sending_or_normal_trans(
                                      const system_trans_t & sys_trans) const
{
 return dynamic_cast<dve_transition_t*>(sys_trans[0]);
}

dve_transition_t * dve_explicit_system_t::get_receiving_trans(
                                      const system_trans_t & sys_trans) const
{
 size_int_t limit = 2;
 if (get_with_property()) ++limit; //has property => has 2 transitions even
                                   //when no synchronization is present
 if (sys_trans.get_count()>=limit)
   return dynamic_cast<dve_transition_t*>(sys_trans[1]);
 else return 0;
}

dve_transition_t * dve_explicit_system_t::get_property_trans(
                                      const system_trans_t & sys_trans) const
{
 //the transition of property is the last one in enabled transition
 if (get_with_property())
   return dynamic_cast<dve_transition_t*>(sys_trans[sys_trans.get_count()-1]);
 else return 0;
}

void dve_explicit_system_t::DBG_print_state(state_t state, std::ostream & outs,
                         const ulong_int_t format)
         //defautly outs = std::cerr
{
 if (is_erroneous(state)) outs << "(ERR)";
 else
  {
   bool global_env = true;
   size_int_t process_nbr = 0;
   for (size_int_t i = 0; i!=state_creators_count; ++i)
   {
    state_creator_t & state_creator = state_creators[i];
    size_int_t state_pos;
    if (state_creator.type == state_creator_t::VARIABLE)
     {//state creator is a variable
      state_pos = state_positions_var[state_creator.gid];
      if (i) outs << ", ";
      else outs << '[';
      if ((format & ES_FMT_PRINT_VAR_NAMES) == ES_FMT_PRINT_VAR_NAMES)
        outs << psymbol_table->get_variable(state_creator.gid)->get_name()<<':'; 
      if (state_creator.array_size) 
       { //symbol is vector
        outs << "{";
        if (state_creator.var_type == VAR_BYTE)
          for (size_int_t j=0; j!=state_creator.array_size; ++j)
           { 
            outs << all_values_t(state_pos_to_byte(state,state_pos+j));
            if (j!=(state_creator.array_size-1)) outs << '|'; 
           }
        else //state_creator.var_type == VAR_INT
          for (size_int_t j=0; j!=state_creator.array_size; ++j)
           {
            outs << all_values_t(state_pos_to_int(state,state_pos+j*2));
            if (j!=(state_creator.array_size-1)) outs << '|';
           }
        outs << "}";
       }
      else //symbol is scalar
       {
        if (state_creator.var_type == VAR_BYTE)
          outs << all_values_t(state_pos_to_byte(state,state_pos));
        else //state_creator.var_type == VAR_INT
          outs << all_values_t(state_pos_to_int(state,state_pos));
       }
     }   
    else if (state_creator.type == state_creator_t::PROCESS_STATE)
     {// it's in a state of process
      state_pos = state_positions_proc[state_creator.gid];
      if (i) outs << ']';
      else outs << "[]";
      if (global_env) global_env = false; else process_nbr++;
      if ((format & ES_FMT_DIVIDE_PROCESSES_BY_CR) ==
                                                ES_FMT_DIVIDE_PROCESSES_BY_CR)
        outs << '\n';
      else outs << "; ";
      if ((format & ES_FMT_PRINT_PROCESS_NAMES) == ES_FMT_PRINT_PROCESS_NAMES)
        outs << psymbol_table->get_process(
            processes[process_nbr]->get_gid())->get_name() << ':';
      outs << '[';
      size_int_t state_nbr = state_pos_to<dve_state_int_t>(state,state_pos);
      if ((format & ES_FMT_PRINT_STATE_NAMES) == ES_FMT_PRINT_STATE_NAMES)
        outs << psymbol_table->get_state(
           processes[process_nbr]->get_state_gid(state_nbr))->get_name();
      else
        outs << state_pos_to<dve_state_int_t>(state,state_pos);
     }
    else if (state_creator.type == state_creator_t::CHANNEL_BUFFER)
     {
      if (i) outs << ", ";
      else outs << '[';
      if ((format & ES_FMT_PRINT_VAR_NAMES) == ES_FMT_PRINT_VAR_NAMES)
        outs << psymbol_table->get_channel(state_creator.gid)->get_name()<<':'; 
      size_int_t count = channel_content_count(state,state_creator.gid);
      if (count)
       {
        outs << "{";
        size_int_t item_count =
          psymbol_table->get_channel(state_creator.gid)->get_channel_type_list_size();
        for (size_int_t j=0; j!=count; ++j)
         {
          if (j) outs << "|";
          if (item_count==1)
            outs << read_from_channel(state,state_creator.gid,0,j);
          else
           {
            for (size_int_t k=0; k!=item_count; ++k)
             {
              if (k) outs << ",";
              else outs << "(";
              outs << read_from_channel(state,state_creator.gid,k,j);
             }
            outs << ")";
           }
         }
        outs << "}";
       }
      else outs << "EMPTY";
     }
   }
   outs << ']';
  }
 if ((format & ES_FMT_DIVIDE_PROCESSES_BY_CR) == ES_FMT_DIVIDE_PROCESSES_BY_CR)
   outs << endl;
}

enabled_trans_t * dve_explicit_system_t::new_enabled_trans() const
{
 return (new dve_enabled_trans_t);
}

bool dve_explicit_system_t::violates_assertion(const state_t state) const
{
 bool eval_err;
 for (size_int_t i=0; i!=process_count; ++i)
  {
   size_int_t process_state =
     size_int_t(state_pos_to<dve_state_int_t>(state,state_positions_proc[i]));
   size_int_t assert_count = processes[i]->get_assertion_count(process_state);
   for (size_int_t j=0; j!=assert_count; ++j)
     if(!eval_expr(processes[i]->get_assertion(process_state,j),state,eval_err))
       return true;
  }
 return false;
}

size_int_t dve_explicit_system_t::violated_assertion_count(
                                                    const state_t state) const
{
 bool eval_err;
 size_int_t count = 0;
 for (size_int_t i=0; i!=process_count; ++i)
  {
   size_int_t process_state =
     size_int_t(state_pos_to<dve_state_int_t>(state,state_positions_proc[i]));
   size_int_t assert_count = processes[i]->get_assertion_count(process_state);
   for (size_int_t j=0; j!=assert_count; ++j)
     if(!eval_expr(processes[i]->get_assertion(process_state,j),state,eval_err))
       ++count;
  }
 return count;
}

std::string dve_explicit_system_t::violated_assertion_string(
                                      const state_t state,
                                      const size_int_t index) const
{
 bool eval_err;
 size_int_t count = 0;
 for (size_int_t i=0; i!=process_count; ++i)
  {
   size_int_t process_state =
     size_int_t(state_pos_to<dve_state_int_t>(state,state_positions_proc[i]));
   size_int_t assert_count = processes[i]->get_assertion_count(process_state);
   for (size_int_t j=0; j!=assert_count; ++j)
     if(!eval_expr(processes[i]->get_assertion(process_state,j),state,eval_err))
      {
       if (count==index)
        {
         std::ostringstream ostr;
         dve_expression_t * assertion = processes[i]->get_assertion(process_state,j);
         ostr << assertion->get_source_first_line() << ':'
              << assertion->get_source_first_col() << '-'
              << assertion->get_source_last_line() << ':'
              << assertion->get_source_last_col() << "  ";
         assertion->write(ostr);
         return ostr.str();
        }
       ++count;
      }
  }

 return std::string("");;
}

