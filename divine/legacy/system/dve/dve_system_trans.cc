#include "system/dve/dve_system_trans.hh"
#include "system/dve/dve_explicit_system.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif

system_trans_t & dve_system_trans_t::operator=(const system_trans_t & second)
{
 copy_from(second);
 return (*this);
}

void dve_system_trans_t::set_count(const size_int_t new_count)
 {
  if (trans_list) delete [] trans_list;
  trans_list = new transition_t * [new_count];
  count = new_count;
 }

std::string dve_system_trans_t::to_string() const
{
 std::ostringstream ostr;
 write(ostr);
 return ostr.str();
}

void dve_system_trans_t::write(std::ostream & ostr) const
{
 ostr << '<';
 dve_transition_t * trans;
 for (size_int_t i=0; i<count; ++i)
  {
   trans = dynamic_cast<dve_transition_t*>(trans_list[i]);
   ostr << trans->get_process_gid() << '.' << trans->get_lid();
   if (i!=count-1) ostr << '&';
  }
 ostr << '>';
}

dve_system_trans_t::~dve_system_trans_t()
{ if (trans_list) delete [] trans_list; }

void dve_system_trans_t::create_from(const system_trans_t & second)
{
 count = second.get_count();
 trans_list = new transition_t * [count];
 for (size_int_t i = 0; i<count; ++i) trans_list[i]=second[i];
}

dve_enabled_trans_t::dve_enabled_trans_t(const dve_enabled_trans_t & second)
{
 const enabled_trans_t * enabled_trans = dynamic_cast<const enabled_trans_t*>(&second);
 dve_system_trans_t::create_from(*enabled_trans);
 error = second.get_erroneous();
}

enabled_trans_t & dve_enabled_trans_t::operator=(const enabled_trans_t & second)
{
 dve_system_trans_t::copy_from(second);
 error = second.get_erroneous();
 return (*this);
}

