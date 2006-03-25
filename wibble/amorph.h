/** -*- C++ -*-
    @file wibble/amorph.h
    @author Peter Rockai <me@mornfall.net>
*/

#include <wibble/cast.h>
#include <wibble/maybe.h>
// #include <iostream> // for debug noise

#ifndef WIBBLE_AMORPH_H
#define WIBBLE_AMORPH_H

namespace wibble {

/**
   @brief A base class for morphs

   You usually have a base class for all the Morphs. It would probably
   inherit this class. See MorphImpl and Amorph class documentation for details.
 */
template <typename T>
struct MorphBase {
    virtual T *constructCopy( void *where = 0, unsigned int available = 0 ) const = 0;
    virtual void destroy( unsigned int available = 0 ) = 0;
    virtual bool equals( T *i ) const = 0;
    virtual bool less( T *i ) const = 0;
    virtual ~MorphBase() {}
};

const int notComparable = 0;
const int equalityComparable = 1;
const int comparable = 2;

struct MorphImplBase {
    void *operator new( unsigned int bytes, void *where, unsigned available ) {
        if ( bytes > available ) {
            where = ::operator new( bytes );
            std::cerr << "heap constrtuct on: " << where << std::endl;
            return where;
        }
        std::cerr << "in-place constrtuct on: " << where << std::endl;
        return where;
    }
};

/**
   @brief (Default) Base class for morph classes

   You will usually want to have a FooImpl class for your Foo amorph
   set. The Morphs of this kind will be all derived from FooImpl
   (maybe apart from pathological cases).

   The FooBase class would probably look like:

   struct FooBase: public MorphBase< FooBase > {
       virtual someSharedMethod() = 0;
       virtual anotherSharedMethod() = 0;
   };

   Then, FooImpl would look like this:

   template< typename Self, typename Base = FooBase >
   struct FooImpl : MorphImpl< Safe, Base, equalityComparable >
   {
       // stuff
   };

   Then, you derive your Morph classe from your FooImpl. It will
   provide default implementations of the important (but repetitive)
   members like constructCopy, equals and such. It also provides
   initFromBase which will use your assignment operator and casting to
   make it easy for you to implement a casting from an Amorph in a
   safe manner.

   This variant provides empty equals() and less(), so use it if your
   class does not implement operator == and <, respectively. See the
   equalityComparable and comparable variants if you do provide (some
   of) those.

   See Amorph class for details.
*/
template <typename Self, typename Base, int comparable = notComparable> 
struct MorphImpl : public Base, MorphImplBase {

    void initFromBase (const Base *i) {
        const Self *p = dynamic_cast <const Self *> (i);
        if (p)
            self() = *p;
        else
            throw exception::BadCastExt< Base, Self >( "dynamic cast failed" );
    }

   virtual Base *constructCopy( void *where, unsigned int available ) const {
        if ( !where || !available )
            return ::new Self( self() );
        return new( where, available ) Self( self() );
    }

    virtual void destroy( unsigned int available ) {
        if ( sizeof( Self ) <= available ) {
            std::cerr << "in-place destroy on: " << this << std::endl;
            (&self())->~Self();
        } else {
            std::cerr << "heap destroy on: " << this << std::endl;
            delete this;
        }
    }

    const Self &self() const {
        return *static_cast<const Self *>( this );
    }

    Self &self() {
        return *static_cast<Self *>( this );
    }

    virtual bool equals( Base * ) const {
        return false; // THROW!
    }

    virtual bool less( Base * ) const {
        return false; // THROW!
    }

};

/**
   @brief Base class for morph class implementations

   See MoprhImpl notComparable. This class provides equals() in
   addition, use this if you implement operator==, but not operator<.
*/
template <typename Self, typename Base>
struct MorphImpl<Self, Base, equalityComparable>
    : MorphImpl<Self, Base, notComparable>
{
    virtual bool equals( Base *i ) const {
        const Self *p = dynamic_cast<const Self *>( i );
        /* if (! p)
           throw std::bad_cast(); */
        if (!p)
            return false; // distinct types aren't really equal, are
                          // they? :)
        return (this->self()) == p->MorphImpl::self(); //Self( *p );
    }

    bool operator!=( const Self &a ) const {
        return ! ( (this->self()) == a );
    }

};

/**
   @brief Base class for morph class implementations

   See MorphImpl equalityComparable. This one provides less() in
   addition to the above, use it if you provide both operator== and
   operator<
*/
template <typename Self, typename Base>
struct MorphImpl<Self, Base, comparable>
    : MorphImpl<Self, Base, equalityComparable>
{
    virtual bool less( Base *i ) const {
        const Self *p = dynamic_cast<const Self *>( i );
        if (!p)
            throw exception::BadCastExt< Base, Self >( "dynamic cast failed" );
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
template <typename Self, typename Base, int Padding = 0>
struct Amorph {
    template <typename T> struct Convert {
        typedef T type;
    };

    Amorph( const Base *b ) {
        m_impl = b->constructCopy( &m_padding, sizeof( m_padding ) );
    }

    Amorph( const Amorph &t ) {
        self() = t.self();
    }

    Amorph() {
        m_impl = 0;
    }

    const Self &self() const {
        return *static_cast<const Self *>( this );
    }

    Self &self() {
        return *static_cast<Self *>( this );
    }

    bool operator== (const Amorph &i) const {
        if (m_impl)
            if (i.m_impl)
                return m_impl->equals( i.m_impl );
            else
                return false;
        else
            return !i.m_impl;
    }

    bool operator< (const Amorph &i) const {
        if (m_impl)
            if (i.m_impl)
                return m_impl->less( i.m_impl );
            else
                return false;
        else
            return !i.m_impl;
    }
    
    bool operator!=(const Amorph &i) const {
        return !self().operator==(i);
    }

    bool operator> (const Amorph &i) const {
        return !( *this < i || *this == i);
    }

    Amorph &operator=( const Amorph &i ) {
        if (i.m_impl)
            m_impl = i.m_impl->constructCopy( &m_padding, sizeof( m_padding ) );
        else
            m_impl = 0;
        return *this;
    }

    ~Amorph() {
        impl()->destroy( sizeof( m_padding ) );
        // delete m_impl;
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

    Base *impl() const {
        return m_impl;
    }

    template <typename R> R *impl() const {
        return dynamic_cast<R *>( impl() );
    }

protected:
    unsigned int reservedSize() { return sizeof( m_padding ) + sizeof( m_impl ); }
    int m_padding[ Padding ];
    Base *m_impl;
};

template <typename T, typename X>
typename X::template Convert<T>::type &downcast( X &a )
{
    return *a.template impl<T>();
}

}

#endif
