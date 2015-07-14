#ifndef _MYSTUFF_H
#define _MYSTUFF_H

extern char generic_print_buffer[384];

#define printf( ... ) ets_sprintf( generic_print_buffer, __VA_ARGS__ );  uart0_sendStr( generic_print_buffer );
#define sprintf ets_sprintf
#define memset ets_memset
#define memcpy ets_memcpy
#define strlen ets_strlen

#endif
