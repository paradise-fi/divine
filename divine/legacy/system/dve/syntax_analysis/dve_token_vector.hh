/*!\file
 * This file contains class dve_token_vector_t that can store a long vector of
 * strings and serves as a dealocator of the memory used for strings.
 *
 * This class is used in class table_t for storing names of identifiers.
 */
#ifndef _TOKEN_VECTOR_
#define _TOKEN_VECTOR_

#ifndef DOXYGEN_PROCESSING
#include <cstdlib>
#include <vector>
#include "system/dve/syntax_analysis/dve_commonparse.hh"
#include "common/deb.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
#endif //DOXYGEN_PROCESSING
 //!Class used by dve_symbol_table_t to store the names of symbols.
 /*!This is an internal type.
  * Programmer of algorithms using DiVinE usually should not use this
  * structure.*/
 class dve_token_vector_t : public std::vector<char *>
 {
  public:
  ~dve_token_vector_t()
   {
    DEBFUNC(std::cerr << "BEGIN of destructor of token_vector_t" << std::endl;)
    for (dve_token_vector_t::iterator i = this->begin();i != this->end(); ++i)
    delete[] (*i);
    DEBFUNC(std::cerr << "END of destructor of token_vector_t" << std::endl;)
   }
  const char * save_token(const char * const token)
   {
    char * aux = new char[ MAXLEN ];
    strncpy(aux,token,MAXLEN);
    this->push_back(aux);
    return aux;
   }
 };

#ifndef DOXYGEN_PROCESSING
} //end of namespace `dve'

#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif
