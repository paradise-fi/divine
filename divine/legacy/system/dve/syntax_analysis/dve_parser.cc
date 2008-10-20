#include <cstdio>
#include <cstring>
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "system/dve/syntax_analysis/dve_parser.hh"
#include "system/dve/dve_expression.hh"
#include "system/dve/dve_prob_system.hh"
#include "system/dve/dve_prob_process.hh"
#include "common/error.hh"
#include "common/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

#define SYN_ERR(x) terr << firstline << ":" << firstcol << "-" << lastline << ":" << lastcol << " " << x
using std::vector;

dve_parser_t::dve_parser_t(error_vector_t & evect, dve_system_t * my_system):
  terr(evect), dve_system_to_fill(my_system), dve_process_to_fill(0),
  dve_transition_to_fill(0), dve_expression_to_fill(0),
  dve_prob_transition_to_fill(0)
{
 if (dynamic_cast<dve_prob_system_t *>(my_system)) probabilistic = true;
 else probabilistic = false;
 mode = SYSTEM;
 set_dve_system_to_fill(dve_system_to_fill);
 accepting_groups=0;
 new_accepting_set = true;
}

dve_parser_t::dve_parser_t(error_vector_t & evect, dve_process_t * my_process):
  terr(evect), dve_system_to_fill(0), dve_process_to_fill(my_process),
  dve_transition_to_fill(0), dve_expression_to_fill(0),
  dve_prob_transition_to_fill(0)
{
 probabilistic = false;
 mode = PROCESS;
 set_dve_process_to_fill(dve_process_to_fill);
 accepting_groups=0;
 new_accepting_set = true;
}

dve_parser_t::dve_parser_t(error_vector_t & evect,
                           dve_prob_process_t * my_process):
  terr(evect), dve_system_to_fill(0), dve_process_to_fill(my_process),
  dve_transition_to_fill(0), dve_expression_to_fill(0),
  dve_prob_transition_to_fill(0)
{
 probabilistic = true;
 mode = PROCESS;
 set_dve_process_to_fill(dve_process_to_fill);
 accepting_groups=0;
 new_accepting_set = true;
}

dve_parser_t::dve_parser_t(error_vector_t & evect, dve_transition_t * my_transition):
  probabilistic(false),
  terr(evect), dve_system_to_fill(0), dve_process_to_fill(0),
  dve_transition_to_fill(my_transition), dve_expression_to_fill(0),
  dve_prob_transition_to_fill(0)
{
 mode = TRANSITION;
 set_dve_transition_to_fill(dve_transition_to_fill);
 accepting_groups=0;
 new_accepting_set = true;
}

dve_parser_t::dve_parser_t(error_vector_t & evect, dve_prob_transition_t * my_transition):
  probabilistic(true),
  terr(evect), dve_system_to_fill(0), dve_process_to_fill(0),
  dve_transition_to_fill(0), dve_expression_to_fill(0),
  dve_prob_transition_to_fill(my_transition)
{
 mode = PROB_TRANSITION;
 set_dve_prob_transition_to_fill(dve_prob_transition_to_fill);
 accepting_groups=0;
 new_accepting_set = true;
}

dve_parser_t::dve_parser_t(error_vector_t & evect, dve_expression_t * my_expression):
  probabilistic(false),
  terr(evect), dve_system_to_fill(0), dve_process_to_fill(0),
  dve_transition_to_fill(0), dve_expression_to_fill(my_expression),
  dve_prob_transition_to_fill(0)
{
 mode = EXPRESSION;
 set_dve_expression_to_fill(dve_expression_to_fill);
 accepting_groups=0;
 new_accepting_set = true;
}

dve_process_t * dve_parser_t::get_current_process_pointer() const
{
 if (mode==SYSTEM)
   return dynamic_cast<dve_process_t *>(dve_system_to_fill->get_process(current_process));
 else if (mode==PROCESS)
   return dve_process_to_fill;
 else
  { gerr << "Unexpected error: not parsing process or system" << thr();
    return 0; /*this line is unreachable*/ }
}

void dve_parser_t::set_dve_system_to_fill(dve_system_t * const sys)
{ dve_system_to_fill = sys; if (sys) initialise(sys); }

void dve_parser_t::set_dve_process_to_fill(dve_process_t * const proc)
{
 dve_process_to_fill = proc;
 if (proc) initialise(dynamic_cast<dve_system_t*>(proc->get_parent_system()));
}

void dve_parser_t::set_dve_transition_to_fill(dve_transition_t * const trans)
{
 dve_transition_to_fill = trans;
 if (trans) initialise(dynamic_cast<dve_system_t*>(trans->get_parent_system()));
}

void dve_parser_t::set_dve_prob_transition_to_fill(dve_prob_transition_t * const trans)
{
 dve_prob_transition_to_fill = trans;
 if (trans) initialise(dynamic_cast<dve_system_t*>(trans->get_parent_system()));
}

void dve_parser_t::set_dve_expression_to_fill(dve_expression_t * const expr)
{
 dve_expression_to_fill = expr;
 if (expr) initialise(dynamic_cast<dve_system_t*>(expr->get_parent_system()));
}

void dve_parser_t::initialise(dve_system_t * const pointer_to_system)
{
 current_process = NO_ID;
 while (!subexpr_stack.empty())
  {
   delete subexpr_stack.top();
   subexpr_stack.pop();
  }
 current_bound = 0;
 current_vartype = 0;
 current_const = false;
 psymbol_table = pointer_to_system->get_symbol_table();
 psystem = pointer_to_system;
 last_init_was_field = false;
 current_guard = 0;
 for(std::list<dve_expression_t *>::iterator i=current_sync_expr_list.begin();
     i!=current_sync_expr_list.end(); ++i) delete (*i);
 current_sync_expr_list.clear();
 current_effects.clear();
 current_channel_gid = 0;
 current_field.clear();
 current_first_state_gid = NO_ID;
 glob_count = 0;
 save_glob_used = false;
 glob_used.clear();
 for(std::list<dve_expression_t *>::iterator i=current_expression_list.begin();
     i!=current_expression_list.end(); ++i) delete (*i);
 current_expression_list.clear();
 current_type_list.clear();
 prop_req.is_set = false;
 
 firstline=0;
 lastline=0;
 firstcol=0;
 lastcol=0;
}

dve_parser_t::~dve_parser_t() {
}

//THINGS FOR PARSING-TIME//

