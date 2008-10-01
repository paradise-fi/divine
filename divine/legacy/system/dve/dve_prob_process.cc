#include "system/dve/dve_prob_process.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

dve_prob_process_t::~dve_prob_process_t()
{
 for (size_int_t i=0; i!=prob_transitions.size(); ++i)
   delete prob_transitions[i];
}

prob_transition_t * dve_prob_process_t::get_prob_transition
                                     (const size_int_t transition_index)
{ return prob_transitions[transition_index]; }

const prob_transition_t * dve_prob_process_t::get_prob_transition
                                (const size_int_t transition_index) const
{ return prob_transitions[transition_index]; }

dve_parser_t dve_prob_process_t::prob_proc_parser(*pproc_terr, (dve_prob_process_t *)(0));

void dve_prob_process_t::add_prob_transition
           (prob_transition_t * const prob_trans)
{
 prob_trans->set_lid(prob_transitions.size());
 dve_prob_transition_t * dve_prob_trans =
      dynamic_cast<dve_prob_transition_t *>(prob_trans);
 if (dve_prob_trans)
   prob_transitions.push_back(dve_prob_trans);
 else
   (*process_t::pproc_terr) << "Probabilistic transition is not of DVE type"
                            << thr();
}

void dve_prob_process_t::remove_prob_transition(const size_int_t transition_index)
{
 prob_transitions[transition_index]->set_valid(false);
}

std::string dve_prob_process_t::to_string() const
{
 std::ostringstream ostr;
 write(ostr);
 return ostr.str();
}

void dve_prob_process_t::write(std::ostream & ostr) const
{
 dve_symbol_table_t * parent_table = get_symbol_table();
 if (parent_table)
  {
   ostr << "process " << parent_table->get_process(gid)->get_name() << " {"
        << endl;
   write_declarations(ostr);
   if (get_trans_count())
    {
     ostr << "trans" << endl;
     bool prob_trans_written[get_prob_trans_count()];
     size_int_t prob_trans_lids[get_trans_count()];
     for (size_int_t i=0; i!=get_trans_count(); i++)
       prob_trans_lids[i] = NO_ID;
     for (size_int_t i=0; i!=get_prob_trans_count(); i++)
      {
       prob_trans_written[i] = false; //initialization to false
       for (size_int_t j=0; j!=get_prob_transition(i)->get_trans_count(); ++j)
        prob_trans_lids[get_prob_transition(i)->get_transition(j)->get_lid()]=i;
      }
     for (size_int_t i=0; i!=get_trans_count(); i++)
      {
       size_int_t prob_trans_lid = prob_trans_lids[i];
       if (prob_trans_lid==NO_ID)
        {
         if (i!=0) ostr << "," << endl;
         get_transition(i)->write(ostr);
        }
       else if (!prob_trans_written[prob_trans_lid])
        {
         if (i!=0) ostr << "," << endl;
         prob_trans_written[prob_trans_lid] = true;
         get_prob_transition(prob_trans_lid)->write(ostr);
        }
      }
     ostr << ";" << endl;
    }
   ostr << "}" << endl;
  }
}

int dve_prob_process_t::read(std::istream & istr)
{
 return read_using_given_parser(istr,prob_proc_parser);
}

int dve_prob_process_t::from_string(std::string & proc_str)
{
 std::istringstream str_stream(proc_str);
 return read(str_stream);
}

