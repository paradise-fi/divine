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

#include <divine/sim/command.hpp>

#include <divine/mc/bitcode.hpp>
#include <divine/mc/builder.hpp>
#include <divine/vm/memory.hpp>
#include <divine/vm/program.hpp>
#include <divine/dbg/stepper.hpp>
#include <divine/dbg/context.hpp>
#include <divine/dbg/node.hpp>

#include <brick-cmd>
#include <brick-proc>
#include <random>
#include <histedit.h>

namespace divine::str::doc { extern const std::string_view manual_sim_md; }

namespace divine::sim
{

namespace cmd = brick::cmd;

using Context = dbg::Context< vm::CowHeap >;
using DN = dbg::Node< vm::Program, vm::CowHeap >;
using BC = std::shared_ptr< mc::BitCode >;
using PointerV = Context::PointerV;
using Stepper = dbg::Stepper< Context >;
using RefCnt = brick::mem::RefCnt< typename Context::RefCnt >;
using Snapshot = vm::CowHeap::Snapshot;

struct OneLineTokenizer
{
    OneLineTokenizer() : _tok( tok_init( nullptr ) ) {}
    ~OneLineTokenizer() { tok_end( _tok ); }

    cmd::Tokens tokenize( const std::string& s )
    {
        int argc;
        const char **argv;
        tok_reset( _tok );
        int r = tok_str( _tok, s.c_str(), &argc, &argv );
        if ( r == -1 )
            throw brick::except::Error{ "Uknown tokenizer error" };
        if ( r > 0 )
            throw brick::except::Error{ "Unmatched quotes" };
        cmd::Tokens ret;
        std::copy_n( argv, argc, std::back_inserter( ret ) );
        return ret;
    }

    ::Tokenizer *_tok;
};

struct CLI
{
    bool _exit, _batch;
    BC _bc;

    std::vector< std::string > _env;

    std::map< std::string, DN > _dbg;
    std::map< Snapshot, RefCnt > _state_refs;
    std::map< Snapshot, std::string > _state_names;
    std::map< Snapshot, vm::Step > _trace;
    mc::ExplicitBuilder _explore;

    std::pair< int, int > _sticky_tid;
    std::mt19937 _rand;
    bool _sched_random, _pygmentize = false;
    using Comp = dbg::Component;
    using Components = dbg::Components;
    Components _ff_components = ~Components( Comp::Program );

    std::vector< cmd::Tokens > _sticky_commands;

    Context _ctx;

    using RefLocation = std::pair< llvm::StringRef, int >;
    using Location = std::pair< std::string, int >;
    using Breakpoint = brick::types::Union< vm::CodePointer, Location >;
    std::vector< Breakpoint > _bps;

    char *_prompt;
    int _state_count;

    static bool *_sigint;

    std::map< std::string, cmd::Tokens > _info_cmd;
    std::map< std::string, brick::proc::XTerm > _xterms;
    std::ostream *_stream;
    std::ostream &out() { return *_stream; }

    void command( cmd::Tokens cmd );
    void command_raw( cmd::Tokens cmd );
    char *prompt() { return _prompt; }

    RefLocation location( vm::CodePointer pc )
    {
        auto npc = _bc->program().nextpc( pc );
        auto insn = _bc->debug().find( nullptr, npc ).first;
        return dbg::fileline( *insn );
    };

    DN dn( vm::GenericPointer p, dbg::DNKind k, llvm::Type *t, llvm::DIType *dit, bool exec = false )
    {
        DN r( _ctx, _ctx.snapshot() );
        r.address( k, p, exec );
        r.type( t );
        r.di_type( dit );
        return r;
    }

    DN nullDN() { return dn( vm::nullPointer(), dbg::DNKind::Object, nullptr, nullptr ); }
    DN frameDN() { return dn( _ctx.frame(), dbg::DNKind::Frame, nullptr, nullptr, true ); }

