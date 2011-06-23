#ifndef COIN_SEQ_BIT_SET_H
#define COIN_SEQ_BIT_SET_H

#include <iostream>

#define WORD_SIZE (sizeof(unsigned int) * 8)

class bit_set_t {
private:
  unsigned int * data;
  unsigned int size;

public:
  explicit // so that e.g. `bit_set_t x = 3;' is disallowed
  bit_set_t (unsigned int length) {
    	size = (length + WORD_SIZE - 1) / WORD_SIZE;
	data = new unsigned int [size];
	clear();
  }
  ~bit_set_t () { 
  	delete [] data; 
  }

  void clear () {
    	for (unsigned int i = 0; i < size; i++) data[i] = 0;
  }
    
#define word_pos	(elem / WORD_SIZE)
#define inword_pos	(elem % WORD_SIZE)

  void insert (unsigned int elem) {
  	data[word_pos] |= (1 << inword_pos);
  }

  void remove (unsigned int elem) {
	data[word_pos] &= ((unsigned int)(-1) - (1 << inword_pos));
  }

  bool contains (unsigned int elem) const {
    	return data[word_pos] & (1 << inword_pos);
  }

  // assume other.size == size
  void union_with (const bit_set_t * other) {
    	for (unsigned int i = 0; i < size; i++) {
	  	data[i] |= other->data[i];
	}
  }

  bool common_element (const bit_set_t * other) const {
    	for (unsigned int i = 0; i < size; i++) {
	  	if (data[i] & other->data[i]) return true;
	}
	return false;
  }

  bool operator== (const bit_set_t & other) const {
    	for (unsigned int i = 0; i < size; i++) {
	  	if (data[i] != other.data[i]) return false;
	}
	return true;
  }

  bool operator!= (const bit_set_t & other) const {
    	return !(*this == other);
  }

#undef inword_pos
#undef word_pos
#undef WORD_SIZE
  friend std::ostream& operator<<(std::ostream&, const bit_set_t &);
};


#endif
