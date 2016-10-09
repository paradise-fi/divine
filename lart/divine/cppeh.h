// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once


/* From libC++ABI

    Exception Header Layout:

+---------------------------+-----------------------------+---------------+
| __cxa_exception           | _Unwind_Exception CLNGC++\0 | thrown object |
+---------------------------+-----------------------------+---------------+
                                                          ^
                                                          |
  +-------------------------------------------------------+
  |
+---------------------------+-----------------------------+
| __cxa_dependent_exception | _Unwind_Exception CLNGC++\1 |
+---------------------------+-----------------------------+

    Exception Handling Table Layout:

+-----------------+--------+
| lpStartEncoding | (char) |
+---------+-------+--------+---------------+-----------------------+
| lpStart | (encoded with lpStartEncoding) | defaults to funcStart |
+---------+-----+--------+-----------------+---------------+-------+
| ttypeEncoding | (char) | Encoding of the type_info table |
+---------------+-+------+----+----------------------------+----------------+
| classInfoOffset | (ULEB128) | Offset to type_info table, defaults to null |
+-----------------++--------+-+----------------------------+----------------+
| callSiteEncoding | (char) | Encoding for Call Site Table |
+------------------+--+-----+-----+------------------------+--------------------------+
| callSiteTableLength | (ULEB128) | Call Site Table length, used to find Action table |
+---------------------+-----------+---------------------------------------------------+
+---------------------+-----------+------------------------------------------------+
| Beginning of Call Site Table            The current ip is a 1-based index into   |
| ...                                     this table.  Or it is -1 meaning no      |
|                                         action is needed.  Or it is 0 meaning    |
|                                         terminate.                               |
| +-------------+---------------------------------+------------------------------+ |
| | landingPad  | (ULEB128)                       | offset relative to lpStart   | |
| | actionEntry | (ULEB128)                       | Action Table Index 1-based   | |
| |             |                                 | actionEntry == 0 -> cleanup  | |
| +-------------+---------------------------------+------------------------------+ |
| ...                                                                              |
+----------------------------------------------------------------------------------+
+---------------------------------------------------------------------+
| Beginning of Action Table       ttypeIndex == 0 : cleanup           |
| ...                             ttypeIndex  > 0 : catch             |
|                                 ttypeIndex  < 0 : exception spec    |
| +--------------+-----------+--------------------------------------+ |
| | ttypeIndex   | (SLEB128) | Index into type_info Table (1-based) | |
| | actionOffset | (SLEB128) | Offset into next Action Table entry  | |
| +--------------+-----------+--------------------------------------+ |
| ...                                                                 |
+---------------------------------------------------------------------+-----------------+
| type_info Table, but classInfoOffset does *not* point here!                           |
| +----------------+------------------------------------------------+-----------------+ |
| | Nth type_info* | Encoded with ttypeEncoding, 0 means catch(...) | ttypeIndex == N | |
| +----------------+------------------------------------------------+-----------------+ |
| ...                                                                                   |
| +----------------+------------------------------------------------+-----------------+ |
| | 1st type_info* | Encoded with ttypeEncoding, 0 means catch(...) | ttypeIndex == 1 | |
| +----------------+------------------------------------------------+-----------------+ |
| +---------------------------------------+-----------+------------------------------+  |
| | 1st ttypeIndex for 1st exception spec | (ULEB128) | classInfoOffset points here! |  |
| | ...                                   | (ULEB128) |                              |  |
| | Mth ttypeIndex for 1st exception spec | (ULEB128) |                              |  |
| | 0                                     | (ULEB128) |                              |  |
| +---------------------------------------+------------------------------------------+  |
| ...                                                                                   |
| +---------------------------------------+------------------------------------------+  |
| | 0                                     | (ULEB128) | throw()                      |  |
| +---------------------------------------+------------------------------------------+  |
| ...                                                                                   |
| +---------------------------------------+------------------------------------------+  |
| | 1st ttypeIndex for Nth exception spec | (ULEB128) |                              |  |
| | ...                                   | (ULEB128) |                              |  |
| | Mth ttypeIndex for Nth exception spec | (ULEB128) |                              |  |
| | 0                                     | (ULEB128) |                              |  |
| +---------------------------------------+------------------------------------------+  |
+---------------------------------------------------------------------------------------+

Notes:

*  ttypeIndex in the Action Table, and in the exception spec table, is an index,
     not a byte count, if positive.  It is a negative index offset of
     classInfoOffset and the sizeof entry depends on ttypeEncoding.
   But if ttypeIndex is negative, it is a positive 1-based byte offset into the
     type_info Table.
   And if ttypeIndex is zero, it refers to a catch (...).

*  landingPad can be 0, this implies there is nothing to be done.

*  landingPad != 0 and actionEntry == 0 implies a cleanup needs to be done
     @landingPad.

*  A cleanup can also be found under landingPad != 0 and actionEntry != 0 in
     the Action Table with ttypeIndex == 0.
*/

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
DIVINE_UNRELAX_WARNINGS
#include <divine/vm/value.hpp>
#include <divine/vm/pointer.hpp>
#include <brick-types>
#include <brick-query>
#include <brick-data>
#include <brick-mem>
#include <vector>
#include <set>