    DN objDN( vm::GenericPointer p, llvm::Type *t, llvm::DIType *dit )
    {
        return dn( p, dbg::DNKind::Object, t, dit );
    }

    void set( std::string n, DN dn ) { _dbg.erase( n ); _dbg.emplace( n, dn ); }
    void set( std::string n, std::string value, bool silent = false )
    {
        set( n, get( value, silent ) );
    }

    DN get( std::string n, bool silent = false,
            std::unique_ptr< DN > start = nullptr, bool comp = false );

    void update();

    void info()
    {
        auto top = get( "$top" ), frame = get( "$frame" );
        auto sym = top.attribute( "symbol" ), loc = top.attribute( "location" );
        out() << "# executing " << sym;
        if ( sym.size() + loc.size() > 60 && !_batch )
            out() << std::endl << "#        at ";
        else
            out() << " at ";
        out() << loc << std::endl;
        if ( frame._address != top._address )
            out() << "# NOTE: $frame in " << frame.attribute( "symbol" ) << std::endl;
    }

    CLI( BC bc )
        : _exit( false ), _batch( false ), _bc( bc ), _explore( bc ), _sticky_tid( -1, 0 ),
          _sched_random( false ), _ctx( _bc->program(), _bc->debug() ),
          _state_count( 0 ), _stream( &std::cerr )
    {
        _ctx._lock_mode = Context::LockScheduler;
        vm::setup::boot( _ctx );
        _prompt = strdup( "> " );
        set( "$_", nullDN() );
        update();
    }

    bool update_lock( vm::CowHeap::Snapshot snap );
    void sched_policy();
    bool check_bp( RefLocation initial, vm::CodePointer pc, bool ch );
    Snapshot newstate( Snapshot snap, bool update_choices = true, bool terse = false );

    void check_running()
    {
        if ( _ctx.frame().null() )
            throw brick::except::Error( "the program has already terminated" );
    }

    void run( Stepper &step, bool verbose );
    void run( Stepper &step, Stepper::Verbosity verbose );
    Stepper stepper();
    Stepper stepper( command::WithSteps s, bool jmp );
    void reach_user();
    void reach_error();
    void trace( sim::Trace, bool, bool, std::function< void() > );

    void bplist( command::Break b );
    void dump_registers();

    DN frame_up( DN frame );

    /* all of the go() variants are implemented in dispatch.cpp */
    void go( command::Exit );
    void go( command::Start s );
    void go( command::Break b );
    void go( command::Step s );
    void go( command::StepI s );
    void go( command::StepA s );
    void go( command::Rewind re );
    void go( command::BackTrace bt );
    void go( command::Show cmd );
    void go( command::Diff );
    void go( command::Dot cmd );
    void go( command::Inspect i );
    void go( command::Call c );
    void go( command::Info i );

    void go( command::Up );
    void go( command::Down );
    void go( command::Set s );
    void go( command::Trace tr );
    void go( command::Thread thr );
    void go( command::BitCode bc );
    void go( command::Source src );
    void go( command::Setup set );
    void go( command::Help ) { UNREACHABLE( "impossible case" ); }

    template< typename Parser >
    void help( Parser &p, std::string arg )
    {
        if ( arg.empty() )
            out() << str::doc::manual_sim_md << std::endl
                  << "# Command overview" << std::endl << std::endl;
        out() << p.describe( arg ) << std::endl;
    }

    void prepare( const command::Teflon &tfl )
    {
        if ( !tfl.output_to.empty() )
        {
            if ( _xterms.count( tfl.output_to ) )
                _stream = &_xterms[ tfl.output_to ].stream();
            else
                out() << "ERROR: no xterm named " << tfl.output_to << " found!";
        }

        if ( tfl.clear_screen )
            out() << char( 27 ) << "[2J" << char( 27 ) << "[;H";
    }

    void prepare( command::CastIron ) {}
    void finalize( command::Teflon ) { update(); }
    void finalize( command::CastIron );
};

}
