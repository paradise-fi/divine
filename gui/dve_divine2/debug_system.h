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

#ifndef DEBUG_SYSYTEM_H_
#define DEBUG_SYSYTEM_H_

#include <QList>

#include "sevine.h"

/*!
 * This class provides access to some of the protected members of
 * divine::dve_explicit_system_t. This is a necessary workaround to get
 * more information about channels.
 */
class dve_debug_system_t : public divine::dve_explicit_system_t {
  public:
    dve_debug_system_t(divine::error_vector_t & evect = divine::gerr);
    
    divine::slong_int_t read (const char * const filename);
    
    int get_typed_channel_count(void) {return channelGids_.size();}
    divine::size_int_t get_channel_gid(int cid) const {return channelGids_.at(cid);}
    int get_channel_contents_count(const divine::state_t & state, int gid);
    divine::all_values_t get_channel_value(const divine::state_t & state, int gid, int index, int elem_index = 0);
    
  private:
    QList<divine::size_int_t> channelGids_;
};

#endif