namespace lart {
namespace divine {

struct CppEhTab {

    // DWARF Constants
    enum
    {
        DW_EH_PE_absptr   = 0x00,
        DW_EH_PE_uleb128  = 0x01,
        DW_EH_PE_udata2   = 0x02,
        DW_EH_PE_udata4   = 0x03,
        DW_EH_PE_udata8   = 0x04,
        DW_EH_PE_sleb128  = 0x09,
        DW_EH_PE_sdata2   = 0x0A,
        DW_EH_PE_sdata4   = 0x0B,
        DW_EH_PE_sdata8   = 0x0C,
        DW_EH_PE_pcrel    = 0x10,
        DW_EH_PE_textrel  = 0x20,
        DW_EH_PE_datarel  = 0x30,
        DW_EH_PE_funcrel  = 0x40,
        DW_EH_PE_aligned  = 0x50,
        DW_EH_PE_indirect = 0x80,
        DW_EH_PE_omit     = 0xFF
    };

    struct TypeInfo : brick::types::Ord {
        TypeInfo( llvm::Constant *ti ) : self( ti ) { }

        llvm::Constant *self;

        bool operator<( const TypeInfo &o ) const { return self < o.self; }
    };

    struct ExceptSpec : brick::types::Ord {
        std::vector< const TypeInfo * > typeInfos;

        size_t size() const { return typeInfos.size(); }

        bool operator<( const ExceptSpec &o ) const { return typeInfos < o.typeInfos; }
    };

    using Handler = brick::types::Union< const TypeInfo *, const ExceptSpec * >;

    struct Action : brick::types::Ord {
        std::vector< Handler > handlers;

        bool operator<( const Action &o ) const {
            return handlers < o.handlers;
        }
    };

    struct CallSite {
        CallSite( llvm::Instruction *pc, llvm::BasicBlock *lb, const Action *act ) :
            pc( pc ), landingBlock( lb ), actions( act )
        { }

        llvm::Instruction *pc;
        llvm::BasicBlock *landingBlock;
        const Action *actions;
    };

    explicit CppEhTab( llvm::Function &fn ) {
        build( fn );
    }

