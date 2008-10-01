/*!\file
 * The main contribution of this file is the class dve_source_position_t.
 */
#ifndef DIVINE_DVE_SOURCE_POSITION_HH
#define DIVINE_DVE_SOURCE_POSITION_HH

#ifndef DOXYGEN_PROCESSING

#include "common/types.hh"

namespace divine { //We want Doxygen not to see namespace `divine'
#endif //DOXYGEN_PROCESSING

//!Class for storage of position in a source code
/*!This class represents a position in a source code. Other classes representing
 * various objects in the source code derive from this class to be possible
 * to locate them in the code.
 *
 * Position in the source code consists of four parameters:
 * - first line
 * - first column
 * - last line
 * - last column
 */
class dve_source_position_t {
  private:
  /* position in a source code */
  size_int_t first_line;
  size_int_t first_col;
  size_int_t last_line;
  size_int_t last_col;

  public:
  //!A constructor (initializs all position parameters to zero)
  dve_source_position_t(): first_line(0), first_col(0),
                           last_line(0), last_col(0)     {}
  //!A constructor
  dve_source_position_t(const size_int_t fline, const size_int_t fcol,
                        const size_int_t lline, const size_int_t lcol):
     first_line(fline), first_col(fcol), last_line(lline), last_col(lcol) {}

  //!Copies the position in a source code to four given parameters
  /*!\param fline = first line of position
     \param fcol  = first column of position
     \param lline = last line of position
     \param lcol  = last column of position
   */
  void get_source_pos(size_int_t & fline, size_int_t & fcol,
                      size_int_t & lline, size_int_t & lcol) const;
  //!Sets a position in a source code given by four given parameters
  /*!\param fline = first line of position
     \param fcol  = first column of position
     \param lline = last line of position
     \param lcol  = last column of position
   */
  void set_source_pos(const size_int_t fline, const size_int_t fcol,
                      const size_int_t lline, const size_int_t lcol);
  
  //!Copies te position in a source code to the given object
  void set_source_pos(const dve_source_position_t & second);
  
  //!Sets a position in a source code (copies parameters from the given object)
  void get_source_pos(dve_source_position_t & second);
  
  //!Returns the first line of the position in a source code
  size_int_t get_source_first_line() const { return first_line; }
  //!Returns the first column of the position in a source code
  size_int_t get_source_first_col() const { return first_col; }
  //!Returns the last line of the position in a source code
  size_int_t get_source_last_line() const { return last_line; }
  //!Returns the last column of the position in a source code
  size_int_t get_source_last_col() const { return last_col; }
};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace divine
#endif //DOXYGEN_PROCESSING

#endif



