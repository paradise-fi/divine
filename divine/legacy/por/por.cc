#ifdef DO_DEB
 #define DEB(x)
#else
 #define DEB(x)
#endif
#include <iostream>
#include <stack>
#include <math.h>
#include "por/por.hh"
#include "system/dve/dve_system.hh"
#include "por/spor.hh"

using namespace std;
using namespace divine;

const int debug=0;

void clear_array(bool *array, std::size_t length)
{ 
  for (std:: size_t i=0; i<length; i++)
    array[i]=false;
}

void fulfill_array(bool *array, std::size_t length)
{ 
  for (std:: size_t i=0; i<length; i++)
    array[i]=true;
}

bit_string_t por_t::get_visibility()
{
  return trans_is_visible;
}

void por_t::set_visibility(bit_string_t trans)
{
  if (trans_is_visible.get_bit_count()==trans.get_bit_count())
    {
      trans_is_visible=trans;
    }
  else
    {
      cerr << "por_t: changing visibility by differently large bit_string_t" 
	   << endl;
    }
}

void por_t::get_pre(bit_string_t* result)
{
  if (pre[0].get_bit_count()==result[0].get_bit_count())
    {
      for (std::size_t trans_gid=0; trans_gid<trans_count; trans_gid++)
	result[trans_gid]=pre[trans_gid];
    }
  else
    {
      cerr << "por_t::get_pre: argument with differently large bit_string_t" 
	   << endl;
    }
}

void por_t::set_pre(bit_string_t* new_pre)
{
  if (pre[0].get_bit_count()==new_pre[0].get_bit_count())
    {
      for (std::size_t trans_gid=0; trans_gid<trans_count; trans_gid++)
	pre[trans_gid]=new_pre[trans_gid];
    }
  else
    {
      cerr << "por_t: changing \"pre\" by differently large bit_string_t" 
	   << endl;
    }
}

void por_t::get_dep(bit_string_t* result)
{
  if (dep[0].get_bit_count()==result[0].get_bit_count())
    {
      for (std::size_t trans_gid=0; trans_gid<trans_count; trans_gid++)
	result[trans_gid]=dep[trans_gid];
    }
  else
    {
      cerr << "por_t::get_dep: argument with differently large bit_string_t" 
	   << endl;
    }
}

void por_t::set_dep(bit_string_t* new_dep)
{
  if (dep[0].get_bit_count()==new_dep[0].get_bit_count())
    {
      for (std::size_t trans_gid=0; trans_gid<trans_count; trans_gid++)
	dep[trans_gid]=new_dep[trans_gid];
    }
  else
    {
      cerr << "por_t: changing \"dep\" by differently large bit_string_t" 
	   << endl;
    }
}

void por_t::find_visible_symbols_from_ba(bool *glob_vars, bool *loc_states)
{ 
  prop_process=System->get_property_process();
  for (std::size_t trans_id=0; trans_id<prop_process->get_trans_count(); trans_id++)
    { 
      const dve_transition_t *trans =
        dynamic_cast<const dve_transition_t*>
          (prop_process->get_transition(trans_id));
      const dve_expression_t *expr = trans->get_guard();
      parse_guard_expr(expr, glob_vars, loc_states);
    }
}

void por_t::find_visible_symbols_from_expr(list<expression_t*>* vis_list,
					   bool *glob_vars, bool *loc_states)
{ 
  if (vis_list)
    for (list<expression_t*>::const_iterator i = vis_list->begin(); 
	 i != vis_list->end(); ++i)
      {
	const dve_expression_t *expr = dynamic_cast<const dve_expression_t*>(*i);
	parse_guard_expr(expr, glob_vars, loc_states);
      }
}


void por_t::parse_effect_expr(const expression_t *expr0, bool *glob_vars)
{ 
  const dve_expression_t * dve_expr0 =
    dynamic_cast<const dve_expression_t*>(expr0);
  if (dve_expr0)
    { 
      int op = dve_expr0->get_operator();
      switch (op)
	{
	case T_ID: 
	  { 
	    std::size_t gid=dve_expr0->get_ident_gid();
	    if ((!(Table->get_variable(gid)->is_const()))&& //it isn't constant
		(Table->get_variable(gid)->get_process_gid())==NO_ID) //it is global variable
	      { 
		glob_vars[index_of_glob_var[gid]]=true;
	      }
	    break;
	  }
	case T_SQUARE_BRACKETS:
	  {
	    std::size_t gid=dve_expr0->get_ident_gid();
	    if ((!(Table->get_variable(gid)->is_const()))&& //it isn't constant
		(Table->get_variable(gid)->get_process_gid())==NO_ID) //it is global variable
	      { 
		const dve_expression_t* index_of_field=dve_expr0->subexpr(0);
		if (index_of_field->get_operator()==T_NAT) //index is constant => fulfill only one item of glob_vars
		  {
		    glob_vars[index_of_glob_var[gid]+index_of_field->get_value()]=true;
		  }
		else //index can have different value => fulfill all items of glob_vars used for this vector-variable
		  {
		    std::size_t size=Table->get_variable(gid)->get_vector_size();
		    for (std::size_t i=0; i<size; i++)
		      glob_vars[index_of_glob_var[gid]+i]=true;
		  }
	      }
	    break;
	  }
	}
    }
}


void por_t::parse_guard_expr(const expression_t *expr0, bool *glob_vars, bool *loc_states)
{ 
  std::stack<const dve_expression_t *> estack;
  const dve_expression_t *expr;
  int op;
  estack.push(dynamic_cast<const dve_expression_t*>(expr0));
  while (!(estack.empty()))
    { 
      expr=estack.top();
      estack.pop();
      if (expr)
	{
	  op = expr->get_operator();
	  switch (op)
	    { 
	    case T_ID: //global/local variable or constant
	      { 
		if ((!(Table->get_variable(expr->get_ident_gid())->is_const()))&&
		    (Table->get_variable(expr->get_ident_gid())->get_process_gid())==NO_ID)
		  { 
		    glob_vars[index_of_glob_var[expr->get_ident_gid()]]=true;
		  }
		break;
	      }
	    case T_SQUARE_BRACKETS: //index to the field
	      { 
		if ((!(Table->get_variable(expr->get_ident_gid())->is_const()))&& //isn't constant?
		    (Table->get_variable(expr->get_ident_gid())->get_process_gid())==NO_ID) //global array?
		  { 		    
		    const dve_expression_t* index_of_field=expr->subexpr(0);
		    if (index_of_field->get_operator()==T_NAT) //index is constant => fulfill only one item of glob_vars
		      {
			glob_vars[index_of_glob_var[expr->get_ident_gid()]+index_of_field->get_value()]=true;
		      }
		    else //index can have different value => fulfill all items of glob_vars used for this vector-variable
		      {
			std::size_t size=Table->get_variable(expr->get_ident_gid())->get_vector_size();
			for (std::size_t i=0; i<size; i++)
			  glob_vars[index_of_glob_var[expr->get_ident_gid()]+i]=true;
		      }
		  }
		estack.push(expr->subexpr(0));
		break;
	      }
	    case T_DOT:
	      { 
		loc_states[expr->get_ident_gid()]=true;	    
		break;
	      }
	    default:
	      { 
		for (std::size_t i=0; i<expr->arity(); i++)
		  { 
		    const dve_expression_t *subexpr = expr->subexpr(i);
		    estack.push(subexpr);
		  }
		break;
	      }
	    }
	}
    }
}

//                       trans1_gid                   trans1_gid             trans2_gid
bool por_t::set_pre(bool *trans_scan_loc_states, bool *trans_scan_vars, bool *trans_change_vars, const transition_t *trans1, const transition_t *trans2)
{ 
  const dve_transition_t
     *dve_trans1 = dynamic_cast<const dve_transition_t*>(trans1),
     *dve_trans2 = dynamic_cast<const dve_transition_t*>(trans2);
  //transition with guard "Consumer.ready" is dependent on trans. of process "Consumer" leading to or from local state "ready"
  bool is_pre=
    (((trans_scan_loc_states[dve_trans2->get_state1_gid()])||
      (trans_scan_loc_states[dve_trans2->get_state2_gid()]))&&
     (dve_trans2->get_state1_gid()!=dve_trans2->get_state2_gid())); //does this transition really change its local state?

  //can be dve_trans1 and dve_trans2 synchronized by channel?
  if ((((dve_trans1->is_sync_exclaim())&&(dve_trans2->is_sync_ask()))||
       ((dve_trans1->is_sync_ask())&&(dve_trans2->is_sync_exclaim())))&&
      (dve_trans1->get_channel_gid()==dve_trans2->get_channel_gid()))
    is_pre=true;

  //can dve_trans2 make dve_trans1 enable?
  for (std::size_t i=0; ((i<size_of_glob_vars)&&(!is_pre)); i++)
    is_pre=((trans_scan_vars[i])&&(trans_change_vars[i]));
  return is_pre;
}


void por_t::init(explicit_system_t *S, list<expression_t*>* vis_list)
{ 
    count_approx_interuptions = false; // XXX
  System=dynamic_cast<dve_explicit_system_t*>(S);
  Table=System->get_symbol_table();
  proc_count=System->get_process_count(); //property process included here :-(
  trans_count=System->get_trans_count(); //included nr. of property process's transitions :-(
  size_of_glob_vars=0;

  process_of_trans = new std::size_t[trans_count];
  state1_of_trans = new size_int_t[trans_count];
  for (std::size_t i=0; i<trans_count; i++)
    { 
      const dve_transition_t *trans=
	dynamic_cast<dve_transition_t*>(System->get_transition(i));
      process_of_trans[i]=trans->get_process_gid();
      state1_of_trans[i]=trans->get_state1_lid();
    }

  std::size_t glob_vars_count = System->get_global_variable_count();
  index_of_glob_var=new std::size_t[glob_vars_count];
  for (std::size_t i=0; i<glob_vars_count; i++)
    {
      index_of_glob_var[i]=size_of_glob_vars;
      dve_symbol_t* var=Table->get_variable(i);
      if (var->is_vector()==true)
	size_of_glob_vars+=var->get_vector_size();
      else
	size_of_glob_vars++;
    }
  
  bool visible_glob_vars[size_of_glob_vars]; //is global variable with that lid visible?
  bool visible_loc_states[Table->get_state_count()]; //is the local state with that gid in some guard of prop.process (or in some "visible expression")?
  bool trans_scan_vars[trans_count][size_of_glob_vars]; //trans_scan_vars[i][j]=true iff transition with UID 'i' reads global variable with UID 'j'
  bool trans_change_vars[trans_count][size_of_glob_vars]; //trans_change_vars[i][j]=true iff transition with UID 'i' changes global variable with UID 'j'
  bool trans_scan_loc_states[trans_count][Table->get_state_count()]; //trans_scan_loc_states[i][j]=true iff transition with UID 'i' has in guard "Sender.ready" with UID 'j'
  
  clear_array(visible_glob_vars,size_of_glob_vars);
  clear_array(visible_loc_states,Table->get_state_count());

  //finding visible symbols: variables and local states ("Producer.ready")
  if (System->get_with_property()) //extract them from the property process
    {
      prop_process=System->get_property_process();
      find_visible_symbols_from_ba(visible_glob_vars, visible_loc_states);
    }
  else //extract them from the list of expressions
    {
      prop_process=NULL;
      find_visible_symbols_from_expr(vis_list, visible_glob_vars,
				     visible_loc_states);
    }
    
  //creating records, fulfilling trans_* fields and finding visible transitions
  dep=new bit_string_t[trans_count];
  pre=new bit_string_t[trans_count];
  initialized = true;
  trans_is_visible.alloc_mem(trans_count);
  for (std::size_t i=0; i<trans_count; i++)
    {
      dep[i].alloc_mem(proc_count);
      pre[i].alloc_mem(proc_count);
      dep[i].clear();
      pre[i].clear();
    }
  
  //cerr << "Creating records and fulfilling trans_* fields" << endl;
  for (std::size_t proc_gid=0; proc_gid<proc_count; proc_gid++) 
    {
      process_t *proc = System->get_process(proc_gid);
      if (proc!=prop_process)
	for (std::size_t trans_nr=0; trans_nr<proc->get_trans_count(); trans_nr++)
	  { 
	    const dve_transition_t *trans =
              dynamic_cast<dve_transition_t*>(proc->get_transition(trans_nr));
	    std::size_t trans_gid=trans->get_gid();
	    if (debug)
	      { 
		cerr << proc_gid << '.' << trans_gid << ": ";
		cerr << endl;
	      }
	    clear_array(trans_scan_vars[trans_gid],size_of_glob_vars);
	    trans_scan_loc_states[trans_gid][0]=false;
	    clear_array(trans_scan_loc_states[trans_gid],Table->get_state_count());
	    clear_array(trans_change_vars[trans_gid],size_of_glob_vars);
	    //processing guard of the transition
	    const dve_expression_t *expr=trans->get_guard();
	    parse_guard_expr(expr, trans_scan_vars[trans_gid], trans_scan_loc_states[trans_gid]);
	    
	    //processing synchronization of the transition
	    switch (trans->get_sync_mode())
	      { 
	      case (SYNC_EXCLAIM):
		{ 
		  for (size_int_t i=0; i<trans->get_sync_expr_list_size(); i++)
		    parse_guard_expr(trans->get_sync_expr_list_item(i), trans_scan_vars[trans_gid], trans_scan_loc_states[trans_gid]);
		  break;
		}
	      case (SYNC_ASK):
		{ 
		  for (size_int_t i=0; i<trans->get_sync_expr_list_size(); i++)
		    parse_effect_expr(trans->get_sync_expr_list_item(i), trans_change_vars[trans_gid]);
		  break;
		}
	      case (SYNC_EXCLAIM_BUFFER):
		{
		  for (size_int_t i=0; i<trans->get_sync_expr_list_size(); i++)
		    parse_guard_expr(trans->get_sync_expr_list_item(i), trans_scan_vars[trans_gid], trans_scan_loc_states[trans_gid]);
		  trans_scan_vars[trans_gid][trans->get_channel_gid()]=true; //I'm scanning nonfulness of a buffer before the execution of the transition (but this line is not probably necessary)
		  trans_change_vars[trans_gid][trans->get_channel_gid()]=true; //the value of the buffer is changed by execution of this transition
		  break;
		}
	      case (SYNC_ASK_BUFFER):
		{
		  for (size_int_t i=0; i<trans->get_sync_expr_list_size(); i++)
		    parse_effect_expr(trans->get_sync_expr_list_item(i), trans_change_vars[trans_gid]);
		  trans_scan_vars[trans_gid][trans->get_channel_gid()]=true; //I'm scanning nonfulness of a buffer before the execution of the transition (but this line is not probably necessary)
		  trans_change_vars[trans_gid][trans->get_channel_gid()]=true; //the value of the buffer is changed by execution of this transition
		  break;
		}
	      case (SYNC_NO_SYNC): { }
	      }
	    
	    //processing effects of the transition
	    for (std::size_t i=0; i<trans->get_effect_count(); i++)
	      { 
		expr = trans->get_effect(i);
		parse_effect_expr(expr->left(), trans_change_vars[trans_gid]);
		parse_guard_expr(expr->right(), trans_scan_vars[trans_gid], trans_scan_loc_states[trans_gid]);
	      }
	    
	    //find out whether the transition is or isn't visible
	    bool visible=(((visible_loc_states[trans->get_state1_gid()])||
			   (visible_loc_states[trans->get_state2_gid()]))&&
			  (trans->get_state1_gid()!=trans->get_state2_gid())); //does this transition really change its local state?
	    for (std::size_t i=0; ((i<size_of_glob_vars)&&(!visible)); i++)
	      { 
		visible=((trans_change_vars[trans_gid][i])&&(visible_glob_vars[i]));
	      }
	    trans_is_visible.set_bit(trans_gid,visible);
	  }
    }
  
  if (debug)
    { 
      cerr << "Visible transitions:" << endl;
      for (std::size_t i=0; i<trans_count; i++)
	if (trans_is_visible.get_bit(i))
	  { 
	    const dve_transition_t *trans=
              dynamic_cast<dve_transition_t*>(System->get_transition(i));
	    cerr << Table->get_process(trans->get_process_gid())->get_name() << '.' << trans->get_lid() << ": ";
	    cerr << endl;
	  }
      cerr << endl << "Computing \"pre\" and \"dep\" sets" << endl;
    }
  
  //finding transition dependencies
  for (std::size_t proc1_gid=0; proc1_gid<proc_count; proc1_gid++)
    { 
      process_t *proc1 = System->get_process(proc1_gid);
      if (proc1!=prop_process)
	for (std::size_t trans1_nr=0; trans1_nr<proc1->get_trans_count(); trans1_nr++)
	  { 
	    const transition_t *trans1=proc1->get_transition(trans1_nr);
	    std::size_t trans1_gid = trans1->get_gid();
	    //testing dependency to "higher" transitions
	    for (std::size_t proc2_gid=proc1_gid+1; proc2_gid<proc_count; proc2_gid++)
	      { 
		process_t *proc2 = System->get_process(proc2_gid);
		if (proc2!=prop_process)
		  for (std::size_t trans2_nr=0; trans2_nr<proc2->get_trans_count(); trans2_nr++)
		    { 
		      const transition_t *trans2=proc2->get_transition(trans2_nr);
		      std::size_t trans2_gid = trans2->get_gid();
		      //estimating pre[trans1_gid][proc2_gid] and pre[trans2_gid][proc1_gid]
		      bool is_pre1=set_pre(trans_scan_loc_states[trans1_gid],
					   trans_scan_vars[trans1_gid],
					   trans_change_vars[trans2_gid],
					   trans1, trans2);
		      bool is_pre2=set_pre(trans_scan_loc_states[trans2_gid],
					   trans_scan_vars[trans2_gid],
					   trans_change_vars[trans1_gid],
					   trans2, trans1);
		      if (is_pre1)
			pre[trans1_gid].enable_bit(proc2_gid);
		      if (is_pre2)
			pre[trans2_gid].enable_bit(proc1_gid);
		      
		      //estimating dep[trans1_gid][proc2_gid]
		      bool is_dep=((is_pre1)||(is_pre2));
		      for (std::size_t i=0; ((i<size_of_glob_vars)&&(!is_dep)); i++)
			is_dep=((trans_change_vars[trans1_gid][i])&&(trans_change_vars[trans2_gid][i]));
		      if (is_dep)
			{ 
			  dep[trans1_gid].enable_bit(proc2_gid);
			  dep[trans2_gid].enable_bit(proc1_gid);
			}
		      
		      if (debug)
			{ 
			  if (is_pre1)
			    cerr << "pre(" << proc1_gid << '.' << trans1_gid << "): "
				 << "inserting "  << proc2_gid << '.' << trans2_gid << endl;
			  if (is_pre2)
			    cerr << "pre(" << proc2_gid << '.' << trans2_gid << "): "
				 << "inserting " << proc1_gid << '.' << trans1_gid << endl;
			  if (is_dep)
			    cerr << "dep: " << proc1_gid << '.' << trans1_gid << " @ "
				 << proc2_gid << '.' << trans2_gid << endl;
			}
		    }
	      }
	  }
    }
  
  if (debug)
    {      
      for (std::size_t proc_gid=0; proc_gid<proc_count; proc_gid++)
	{ 
	  process_t *proc = System->get_process(proc_gid);
	  for (std::size_t trans_nr=0; trans_nr<proc->get_trans_count(); trans_nr++)
	    { 
	      const transition_t *trans=proc->get_transition(trans_nr);
	      std::size_t trans_gid = trans->get_gid();
	      cerr << proc_gid << '.' << trans_gid << ": \n";
	      cerr << "  dep: ";
	      dep[trans_gid].DBG_print();
	      cerr << "  pre: ";
	      pre[trans_gid].DBG_print();
	    }
	}
    }
}