void dve_parser_t::done()
{
 DEBFUNC(cerr << "BEGIN of dve_parser_t::done()" << endl;)
 //initialization of iterators for some functions
 
 if (mode==SYSTEM)
  {
   //and now: checking of temporarity of the process and their states
   for (size_int_t i = 0; i!=dve_system_to_fill->get_process_count(); ++i)
    {
     //test of temporarity for processes:
     const dve_symbol_t * symb = psymbol_table->get_process(i);
     if (!symb->get_valid())
       SYN_ERR("Process " << symb->get_name() <<
               " was referenced, but was not declared"
               " till the end of the document" << thr());
     else
      {
       //similar test of temporarity of each state symbol of non-temporary process
       dve_process_t * ith_process = dynamic_cast<dve_process_t *>(dve_system_to_fill->get_process(i));
       size_int_t count = ith_process->get_state_count();
       for (size_int_t j=0; j!=count; ++j)
        {
         const dve_symbol_t * state_symb =
           psymbol_table->get_state(ith_process->get_state_gid(j));
         if (!state_symb->get_valid())
           SYN_ERR("Referenced symbol " << symb->get_name() << "."
                   << state_symb->get_name() <<" that was referenced, "
                      "but was not declared" << thr());
        }
      }
    }
  }
 else if (mode == EXPRESSION) {
   if (subexpr_stack.size()!=1)
     if (subexpr_stack.size()==0)
      {
       terr << "There is not stored any expression" << thr();
      }
     else
      {
       terr << "There are stored" << subexpr_stack.size() << " expression "
            << " instead of the single one" << thr();
      }
   else
     dve_expression_to_fill->swap(*subexpr_stack.top());
   
 }
 
 DEBFUNC(cerr << "END of dve_parser_t::done() => parsing finished" << endl;)
}

void dve_parser_t::proc_decl_begin(const char * name)
{
 size_int_t fpos = add_process(name);
 if (fpos!=NO_ID)
   current_process = fpos;
 else
   SYN_ERR("Process of name " << name << " is already declared" << thr());
}

void dve_parser_t::proc_decl_done()
{
 current_process = NO_ID;
}

void dve_parser_t::var_decl_begin(const int type_nbr)
{
 DEB(cerr << "Beginning of " << (current_const ? "const " : "") <<)
 DEB(        "variables of base type: " <<)
 DEB(         (type_nbr==T_INT ? "int" : "byte") << endl;)
 current_vartype = type_nbr;
}

void dve_parser_t::take_expression()
{
 DEBFUNC(cerr << "BEGIN of taking expression" << endl;)
 if (!current_expression_list.empty())
   terr << "Unexpected error - list of expressions is not empty!" << thr();
 if (!subexpr_stack.empty())
   terr << "Unexpected error - stack of subexpressions is not empty!" << thr();
}

void dve_parser_t::take_expression_cancel() {
 DEBFUNC(cerr << "END of taking expression - CANCELLED" << endl;)
 if (!subexpr_stack.empty()) { delete subexpr_stack.top();
                               subexpr_stack.pop();
                               DEB(cerr << "Poping from subexpr_stack" << endl;) }
}

void dve_parser_t::var_init_is_field(const bool is_field)
{
 last_init_was_field = is_field;
}

void dve_parser_t::var_init_field_part() {
 if (!subexpr_stack.empty()) {
  current_field.push_back(subexpr_stack.top());
  subexpr_stack.pop();
  DEB(cerr << "Poping from subexpr_stack" << endl;)
 }
 else
   terr << "Unexpected error - stack of subexpressions is empty" << thr();
}

void dve_parser_t::var_decl_done()
{
 DEBFUNC(cerr << "END of variable declaration." << endl;)
}
  
void dve_parser_t::var_decl_create(const char * name, const int arrayDimesions, const bool initialized)
{
 DEBFUNC(cerr << "END of taking expression - to initialisation" << endl;)
 DEBFUNC(cerr << "BEGIN of variable creation" << endl;)
 DEB(cerr << name << " ... dimensions: " << arrayDimesions << " initiated: " << (initialized ? "Yes" : "No") << endl;)
 if (initialized)
  {
   if ((last_init_was_field) && arrayDimesions == 0)
    {SYN_ERR("Scalar variable cannot be initialised by a vector value!"<<thr());}
   if ((!last_init_was_field) && arrayDimesions != 0)
    {SYN_ERR("Vector variable cannot be initalised by a scalar value!"<<thr();)}
  }
 if (add_variable(name, current_process, initialized, bool(arrayDimesions)))
  {
   
  }
 else
  {
   if (initialized) take_expression_cancel();
   SYN_ERR("Identifier " << name << " redeclared" << thr());
  }
 DEBFUNC(cerr << "END of variable creation" << endl;)
}

void dve_parser_t::var_decl_cancel()
{
 DEB(cerr << "Declaration canceled." << endl;)
}

void dve_parser_t::var_decl_array_size(const int bound)
{
 DEB(cerr << "Array bound: " << bound << endl;)
 current_bound = bound;
 if (current_bound<=0)
   SYN_ERR("Error: Array bound can't be less than 1." << thr());
 if (current_bound>=DVE_MAX_ARRAY_SIZE)
   SYN_ERR("Error: Array bound can't be more than " << DVE_MAX_ARRAY_SIZE <<
           thr());
}

void dve_parser_t::expr_false()
{
 DEB(cerr << "Subexpression 'false'" << endl;)
 add_sub_num(0);
}

void dve_parser_t::expr_true()
{
 DEB(cerr << "Subexpression 'true'" << endl;)
 add_sub_num(1);
}

void dve_parser_t::expr_nat(const int num)
{
 DEB(cerr << "Subexpression 'number': " << num << endl;)
 add_sub_num(num);
}

//Inserts scalar variable to tthe expression
void dve_parser_t::expr_id(const char * name, int expr_op)
{
 DEB(cerr << "Subexpression 'ID':" << name << endl;)
 size_int_t aux_symbol_pos;
 if (current_process==NO_ID)
   aux_symbol_pos = psymbol_table->find_global_symbol(name);
 else aux_symbol_pos = psymbol_table->find_visible_symbol(name,current_process);
 if (aux_symbol_pos == NO_ID)
   SYN_ERR("Variable " << name << " undeclared" << thr());
 else
  {
   if (psymbol_table->get_symbol(aux_symbol_pos)->is_variable())
     add_sub_id(aux_symbol_pos,expr_op);
   else
     SYN_ERR(name << "is not a scalar variable" << thr());
  }
}

void dve_parser_t::expr_array_mem(const char * name, int expr_op)
{
 DEB(cerr << "Subexpression 'array dereference'" << endl;)
 size_int_t aux_symbol_pos;
 if (current_process==NO_ID)
   aux_symbol_pos = psymbol_table->find_global_symbol(name);
 else aux_symbol_pos = psymbol_table->find_visible_symbol(name,current_process);
 if (aux_symbol_pos == NO_ID)
   SYN_ERR("Identifier " << name << " undeclared" << thr());
 else
  {
   if (psymbol_table->get_symbol(aux_symbol_pos)->is_variable() &&
       psymbol_table->get_symbol(aux_symbol_pos)->is_vector())
     add_sub_id(aux_symbol_pos, expr_op, 1); 
   else
     SYN_ERR(name << "is not a vector variable" << thr());
  }
}

void dve_parser_t::expr_parenthesis()
{
 DEB(cerr << "Subexpression '()'" << endl;)
 add_sub_op(T_PARENTHESIS,1);
}

void dve_parser_t::expr_assign(const int assignType)
{
 DEB(cerr << "Subexpression 'Assign': " << assignType << endl;)
 add_sub_op(assignType,2);  //inserting subexpression "assignment"
 /* analysis for glob_used list: */
 const dve_expression_t * s = subexpr_stack.top()->left();
 
 //assures that only one assignment in a single command is permitted
 if (s->get_operator() != T_ID && s->get_operator() != T_SQUARE_BRACKETS)
  {
   SYN_ERR("Argument on the left side is not a l-value. Only form <id> = <expr>"
           " is allowed" << thr());
   DEB(cerr << "Non-l-value operator: " << s->get_operator() << endl;)
  }
 else //It is a variable (simple or array)
   if (save_glob_used)
    {
     size_int_t sid =
       psymbol_table->get_variable(s->get_ident_gid())->get_sid();
     if (psymbol_table->get_symbol(sid)->get_process_gid()==NO_ID)
      {
       dve_symbol_t * symb = psymbol_table->get_symbol(sid);
       if (symb->is_const())
         SYN_ERR(symb->get_name() << " is a constant" << thr());
       else
        {
         if (symb->is_variable())
          {
           DEB(cerr << "Pushing variable " << symb->get_name())
           DEB(     << " with id " << symb->get_lid())
           DEB(     << " to glob_used" << endl;)
           glob_used.push_back(symb->get_lid());
          }
        }
      }
    }
}

void dve_parser_t::expr_unary(const int unaryType)
{
 DEB(cerr << "Subexpression 'Un(E)': " << unaryType << endl;)
 add_sub_op(unaryType,1);
}

void dve_parser_t::expr_bin(const int binaryType)
{
 DEB(cerr << "Subexpression 'Bin(E1,E2)': " << binaryType << endl;)
 add_sub_op(binaryType,2);
}

void dve_parser_t::expr_state_of_process(const char * proc_name, const char * name)
{
 DEB(cerr << "Subexpression 'ID.ID': " << name << endl;)
 size_int_t fpos;
 size_int_t sid, state_sid;
 sid = psymbol_table->find_global_symbol(proc_name);
 //presume proccess names are only in the top-level frame
 if (sid != NO_ID)
  {
   //when the process is already declared, then fpos = position of process frame
   //in a process list
   //and state_sid = GID of a (possibly new) symbol in process `fpos'
   const dve_symbol_t * cp_symbol = psymbol_table->get_symbol(sid);
   if (cp_symbol->get_symbol_type() == SYM_PROCESS)
    {
     fpos = cp_symbol->get_gid();
     state_sid = psymbol_table->find_symbol(name,fpos);
     if (state_sid == NO_ID)
       //we can insert temporary symbol only into temporary process
       //while parsing system
       if (!cp_symbol->get_valid())
         if (mode==SYSTEM)
          {
           DEB(cerr << "Adding process state as temporary" << endl;)
           if (mode!=SYSTEM)
            { SYN_ERR("Parsing of source, which is not a system and undeclared"
                      " state " << name << " of process " << proc_name <<
                      " found." << thr()); }
           state_sid = add_state(name,fpos,true);
	  }
	 else
	   SYN_ERR("Process " << proc_name << " is not valid and parser is not"
                   "in a mode of parsing of system" << thr());
       else
         SYN_ERR("State " << name << " undeclared in process " << proc_name <<
                 thr());
     else
       if (psymbol_table->get_symbol(state_sid)->get_symbol_type() != SYM_STATE)
         SYN_ERR("There is referenced symbol "
                 << psymbol_table->get_symbol(state_sid)->get_name()
                 << ", but it's not a process state!" << thr());
    }
   else
    {
     SYN_ERR("There is a global variable conflicting with the process name"
             << proc_name << thr());
     state_sid = 0; //Unreachable line - it is here bacause of warnings
    }
  }
 else
  //when process is not yet declared, then we have to declare it (only
  //as temporary record => second argument = true)
  {
   DEB(cerr << "Both, process and state symbols are only temporary" << endl;)
   if (mode!=SYSTEM)
    { SYN_ERR("Parsing of source, which is not a system and undeclared"
              "process " << proc_name << " found." << thr()); }
   //fpos = position of new process frame in a frame list
   fpos = add_process(proc_name,true);
   //state_sid = GID of new state symbol in a process with ID `fpos'
   state_sid = add_state(name,fpos,true);
  }
   
 add_sub_id(state_sid,T_DOT,0);
}

void dve_parser_t::expr_var_of_process(const char * const proc_name,
                                   const char * name,
                                   const bool array)
                                   //defaultly array=false
{
 size_int_t sid;
 sid = psymbol_table->find_global_symbol(proc_name);
 //presume proccess names are only in the top-level frame
 if (sid!=NO_ID && psymbol_table->get_symbol(sid)->get_valid())
  {
   const dve_symbol_t * proc_symbol = psymbol_table->get_symbol(sid);
   register_requirement_for_being_property_process(current_process);
   size_int_t current_process_stored = current_process;
   current_process = proc_symbol->get_gid();
   //These T_FOREIGN_* identifiers will be evaluated the same way as
   //their non-foreign counterpart, but will be printed with process context
   if (array)
     expr_array_mem(name, T_FOREIGN_SQUARE_BRACKETS);
   else
     expr_id(name, T_FOREIGN_ID);
   current_process = current_process_stored;
  }
 else
  { SYN_ERR("Construct process_name->variable_name is permitted only for"
            "process_name defined before property process" << thr()); }
}

void dve_parser_t::state_decl(const char * name)
{
 DEB(cerr << "Declaration of state " << name << " in a process ")
 DEB(     << psymbol_table->get_process(current_process)->get_name() << endl;)
 if (add_state(name, current_process)==NO_ID)
   SYN_ERR(" Redeclaration of symbol " << name << psh());
}

void dve_parser_t::state_init(const char * name)
{
 DEB(cerr << "Initial state: " << name << endl;)
 size_int_t found_symb = psymbol_table->find_symbol(name,current_process);
 if (found_symb != NO_ID)
  {
   dve_process_t * dve_process_to_use = get_current_process_pointer();
   dve_process_to_use->set_initial_state(
                             psymbol_table->get_symbol(found_symb)->get_lid());
  }
 else
   SYN_ERR("Initial state " <<name<< " is not any of declared states" << thr());
}

//  void dve_parser_t::state_accept(const char * name)
//  {
//   DEB(cerr << "Accepting state: " << name << endl;)
//   size_int_t found_symb = psymbol_table->find_symbol(name,current_process);
//   if (found_symb != NO_ID)
//    {
//     DEB(cerr << "accept_state.size() = " << endl;)
//     DEB(dve_symbol_t * symb = psymbol_table->get_symbol(found_symb);)
//     DEB(cerr << symb->get_name() << ".get_lid() = ")
//     DEB(     << symb->get_lid() << endl;)
   
//     dve_process_t * dve_process_to_use = get_current_process_pointer();
//     dve_process_to_use->set_acceptance(
//                          psymbol_table->get_symbol(found_symb)->get_lid(),true);
//    }
//   else
//     SYN_ERR("Accepting state " << name << " is not any of declared states"
//             << thr());
//  }


void dve_parser_t::accept_type(int type)
{
  dve_process_t * dve_process_to_use = get_current_process_pointer();
  size_int_t groupid = 0;
  size_int_t paircomponent=1;
  std::list<size_int_t> tmp_list;
  switch (type)
    {
    case T_BUCHI:
      dve_process_to_use->set_acceptance_type_and_groups(BUCHI,1);
      tmp_list = *(accepting_sets.begin());
      for (std::list<size_int_t>::iterator i=tmp_list.begin(); i!=tmp_list.end(); i++)
	{	  
	  dve_process_to_use->set_acceptance(*i,true);
	}
      break;
    case T_GENBUCHI:
    case T_MULLER:
      dve_process_to_use->set_acceptance_type_and_groups((type==T_MULLER)?MULLER:GENBUCHI,
							 accepting_groups);
      for (std::list< std::list<size_int_t> >::iterator i=accepting_sets.begin();
	   i!=accepting_sets.end();
	   i++)
	{
	  tmp_list=*i;
	  for (std::list<size_int_t>::iterator j=tmp_list.begin(); j!=tmp_list.end(); j++)
	    {	  
	      dve_process_to_use->set_acceptance(*j,true,groupid);
	    }
	  groupid ++;
	}
      break;
    case T_RABIN:
    case T_STREETT:
      dve_process_to_use->set_acceptance_type_and_groups((type==T_RABIN)?RABIN:STREETT,
							 accepting_groups);
       for (std::list< std::list<size_int_t> >::iterator i=accepting_sets.begin();
	   i!=accepting_sets.end();
	    i++)
	 {
	   tmp_list=*i;
	   for (std::list<size_int_t>::iterator j=tmp_list.begin(); j!=tmp_list.end(); j++)
	     {	  
	       dve_process_to_use->set_acceptance(*j,true,groupid,paircomponent);
	     }
	   if (paircomponent == 1)
	     {
	        paircomponent = 2;
	     }
	   else
	     {
	       paircomponent = 1;	       
	       groupid ++;
	     }
	 }
       break;
    }
  //now we must clear data structures to be ready to process another process 
  
  for (std::list< std::list<size_int_t> >::iterator i=accepting_sets.begin();
       i!=accepting_sets.end();
       i++)
    {
      tmp_list=*i;
      tmp_list.clear();
    }
  accepting_sets.clear();
  accepting_groups = 1;
  new_accepting_set = true;
}

void dve_parser_t::state_accept(const char * name)
{
  std::list<size_int_t> new_set;
  if (new_accepting_set)
    {
      accepting_sets.push_back(new_set);
    }
  new_accepting_set = false;

  DEB(cerr << "Accepting state: " << name << endl;)
    size_int_t found_symb = psymbol_table->find_symbol(name,current_process);
  if (found_symb != NO_ID)
    {
      size_int_t local_lid;
      DEB(cerr << "accept_state.size() = " << endl;)
      DEB(dve_symbol_t * symb = psymbol_table->get_symbol(found_symb);)
      DEB(cerr << symb->get_name() << ".get_lid() = ")
      DEB(     << symb->get_lid() << endl;)
   
      local_lid=psymbol_table->get_symbol(found_symb)->get_lid();
      (accepting_sets.back()).push_back(local_lid);
    }
  else
    SYN_ERR("Accepting state " << name << " is not any of declared states"
	    << thr());
}

void dve_parser_t::state_genbuchi_muller_accept(const char * name)
{
  state_accept(name);
}

void dve_parser_t::state_rabin_streett_accept(const char * name)
{
  state_accept(name);
}
  
void dve_parser_t::accept_genbuchi_muller_set_complete()
{
  accepting_groups++;
  new_accepting_set = true;
}

void dve_parser_t::accept_rabin_streett_pair_complete()
{
  DEB(cerr<<"pair complete"<<endl;)
  accepting_groups++;
  new_accepting_set = true;
}

void dve_parser_t::accept_rabin_streett_first_complete()
{
  DEB(cerr<<"pair half-complete"<<endl;)
  new_accepting_set = true;
}

void dve_parser_t::state_commit(const char * name)
{
 DEB(cerr << "Commited state: " << name << endl;)
 size_int_t found_symb = psymbol_table->find_symbol(name,current_process);
 if (found_symb != NO_ID)
  {
   DEB(cerr << "commit_state.size() = " << endl;)
   DEB(dve_symbol_t * symb = psymbol_table->get_symbol(found_symb);)
   DEB(cerr << symb->get_name() << ".get_lid() = ")
   DEB(     << symb->get_lid() << endl;)
   
   dve_process_t * dve_process_to_use = get_current_process_pointer();
   dve_process_to_use->set_commited(
                        psymbol_table->get_symbol(found_symb)->get_lid(),true);
  }
 else
   SYN_ERR("Commited state " << name << " is not any of declared states"
           << thr());
}

void dve_parser_t::state_list_done()
{
 DEB(cerr << "State list finished" << endl;)
}


void dve_parser_t::assertion_create(const char * state_name)
{
 size_int_t aux_sid = psymbol_table->find_symbol(state_name,current_process);
 if (aux_sid != NO_ID)
  {
   size_int_t state_lid = psymbol_table->get_symbol(aux_sid)->get_lid();
   
   if (subexpr_stack.size()!=1) gerr << "Unexpected error: different coutn of expressions on a stack"
                                     << thr();
   
   //assertion expression extraction:
   dve_expression_t * assertion_expr = subexpr_stack.top();
   subexpr_stack.pop();
   
   //storing assertion to process:
   dve_process_t * process = get_current_process_pointer();
   process->add_assertion(state_lid, assertion_expr);
  }
 else
   SYN_ERR("State " << state_name << " not declared" << thr());
}


void dve_parser_t::trans_create(const char * name1, const char * name2,
   const bool has_guard, const int sync, const int effect_count)
{
 DEBFUNC(cerr << "Definition of transition: " << name1 << " -> " << name2)
 DEBFUNC(     << " with" << (has_guard?"":"OUT") << " guard, ")
 DEBFUNC(     << " with synchro " << (sync==0?"NO":sync==1?"!":"?") )
 DEBFUNC(     << " and with " << effect_count << " effects" << endl; )
 
 dve_transition_t * aux_tran;
 
 if (mode == TRANSITION)
  {
   aux_tran = dve_transition_to_fill;
   compute_glob_count_according_to_symbol_table();
  }
 else
   aux_tran = new dve_transition_t(psystem); 
 aux_tran->set_sync_expr_list_size(current_sync_expr_list.size());
 size_int_t index=0;
 for (std::list<dve_expression_t *>::iterator i=current_sync_expr_list.begin();
      i!=current_sync_expr_list.end(); ++i)
  { aux_tran->set_sync_expr_list_item(index, (*i)), index++; }
 
 if (sync!=0)
  {
   aux_tran->set_channel_gid(current_channel_gid);
   const dve_symbol_t * const p_channel =
     psymbol_table->get_channel(current_channel_gid);
   if (p_channel->get_channel_typed() && p_channel->get_channel_buffer_size())
     aux_tran->set_sync_mode(sync_mode_t(sync+2)); //buffered send/receive
   else
     aux_tran->set_sync_mode(sync_mode_t(sync)); //synchronous send/receive
  }
 else aux_tran->set_sync_mode(SYNC_NO_SYNC); //no synchronization
 for (size_int_t i=0; i!=current_effects.size(); i++)
   aux_tran->add_effect(current_effects[i]);
 current_effects.clear();
 if (glob_count) aux_tran->alloc_glob_mask(glob_count);
 for (std::list<size_int_t>::const_iterator i = glob_used.begin();
      i!=glob_used.end(); ++i)
   aux_tran->set_glob_mark((*i),true);
 //SUSPISIOUS DEB produce SIGSEGV!!! 
 //DEB(aux_tran->get_glob_mask().DBG_print();)
 //SUSPISIOUS DEB produce SIGSEGV!!! 
 aux_tran->set_process_gid(current_process);

 if (name1 == 0)
  {
   if (current_first_state_gid == NO_ID)
     SYN_ERR("First state omitted." << thr());
  }
 else
  {
   size_int_t aux_sid = psymbol_table->find_symbol(name1,current_process);
   if (aux_sid == NO_ID)
     SYN_ERR("First state not declared" << thr());
   else
     current_first_state_gid = psymbol_table->get_symbol(aux_sid)->get_gid();
  }
 DEB(cerr << "OK, first state found" << endl;)
 aux_tran->set_state1_gid(current_first_state_gid);

 size_int_t aux_state2_gid;
 size_int_t aux_state2_sid = psymbol_table->find_symbol(name2,current_process);
 if (aux_state2_sid == NO_ID)
  {
   SYN_ERR("Second state not declared" << thr());
   aux_state2_gid=0; //unreachable line - because of warnings
  }
 else
   aux_state2_gid = psymbol_table->get_symbol(aux_state2_sid)->get_gid();
 
 DEB(cerr << "OK, second state found" << endl;)
 aux_tran->set_state2_gid(aux_state2_gid);
 
 if (!has_guard) aux_tran->set_guard(0);
 else aux_tran->set_guard(current_guard);
 
 aux_tran->set_source_pos(size_int_t(firstline),size_int_t(firstcol),
                          size_int_t(lastline),size_int_t(lastcol));
 
 aux_tran->set_valid(true);
 
 glob_used.clear();
 if (mode!=TRANSITION) add_transition(aux_tran);
 current_guard = 0;
 current_channel_gid = 0;
 current_sync_expr_list.clear();
 DEBFUNC(cerr << "OK, transition created" << endl;)
}

void dve_parser_t::trans_effect_list_begin() {
 if (!subexpr_stack.empty())
   terr << "Unexpected error: stack of expressions is not empty, when"
           "parsing a new expression" << thr();
 if (save_glob_used)
   terr << "Unexpected error: save_glob_used should be false" << thr();
 save_glob_used = true;
}

void dve_parser_t::trans_effect_list_end()
{ save_glob_used = false; }

void dve_parser_t::trans_effect_list_cancel() {
 if (!subexpr_stack.empty()) { delete subexpr_stack.top();
                               subexpr_stack.pop();
                             DEB(cerr << "Poping from subexpr_stack" << endl;) }
 save_glob_used = false;
}

void dve_parser_t::trans_effect_part() {
 if (!subexpr_stack.empty()) {
  current_effects.push_back(subexpr_stack.top());
  subexpr_stack.pop();
  DEB(cerr << "Poping from subexpr_stack" << endl;)
 }
 else
   terr << "Unexpected error - stack of subexpressions is empty" << thr();
}

void dve_parser_t::trans_sync(const char * name, const int sync,
                          const bool sync_val) {
 DEBFUNC(cerr << "END of taking expression - to synchronisation" << endl;)
 DEBFUNC(cerr << "BEGIN of trans_sync called with name " << name << endl;)
 
 size_int_t sid;
 
 /* sid = id of channel symbol */
 sid = psymbol_table->find_global_symbol(name);
 if (sid == NO_ID)
   SYN_ERR("You have not declared channel " << name << thr());
 dve_symbol_t * p_symbol = psymbol_table->get_symbol(sid);
 if (p_symbol->get_symbol_type() != SYM_CHANNEL)
   SYN_ERR(name << " is not a channel" << thr());
 
 if (sync_val)
  {
   if (!subexpr_stack.empty())
    { //we have to recognize that the process assigns a global
      //variable in a synchronisation
     if (!current_expression_list.empty())
       terr << "Unexpected error - expression list is not empty!" << thr();
     current_expression_list.push_back(subexpr_stack.top());
     subexpr_stack.pop();
     DEB(cerr << "Poping from subexpr_stack" << endl;)
    }
   if (!current_expression_list.empty())
    {
     current_sync_expr_list.swap(current_expression_list);
     if (p_symbol->get_channel_typed() &&
         current_sync_expr_list.size()!=p_symbol->get_channel_type_list_size())
       SYN_ERR("Channel " << name << " requires exactly "
               << p_symbol->get_channel_type_list_size()
               << " values to transmit simultaneously (only "
               << current_sync_expr_list.size() << " values given)." << thr());
     if (sync==2) //=>receive
      {
       for (std::list<dve_expression_t*>::iterator i =
               current_sync_expr_list.begin();
            i!=current_sync_expr_list.end(); ++i)
        {
         const dve_expression_t * s = (*i);
         if ((s->get_operator()==T_ID || s->get_operator()==T_SQUARE_BRACKETS)
             && (!psymbol_table->get_variable(s->get_ident_gid())->is_const()))
          {
           if (save_glob_used)
            {
             dve_symbol_t * symb = 
               psymbol_table->get_variable(s->get_ident_gid());
             if (symb->get_process_gid()==NO_ID)
              {
               if (symb->is_const())
                 SYN_ERR(symb->get_name() << " is a constant" << thr());
               else
                 if (symb->is_variable())
                  {
                   DEB(cerr << "Pushing variable " << symb->get_name())
                   DEB(     << " with id " << symb->get_lid())
                   DEB(     << " to glob_used" << endl;)
                   glob_used.push_back(symb->get_lid());
                  }
              }
            }
          }
         else
           SYN_ERR(s->to_string() << " is not an l-value" << thr());
        }
      }
    }
   else
    gerr << "Unexpected error: current list of expression is empty" << thr();
  }
 else
  { current_sync_expr_list.clear(); }
 
 current_channel_gid = p_symbol->get_gid();
 

 if (!p_symbol->get_channel_typed())
  {
   /* checking whether untyped channel passes the same count of values as before
    * (given by 1st usage)*/
   if (p_symbol->get_channel_item_count() == dve_symbol_t::CHANNEL_UNUSED)
     p_symbol->set_channel_item_count(current_sync_expr_list.size());
   else if (p_symbol->get_channel_item_count()!=current_sync_expr_list.size())
     SYN_ERR("Untyped channel " << name << " have been used to transmit " <<
             p_symbol->get_channel_item_count() << " values in past. " <<
             "But here it transmits " << current_sync_expr_list.size() <<
             " values." << thr());
   //else everything is all right
  }
 DEBFUNC(cerr << "END of trans_sync" << endl;)
}