    void build( llvm::Function &fn )
    {
        for ( auto &i : brick::query::flatten( fn ) ) {
            if ( auto *invoke = llvm::dyn_cast< llvm::InvokeInst >( &i ) ) {
                llvm::BasicBlock *unw = invoke->getUnwindDest();
                auto *lp = invoke->getLandingPadInst();

                Action action;
                for ( int i = 0, end = lp->getNumClauses(); i < end; ++i ) {
                    if ( lp->isCatch( i ) ) {
                        auto *ti = &*typeInfos.emplace( lp->getClause( i )->stripPointerCasts() ).first;
                        action.handlers.emplace_back( ti );
                    }
                    else if ( lp->isFilter( i ) ) {
                        const ExceptSpec *es = nullptr;
                        if( auto *array = llvm::dyn_cast< llvm::ConstantArray >( lp->getClause( i ) ) )
                        {
                                auto *t = array->getType();
                            auto cnt = t->getNumElements();
                            ExceptSpec spec;
                            for ( unsigned j = 0; j < cnt; ++j ) {
                                auto *ti = &*typeInfos.emplace( array->getOperand( j ) ).first;
                                spec.typeInfos.emplace_back( ti );
                            }
                            es = &*exceptSpecs.emplace( std::move( spec ) ).first;
                        }
                        else if ( llvm::isa< llvm::ConstantAggregateZero >( lp->getClause( i ) ) )
                        {   // throw()
                            es = &*exceptSpecs.emplace().first;
                        }
                        else {
                            lp->getClause( i )->dump();
                            UNREACHABLE( "Unexpected landingpad clause type" );
                        }
                        action.handlers.emplace_back( es );
                    }
                }
                if ( lp->isCleanup() )
                    action.handlers.emplace_back();
                auto *act = &*actions.emplace( action ).first;
                callSites.emplace_back( invoke, unw, act );
            }
        }
    }

    const size_t ptrSize = 8;
    const size_t lebOffsetSize = 4; // we encode all LEB128 data to 4 bytes

    size_t tableHeaderSize() const {

        auto size = 1 // lpStartEncoding
            // lpStart is omitted
            + 1 // ttypeEncoding
            + 6 // classInfoOffset + padding
            + 1 // callSiteEncoding
            + 7; // callSiteTableLength + padding
        ASSERT_EQ( size % 4, 0 );
        return size;
    }

    size_t tableSizeBound() const {

        auto size = tableHeaderSize()
            + (3 * 4 + lebOffsetSize) * callSites.size()
            + (2 * lebOffsetSize) * brick::query::query( actions )
                                      .map( []( auto &a ) { return a.handlers.size(); } )
                                      .sum() // action table
            + ptrSize * typeInfos.size() // type info table (first part)
            + lebOffsetSize * brick::query::query( exceptSpecs )
                                  .map( []( auto &es ) { return es.size() + 1; } )
                                  .sum(); // exception spec records
        return brick::mem::align( size, 4 );
    }

    template< typename T >
    static void pushRaw( std::string &str, T v ) {
        union {
            T v;
            char data[ sizeof( T ) ];
        } u;
        u.v = v;
        for ( auto x : u.data )
            str.push_back( x );
    }

    static void pushStr( std::string &str, const std::string &data ) {
        str.insert( str.end(), data.begin(), data.end() );
    }

    static void pushLeb_n( std::string &str, int32_t data, int n = 4 ) {
        ASSERT_LEQ( data, (1 << (4 * 7)) - 1 ); // maximum value which fits 4 bytes
        // taken and modified from LLVM
        int todo = n;
        do {
            uint8_t byte = data & 0x7f;
            // NOTE: this assumes that this signed shift is an arithmetic right shift.
            data >>= 7;
            if ( --todo > 0 )
                byte |= 0x80; // Mark this byte to show that more bytes will follow.
            str.push_back( byte );
        } while ( todo > 0 );
    }

    template< typename Program >
    std::string callSiteTable( const Program &p ) const {
        std::string table;
        for ( auto &ci : callSites ) {
            auto callPC = p.pcmap.at( ci.pc );
            auto landingBlock = p.blockmap.at( ci.landingBlock );
            pushRaw< uint32_t >( table, callPC.instruction() ); // PC range start
            pushRaw< uint32_t >( table, 1 );                    // PC range length
            pushRaw< uint32_t >( table, landingBlock.instruction() );
            pushLeb_n( table, actionIndex[ ci.actions ] );
        }
        return table;
    }

    std::string actionTable() {
        std::string table;
        for ( auto &a : actions ) {
            actionIndex[ &a ] = table.size() + 1;
            for ( auto &h : a.handlers ) {
                pushLeb_n( table, typeIndex[ h ] );

                // push next action offset for this call site
                if ( &h == &a.handlers.back() ) {
                    pushLeb_n( table, 0 ); // end
                } else
                    pushLeb_n( table, 4, 4 ); // offset 4 written in 4 bytes
            }
        }
        return table;
    }

