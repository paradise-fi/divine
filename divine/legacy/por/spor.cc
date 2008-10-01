#include "spor.hh"

using namespace divine;
using namespace std;

string output_cv[4] = { "CV_INCREASE", "CV_DECREASE", "CV_NOTHING", "CV_ANYTHING" };
string output_cc[7] = { "CC_INCREASE", "CC_DECREASE", "CC_POSITIVE", "CC_NEGATIVE", "CC_ZERO", "CC_GOOD_VAR", "CC_ANYTHING" };

#define debug 0

size_int_t best_trans, cycles=0, covered_cycles=0;

computed_change_t cc_byte_plus[7][7] = { { CC_INCREASE, CC_ANYTHING, CC_INCREASE, CC_ANYTHING, CC_INCREASE, CC_INCREASE, CC_ANYTHING, },
					 { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_DECREASE, CC_DECREASE, CC_ANYTHING, CC_ANYTHING, },
					 { CC_INCREASE, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_INCREASE, CC_ANYTHING, },
					 { CC_ANYTHING, CC_DECREASE, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_DECREASE, CC_ANYTHING, },
					 { CC_INCREASE, CC_DECREASE, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_GOOD_VAR, CC_ANYTHING, },
					 { CC_INCREASE, CC_ANYTHING, CC_INCREASE, CC_DECREASE, CC_GOOD_VAR, CC_INCREASE, CC_ANYTHING, },
					 { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING  }, };

computed_change_t cc_int_plus[7][7] = { { CC_ANYTHING, CC_ANYTHING, CC_INCREASE, CC_ANYTHING, CC_INCREASE, CC_ANYTHING, CC_ANYTHING, },
					{ CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_DECREASE, CC_DECREASE, CC_ANYTHING, CC_ANYTHING, },
					{ CC_INCREASE, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_INCREASE, CC_ANYTHING, },
					{ CC_ANYTHING, CC_DECREASE, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_DECREASE, CC_ANYTHING, },
					{ CC_INCREASE, CC_DECREASE, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_GOOD_VAR, CC_ANYTHING, },
					{ CC_ANYTHING, CC_ANYTHING, CC_INCREASE, CC_DECREASE, CC_GOOD_VAR, CC_ANYTHING, CC_ANYTHING, },
					{ CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING  }, };

