#ifndef DIVINE_ARRAY_OF_ABSTRACT_HH
#define DIVINE_ARRAY_OF_ABSTRACT_HH
/*!\file array.hh
 * This header file contains a complete definition of the array_t class
 * template
 *
 * This class template is commonly used (e. g. in system_t and
 * explicit_system_t). It is used as a replacement of slow containers from STL.
 *
 * \author Pavel Simecek
 */

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include "common/error.hh"
#include "common/types.hh"
#include "common/deb.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

template <class T, class ArrayIterator>
class array_of_abstract_iterator_t
{
 private:
 ArrayIterator p_item;
 public:
 array_of_abstract_iterator_t<T,ArrayIterator>() { p_item=0; }
 array_of_abstract_iterator_t<T,ArrayIterator>
                          (const array_of_abstract_iterator_t & second)
 { p_item=second.p_item; }
 array_of_abstract_iterator_t<T,ArrayIterator>(const ArrayIterator iter)
 { p_item=iter; }
 T & operator*() { return (**p_item); }
 T * operator->() { return (*p_item); }
 array_of_abstract_iterator_t<T,ArrayIterator> operator+(const int i)
 { return array_of_abstract_iterator_t(p_item+i); }
 array_of_abstract_iterator_t<T,ArrayIterator> operator-(const int i)
 { return array_of_abstract_iterator_t(p_item-i); }
 array_of_abstract_iterator_t<T,ArrayIterator> operator++()
 { p_item = p_item + 1; return (*this); }
 array_of_abstract_iterator_t<T,ArrayIterator> operator--()
 { p_item = p_item - 1; return (*this); }
 array_of_abstract_iterator_t<T,ArrayIterator> operator++(int)
 { array_of_abstract_iterator_t<T,ArrayIterator> copy = (*this);
   p_item = p_item + 1; return copy; }
 array_of_abstract_iterator_t<T,ArrayIterator> operator--(int)
 { array_of_abstract_iterator_t<T,ArrayIterator> copy = (*this);
   p_item = p_item - 1; return copy; }
 bool operator==(const array_of_abstract_iterator_t second)
 { return (this->p_item==second.p_item); }
 bool operator!=(const array_of_abstract_iterator_t second)
 { return (this->p_item!=second.p_item); }
};


//!Simple resizable container representing 1-dimensional array
/*!This container reimplements array_t template for cases of abstract
 * classes. They cannot be normally instantiated, therefore this
 * container stores only pointers to them. In fact it stores pointers
 * to the childs of the abstract class.
 *
 * This container presumes, that all items have the same (but unknown)
 * type derived from T (where T is a parameter of template).
 * The unknown type is given by the function SingleAlloc given as
 * a second parameter of a template. SingleAlloc creates the instance
 * of the unknown type and returns it in the form of pointer to the
 * abstract class.
 *
 * This has been espesially useful in an implementation of
 * enabled_trans_container_t,
 * derived from array_of_abstract_t. It enables enabled_trans_container_t
 * to be relatively fast (no redundant allocation and deallocation)
 * and universal for all possible systems.
 */