    std::string exceptionSpecTable() {
        std::string table;
        for ( auto &es : exceptSpecs ) {
            typeIndex[ &es ] = -( table.size() + 1 ); // indices start with 1 and are negative
            for ( auto *ti : es.typeInfos )
                pushLeb_n( table, typeIndex[ ti ] );
            pushLeb_n( table, 0 ); // end of record
        }
        return table;
    }

    template< typename Program, typename Ptr >
    void insertEhTable( Program &p, Ptr table, std::vector< ::divine::vm::ConstPointer > &tiPtrs ) {
        ASSERT( tiPtrs.empty() );
        auto startOffset = table.offset();
        ASSERT_EQ( startOffset % 4, 0 );
        namespace vm = ::divine::vm;
        namespace value = vm::value;

        auto write = [&]( auto v ) {
                p.heap().write( table, v );
                table.offset( table.offset() + sizeof( typename decltype( v )::Raw ) ); // TODO
            };
        auto writeStr = [&]( const std::string &s ) {
                for ( auto c : s )
                    write( value::Int< 8 >( c ) );
            };
        auto writeLeb_n = [&]( auto val, int n = 4 ) {
                std::string buf;
                pushLeb_n( buf, val, n );
                writeStr( buf );
            };

        // calculate subtables
        // reversed oreder is important to make sure indices are properly computed
        for ( auto &ti : typeInfos ) {
            typeIndex[ &ti ] = tiPtrs.size() + 1; // indices start with 1
            auto tiPtrPtr = p.s2hptr( p.valuemap.at( ti.self ).slot );
            value::Pointer tiPtr;
            p.heap().read( tiPtrPtr, tiPtr );
            tiPtrs.push_back( tiPtr.cooked() );
        }
        typeIndex[ Handler() ] = 0; // cleanup
        auto esTable = exceptionSpecTable();
        auto actTable = actionTable();
        auto csTable = callSiteTable( p );

        // must be aligned as classInfoTable contains pointers, tables don't
        // care as they have variable length entries anyway
        // Also, the offset points *after* type infos, at the beginnig of
        // execption specifications
        // offset is from the point after classInfoOffset byte
        auto tiOffset = csTable.size() + actTable.size() + 1 /* callSiteEncoding */ + 7; /* callSiteTableLength + padding */
        auto classInfoOffset = tiOffset + tiPtrs.size() * sizeof( vm::PointerRaw );

        // **** HEADER
        // lpStartEncoding, implies lpStart = 0 and therefore lpStart =
        // funcStart
        write( value::Int< 8 >( DW_EH_PE_omit ) );
        // ttypeEncoding
        write( value::Int< 8 >( DW_EH_PE_absptr ) );
        writeLeb_n( classInfoOffset, 6 );

        auto tiStart = table.offset() + tiOffset;
        ASSERT_EQ( tiStart % 4, 0 );

        // callSiteEncoding
        write( value::Int< 8 >( DW_EH_PE_udata4 ) );
        // pad header for 4 byte alignment
        writeLeb_n( csTable.size(), 7 ); // callSiteTableLength
        ASSERT_EQ( table.offset() - startOffset, tableHeaderSize() );
        // **** END OF HEADER

        writeStr( csTable );
        writeStr( actTable );

        // add padding of class table
        ASSERT_EQ( table.offset(), tiStart );

        for ( auto ti : brick::query::reversed( tiPtrs ) )
            write( value::Pointer( ti ) );
        writeStr( esTable );

        ASSERT_EQ( table.offset() - startOffset, tableSizeBound() );
    }

    std::vector< CallSite > callSites;
    std::set< Action > actions;
    std::set< TypeInfo > typeInfos;
    std::set< ExceptSpec > exceptSpecs;

    brick::data::RMap< Handler, int > typeIndex;
    brick::data::RMap< const Action *, int > actionIndex;
};

}
}