por_t::~por_t()
{ 
  if (initialized) 
    {
      delete[] pre;
      delete[] dep;
      delete[] index_of_glob_var;
    }
}

void por_t::static_c3(void)
{
  spor_t spor(System);
  std::set<size_t> sticky_trans;
  for (size_int_t i=0; i<System->get_trans_count(); i++)
    if (trans_is_visible.get_bit(i))
      sticky_trans.insert(i);
  if (debug)
    cerr << "Computing static C3...";
  spor.get_sticky_transitions(sticky_trans);
  if (debug)
    cerr << " done." << endl;
  for(std::set<size_t>::iterator iter = sticky_trans.begin(); iter != sticky_trans.end(); iter++)
    trans_is_visible.enable_bit(*iter);
}

int por_t::generate_composed_ample_sets(state_t s, bool *ample_sets, enabled_trans_container_t **ample_trans, enabled_trans_container_t &all_enabled_trans)
{ 
  //we presuppose that ample_sets and enabled_trans has a good dimension
  bool trans_is_enabled[trans_count];
  int succ_result;
  clear_array(trans_is_enabled, trans_count);
  clear_array(ample_sets, proc_count);

  //finding "passive_trans" and checking C0 condition (nonemptiness): only processes with some enabled transition may be taken as ample
  try
    {
      succ_result = System->get_enabled_trans(s, all_enabled_trans);
    }
  catch (ERR_throw_t)
    {
      for (ERR_nbr_t i = 0; i!=gerr.count(); i++)
	{ 
	  gerr.perror("Successor error",i); 
	}
      gerr.clear();
      return SUCC_ERROR;
    }
  if (debug)
    {
      cerr << endl << "----------------------" << endl;
      System->print_state(s, cout);
      for (unsigned i=0; i<s.size; i++)
	cout << (int)s.ptr[i];
      cout << endl;
    }

  bool Dep[proc_count][proc_count], Pre[proc_count][proc_count]; //Dep[i] stands for dep(T_i(s)), Pre[i] stands for pre(current_i(s) \setminus T_i(s))
  for (size_t proc_gid=0; proc_gid<proc_count; proc_gid++)
    {
      clear_array(Dep[proc_gid], proc_count);
      clear_array(Pre[proc_gid], proc_count);
      Dep[proc_gid][proc_gid]=true;
      Pre[proc_gid][proc_gid]=true;
    }

  for (std::size_t i=0; i!=all_enabled_trans.size(); i++)
    {
      dve_transition_t * const trans = System->get_sending_or_normal_trans(all_enabled_trans[i]);
      if (!System->get_with_property() ||
	  trans->get_process_gid()!=System->get_property_gid())
	{
	  trans_is_enabled[trans->get_gid()]=true;
	  //	  cout << "trans. " << trans->get_process_gid() << '.' << trans->get_gid() << " is enabled" << endl;
	  ample_sets[trans->get_process_gid()]=true;
	  dve_transition_t * const recv_trans = System->get_receiving_trans(all_enabled_trans[i]);
	  if (recv_trans)
	    {
	      trans_is_enabled[recv_trans->get_gid()]=true;
	      //	      cout << "trans. " << recv_trans->get_process_gid() << '.' << recv_trans->get_gid() << " is enabled" << endl;
	      Dep[trans->get_process_gid()][recv_trans->get_process_gid()]=true;
	      Dep[recv_trans->get_process_gid()][trans->get_process_gid()]=true;
	      //	      ample_sets[recv_trans->get_process_gid()]=true; -- line may NOT be uncommented, otherwise it may cause problems when a process has enabled transitions only of the type " ... sync ch? ..." (it would result in an ample set with zero transitions in ample_trans[some_gid])
	    }
	}
    }
  if (debug)
    { 
      cerr << "C0 holds for processes: ";
      for (std::size_t i=0; i<proc_count; i++)
	if (ample_sets[i]) cerr << i << " ";
      cerr << endl;
    }

  //checking C1 and C2

  //finding dependencies between different processes
  for (std::size_t trans_gid=0; trans_gid<trans_count; trans_gid++)
  { 
    if (trans_is_enabled[trans_gid]) 
      { //the transition should be invisible; all in dep(trans) copy to Dep[process_of_trans[trans_gid]]
	//condition C2: transition must be invisible
	ample_sets[process_of_trans[trans_gid]]&=(!(trans_is_visible.get_bit(trans_gid))); //condition C2
	if (debug)
	  if (trans_is_visible.get_bit(trans_gid))
	    cerr << "Trans. " << process_of_trans[trans_gid] << '.' << trans_gid << " is visible.\n";
	//condition C1: part for all_enabled_trans: dep("trans") \subseteq "proc"
	for (std::size_t proc2_gid=0; (proc2_gid<proc_count); proc2_gid++)
	  { 
	    Dep[process_of_trans[trans_gid]][proc2_gid]|=dep[trans_gid].get_bit(proc2_gid); //Dep is symemtric!!!
	    Dep[proc2_gid][process_of_trans[trans_gid]]|=dep[trans_gid].get_bit(proc2_gid); //therefore this line has to be here, too
	    if (debug)
	      if (dep[trans_gid].get_bit(proc2_gid)==true)
		cerr << process_of_trans[trans_gid] << '.' << trans_gid << " dep. with process " << proc2_gid << endl;
	  }
      }
    else //transition isn't enabled; if it is passive (in good local state, but unsatisfied guard or sync), test for condition C1: "pre"-set
      if (state1_of_trans[trans_gid]==
	  System->get_state_of_process(s, process_of_trans[trans_gid]))
	{ 
	  for (std::size_t proc2_gid=0; (proc2_gid<proc_count); proc2_gid++)
	    { 
	      Pre[process_of_trans[trans_gid]][proc2_gid]|=pre[trans_gid].get_bit(proc2_gid);
	      if (debug)
		if (pre[trans_gid].get_bit(proc2_gid)==true)
		  cerr << process_of_trans[trans_gid] << '.' << trans_gid << " pre- with process " << proc2_gid << endl;
	    }
	}
  }

  //ample_sets[prop_process->get_gid()]=false; //it shouldn't be necessary, if so, then the test for prop_process!=NULL has to be
  if ((debug)&&(prop_process))
    cerr << "Process " << prop_process->get_gid() << " is property.\n";
    
  //transitive closure of Dep and Pre
  float iters=log((float)proc_count)/log(2.0)+1; //number of iterations necessary to perform optimized ?Warshall? algorithm
  std::size_t i,j,k;

  if (debug)
    {
      cout << "Dep:" << endl;
      for (i=0; i<proc_count; i++)
        {
          for (j=0; j<proc_count; j++)
            cout << Dep[i][j];
          cout << endl;
        }
      cout << "Pre:" << endl;
      for (i=0; i<proc_count; i++)
        {
          for (j=0; j<proc_count; j++)
            cout << Pre[i][j];
          cout << endl;
        }
    }

  for (std::size_t iter=0; iter<=iters; iter++)
    for (i=0; i<proc_count; i++)
      for (j=0; j<proc_count; j++)
	for (k=0; k<proc_count; k++)
	  {
	    Dep[i][j]|=Dep[i][k]&&Dep[k][j];
	    Pre[i][j]|=Pre[i][k]&&Pre[k][j];
	  }

  if (debug)
    {
      cout << "Dep:" << endl;
      for (i=0; i<proc_count; i++)
	{
	  for (j=0; j<proc_count; j++)
	    cout << Dep[i][j];
	  cout << endl;
	}
      cout << "Pre:" << endl;
      for (i=0; i<proc_count; i++)
	{
	  for (j=0; j<proc_count; j++)
	    cout << Pre[i][j];
	  cout << endl;
	}
    }

  //fulfilling and ample_trans (only if ample_sets==true)
  for (std::size_t proc_gid=0; proc_gid<proc_count; proc_gid++)
    {
      ample_trans[proc_gid]->clear();
      if ((!(prop_process))||(proc_gid!=prop_process->get_gid()))
	{
	  if (ample_sets[proc_gid]) //fulfill ample_trans[proc_gid]
	    { 
	      k=0;
	      for (std::size_t proc_gid2=0; proc_gid2<proc_count; proc_gid2++)
		{
		  if ((Dep[proc_gid][proc_gid2]==true)||(Pre[proc_gid][proc_gid2]==true))
		    { 
		      j=all_enabled_trans.get_count(proc_gid2);
		      k+=j;
		      ample_trans[proc_gid]->set_next_begin(proc_gid2,k);
		      for (i=0; i<j; i++)
			{
			  ample_trans[proc_gid]->extend(1);
                          ample_trans[proc_gid]->back() =
                            (*(all_enabled_trans.get_enabled_transition(
                                                                 proc_gid2,i)));
			}
		    }
		  else ample_trans[proc_gid]->set_next_begin(proc_gid2,k);
		}
	      if (prop_process)
		ample_trans[proc_gid]->set_property_succ_count(all_enabled_trans.get_property_succ_count());
	    }
	}
    }

  if (count_approx_interuptions)
    {
      unsigned best_ample, proc_active_trans_count, best_ample_size = MAX_ULONG_INT;
      for (size_t proc_gid=0; proc_gid<proc_count; proc_gid++)
	{
	  proc_active_trans_count = ample_trans[proc_gid]->size();
	  if ((ample_sets[proc_gid])&&(proc_active_trans_count<all_enabled_trans.size())&&
	      (best_ample_size>proc_active_trans_count))
	    {
	      best_ample_size = proc_active_trans_count;
	      best_ample = proc_gid;
	      //	      cout << "better ample of proc.: " << proc_gid << " of size: " << best_ample_size << endl;
	    }
	}
      if (best_ample_size < MAX_ULONG_INT)
	{
	  dve_enabled_trans_t tr;
	  bool process_in_ample[proc_count];
	  for (unsigned i=0;i<proc_count;i++)
	    process_in_ample[i]=false;
	  for (unsigned i=0; i<ample_trans[best_ample]->size(); i++)
	    {
	      tr=(*ample_trans[best_ample])[i];
	      dve_transition_t * const trans = System->get_sending_or_normal_trans(tr);
	      if (!System->get_with_property() || trans->get_process_gid()!=System->get_property_gid())
		{
		  process_in_ample[trans->get_process_gid()]=true;
		  dve_transition_t * const recv_trans = System->get_receiving_trans(tr);
		  if (recv_trans)
		    process_in_ample[recv_trans->get_process_gid()]=true;
		}
	    }
	  /*	  cout << "best_ample_size of proc.: " << best_ample << "; procs. in ample: ";
	  for (unsigned q=0;q<proc_count; q++)
	    if (process_in_ample[q]) cout << q << ' ';
	    cout << endl;*/
	  for (std::size_t trans_gid=0; trans_gid<trans_count; trans_gid++)
	    if (process_in_ample[(dynamic_cast<const dve_transition_t*>(System->get_transition(trans_gid)))->get_process_gid()])
	      {
		if (trans_is_enabled[trans_gid]) 
		  { //the transition should be invisible; all in dep(trans) copy to Dep[process_of_trans[trans_gid]]
		    //condition C2: transition must be invisible
		    if ((full_vis.get_bit(trans_gid))&&(!trans_is_visible.get_bit(trans_gid))) //must be invisible but it isn't set as invisible
		      {
			vis_interupted[trans_gid]++;
			//			cout << "trans. " << trans_gid << " interupted visible" << endl;
		      }

		    //condition C1: part for all_enabled_trans: dep("trans") \subseteq "proc"
		    for (unsigned proc2_gid=0; proc2_gid<proc_count; proc2_gid++)
		      {
			if ((!process_in_ample[proc2_gid])&&(full_dep[trans_gid].get_bit(proc2_gid))&&(!dep[trans_gid].get_bit(proc2_gid)))
			  {
			    dep_interupted[trans_gid*proc_count+proc2_gid]++;
			    //			    cout << "trans. " << trans_gid << " interupted DEP on proc. " << proc2_gid << endl;
			  }
		      }

		  }
		else //transition isn't enabled; if it is passive (in good local state, but unsatisfied guard or sync), test for condition C1: "pre"-set
		  if (state1_of_trans[trans_gid]==
		      System->get_state_of_process(s, process_of_trans[trans_gid]))
		    { 
		      for (unsigned proc2_gid=0; proc2_gid<proc_count; proc2_gid++)
                      {
			if ((!process_in_ample[proc2_gid])&&(full_pre[trans_gid].get_bit(proc2_gid))&&(!pre[trans_gid].get_bit(proc2_gid)))
			  {
			    pre_interupted[trans_gid*proc_count+proc2_gid]++;
			    //			    cout << "trans. " << trans_gid << " interupted PRE on proc. " << proc2_gid << endl;
			  }
                      }
		    }
	      }
	}
      /*      else
	{
	  cout << "full expansion" << endl;
	  }*/
    }

  return succ_result;
}


