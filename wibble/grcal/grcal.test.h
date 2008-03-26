/* -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
               (c) 2007 Enrico Zini <enrico@enricozini.org> */

#include <wibble/test.h>
#include <wibble/grcal/grcal.h>

namespace {

using namespace std;
using namespace wibble;
using namespace wibble::grcal;

struct TestGrcalDate {

    Test daysinmonth()
    {
		// Trenta giorni ha novembre
        assert_eq(date::daysinmonth(2008, 11), 30);
		// Con april, giugno e settembre
        assert_eq(date::daysinmonth(2008, 4), 30);
        assert_eq(date::daysinmonth(2008, 6), 30);
        assert_eq(date::daysinmonth(2008, 9), 30);
		// Di ventotto ce n'è uno
        assert_eq(date::daysinmonth(2001, 2), 28);
        assert_eq(date::daysinmonth(2004, 2), 29);
        assert_eq(date::daysinmonth(2100, 2), 28);
        assert_eq(date::daysinmonth(2000, 2), 29);
		// Tutti gli altri ne han trentuno
        assert_eq(date::daysinmonth(2008, 1), 31);
        assert_eq(date::daysinmonth(2008, 3), 31);
        assert_eq(date::daysinmonth(2008, 5), 31);
        assert_eq(date::daysinmonth(2008, 7), 31);
        assert_eq(date::daysinmonth(2008, 8), 31);
        assert_eq(date::daysinmonth(2008, 10), 31);
        assert_eq(date::daysinmonth(2008, 12), 31);
    }

    Test daysinyear()
	{
        assert_eq(date::daysinyear(2001), 365);
        assert_eq(date::daysinyear(2004), 366);
        assert_eq(date::daysinyear(2100), 365);
        assert_eq(date::daysinyear(2000), 366);
	}

	Test easter()
	{
		int month, day;
		date::easter(2008, &month, &day);
		assert_eq(month, 3);
		assert_eq(day, 23);
	}
};

}

// vim:set ts=4 sw=4:
