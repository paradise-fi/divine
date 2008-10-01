
#ifndef DOXYGEN_PROCESSING
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "common/deb.hh"
using namespace divine;
#endif //DOXYGEN_PROCESSING

//THINGS FOR PARSING TIME AND ANOTHER QUERIES ON A TABLE

void dve_symbol_table_t::add_channel(dve_symbol_t * const symbol)
{
 symbol->set_sid(symbols.size());
 symbol->set_gid(channel_symbs.size());
 symbol->set_name(save_token(symbol->get_name()));
 channel_symbs.push_back(symbol);
 processes_of_symbols.push_back(NO_ID);
 global_symbols.push_back(symbol);
 symbols.push_back(symbol);
}

void dve_symbol_table_t::add_process(dve_symbol_t * const symbol)
{
 symbol->set_sid(symbols.size());
 symbol->set_gid(process_symbs.size());
 symbol->set_name(save_token(symbol->get_name()));
 process_symbs.push_back(symbol);
 global_symbols.push_back(symbol);
 processes_of_symbols.push_back(NO_ID);
 symbols_of_processes.push_back(new array_t<dve_symbol_t *>(3,5));
 symbols.push_back(symbol);
}

void dve_symbol_table_t::add_variable(dve_symbol_t * const symbol)
{
 symbol->set_sid(symbols.size());
 symbol->set_gid(variable_symbs.size());
 symbol->set_name(save_token(symbol->get_name()));
 variable_symbs.push_back(symbol);
 processes_of_symbols.push_back(symbol->get_process_gid());
 if (symbol->get_process_gid()==NO_ID)
   global_symbols.push_back(symbol);
 else
   symbols_of_processes[symbol->get_process_gid()]->push_back(symbol);
 symbols.push_back(symbol);
}

void dve_symbol_table_t::add_state(dve_symbol_t * const symbol)
{
 symbol->set_sid(symbols.size());
 symbol->set_gid(state_symbs.size());
 symbol->set_name(save_token(symbol->get_name()));
 state_symbs.push_back(symbol);
 processes_of_symbols.push_back(symbol->get_process_gid());
 if (symbol->get_process_gid()!=NO_ID)
   symbols_of_processes[symbol->get_process_gid()]->push_back(symbol);
 symbols.push_back(symbol);
}

bool dve_symbol_table_t::found_symbol
  (const char * name, const std::size_t proc_gid) const
{
 bool notfound = true;
 array_t<dve_symbol_t *> * process_symbols = symbols_of_processes[proc_gid];
 for (std::size_t i=0; i!=process_symbols->size(); i++)
    if (strcmp(name,(*process_symbols)[i]->get_name()) == 0)
      notfound = false;
 return (!notfound);
}

bool dve_symbol_table_t::found_global_symbol(const char * name) const
{
 bool notfound = true;
 for (std::size_t i=0; i!=global_symbols.size(); i++)
   {
    if (strcmp(name,global_symbols[i]->get_name()) == 0)
      notfound = false;
   }
 return (!notfound);
}

// find_symbol returns GID of found symbol or NO_ID
std::size_t dve_symbol_table_t::find_symbol
  (const char * name, const std::size_t proc_gid) const
{
 bool notfound = true;
 std::size_t found_pos = NO_ID;
 array_t<dve_symbol_t *> * process_symbols = symbols_of_processes[proc_gid];
 for (std::size_t i=0; i!=process_symbols->size() && notfound; i++)
  {
   DEB(cerr << "find_symbol: comparing " << name << " and ")
   DEB(     << (*process_symbols)[i]->get_name()<<endl;)
   if (strcmp(name,(*process_symbols)[i]->get_name()) == 0)
    {
     notfound = false;
     found_pos = (*process_symbols)[i]->get_sid();
     DEB(cerr<<"find_symbol: symbol found with symbol GID "<<found_pos<<endl;)
    }
  }
 return (notfound ? NO_ID : found_pos);
}

std::size_t dve_symbol_table_t::find_global_symbol(const char * name) const
{
 bool notfound = true;
 std::size_t found_pos = NO_ID;
 for (std::size_t i=0; i!=global_symbols.size() && notfound; i++)
  {
    DEB(cerr << "find_global_symbol: comparing " << name << " and ")
    DEB(     << global_symbols[i]->get_name()<<endl;)
   if (strcmp(name,global_symbols[i]->get_name()) == 0)
    {
     notfound = false;
     found_pos = global_symbols[i]->get_sid();
     DEB(cerr<<"find_global_symbol: symbol found with symbol GID "<<found_pos<<endl;)
    }
  }
 return (notfound ? NO_ID : found_pos);
}

//find_visible_symbol returns GID of found symbol or NO_ID
std::size_t dve_symbol_table_t::find_visible_symbol
               (const char * name, const std::size_t proc_gid) const
{
 std::size_t found_pos = NO_ID;
 found_pos = find_symbol(name, proc_gid);
 if (found_pos != NO_ID)
   return found_pos;
 return find_global_symbol(name);
}

dve_symbol_table_t::~dve_symbol_table_t()
{
 for (size_int_t i=0; i!=symbols.size();++i)
 {
  delete symbols[i];
 }
 for (size_int_t i=0; i!=symbols_of_processes.size();++i)
 {
  delete symbols_of_processes[i];
 }
}


