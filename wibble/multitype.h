/** -*- C++ -*-
    @file utils/multitype.h
    @author Peter Rockai <me@mornfall.net>
*/

#include <apt-front/utils/cast.h>
#include <typeinfo> // for bad_cast
#ifndef APTFRONT_UTILS_MULTITYPE_H
#define APTFRONT_UTILS_MULTITYPE_H

namespace aptFront {
namespace utils {

template <typename T>
struct Maybe {
    bool nothing() { return m_nothing; }
    T &value() { return m_value; }
    Maybe( bool n, const T &v ) : m_nothing( n ), m_value( v ) {}
    Maybe( const T &df = T() )
       : m_nothing( true ), m_value( df ) {}
    static Maybe Just( const T &t ) { return Maybe( false, t ); }
    static Maybe Nothing( const T &df = T() ) {
        return Maybe( true, df ); }
    operator T() { return value(); }
protected:
    bool m_nothing:1;
    T m_value;
};

/* template <typename T>
Maybe<T> Just( const T &t ) { return Maybe( false, t ); }
template <typename T>
Maybe<T> Nothing( const T &df = T() ) {
return Maybe( true, df ); } */

/**
   @brief A base class for multitype-participating classes

   Inherit your multitype base class from this template to enable use
   of its derived classes inside a multitype set. See MultiType
   template for details on multitype sets.
 */
template <typename T>
struct MultiTypeBase {
    virtual T *duplicate () const = 0;
    virtual bool equals( T *i ) const = 0;
    virtual bool less( T *i ) const = 0;
    virtual ~MultiTypeBase() {}
};

const int notComparable = 0;
const int equalityComparable = 1;
const int comparable = 2;

/**
   @brief Base class for multitype-participating class implementations

   Derive your multitype-participating classes from this one. It will
   provide default implementations of the important (but repetitive)
   members like dubplicate, equals and such. It also provides initFromBase
   which will use your assignment operator and casting to make it easy
   for you to implement a copy-from-multitype in a safe manner.

   This variant provides empty equals() and less(), so use it if your
   class does not implement operator == and <, respectively. See the
   equalityComparable and comparable variants if you do provide (some
   of) those.

   See MultiType class for details.
*/
template <typename Self, typename Base, int comparable = notComparable> 
struct MultiTypeImpl : public Base {

    void initFromBase (const Base *i) {
        const Self *p = dynamic_cast <const Self *> (i);
        if (p)
            self() = *p;
        else
            throw std::bad_cast ();
    }

    virtual Base *duplicate () const {
        return new Self( self() );
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
   @brief Base class for multitype-participating class implementations

   See MultiTypeImpl notComparable. This class provides equals()
   in addition, use this if you implement operator==, but not operator<.
*/
template <typename Self, typename Base>
struct MultiTypeImpl<Self, Base, equalityComparable>
    : MultiTypeImpl<Self, Base, notComparable>
{
    virtual bool equals( Base *i ) const {
        const Self *p = dynamic_cast<const Self *>( i );
        /* if (! p)
           throw std::bad_cast(); */
        if (!p)
            return false; // distinct types aren't really equal, are
                          // they? :)
        return (this->self()) == p->MultiTypeImpl::self(); //Self( *p );
    }

    bool operator!=( const Self &a ) const {
        return ! ( (this->self()) == a );
    }

};

/**
   @brief Base class for multitype-participating class implementations

   See MultiTypeImpl equalityComparable. This one provides less()
   in addition to the above, use it if you provide both operator==
   and operator<
*/
template <typename Self, typename Base>
struct MultiTypeImpl<Self, Base, comparable>
    : MultiTypeImpl<Self, Base, equalityComparable>
{
    virtual bool less( Base *i ) const {
        const Self *p = dynamic_cast<const Self *>( i );
        if (!p)
           throw std::bad_cast();
        return (this->self()) < Self( *p );
    }
};

/**
   @brief a multitype base class

   This class is a multitype wrapper. Inherit your own multitype
   wrappers from this class.

   Multitype sets are sets of classes with similar interface. The
   interface itself is defined in the base class of the set, which
   is derived from MultiTypeBase. This wrapper enables interchangable
   use of the implementations of the interface (usually derived from
   one of the MultiTypeImpl variants), as values. You can pass and
   use multitypes as values. The wrapper will act as if it was an instance
   of one of those implementations.

   It is implemented as a kind of smart pointer. It holds a pointer
   to a specific implementation instance and it manages its allocation
   and deallocation on heap. You need not to worry about the pointer,
   the class completely hides it in normal operation. This allows
   the methods provided in multitype itself to use dynamic (virtual)
   dispatch of the methods on its contained type. This means,
   if you have MyMultiTYpe x; you can use x.methodFromInterface() and
   the implementation from the currently held instance will be
   called. Also, operators ==, <, = and copy constructors are provided,
   so you can safely use instances of this class in std containers.

   The pointer is owned exclusively by the multitype instance,
   and if the instance is copied, the underlying implementation
   will be duplicate()ed.

   Multitype instances are equal, if they hold same type and the
   held implementations are equal. Same goes for <, != and >.

   You will want to implement all the methods defined in interface,
   and simply make them call this->m_impl->methodInQuestion(
   parameters ). Then your functions and methods can take parameters
   of the multitype type, by value and operate on them in a generic
   fashion.

   Use of multitypes makes it convenient to write generic algorithms
   even when templates are not applicable in given situation. It
   makes it possible to safely dispatch on the contained type as well,
   avoiding the dynamic_cast mess.
*/
template <typename Self, typename Base>
struct MultiType {
    template <typename T> struct Convert {
        typedef T type;
    };

    MultiType( const Base *b ) {
        m_impl = b->duplicate();
    }

    MultiType( const MultiType &t ) {
        self() = t.self();
    }

    MultiType() {
        m_impl = 0;
    }

    const Self &self() const {
        return *static_cast<const Self *>( this );
    }

    Self &self() {
        return *static_cast<Self *>( this );
    }

    bool operator== (const MultiType &i) const {
        if (m_impl)
            if (i.m_impl)
                return m_impl->equals( i.m_impl );
            else
                return false;
        else
            return !i.m_impl;
    }

    bool operator< (const MultiType &i) const {
        if (m_impl)
            if (i.m_impl)
                return m_impl->less( i.m_impl );
            else
                return false;
        else
            return !i.m_impl;
    }
    
    bool operator!=(const MultiType &i) const {
        return !self().operator==(i);
    }

    bool operator> (const MultiType &i) const {
        return !( *this < i || *this == i);
    }

    MultiType &operator=( const MultiType &i ) {
        if (i.m_impl)
            m_impl = i.m_impl->duplicate();
        else
            m_impl = 0;
        return *this;
    }

    ~MultiType() {
        delete m_impl;
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
    Base *m_impl;
};

}

template <typename T, typename X>
typename X::template Convert<T>::type &downcast( X &a )
{
    return *a.template impl<T>();
}

}

#endif