void dve_parser_t::trans_guard_expr() {
 DEBFUNC(cerr << "END of taking expression - to guard" << endl;)
 current_guard =subexpr_stack.top();
 subexpr_stack.pop();
 DEB(cerr << "Poping from subexpr_stack" << endl;)
}

void dve_parser_t::channel_decl(const char * name)
{
 DEB(cerr << "Declaration of channel " << name << endl;)
 add_channel(name);
}

void dve_parser_t::typed_channel_decl(const char * name, const int buffer_size)
{
 add_channel(name,true,buffer_size);
 
}

void dve_parser_t::type_list_add(const int type_nbr)
{
 current_type_list.push_back((type_nbr==T_BYTE)? VAR_BYTE : VAR_INT);
}

void dve_parser_t::type_list_clear()
{
 current_type_list.clear();
}


void dve_parser_t::expression_list_store()
{
 if (!subexpr_stack.empty()) {
  current_expression_list.push_back(subexpr_stack.top());
  subexpr_stack.pop();
  DEB(cerr << "Poping from subexpr_stack" << endl;)
 }
 else
   terr << "Unexpected error - stack of subexpressions is empty" << thr();
}

void dve_parser_t::system_synchronicity(const int sync, const bool prop)
{
 DEBCHECK(if (mode!=SYSTEM) gerr << "Unexpected: not parsing a systen"<<thr();)
 dve_system_to_fill->set_system_synchro(system_synchronicity_t(sync));
 dve_system_to_fill->set_with_property(prop);
}

void dve_parser_t::system_property(const char * name)
{
 DEBCHECK(if (mode!=SYSTEM) gerr << "Unexpected: not parsing a systen"<<thr();)
 size_int_t sid;
 sid = psymbol_table->find_global_symbol(name);
 if (sid == NO_ID)
   SYN_ERR("There is no process called " << name << thr());

 dve_symbol_t * symb = psymbol_table->get_symbol(sid);
 if (symb->get_symbol_type() == SYM_PROCESS)
  {
   dve_system_to_fill->set_property_gid(symb->get_gid());
   check_restrictions_put_on_property();
  }
 else 
   SYN_ERR("There is a global variable conflicting with the process name"
           << name << thr());
}

void dve_parser_t::check_restrictions_put_on_property()
{
 process_t * prop = dve_system_to_fill->get_property_process();
 for (size_int_t i=0; i!=prop->get_trans_count(); ++i)
  {
   dve_transition_t * dve_trans =
     dynamic_cast<dve_transition_t*>(prop->get_transition(i));
   if (!dve_trans) terr << "Unexpected error when casting transitions" << thr();
   
   if (dve_trans->get_sync_mode()!=SYNC_NO_SYNC)
    {
     if (dve_trans->get_sync_mode()==SYNC_EXCLAIM)
       SYN_ERR("Property process must not contain sychronization with '!' (use '?' instread" << thr());
     else if (dve_trans->get_sync_expr_list_size()!=0)
       SYN_ERR("Property process cannot receive values. It can only synchronize using commands like 'sync mychannel?' without value passing" << thr());
     else if (psymbol_table->get_channel(dve_trans->get_channel_gid())->get_channel_typed())
       SYN_ERR("Property process cannot use typed channels" << thr());
    }
   
   //Assignment to a global variable is not permitted:
   size_int_t effect_count = dve_trans->get_effect_count();
   for (size_int_t j=0; j!=effect_count; j++)
    {
     dve_expression_t * effect = dve_trans->get_effect(j);
     const dve_expression_t * assign_to = effect->left();
     const dve_symbol_t * var =
                      psymbol_table->get_variable(assign_to->get_ident_gid());
     if (var->get_process_gid() == NO_ID) //if a variable is global...
      {
       SYN_ERR("Property process cannot assign to global variables "
               "(but is does here: " <<
               effect->get_source_first_line() << ":" <<
               effect->get_source_first_col() << "-" <<
               effect->get_source_last_line() << ":" <<
               effect->get_source_last_col() << ")" << thr());
      }
    }
  }
}

////METHODS for work with processes and channels:
bool dve_parser_t::add_channel(const char * name, const bool typed,
                               const int buffer_size)
//defaultly `typed' = false, `buffer_size' = 0
 {
  if (psymbol_table->found_global_symbol(name)) return false;
  dve_symbol_t * aux_sym =
    new dve_symbol_t(SYM_CHANNEL,name,strlen(name));
  aux_sym->set_process_gid(NO_ID);
  if (typed)
   {
    aux_sym->set_channel_typed(true);
    aux_sym->set_channel_type_list_size(current_type_list.size());
    size_int_t index = 0;
    for (std::list<dve_var_type_t>::iterator i=current_type_list.begin();
         i!=current_type_list.end(); ++i)
      { aux_sym->set_channel_type_list_item(index,(*i)); index++; }
    aux_sym->set_channel_buffer_size(buffer_size);
   }
  else aux_sym->set_channel_typed(false);
  
  aux_sym->set_valid(true);
  psymbol_table->add_channel(aux_sym);
  
  DEB(cerr << "Channel " << name << " has an id = " <<)
  DEB(aux_sym->get_gid() << endl;)
  return true;
 }

