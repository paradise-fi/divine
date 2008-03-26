/*
 * Gregorian calendar functions
 *
 * Copyright (C) 2007--2008  Enrico Zini <enrico@debian.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include <wibble/grcal/grcal.h>
#include <wibble/exception.h>
#include <ctime>
#include <sstream>

using namespace std;

namespace wibble {
namespace grcal {

namespace date {

int daysinmonth(int year, int month)
{
	// Normalise month
	--month;
	year += month / 12;
	month = month % 12;

	switch (month)
	{
		case  1-1: return 31;
		case  2-1:
			if (year % 400 == 0 || (year % 4 == 0 && ! (year % 100 == 0)))
				return 29;
			return 28;
		case  3-1: return 31;
		case  4-1: return 30;
		case  5-1: return 31;
		case  6-1: return 30;
		case  7-1: return 31;
		case  8-1: return 31;
		case  9-1: return 30;
		case 10-1: return 31;
		case 11-1: return 30;
		case 12-1: return 31;
		default: {
			stringstream str;
			str << "Month '" << month+1 << "' is not between 1 and 12";
			throw wibble::exception::Consistency("computing number of days in month", str.str());
		}
	}
}

int daysinyear(int year)
{
	if (year % 400 == 0 || (year % 4 == 0 && ! (year % 100 == 0)))
		return 366;
	return 365;
}

void easter(int year, int* month, int* day)
{
	// Meeus/Jones/Butcher Gregorian algorithm
	// from http://en.wikipedia.org/wiki/Computus
	int a = year % 19;
	int b = year / 100;
	int c = year % 100;
	int d = b / 4;
	int e = b % 4;
	int f = (b + 8) / 25;
	int g = (b - f + 1) / 3;
	int h = (19 * a + b - d - g + 15) % 30;
	int i = c / 4;
	int k = c % 4;
	int L = (32 + 2 * e + 2 * i - h - k) % 7;
	int m = (a + 11 * h + 22 * L) / 451;
	*month = (h + L - 7 * m + 114) / 31;
	*day = ((h + L - 7 * m + 114) % 31) + 1;
}


void lowerbound(const int* src, int* dst)
{
	dst[0] = src[0];
	dst[1] = src[1] != -1 ? src[1] : 1;
	dst[2] = src[2] != -1 ? src[2] : 1;
	dst[3] = src[3] != -1 ? src[3] : 0;
	dst[4] = src[4] != -1 ? src[4] : 0;
	dst[5] = src[5] != -1 ? src[5] : 0;
}

void lowerbound(int* val)
{
	if (val[1] == -1) val[1] = 1; else return;
	if (val[2] == -1) val[2] = 1; else return;
	if (val[3] == -1) val[3] = 0; else return;
	if (val[4] == -1) val[4] = 0; else return;
	if (val[5] == -1) val[5] = 0; else return;
}

void normalise(int* res)
{
	// Normalise seconds
	res[4] += res[5] / 60;
	res[5] = res[5] % 60;

	// Normalise minutes
	res[3] += res[4] / 60;
	res[4] = res[4] % 60;

	// Normalise hours
	res[2] += res[3] / 24;
	res[3] = res[3] % 24;

	// Normalise days
	while (res[2] < 0)
	{
		--res[1];
		res[2] += daysinmonth(res[0], res[1]);
	}
	while (true)
	{
		int dim = daysinmonth(res[0], res[1]);
		if (res[2] <= dim) break;
		res[2] -= dim;
		++res[1];
	}

	// Normalise months
	res[0] += (res[1]-1) / 12;
	res[1] = ((res[1]-1) % 12) + 1;
}

void upperbound(const int* src, int* dst)
{
	// Lowerbound and increment the last valid one
	for (int i = 0; i < 6; ++i)
		if (src[i] == -1)
			dst[i] = (i == 1 || i == 2) ? 1 : 0;
		else if (i != 5 && src[i+1] != -1)
			dst[i] = src[i];
		else
			dst[i] = src[i] + 1;
	// Then decrement the second, to get an inclusive range
	--dst[5];
	// Normalise the result
	normalise(dst);
}

void upperbound(int* val)
{
	// Lowerbound and increment the last valid one
	for (int i = 0; i < 6; ++i)
		if (val[i] == -1)
			val[i] = (i == 1 || i == 2) ? 1 : 0;
		else if (i != 5 && val[i+1] != -1)
			;
		else
			++val[i];
	// Then decrement the second, to get an inclusive range
	--val[5];
	// Normalise the result
	normalise(val);
}

/**
 * Convert the given time in seconds elapsed since the beginning of the given
 * year.  It is assumed that year <= t[0]
 */
long long int secondsfrom(int year, const int* val)
{
	long long int res = 0;
	if (val[5] != -1) res += val[5];
	if (val[4] != -1) res += val[4]*60;
	if (val[3] != -1) res += val[3]*3600;
	if (val[2] != -1) res += (val[2]-1)*3600*24;
	int month = val[1] != -1 ? val[1] : 1;
	for (int i = 1; i < month; ++i)
		res += daysinmonth(val[0], month) * 3600*24;
	for (int i = year; i < val[0]; ++i)
		res += daysinyear(i) * 3600*24;
	return res;
}

// Give the duration of the interval between begin and end in seconds
long long int duration(const int* begin, const int* end)
{
	int b[6];
	int e[6];

	// Fill in missing values appropriately
	upperbound(begin, b);
	lowerbound(end, e);

	// Find the smaller year, to use as a reference for secondsfrom
	int y = b[0] < e[0] ? b[0] : e[0];
	return secondsfrom(y, e) - secondsfrom(y, b);
}

void mergetime(const int* date, const int* time, int* dst)
{
	dst[0] = date[0];
	dst[1] = date[1] != -1 ? date[1] : 1;
	dst[2] = date[2] != -1 ? date[2] : 1;
	for (int i = 0; i < 3; ++i)
		dst[3+i] = time[i];
}

void mergetime(int* date, const int* time)
{
	if (date[1] == -1) date[1] = 1;
	if (date[2] == -1) date[2] = 1;
	for (int i = 0; i < 3; ++i)
		date[3+i] = time[i];
}

void totm(const int* src, struct tm* dst)
{
	dst->tm_year = src[0] - 1900;
	dst->tm_mon = src[1] != -1 ? src[1] - 1 : 0;
	dst->tm_mday = src[2] != -1 ? src[2] : 1;
	dst->tm_hour = src[3] != -1 ? src[3] : 0;
	dst->tm_min = src[4] != -1 ? src[4] : 0;
	dst->tm_sec = src[5] != -1 ? src[5] : 0;
}

void fromtm(const struct tm& src, int* dst, int count)
{
	dst[0] = src.tm_year + 1900;
	dst[1] = count < 2 ? -1 : src.tm_mon + 1;
	dst[2] = count < 3 ? -1 : src.tm_mday;
	dst[3] = count < 4 ? -1 : src.tm_hour;
	dst[4] = count < 5 ? -1 : src.tm_min;
	dst[5] = count < 6 ? -1 : src.tm_sec;
}

}

namespace time {


void lowerbound(const int* src, int* dst)
{
	for (int i = 0; i < 3; ++i)
		dst[i] = src[i] != -1 ? src[i] : 0;
}

void lowerbound(int* val)
{
	for (int i = 0; i < 3; ++i)
		if (val[i] == -1) val[1] = 0;
}

int lowerbound_sec(const int* src)
{
	int res = 0;
	if (src[0] != -1) res += src[0] * 3600;
	if (src[1] != -1) res += src[1] * 60;
	if (src[2] != -1) res += src[2];
	return res;
}


void upperbound(const int* src, int* dst)
{
	dst[0] = src[0] != -1 ? src[0] : 23;
	dst[1] = src[1] != -1 ? src[1] : 59;
	dst[2] = src[2] != -1 ? src[2] : 59;
}

void upperbound(int* val)
{
	if (val[0] == -1) val[0] = 23;
	if (val[1] == -1) val[1] = 59;
	if (val[2] == -1) val[2] = 59;
}

int upperbound_sec(const int* src)
{
	int res = 0;
	res += (src[0] != -1 ? src[0] : 23) * 3600;
	res += (src[1] != -1 ? src[1] : 59) * 60;
	res += (src[2] != -1 ? src[2] : 59);
	return res;
}

int duration(const int* begin, const int* end)
{
	return lowerbound_sec(end) - upperbound_sec(begin);
}

}

}
}

// vim:set ts=4 sw=4:
