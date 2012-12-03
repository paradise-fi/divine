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
// Filename : partition.h
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: partition.h,v 1.9 2005/08/02 09:36:43 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DBM_PARTITION_H
#define INCLUDE_DBM_PARTITION_H

#include "dbm/fed.h"
#include "base/intutils.h"

/**
 * @file The type partition_t defines a partition
 * of federations (fed_t) indexed by a given ID.
 * The federations added are kept disjoint by
 * definition of a partition and the decision of
 * where to add the intersecting parts is left to
 * the user.
 */

namespace dbm
{
    /// Type for partitions, designed to be used as a simple scalar with
    /// cheap copies as long as there is no need to modify a partition_t.
    class partition_t
    {
    public:
        /** Initialise an empty partition for federations
         * of dimension dim.
         * @param noPartition ignores the partition for IDs != 0
         * (0 interpreted as delay).
         */
        partition_t(cindex_t dim, bool noPartition = false)
            : fedTable(dimFlag2ptr(dim, noPartition)) {}

        /// The copy is a reference copy only.
        partition_t(const partition_t& arg)
            : fedTable(arg.fedTable) {
            assert(fedTable);
            if (isPtr()) fedTable->incRef();
        }

        /// Copy operator is a reference copy only.
        partition_t& operator = (const partition_t& arg) {
            assert(fedTable);
            if (arg.isPtr()) arg.fedTable->incRef();
            if (isPtr()) fedTable->decRef();
            fedTable = arg.fedTable;
            return *this;
        }

        /// Reset the partition.
        void reset() {
            if (isPtr())
            {
                cindex_t dim = fedTable->getDimension();
                bool flag = fedTable->isNotPartition();
                fedTable->decRef();
                fedTable = dimFlag2ptr(dim, flag);
            }
        }

        ~partition_t() {
            assert(fedTable);
            if (isPtr()) fedTable->decRef();
        }

        /// Wrappers to the federations of the partition.
        /// @see fed_t
        void intern();
        
        /** Add a federation to the partition subset
         * 'id'. The intersecting DBMs of fed with
         * the partition will be given to fed or one
         * subset of the partition and subtracted from
         * fed or the partition depending on the decision
         * function (@see constructor and setChooser).
         * @param id,fed ID of the federation fed to add
         * to the partition.
         * @pre fed.getDimension() == getDimension()
         * @post fed.isEmpty() and convexReduce() is
         * maintained on the internal federations.
         */
        void add(uintptr_t id, fed_t& fed);

        /** @return the federation corresponding to the
         * subset 'id' of the partition. If there is no
         * such subset, an empty federation is returned.
         * @param id ID of the subset to find.
         */
        fed_t get(uint32_t id) const {
            assert(fedTable);
            return isPtr() ? fedTable->get(id) : fed_t(edim());
        }
        fed_t getAll() const {
            assert(fedTable);
            return isPtr() ? fedTable->getAll() : fed_t(edim());
        }

        /// @return the dimension of the federations.
        cindex_t getDimension() const {
            assert(fedTable);
            return isPtr() ? fedTable->getDimension() : edim();
        }

        /// @return total number of DBMs of all the federations
        /// (compute the number on demand).
        size_t getNumberOfDBMs() const {
            assert(fedTable);
            return isPtr() ? fedTable->getNumberOfDBMs() : 0;
        }

        /// An entry in the table of federations.
        class entry_t
        {
        public:
            entry_t(uint32_t i, const fed_t& f)
                : id(i), fed(f) {}

            uint32_t id; //< ID of the federation, subset of the partition.
            fed_t fed;   //< Subset of partition.
        };

        /// Iterator to read the federation of the partitions. Only
        /// const is provided since modifying federations may destroy
        /// the partition.
        class const_iterator
        {
        public:
            /// Constructor should be used only by partition_t.
            const_iterator(const entry_t* const* start)
                : current(start) {}

            /// Dereference returns a federation.
            const fed_t& operator *() {
                assert(current && *current);
                return (*current)->fed;
            }
            const fed_t* operator ->() {
                assert(current && *current);
                return &(*current)->fed;
            }

            bool isNull() const {
                assert(current);
                return *current == NULL;
            }
            uint32_t id() const {
                assert(current && *current);
                return (*current)->id;
            }

            const_iterator& operator ++() {
                assert(current);
                ++current;
                return *this;
            }