int por_t::generate_ample_sets(state_t s, bool *ample_sets, enabled_trans_container_t &enabled_trans, std::size_t &ample_proc_gid)
{ 
  bool trans_is_enabled[trans_count];
  int succ_result;
  clear_array(trans_is_enabled, trans_count);
  clear_array(ample_sets, proc_count);

  //finding "passive_trans" and checking C0 condition (nonemptiness): only processes with some enabled transition may be taken as ample
  try
  {
    succ_result = System->get_enabled_trans(s, enabled_trans);
  }
  catch (ERR_throw_t)
  {
    for (ERR_nbr_t i = 0; i!=gerr.count(); i++)
      { 
	gerr.perror("Successor error",i);
      }
    gerr.clear();
    return SUCC_ERROR;
  }
  if (debug)
    cerr << "----------------------" << endl << endl;
  for (std::size_t i=0; i!=enabled_trans.size(); i++)
    {
      dve_transition_t * const trans = System->get_sending_or_normal_trans(enabled_trans[i]);
      if (!System->get_with_property() ||
	  trans->get_process_gid()!=System->get_property_gid())
	{
	  trans_is_enabled[trans->get_gid()]=true;
	  ample_sets[trans->get_process_gid()]=true;
	  dve_transition_t * const recv_trans = System->get_receiving_trans(enabled_trans[i]);
	  if (recv_trans)
	    {
	      trans_is_enabled[recv_trans->get_gid()]=true;
	      //	      Dep[trans->get_process_gid()][recv_trans->get_process_gid()]=true;
	      //	      Dep[recv_trans->get_process_gid()][trans->get_process_gid()]=true;
	    }
	}
    }
  if (debug)
  { 
    cerr << "C0 holds for processes: ";
    for (std::size_t i=0; i<proc_count; i++)
      if (ample_sets[i]) cerr << i << " ";
    cerr << endl;
  }

  //checking C1 and C2
  for (std::size_t trans_gid=0; trans_gid<trans_count; trans_gid++)
  { 
    if (trans_is_enabled[trans_gid]) //hence dep(trans) must be free and trans is invisible
      { 
	//	cout << "transition " << trans_gid << " is enabled." << endl;
	//condition C2: transition must be invisible
	ample_sets[process_of_trans[trans_gid]]&=(!(trans_is_visible.get_bit(trans_gid))); //condition C2
	if (debug)
	  if (trans_is_visible.get_bit(trans_gid))
	    cerr << "Trans. " << process_of_trans[trans_gid] << '.' << trans_gid << " is visible.\n";
	
	//condition C1: part for enabled_trans: dep("trans") \subseteq "proc"
	for (std::size_t proc2_gid=0; ((proc2_gid<proc_count)&&(ample_sets[process_of_trans[trans_gid]])); proc2_gid++)
	  { 
	    ample_sets[process_of_trans[trans_gid]]=(!(dep[trans_gid].get_bit(proc2_gid)));
	    if (debug)
	      if (!ample_sets[process_of_trans[trans_gid]])
		cerr << process_of_trans[trans_gid] << '.' << trans_gid << " dep. with process " << proc2_gid << endl;
	  }
      }
    else //transition isn't enabled; if it is passive (in good local state, but unsatisfied guard or sync), test for condition C1: "pre"-set
      if (state1_of_trans[trans_gid]==
	  System->get_state_of_process(s, process_of_trans[trans_gid]))
	{ 
	  for (std::size_t proc2_gid=0; ((proc2_gid<proc_count)&&(ample_sets[process_of_trans[trans_gid]])); proc2_gid++)
	    { 
	      ample_sets[process_of_trans[trans_gid]]=(!(pre[trans_gid].get_bit(proc2_gid)));
	      if (debug)
		if (!ample_sets[process_of_trans[trans_gid]])
		  cerr << process_of_trans[trans_gid] << '.' << trans_gid << " pre- with process " << proc2_gid << endl;
	    }
	}
  }

  //ample_sets[prop_process->get_gid()]=false; //it shouldn't be necessary, if so, then the test for prop_process!=NULL has to be
  if ((debug)&&(prop_process))
    cerr << "Process " << prop_process->get_gid() << " is property.\n";

  if (count_approx_interuptions)
    {
      ample_proc_gid=System->get_process_count();
      std::size_t min_succs=MAX_SIZE_INT;
      for (std::size_t i=0; i<System->get_process_count(); i++)
	{ 
	  if ((ample_sets[i])&&(min_succs>enabled_trans.get_count(i)))
	    { 
	      min_succs=enabled_trans.get_count(i);
	      ample_proc_gid=i;
	    }
	}
      //      cout << "ample procesu " << (ample_proc_gid<System->get_process_count()?(ample_proc_gid):(999)) << " ma stavu: " << min_succs << endl;

      if (ample_proc_gid<System->get_process_count())
	{
	  dve_enabled_trans_t* tr;
	  bool process_in_ample[proc_count];
	  for (unsigned i=0;i<proc_count;i++)
	    process_in_ample[i]=false;
	  for (unsigned i=0; i<enabled_trans.get_count(ample_proc_gid); i++)
	    {
	      tr=dynamic_cast<dve_enabled_trans_t*>(enabled_trans.get_enabled_transition(ample_proc_gid,i));
	      dve_transition_t * const trans = System->get_sending_or_normal_trans(*tr);
	      if (!System->get_with_property() || trans->get_process_gid()!=System->get_property_gid())
		{
		  process_in_ample[trans->get_process_gid()]=true;
		  //		  dve_transition_t * const recv_trans = System->get_receiving_trans(*tr);
		  //		  if (recv_trans)
		  //		    process_in_ample[recv_trans->get_process_gid()]=true;
		}
	    }
	  /*	  cout << " procsesses in ample: ";
	  for (unsigned q=0;q<proc_count; q++)
	    if (process_in_ample[q]) cout << q << ' ';
	    cout << endl;*/
	  for (std::size_t trans_gid=0; trans_gid<trans_count; trans_gid++)
	    if (process_in_ample[(dynamic_cast<const dve_transition_t*>(System->get_transition(trans_gid)))->get_process_gid()])
	      {
		if (trans_is_enabled[trans_gid]) 
		  { //the transition should be invisible; all in dep(trans) copy to Dep[process_of_trans[trans_gid]]
		    //condition C2: transition must be invisible
		    if ((full_vis.get_bit(trans_gid))&&(!trans_is_visible.get_bit(trans_gid))) //must be invisible but it isn't set as invisible
		      {
			vis_interupted[trans_gid]++;
			//			cout << "trans. " << trans_gid << " interupted VISIBLE" << endl;
		      }

		    //condition C1: part for all_enabled_trans: dep("trans") \subseteq "proc"
		    for (unsigned proc2_gid=0; proc2_gid<proc_count; proc2_gid++)
		      {
			if ((!process_in_ample[proc2_gid])&&(full_dep[trans_gid].get_bit(proc2_gid))&&(!dep[trans_gid].get_bit(proc2_gid)))
			  {
			    dep_interupted[trans_gid*proc_count+proc2_gid]++;
			    //			    cout << "trans. " << trans_gid << " interupted DEP on proc. " << proc2_gid << endl;
			  }
		      }

		  }
		else //transition isn't enabled; if it is passive (in good local state, but unsatisfied guard or sync), test for condition C1: "pre"-set
		  if (state1_of_trans[trans_gid]==
		      System->get_state_of_process(s, process_of_trans[trans_gid]))
		    { 
		      for (unsigned proc2_gid=0; proc2_gid<proc_count; proc2_gid++)
                      {
			if ((!process_in_ample[proc2_gid])&&(full_pre[trans_gid].get_bit(proc2_gid))&&(!pre[trans_gid].get_bit(proc2_gid)))
			  {
			    pre_interupted[trans_gid*proc_count+proc2_gid]++;
			    //			    cout << "trans. " << trans_gid << " interupted PRE on proc. " << proc2_gid << endl;
			  }
                      }
		    }
	      }
	}
    }

  return succ_result;
}

