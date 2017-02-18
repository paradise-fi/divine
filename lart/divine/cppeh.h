// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2017 Vladimír Štill <xstill@fi.muni.cz>
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
| Beginning of Call Site Table            The current ip lies within the           |
| ...                                     (start, length) range of one of these    |
|                                         call sites. There may be action needed.  |
| +-------------+---------------------------------+------------------------------+ |
| | start       | (encoded with callSiteEncoding) | offset relative to funcStart | |
| | length      | (encoded with callSiteEncoding) | length of code fragment      | |
| | landingPad  | (encoded with callSiteEncoding) | offset relative to lpStart   | |
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
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Dwarf.h>
DIVINE_UNRELAX_WARNINGS
#include <brick-types>
#include <brick-query>
#include <brick-data>
#include <brick-mem>
#include <vector>
#include <set>

namespace lart {
namespace divine {

namespace dwarf = llvm::dwarf;

struct CppEhTab {

    using Actions = std::vector< llvm::Constant * >;

    struct CallSite {
        CallSite( llvm::Instruction *pc, llvm::BasicBlock *lb, Actions act, bool cleanup ) :
            pc( pc ), landingBlock( lb ), actions( act ), cleanup( cleanup )
        { }

        llvm::Instruction *pc;
        llvm::BasicBlock *landingBlock;
        Actions actions;
        bool cleanup;
    };

    explicit CppEhTab( llvm::Function &fn ) :
        _ctx( &fn.getParent()->getContext() ),
        _dl( fn.getParent() ),
        _fn( &fn )
    {
        build( fn );
    }

    void build( llvm::Function &fn )
    {
        for ( auto &i : brick::query::flatten( fn ) ) {
            if ( auto *invoke = llvm::dyn_cast< llvm::InvokeInst >( &i ) ) {
                llvm::BasicBlock *unw = invoke->getUnwindDest();
                auto *lp = invoke->getLandingPadInst();

                Actions actions;
                for ( int i = 0, end = lp->getNumClauses(); i < end; ++i ) {
                    if ( lp->isCatch( i ) ) {
                        auto *ti = lp->getClause( i )->stripPointerCasts();
                        addID( ti );
                        actions.emplace_back( ti );
                    }
                    else if ( lp->isFilter( i ) ) {
                        std::vector< llvm::Constant * > filter;
                        auto *clause = lp->getClause( i )->stripPointerCasts();
                        iterateFilter( clause, [&]( auto *ti ) {
                                addID( ti );
                                filter.emplace_back( ti );
                            } );
                        addSpec( clause );
                        actions.emplace_back( clause );
                    }
                }
                _callSites.emplace_back( invoke, unw, actions, lp->isCleanup() );
            }
        }
    }

    int lebEncSize( int maxVal ) {
        return std::max( (brick::bitlevel::MSB( maxVal ) + 6) / 7, 1u );
    }

    llvm::Constant *getLSDAConst() {
        if ( _callSites.empty() )
            return nullptr;

        const int typeIDEncBytes = lebEncSize( _typeInfos.size() + 1 );
        auto *intptrt = _dl.getIntPtrType( *_ctx );
        auto *i32t = llvm::Type::getInt32Ty( *_ctx );
        brick::data::RMap< llvm::Constant *, int > typeIndex;
        brick::data::RMap< const Actions *, int > actionIndex;


        for ( auto *ti : _typeInfos )
            typeIndex[ ti ] = typeID( ti );
        // type infos are reversed as they are indexed by negated index
        auto typeConstants = _typeInfos;
        std::reverse( typeConstants.begin(), typeConstants.end() );
        auto *typeInfosArray = _typeInfos.empty() ? nullptr : llvm::ConstantStruct::getAnon( typeConstants );


        std::string exSpecTable;
        for ( auto *es : _exceptSpecs ) {
            typeIndex[ es ] = -( exSpecTable.size() + 1 ); // indices start with 1 and are negativea
            iterateFilter( es, [&]( auto *ti ) {
                    pushLeb_n( exSpecTable, typeIndex[ ti ], typeIDEncBytes );
                } );
            pushLeb_n( exSpecTable, 0, 1 ); // end of record
        }
        auto *exSpecArray = llvm::ConstantDataArray::getString( *_ctx, exSpecTable, false );


        std::string actTable;
        for ( auto &cs : _callSites ) {
            actionIndex[ &cs.actions ] = actTable.size() + 1;
            for ( auto *h : cs.actions ) {
                ASSERT_NEQ( typeIndex[ h ], 0 );
                pushLeb_n( actTable, typeIndex[ h ], typeIDEncBytes );

                // push next action offset for this call site
                if ( h == cs.actions.back() && !cs.cleanup )
                    pushLeb_n( actTable, 0, 1 ); // end
                else
                    pushLeb_n( actTable, 1, 1 ); // offset is relative to the offset entry
            }
            if ( cs.cleanup ) {
                pushLeb_n( actTable, 0, 1 ); // cleanup is id 0
                pushLeb_n( actTable, 0, 1 ); // end it terminates actions entry
            }

        }
        auto *actTableArray = llvm::ConstantDataArray::getString( *_ctx, actTable, false );

        auto intbbpc = [intptrt]( llvm::BasicBlock *bb ) {
            if ( bb == bb->getParent()->begin() )
                return llvm::ConstantExpr::getPtrToInt( bb->getParent(), intptrt );
            return llvm::ConstantExpr::getPtrToInt( llvm::BlockAddress::get( bb ), intptrt );
        };
        auto sub32 = [=]( llvm::Constant *a, llvm::Constant *b ) {
            return llvm::ConstantExpr::getTrunc( llvm::ConstantExpr::getNUWSub( a, b ), i32t );
        };
        auto lebConstant = [this]( int val, int size = -1 ) {
            std::string str;
            pushLeb_n( str, val, size < 0 ? lebEncSize( val ) : size );
            return llvm::ConstantDataArray::getString( *_ctx, str, false );
        };

        auto *startaddr = intbbpc( _fn->begin() );
        std::vector< llvm::Constant * > callSiteEntries;
        for ( auto &ci : _callSites ) {
            auto *bb = ci.pc->getParent();
            auto *callPC = sub32( intbbpc( bb ), startaddr ),
                 *landingBlock = sub32( intbbpc( ci.landingBlock ), startaddr );

            // entry must be aligned, so index is 32 bits
            auto *actionEntry = lebConstant( actionIndex[ &ci.actions ], 4 );
            callSiteEntries.emplace_back( llvm::ConstantStruct::getAnon(
                                              { callPC,
                                                llvm::ConstantInt::get( i32t, 1 ),
                                                landingBlock,
                                                actionEntry }, true ) );
        }
        auto *callSiteArray = llvm::ConstantStruct::getAnon( callSiteEntries, true );

        std::vector< llvm::Constant * > tablesElems = { callSiteArray, actTableArray };
        if ( typeInfosArray ) {
            tablesElems.emplace_back( typeInfosArray );
            tablesElems.emplace_back( exSpecArray );
        }
        // not packed, padding might be added at the end of action table to alight type infor pointers
        auto *tables = llvm::ConstantStruct::getAnon( tablesElems );

        auto *chart = llvm::Type::getInt8Ty( *_ctx );
        std::vector< llvm::Constant ** > headerEntries;
        std::vector< llvm::Type * >headerElemTypes;

        llvm::Constant *lpStartEncoding = llvm::ConstantInt::get( chart, dwarf::DW_EH_PE_omit );
        llvm::Constant *ttypeEncoding = llvm::ConstantInt::get( chart, dwarf::DW_EH_PE_absptr );
        llvm::Constant *classInfoOffset = lebConstant( 0, 4 ); // filled later
        llvm::Constant *callSiteEncoding = llvm::ConstantInt::get( chart, dwarf::DW_EH_PE_udata4 );
        headerEntries = { &lpStartEncoding, /* &lpStart, */ &ttypeEncoding,
                          &classInfoOffset, &callSiteEncoding };
        std::transform( headerEntries.begin(), headerEntries.end(),
                        std::back_inserter( headerElemTypes ),
                        []( auto **v ) { return (*v)->getType(); } );
        // we must make sure tables are aligned, but header needs to be packed, so align it explicitly
        auto tmpHSize = _dl.getTypeAllocSize( llvm::StructType::get( *_ctx, headerElemTypes, true ) );
        auto hSize = ((tmpHSize + 4 /* for callSiteTableLength */ + 7) / 8) * 8;
        ASSERT_LEQ( 4, hSize - tmpHSize );
        ASSERT_LT( hSize - tmpHSize, 12 );
        ASSERT_EQ( hSize % 8, 0 );
        int callSiteTableLengthLength = hSize - tmpHSize;
        auto *callSiteTableLength = lebConstant( 0, callSiteTableLengthLength );
        headerEntries.emplace_back( &callSiteTableLength );
        headerElemTypes.emplace_back( callSiteTableLength->getType() );

        auto *headert = llvm::StructType::get( *_ctx, headerElemTypes, true );
        auto *lsdat = llvm::StructType::get( *_ctx, { headert, tables->getType() } );

        auto getOffset = [=]( llvm::Type *t, std::initializer_list< int > indices ) -> int {
            std::vector< llvm::Value * > idx { llvm::ConstantInt::get( llvm::Type::getInt64Ty( *_ctx ), 0 ) };
            for ( auto i : indices )
                idx.emplace_back( llvm::ConstantInt::get( llvm::Type::getInt32Ty( *_ctx ), i ) );
            int o = _dl.getIndexedOffset( llvm::PointerType::get( t, 0 ), idx );
            return o;
        };

        auto classInfoOffsetStart = getOffset( lsdat, { 0, 3 } );
        if ( typeInfosArray )
            classInfoOffset = lebConstant( getOffset( lsdat, { 1, 3 } ) - classInfoOffsetStart, 4 );
        callSiteTableLength = lebConstant( getOffset( lsdat, { 1, 1 } ) - hSize,
                                           callSiteTableLengthLength );

        std::vector< llvm::Constant * > headerConsts;
        std::transform( headerEntries.begin(), headerEntries.end(),
                        std::back_inserter( headerConsts ),
                        []( auto **v ) { return *v; } );
        auto *header = llvm::ConstantStruct::get( headert, headerConsts );
        auto *lsda = llvm::ConstantStruct::get( lsdat, { header, tables } );
        return lsda;
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

    int typeID( llvm::Constant *c ) {
        auto it = std::find( _typeInfos.begin(), _typeInfos.end(), c );
        return it == _typeInfos.end() ? 0 : it - _typeInfos.begin() + 1;
    }

    void addID( llvm::Constant *ti ) {
        if ( !typeID( ti ) )
            _typeInfos.emplace_back( ti );
    }

    void addSpec( llvm::Constant *spec ) {
        auto it = std::find( _exceptSpecs.begin(), _exceptSpecs.end(), spec );
        if ( it == _exceptSpecs.end() )
            _exceptSpecs.emplace_back( spec );
    }

    template< typename Yield >
    void iterateFilter( llvm::Constant *clause, Yield yield )
    {
        if( auto *array = llvm::dyn_cast< llvm::ConstantArray >( clause ) )
        {
            auto cnt = array->getType()->getNumElements();
            for ( unsigned j = 0; j < cnt; ++j ) {
                auto *ti = array->getOperand( j )->stripPointerCasts();
                yield( ti );
            }
        }
        else if ( llvm::isa< llvm::ConstantAggregateZero >( clause ) )
        {
            /* throw () */
        }
        else {
            clause->dump();
            UNREACHABLE( "Unexpected landingpad clause type" );
        }
    }

    llvm::LLVMContext *_ctx;
    llvm::DataLayout _dl;
    llvm::Function *_fn;

    std::vector< CallSite > _callSites;
    std::vector< llvm::Constant * > _typeInfos;
    std::vector< llvm::Constant * > _exceptSpecs;
};

}
}
