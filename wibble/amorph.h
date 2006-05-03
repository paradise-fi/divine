/** -*- C++ -*-
    @file wibble/amorph.h
    @author Peter Rockai <me@mornfall.net>
*/

#include <iostream> // for noise

#include <wibble/mixin.h>
#include <wibble/cast.h>
#include <wibble/maybe.h>

#ifndef WIBBLE_AMORPH_H
#define WIBBLE_AMORPH_H

namespace wibble {

struct Baseless {};

/**
   @brief An interface implemented by all morph classes
 */
struct MorphInterface {
    virtual MorphInterface *constructCopy( void *where = 0, unsigned int available = 0 ) const = 0;
    virtual void destroy( unsigned int available = 0 ) = 0;
    virtual ~MorphInterface() {}
    virtual bool leq( const MorphInterface * ) const = 0;
};

/** @brief custom allocator for morph classes */
struct MorphAllocator {
    void *operator new( unsigned int bytes, void *where, unsigned available ) {
        if ( bytes > available || where == 0 ) {
            where = ::operator new( bytes );
            return where;
        }
        return where;
    }
    void *operator new( unsigned int bytes ) {
        return ::operator new( bytes );
    }
};

template< typename Self, typename W >
struct Morph : MorphInterface,
    mixin::Comparable< Morph< Self, W > >,
    MorphAllocator
{
    typedef W Wrapped;

    Morph( const Wrapped &w ) : m_wrapped( w ) {}

    const Self &self() const { return *static_cast< const Self * >( this ); }

    bool operator<=( const Morph &o ) const {
        return wrapped().operator<=( o.wrapped() );
    }

    virtual bool leq( const MorphInterface *_o ) const {
        const Morph *o = dynamic_cast< const Morph * >( _o );
        if ( !o ) return false; // not comparable
        return wrapped() <= o->wrapped();
    }

    virtual MorphInterface *constructCopy( void *where, unsigned int available ) const {
        return new( where, available ) Self( self() );
    }

    virtual void destroy( unsigned int available ) {
        if ( sizeof( Morph ) <= available ) {
            this->~Morph();
        } else {
            delete this;
        }
    }

    const Wrapped &wrapped() const {
        return m_wrapped;
    }

    Wrapped &wrapped() {
        return m_wrapped;
    }

    virtual ~Morph() {}
private:
    Wrapped m_wrapped;
};

/**
   @brief Amorph base class

   This class is an Amorph class. An Amorph can hold one of many
   Morhps of the corresponding kind. That means, Amorph is an envelope
   that can contain an instance of a Morph. The Amorph envelope can
   provide methods that all the Morphs that it can hold provide as
   well. These methods will then act polymoprhically in the Amorph
   instance, depending on what Morph is inside.

   You use the Amorph and Morph classes as values, that is, they have
   value semantics. You usually do not need (nor want) to use pointers
   to access them. Of course it may be useful sometimes, but is not
   the normal mode of operation.

   Amorph objects are equal if they hold same type of Morph and the
   Morphs themselves are also equal. Different types of Morphs are
   never equal.

   When implementing your own Amorph class, you will want it to
   contain the methods that are shared by all it's Morphs. These will
   usually look like

   methodFoo() { return this->impl()->methodFoo(); }

   If you need to dispatch on the type of the Morph inside the Amorph
   envelope, you can use

   if ( amorph.is< MyMorph >() ) {
       MyMorph morph = amorph;
   }

   or if you write adaptable unary function objects (see stl manual)
   handling the specific morph types, you can write:

   amorph.ifType( functor );

   This will call functor on the morph type if the functor's
   argument_type matches the type of contained Morph. The returned
   type is Maybe< functor::result_type >. If the type matches, the
   returned value is Just (returned value), otherwise Nothing.

   See wibble::Maybe documentation for details.

   This also lends itself to template specialisation approach, where
   you have a template that handles all Morphs but you need to
   specialize it for certain Morphs. Eventually, an example of this
   usage will appear in amorph.cpp over time.

   Often, using Amorph types will save you a template parameter you
   cannot afford. It also supports value-based programming, which
   means you need to worry about pointers a lot less.

   For a complex example of Amorph class set implementation, see
   range.h.

   Implementation details: the current Amorph template takes an
   integral Padding argument. The MorphImpl class contains an
   overloaded operator new, that allows it to be constructed
   off-heap. The Padding argument is used as a number of words to
   reserve inside the Amorph object itself. If the Morph that will be
   enveloped in the Amorph fits in this space, it will be allocated
   there, otherwise on heap. The Padding size defaults to 0 and
   therefore all Morphs are by default heap-allocated. Reserving a
   reasonable amount of padding should improve performance a fair bit
   in some applications (and is worthless in others).
*/
template <typename Self, typename _Interface, int Padding = 0>
struct Amorph {
    typedef _Interface Interface;

    template <typename T> struct Convert {
        typedef T type;
    };

    Amorph( const Interface &b ) {
        setInterfacePointer( &b );
    }

    Amorph( const Amorph &a ) {
        setInterfacePointer( a.implInterface() );
    }

    Amorph() : m_impl( 0 ) {}

    const Self &self() const {
        return *static_cast< const Self * >( this );
    }

    Self &self() {
        return *static_cast<Self *>( this );
    }

    bool leq( const Self &i ) const {
        if ( morphInterface() )
            if ( i.morphInterface() )
                return morphInterface()->leq( i.morphInterface() );
            else
                return false; // it's false that non-0 <= 0
        else
            return !i.morphInterface(); // 0 <= 0 holds, but 0 <=
                                        // non-0 doesn't
    }

    bool operator<=( const Self &i ) const { return leq( i ); }

    void setInterfacePointer( const Interface *i ) {
        if ( !i ) {
            m_impl = 0;
            return;
        }

        assert( dynamic_cast< const MorphInterface * >( i ) );
        assert( dynamic_cast< const Interface * >(
                    dynamic_cast< const MorphInterface * >( i ) ) );

        m_impl = dynamic_cast< const MorphInterface * >( i )->constructCopy(
            &m_padding, sizeof( m_padding ) );

        assert( dynamic_cast< const Interface * >( m_impl ) );
    }

    Amorph &operator=( const Amorph &i ) {
        setInterfacePointer( i.implInterface() );
        return *this;
    }

    ~Amorph() {
        if ( morphInterface() )
            morphInterface()->destroy( sizeof( m_padding ) );
    }

    template< typename F >
    Maybe< typename F::result_type > ifType( F func ) {
        return ifType< F::argument_type >( func );
    }

    template<typename T, typename F>
    Maybe< typename F::result_type > ifType( F func ) { // ifType?
        typedef Maybe< typename F::result_type > rt;
        T *ptr = impl<T>();
        if (ptr) {
            return rt::Just( func(*ptr) );
        }
        return rt::Nothing();
    }

    template<typename T>
    bool is() const {
        T *ptr = impl<T>();
        return ptr;
    }

    Interface *implInterface() const {
        return dynamic_cast< Interface * >( m_impl );
    }

    MorphInterface *morphInterface() const {
        return m_impl;
    }

    template <typename R> R *impl() const {
        return dynamic_cast<R *>( m_impl );
    }

private:
    unsigned int reservedSize() { return sizeof( m_padding ) + sizeof( m_impl ); }
    int m_padding[ Padding ];
    MorphInterface *m_impl;
    // Interface *m_impl;
};

template <typename T, typename X>
typename X::template Convert<T>::type &downcast( const X &a )
{
    return *a.template impl<T>();
}

}

#endif