            /// Standard == and != operators. The implementation uses ||,
            /// this is not a typo. The hash table may be sparse internally
            /// and the ++ operator stops to increment whenever there is
            /// nothing left.
            bool operator == (const const_iterator& arg) const {
                return current == arg.current;
            }
            bool operator != (const const_iterator& arg) const {
                return !(*this == arg);
            }
            bool operator < (const const_iterator& arg) const {
                return current < arg.current;
            }

        private:
            const entry_t* const* current; /// Current position in the table.
        };

        /// Standard begin() and end() for const_iterator.
        const_iterator begin() const {
            assert(fedTable);
            return isPtr()
                ? const_iterator(fedTable->getBeginTable())
                : const_iterator(NULL);
        }
        const_iterator end() const {
            assert(fedTable);
            return isPtr()
                ? const_iterator(fedTable->getEndTable())
                : const_iterator(NULL);
        }

    private:

        /// The federation table (a simple hash table with chained collisions
        /// on the table entries) to access the subsets (fed_t) of the partition.
        class fedtable_t
        {
        public:
            static fedtable_t* create(cindex_t dim, bool noPartition);

            /// @pre !isMutable() or it is stupid to call this
            /// @post refCounter is decremented
            fedtable_t* copy();

            void incRef() {
                refCounter++;
            }
            void decRef() {
                assert(refCounter > 0);
                if (!--refCounter) {
                    remove();
                }
            }
            bool isMutable() {
                assert(refCounter > 0);
                return refCounter == 1;
            }

            /// Access methods.
            entry_t** getTable() { return table; }
            const entry_t* const* getBeginTable() const { return table; }
            const entry_t* const* getEndTable()   const { return getBeginTable() + getSize(); }
            cindex_t getDimension() const { return all.getDimension(); }
            size_t getSize() const { return mask + 1; }
            bool isNotPartition() const { return noPartition; }

            /// Remove (deallocate) this fedTable. @pre refCounter == 0.
            void remove();

            /// @return the fed_t corresponding to the partition's subset id.
            /// The federation is empty if the subset id is not defined.
            fed_t get(uint32_t id) const;
            fed_t getAll() const { return all; }
            
            /// Add a federation to the subset 'id' of the partition.
            /// @return true if this table must be resized.
            bool add(uint32_t id, fed_t& fed);

            /// @return a resized table.
            /// @pre isMutable()
            /// @post this table is deallocated.
            fedtable_t* larger();

            /// @return total number of DBMs.
            size_t getNumberOfDBMs() const;

        private:
            /// Constants for initialization.
            enum {
                INIT_POWER = 1,
                INIT_SIZE = (1 << INIT_POWER),
                INIT_MASK = (INIT_SIZE - 1)
            };

            /// Private constructors because the allocation is special.
            fedtable_t(cindex_t d, size_t nb, uint32_t m, bool noP)
                : refCounter(1), all(d), nbEntries(nb), mask(m), noPartition(noP) {
                // table not initialized
            }

            /// Default initial constructor.
            fedtable_t(cindex_t d, bool noP)
                : refCounter(1), all(d), nbEntries(0), mask(INIT_MASK), noPartition(noP) {
                base_resetSmall(table, INIT_SIZE*(sizeof(void*)/sizeof(int)));
            }

            uint32_t refCounter; //< standard reference counter
            fed_t all;           //< union of all federations
            size_t nbEntries;    //< number of non null entries
            uint32_t mask;       //< bit mask for access, table size = mask+1
            bool noPartition;
            entry_t *table[];    //< the table of federations.
        };

        /// Check that the referenced fedTable is mutable (refCounter == 1)
        /// and make a copy if necessary.
        void checkMutable() {
            assert(fedTable);
            if (!fedTable->isMutable()) {
                fedTable = fedTable->copy();
            }
        }

        static fedtable_t* dimFlag2ptr(cindex_t dim, bool flag) {
            return (fedtable_t*) ((dim << 2) | (flag << 1) | 1);
        }
        bool isPtr() const {
            return (((uintptr_t)fedTable) & 1) == 0;
        }
        cindex_t edim() const {
            assert(!isPtr());
            return ((uintptr_t)fedTable) >> 2;
        }
        bool eflag() const {
            assert(!isPtr());
            return (((uintptr_t)fedTable) & 2) != 0;
        }

        fedtable_t *fedTable; //< table of federations.
    };
}

#endif // INCLUDE_DBM_PARTITION_H