computed_change_t cc_byte_mult[7][7] = { { CC_INCREASE, CC_ANYTHING, CC_INCREASE, CC_ANYTHING, CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					 { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					 { CC_INCREASE, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					 { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					 { CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     },
					 { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ANYTHING, CC_ANYTHING  },
					 { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ANYTHING, CC_ANYTHING  }, };

computed_change_t cc_int_mult[7][7] = { { CC_INCREASE, CC_ANYTHING, CC_INCREASE, CC_ANYTHING, CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					{ CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					{ CC_INCREASE, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					{ CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					{ CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     },
					{ CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ANYTHING, CC_ANYTHING  },
					{ CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ANYTHING, CC_ANYTHING  }, };

computed_change_t cc_both_minus[7][7] = { { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_INCREASE, CC_INCREASE, CC_ANYTHING, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_DECREASE, CC_ANYTHING, CC_DECREASE, CC_DECREASE, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_DECREASE, CC_INCREASE, CC_GOOD_VAR, CC_ZERO,     CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING  }, };

computed_change_t cc_byte_div[7][7] = { { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, },
					{ CC_ANYTHING, CC_ANYTHING, CC_DECREASE, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, },
					{ CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					{ CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					{ CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     },
					{ CC_ANYTHING, CC_ANYTHING, CC_DECREASE, CC_ANYTHING, CC_ANYTHING, CC_POSITIVE, CC_ANYTHING, },
					{ CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING  }, };

computed_change_t cc_int_div[7][7] = { { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, },
				       { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, },
				       { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
				       { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
				       { CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     },
				       { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_POSITIVE, CC_ANYTHING, },
				       { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING  }, };

computed_change_t cc_byte_unar_minus[7] = { CC_DECREASE, CC_DECREASE, CC_NEGATIVE, CC_POSITIVE, CC_ZERO, CC_DECREASE, CC_ANYTHING, };

computed_change_t cc_int_unar_minus[7] = { CC_ANYTHING, CC_ANYTHING, CC_NEGATIVE, CC_POSITIVE, CC_ZERO, CC_ANYTHING, CC_ANYTHING, };

computed_change_t cc_byte_lshift[7][7] = { { CC_ANYTHING, CC_ANYTHING, CC_INCREASE, CC_ANYTHING, CC_INCREASE, CC_ANYTHING, CC_ANYTHING, },
					   { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_DECREASE, CC_DECREASE, CC_ANYTHING, CC_ANYTHING, },
					   { CC_POSITIVE, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					   { CC_NEGATIVE, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					   { CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     },
					   { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_POSITIVE, CC_ANYTHING, },
					   { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_GOOD_VAR, CC_ANYTHING, CC_ANYTHING  }, };

computed_change_t cc_int_lshift[7][7] = { { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_INCREASE, CC_ANYTHING, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_DECREASE, CC_ANYTHING, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					  { CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     },
					  { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_POSITIVE, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_GOOD_VAR, CC_ANYTHING, CC_ANYTHING  }, };

computed_change_t cc_byte_rshift[7][7] = { { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_INCREASE, CC_INCREASE, CC_ANYTHING, CC_ANYTHING, },
					   { CC_ANYTHING, CC_ANYTHING, CC_DECREASE, CC_ANYTHING, CC_DECREASE, CC_ANYTHING, CC_ANYTHING, },
					   { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					   { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					   { CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     },
					   { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_POSITIVE, CC_ANYTHING, },
					   { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_GOOD_VAR, CC_ANYTHING, CC_ANYTHING  }, };

computed_change_t cc_int_rshift[7][7] = { { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_INCREASE, CC_ANYTHING, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_DECREASE, CC_ANYTHING, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ANYTHING, CC_ANYTHING, },
					  { CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     CC_ZERO,     },
					  { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_POSITIVE, CC_ANYTHING, },
					  { CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_ANYTHING, CC_GOOD_VAR, CC_ANYTHING, CC_ANYTHING  }, };

change_var_on_cycle_t cv_combine[4][4] = { { CV_INCREASE, CV_ANYTHING, CV_INCREASE, CV_ANYTHING, },
					   { CV_ANYTHING, CV_DECREASE, CV_DECREASE, CV_ANYTHING, },
					   { CV_INCREASE, CV_DECREASE, CV_NOTHING,  CV_ANYTHING, },
					   { CV_ANYTHING, CV_ANYTHING, CV_ANYTHING, CV_ANYTHING, }, };


spor_t::spor_t(dve_explicit_system_t* system)
{
  System = system;
  Table = System->get_symbol_table();
  index_of_var = new size_int_t[Table->get_variable_count()];
  unwrapped_variables=0;
  for (size_int_t i=0; i<Table->get_variable_count(); i++)
    {
      index_of_var[i]=unwrapped_variables;
      dve_symbol_t* var=Table->get_variable(i);
      if (var->is_vector()==true)
	unwrapped_variables+=var->get_vector_size();
      else
	unwrapped_variables++;
    }
  if (debug>1)
    {
      cout << "Transitions: " << System->get_trans_count() << endl
	   << "Unwrapped variables & channels: " << unwrapped_variables+Table->get_channel_count() << endl << endl;
    }
}

spor_t::~spor_t()
{
  delete[] index_of_var;
}

void spor_t::guard_expr_to_vars(const expression_t *expr0, change_var_on_cycle_t* vars)
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
		    vars[index_of_var[expr->get_ident_gid()]]=CV_ANYTHING;
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
			vars[index_of_var[expr->get_ident_gid()]+index_of_field->get_value()]=CV_ANYTHING;
		      }
		    else //index can have different value => fulfill all items of glob_vars used for this vector-variable
		      {
			std::size_t size=Table->get_variable(expr->get_ident_gid())->get_vector_size();
			for (std::size_t i=0; i<size; i++)
			  vars[index_of_var[expr->get_ident_gid()]+i]=CV_ANYTHING;
		      }
		  }
		estack.push(expr->subexpr(0));
		break;
	      }
	    case T_DOT:
	      { 
		//this shouldn't occur
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

bool evaluate_to_nat(const dve_expression_t *expr0, size_int_t &result)
{
  bool left, right;
  size_int_t left_result, right_result;
  switch (expr0->get_operator())
    {
    case T_NAT:
      {
	result=expr0->get_value();
	return true;
	break;
      }
    case T_PLUS:
      {
	left=evaluate_to_nat(expr0->left(), left_result);
	right=evaluate_to_nat(expr0->right(), right_result);
	result=left_result+right_result;
	return (left && right);
	break;
      }
    case T_MINUS:
      {
	left=evaluate_to_nat(expr0->left(), left_result);
	right=evaluate_to_nat(expr0->right(), right_result);
	result=left_result-right_result;
	return (left && right);
	break;
      }
    case T_MULT:
      {
	left=evaluate_to_nat(expr0->left(), left_result);
	right=evaluate_to_nat(expr0->right(), right_result);
	result=left_result*right_result;
	return (left && right);
	break;
      }
    case T_DIV:
      {
	left=evaluate_to_nat(expr0->left(), left_result);
	right=evaluate_to_nat(expr0->right(), right_result);
	result=left_result/right_result;
	return (left && right);
	break;
      }
    case T_MOD:
      {
	left=evaluate_to_nat(expr0->left(), left_result);
	right=evaluate_to_nat(expr0->right(), right_result);
	result=left_result%right_result;
	return (left && right);
	break;
      }
    case T_UNARY_MINUS:
      {
	left=evaluate_to_nat(expr0->left(), left_result);
	result=-left_result;
	return (left);
	break;
      }
    case T_RSHIFT:
      {
	left=evaluate_to_nat(expr0->left(), left_result);
	right=evaluate_to_nat(expr0->right(), right_result);
	result=left_result>>right_result;
	return (left && right);
      }
    case T_LSHIFT:
      {
	left=evaluate_to_nat(expr0->left(), left_result);
	right=evaluate_to_nat(expr0->right(), right_result);
	result=left_result<<right_result;
	return (left && right);
      }
    default:
      return false;
    }
}

inline void combine_value(change_var_on_cycle_t &result, change_var_on_cycle_t change)
{
  result=cv_combine[result][change];
}

bool spor_t::compute_changes(const dve_expression_t *expr_on_left, const dve_expression_t *expr0, computed_change_t &change)
{
  size_int_t nat_result;
  bool result;
  computed_change_t left=CC_ANYTHING, right=CC_ANYTHING;
  change=CC_ANYTHING;
  if (evaluate_to_nat(expr0, nat_result))
    {
      if (nat_result>0) change=CC_POSITIVE;
      if (nat_result==0) change=CC_ZERO;
      if (nat_result<0) change=CC_NEGATIVE;
      return true;
    }
  switch (expr0->get_operator())
    {
    case T_ID:
      {
	if (expr_on_left->to_string()==expr0->to_string()) //A CO KDYZ BUDE pole[3-1]=pole[2]+1 ?
	  {
	    change=CC_GOOD_VAR;
	    return true;
	  }
	else return false;
	break;
      }
    case T_PLUS:
      {
	result=compute_changes(expr_on_left, expr0->left(), left) && compute_changes(expr_on_left, expr0->right(), right);
	if (Table->get_variable(expr_on_left->get_ident_gid())->is_byte())
	  change=cc_byte_plus[left][right];
	else
	  change=cc_int_plus[left][right];
	return result;
	break;
      }
    case T_MINUS:
      {
	result=compute_changes(expr_on_left, expr0->left(), left) && compute_changes(expr_on_left, expr0->right(), right);
	change=cc_both_minus[left][right];
	return result;
	break;
      }
    case T_MULT:
      {
	result=compute_changes(expr_on_left, expr0->left(), left) && compute_changes(expr_on_left, expr0->right(), right);
	if (Table->get_variable(expr_on_left->get_ident_gid())->is_byte())
	  change=cc_byte_mult[left][right];
	else
	  change=cc_int_mult[left][right];
	return result;
	break;
      }
    case T_DIV:
      {
	result=compute_changes(expr_on_left, expr0->left(), left) && compute_changes(expr_on_left, expr0->right(), right);
	if (Table->get_variable(expr_on_left->get_ident_gid())->is_byte())
	  change=cc_byte_div[left][right];
	else
	  change=cc_int_div[left][right];
	return result;
	break;
      }
    case T_UNARY_MINUS:
      {
	result=compute_changes(expr_on_left, expr0->left(), left);
	if (Table->get_variable(expr_on_left->get_ident_gid())->is_byte())
	  change=cc_byte_unar_minus[left];
	else
	  change=cc_int_unar_minus[left];
	return result;
	break;
      }
    case T_RSHIFT:
      {
	result=compute_changes(expr_on_left, expr0->left(), left) && compute_changes(expr_on_left, expr0->right(), right);
	if (Table->get_variable(expr_on_left->get_ident_gid())->is_byte())
	  change=cc_byte_rshift[left][right];
	else
	  change=cc_int_rshift[left][right];
	return result;
	break;
      }
    case T_LSHIFT:
      {
	result=compute_changes(expr_on_left, expr0->left(), left) && compute_changes(expr_on_left, expr0->right(), right);
	if (Table->get_variable(expr_on_left->get_ident_gid())->is_byte())
	  change=cc_byte_lshift[left][right];
	else
	  change=cc_int_lshift[left][right];
	return result;
	break;
      }
    default: { return false; }
    }
}

void spor_t::effect_expr_to_vars(const dve_expression_t *expr0, change_var_on_cycle_t* vars)
{
  const dve_expression_t* left_expr = expr0->left();
  change_var_on_cycle_t change = CV_ANYTHING;
  computed_change_t computed_change = CC_ZERO;
  if (compute_changes(expr0->left(), expr0->right(), computed_change))
    switch (computed_change)
      {
      case CC_INCREASE: { change=CV_INCREASE; break; }
      case CC_DECREASE: { change=CV_DECREASE; break; }
      default: {} //let "change" be set to CV_ANYTHING
      }

  //fulfill corresponding variable(s)
  size_int_t field_position;
  if ((left_expr->get_operator()==T_SQUARE_BRACKETS)&&(evaluate_to_nat(left_expr->right(), field_position))) //one position of a field
    {
      combine_value(vars[index_of_var[left_expr->get_ident_gid()]+field_position], change);
    }
  else //scalar or whole field
    {
      if (Table->get_variable(left_expr->get_ident_gid())->is_vector()) //whole field
	{
	  std::size_t size=Table->get_variable(left_expr->get_ident_gid())->get_vector_size();
	  for (std::size_t i=0; i<size; i++)
	    combine_value(vars[index_of_var[left_expr->get_ident_gid()]+i], change);
	}
      else //scalar
	combine_value(vars[index_of_var[left_expr->get_ident_gid()]], change);
    }
}

void spor_t::collect_local_cycle(size_int_t start_state)
{  
  dve_transition_t *trans;
  if (debug>1)
    cout << "\t Local cycle: ";
  loc_cycle_t loc_cycle;
  loc_cycle.vars_and_chans = new change_var_on_cycle_t[unwrapped_variables+Table->get_channel_count()];
  for (size_int_t i=0; i<unwrapped_variables+Table->get_channel_count(); i++)
    loc_cycle.vars_and_chans[i]=CV_NOTHING;
  loc_cycle.transitions = new size_int_t[stack_size-start_state];
  loc_cycle.cycle_length=stack_size-start_state;
  bool cycle_uncovered=true;
  for (size_int_t i=start_state; i<stack_size; i++)
    {
      trans = dynamic_cast<dve_transition_t*>(act_process->get_transition(stack_of_trans[i]));
      loc_cycle.transitions[i-start_state]=trans->get_gid();
      cycle_uncovered &= (sticky_trans.find(trans->get_gid()) == sticky_trans.end());

      //synchronization
      switch (trans->get_sync_mode())
	{
	case SYNC_EXCLAIM: 
	case SYNC_EXCLAIM_BUFFER: { combine_value(loc_cycle.vars_and_chans[unwrapped_variables+trans->get_channel_gid()], CV_INCREASE); break; }
	case SYNC_ASK: 
	case SYNC_ASK_BUFFER: 
	  { 
	    combine_value(loc_cycle.vars_and_chans[unwrapped_variables+trans->get_channel_gid()], CV_DECREASE);
	    for (size_int_t j=0; j<trans->get_sync_expr_list_size(); j++)
	      guard_expr_to_vars(trans->get_sync_expr_list_item(j), loc_cycle.vars_and_chans);
	    break;
	  }
	case SYNC_NO_SYNC: {} //nothing to do
	}

      //effect
      for (size_int_t j=0; j<trans->get_effect_count(); j++)
	{
	  dve_expression_t *effect = trans->get_effect(j);
	  effect_expr_to_vars(effect, loc_cycle.vars_and_chans);
	}
      if (debug>1)
	cout << trans->get_process_gid() << '.' << trans->get_partial_id() << " ";
    }
  if (cycle_uncovered)
    {
      local_cycles.push_back(loc_cycle);
      if (debug>1)
	{
	  cout << ": " << endl << "\t-- it changes values to: ";
	  for (size_int_t j=0; j<unwrapped_variables+Table->get_channel_count(); j++)
	    cout << output_cv[loc_cycle.vars_and_chans[j]] << ' ';
	  cout << endl << endl;
	}
    }
  else
    {
      if (debug>1) 
	cout << ": -- the cycle is covered by a transition." << endl;
      covered_cycles++;
      delete[] loc_cycle.transitions;
      delete[] loc_cycle.vars_and_chans;
    }
}

void spor_t::dfs(size_int_t state)
{
  state_on_stack[state]=stack_size;
  stack_size++;
  for (size_int_t i=0; i<act_process->get_state_count(); i++)
    {
      std::deque<size_int_t>* tmp_deque = &(process_graph[state*act_process->get_state_count()+i]);
      for (deque<size_int_t>::iterator iter = tmp_deque->begin(); iter != tmp_deque->end(); ++iter) //go through all transitions "state -> i"
      {
	stack_of_trans[stack_size-1]=(*iter);
	if (state_on_stack[i]<MAX_ULONG_INT) //a cycle detected
	  collect_local_cycle(state_on_stack[i]);
	else
	  if (state_on_stack[i]==MAX_ULONG_INT)
	    dfs(i);
      }
    }
  stack_size--;
  state_on_stack[state]=MAX_ULONG_INT;
}

void spor_t::detect_local_cycles()
{
  size_int_t init_state; //LID of initial state of a process
  dve_transition_t *trans;
  for (size_int_t proc_gid=0; proc_gid<System->get_process_count(); proc_gid++)
    {
      act_process = dynamic_cast<dve_process_t*>(System->get_process(proc_gid));
      if ((!System->get_with_property())||(act_process!=System->get_property_process()))
	{
	  stack_of_trans = new size_int_t[act_process->get_state_count()]; //USAGE: NECESSARY TO SUBTRACT 1 FROM stack_size; other usage as normal (0..get_state_count())
	  state_on_stack = new size_int_t[act_process->get_state_count()];
	  for (size_int_t i=0; i<act_process->get_state_count(); i++)
	    state_on_stack[i]=MAX_ULONG_INT;
	  process_graph = new std::deque<size_int_t>[act_process->get_state_count()*act_process->get_state_count()];
	  for (size_int_t i=0; i<act_process->get_trans_count(); i++)
	    {
	      trans=dynamic_cast<dve_transition_t*>(act_process->get_transition(i));
	      process_graph[trans->get_state1_lid()*act_process->get_state_count()+trans->get_state2_lid()].push_back(i); //i=LID of the transition
	    }

	  //DFS
	  stack_size=0;
	  init_state = act_process->get_initial_state();
	  dfs(init_state);
	  
	  for (size_int_t i=0; i<act_process->get_state_count()*act_process->get_state_count(); i++)
	    while (!process_graph[i].empty())
	      process_graph[i].pop_front();
	  delete[] stack_of_trans;
	  delete[] state_on_stack;
	  delete[] process_graph;
	}
    }
  if (debug)
    cout << "\tLocal cycles covered by visible transition: " << covered_cycles << endl;
  remove_irelevant_local_cycles();
  if (debug)
    cout << "\tNumber of relevant local cycles: " << local_cycles.size() << endl;
}

void spor_t::complete_to_global_cycle(list<std::list<loc_cycle_t>::iterator> *candidate, change_var_on_cycle_t *vars_and_chans)
{
  //try whether the candidate is complete global cycle
  change_var_on_cycle_t result[unwrapped_variables+Table->get_channel_count()];
  size_int_t diff=0;
  while ((diff<unwrapped_variables+Table->get_channel_count())&&((vars_and_chans[diff]==CV_NOTHING)||(vars_and_chans[diff]==CV_ANYTHING)))
    diff++;
  if (diff==unwrapped_variables+Table->get_channel_count()) //candidate _is_ global cycle
    {
      glob_cycle_t glob_cycle;
      set<size_int_t> trans_in_cycle;
      for (list<std::list<loc_cycle_t>::iterator>::iterator iter = candidate->begin(); iter != candidate->end(); ++iter)
	for (size_int_t i=0; i<(*(*iter)).cycle_length; i++)
	  //	  if (trans_in_cycle.find((*iter).transitions[i] == trans_in_cycle.end()))
	  trans_in_cycle.insert((*(*iter)).transitions[i]);
      size_int_t pos=0;
      glob_cycle.cycles_length=trans_in_cycle.size();
      glob_cycle.transitions = new size_int_t[glob_cycle.cycles_length];      
      if (debug>1)
	cout << "\tGlobal cycle: ";
      for (set<size_int_t>::iterator iter = trans_in_cycle.begin(); iter != trans_in_cycle.end(); ++iter)
	{
	  glob_cycle.transitions[pos++]=(*iter);
	  if (debug>1)
	    {
	      dve_transition_t* trans = dynamic_cast<dve_transition_t*>(System->get_transition(*iter));
	      cout << trans->get_process_gid() << '.' << trans->get_partial_id() << " ";
	    }
	}
      global_cycles.push_back(glob_cycle);
      cycles++;
      if (((cycles%100000)==0)&&(debug))
	cout << "\t\t Global cycles: " << cycles << endl;
      if (debug>1)
	cout << endl;
    }
  else //candidate isn't global cycle; some local cycle is necessary to add in order to be able to complete global cycle
    {
      std::list<loc_cycle_t>::iterator iter=candidate->front();
      iter++;
      change_var_on_cycle_t val;
      while (iter != local_cycles.end())
	{
	  val=cv_combine[(*iter).vars_and_chans[diff]][vars_and_chans[diff]];
	  if ((val==CV_NOTHING)||(val==CV_ANYTHING))
	    {
	      for (size_int_t i=0; i<unwrapped_variables+Table->get_channel_count(); i++)
		{
		  result[i]=vars_and_chans[i];
		  combine_value(result[i],(*iter).vars_and_chans[i]);
		}
	      candidate->push_back(iter);
	      complete_to_global_cycle(candidate, result);
	      candidate->pop_back();
	    }
	  iter++;
	}
    }
}

void spor_t::remove_irelevant_local_cycles(void) //those cycles which cann't lie on a global cycle (e.g. the if a variable is increased only by the cycle
{                                                // and other cycles don't change the variable  
  size_int_t deleted_loc_cycles=0;
  change_var_on_cycle_t changed_vars_and_chans[unwrapped_variables+Table->get_channel_count()];
  bool cycle_deleted=false;
  do
    {
      cycle_deleted=false;
      for (size_int_t i=0; i<unwrapped_variables+Table->get_channel_count(); i++)
	changed_vars_and_chans[i]=CV_NOTHING;
      for (std::list<loc_cycle_t>::iterator iter = local_cycles.begin(); iter != local_cycles.end(); ++iter)
	{
	  for (size_int_t i=0; i<unwrapped_variables+Table->get_channel_count(); i++)
	    combine_value(changed_vars_and_chans[i],(*iter).vars_and_chans[i]);
	}
      for (size_int_t i=0; i<unwrapped_variables+Table->get_channel_count(); i++)
	if ((changed_vars_and_chans[i]==CV_INCREASE)||(changed_vars_and_chans[i]==CV_DECREASE)) //remove all local cycles which increase/decrease this variable or channel
	  {
	    list<loc_cycle_t>::iterator tmp_iter, iter=local_cycles.begin();
	    while (iter != local_cycles.end())
	      {
		tmp_iter=iter;
		iter++;
		if (((*tmp_iter).vars_and_chans[i]==CV_INCREASE)||((*tmp_iter).vars_and_chans[i]==CV_DECREASE))
		  {
		    delete[] (*tmp_iter).transitions;
		    delete[] (*tmp_iter).vars_and_chans;
		    local_cycles.erase(tmp_iter);
		    deleted_loc_cycles++;
		    cycle_deleted=true;
		  }
	      }
	  }
    }
  while (cycle_deleted);
  if (debug)
    cout << "\tIrelevant local cycles: " << deleted_loc_cycles << endl;
}

void spor_t::detect_global_cycles(void)
{  
  change_var_on_cycle_t changed_vars_and_chans[unwrapped_variables+Table->get_channel_count()];    
  list<std::list<loc_cycle_t>::iterator> candidate_to_glob_cycle;
  for (std::list<loc_cycle_t>::iterator i = local_cycles.begin(); i != local_cycles.end(); ++i)
    {
      candidate_to_glob_cycle.push_front(i);
      for (size_int_t j=0; j<unwrapped_variables+Table->get_channel_count(); j++)
	changed_vars_and_chans[j]=(*i).vars_and_chans[j];
      complete_to_global_cycle(&candidate_to_glob_cycle, changed_vars_and_chans);
      candidate_to_glob_cycle.pop_front();
    }
  if (debug)
    cout << "\tNumber of global cycles: " << global_cycles.size() << endl;
}

/*bool LiesOnCycle(glob_cycle_t cycle)
{
  size_int_t i=0;
  while ((i<cycle.cycles_length)&&(cycle.transitions[i]!=best_trans))
    i++;
  return (i<cycle.cycles_length);
}*/

void spor_t::global_cycles_cover(void)
{ 
  size_int_t best_value;
  size_int_t trans_in_glob_cycle[System->get_trans_count()]; //trans_in_glob_cycle[transition_gid] = number of global cycles on which the transition lies
  while (global_cycles.size()>0)
    {
      //fulfill trans_in_glob_cycle
      for (size_int_t i=0; i<System->get_trans_count(); i++)
	trans_in_glob_cycle[i]=0;
      for (list<glob_cycle_t>::iterator iter = global_cycles.begin(); iter != global_cycles.end(); ++iter)
	for (size_int_t i=0; i<(*iter).cycles_length; i++)
	  trans_in_glob_cycle[(*iter).transitions[i]]++;

      //find transition lying on most global cycles
      best_value=0;
      for (size_int_t i=System->get_trans_count(); i>0; i--) //searching from back in order to detect as latest transition on the cycle as possible 	
	{                                                    //(it can result in better reduction)
	  if (trans_in_glob_cycle[i-1]>best_value)
	    {
	      best_value=trans_in_glob_cycle[i-1];
	      best_trans=i-1;
	    }	  
	}
      
      //ad the transition to sticky transitions and remove global cycles which the transition lies on
      dve_transition_t* trans = dynamic_cast<dve_transition_t*>(System->get_transition(best_trans));
      if (debug)
	cout << "\tSticky transition " << trans->get_process_gid() << '.' << trans->get_partial_id() << " lies on " << best_value << " global cycle(s)." << endl;
      sticky_trans.insert(best_trans);
      //      global_cycles.remove_if(LiesOnCycle); -- can't use since some memory release ("delete[]") is necessary
      list<glob_cycle_t>::iterator tmp_iter, iter=global_cycles.begin();
      size_int_t i;
      while (iter != global_cycles.end())
	{
	  i=0;
	  while ((i<(*iter).cycles_length)&&((*iter).transitions[i]!=best_trans))
	    i++;
	  tmp_iter=iter;
	  iter++;
	  if (i<(*tmp_iter).cycles_length)
	    {
	      delete[] (*tmp_iter).transitions;
	      global_cycles.erase(tmp_iter);
	    }
	}
    }
}

void spor_t::get_sticky_transitions(std::set<size_t> &sticky)
{
  sticky_trans=sticky;
  compute_sticky_transitions();
  sticky=sticky_trans;
}

void delete_local_cycles(std::list<loc_cycle_t> *loc_cycles)
{
  loc_cycle_t cycle;
  while (!loc_cycles->empty())
    {
      cycle = loc_cycles->front();
      delete[] cycle.vars_and_chans;
      delete[] cycle.transitions;
      loc_cycles->pop_front();
    }
}

void spor_t::compute_sticky_transitions(void)
{  
  if (debug)
    cout << "Detection of local cycles.." << endl;
  detect_local_cycles();
  if (debug)
    cout << endl << "Detection of global cycles.." << endl;
  detect_global_cycles();
  if (debug)
    cout << endl << "Covering global cycles.." << endl;
  global_cycles_cover(); //which transitions cover all global cycles
  delete_local_cycles(&local_cycles);
  if (debug)
    cout << endl << "I am done." << endl << endl;
}
