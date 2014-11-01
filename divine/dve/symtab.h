// -*- C++ -*- (c) 2011 Petr Rockai
#include <divine/dve/parse.h>
#include <divine/dve/flag.h>

#ifndef DIVINE_DVE_SYMTAB_H
#define DIVINE_DVE_SYMTAB_H

namespace divine {
namespace dve {

struct Channel;
std::ostream & dumpChannel( std::ostream &os, Channel * chan, char * data);

struct NS {
    enum Namespace { Process, Variable, Channel, State, Initialiser, InitState, Flag };
};

typedef int SymId;

struct SymContext : NS {
    int offset; // allocation
    int id;

    std::vector< int > constants;

    struct Item {
        size_t offset:30;
        size_t width:4;
        size_t array:8;
        bool is_array:1;
        bool is_constant:1;
        bool is_compound:1;
        Item() : array( 1 ), is_array( false ),
                 is_constant( false ), is_compound( false ) {}
    };

    std::vector< Item > ids;

    SymContext() : offset( 0 ), id( 0 ) {}
};

struct Symbol {
    SymContext *context;
    SymId id;

    Symbol() : context( 0 ), id( 0 ) {}
    Symbol( SymContext *ctx, SymId id ) : context( ctx ), id( id ) {}

    bool valid() { return context; }

    bool operator<( const Symbol &o ) const {
        if ( context < o.context )
            return true;
        if ( context == o.context && id < o.id )
            return true;
        return false;
    }

    bool operator==( const Symbol &o ) const {
        return id == o.id && context == o.context;
    }

    bool operator!=( const Symbol &o ) const {
        return id != o.id || context != o.context;
    }

    SymContext::Item &item() {
        ASSERT( context );
        ASSERT_LEQ( size_t( id + 1 ), context->ids.size() );
        return context->ids[ id ];
    }

    int deref( char *mem = 0, int idx = 0 ) {
        if ( item().is_constant )
            return context->constants[ item().offset + idx ];
        else {
            if ( idx )
                ASSERT( item().is_array );
            char *place = mem + item().offset + idx * item().width;
            switch ( item().width ) {
                case 1: return *reinterpret_cast< uint8_t * >( place );
                case 2: return *reinterpret_cast< int16_t * >( place );
                case 4: return *reinterpret_cast< uint32_t * >( place );
                default: ASSERT_UNREACHABLE_F( "wrong deref width %d", item().width );
            }
        }
    }

    template< typename T >
    void deref( char *mem, int idx, T &retval ) {
        ASSERT( !item().is_constant );
        if ( idx )
            ASSERT( item().is_array );
        char *place = mem + item().offset + idx * item().width;
        retval = *reinterpret_cast< T * >( place );
    }

    char * getref( char *mem = 0, int idx = 0 ) {
        ASSERT( !item().is_constant );
        if ( idx )
            ASSERT( item().is_array );
        char *place = mem + item().offset + idx * item().width;
        return place;
    }

    void set( char *mem, int idx, int value, ErrorState &err ) {
        ASSERT( !item().is_constant );
        char *place = mem + item().offset + idx * item().width;
        switch ( item().width ) {
            case 1: setval< uint8_t >( place, value, err ); break;
            case 2: setval< int16_t >( place, value, err ); break;
            case 4: setval< uint32_t >( place, value, err ); break;
            default: ASSERT_UNREACHABLE_F( "wrong set width %d", item().width );
        }
    }

    template< typename T >
    inline void setval( char * place, int value, ErrorState &err ) {
        if ( value != static_cast<int>(static_cast<T>(value)) )
            err |= ErrorState::i_overflow;
        *reinterpret_cast< T * >( place ) = value;
    }


    template< typename T >
    void set( char *mem, int idx, const T value ) {
        ASSERT( !item().is_constant );
        char *place = mem + item().offset + idx * item().width;
        *reinterpret_cast< T * >( place ) = value;
    }

};

std::ostream &operator<<( std::ostream &os, Symbol s );

struct SymTab : NS {
    typedef std::map< std::string, SymId > Tab;
    std::vector< Tab > tabs;
    std::map< std::string, divine::dve::Channel*> channels;

    SymContext *context;
    SymTab *parent; // scoping
    std::map< Symbol, SymTab * > children;

    SymId newid( Namespace ns, std::string name ) {
        ASSERT( context );
        ASSERT( !tabs[ ns ].count( name ) );

        unsigned id = context->id ++;
        context->ids.resize( id + 1 );
        tabs[ ns ][ name ] = id;
        return id;
    }

    Symbol lookup( Namespace ns, std::string id ) const {
        if ( tabs[ ns ].count( id ) )
            return Symbol( context, tabs[ ns ].find( id )->second );
        if ( parent )
            return parent->lookup( ns, id );
        return Symbol();
    }

    Symbol lookup( Namespace ns, const parse::Identifier id ) const {
        return lookup( ns, id.name() );
    }

    divine::dve::Channel* lookupChannel( std::string id ) const {
        if ( channels.count( id ) )
            return channels.find( id )->second;
        if ( parent )
            return parent->lookupChannel( id );
        return 0;
    }

    divine::dve::Channel* lookupChannel( const parse::Identifier id ) const {
        return lookupChannel( id.name() );
    }

    Symbol allocate( Namespace ns, std::string name, int width )
    {
        unsigned id = newid( ns, name );
        SymContext::Item &i = context->ids[ id ];
        i.offset = context->offset;
        i.width = width;
        context->offset += width;
        return Symbol( context, id );
    }

    template< typename T >
    Symbol allocate( Namespace ns, const T &decl ) {
        int width = decl.width;

        ASSERT_LEQ( 1, width );
        if ( !decl.is_compound )
            ASSERT_LEQ( width, 4 );

        Symbol s = allocate( ns, decl.name, width * (decl.is_array ? decl.size : 1) );
        s.item().is_array = decl.is_array;
        s.item().width = width;
        s.item().array = decl.size;
        s.item().is_compound = decl.is_compound;
        return s;
    }

    Symbol constant( Namespace ns, std::string name, int value ) {
        unsigned id = newid( ns, name );
        SymContext::Item &i = context->ids[ id ];
        std::vector< int > &c = context->constants;
        i.offset = c.size();
        i.is_constant = true;
        c.resize( c.size() + 1 );
        c[ i.offset ] = value;
        return Symbol( context, id );
    }

    Symbol constant( Namespace ns, std::string name, std::vector< int > vals ) {
        unsigned id = newid( ns, name );
        SymContext::Item &i = context->ids[ id ];
        std::vector< int > &c = context->constants;
        i.offset = c.size();
        i.is_constant = true;
        i.is_array = true;
        i.array = vals.size();
        std::copy( vals.begin(), vals.end(), std::back_inserter( c ) );
        return Symbol( context, id );
    }

    const SymTab *child( Symbol id ) const {
        ASSERT( children.find( id ) != children.end() );
        ASSERT( children.find( id )->second );
        return children.find( id )->second;
    }

    const SymTab *toplevel() const {
        if ( parent )
            return parent->toplevel();
        return this;
    }

    SymTab( SymTab *parent = 0 ) : parent( parent ) {
        tabs.resize( 7 ); // one for each namespace
        if ( !parent ) {
            context = new SymContext();
            allocate( Flag, "Error", sizeof( ErrorState ) );
            allocate( Flag, "Flags", sizeof( StateFlags ) );
        }
        else
            context = parent->context;
    }

    std::ostream& dump( std::ostream &o, char *mem ) const {
        ErrorState err;
        lookup( Flag, "Error" ).deref( mem, 0, err );
        if ( err.error ) {
            o << err;
            return o;
        }
        for ( Namespace ns = Process; ns < State; ns = NS::Namespace( ns + 1 ) ) {
            for ( std::map< std::string, int >::const_iterator i = tabs[ ns ].begin();
                  i != tabs[ ns ].end(); ++i ) {
                Symbol s( context, i->second );
                if ( ns == NS::Channel ) {
                    divine::dve::Channel* const chan = channels.find(i->first)->second;
                    dumpChannel( o, chan, s.getref( mem ) );
                }
                else {
                    o << i->first << " = " << (s.item().is_array ? "[" : "");
                    o << s.deref( mem );
                    for ( int j = 1; j < s.item().array; j++ )
                        o << ", " << s.deref( mem, j );
                    o << (s.item().is_array ? "], " : ", ");
                }
                if (ns == Process) {
                    child(s)->dump(o, mem);
                }
            }
        }

        if ( parent )
            return o << std::endl;

        StateFlags flags;
        lookup( Flag, "Flags" ).deref( mem, 0, flags );
        return o << std::endl << flags << std::endl;
    }
};

}
}

#endif
