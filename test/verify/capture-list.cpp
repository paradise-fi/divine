/* TAGS: posix c++ */
/* VERIFY_OPTS: --capture $(dirname $1)/subdir:follow:/ -o nofail:malloc */
#include <dirent.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <string_view>
#include <set>
#include <cassert>
#include <sys/trace.h>

using namespace std::literals;

std::set< std::string > paths;

void list( std::string entry, std::string name, std::string indent )
{
    assert( paths.count( entry ) == 0 );
    __dios_trace_f( "found: %s", entry.c_str() );
    paths.insert( entry );

    DIR *d = opendir( entry.c_str() );
    if ( d )
    {
        struct dirent *dir;
        indent += " ";
        while ( (dir = readdir( d )) != nullptr )
        {
            if ( dir->d_name == "."sv || dir->d_name == ".."sv )
                continue;
            list( ( entry == "/" ? "/" : entry + "/" ) + dir->d_name, dir->d_name, indent );
        }
        closedir( d );
    }
}

int main()
{
    char from[ 256 ];
    getcwd( from, sizeof( from ) );
    list( from, from, "" );
    assert( paths.size() == 4 );
    assert( paths.count( "/" ) );
    assert( paths.count( "/foo" ) );
    assert( paths.count( "/bar" ) );
    assert( paths.count( "/bar/baz" ) );
}