size_int_t dve_parser_t::add_process(const char * name,
                                  const bool only_temporary)
 //only_temporary = false by default 
 {
  size_int_t sid = psymbol_table->find_global_symbol(name);
  if (sid!=NO_ID)
   {
    dve_symbol_t * p_symbol = psymbol_table->get_symbol(sid);
    if (!p_symbol->get_valid())
     {
      p_symbol->set_valid(true);
      //if only temporary, then returns its id
      return p_symbol->get_gid();
     }
    else
     return NO_ID; //unsuccessful creation of process (NO_ID cannot be a process id)
   }
  else
   {
    DEB(cerr << "State counter set to zero." << endl;)
    dve_symbol_t * aux_sym =
      new dve_symbol_t(SYM_PROCESS,name,strlen(name));
    aux_sym->set_process_gid(NO_ID);

    aux_sym->set_valid(!only_temporary);
    psymbol_table->add_process(aux_sym);
    if (mode==SYSTEM)
     {
      dve_process_t * new_process;
      if (probabilistic)
       {
        prob_system_t * prob_system = dynamic_cast<prob_system_t*>(psystem);
        if (prob_system) new_process = new dve_prob_process_t(prob_system);
        else terr << "Unexpected error: system is not probabilistic" << thr();
       }
      else
        new_process = new dve_process_t(psystem);
      new_process->set_gid(aux_sym->get_gid());
      dve_system_to_fill->add_process(new_process);
      return (dve_system_to_fill->get_process_count()-1);
     }
    else if (mode==PROCESS)
     {
      //we do not do anything, content of the process is set during further
      //parsing
      
      compute_glob_count_according_to_symbol_table();

      dve_process_to_fill->set_gid(aux_sym->get_gid());
      return (psymbol_table->get_process_count()-1);
     }
    else { gerr << "Unexpected error: Not parsing a system or process."<<thr();
           return 0; /* unreachable line - because of warnings*/}
   }
 }

bool dve_parser_t::add_variable(
         const char * name,  const size_int_t proc_id,
	 const bool initialized, const bool isvector)
         //isvector is defaultly false
 {
  DEB(cerr << "Symbol is " << ((proc_id==NO_ID)?"":" not "))
  DEB(     << "global." << endl;)
  if (((proc_id==NO_ID)&&psymbol_table->found_global_symbol(name)) ||
      ((proc_id!=NO_ID)&&psymbol_table->found_symbol(name,proc_id)))
    return false;
  DEB(cerr << "Saved token " << name << endl;)
  dve_symbol_t * aux_sym;
  
  //Local ID is initialized to 0, but then it is set by set_lid()
  aux_sym =
    new dve_symbol_t(SYM_VARIABLE,name,strlen(name));
  aux_sym->set_vector(isvector);
  
  aux_sym->set_process_gid(proc_id);
  
  if (initialized)
    if (isvector)
     {
      aux_sym->create_init_expr_field();
      aux_sym->get_init_expr_field()->swap(current_field);
     }
    else
     {
      aux_sym->set_init_expr(subexpr_stack.top());
      subexpr_stack.pop();
      DEB(cerr << "Poping from subexpr_stack" << endl;)
     }
  else { aux_sym->no_init_expr_field(); aux_sym->set_init_expr(0); }

  aux_sym->set_const(current_const);

  aux_sym->set_var_type(current_vartype==T_BYTE?
                        VAR_BYTE: VAR_INT );

  if (isvector)
    aux_sym->set_vector_size(current_bound);
  else
    aux_sym->set_vector_size(0);
    
  aux_sym->set_valid(true);
  psymbol_table->add_variable(aux_sym);
  
  if (proc_id!=NO_ID)
   {
    dve_process_t * proc_to_use;
    if (proc_id!=current_process) //function get_current_process_pointer
                                  //needs proc_id==current_process
     {
      if (mode==SYSTEM)
        proc_to_use = dynamic_cast<dve_process_t *>(dve_system_to_fill->get_process(proc_id));
      else
       { gerr << "Unexpected error: mode SYSTEM in parser expected."<<thr();
         proc_to_use = 0; /* not reachable command */ }
     }
    else
      proc_to_use = get_current_process_pointer();
    
    aux_sym->set_lid(proc_to_use->get_variable_count());
    proc_to_use->add_variable(aux_sym->get_gid());
   }
  else
   {
    aux_sym->set_lid(glob_count);
    //variable will be inserted in a consolidate() function
   }
  if (proc_id==NO_ID) glob_count++;
  
  return true;
 }

size_int_t dve_parser_t::add_state
  (const char * const name, const size_int_t proc_id,
   const bool only_temporary)
            //only_temporary is defaultly false
{
 DEBFUNC(cerr << "BEGIN of function add_state()" << endl;)
 size_int_t sid = psymbol_table->find_symbol(name,proc_id);
 if (sid!=NO_ID)
  { //there already exists an symbol of this name
   DEB(cerr << "There alredy exists symbol of name " << name << endl;)
   dve_symbol_t * p_symbol = psymbol_table->get_symbol(sid);
   if (!p_symbol->get_valid())
    {
     p_symbol->set_valid(true);
     //if only temporary, then returns its symbol id
     return sid;
    }
   else
     return NO_ID; //redeclaration of state => error/warning
  }
 else //symbol does not exist
  {
   if (psymbol_table->found_symbol(name, proc_id)) return false;
   DEB(cerr << "State has a name " << name << endl;)
   dve_symbol_t * aux_sym =
     new dve_symbol_t(SYM_STATE,name,strlen(name));
   aux_sym->set_symbol_type(SYM_STATE);
   aux_sym->set_process_gid(proc_id);
   
   aux_sym->set_valid(!only_temporary);
   psymbol_table->add_state(aux_sym);
   
   if (proc_id!=NO_ID)
    {
     if (proc_id!=current_process) //function get_current_process_pointer
                                   //needs proc_id==current_process
       if (mode==SYSTEM)
        {
         dve_process_t * process_where_to_add_state =
           dynamic_cast<dve_process_t *>(dve_system_to_fill->get_process(proc_id));
         process_where_to_add_state->add_state(aux_sym->get_gid());
        }
       else gerr << "Unexpected error: mode SYSTEM in parser expected."<<thr();
     else
      {
       dve_process_t * proc_to_use = get_current_process_pointer();
       proc_to_use->add_state(aux_sym->get_gid());
      }
    }
   else
     terr << "Unexpected: Declaration of state outside of a process!" << thr();
 
   DEB(cerr << "State has LID " << aux_sym->get_lid()<< endl;)
   
   return aux_sym->get_sid();
  }

 DEBFUNC(cerr << "END of function add_state()" << endl;)
}

void dve_parser_t::add_transition(dve_transition_t * const transition)
{
 if (mode==PROB_TRANSITION)
  {
   prob_trans_trans.push_back(transition);
  }
 else
  {
   dve_process_t * proc_to_use = get_current_process_pointer();
   proc_to_use->add_transition(transition);
  }
}

