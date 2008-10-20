#include "common/array.hh"
#include "system/bymoc/bymoc_transition.hh"
#include "system/bymoc/bymoc_process.hh"
#include "common/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

using std::string;

bymoc_process_t::~bymoc_process_t()
{ /*empty destructor*/ }

transition_t * bymoc_process_t::get_transition(const size_int_t id)
{
 gerr << "bymoc_process_t::get_transition is not implemented" << thr();
 return 0; //unreachable
}

const transition_t * bymoc_process_t::get_transition(const size_int_t id) const
{
 gerr << "bymoc_process_t::get_transition is not implemented" << thr();
 return 0; //unreachable
}

size_int_t bymoc_process_t::get_trans_count() const
{
 gerr << "bymoc_process_t::get_trans_count is not implemented" << thr();
 return 0; //unreachable
}

void bymoc_process_t::add_transition(transition_t * const transition)
{
 gerr << "bymoc_process_t::add_transition is not implemented" << thr();
}

void bymoc_process_t::remove_transition(const size_int_t transition_gid)
{
 gerr << "bymoc_process_t::remove_transition is not implemented" << thr();
}

string bymoc_process_t::to_string() const
{
 gerr << "bymoc_process_t::to_string not implemented" << thr();
 return string(""); //unreachable
}

int bymoc_process_t::from_string(std::string & proc_str)
{
 gerr << "bymoc_process_t::from_string not implemented" << thr();
 return 0; //unreachable
}

int bymoc_process_t::read(std::istream & istr)
{
 gerr << "bymoc_process_t::read not implemented" << thr();
 return 0; //unreachable
}

void bymoc_process_t::write(std::ostream & ostr) const
{
 gerr << "bymoc_process_t::write not implemented" << thr();
}


