//Confusing tool to turn .pbm fonts into compressed RREs for RFB

#include <stdio.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ECHARS 256
#define MAX_RECTS_PER_CHAR 100

#define DOUBLEPACK

int cw, ch;
int w, h;
int bytes;
uint8_t * buff;

struct MRect
{
	uint8_t x, y, w, h;
};

int ReadFile( const char * rn );
void DumpBuffer( uint8_t * dat, int len, char * name )
{

	printf( "const unsigned char %s[%d] = {", name, len );
	int i;
	for( i = 0; i < len; i++ )
	{
		if( (i%16) == 0 )
		{
			printf( "\n\t" );
		}	
		printf( "0x%02x, ", dat[i] );
	}
	printf( "\n};\n\n" );
}
void DumpBuffer16( uint16_t * dat, int len, char * name )
{

	printf( "const unsigned short %s[%d] = {", name, len );
	int i;
	for( i = 0; i < len; i++ )
	{
		if( (i%16) == 0 )
		{
			printf( "\n\t" );
		}	
		printf( "0x%04x, ", dat[i] );
	}
	printf( "\n};\n\n" );
}

int TryCover( uint8_t * ibuff, struct MRect * r )
{
	int x, y;
	int count = 0;
	for( y = r->y; y < r->y+r->h; y++ )
	{
		for( x = r->x; x < r->x+r->w; x++ )
		{
			if( ibuff[x+y*cw] == 1 ) count++;
			else if( ibuff[x+y*cw] == 0 ) return -1;
		}
	}
	return count;
}

void DoCover( uint8_t * ibuff, struct MRect * r )
{
	int x, y;
	int count = 0;
	for( y = r->y; y < r->y+r->h; y++ )
	{
		for( x = r->x; x < r->x+r->w; x++ )
		{
			if( ibuff[x+y*cw] > 0 ) ibuff[x+y*cw]++;
		}
	}
}


int Covers( uint8_t * ibuff, struct MRect * rs )
{
	int i;
	int x, y, w, h;

	for( i = 0; i < MAX_RECTS_PER_CHAR; i++ )
	{
		struct MRect most_efficient, tmp;
		int most_efficient_count = 0;
		for( y = 0; y < ch; y++ )
		for( x = 0; x < cw; x++ )
		for( h = 1; h <= ch-y; h++ )
		for( w = 1; w <= cw-x; w++ )
		{
#ifdef DOUBLEPACK
			if( w > 16 || h > 16 || x > 16 || y > 16 ) continue;
#endif
			tmp.x = x; tmp.y = y; tmp.w = w; tmp.h = h;
			int ct = TryCover( ibuff, &tmp );
			if( ct > most_efficient_count )
			{
				memcpy( &most_efficient, &tmp, sizeof( tmp ) );
				most_efficient_count = ct;
			}
		}

		if( most_efficient_count == 0 )
		{
			return i;
		}

		DoCover( ibuff, &most_efficient );
		memcpy( &rs[i], &most_efficient, sizeof( struct MRect ) );
	}
	fprintf( stderr, "Error: too many rects per char.\n ");
	exit( -22 );
}

int GreedyChar( int chr, int debug, struct MRect * RectSet )
{
	int x, y, i;
	uint8_t cbuff[ch*cw];
	int rectcount;

	for( y = 0; y < ch; y++ )
	for( x = 0; x < cw/8; x++ )
	{
		int ppcx = w/cw;
		int xpos = (chr % ppcx)*cw;
		int ypos = (chr / ppcx)*ch;	
		int idx = xpos/8+(ypos+y)*w/8+x;
		int inpos = buff[idx];

		for( i = 0; i < 8; i++ )
		{
			cbuff[x*8+y*cw+i] = (inpos&(1<<(7-i)))?0:1;
		}
	}


	//Greedily find the minimum # of rectangles that can cover this.
	rectcount = Covers( cbuff, RectSet );

	if( debug )
	{
		printf( "Char %d:\n", chr );
		for( i = 0; i < rectcount; i++ )
		{
			printf( "  %d %d %d %d\n", RectSet[i].x, RectSet[i].y, RectSet[i].w, RectSet[i].h );
		}
		printf( "\n" );

		//Print a char for test.
		printf( "%d\n", ch );
		for( y = 0; y < ch; y++ )
		{
			for( x = 0; x < cw; x++ )
			{
				printf( "%d", cbuff[x+y*cw] );
			}
			printf( "\n" );
		}
	}

	return rectcount;
}


