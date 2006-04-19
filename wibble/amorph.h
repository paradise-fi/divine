/** -*- C++ -*-
    @file wibble/amorph.h
    @author Peter Rockai <me@mornfall.net>
*/

#include <iostream> // for noise

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
    // virtual bool equals( const MorphInterface *i ) const = 0;
    // virtual bool less( const MorphInterface *i ) const = 0;
    virtual ~MorphInterface() {}
    virtual bool equals( const MorphInterface * ) const {
        std::cout << "MorphInterface::equals()" << std::endl;
        return false; // THROW!
    }

    virtual bool less( const MorphInterface * ) const {
        std::cout << "MorphInterface::less()" << std::endl;
        return false; // THROW!
    }
};

/** @brief custom allocator for morph classes */
struct MorphAllocator {
    void *operator new( unsigned int bytes, void *where, unsigned available ) {
        if ( bytes > available ) {
            where = ::operator new( bytes );
            return where;
        }
        return where;
    }
};

/**
   @brief (Default) Base class for morph classes

   You will usually want to have a FooImpl class for your Foo amorph
   set. The Morphs of this kind will be all derived from FooImpl
   (maybe apart from pathological cases).

   The FooInterface class would probably look like:

   struct FooInterface {
       virtual someSharedMethod() = 0;
       virtual anotherSharedMethod() = 0;
   };

   Then, FooImpl would look like this:

   template< typename Self, typename Interface = FooInterface >
   struct FooImpl : MorphImpl< Self, Interface, equalityComparable >
   {
       // stuff
   };

   Then, you derive your Morph classe from your FooImpl. It will
   provide default implementations of the important (but repetitive)
   members like constructCopy, equals and such. It also provides
   initFromBase which will use your assignment operator and casting to
   make it easy for you to implement a casting from an Amorph in a
   safe manner.

   Provided equals() and less() methods are empty, so your class does
   not need to implement operators == and <. See also
   MorphEqualityComparable and MorphComparable if you provide these
   operators.

   See Amorph class for details.
*/
template< typename Self, typename Interface >
struct MorphImpl : virtual Interface, virtual MorphInterface, virtual MorphAllocator {
    // typedef typename Base::Interface Interface;

    void initFromBase( const Interface *i ) {
        const Self *p = dynamic_cast <const Self *> (i);
        if ( p )
            self() = *p;
        else
            throw exception::BadCastExt< Interface, Self >( "dynamic cast failed" );
    }

   virtual MorphInterface *constructCopy( void *where, unsigned int available ) const {
        if ( !where || !available )
            return ::new Self( self() );
        return new( where, available ) Self( self() );
    }

    virtual void destroy( unsigned int available ) {
        if ( sizeof( Self ) <= available ) {
            (&self())->~Self();
        } else {
            delete this;
        }
    }

    const Self &self() const {
        return *static_cast<const Self *>( this );
    }

    Self &self() {
        return *static_cast<Self *>( this );
    }

    virtual ~MorphImpl() {}
};

/**
   @brief Base class for morph class implementations

   See MoprhImpl notComparable. This class provides equals() in
   addition, use this if you implement operator==, but not operator<.
*/
template< typename Self >
struct MorphEqualityComparable : virtual MorphInterface
{
    virtual bool equals( const MorphInterface *i ) const {
        const Self *p = dynamic_cast< const Self * >( i ),
                   &self = *dynamic_cast< const Self * >( this );
        if (!p)
            return false; // distinct types aren't really equal, are they? :)
        return self == *p;
    }

    bool operator!=( const Self &a ) const {
        const Self &self = *dynamic_cast< const Self * >( this );
        return !( self == a );
    }

};

/**
   @brief Base class for morph class implementations

   This one provides less() in addition to equals() in the above, use
   it if you provide both operator== and operator<
*/
template< typename Self >
struct MorphComparable : virtual MorphEqualityComparable< Self >
{
    virtual bool less( const MorphInterface *i ) const {
        const Self *p = dynamic_cast<const Self *>( i );
        if (!p)
            throw exception::BadCastExt< MorphInterface, Self >( "dynamic cast failed" );
        return (this->self()) < Self( *p );
    }
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

    Amorph() : m_impl( 0 ) {}
    Amorph( const Interface &b ) {
        setInterfacePointer( &b );
    }
    Amorph( const Amorph &a ) {
        setInterfacePointer( a.implInterface() );
    }

    const Self &self() const {
        return *static_cast< const Self * >( this );
    }

    Self &self() {
        return *static_cast<Self *>( this );
    }

    bool operator== (const Amorph &i) const {
        // std::cout << "Amorph::operator==()" << std::endl;
        if ( morphInterface() )
            if ( i.morphInterface() )
                return morphInterface()->equals( i.morphInterface() );
            else
                return false;
        else
            return !i.morphInterface();
    }

    bool operator< (const Amorph &i) const {
        if ( morphInterface() )
            if ( i.morphInterface() )
                return morphInterface()->less( i.morphInterface() );
            else
                return false;
        else
            return !i.morphInterface();
    }

    /* bool operator!=(const Amorph &i) const {
        return !self().operator==(i);
        } */

    bool operator> (const Amorph &i) const {
        return !( *this < i || *this == i);
    }

    void setInterfacePointer( const Interface *i ) {
        if ( dynamic_cast< const MorphInterface * >( i ) ) {
            m_impl = dynamic_cast< const MorphInterface * >( i )->constructCopy(
                &m_padding, sizeof( m_padding ) );
        } else
            m_impl = 0;
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

protected:
    unsigned int reservedSize() { return sizeof( m_padding ) + sizeof( m_impl ); }
    int m_padding[ Padding ];
    MorphInterface *m_impl;
};

template <typename T, typename X>
typename X::template Convert<T>::type &downcast( X &a )
{
    return *a.template impl<T>();
}

}

#endif
