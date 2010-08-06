#include <fstream>
#include <sstream>
#include <vector>
#include "system/dve/dve_system.hh"
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "system/dve/syntax_analysis/dve_grammar.hh"
#include "common/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

//// Implementation of system_t virtual interface: ////

//Constructor - yes it is not virtual, but it is needed and it completes
//the necessary interface
dve_system_t::dve_system_t(error_vector_t & evect):
   system_t(evect), MAX_INIT_VALUES_SIZE(size_int_t(-1))
{
 //initialized before parsing because of use in the destructor
 property_has_synchronization = false;
 parameters = 0;
 initial_values = 0;
 initial_values_counts = 0;
 initial_states = 0;
 state_lids = 0;
 var_types = 0;
 psymbol_table = new dve_symbol_table_t(terr);
 channel_freq_answ = 0;
 channel_freq_ask = 0;
 constants = 0 ;
 global_variable_gids = 0;
// not_initialized = true;

 //Initialization of abilities:
 get_abilities().explicit_system_can_system_transitions   = true;
 get_abilities().explicit_system_can_evaluate_expressions = true;
 
 get_abilities().system_can_be_modified      = true;
 get_abilities().system_can_processes        = true;
 get_abilities().system_can_property_process = true;
 get_abilities().system_can_transitions      = true;

 get_abilities().process_can_be_modified = true;
 get_abilities().process_can_read        = true;
 get_abilities().process_can_transitions = true;

 get_abilities().transition_can_be_modified = true;
 get_abilities().transition_can_read        = true;
}

dve_system_t::~dve_system_t()
{
 DEB(cerr << "BEGIN of destructor of dve_system_t" << endl;)
// if (processes) delete [] processes;
 if (psymbol_table) delete psymbol_table;
 delete_fields();
 for (size_int_t i=0; i<processes.size(); ++i) delete processes[i];
 DEB(cerr << "END of destructor of dve_system_t" << endl;)
}

slong_int_t dve_system_t::read(std::istream & ins)
                   //defaultly inc = std::cin
{
 if (psymbol_table) delete psymbol_table;
 psymbol_table = new dve_symbol_table_t(terr);
 dve_parser_t parser(terr,this);
 int parse_result;
 dve_init_parsing(&parser,&terr,ins);   //function call from grammar.hh
 try
  {
   parse_result = dve_yyparse();
   if ((get_process_count() - int(get_with_property())) <= 0)
    {
     terr << "Error in a DVE source: You have to define at least 1 process."
          << psh();
    }
  }   //function call from grammar.hh
 catch (ERR_throw_t & err_type)
  {
   terr << "Error in a syntax of DVE source" << psh();
   return err_type.id;
  }
 if (!parse_result) consolidate();
 return parse_result;
}

slong_int_t dve_system_t::read(const char * const filename)
{
 try
  {
   std::ifstream f(filename);
   if (f)
    {
     int result = read(f);
     f.close();
     return result;
    }
   else
    { terr << "Opening of " << filename << " for reading is not possible."
           << thr(ERR_TYPE_SYSTEM, ERR_FILE_NOT_OPEN);
      return 1; /* unreachable return */
    }
  }
 catch (ERR_throw_t & err_type)
  {
   return err_type.id;
  }
 return 0; //file successfuly opened and parsed
}

slong_int_t dve_system_t::from_string(const std::string str)
{
 std::istringstream str_stream(str);
 return read(str_stream);
}

void dve_system_t::write(std::ostream & outs)
{
 //First writes out global variables:
 for (size_int_t i=0; i!=glob_var_count; i++)
 {
  dve_symbol_t * var = psymbol_table->get_variable(global_variable_gids[i]);
  if (var->is_const()) outs << "const ";
  if (var->is_byte()) outs << "byte ";
  else outs << "int ";
  outs << var->get_name();
  if (var->is_vector())
   {
    outs << "[" << var->get_vector_size() << "]";
    if (var->get_init_expr_count()) outs << " = {";
    for (size_int_t j=0; j!=var->get_init_expr_count(); j++)
     {
      outs << var->get_init_expr(j)->to_string();
      if (j!=(var->get_init_expr_count()-1)) outs << ", ";
      else outs << "}";
     }
   }
  else if (var->get_init_expr())
   { outs << " = " << var->get_init_expr()->to_string(); }
  outs << ";" << endl;
 }
 outs << endl;
 
 //Then writes out channels:
 for (size_int_t i=0; i!=channel_count; i++)
 {
  outs << "channel";
  dve_symbol_t * p_channel = psymbol_table->get_channel(i);
  //printing a list of types passing simult. through a channel
  if (p_channel->get_channel_typed())
   {
    outs << "(";
    for (size_int_t j=0; j!=p_channel->get_channel_type_list_size(); ++j)
     {
         if (p_channel->get_channel_type_list_item(j)==(dve_var_type_t)T_BYTE)
             outs << "byte";
      else outs << "int";
      if (j!=p_channel->get_channel_type_list_size()-1) outs << ",";
     }
    outs << ")";
   }
  outs << " ";
  outs << p_channel->get_name();
  if (p_channel->get_channel_typed())
   {
    outs << "[" << p_channel->get_channel_buffer_size() << "]";
   }
  outs << ";" << endl;
 }
 outs << endl;
 
 //Then writes out processes:
 outs << endl;
 for (size_int_t i=0; i!=processes.size(); i++)
  {
   processes[i]->write(outs);
   outs << endl;
  }
 
 //And finally writes out a type of a system:
 outs << endl << "system " << (system_synchro==SYSTEM_SYNC ? "sync" : "async");
 if (get_with_property())
   outs << " property "
        << psymbol_table->get_process(pproperty->get_gid())->get_name();
 outs << ";" << endl;
}

bool dve_system_t::write(const char * const filename)
{
 std::ofstream f(filename);
 if (f)
  {
   write(f);
   f.close();
   return true;
  }
 else
  { return false; }
}

std::string dve_system_t::to_string()
{
 std::ostringstream ostr;
 write(ostr);
 return ostr.str();
}

void dve_system_t::set_property_gid(const size_int_t gid)
{
 prop_gid = gid;
 pproperty = processes[gid];
 
 //determining a value of property_has_synchronization:
 property_has_synchronization = false;
 for (size_int_t i = 0; i!=pproperty->get_trans_count(); ++i)
  {
   dve_transition_t * dve_trans =
      dynamic_cast<dve_transition_t*>(pproperty->get_transition(i));
   if (dve_trans->get_sync_mode()!=SYNC_NO_SYNC)
     property_has_synchronization = true;
  }
}


   ///// Methods of the virtual interface of system_t
   ///// dependent on can_property_process() are inlined in dve_system.hh
   
   ///// Methods of the virtual interface of system_t
   ///// dependent on can_processes() are inlined in dve_system.hh
   
   ///// Methods of the virtual interface of system_t
   ///// dependent on can_transitions() are inlined in dve_system.hh
   
   ///// Methods of the virtual interface of system_t
   ///// dependent on can_be_modified() are here:

void dve_system_t::add_process(process_t * const process)
{
 dve_process_t * const dve_process = dynamic_cast<dve_process_t *>(process);
 processes.push_back(dve_process);
 psymbol_table->get_process(process->get_gid())->set_lid(process->get_gid());
}

void dve_system_t::remove_process(const size_int_t process_id)
{
 processes[process_id]->set_valid(false);
}

///// END of the implementation of the virtual interface of system_t

/* Functions for the evaluation in dve_system_t - they are substituted
 * in dve_explicit_system_t by the state-dependent functions */


all_values_t SYS_eval_id
(const void * params, const dve_expression_t & subexpr, bool & eval_err)
{
 DEB(cerr << "Evaluating ID" << endl;)
 if (SYS_p_parameters_t(params)->initial_values_counts[subexpr.get_ident_gid()])
   return
     SYS_p_parameters_t(params)->
         initial_values[subexpr.get_ident_gid()].all_value;
 else { eval_err = true; gerr << "Unexpected error: uninitialized variable used"
        " in initialization." << thr(); return 0; }
}

all_values_t SYS_eval_id_compact
(const void * params, const size_int_t & gid, bool & eval_err)
{
 DEB(cerr << "Evaluating ID" << endl;)
 if (SYS_p_parameters_t(params)->initial_values_counts[gid])
   return
     SYS_p_parameters_t(params)->
         initial_values[gid].all_value;
 else { eval_err = true; gerr << "Unexpected error: uninitialized variable used"
        " in initialization." << thr(); return 0; }
}

all_values_t SYS_eval_square_bracket
  (const void * params, const dve_expression_t & subexpr,
   const size_int_t & array_index, bool & eval_err)
{
 size_int_t init_size =
   SYS_p_parameters_t(params)->initial_values_counts[subexpr.get_ident_gid()];
 if (init_size && array_index<init_size)
   return
     SYS_p_parameters_t(params)->initial_values[subexpr.get_ident_gid()].
       all_values[array_index];
 else { eval_err = true; gerr << "Unexpected error: uninitialized variable used"
        " in initialization." << thr(); return 0; }
}

all_values_t SYS_eval_square_bracket_compact
  (const void * params, const size_int_t & gid,
   const size_int_t & array_index, bool & eval_err)
{
 size_int_t init_size =
   SYS_p_parameters_t(params)->initial_values_counts[gid];
 if (init_size && array_index<init_size)
   return
     SYS_p_parameters_t(params)->initial_values[gid].
       all_values[array_index];
 else { eval_err = true; gerr << "Unexpected error: uninitialized variable used"
        " in initialization." << thr(); return 0; }
}

all_values_t SYS_eval_dot
   (const void * params, const dve_expression_t & subexpr, bool & eval_err)
{
 DEBFUNC(cerr << "CALL of SYS_eval_dot" << endl;)
 return
   (SYS_p_parameters_t(params)->initial_states[subexpr.get_symbol_table()->get_state(subexpr.get_ident_gid())->get_process_gid()]
    == SYS_p_parameters_t(params)->state_lids[subexpr.get_ident_gid()]);
}

all_values_t SYS_eval_dot_compact
   (const void * params, const dve_symbol_table_t *symbol_table, const size_int_t &gid, bool & eval_err)
{
 DEBFUNC(cerr << "CALL of SYS_eval_dot" << endl;)
 return
   (SYS_p_parameters_t(params)->initial_states[symbol_table->get_state(gid)->get_process_gid()]
    == SYS_p_parameters_t(params)->state_lids[gid]);
}

////DVE specific methods of dve_system_t:

void dve_system_t::delete_fields() {
 if (initial_values)
  {
   for (size_int_t i = 0; i != var_count; ++i)
     if (initial_values_counts[i] &&
         initial_values_counts[i]!=MAX_INIT_VALUES_SIZE)
       delete [] initial_values[i].all_values;
   delete [] initial_values;
  }
 if (initial_values_counts) delete [] initial_values_counts;
 if (initial_states) delete [] initial_states;
 if (state_lids) delete [] state_lids;
 if (var_types) delete [] var_types;
 if (constants) delete [] constants;
 if (channel_freq_answ) delete [] channel_freq_answ;
 if (channel_freq_ask) delete [] channel_freq_ask;
 if (global_variable_gids) delete [] global_variable_gids;
}


