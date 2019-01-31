// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2017 Petr Ročkai <code@fixp.eu>
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
#include <divine/sim/trace.hpp>
#include <divine/dbg/info.hpp>
#include <string>

namespace divine::sim::command
{

struct CastIron {}; // finalize with sticky commands
struct Teflon       // finalize without sticky commands
{
    std::string output_to;
    bool clear_screen = false;
};

struct WithVar
{
    std::string var;
    WithVar( std::string v = "$_" ) : var( v ) {}
};

struct WithFrame : WithVar
{
    WithFrame() : WithVar( "$frame" ) {}
};

struct Set : CastIron { std::vector< std::string > options; };
struct WithSteps : WithFrame
{
    bool over, out, quiet, verbose; int count;
    WithSteps() : over( false ), out( false ), quiet( false ), verbose( false ), count( 1 ) {}
};

struct Start : CastIron
{
    bool verbose = false;
    bool noboot = false;
};

struct Break : Teflon
{
    std::vector< std::string > where;
    bool list;
    brick::types::Union< std::string, int > del;
    Break() : list( false ) {}
};

struct StepI : WithSteps, CastIron {};
struct StepA : WithSteps, CastIron {};
struct Step : WithSteps, CastIron {};
struct Rewind : WithVar, CastIron
{
    Rewind() : WithVar( "#last" ) {}
};

struct Show : WithVar, Teflon
{
    bool raw;
    int depth, deref;
    Show() : raw( false ), depth( 10 ), deref( 0 ) {}
};

struct Diff : WithVar, Teflon
{
    std::vector< std::string > vars;
};

struct Dot : WithVar, Teflon
{
    std::string type = "none";
    std::string output_file;
};

struct Draw : Dot
{
    Draw() { type = "x11"; }
};

struct Inspect : Show {};
struct BitCode : WithFrame, Teflon {};
struct Source : WithFrame, Teflon {};
struct Call : Teflon
{
    std::string function;
};

struct Info : Teflon
{
    std::string cmd;
    std::string setup;
};

struct Thread : CastIron  { std::string spec; bool random; };

struct BackTrace : WithVar, Teflon
{
    BackTrace() : WithVar( "$top" ) {}
};

struct Setup : Teflon
{
    bool clear_sticky, pygmentize = false, debug_everything = false;
    std::string xterm;
    std::vector< std::string > sticky_commands;
    std::vector< dbg::Component > debug_components, ignore_components;
    Setup() : clear_sticky( false ) {}
};

struct Down : CastIron {};
struct Up : CastIron {};

struct Exit : Teflon
{
    static std::array< std::string, 2 > names()
    {
        return { { std::string( "exit" ), std::string( "quit" ) } };
    };
};

struct Help : Teflon { std::string _cmd; };

}
