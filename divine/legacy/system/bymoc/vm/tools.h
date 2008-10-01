/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_tools
#define INC_tools

#include <sys/types.h>
#ifdef WIN32
# include <winsock2.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#endif

// minimum and maximum
#ifndef min
#  define min( a, b ) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#  define max( a, b ) ((a) > (b) ? (a) : (b))
#endif

// number of entries in an array
#define count( array ) (sizeof( (array) ) / sizeof( (array)[0] ))

// packed (and unaligned) structures
#define PACKED __attribute__((packed))

// types to define macros for unaligned reads and writes
typedef struct { uint16_t u16; } PACKED ua_16;
typedef struct { uint32_t u32; } PACKED ua_32;
// unaligned reads
#define ua_rd_16( ptr ) (((ua_16 *)(ptr))->u16)
#define ua_rd_32( ptr ) (((ua_32 *)(ptr))->u32)
// unaligned writes
#define ua_wr_16( ptr, val ) (((ua_16 *)(ptr))->u16 = (uint16_t)(val))
#define ua_wr_32( ptr, val ) (((ua_32 *)(ptr))->u32 = (uint32_t)(val))

// byte order (big endian)
// we steal this code from netinet/in.h
#define h2be_16( x ) ((uint16_t)htons( (uint16_t)(x) ))
#define h2be_32( x ) ((uint32_t)htonl( (uint32_t)(x) ))
#define be2h_16( x ) ((uint16_t)ntohs( (uint16_t)(x) ))
#define be2h_32( x ) ((uint32_t)ntohl( (uint32_t)(x) ))

// byte oder (little endian)
// use big endian macros and swap bytes
// FIXME: there must be a better way
#define byteswap_16( x ) ((uint16_t)x << 8 | (uint16_t)x >> 8)
#define byteswap_32( x ) ((uint32_t)x << 24 | ((uint32_t)x << 8 & 0x00FF0000) | ((uint32_t)x >> 8 & 0x0000FF00) | (uint32_t)x >> 24)
#define h2le_16( x ) (byteswap_16( h2be_16( x ) ))
#define h2le_32( x ) (byteswap_32( h2be_32( x ) ))
#define le2h_16( x ) (be2h_16( byteswap_16( x ) ))
#define le2h_32( x ) (be2h_32( byteswap_32( x ) ))

#endif // #ifndef INC_tools
