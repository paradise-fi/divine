/* _PDCLIB_stdinit

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

/* This is an example initialization of stdin, stdout and stderr to the integer
   file descriptors 0, 1, and 2, respectively. This applies for a great variety
   of operating systems, including POSIX compliant ones.
*/

#include <stdio.h>

#ifndef REGTEST
#include "_PDCLIB/io.h"
#include <threads.h>

/* In a POSIX system, stdin / stdout / stderr are equivalent to the (int) file
   descriptors 0, 1, and 2 respectively.
*/
/* TODO: This is proof-of-concept, requires finetuning. */
static char _PDCLIB_serr_buffer[BUFSIZ];

static unsigned char _PDCLIB_sin_ungetbuf[_PDCLIB_UNGETCBUFSIZE];
static unsigned char _PDCLIB_sout_ungetbuf[_PDCLIB_UNGETCBUFSIZE];
static unsigned char _PDCLIB_serr_ungetbuf[_PDCLIB_UNGETCBUFSIZE];

extern _PDCLIB_fileops_t _PDCLIB_fileops;

static FILE _PDCLIB_serr = { 
    .ops        = &_PDCLIB_fileops, 
    .handle     = { .sval = 2 }, 
    .buffer     = _PDCLIB_serr_buffer, 
    .bufsize    = BUFSIZ, 
    .bufidx     = 0, 
    .bufend     = 0, 
    .ungetidx   = 0, 
    .ungetbuf   = _PDCLIB_serr_ungetbuf, 
    .status     = _IONBF | _PDCLIB_FWRITE | _PDCLIB_STATIC, 
    .filename   = NULL, 
    .next       = NULL,
};
static FILE _PDCLIB_sout = { 
    .ops        = &_PDCLIB_fileops, 
    .handle     = { .sval = 1 },
    .buffer     = NULL,
    .bufsize    = 0,
    .bufidx     = 0, 
    .bufend     = 0, 
    .ungetidx   = 0, 
    .ungetbuf   = _PDCLIB_sout_ungetbuf, 
    .status     = _IONBF | _PDCLIB_FWRITE | _PDCLIB_STATIC,
    .filename   = NULL, 
    .next       = &_PDCLIB_serr 
};
static FILE _PDCLIB_sin  = { 
    .ops        = &_PDCLIB_fileops, 
    .handle     = { .sval = 0 }, 
    .buffer     = NULL,
    .bufsize    = 0,
    .bufidx     = 0, 
    .bufend     = 0, 
    .ungetidx   = 0, 
    .ungetbuf   = _PDCLIB_sin_ungetbuf, 
    .status     = _IONBF | _PDCLIB_FREAD | _PDCLIB_STATIC,
    .filename   = NULL, 
    .next       = &_PDCLIB_sout 
};

FILE * stdin  = &_PDCLIB_sin;
FILE * stdout = &_PDCLIB_sout;
FILE * stderr = &_PDCLIB_serr;

/* FIXME: This approach is a possible attack vector. */
FILE * _PDCLIB_filelist = &_PDCLIB_sin;
mtx_t _PDCLIB_filelist_lock;


/* Todo: Better solution than this! */
__attribute__((constructor)) static void init_stdio(void)
{
    if ( stdin )
        mtx_init(&stdin->lock,  mtx_recursive);
    if ( stdout )
        mtx_init(&stdout->lock, mtx_recursive);
    if ( stderr )
        mtx_init(&stderr->lock, mtx_recursive);
#ifndef __divine__ // for now
    mtx_init( &_PDCLIB_filelist_lock, mtx_recursive );
#endif
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"

int main( void )
{
    /* Testing covered by several other testdrivers using stdin / stdout / 
       stderr.
    */
    return TEST_RESULTS;
}

#endif