int main( int argc, char ** argv )
{
	int r, i, x, y, j;

	if( argc != 4 )
	{
		fprintf( stderr, "Got %d args.\nUsage: [tool] [input_pbm] [char w] [char h]\n", argc );
		return -1;
	}

	if( (r = ReadFile( argv[1] )) )
	{
		return r;
	}

	cw = atoi( argv[2] );
	ch = atoi( argv[3] );

	printf( "#define FONT_CHAR_W %d\n", cw );
	printf( "#define FONT_CHAR_H %d\n\n", ch );

	if( ( cw % 8 ) != 0 )
	{
		fprintf( stderr, "Error: CW MUST be divisible by 8.\n" );
	}

//	struct MRect MRect rs;
//	GreedyChar( 0xdc, 1, &rs );

	struct MRect MRects[ECHARS*MAX_RECTS_PER_CHAR];
	uint16_t places[ECHARS+1];
	int place = 0;
	for( i = 0; i < ECHARS; i++ )
	{
		places[i] = place;
		place += GreedyChar( i, 0, &MRects[place] );
	}
	places[i] = place;

	uint16_t outbuffer[ECHARS*MAX_RECTS_PER_CHAR*2];
	for( i = 0; i < place; i++ )
	{
		int x = MRects[i].x;
		int y = MRects[i].y;
		int w = MRects[i].w;
		int h = MRects[i].h;
		if( w == 0 || w > 16 ) { fprintf( stderr, "Error: invalid W value\n" ); return -5; }
		if( h == 0 || h > 16 ) { fprintf( stderr, "Error: invalid H value\n" ); return -5; }
		if( x > 15 ) { fprintf( stderr, "Error: invalid X value\n" ); return -5; }
		if( y > 15 ) { fprintf( stderr, "Error: invalid Y value\n" ); return -5; }
		w--;
		h--;
		outbuffer[i] = x | (y<<4) | (w<<8) | (h<<12);
	}

	fprintf( stderr, "Total places: %d\n", place );
	DumpBuffer16( outbuffer, place, "font_pieces" );
	DumpBuffer16( places, ECHARS+1, "font_places" );

	return 0;
}





int ReadFile( const char * rn )
{
	int r;
	char ct[1024];
	char * cct;
	FILE * f = fopen( rn, "r" );

	if( !f )
	{
		fprintf( stderr, "Error: Cannot open file.\n" );
		return -11;
	}

	if( fscanf( f, "%1023s\n", ct ) != 1 || strcmp( ct, "P4" ) != 0 )
	{
		fprintf( stderr, "Error: Expected P4 header.\n" );
		return -2;
	}

	size_t sizin = 0;
	cct = 0;
	ssize_t s = getline( &cct, &sizin, f );

	if( !cct || cct[0] != '#' )
	{
		fprintf( stderr, "Error: Need a comment line.\n" );
		return -3;
	}
	free( cct );

	if( (r = fscanf( f, "%d %d\n", &w, &h )) != 2 || w <= 0 || h <= 0 )
	{
		fprintf( stderr, "Error: Need w and h in pbm file.  Got %d params.  (%d %d)\n", r, w, h );
		return -4;
	}

	bytes = (w*h)>>3;
	buff = malloc( bytes );
	r = fread( buff, 1, bytes, f ); 
	if( r != bytes )
	{
		fprintf( stderr, "Error: Ran into EOF when reading file.  (%d)\n",r  );
		return -5;
	}

	fclose( f );
	return 0;
}
