// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
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
#if OPT_SQL

#include <divine/ui/log.hpp>
#include <external/nanodbc/nanodbc.h>
#include <brick-types>

namespace divine::ui {
namespace odbc {

using Keys = std::vector< std::string >;
using Vals = std::vector< brick::types::Union< std::string, std::vector< uint8_t >, int > >;

struct ExternalBuildInfo
{
    std::string driver;
    std::string driver_checksum;
    std::string checksum;
    std::string version;
    std::string build_type;
};

void bind_vals( nanodbc::statement &stmt, Vals &vals );
nanodbc::statement insert( nanodbc::connection conn, std::string table, Keys keys, Vals &vals );
nanodbc::statement select_id( nanodbc::connection conn, std::string table, Keys keys, Vals &vals );
int unique_id( nanodbc::connection conn, std::string table, Keys keys, Vals vals );
int get_build( nanodbc::connection conn );
int get_external_build( nanodbc::connection conn, ExternalBuildInfo & );
int get_machine( nanodbc::connection conn );
int add_execution( nanodbc::connection conn );
void update_execution( nanodbc::connection conn, int id, const char *result, int lart, int load,
                       int boot, int search, int ce, int states );

}

SinkPtr make_odbc( std::string connstr );

}

#endif
