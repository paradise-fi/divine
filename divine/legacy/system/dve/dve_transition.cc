#include "common/array.hh"
#include "system/dve/dve_transition.hh"
#include "system/dve/dve_expression.hh"
#include "system/dve/syntax_analysis/dve_parser.hh"
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "system/dve/syntax_analysis/dve_grammar.hh"
#include "common/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

using std::string;

////METHODS of dve_transition_t

dve_parser_t dve_transition_t::trans_parser(*ptrans_terr, (dve_transition_t *)(0));

dve_transition_t::~dve_transition_t()
{
 DEB(cerr << "BEGIN of destructor of dve_transition_t" << endl;)
 if (effects.size())
   for (effects_container_t::iterator i = effects.begin();
        i != effects.end(); ++i)
     delete (*i);
 if (guard) delete guard;
 for (size_int_t i=0; i!=sync_exprs.size(); ++i) delete sync_exprs[i];
 DEB(cerr << "END of destructor of dve_transition_t" << endl;)
}

void dve_transition_t::set_state1_gid(const size_int_t state_gid)
{
 DEBAUX(cerr << state_gid << " " << get_symbol_table()->get_state_count() << endl;)
 state1_gid = state_gid;
 state1_lid  = get_symbol_table()->get_state(state_gid)->get_lid();
}

void dve_transition_t::set_state2_gid(const size_int_t state_gid)
{
 state2_gid = state_gid;
 state2_lid  = get_symbol_table()->get_state(state_gid)->get_lid();
}

/* methods for getting string representation of transition */
const char * dve_transition_t::get_state1_name() const
{ return get_symbol_table()->get_state(state1_gid)->get_name(); }

const char * dve_transition_t::get_state2_name() const
{ return get_symbol_table()->get_state(state2_gid)->get_name(); }

bool dve_transition_t::get_guard_string(std::string & str) const
{
 if (guard) { str = guard->to_string(); return true; }
 else return false;
}

const char * dve_transition_t::get_sync_channel_name() const
{
 if (sync_mode!=SYNC_NO_SYNC)
   return get_symbol_table()->get_channel(channel_gid)->get_name();
 else return 0;
}

bool dve_transition_t::get_sync_expr_string(std::string & str) const
{
 if (sync_exprs.size())
  {
   if (sync_exprs.size()==1)
     str = sync_exprs[0]->to_string();
   else
    {
     str = "{";
     for (size_int_t i=0; i!=sync_exprs.size(); ++i)
      {
       str += sync_exprs[i]->to_string();
       if (i!=sync_exprs.size()-1) str += ", ";
      }
     str += "}";
    }
   return true;
  }
 else return false;
}

void dve_transition_t::get_effect_expr_string(size_int_t i,
                                          std::string & str) const
{
 str = effects[i]->to_string();
}

std::string dve_transition_t::to_string() const
{
 std::ostringstream ostr;
 write(ostr);
 return ostr.str();
}

int dve_transition_t::from_string(std::string & trans_str,
                              const size_int_t process_gid)
{
 std::istringstream str_stream(trans_str);
 DEB(cerr << "Creating transition by parsing " << trans_str << endl;)
 return read(str_stream, process_gid);
}
using namespace std;

int dve_transition_t::read(std::istream & istr, size_int_t process_gid)
{
 for (size_int_t i=0; i!=effects.size(); ++i) delete effects[i];
 effects.clear();
 dve_trans_init_parsing(&trans_parser, ptrans_terr, istr);
 trans_parser.set_dve_transition_to_fill(this);
 trans_parser.set_current_process(process_gid);
 try
  { dve_ttparse(); }
 catch (ERR_throw_t & err)
  { return err.id; } //returns non-zero value
 return 0; //returns zero in case of successful parsing
}

void dve_transition_t::write(std::ostream & ostr) const
{
 DEBFUNC(cerr << "BEGIN of dve_transition_t::write" << endl;)
 string aux="";
 size_int_t minus=0;
 string str;
 str = string(get_state1_name()) + " -> " + get_state2_name() + " { ";
 std::string guard;
 if (get_guard_string(guard))
  {
   aux = string("guard ") + guard + "; ";
   if (aux.size()+str.size()>80) str += "\n     ";
   str += aux;
  }
 
 const char * synchro_channel = get_sync_channel_name();
 if (synchro_channel)
  {
   std::string synchro_value;
   aux = string("sync ") + synchro_channel + (get_sync_mode()==SYNC_ASK||
                                              get_sync_mode()==SYNC_ASK_BUFFER?
                                              '?':'!')
         + (get_sync_expr_string(synchro_value)?(synchro_value):"") + "; ";
   if (aux.size()+str.size()-minus>80) { str += "\n     "; minus+=80; }
   str += aux;
  }
 
 if (get_effect_count())
  {
   aux = "effect ";
   if (aux.size()+str.size()-minus>80) { str += "\n     "; minus+=80; }
   str += aux;
   for (size_int_t i=0; i!=get_effect_count(); i++)
    {
     aux = get_effect(i)->to_string()
           + ((i==(get_effect_count()-1)) ? "; " : ", ");
     if (aux.size()+str.size()-minus>80) { str += "\n     "; minus+=80; }
     str += aux;
    }
  }
 if (str.size()-minus<=79) str += "}";
 else str += "\n     }";
 
 ostr << str;
 DEBFUNC(cerr << "END of dve_transition_t::write" << endl;)
}

dve_symbol_table_t * dve_transition_t::get_symbol_table() const
{
 dve_system_t * dve_system;
 if ((dve_system=dynamic_cast<dve_system_t *>(parent_system)))
   return dve_system->get_symbol_table();
 else return 0;
}



