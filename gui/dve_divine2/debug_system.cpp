/***************************************************************************
 *   Copyright (C) 2009 by Martin Moracek                                  *
 *   xmoracek@fi.muni.cz                                                   *
 *                                                                         *
 *   DiVinE is free software, distributed under GNU GPL and BSD licences.  *
 *   Detailed licence texts may be found in the COPYING file in the        *
 *   distribution tarball. The tool is a product of the ParaDiSe           *
 *   Laboratory, Faculty of Informatics of Masaryk University.             *
 *                                                                         *
 *   This distribution includes freely redistributable third-party code.   *
 *   Please refer to AUTHORS and COPYING included in the distribution for  *
 *   copyright and licensing details.                                      *
 ***************************************************************************/

#include "debug_system.h"

#include "sevine.h"

dve_debug_system_t::dve_debug_system_t(divine::error_vector_t & evect)
    : dve_explicit_system_t(evect), explicit_system_t(evect), system_t(evect)
{
}

divine::slong_int_t dve_debug_system_t::read (const char * const filename)
{
  divine::slong_int_t res = dve_explicit_system_t::read(filename);
  
  if(res != 0)
    return res;
  
  // get channel gids
  for(divine::size_int_t i = 0; i < state_creators_count; ++i) {
    state_creator_t & state_creator = state_creators[i];
  
    // mark only channels (typed)
    if(state_creator.type != state_creator_t::CHANNEL_BUFFER)
      continue;
    
    channelGids_.append(state_creator.gid);
  }
  return res;
}

int dve_debug_system_t::get_channel_contents_count(const divine::state_t & state, int gid)
{
  return channel_content_count(state, gid);
}

divine::all_values_t dve_debug_system_t::get_channel_value
(const divine::state_t & state, int gid, int index, int sub_index)
{ 
  divine::all_values_t res = read_from_channel(state, gid, sub_index, index);
  return res;
}