template <class T, T* (*SingleAlloc)(const void * params) >
class array_of_abstract_t
{
 private:
  //!Parameters of allocation
  const void * parameters;
  array_t<T*> array;
 protected:
  //!The entire field of objects stored in array_t
  T * field;
 public:
  //!Iterator
  /*!Iterator. Dereferencing, increasing and descresing takes time <i>O(1)</i>
   * and it is really fast. */
  typedef array_of_abstract_iterator_t<T, T**> iterator;
  //!Constant iterator
  /*!Constant iterator - you cannot change the value to which the iterator
   * points. Dereferencing, increasing and descresing takes time <i>O(1)</i> and
   * it is really fast. */
  typedef array_of_abstract_iterator_t<T, const T**> const_iterator;
  //!Returns an iterator pointing to the first item of the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  iterator begin() { return iterator(array.begin()); }
  //!Returns an iterator pointing immediatelly behind the last item of
  //! the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  iterator end() { return iterator(array.end()); }
  //!Returns an iterator pointing to the last item of the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  iterator last() { return iterator(array.last()); }
  //!Returns a constant iterator pointing to the first item of the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  const_iterator begin() const { return const_iterator(array.begin()); }
  //!Returns a constant iterator pointing immediatelly behind the last item of
  //! the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  const_iterator end() const { return const_iterator(array.end()); }
  //!Returns a constant iterator pointing to the last item of the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  const_iterator last() const { return const_iterator(array.last()); }
  //!Copies `to_copy' to this container
  /*!Copies the entrire contents of one instance of container to another one.
   *
   * This operation takes both memory and time <i>O(n)</i>, where <i>n</i> is
   * a number of items in \a to_copy instance of the container.*/
  void assign_from(const array_of_abstract_t<T, SingleAlloc> & to_copy)
   {
    resize(to_copy.array.size());
    for (size_int_t i=0; i!=array.size(); ++i)
      (*array[i]) = (*to_copy.array[i]);
   }
  //!A constructor
  //!\param params = parameters of allocation
  /* \param allocate = the number of items to pre-alllocate
   * \param step = the step of allocation in case of extending the container
   */
  array_of_abstract_t(const void * params,
                               size_int_t allocate = 2, size_int_t step = 2):
     parameters(params), array(allocate, step)
   { for (size_int_t i=0; i!=array.get_allocated(); ++i)
       array[i] = SingleAlloc(parameters); }
  //!A copy constructor
  /* It is implemented using assign_from() method, therefor it takes
   * both time and memory <i>O(n)</i>, where <i>n</i> is
   * a number of items in \a to_copy instance of the container.*/
  array_of_abstract_t
       (const array_of_abstract_t<T, SingleAlloc> & to_copy)
  { assign_from(to_copy); }
  //!A destructor
  ~array_of_abstract_t()
  { for (size_int_t i=0; i!=array.get_allocated(); ++i) delete array[i]; }
  //!Appends `what' to the end of the container
  /*!Appends \a what to the end of the container. If neccessary it
   * extends the allocated memory. Therefore in that case it runs
   * in a time<i>O(n)</i>, where <i>n</i> is a number of items stored
   * in the container.*/
  void push_back(T what)
   {
    extend(1);
    (*array.back())=what;
   }
  //!Removes the last item from the container
  /*!Removes the last item from the container and returns its value. It
   * doesn't relesase the memory allocated for the last item (this memory
   * will be reused in the next push_back()) - therefore it runs in a
   * time <i>O(1)</i> and it it really fast operation. */
  T pop_back()
   { T result = (*array[array.size()]);
     shrink_to(array.size()-1);
     return result; }
  //!Resizes the container to the size of `count' elements.
  /*!It is implemented using shrink_to() and extend_to() methods.
   * Therefore if \a count <= size() it runs in a time <i>O(1)</i> (it uses
   * shrink_to() method), otherwise it runs in a time <i>O(n)</i> (it uses
   * extend_to() method), where <i>n</i> is a number of items stored in
   * the container.*/
  void resize(const size_int_t count)
   {
    const size_int_t initial_allocated = array.get_allocated();
    array.resize(count);
    const size_int_t currently_allocated = array.get_allocated();
    for (size_int_t i=initial_allocated; i<currently_allocated; ++i)
      array[i]=SingleAlloc(parameters);
   }
  //!Shrinks the container to the size of `count' elements.
  /*!\warning Important:
   * This method presumes, that \a count <= size().
   *
   * Its running time is <i>O(1)</i> and it is really fast operation*/
  void shrink_to(const size_int_t count)
  { array.shrink_to(count); }
  //!Extends the container to the size of `count' elements.
  /*!\warning Important:
   * This method presumes, that \a count >= size().
   *
   * Its running time is <i>O(n)</i>, where <i>n</i> is a number of items
   * stored in the container. */
  void extend_to(const size_int_t count)
  { extend(count-array.size()); }
  //!Extends the container <b>by</b> count members.
  /*!\param count = the count of items we want to add to the container
   *
   * Its running time is <i>O(n)</i>, where <i>n</i> is a number of items
   * stored in the container. */
  void extend(const size_int_t count)
   {
    const size_int_t initial_allocated = array.get_allocated();
    array.extend(count);
    const size_int_t currently_allocated = array.get_allocated();
    for (size_int_t i=initial_allocated; i<currently_allocated; ++i)
      array[i]=SingleAlloc(parameters);
   }
  //!Returns a count of items stored in a container
  size_int_t size() const { return array.size(); }
  //!Returns a count of items allocated in a memory of this container.
  /*!Returns a count of items allocated in a memory of this container.
   * It's return value is always more or equal to return value of size() */
  size_int_t get_allocated() const { return array.get_allocated(); }
  //!Returns an allocation step
  /*!Returns an allocation step. Allocation step is the least step of allocation
   * of new items (we always allocate the number of items, which is divisible
   * by get_alloc_step()) */
  size_int_t get_alloc_step() const { return array.get_alloc_step(); }
  //!Sets an alloc_step step.
  /*!Sets an allocation step. Allocation step is the least step of allocation
   * of new items (we always allocate the number of items, which is divisible
   * by get_alloc_step()) */
  void set_alloc_step(const size_int_t new_alloc_step)
    { array.set_alloc_step(new_alloc_step); }
  //!Returns a reference to the first item in the container.
  T & front() { return (*array.front()); }
  //!Returns a constant reference to the first item in the container.
  const T & front() const { return (*array.front()); }
  //!Returns a reference to the last item in the container.
  T & back() { return (*array.back()); }
  //!Returns a constant reference to the last item in the container.
  const T & back() const { return (*array.back()); }
  //!Returns a reference to `i'-th item in the container.
  T & operator[](const size_int_t i)
  { return (*array[i]); }
  //!Returns a constant reference to `i'-th item in the container.
  const T & operator[](const size_int_t i) const
  { return (*array[i]); }
  //!Swaps the containment of instance `second' and this.
  /*!Swaps the containment of instance `second' and this. On one hand
   * (unlike assign_from()) it changes its parameter, but on the other
   * hand it runs only in <i>O(1)</i> time, what is much faster than
   * the running time of assign_from() */
  void swap(array_of_abstract_t<T, SingleAlloc> & second)
   { array.swap(second.array); }
  //!Lowers the size of array to zero, but does not release allocated memory.
  /*!Lowers the size of array to zero, but does not release allocated memory.
   * It is the same as <code>shrink_to(0)</code>.*/
  void clear() { array.clear(); }
};


#ifndef DOXYGEN_PROCESSING
} //end of namespace
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif

