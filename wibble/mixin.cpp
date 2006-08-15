#include <wibble/config.h>
#include <wibble/mixin.h>
using namespace std;

#ifdef WIBBLE_COMPILE_TESTSUITE
#include <wibble/tests.h>

namespace wibble {
namespace tut {

struct mixin_shar {};
TESTGRP( mixin );

class Integer : public wibble::mixin::Comparable<Integer>
{
	int val;
public:
	Integer(int val) : val(val) {}
	bool operator<=( const Integer& o ) const { return val <= o.val; }
};

class Discard : public wibble::mixin::OutputIterator<Discard>
{
public:
	Discard& operator=(const int&)
	{
		return *this;
	}
};

// Test Comparable mixin
template<> template<>
void to::test< 1 >() {
	Integer i10(10);
	Integer i10a(10);
	Integer i20(20);

	// Test the base method first
	ensure(i10 <= i10a);
	ensure(i10a <= i10);
	ensure(i10 <= i20);
	ensure(! (i20 <= i10));

	// Test the other methods
	ensure(i10 != i20);
	ensure(!(i10 != i10a));

	ensure(i10 != i10a);
	ensure(!(i10 != i20));

	ensure(i10 < i20);
	ensure(!(i20 < i10));
	ensure(!(i10 < i10a));

	ensure(i20 > i10);
	ensure(!(i10 > i20));
	ensure(!(i10 > i10a));

	ensure(i10 >= i10a);
	ensure(i10a >= i10);
	ensure(i20 >= i10);
	ensure(! (i10 >= i20));
}

template<> template<>
void to::test< 2 >() {
	vector<int> data;
	data.push_back(1);
	data.push_back(2);
	data.push_back(3);

	std::copy(data.begin(), data.end(), Discard());
}

}
}

#endif
