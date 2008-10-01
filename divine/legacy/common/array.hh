#ifndef DIVINE_ARRAY_HH
#define DIVINE_ARRAY_HH
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
#include <cstring>
#include "common/error.hh"
#include "common/types.hh"
#include "common/deb.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

template <class T>
T * default_new_field_of_objects(const size_int_t count)
{ 
  return (new T[count]); 
}


//!Simple resizable container representing 1-dimensional array
/*!This container implement a resizable 1-dimensional array of a choosen type.
 * The access to the single item of an array is implemented simply using
 * operator [].
 *
 * Constraints imposed on type that can be paramater of this template:
 * Type must not have contructor or destructor. It should be a scalar
 * type like integer, pointer, etc.
 * 
 * You can do reallocation using methods resize(), shrink_to(), extend_to(),
 * extend() or push_back().
 * 
 * The purpose of this container is to implement container with really fast
 * random access times to it. The penalty for the really fast read/write
 * operations is a possibly slow realocation. Reallocation is implemented
 * such way that if the container has not allocated sufficiently large memory,
 * reallocation methods resize(), extend(), extend_to() or push_back()
 * allocate a larger piece of memory. It means that resize(), extend(),
 * extend_to() and push_back() may have time complexity <i>O(n)</i>, where
 * <i>n</i> is a number items in an array. You can influence how often the
 * array will be reallocated using set_alloc_step() method.
 *
 * There are defined functions swap() and assign_from() to copy the contents
 * of one instace of the container to another instance.
 */

