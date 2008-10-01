#include "common/array.hh"
#include "system/bymoc/bymoc_transition.hh"
#include "system/bymoc/bymoc_expression.hh"
#include "common/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

using std::string;

////METHODS of bymoc_transition_t

bymoc_transition_t::~bymoc_transition_t()
{ }


std::string bymoc_transition_t::to_string() const
{
// return string_copy;
  gerr << "bymoc_transition_t::to_string not implemented" << thr();
  return ""; //unreachable
}

int bymoc_transition_t::from_string(std::string & trans_str,
                              const size_int_t process_gid)
{
// if (process_gid!=NO_ID) gerr << "bymoc_transition_t::from_string ignores "
//                                 "process_gid argument." << psh();
// string_copy = trans_str;
 
 gerr << "bymoc_transition_t::to_string not implemented" << thr();
 return 0; //unreachable
}

void bymoc_transition_t::write(std::ostream & ostr) const
{
 gerr << "bymoc_transition_t::write not implemented" << thr();
// ostr << string_copy;
}

int bymoc_transition_t::read(std::istream & istr, size_int_t process_gid)
{
// if (process_gid!=NO_ID) gerr << "bymoc_transition_t::from_string ignores "
//                                 "process_gid argument." << psh();
// string_copy="";
// while (istr)
//  {
//   istr >> string_copy;
//   if (istr) string_copy += " ";
//  }
 gerr << "bymoc_transition_t::read not implemented" << thr();
 return 0; //unreachable
}