void dve_system_t::consolidate()
{
 DEB(cerr << "Post-parsing analysis has begun" << endl;)
 
 channel_count = psymbol_table->get_channel_count();
 var_count = psymbol_table->get_variable_count();
 state_count = psymbol_table->get_state_count();
  
 DEB(cerr << "BEGIN of initializators evaluation" << endl;)
 eval_id = SYS_eval_id;
 eval_square_bracket = SYS_eval_square_bracket;
 eval_dot = SYS_eval_dot;
 eval_id_compact = SYS_eval_id_compact;
 eval_square_bracket_compact = SYS_eval_square_bracket_compact;
 eval_dot_compact = SYS_eval_dot_compact;
 //we use the claim, that formerly declared variables
 const dve_symbol_t * cp_symbol;
 delete_fields();
 initial_values = new SYS_initial_values_t[var_count];
 initial_values_counts = new size_int_t[var_count];
 constants = new bool[var_count];
 initial_states = new size_int_t[processes.size()];
 state_lids = new size_int_t[state_count];
 var_types = new dve_var_type_t[var_count];
 DEBCHECK(if (parameters) gerr << "Check failed: parameters!=0" << thr();)
 parameters = new SYS_parameters_t(initial_values, initial_states,
                                   initial_values_counts, state_lids);
 for (size_int_t i = 0; i!=var_count; ++i) initial_values_counts[i] = 0;
 DEB(cerr << "Creating list of all transitions." << endl;)
 for (size_int_t i=0; i!=processes.size(); i++)
  {
   dve_process_t * process = processes[i];
   for (size_int_t j=0; j!=process->get_trans_count(); j++)
    {
      dve_transition_t * t=dynamic_cast<dve_transition_t*>(process->get_transition(j));
      t->set_gid(transitions.size());
      transitions.push_back(t);
    }
  }
 
 DEB(cerr << "Computing frequencies of channels." << endl;)
 channel_freq_ask = new size_int_t[channel_count];
 channel_freq_answ = new size_int_t[channel_count];
 for (size_int_t i=0; i!=channel_count; i++)
   channel_freq_ask[i] = channel_freq_answ[i] = 0;
 for (size_int_t i=0; i!=transitions.size(); i++)
  {
   sync_mode_t mode = transitions[i]->get_sync_mode();
   if (mode==SYNC_ASK)
     ++(channel_freq_ask[transitions[i]->get_channel_gid()]);
   else if (mode==SYNC_EXCLAIM)
     ++(channel_freq_answ[transitions[i]->get_channel_gid()]);
  }
 
 DEB(cerr << "Initializing the field of GIDs of global variables." << endl;)
 glob_var_count = 0;
 for (size_int_t i=0; i!=var_count; i++)
   if (psymbol_table->get_variable(i)->get_process_gid()==NO_ID)
     glob_var_count++;
 global_variable_gids = new size_int_t[glob_var_count];
 glob_var_count = 0;
 for (size_int_t i=0; i!=var_count; i++)
   if (psymbol_table->get_variable(i)->get_process_gid()==NO_ID)
     { global_variable_gids[glob_var_count] = i; glob_var_count++; }
 
 DEB(cerr << "Getting LIDs of states" << endl;)
 for (size_int_t i=0; i!=state_count; i++)
  {
   cp_symbol = psymbol_table->get_state(i);
   state_lids[i] = cp_symbol->get_lid();
  }
 
 DEB(cerr << "Getting initial states of processes."<< endl;)
 for (size_int_t i=0; i!=processes.size(); i++)
   initial_states[i] = processes[i]->get_initial_state();
 
 DEB(cerr << "Initializing values for variables."<< endl;)
 for (size_int_t i = 0; i!=var_count; ++i)
  {
   cp_symbol = psymbol_table->get_variable(i);
   constants[i] = cp_symbol->is_const();
   if (!cp_symbol->is_vector())
    {
     DEB(cerr << "SCALAR " << cp_symbol->get_name() << " GID:" << i)
     DEB(     << " process: " << cp_symbol->get_process_gid() << endl;)
     const dve_expression_t * cp_expr = cp_symbol->get_init_expr();
     var_types[i] = cp_symbol->get_var_type();
     if (cp_expr)
      {
       const dve_expression_t & expr = *cp_expr;
       bool eval_err = false;
       size_int_t eval_result;
       initial_values_counts[i] = MAX_INIT_VALUES_SIZE;
       eval_result = eval_expr(&expr,eval_err);
       if (not_in_limits(cp_symbol->get_var_type(),eval_result))
         terr << "Error: Expression "
              << cp_symbol->get_init_expr()->to_string()
              << " exceeded bounds of type." << thr();
       initial_values[i].all_value = eval_result;
       DEB(cerr << "Initialized to " << eval_result << endl;)
       if (eval_err) terr << "Error when evaluating the expression "
                          << cp_symbol->get_init_expr()->to_string()
                          << thr();
      }
     else initial_values_counts[i] = 0;
    }
   else
    {
     DEB(cerr << "VECTOR " << cp_symbol->get_name() << " GID:" << i)
     DEB(     << " process: " << cp_symbol->get_process_gid() << endl;)
     size_int_t count = cp_symbol->get_init_expr_count();
     size_int_t array_size = cp_symbol->get_vector_size();
     DEB(cerr << "Evaluating initial values of an array with array_size = ")
     DEB(     << array_size << " and count of initializators = ")
     DEB(     << count << endl;)
     var_types[i] = cp_symbol->get_var_type();
     /* when the field of initial values is longer then actual field
      * the remaining values are omitted */
     if (array_size<count) count = array_size;
     if (count)
      {
       initial_values_counts[i] = count;
       initial_values[i].all_values = new all_values_t[count];
       for (size_int_t j=0; j!=count; ++j)
        {
         const dve_expression_t * expr = cp_symbol->get_init_expr(j);
         bool eval_err = false;
         size_int_t eval_result;
         eval_result = eval_expr(expr,eval_err);
         if (not_in_limits(cp_symbol->get_var_type(),eval_result))
           terr << "Error: Expression "
                << cp_symbol->get_init_expr(j)->to_string()
                << " exceeded bounds of type." << thr();
         initial_values[i].all_values[j] = eval_result;
         if (eval_err) terr << "Error when evaluating the expression "
                            << cp_symbol->get_init_expr(j)->to_string()
                            << thr();
        }
      }
     else initial_values_counts[i] = 0;
    }
  }
 delete SYS_p_parameters_t(parameters);
 parameters = 0;
 DEB(cerr << "END of initializators evaluation" << endl;)
 
 DEB(DBG_print_all_initialized_variables();)
 
 DEB(cerr << "Post-parsing analysis has finished" << endl;)
}

bool dve_system_t::not_in_limits(dve_var_type_t var_type,all_values_t value)
{
 if (var_type==VAR_BYTE)
   return (value<MIN_BYTE || value>MAX_BYTE);
 else if (var_type==VAR_INT)
   return (value<MIN_SSHORT_INT || value>MAX_SSHORT_INT);
 else { terr << "Unexpected error: Unknown variable type." << thr();
        return false; }
}

void dve_system_t::DBG_print_all_initialized_variables()
{
 cerr << "Initial values of variables:" << endl;
 for (size_int_t i=0; i!=var_count; ++i)
  {
   const dve_symbol_t * s = psymbol_table->get_variable(i);
   if (!s->is_vector())
    {
     cerr << s->get_name() << "=";
     if (initial_values_counts[i]) cerr << initial_values[i].all_value << endl;
     else cerr << "NI" << endl;
    }
   else
    {
     cerr << s->get_name() << "=| ";
     if (initial_values_counts[i])
      {
       for (size_int_t j=0;j!=initial_values_counts[i];++j)
         cerr << initial_values[i].all_values[j] << " | ";
       cerr << endl;
      }
     else cerr << "NI" << endl;
    }
  }
}


all_values_t dve_system_t::eval_expr
   (const dve_expression_t * const expr, bool & eval_err) const
{
 DEBAUX(cerr << "BEGIN of evalutaion" << endl;)
 DEBFUNC(cerr << "BEGIN of evaluation of "<<expr->to_string()<<endl;)
 expr_stack_element_t * aux_top;
 all_values_t aux_num;
 int op;

 if (expr->is_compacted())
   {
     return fast_eval(static_cast<compacted_viewer_t*>(expr->get_p_compact()),eval_err);
   }

 estack.clear();
 vstack.clear();

 //When the expression is not a constant, that we push double (expression,left
 //subexpression) on the top of the expression stack, otherwise we straightly
 //put there the constant.
 // NOTE left()  = last()     //why fuck is it this way???
 //      right() = begin()

 if (expr->arity()) estack.push_back(expr_stack_element_t(expr, expr->left()));
 else estack.push_back(expr_stack_element_t(expr,0));
 
 while (!estack.empty())
  {
   DEBEVAL(cerr << "Stack in not empty (size " << estack.size() << ")" << endl;)
   DEBEVAL(cerr << "vstack.size() = " << vstack.size();)
   DEBEVAL(if (!vstack.empty()) cerr<<" -- "<<vstack.back()<<endl; else cerr<<endl;)
   aux_top = & estack.back();
   
   const dve_expression_t & subexpr = *aux_top->expr;
   op = subexpr.get_operator();

   //lazy evaluation of boolean expressions:
   //of operator is boolean AND and we should begin the evaluation of the
   //right subexpression we look to the vstack on the value of the left
   //subexpression and of it is 0 then we can finish the evaluation.
   //
   //Similarly in the case of OR
   if (op==T_BOOL_AND && subexpr.right()==aux_top->ar && (!vstack.back()))
     estack.pop_back();
   else if (op==T_BOOL_OR && subexpr.right()==aux_top->ar && vstack.back())
     estack.pop_back();
   else if ((!aux_top->ar) || (subexpr.right()-1) == aux_top->ar)
    {
     DEBEVAL(cerr << "Operator " << op << endl;)
     switch (op) //presumes correct expressions!!!
                 //no multiple assignments!!!
      {
       case T_NAT:
        {
         DEBEVAL(cerr << "It's number!";)
         aux_num = subexpr.get_value();
         DEBEVAL(cerr << "(" << aux_num << ")" << endl;)
         vstack.push_back(aux_num);
         break;
        }
       //TODO:Maybe control of byte/int overflow could be goog here
       case T_FOREIGN_ID:
       case T_ID:
        {
         if (constants[subexpr.get_ident_gid()])
          {
           if (initial_values_counts[subexpr.get_ident_gid()])
             aux_num = initial_values[subexpr.get_ident_gid()].all_value;
           else terr << "Unexpected error - constant is not initialized"
                     << thr();
           DEBEVAL(cerr << "(" << aux_num << ")" << endl;)
           vstack.push_back(aux_num);
          }
         else
           vstack.push_back(eval_id(parameters,subexpr,eval_err));
         break;
        }
       case T_FOREIGN_SQUARE_BRACKETS:
       case T_SQUARE_BRACKETS:
        {
         if (constants[subexpr.get_ident_gid()])
          {
           if (initial_values_counts[subexpr.get_ident_gid()])
             aux_num = initial_values[subexpr.get_ident_gid()].all_values
                                                                 [vstack.back()];
           else terr << "Unexpected error - constant field is not initialized"
                     << thr();
           vstack.back() = aux_num;
          }
         else
           vstack.back() = eval_square_bracket
                             (parameters,subexpr,vstack.back(),eval_err);
         break;
        }
       case T_PARENTHESIS:
        { /* nothing happens */ break; }
       case T_UNARY_MINUS:
        { vstack.back() = - vstack.back(); break; }
       case T_TILDE:
        { vstack.back() = ~ vstack.back(); break; }
       case T_BOOL_NOT:
        { vstack.back() = ! vstack.back(); break; }
       case T_LT:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() < aux_num; break; }
       case T_LEQ:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() <= aux_num; break; }
       case T_GT:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() > aux_num; break; }
       case T_GEQ:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() >= aux_num; break; }
       case T_NEQ:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() != aux_num; break; }
       case T_EQ:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() == aux_num; break; }
       case T_PLUS:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() + aux_num; break; }
       case T_MINUS:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() - aux_num; break; }
       case T_MULT:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() * aux_num; break; }
       case T_DIV:
        { aux_num = vstack.back(); vstack.pop_back();
          if (aux_num==0) eval_err = true;  //division by zero
          else vstack.back() = vstack.back() / aux_num;
          break; }
       case T_MOD:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() % aux_num; break; }
       case T_AND:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() & aux_num; break; }
       case T_OR:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() | aux_num; break; }
       case T_LSHIFT:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() << aux_num; break; }
       case T_RSHIFT:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() >> aux_num; break; }
       case T_BOOL_AND:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() && aux_num; break; }
       case T_BOOL_OR:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = vstack.back() || aux_num; break; }
       case T_IMPLY:
        { aux_num = vstack.back(); vstack.pop_back();
          vstack.back() = (!vstack.back()) || aux_num; break; }
       case T_DOT:
        { 
         vstack.push_back(eval_dot(parameters,subexpr,eval_err));
         break;
        }
       default:
        terr << "Unexpected error: unknown expression operator" << thr();
      }
     estack.pop_back();
    }
   else
    {
     const dve_expression_t * const to_push = aux_top->ar;
     aux_top->ar -= 1;
     estack.push_back(expr_stack_element_t(to_push, to_push->left()));
    }
  }

 DEB(if (vstack.size()>1) cerr << "WARNING - vstack is of size > 1" << endl;)
 DEB(cerr << "eval_(sub)expr returns " << vstack.back() << endl;)
 DEBFUNC(cerr << "END of evaluation" <<endl;)
 return vstack.back();
}


all_values_t dve_system_t::fast_eval(compacted_viewer_t *_expr,bool & eval_err) const
{
  all_values_t aux_num;
  compacted_t e,e1;
  e.ptr = _expr;

  DEBFUNC(cerr<<"EVAL: "<<e.to_string()<<endl;);
  DEBFUNC(cerr<<"fast_eval op="<<e.get_operator()<<endl;);

  switch(e.get_operator()) {
  case T_NAT:
    return e.get_value();
    break;
  case T_FOREIGN_ID:
  case T_ID:
    if (constants[e.get_gid()])
      {
	if (initial_values_counts[e.get_gid()])
	  {
	    return initial_values[e.get_gid()].all_value;
	  }
	else
	  {
	    gerr << "Unexpected error - constant is not initialized"
		 << thr();
	  }
      }
    else
      { 
	size_int_t g=e.get_gid();
	return eval_id_compact(parameters,g,eval_err);
      }
    break;
  case T_FOREIGN_SQUARE_BRACKETS:
  case T_SQUARE_BRACKETS:
    //left subexpr is always T_NAT with gid of the variable
     e1.ptr=e.left();
     aux_num=e1.get_value();
    if (constants[aux_num])
      {
	if (initial_values_counts[aux_num])
	  return initial_values[aux_num].all_values[fast_eval(e.right(),eval_err)];
	else gerr << "Unexpected error - constant field is not initialized"
		  << thr();
      }
    else
      {
	return eval_square_bracket_compact
	  (parameters,aux_num,fast_eval(e.right(),eval_err),eval_err);
      }
    break;
  case T_PARENTHESIS:
    return fast_eval(e.left(),eval_err);
    break;
  case T_UNARY_MINUS:
    return -fast_eval(e.left(),eval_err); 
    break;
  case T_TILDE:
    return ~fast_eval(e.left(),eval_err);
    break;
  case T_BOOL_NOT:
    return !fast_eval(e.left(),eval_err);
    break;
  case T_LT:
    return ( fast_eval(e.left(),eval_err) < fast_eval(e.right(),eval_err) );
    break; 
  case T_LEQ:
    return ( fast_eval(e.left(),eval_err) <= fast_eval(e.right(),eval_err) );
    break;
  case T_GT:
    return ( fast_eval(e.left(),eval_err) > fast_eval(e.right(),eval_err) );
    break;
  case T_GEQ:
    return ( fast_eval(e.left(),eval_err) >= fast_eval(e.right(),eval_err) );
    break;
  case T_NEQ:
    return ( fast_eval(e.left(),eval_err) != fast_eval(e.right(),eval_err) );
    break;
  case T_EQ:
    // right subexpr is often T_NAT, lets try to optimize this
    e1.ptr=e.right();
    if (e1.get_operator()==T_NAT)
      {
	aux_num=e1.get_value();
	return ( fast_eval(e.left(),eval_err) == aux_num );
      }
    else
      return ( fast_eval(e.left(),eval_err) == fast_eval(e.right(),eval_err) );
    break;
  case T_PLUS:
    return ( fast_eval(e.left(),eval_err) + fast_eval(e.right(),eval_err) );
    break;
  case T_MINUS:
    return ( fast_eval(e.left(),eval_err) - fast_eval(e.right(),eval_err) );
    break;
  case T_MULT:
    return ( fast_eval(e.left(),eval_err) * fast_eval(e.right(),eval_err) );
    break;
  case T_DIV:
    aux_num=fast_eval(e.right(),eval_err);
    if (aux_num)
      {
	return ( fast_eval(e.left(),eval_err) / fast_eval(e.right(),eval_err) );
      }
    else
      {
	eval_err = true;
	DEBFUNC(cerr<<"Division by zero"<<endl;);
	return 0;
      }
    break;
  case T_MOD:
    return ( fast_eval(e.left(),eval_err) % fast_eval(e.right(),eval_err) );
    break;
  case T_AND:
    return ( fast_eval(e.left(),eval_err) & fast_eval(e.right(),eval_err) );
    break;
  case T_OR:
    return ( fast_eval(e.left(),eval_err) | fast_eval(e.right(),eval_err) );
    break;
  case T_LSHIFT:
    return ( fast_eval(e.left(),eval_err) << fast_eval(e.right(),eval_err) );
    break;
  case T_RSHIFT:
    return ( fast_eval(e.left(),eval_err) >> fast_eval(e.right(),eval_err) );
    break;
  case T_BOOL_AND:
    aux_num=fast_eval(e.left(),eval_err);
    if (aux_num)
      {
	return ( fast_eval(e.right(),eval_err) > 0 );
      }
    else
      return 0;
    break;
  case T_BOOL_OR:
    aux_num=fast_eval(e.left(),eval_err);
    if (!aux_num)
      {
	return ( fast_eval(e.right(),eval_err) > 0 );
      }
    else
      return 1;
    break;
  case T_IMPLY: //lazy evaluation of implication
    aux_num=fast_eval(e.left(),eval_err);
    if (aux_num)
      {
	return ( fast_eval(e.right(),eval_err) > 0 );
      }
    else
      return 1;
    break;
  case T_DOT:
    return eval_dot_compact(parameters,
			    psymbol_table,
			    e.get_gid(),
			    eval_err);
    break;
  default:
    gerr << "Unexpected error: unknown expression operator" << thr();
    break;
  }
  gerr << "Shouldn't reach this point in fast_eval()"<<thr();
  return 0;
}










