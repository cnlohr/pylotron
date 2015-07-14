#include "mathfuncts.h"
#include "mathtables.h"

short tsin( unsigned char angle )
{
	if( angle < 128 )
	{
		if( angle < 64 )
		{
			return sintab[angle];
		}
		else
		{
			return sintab[128-angle];
		}
	}
	else
	{
		if( angle < (64+128) )
		{
			return -sintab[angle-128];
		}
		else
		{
			return -sintab[256-angle];
		}
	}
}

short tatan2( short dy, short dx )
{
	unsigned short absx = (dx<0)?-dx:dx;
	unsigned short absy = (dy<0)?-dy:dy;

	unsigned char ang;
	char angmode;

	if( absx == 0 && absy == 0 ) return 0;

	if( absx < absy )
	{
		ang = absx * 128 / absy;
		angmode = 1;
	}
	else
	{
		ang = absy * 128 / absx;
		angmode = 0;
	}

	//Ang is now 0..63 but could be from either mode.
	short tang = (atancorrect[ang]);
	if( angmode ) tang = 511 - tang;
	if( dx < 0 )  tang = 1023 - tang;
	if( dy < 0 )  tang = -tang;

	return tang<<5;
}

//from http://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
unsigned short tasqrt( unsigned long n )
{
    unsigned long res = 0;
    unsigned long one = 1uL << 30;

    // "one" starts at the highest power of four <= than the argument.
    while (one > n)
    {
        one >>= 2;
    }

    while (one != 0)
    {
        if (n >= res + one)
        {
            n = n - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

