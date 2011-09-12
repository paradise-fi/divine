// -*- C++ -*- (c) 2011 Petr Rockai
#include <divine/dve/parse.h>

#ifndef DIVINE_DVE_SYMTAB_H
#define DIVINE_DVE_SYMTAB_H

namespace divine {
namespace dve {

struct NS {
    enum Namespace { Process, Variable, Channel, State };
};

typedef int SymId;

struct SymContext : NS {
    int offset; // allocation
    int id;

    std::vector< int > constants;

    struct Item {
        int offset;
        int width;
        bool is_array:1;
        bool is_constant:1;
        int array;
        Item() : is_array( false ), is_constant( false ), array( 1 ) {}
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
        assert( context );
        assert_leq( size_t( id + 1 ), context->ids.size() );
        return context->ids[ id ];
    }

    template< typename T >
    T deref( char *mem = 0, int idx = 0 ) {
        if ( item().is_constant )
            return context->constants[ item().offset + idx ];
        else {
            int value = *reinterpret_cast< int * >( mem + item().offset + idx * item().width );
            return value;
        }
    }

    template< typename T >
    void set( char *mem, int idx, T value ) {
        assert ( !item().is_constant );
        *reinterpret_cast< T * >( mem + item().offset + idx * item().width ) = value;
    }

};

std::ostream &operator<<( std::ostream &os, Symbol s )
{
    return os << s.id;
}

struct SymTab : NS {
    typedef std::map< std::string, SymId > Tab;
    std::vector< Tab > tabs;
    std::set< SymId > scope;

    SymContext *context;
    SymTab *parent; // scoping
    std::map< Symbol, SymTab * > children;

    SymId newid( Namespace ns, std::string name ) {
        assert( context );
        assert( !tabs[ ns ].count( name ) );

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

    Symbol allocate( Namespace ns, std::string name, int width )
    {
        unsigned id = newid( ns, name );
        SymContext::Item &i = context->ids[ id ];
        i.offset = context->offset;
        i.width = width;
        context->offset += width;
        return Symbol( context, id );
    }

    Symbol allocate( Namespace ns, const parse::Declaration &decl ) {
        int width = 4; // XXX
        Symbol s = allocate( ns, decl.name, width * (decl.is_array ? decl.size : 1) );
        s.item().is_array = decl.is_array;
        s.item().width = width;
        s.item().array = decl.size;
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

    const SymTab *child( Symbol id ) const {
        assert( children.find( id ) != children.end() );
        assert( children.find( id )->second );
        return children.find( id )->second;
    }

    const SymTab *toplevel() const {
        if ( parent )
            return parent->toplevel();
        return this;
    }

    SymTab( SymTab *parent = 0 ) : parent( parent ) {
        if ( !parent )
            context = new SymContext();
        else
            context = parent->context;
        tabs.resize( 4 ); // one for each namespace
    }

    std::ostream& dump(std::ostream &o, char *mem) {
        for ( Namespace ns = Process; ns < State; ns = NS::Namespace( ns + 1 ) ) {
            for ( std::map< std::string, int >::iterator i = tabs[ ns ].begin();
                  i != tabs[ ns ].end(); ++i ) {
                Symbol s( context, i->second );
                o << i->first << " = " << s.deref< short >( mem ) << ", ";
            }
        }
        return o;
    }
};

}
}

#endif
