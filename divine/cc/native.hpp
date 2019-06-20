// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
#pragma once

/*
 * (c) 2019 Zuzana Baranová <xbaranov@fi.muni.cz>
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

#include <divine/cc/cc1.hpp>
#include <divine/cc/options.hpp>

namespace divine::cc
{
    /* PairedFiles is used for mapping IN => OUT filenames at compilation,
     * or checking whether to compile a given file,
     * i.e. file.c => compiled to file.o; file.o => will not be compiled,
     * similarly for archives; the naming can further be affected by the -o
     * option.
     */
    using PairedFiles = std::vector< std::pair< std::string, std::string > >;

    struct Native
    {
        int compileFiles();

        cc::ParsedOpts _po;
        PairedFiles _files;
        std::vector< std::string > _ld_args;
        cc::CC1 _clang;
        bool _cxx;
    };
}
