// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.hpp>

/* Once-only execution */
/*
  pthread_once_t representation (extends pthread_mutex_t):
  --------------------------------------------------------------------------------
 | *free* | should be init_routine called?: 1bit | < pthread_mutex_t (27 bits) > |
  --------------------------------------------------------------------------------
  */

int pthread_once( pthread_once_t *once_control, void ( *init_routine )( void ) )
{
    if ( once_control->__mtx.__once == 0 )
        return 0;

    pthread_mutex_lock( &once_control->__mtx );

    if ( once_control->__mtx.__once ) {
        init_routine();
        once_control->__mtx.__once = 0;
    }

    pthread_mutex_unlock( &once_control->__mtx );

    return 0;
}
