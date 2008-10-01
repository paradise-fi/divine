#include "system/dve/dve_source_position.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

void dve_source_position_t::get_source_pos(size_int_t & fline,size_int_t & fcol,
                                size_int_t & lline, size_int_t & lcol) const
{
 fline = first_line;
 fcol = first_col;
 lline = last_line;
 lcol = last_col;
}

void dve_source_position_t::set_source_pos(const size_int_t fline,
                                      const size_int_t fcol,
                                      const size_int_t lline,
                                      const size_int_t lcol)
{
 first_line = fline;
 first_col = fcol;
 last_line = lline;
 last_col = lcol;
}

void dve_source_position_t::set_source_pos(const dve_source_position_t & second)
{
 first_line = second.first_line;
 first_col  = second.first_col;
 last_line  = second.last_line;
 last_col   = second.last_col;
}
  
void dve_source_position_t::get_source_pos(dve_source_position_t & second)
{
 second.first_line = first_line;
 second.first_col  = first_col;
 second.last_line  = last_line;
 second.last_col   = last_col;
}