std::size_t por_t::choose_ample_set(bool *ample_sets, enabled_trans_container_t *enabled_trans)
{ 
  std::size_t proc_gid;
  switch (choose_type)
    { 
    case POR_FIRST: 
      { 
	proc_gid=0;
	while ((proc_gid<System->get_process_count())
	       &&(!ample_sets[proc_gid]))
	  proc_gid++;
	break;
      }
    case POR_LAST: 
      { 
	proc_gid=System->get_process_count()-1;
	while ((proc_gid>=0)
	       &&(!ample_sets[proc_gid]))
	  proc_gid--;
	if (proc_gid<0)
	  proc_gid=System->get_process_count();
	break;
      }
    case POR_SMALLEST: 
      { 
	proc_gid=System->get_process_count();
	std::size_t min_succs=MAX_SIZE_INT;
	for (std::size_t i=0; i<System->get_process_count(); i++)
	  { 
	    if ((ample_sets[i])&&(min_succs>enabled_trans->get_count(i)))
	      { 
		min_succs=enabled_trans->get_count(i);
		proc_gid=i;
	      }
	  }
	break;
      }
    default: 
      { 
	proc_gid=System->get_process_count();
      }
    }
  return proc_gid;
}

int por_t::ample_set(state_t s, enabled_trans_container_t &enabled_trans, std::size_t &proc_gid)
{ 
  bool ample_sets[System->get_process_count()];
  int succ_result = generate_ample_sets(s, ample_sets, enabled_trans, proc_gid);
  if (!count_approx_interuptions)
    proc_gid = choose_ample_set(ample_sets, &enabled_trans);
  return succ_result;
}

int por_t::ample_set_succs(state_t s, succ_container_t & succs, std::size_t &proc_gid)
{ 
  bool ample_sets[System->get_process_count()];
  enabled_trans_container_t enabled_trans(*System);
  int succ_result = generate_ample_sets(s, ample_sets, enabled_trans, proc_gid);
  if (!count_approx_interuptions)
    proc_gid = choose_ample_set(ample_sets, &enabled_trans);
  if (proc_gid==System->get_process_count()) //ful expansion
    { 
      System->get_enabled_trans_succs(s, succs, enabled_trans);
    }
  else
    { 
      std::size_t i0=enabled_trans.get_begin(proc_gid);
      std::size_t nr=enabled_trans.get_count(proc_gid);
      state_t succ;
      for (std::size_t i=i0; i<i0+nr; i++)
	{ 
	  System->get_ith_succ(s, i, succ);
	  succs.push_back(succ);
	}
    }
  return succ_result;
}
