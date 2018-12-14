/* TAGS: fork threads c */
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


* This sample test aims to check the following assertion:
*
* NULL can be passed as any of these handlers when no treatment is required.

* The steps are:
* -> Create a new thread
* -> Try all NULL / non NULL combinations (7) of pthread_atfork parameters.

* The test fails if the registered handlers are not executed as expected.

*/

/* We are testing conformance to IEEE Std 1003.1, 2003 Edition */
#define _POSIX_C_SOURCE 200112L

#include <pthread.h>
#include <unistd.h>
#include <assert.h>

int iPrepare = 0, iParent = 0, iChild = 0;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

/* pthread_atfork handlers */
/* 0: NULL NULL NULL  (1)
   1:  p1  NULL NULL  (2)
   2: NULL pa2  NULL  (4)
   3: NULL NULL  c3   (8)
   4:  p4  pa4  NULL  (16)
   5:  p5  NULL  c5   (32)
   6: NULL pa6   c6   (64)
 tot:  50   84  104
 */
void p1( void ) { iPrepare |= 1 << 1; }
void p4( void ) { iPrepare |= 1 << 4; }
void p5( void ) { iPrepare |= 1 << 5; }

void pa2( void ) { iParent |= 1 << 2; }
void pa4( void ) { iParent |= 1 << 4; }
void pa6( void ) { iParent |= 1 << 6; }

void c3( void ) { iChild |= 1 << 3; }
void c5( void ) { iChild |= 1 << 5; }
void c6( void ) { iChild |= 1 << 6; }

void * threaded( void * arg )
{
	int ret, status;
	pid_t child, ctl;

	ret = pthread_mutex_lock( &mtx );
    if(ret != 0) return 0;

	ret = pthread_mutex_unlock( &mtx );
	if(ret != 0) return 0;

	child = fork();

	if ( child == ( pid_t ) - 1 )
        return 0;

	if ( child == ( pid_t ) 0 )
	{
		assert( iPrepare == 50 );
        assert( iChild == 104 );
        exit( 0 );
	}

	assert( iPrepare == 50 );
    assert( iParent == 84 );

	return NULL;
}

int main( int argc, char * argv[] )
{
	int ret;
	pthread_t ch;

	ret = pthread_mutex_lock( &mtx );
   	if(ret != 0) return 0;

	ret = pthread_create( &ch, NULL, threaded, NULL );
	if(ret != 0) return 0;

    ret = pthread_atfork( NULL, NULL, NULL );
	if(ret != 0) return 0;

	ret = pthread_atfork( p1, NULL, NULL );
	if(ret != 0) return 0;

	ret = pthread_atfork( NULL, pa2, NULL );
	if(ret != 0) return 0;

	ret = pthread_atfork( NULL, NULL, c3 );
	if(ret != 0) return 0;

	ret = pthread_atfork( p4, pa4, NULL );
	if(ret != 0) return 0;

	ret = pthread_atfork( p5, NULL, c5 );
	if(ret != 0) return 0;

	ret = pthread_atfork( NULL, pa6, c6 );
	if(ret != 0) return 0;

	/* Let the child go on */
	ret = pthread_mutex_unlock( &mtx );
	if(ret != 0) return 0;

	ret = pthread_join( ch, NULL );
	if(ret != 0) return 0;

    return 0;
}


