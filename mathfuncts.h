#ifndef _MATHFUNCTS_H
#define _MATHFUNCTS_H

short tsin( unsigned char angle ); //Output is -255 to +255
#define tcos( angle ) tsin( angle + 64 )

//If dx is close to 0, and dy is close to 1, answer will be close to 0. 
short tatan2( short dy, short dx );

unsigned short tasqrt( unsigned long totake );

#endif

