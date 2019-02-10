/* TAGS: fork min threads c */
/*
* Copyright (c) 2005, Bull S.A..  All rights reserved.
* Created by: Sebastien Decugis

* This program is free software; you can redistribute it and/or modify it
* under the terms of version 2 of the GNU General Public License as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it would be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write the Free Software Foundation, Inc., 59
* Temple Place - Suite 330, Boston MA 02111-1307, USA.


* This sample test aims to check the following assertions:
*
* pthread_atfork registers the 'prepare' handler to be called before fork()
* processing in the context of the fork() calling thread.
*
* pthread_atfork registers the 'parent' handler to be called after fork()
* processing in the context of the fork() calling thread in the parent process.
*
* pthread_atfork registers the 'child' handler to be called after fork()
* processing in the context of the fork() calling thread in the child process.


* The steps are:
* -> Create a new thread
* -> Call pthread_atfork from the main thread.
* -> Child thread forks.
* -> Check that the handlers have been called, and in the context of the
*    calling thread.

* The test fails if the thread executing the handlers is not the one expected,
* or if the handlers are not executed.

*/

/* We are testing conformance to IEEE Std 1003.1, 2003 Edition */
#define _POSIX_C_SOURCE 200112L

#include <pthread.h>
#include <unistd.h>
#include <assert.h>

pthread_t threads[ 3 ];
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t ch;

/* at fork handlers */
void prepare( void ) {
    threads[ 0 ] = pthread_self();
}

void parent( void ) {
    threads[ 1 ] = pthread_self();
}

void child( void ) {
    threads[ 2 ] = pthread_self();
}

/* Thread function */
void * threaded( void * arg )
{
    int ret, status;
    pid_t child, ctl;

    ret = pthread_mutex_lock( &mtx );
    if ( ret != 0 ) return 0;

    ret = pthread_mutex_unlock( &mtx );
    if ( ret != 0 ) return 0;

    child = -1; // fork();

    if ( child == ( pid_t ) -1 )
        return 0;

    if ( child == ( pid_t ) 0 )
    {
        assert( pthread_equal( ch, threads[ 0 ] ) );
        assert( pthread_equal( pthread_self(), threads[ 2 ] ) );
        exit( 0 );
    }

    assert( pthread_equal( ch, threads[ 0 ] ) );
    assert( pthread_equal( pthread_self(), threads[ 1 ] ) );

    return NULL;
}

int main( int argc, char * argv[] )
{
    int ret;

    ret = pthread_mutex_lock( &mtx );
    if ( ret != 0 ) return 0;

    ret = pthread_create( &ch, NULL, threaded, NULL );
    if ( ret != 0 ) return 0;

    ret = pthread_atfork( prepare, parent, child );
    if ( ret != 0 ) return 0;

    /* Let the child go on */
    ret = pthread_mutex_unlock( &mtx );
    if ( ret != 0 ) return 0;

    ret = pthread_join( ch, NULL );
    if ( ret != 0 ) return 0;
}