void dve_parser_t::add_prob_transition(dve_prob_transition_t * const prob_trans)
{
 dve_process_t * proc_to_use = get_current_process_pointer();
 dve_prob_process_t * prob_process =
                              dynamic_cast<dve_prob_process_t*>(proc_to_use);
 if (prob_process==0)
   terr << "Unexpected error: process is not probabilistic" << thr();
 
 prob_process->add_prob_transition(prob_trans);
}
    
    
void dve_parser_t::add_sub_op(int expr_op, short int ar)
 {
  DEBFUNC(cerr << "BEGIN of add_sub_op" << endl;)
  dve_expression_t *aux_expr =
    new dve_expression_t(expr_op, subexpr_stack, ar, psystem);

  aux_expr->set_source_pos(size_int_t(firstline),size_int_t(firstcol),
                           size_int_t(lastline),size_int_t(lastcol));
  
  subexpr_stack.push(aux_expr);
  DEB(cerr << "Changing subexpr_stack - size: "<<subexpr_stack.size()<< endl;)
  DEBFUNC(cerr << "END of add_sub_op" << endl;)
 }
 
void dve_parser_t::add_sub_id(size_int_t sid, int expr_op, short int ar)
 {
  DEBFUNC(cerr << "BEGIN of add_sub_id" << endl;)
  dve_expression_t *aux_expr =
    new dve_expression_t(expr_op,subexpr_stack,ar,psystem);
  
  aux_expr->set_source_pos(size_int_t(firstline),size_int_t(firstcol),
                           size_int_t(lastline),size_int_t(lastcol));
  
  aux_expr->set_ident_gid(psymbol_table->get_symbol(sid)->get_gid());
  subexpr_stack.push(aux_expr);
  DEB(cerr << "Changing subexpr_stack - size: "<<subexpr_stack.size()<< endl;)
  DEBFUNC(cerr << "END of add_sub_id" << endl;)
 }
 
void dve_parser_t::add_sub_num(int nbr)
 {
  DEBFUNC(cerr << "BEGIN of add_sub_num" << endl;)
  dve_expression_t *aux_expr=new dve_expression_t(T_NAT,subexpr_stack,0,psystem);
  
  aux_expr->set_source_pos(size_int_t(firstline),size_int_t(firstcol),
                           size_int_t(lastline),size_int_t(lastcol));
  
  aux_expr->set_value(nbr);
  subexpr_stack.push(aux_expr);
  DEB(cerr << "Changing subexpr_stack - size: "<<subexpr_stack.size()<< endl;)
  DEBFUNC(cerr << "END of add_sub_num" << endl;)
 }

dve_expression_t * dve_parser_t::get_expression() const
{
 if (subexpr_stack.size()!=1)
   if (subexpr_stack.size()==0)
    {
     terr << "There is not stored any expression" << thr();
     return 0;
    }
   else
    {
     terr << "There are stored" << subexpr_stack.size() << " expression "
          << " instead of the single one" << thr();
     return 0;
    }
 else
   return subexpr_stack.top();
}

void dve_parser_t::set_fpos(int line, int col)
{
 firstline = line;
 firstcol = col;
}

void dve_parser_t::set_lpos(int line, int col)
{
 lastline = line;
 lastcol = col;
}

void dve_parser_t::compute_glob_count_according_to_symbol_table()
{
 glob_count = 0;
 size_int_t var_count = psymbol_table->get_variable_count();
 for (size_int_t i = 0; i!=var_count; ++i)
  {
   if (psymbol_table->get_variable(i)->get_process_gid()==NO_ID)
     glob_count++;
  }
}

void dve_parser_t::register_requirement_for_being_property_process(
                                                   const size_int_t proc_id)
{
 if (prop_req.is_set)
  {
   if (prop_req.process != proc_id) //this is syntax error - 2 prop. processes
                                    //are impossible
    {
     SYN_ERR("Both processes " <<psymbol_table->get_process(proc_id)->get_name()
             <<" and "<<psymbol_table->get_process(prop_req.process)->get_name()
             <<" use construct process_name.(Expression),"
             <<" but only one of them can use it, because this construct is"
             " permitted only in a property processes and there can be at most"
             " 1 property process." << thr());
    }
     
   //else we do not want to change anything
  }
 else //no request registered during current parsing yet
  {
   prop_req.is_set = true;
   prop_req.process = proc_id;
   prop_req.l1 = firstline; prop_req.l2 = lastline;
   prop_req.c1 = firstcol; prop_req.c2 = lastcol;
  }
}

void dve_parser_t::prob_trans_create(const char * name)
{
 if (probabilistic)
  {
   dve_prob_transition_t * prob_trans;
   if (mode==PROB_TRANSITION)
    {
     dve_prob_transition_to_fill->set_trans_count(prob_trans_parts.size());
    }
   else
    {
     prob_system_t * prob_system = dynamic_cast<prob_system_t*>(psystem);
     if (prob_system) prob_trans = new dve_prob_transition_t(prob_system);
     prob_trans->set_trans_count(prob_trans_parts.size());
    }
   size_int_t j=0;
   
   for (prob_trans_part_iter_t i=prob_trans_parts.begin();
        i!=prob_trans_parts.end(); ++i, ++j)
    {
     trans_create(name, i->name.c_str(), false, SYNC_NO_SYNC, 0);
     if (mode!=PROB_TRANSITION)
      {
       dve_process_t * process = get_current_process_pointer();
       if (process->get_trans_count()>0)
         prob_trans->set_transition_and_weight(j,
                         process->get_transition(process->get_trans_count()-1),
                         i->weight);
       else
         terr << "Unexpected error: no transition added" << thr();
      }
    }
   
   //if creating standalone prob. transition, then use a content
   //of prob_trans_trans:
   j=0;
   if (mode==PROB_TRANSITION)
    {
     prob_trans_trans_iter_t k=prob_trans_trans.begin();
     for (prob_trans_part_iter_t i=prob_trans_parts.begin();
        i!=prob_trans_parts.end(); ++i, ++j, ++k)
     dve_prob_transition_to_fill->set_transition_and_weight(j, (*k),
                                                            i->weight);
    }
   
    
   prob_trans_parts.clear();
   prob_trans_trans.clear();
   if (mode!=PROB_TRANSITION) add_prob_transition(prob_trans);
 }
 else SYN_ERR("Probabilistic transition in system, which is not probalistic" << thr());
}

void dve_parser_t::prob_transition_part(const char * name, const int weight)
{
 prob_trans_part_t prob_trans_part;
 prob_trans_part.name = name;
 prob_trans_part.weight = weight;
 prob_trans_parts.push_back(prob_trans_part);
}

