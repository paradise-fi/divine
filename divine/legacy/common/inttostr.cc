#include <cstdlib>
#include "divine/legacy/common/inttostr.hh"
#include <sstream>
#include <cstring>

using namespace std;

//deprecated:
//extern "C" {
//extern int asprintf (char **__restrict __ptr,
//                     __const char *__restrict __fmt, ...)
//                    /*__THROW __attribute__ ((__format__ (__printf__, 2, 3)))*/;
//}

char * divine::create_string_from_uint(const unsigned long int i)
{
 char * auxstr;
 ostringstream oss;
 oss << i;
 std::size_t size = oss.str().size()+1;
 auxstr = new char[size];
 strncpy(auxstr,oss.str().c_str(),size);
 return auxstr;
}

char * divine::create_string_from_int(const signed long int i)
{
 char * auxstr;
 ostringstream oss;
 oss << i;
 std::size_t size = oss.str().size()+1;
 auxstr = new char[size];
 strncpy(auxstr,oss.str().c_str(),size);
 return auxstr;
}

void divine::dispose_string(char * const str)
{ delete [] str; }

