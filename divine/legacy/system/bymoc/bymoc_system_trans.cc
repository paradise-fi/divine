#include "system/bymoc/bymoc_system_trans.hh"
#include "system/bymoc/bymoc_explicit_system.hh"
#include "system/bymoc/bymoc_transition.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif

static transition_t * aux_trans;
static system_trans_t * aux_sys_trans = 0;
static enabled_trans_t * aux_enb_trans = 0;

bymoc_system_trans_t::~bymoc_system_trans_t()
{ /* empty destructor */ }

system_trans_t & bymoc_system_trans_t::operator=(const system_trans_t & second)
{
 gerr << "bymoc_system_trans_t::operator= not implemented" << thr();
 return (*aux_sys_trans); //unreachable;
}

std::string bymoc_system_trans_t::to_string() const
{
 gerr << "bymoc_system_trans_t::to_string not implemented" << thr();
 return std::string(""); //unreachable
}

void bymoc_system_trans_t::write(std::ostream & ostr) const
{
 gerr << "bymoc_system_trans_t::write not implemented" << thr();
}

void bymoc_system_trans_t::set_count(const size_int_t new_count)
{
 gerr << "bymoc_system_trans_t::set_count not implemented" << thr();
}

size_int_t bymoc_system_trans_t::get_count() const
{
 gerr << "bymoc_system_trans_t::get_count not implemented" << thr();
 return 0; //unreachable
}

transition_t *& bymoc_system_trans_t::operator[](const int i)
{
 gerr << "bymoc_system_trans_t::operator[] not implemented" << thr();
 return aux_trans; //unreachable
}

transition_t * const & bymoc_system_trans_t::operator[](const int i) const
{
 gerr << "bymoc_system_trans_t::operator[] not implemented" << thr();
 return aux_trans; //unreachable
}

enabled_trans_t & bymoc_enabled_trans_t::operator=(const enabled_trans_t & second)
{
 gerr << "bymoc_system_trans_t::operator= not implemented" << thr();
 return (*aux_enb_trans); //unreachable
}

