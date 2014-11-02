// -*- C++ -*- (c) 2007-2014 Petr Rockai <me@mornfall.net>

#ifdef __unix
#include <dlfcn.h>
#elif defined _WIN32
#include <external/dlfcn-win32/dlfcn.h>
#endif
#include <stdexcept>

#include <brick-string.h>
#include <divine/generator/common.h>

#ifndef DIVINE_GENERATOR_CESMI_H
#define DIVINE_GENERATOR_CESMI_H

/*
 * Add alloc function to setup, to make the interface more opaque.
 */

namespace divine {
namespace generator {

static constexpr const char * const cesmi_ext =
#if defined(_WIN32)
    ".dll";
#else
    ".so";
#endif

namespace cesmi {
#include <divine/cesmi/usr-cesmi.h>
}

/**
 * A binary model loader. The model is expected to be a shared object (i.e. .so
 * on ELF systems, and a .dll on win32) and conform to the CESMI specification.
 */

struct CESMI : public Common< Blob > {
    typedef Blob Node;
    typedef generator::Common< Blob > Common;
    typedef typename Common::Label Label;
    std::string file;

    struct Dl {
        void *handle;

        decltype( &cesmi::setup             ) setup;

        decltype( &cesmi::get_initial       ) get_initial;
        decltype( &cesmi::get_successor     ) get_successor;
        decltype( &cesmi::get_flags         ) get_flags;

        decltype( &cesmi::show_node         ) show_node;
        decltype( &cesmi::show_transition   ) show_transition;

        Dl() :
            setup( nullptr ),
            get_initial( nullptr ),
            get_successor( nullptr ),
            get_flags( nullptr ),
            show_node( nullptr ),
            show_transition( nullptr )
        {}
    } dl;

    cesmi::cesmi_setup setup;

    cesmi::cesmi_node data( Blob b ) {
        cesmi::cesmi_node n;
        n.handle = b.raw(),
        n.memory = pool().valid( b ) ? ( pool().dereference( b ) + slack() ) : nullptr;
        return n;
    }

    template< typename Yield, typename Get >
    void _successors( Node s, Yield yield, Get get )
    {
        int handle = 1, old = 1;
        call_setup();
        while ( handle ) {
            cesmi::cesmi_node state;
            handle = get( &setup, handle, data( s ), &state );
            if ( handle )
                yield( Blob::fromRaw( state.handle ), old );
            old = handle;
        }
    }

    template< typename Yield >
    void successors( Node s, Yield yield ) {
        if ( !pool().valid( s ) )
            return;
        _successors( s, [yield]( Node n, int ) { yield( n, Label() ); }, dl.get_successor );
    }

    template< typename Yield >
    void initials( Yield yield )
    {
        _successors( Node(), [yield]( Node n, int ) { yield( Node(), n, Label() ); },
                     [this]( cesmi::cesmi_setup *s, int h,
                             cesmi::cesmi_node, cesmi::cesmi_node *to ) -> int
                     {
                         return this->dl.get_initial( s, h, to );
                     } );
    }

    void die( const char *fmt, ... ) __attribute__((noreturn)) {
        va_list ap;
        va_start( ap, fmt );
        vfprintf( stderr, fmt, ap );
        exit( 1 );
    }

    template< typename T >
    void getsym( T *result, const char *name ) {
        static_assert( sizeof( T ) == sizeof( void * ), "pointer size mismatch" );
        union { T sym; void *ptr; };
        ptr = dlsym( dl.handle, name );
        *result = sym;
    }

    void read( std::string path, std::vector< std::string > /* definitions */,
            CESMI *blueprint = nullptr )
    {
        if ( blueprint ) {
            dl = blueprint->dl;
            return;
        }

        // dlopen does not like path-less shared objects outside lib locations
        if ( brick::string::basename( path ) == path )
            path = "./" + path;

        dl.handle = dlopen( path.c_str(), RTLD_LAZY );

        if( !dl.handle )
            die( "FATAL: Error loading \"%s\".\n%s", path.c_str(), dlerror() );

        getsym( &dl.setup, "setup" );

        getsym( &dl.get_initial, "get_initial" );
        getsym( &dl.get_successor, "get_successor" );
        getsym( &dl.get_flags , "get_flags" );

        getsym( &dl.show_node, "show_node" );
        getsym( &dl.show_transition, "show_transition" );

        if ( !dl.get_initial )
            die( "FATAL: Could not resolve get_initial." );
        if ( !dl.get_successor )
            die( "FATAL: Could not resolve get_successor." );
    }

    static cesmi::cesmi_node make_node( const cesmi::cesmi_setup *setup, int size ) {
        CESMI *_this = reinterpret_cast< CESMI * >( setup->loader );
        Blob b = _this->makeBlob( size );
        cesmi::cesmi_node n;
        n.memory = _this->pool().dereference( b ) + _this->slack();
        n.handle = b.raw();
        return n;
    }

    static cesmi::cesmi_node clone_node( const cesmi::cesmi_setup *setup, cesmi::cesmi_node orig ) {
        CESMI *_this = reinterpret_cast< CESMI * >( setup->loader );
        Blob origb = Blob::fromRaw( orig.handle );
        Blob b = _this->pool().allocate( _this->pool().size( origb ) );

        int slack = _this->slack();
        _this->pool().copy( origb, b );
        _this->pool().clear( b, 0, slack );
        cesmi::cesmi_node n;
        n.memory = _this->pool().dereference( b ) + slack;
        n.handle = b.raw();
        return n;
    }

    struct Property {
        std::string id, description;
        graph::PropertyType type;
        int seqno;
        Property( const char *id, const char *desc )
            : id ( id ? id : "" ), description( desc ? desc : "" )
        {}
    };

    std::vector< Property > _properties;

    static int add_property( cesmi::cesmi_setup *setup, char *id, char *desc, int type )
    {
        CESMI *_this = reinterpret_cast< CESMI * >( setup->loader );

        Property p( id, desc );

        ::free( id );
        ::free( desc );

        std::string defd;

        switch ( type ) {
            case cesmi::cesmi_pt_goal:
                p.type = PT_Goal;
                defd = "safety";
                break;
            case cesmi::cesmi_pt_deadlock:
                p.type = PT_Deadlock;
                defd = "deadlock freedom";
                break;
            case cesmi::cesmi_pt_buchi:
                p.type = PT_Buchi;
                defd = "neverclaim / LTL verification";
                break;
            default: ASSERT_UNREACHABLE_F( "unknown CESMI property type %d", type );
        }

        p.seqno = _this->_properties.size();

        if ( p.id.empty() )
            p.id = "p_" + brick::string::fmt( p.seqno + 1 );

        if ( p.description.empty() )
            p.description = defd;

        _this->_properties.push_back( p );
        setup->property_count ++;

        ASSERT_EQ( setup->property_count, int( _this->_properties.size() ) );
        return p.seqno;
    }

    void call_setup()
    {
        if ( setup.instance_initialised )
            return;

        _properties.clear();
        setup.property = 0;
        setup.property_count = 0;
        setup.loader = this;
        setup.make_node = &make_node;
        setup.clone_node = &clone_node;
        setup.add_property = &add_property;
        setup.instance = 0;
        setup.instance_initialised = 0;

        if ( dl.setup )
            dl.setup( &setup );

        setup.instance_initialised = 1;
    }

    template< typename Y >
    void properties( Y yield )
    {
        call_setup();
        for ( auto p : _properties )
            yield( p.id, p.description, p.type );
    }

    void useProperties( PropertySet s )
    {
        if ( s.size() != 1 )
            throw std::logic_error( "DVE only supports singleton properties" );

        std::string n = *s.begin();

        call_setup();
        for ( auto p : _properties )
            if ( n == p.id ) {
                setup.property = p.seqno;
                return;
            }

        throw std::logic_error( "Unknown property " + n + ". Please consult divine info." );
    }

    CESMI() {
        setup.instance_initialised = 0;
    }

    CESMI( const CESMI &other ) {
        dl = other.dl;
        setup.instance_initialised = 0;
    }

    uint64_t flags( Node s ) {
        if ( dl.get_flags )
            return dl.get_flags( &setup, data( s ) );
        else
            return 0;
    }

    bool isGoal( Node s ) { return flags( s ) & cesmi::cesmi_goal; }
    bool isAccepting( Node s ) { return flags( s ) & cesmi::cesmi_accepting; }

    std::string showNode( Node s ) {
        if ( dl.show_node ) {
            char *fmt = dl.show_node( &setup, data( s ) );
            std::string s( fmt );
            ::free( fmt );
            return s;
        } else
            return "()";
    }

    std::string showTransition( Node from, Node to, Label ) {
        char *fmt = nullptr;

        if ( dl.show_transition && pool().valid( from ) ) {
            _successors( from, [&]( Node n, int handle ) {
                    if ( pool().equal( to, n, slack() ) )
                        fmt = this->dl.show_transition( &this->setup, this->data( from ), handle );
                }, dl.get_successor );
        }

        if ( fmt ) {
            std::string s( fmt );
            ::free( fmt );
            return s;
        } else
            return "";
    }

    void release( Node s ) { pool().free( s ); }
};

}
}

#endif
