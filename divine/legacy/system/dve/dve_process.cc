#include "common/array.hh"
#include "system/dve/dve_transition.hh"
#include "system/dve/dve_process.hh"
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "system/dve/syntax_analysis/dve_parser.hh"
#include "system/dve/syntax_analysis/dve_grammar.hh"
#include "common/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

using std::string;

dve_parser_t dve_process_t::proc_parser(*pproc_terr, (dve_process_t *)(0));

////METHODS of dve_process_t
void dve_process_t::intitialize_dve_specific_parts()
{
  //DEBFUNC(cerr << "Creation of process GID:" << gid_arg << endl;)
 valid=false;
 accepting1 = 0;
 accepting2 = 0;
 transitions.set_alloc_step(DVE_PROCESS_ALLOC_STEP);
//   accepting1[0].set_alloc_step(DVE_PROCESS_ALLOC_STEP);
//   accepting2[0].set_alloc_step(DVE_PROCESS_ALLOC_STEP);
 trans_by_sync[0].set_alloc_step(DVE_PROCESS_ALLOC_STEP);
 trans_by_sync[1].set_alloc_step(DVE_PROCESS_ALLOC_STEP);
 trans_by_sync[2].set_alloc_step(DVE_PROCESS_ALLOC_STEP);
 assertions.set_alloc_step(DVE_PROCESS_ALLOC_STEP);
 DEBFUNC(cerr << "End of creation of process" << endl;)
}

dve_process_t::dve_process_t(): process_t()
{ intitialize_dve_specific_parts(); }

dve_process_t::dve_process_t(system_t * const system): process_t(system)
{ intitialize_dve_specific_parts(); }

dve_process_t::~dve_process_t()
{
 DEB(cerr << "BEGIN of destructor of dve_process_t" << endl;)
 for (size_int_t i=0; i!=transitions.size(); ++i) delete transitions[i];
 for (size_int_t i=0; i!=assertions.size(); ++i)
  {
   for (size_int_t j=0; j!=assertions[i]->size(); ++j)
     delete (*assertions[i])[j];
   delete assertions[i];
  }
 if (accepting1) delete[] accepting1;
 if (accepting2) delete[] accepting2;  
 DEB(cerr << "END of destructor of dve_process_t" << endl;)
}

void dve_process_t::add_variable(const size_int_t var_gid)
{
 var_gids.push_back(var_gid);
}

void dve_process_t::add_state(const size_int_t state_gid)
{
  // accepting.push_back(false);
 commited.push_back(false);
 assertions.extend(1);
 assertions.back() = new array_t<dve_expression_t *>;
 get_symbol_table()->get_state(state_gid)->set_lid(state_gids.size());
 state_gids.push_back(state_gid);
}

void dve_process_t::add_transition(transition_t * const transition)
 {
  dve_transition_t * dve_transition = dynamic_cast<dve_transition_t *>(transition);
  const sync_mode_t sync_mode = dve_transition->get_sync_mode();
  dve_transition->set_lid(transitions.size());
  dve_transition->set_partial_id(trans_by_sync[sync_mode].size());
  
  transitions.push_back(dve_transition);
  trans_by_sync[sync_mode].push_back(dve_transition);
 }

void dve_process_t::add_assertion(size_int_t state_lid,
                                  dve_expression_t * const assert_expr)
{
 assertions[state_lid]->extend(1);
 assertions[state_lid]->back() = assert_expr;
}

string dve_process_t::to_string() const
{
 std::ostringstream ostr;
 write(ostr);
 return ostr.str();
}

int dve_process_t::from_string(std::string & proc_str)
{
 std::istringstream str_stream(proc_str);
 DEB(cerr << "Creating process by parsing " << proc_str << endl;)
 return read(str_stream);
}

int dve_process_t::read(std::istream & istr)
{
 return read_using_given_parser(istr,proc_parser);
}

int dve_process_t::read_using_given_parser(std::istream & istr,
                                           dve_parser_t & parser)
{
 for (size_int_t i=0; i!=transitions.size(); ++i) delete transitions[i];
 transitions.clear();
 var_gids.clear();
 state_gids.clear();
 if (accepting1) delete[] accepting1;
 if (accepting2) delete[] accepting2;
 accepting_type=NONE;
 accepting_groups=0;
 commited.clear();
 for (size_int_t i=0; i!=3; ++i) trans_by_sync[i].clear();
 dve_proc_init_parsing(&parser, pproc_terr, istr);
 parser.set_dve_process_to_fill(this);
 try
  { dve_ppparse(); }
 catch (ERR_throw_t & err)
  { return err.id; } //returns non-zero value
 return 0; //returns zero in case of successful parsing
}


void dve_process_t::write_declarations(std::ostream & ostr) const
{
 dve_symbol_table_t * parent_table = get_symbol_table();
 if (parent_table)
  {
   for (size_int_t i=0; i!=var_gids.size(); i++)
    {
     dve_symbol_t * var = parent_table->get_variable(var_gids[i]);
     if (var->is_const()) ostr << "const ";
     if (var->is_byte()) ostr << "byte ";
     else ostr << "int ";
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
    
   if (state_gids.size())
     ostr << "state ";
    {
     for (size_int_t i=0; i!=state_gids.size(); i++)
      {
       ostr << parent_table->get_state(state_gids[i])->get_name();
       if (i!=(state_gids.size()-1)) ostr << ", ";
       else ostr << ";" << endl;
      }
     ostr << "init " << parent_table->get_state(state_gids[initial_state])->
                                                                     get_name()
          << ";" << endl;


     if (accepting_type!=NONE)
       {
	 ostr << "accept ";
	 switch (accepting_type)
	   {
	   case BUCHI:
	     ostr <<"buchi ";
	     for (size_int_t i=0; i!=accepting1[0].size(); i++)
	       {
		 if ((accepting1[0])[i])
		   {
		     ostr << parent_table->get_state(state_gids[i])->get_name();
		     if (i!=(accepting1[0].size()-1)) ostr << ", ";
		     else ostr << ";" << endl;
		   }
	       }
	     break;
	   case GENBUCHI:
	   case MULLER:
	     if (accepting_type==GENBUCHI)
	       ostr <<"genbuchi ";
	     else
	       ostr <<"muller ";
	     for (size_int_t j=0; j!=accepting_groups; j++)
	       {
		 ostr <<"(";
		 for (size_int_t i=0; i!=accepting1[j].size(); i++)
		   {
		     if ((accepting1[j])[i])
		       {
			 ostr << parent_table->get_state(state_gids[i])->get_name();
			 if (i!=(accepting1[j].size()-1)) ostr << ",";
		       }
		   }
		 ostr << ")" << endl;
		 if (j!=(accepting_groups-1))
		   ostr << ", ";
		 else
		   ostr << ";";		   		 
	       }
	     break;
	   case RABIN:
	   case STREETT:
	     ostr <<"rabin ";
	     if (accepting_type==RABIN)
	       ostr <<"rabin ";
	     else
	       ostr <<"streett ";
	     for (size_int_t j=0; j!=accepting_groups; j++)
	       {
		 ostr <<"(";
		 for (size_int_t i=0; i!=accepting1[j].size(); i++)
		   {
		     if ((accepting1[j])[i])
		       {
			 ostr << parent_table->get_state(state_gids[i])->get_name();
			 if (i!=(accepting1[j].size()-1)) ostr << ",";
		       }
		   }
		 ostr <<";";
		 for (size_int_t i=0; i!=accepting2[j].size(); i++)
		   {
		     if ((accepting2[j])[i])
		       {
			 ostr << parent_table->get_state(state_gids[i])->get_name();
			 if (i!=(accepting2[j].size()-1)) ostr << ",";
		       }
		   }
		 ostr << ")" << endl;
		 if (j!=(accepting_groups-1))
		   ostr << ", ";
		 else
		   ostr << ";";		   		 
	       }
	     break;
	   case NONE:
	     break;
	   };

       }

     bool have_commited = false;
     for (size_int_t i=0; i!=commited.size(); i++)
       if (commited[i])
        {
         if (!have_commited) { ostr << "commit "; have_commited = true; }
         ostr << parent_table->get_state(state_gids[i])->get_name();
         if (i!=(commited.size()-1)) ostr << ", ";
         else ostr << ";" << endl;
        }
    }
  }
}

void dve_process_t::write(std::ostream & ostr) const
{
 dve_symbol_table_t * parent_table = get_symbol_table();
 if (parent_table)
  {
   ostr << "process " << parent_table->get_process(gid)->get_name() << " {"
        << endl;
   write_declarations(ostr);
   if (transitions.size()) { ostr << "trans" << endl; }
   for (size_int_t i=0; i!=transitions.size(); i++)
    {
     transitions[i]->write(ostr);
     if (i!=(transitions.size()-1)) ostr << "," << endl;
     else ostr << ";" << endl;
    }
   ostr << "}" << endl;
  }
}

dve_symbol_table_t * dve_process_t::get_symbol_table() const
{
 dve_system_t * dve_system;
 if ((dve_system=dynamic_cast<dve_system_t *>(parent_system)))
   return dve_system->get_symbol_table();
 else return 0;
}



