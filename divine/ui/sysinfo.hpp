// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2016 Petr Ročkai <code@fixp.eu>
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

#include <string>
#include <stdexcept>
#include <functional>

#include <brick-shmem>
#include <brick-string>
#include <brick-except>

#ifndef DIVINE_SYSINFO_H
#define DIVINE_SYSINFO_H

namespace divine {
namespace ui {

struct SysInfo {

    struct Data;
    SysInfo();
    ~SysInfo();

    void update();
    void updateAndCheckTimeLimit( uint64_t time );

    void setMemoryLimitInBytes( uint64_t memory );

    std::string architecture() const;
    uint64_t memory() const;

    uint64_t peakVmSize() const;
    uint64_t vmSize() const;
    uint64_t peakResidentMemSize() const;
    uint64_t residentMemSize() const;

    double userTime() const;
    double systemTime() const;
    double wallTime() const;

    void report( std::function< void ( std::string, std::string ) > );

  private:
    std::unique_ptr< Data > _data;
};

struct ResourceLimit : brq::error
{
    ResourceLimit( std::string x ) : brq::error( "resource exhausted: " + x ) {}
};

struct TimeLimit : ResourceLimit
{
    TimeLimit( int took, int max )
        : ResourceLimit( "time limit (" + std::to_string( took ) + "s / " + std::to_string( max ) + "s)" )
    {}
};

}

}

#endif
