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

using Context = dbg::Context< vm::CowHeap >;
using DN = dbg::Node< vm::Program, vm::CowHeap >;
using BC = std::shared_ptr< mc::BitCode >;
using PointerV = Context::PointerV;
using Stepper = dbg::Stepper< Context >;
using Snapshot = vm::CowHeap::Snapshot;
using cmd_tokens = std::vector< std::string >;

struct OneLineTokenizer
{
    OneLineTokenizer() : _tok( tok_init( nullptr ) ) {}
    ~OneLineTokenizer() { tok_end( _tok ); }

    cmd_tokens tokenize( const std::string& s )
    {
        int argc;
        const char **argv;
        tok_reset( _tok );
        int r = tok_str( _tok, s.c_str(), &argc, &argv );
        if ( r == -1 )
            throw brq::error{ "Uknown tokenizer error" };
        if ( r > 0 )
            throw brq::error{ "Unmatched quotes" };
        cmd_tokens ret;
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
    std::map< Snapshot, Context::RefCnt > _state_refs;
    std::map< Snapshot, std::string > _state_names;
    std::map< Snapshot, vm::Step > _trace;
    mc::ExplicitBuilder _explore;

    std::pair< int, int > _sticky_tid;
    std::mt19937 _rand;
    bool _sched_random, _pygmentize = false;
    using comp = dbg::component;
    using Components = dbg::Components;
    Components _ff_components = ~Components( comp::program );

    std::vector< cmd_tokens > _sticky_commands;

    Context _ctx;

    using RefLocation = std::pair< llvm::StringRef, int >;
    using Location = std::pair< std::string, int >;
    using Breakpoint = brick::types::Union< vm::CodePointer, Location >;
    std::vector< Breakpoint > _bps;

    char *_prompt;
    int _state_count;

    static bool *_sigint;

    std::map< std::string, cmd_tokens > _info_cmd;
    std::map< std::string, brick::proc::XTerm > _xterms;
    std::ostream *_stream;
    std::ostream &out() { return *_stream; }

    void command( cmd_tokens cmd );
    void command_raw( cmd_tokens cmd );
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
        _ctx.set_pool( _explore.pool() );
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
            throw brq::error( "the program has already terminated" );
    }

    void run( Stepper &step, bool verbose );
    void run( Stepper &step, Stepper::Verbosity verbose );
    Stepper stepper();
    Stepper stepper( command::with_steps s, bool jmp );
    void reach_user();
    void reach_error();
    void trace( sim::Trace, bool, std::function< void() > );

    void bplist( command::breakpoint b );
    void dump_registers();
    void dump_functions();

    DN frame_up( DN frame );

    /* all of the go() variants are implemented in dispatch.cpp */
    void go( command::exit );
    void go( command::start s );
    void go( command::breakpoint b );
    void go( command::step s );
    void go( command::stepi s );
    void go( command::stepa s );
    void go( command::rewind re );
    void go( command::backtrace bt );
    void go( command::show cmd );
    void go( command::diff );
    void go( command::dot cmd );
    void go( command::inspect i );
    void go( command::call c );
    void go( command::info i );

    void go( command::up );
    void go( command::down );
    void go( command::set s );
    void go( command::thread thr );
    void go( command::bitcode bc );
    void go( command::source src );
    void go( command::setup set );
    void go( brq::cmd_help ) {}

    template< typename Parser >
    void help( Parser &p, std::string arg )
    {
        if ( arg.empty() )
            out() << str::doc::manual_sim_md << std::endl
                  << "# Command overview" << std::endl << std::endl;
        out() << p.describe( arg ) << std::endl;
    }

    void prepare( const command::teflon &tfl )
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

    void prepare( command::cast_iron ) {}
    void prepare( brq::cmd_help ) {}
    void finalize( command::teflon ) { update(); }
    void finalize( command::cast_iron );
    void finalize( brq::cmd_help ) {}
};

}