template <class T, T* (*Alloc)(const size_int_t count) = default_new_field_of_objects<T> >
class array_t
{
 private:
  size_int_t allocated;
  size_int_t alloc_step;
  size_int_t my_size;
 protected:
  //!The entire field of objects stored in array_t
  T * field;
 public:
  //!Iterator
  /*!Iterator. Dereferencing, increasing and descresing takes time <i>O(1)</i>
   * and it is really fast. */
  typedef T * iterator;
  //!Constant iterator
  /*!Constant iterator - you cannot change the value to which the iterator
   * points. Dereferencing, increasing and descresing takes time <i>O(1)</i> and
   * it is really fast. */
  typedef const T * const_iterator;
  //!Returns an iterator pointing to the first item of the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  iterator begin() { return &field[0]; }
  //!Returns an iterator pointing immediatelly behind the last item of
  //! the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  iterator end() { return &field[my_size]; }
  //!Returns an iterator pointing to the last item of the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  iterator last() { return &field[my_size-1]; }
  //!Returns a constant iterator pointing to the first item of the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  const_iterator begin() const { return &field[0]; }
  //!Returns a constant iterator pointing immediatelly behind the last item of
  //! the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  const_iterator end() const { return &field[my_size]; }
  //!Returns a constant iterator pointing to the last item of the container
  /*!It is really fast operation running in time <i>O(1)</i>*/
  const_iterator last() const { return &field[my_size-1]; }
  //!Copies `to_copy' to this container
  /*!Copies the entrire contents of one instance of container to another one.
   *
   * This operation takes both memory and time <i>O(n)</i>, where <i>n</i> is
   * a number of items in \a to_copy instance of the container.*/
  void assign_from(const array_t<T, Alloc> & to_copy)
   {
    DEBFUNC(cerr << "BEGIN of array_t<T>::assign_from" << endl;)
    my_size = to_copy.size();
    alloc_step = to_copy.get_alloc_step();
    allocated = to_copy.get_allocated();
    if (field) delete [] field;
    field = allocated ? Alloc(allocated) : 0;
    for (size_int_t i = 0; i!=my_size; ++i)
      field[i] = to_copy[i];
    DEBFUNC(cerr << "END of array_t<T>::assign_from" << endl;)
   }
  //!A constructor
  /*!\param allocate = the number of items to pre-alllocate
   * \param step = the step of allocation in case of extending the container
   */
  array_t(size_int_t allocate = 2, size_int_t step = 2)
   {
    allocated = allocate; alloc_step = step; my_size = 0;
    field = allocated ? Alloc(allocated) : 0;
   }
  //!A copy constructor
  /* It is implemented using assign_from() method, therefor it takes
   * both time and memory <i>O(n)</i>, where <i>n</i> is
   * a number of items in \a to_copy instance of the container.*/
  array_t(const array_t<T, Alloc> & to_copy)
  { field = 0; assign_from(to_copy); }
  //!A destructor
  ~array_t() { if (field) delete [] field; }
  //!Appends `what' to the end of the container
  /*!Appends \a what to the end of the container. If neccessary it
   * extends the allocated memory. Therefore in that case it runs
   * in a time<i>O(n)</i>, where <i>n</i> is a number of items stored
   * in the container.*/
  void push_back(T what)
   {
    if (my_size<allocated) { field[my_size] = what; ++my_size; }
    else { extend(1); field[my_size-1] = what; }
   }
  //!Removes the last item from the container
  /*!Removes the last item from the container and returns its value. It
   * doesn't relesase the memory allocated for the last item (this memory
   * will be reused in the next push_back()) - therefore it runs in a
   * time <i>O(1)</i> and it it really fast operation. */
  T pop_back()
   { my_size--; return field[my_size]; }
  //!Resizes the container to the size of `count' elements.
  /*!It is implemented using shrink_to() and extend_to() methods.
   * Therefore if \a count <= size() it runs in a time <i>O(1)</i> (it uses
   * shrink_to() method), otherwise it runs in a time <i>O(n)</i> (it uses
   * extend_to() method), where <i>n</i> is a number of items stored in
   * the container.*/
  void resize(const size_int_t count)
  { if (count<=my_size) shrink_to(count); else extend_to(count); }
  //!Shrinks the container to the size of `count' elements.
  /*!\warning Important:
   * This method presumes, that \a count <= size().
   *
   * Its running time is <i>O(1)</i> and it is really fast operation*/
  void shrink_to(const size_int_t count)
  { my_size = count; }
  //!Extends the container to the size of `count' elements.
  /*!\warning Important:
   * This method presumes, that \a count >= size().
   *
   * Its running time is <i>O(n)</i>, where <i>n</i> is a number of items
   * stored in the container. */
  void extend_to(const size_int_t count)
  { extend(count-my_size); }
  //!Extends the container <b>by</b> count members.
  /*!\param count = the count of items we want to add to the container
   *
   * Its running time is <i>O(n)</i>, where <i>n</i> is a number of items
   * stored in the container. */
  void extend(const size_int_t count)
   {
    if ((my_size + count) > allocated)
     {
      allocated = (((my_size+count) / alloc_step) + 1)*alloc_step;
      DEB(cerr << "Newly allocated " << allocated << " " << sizeof(T))
      DEB(     << "-bytes" << endl;)
      T * aux_pointer = Alloc(allocated);
      if (field)
       {
        memcpy(aux_pointer,field,my_size*sizeof(T));
        delete [] field;
       }
      field = aux_pointer;
     }
    my_size += count;
   }
  //!Returns a count of items stored in a container
  size_int_t size() const { return my_size; }
  //!Returns a count of items allocated in a memory of this container.
  /*!Returns a count of items allocated in a memory of this container.
   * It's return value is always more or equal to return value of size() */
  size_int_t get_allocated() const { return allocated; }
  //!Returns an allocation step
  /*!Returns an allocation step. Allocation step is the least step of allocation
   * of new items (we always allocate the number of items, which is divisible
   * by get_alloc_step()) */
  size_int_t get_alloc_step() const { return alloc_step; }
  //!Sets an alloc_step step.
  /*!Sets an allocation step. Allocation step is the least step of allocation
   * of new items (we always allocate the number of items, which is divisible
   * by get_alloc_step()) */
  void set_alloc_step(const size_int_t new_alloc_step)
    { alloc_step = new_alloc_step; }
  //!Returns a reference to the first item in the container.
  T & front() { return field[0]; }
  //!Returns a constant reference to the first item in the container.
  const T & front() const { return field[0]; }
  //!Returns a reference to the last item in the container.
  T & back() { return field[my_size-1]; }
  //!Returns a constant reference to the last item in the container.
  const T & back() const { return field[my_size-1]; }
  //!Returns a reference to `i'-th item in the container.
  T & operator[](const size_int_t i)
    { DEBCHECK(if (i>=my_size) gerr << "Indexing error" << thr();)
      return field[i]; }
  //!Returns a constant reference to `i'-th item in the container.
  const T & operator[](const size_int_t i) const
    { DEBCHECK(if (i>=my_size) gerr << "Indexing error" << thr();)
      return field[i]; }
  //!Swaps the containment of instance `second' and this.
  /*!Swaps the containment of instance `second' and this. On one hand
   * (unlike assign_from()) it changes its parameter, but on the other
   * hand it runs only in <i>O(1)</i> time, what is much faster than
   * the running time of assign_from() */
  void swap(array_t<T, Alloc> & second)
   {
    DEBFUNC(cerr << "array_t<T>::swap called" << endl;)
    
    T * aux_pointer = field;
    field = second.field;
    second.field = aux_pointer;

    size_int_t aux;
    aux = allocated; allocated = second.allocated; second.allocated = aux;
    aux = alloc_step; alloc_step = second.alloc_step; second.alloc_step = aux;
    aux = my_size; my_size = second.my_size; second.my_size = aux;
   }
  //!Lowers the size of array to zero, but does not release allocated memory.
  /*!Lowers the size of array to zero, but does not release allocated memory.
   * It is the same as <code>shrink_to(0)</code>.*/
  void clear() { my_size = 0; }

///  //!Sets an allocation function
///  void set_allocation_function(const new_field_of_objects_t function)
///  { new_field_of_objects = function; }
///  
///  //!Returns an allocation function
///  new_field_of_objects_t get_allocation_function()
///  { return new_field_of_objects; }
};


#ifndef DOXYGEN_PROCESSING
} //end of namespace
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif

