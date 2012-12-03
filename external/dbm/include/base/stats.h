/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; -*-
 *
 * This file is part of the UPPAAL DBM library.
 *
 * The UPPAAL DBM library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * The UPPAAL DBM library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with the UPPAAL DBM library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA.
 */

// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// Filename : stats.h
//
// General statistics meant for experiments but not in the release
// version of UPPAAL.
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_STATS_H
#define INCLUDE_BASE_STATS_H

#ifdef SHOW_STATS

#include "hash/tables.h"

namespace base
{
    class Stats
    {
    public:
        Stats() {}
        ~Stats();

        void count(const char *statName, const char *subStat = NULL);

        struct stat_t
        {
            stat_t(const char *statName, stat_t *nxt)
                : name(statName), counter(1), next(nxt) {}
            ~stat_t() { delete next; }

            const char *name;
            long counter;
            stat_t *next;
        };

        struct entry_t : public hash::TableSingle<entry_t>::Bucket_t
        {
            entry_t(const char *name, stat_t *next = NULL)
                : stat(name, next) {}

            stat_t stat; // Main stat, with list of sub-stats.
        };

    private:
        hash::TableSingle<entry_t> table;
    };

    extern Stats stats;
}

// Typical way of using stats:

#define RECORD_STAT()        base::stats.count(__PRETTY_FUNCTION__, NULL)
#define RECORD_SUBSTAT(NAME) base::stats.count(__PRETTY_FUNCTION__, NAME)
#define RECORD_NSTAT(ROOT, NAME)\
base::stats.count(ROOT, NULL);  \
base::stats.count(ROOT, NAME)

#else

#define RECORD_STAT()
#define RECORD_SUBSTAT(NAME)
#define RECORD_NSTAT(ROOT, NAME)

#endif // SHOW_STATS

#endif // INCLUDE_BASE_STATS_H
