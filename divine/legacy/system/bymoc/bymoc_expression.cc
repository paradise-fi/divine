#include <string>
#include <sstream>
#include "system/bymoc/bymoc_expression.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING


using std::string;

void bymoc_expression_t::swap(expression_t & expr)
{
// bymoc_expression_t * p_bexpr = dynamic_cast<bymoc_expression_t*>(&expr);
// if (p_bexpr) p_bexpr->string_copy.swap(string_copy);
// else gerr << "bymoc_expression_t::swap: incompatible expression" << thr();
 gerr << "bymoc_expression_t::swap not implemented" << thr();
}

void bymoc_expression_t::assign(const expression_t & expr)
{
// const bymoc_expression_t * p_bexpr =
//   dynamic_cast<const bymoc_expression_t*>(&expr);
// if (p_bexpr) string_copy = p_bexpr->string_copy;
// else gerr << "bymoc_expression_t::assign: incompatible expression" << thr();
 gerr << "bymoc_expression_t::assign not implemented" << thr();
}

std::string bymoc_expression_t::to_string() const
{
 //return string_copy;
 gerr << "bymoc_expression_t::to_string not implemented" << thr();
 return ""; //unreachable
}

int bymoc_expression_t::from_string(std::string & expr_str,
                                    const size_int_t process_gid)
{
// if (process_gid!=NO_ID) gerr << "bymoc_expression_t::from_string ignores "
//                                 "process_gid argument." << psh();
// string_copy = expr_str;
 gerr << "bymoc_expression_t::from_string not implemented" << thr();
 return 0; //unreachable
}

void bymoc_expression_t::write(std::ostream & ostr) const
{
  gerr << "bymoc_expression_t::write not implemented" << thr();
// ostr << string_copy;
}

int bymoc_expression_t::read(std::istream & istr, size_int_t process_gid)
{
// if (process_gid!=NO_ID) gerr << "bymoc_expression_t::from_string ignores "
//                                 "process_gid argument." << psh();
// string_copy="";
// while (istr)
//  {
//   istr >> string_copy;
//   if (istr) string_copy += " ";
//  }
 gerr << "bymoc_expression_t::read not implemented" << thr();
 return 0; //unreachable
}


