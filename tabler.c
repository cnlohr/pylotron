#include <stdio.h>
#include <math.h>

int main()
{
	int i;

	//1/4 of a sinwave
	printf( "const unsigned char sintab[65] = {" );
	for( i = 0; i < 65; i++ )
	{
		float fv = i * 3.14159 / 128.0;
		float f = sin(fv);
		int iv = f*255.5;
		if( i % 16 == 0 ) printf( "\n\t" );
		printf( "0x%02x, ", iv );
	}
	printf( "};\n\n" );

	printf( "const unsigned char atancorrect[129] = {" );
	for( i = 0; i < 129; i++ )
	{
		float fv = i / 128.0;
		float f = atan(fv);
		f = f / 3.15159 * 4 * 256;
		int iv = f;
		if( i % 16 == 0 ) printf( "\n\t" );
		//printf( "%f, ", f );
		printf( "0x%02x, ", iv );
	}
	printf( "};\n" );

	return 0;
}
