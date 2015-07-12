//https://www.realvnc.com/docs/rfbproto.pdf
//http://vncdotool.readthedocs.org/en/latest/rfbproto.html

#include <stdio.h>
#include <string.h>
#include "cnrfb.h"
#include "font.h"

struct CNRFBSt CNRFBs[MAX_RFB_CONNS];

uint8_t ServerInit[] = {
	RFB_WIDTH>>8, RFB_WIDTH&0xff,
	RFB_HEIGHT>>8, RFB_HEIGHT&0xff,

	8, //Bits per pixel.
	8, //Depth
	0, //Big endian (meaningless)
	1, //True color?
	//Color is RRRGGGBB
	0, 7, 0, 7, 0, 3,
	0, 3, 6, 0, 0, 0, 
};

int  CNRFBNewConnection( )
{
	int i;
	for( i = 0; i < MAX_RFB_CONNS; i++ )
	{
		if( CNRFBs[i].in_use == 0 )
		{
			printf( "Connecting at: %d\n", i );
			memset( &CNRFBs[i], 0, sizeof(struct CNRFBSt) );
			CNRFBs[i].in_use = 1;
			CNRFBs[i].state = 0;
			CNRFBs[i].timeout = 0;
			CNRFBs[i].use_zrle = 1;
			return i;
		}
	}
}

void CNRFBGotData( int conn, int recvqty )
{
	struct CNRFBSt * c = &CNRFBs[conn];
	c->timeout = 0;

	switch( c->state )
	{
	case 0:
	case 2: //Server just connected, or server already sent handshake.
		if( recvqty != 12 )
		{
			CNRFBNote( "Error: wrong number of bytes for handshake (%d).\n", recvqty );
			break;
		}
		else
		{
			CNRFBRead4();
			CNRFBRead4();
			CNRFBRead4();
		}
		//Todo: Check to make sure versions are compatible.
		c->state++;
		break;
	case 4:
	{
		uint8_t canshare = CNRFBRead1();
		c->state = 5;
		break;
	}
	case 6:
	case 7:
	case 8:
	{
		while( CNRFBReadRemain() )
		{
			//Normal operations mode.
			uint8_t cmd = CNRFBRead1();
			switch( cmd )
			{
			case 0: //Client requests a different pixel type.
			{
				CNRFBRead2(); CNRFBRead1();
				int bpp = CNRFBRead1();
				int dep = CNRFBRead1();
				int endian = CNRFBRead1();
				int tcolor = CNRFBRead1();
				CNRFBDump(12);

				if( !tcolor )
					c->need_to_send_colormap |= 1;
				break;
			}
			case 2:  //Got encodings
			{
				CNRFBRead2();
				uint16_t codes = CNRFBRead2();
				uint16_t i;
				for( i = 0; i != codes; i++ )
				{
					CNRFBRead4();
				}
				c->got_encodings = 1;
				break;
			}
			case 3: //Wants update!
			{
				int incremental = CNRFBRead1();
				c->update_requested = 1;
				CNRFBDump(8); //x,y,w,h
				c->state = 7;
				break;
			}
			case 4:
				{
					uint8_t down = CNRFBRead1();
					uint8_t key;
					CNRFBRead2();
					key = CNRFBRead4();
					GotKeyPress( conn, key, down );
				}
				break;
			case 5:
			{	//Pointer event
				int bm = CNRFBRead1();
				int x =  CNRFBRead2();
				int y =  CNRFBRead2();
				GotMouseEvent( conn, bm, x, y );
				break;
			}
			case 6:
			{	//Client Cut Text
				CNRFBDump(3);
				uint32_t len = CNRFBRead4();
				uint8_t buff[len];
				int i;
				for( i = 0; i < len; i++ )
					buff[i] = CNRFBRead1();
				GotClipboard( conn, len, buff );
				break;
			}
			default:
				printf( "Unknown code: %d\n", cmd );
			};
		}

		break;
	}
	default:
		break;
	}
}

void CNRFBConnectionWasClosed( int conn )
{
	CNRFBs[conn].in_use = 0;

	CNRFBCloseConnection( conn );
}

void CNRFBInit()
{
	memset( CNRFBs, 0, sizeof( CNRFBs ) );
}

void CNRFBTick( int slow_tick )
{
	static uint8_t cl;

	cl++;
	if( cl >= MAX_RFB_CONNS ) cl = 0;

	int i, j;

	//Do for all clients.
	for( i = 0; i < MAX_RFB_CONNS; i++ )
	{
		struct CNRFBSt * c = &CNRFBs[i];
		if( c->in_use == 0 ) continue;

		if( slow_tick )
		{
			if( c->timeout++ > MAX_RFB_TIMEOUT )
			{
				CNRFBNote( "Connection %d timed out.\n", i );
				c->in_use = 0;
				CNRFBCloseConnection( i );
				continue;
			}
		}

		if( CNRFBCanSendData( i ) )
		{
			switch( c->state )
			{
			case 1:  //cases 012 are for handshaking
			case 0:
				CNRFBStartSend( i );
				CNRFBSendData( "RFB 003.003\n", 12 );
				CNRFBEndSend( i );
				c->state += 2;
				break;
			case 2: break;
			case 3: //Security handshake
				CNRFBStartSend( i );
				CNRFBSend4( 1 ); //No security.
				CNRFBEndSend();
				c->state = 4;
				break;
			case 4: break;
			case 5: //Got desktop share flag
			{
				CNRFBStartSend( i );
				CNRFBSendData( ServerInit, 20 );
				int nl = strlen( RFB_SERVER_NAME );
				CNRFBSend4( nl );
				CNRFBSendData( RFB_SERVER_NAME, nl );
				CNRFBEndSend();

				c->state = 6;  //Move to and expect a clientinit message.
				break;
			}
			case 6:
				if( c->got_encodings )
					c->state = 7;
				break;
			case 7:
			{
				c->update_requested = 1;
				c->state = 8;
				break;
			}
			default: //8 = normal state
				if( c->need_to_send_colormap == 1 ) //Split color map up into two packets so it's not enormous.
				{
					int whichset = c->need_to_send_colormap-1;

					CNRFBStartSend( i );
					CNRFBSend1( 1 ); //Palette
					CNRFBSend1( 0 ); //Padding
					CNRFBSend2( 0 );
					CNRFBSend2( 256 );

					for( j = 0; j < 128; j++ )
					{
						uint8_t color = j;
						CNRFBSend2( (color & 0x07)<<(5+8) );
						CNRFBSend2( ((color>>3) & 0x07)<<(5+8) );
						CNRFBSend2( ((color>>6) & 0x03)<<(6+8) );
					}
					CNRFBEndSend();
					c->need_to_send_colormap |= 2; ///Really weird logic to make it possible to re-send the color map mid-way through the colormap.
					c->need_to_send_colormap &= 2;
				}
				else if( (c->need_to_send_colormap & 2) == 2 )
				{
					CNRFBStartSend( i );
					for( j = 0; j < 128; j++ )
					{
						uint8_t color = j+128;
						CNRFBSend2( (color & 0x07)<<(5+8) );
						CNRFBSend2( ((color>>3) & 0x07)<<(5+8) );
						CNRFBSend2( ((color>>6) & 0x03)<<(6+8) );
					}
					CNRFBEndSend();
					c->need_to_send_colormap &= 1;
				}
				else if( c->update_requested )
				{
					FreeFrameForClient( i );
				}
				break;

			}
		}

	}

}


//17 bytes each.
void DrawRect( uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color )
{
	CNRFBSend2( x );
	CNRFBSend2( y );
	CNRFBSend2( w );
	CNRFBSend2( h );
	CNRFBSend4( 2 ); //RRE
	CNRFBSend4( 0 );
	CNRFBSend1( color );
}

void DrawRectAtAsPartOfRRE( uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color )
{
	CNRFBSend1( color );
	CNRFBSend2( x );
	CNRFBSend2( y );
	CNRFBSend2( w );
	CNRFBSend2( h );
}


int StartFrameDraw( int conn, int nrdr )
{
	if( !CNRFBCanSendData( conn ) ) return -1;
	CNRFBStartSend( conn );

	CNRFBSend1( 0 ); //FramebufferUpdate
	CNRFBSend1( 0 ); //Padding
	CNRFBSend2( nrdr );
	return 0;
}


void StartRRE( int qty, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t bg )
{
	CNRFBSend2( x );
	CNRFBSend2( y );
	CNRFBSend2( w );
	CNRFBSend2( h );
	CNRFBSend4( 2 ); //RRE
	CNRFBSend4( qty );
	CNRFBSend1( bg );
}

int CountCharForRRE( uint8_t ch )
{
	return font_places[ch+1] - font_places[ch];
}

void DrawCharAtAsPartOfRRE( uint8_t ch, uint8_t fg, uint16_t offsetx, uint16_t offsety )
{
	uint16_t pla = font_places[ch];
	uint16_t end = font_places[ch+1];
	for( ; pla != end; pla++ )
	{
		uint16_t col = font_pieces[pla];
		CNRFBSend1( fg );
		CNRFBSend2( ((col>>0)&0xf) + offsetx );
		CNRFBSend2( ((col>>4)&0xf) + offsety );
		CNRFBSend2( ((col>>8)&0xf) + 1 );
		CNRFBSend2( ((col>>12)&0xf) + 1 );
	}
}

void PrintText( const char * text, uint8_t fg, uint8_t bg, uint16_t border, uint16_t x, uint16_t y )
{
	int qty = 0;
	const char * c = text;
	uint16_t maxw = 0, maxh = FONT_CHAR_H;
	uint16_t xp = 0, yp = 0;

	for( ; *c; c++ )
	{
		switch( *c )
		{
		case '\n': xp = 0; maxh+= FONT_CHAR_H; break;
		case '\t': xp += FONT_CHAR_W * 4; break;
		default: qty += CountCharForRRE( *c ); xp += FONT_CHAR_W; break;
		}
		if( xp > maxw ) maxw = xp;
	}

	xp = border;
	yp = border;
	StartRRE( qty, x, y, maxw + border*2, maxh + border*2, bg );

	for( c = text; *c; c++ )
	{
		switch( *c )
		{
		case '\n': xp = border; yp+= FONT_CHAR_H; break;
		case '\t': xp += FONT_CHAR_W * 4; break;
		default: DrawCharAtAsPartOfRRE(*c, fg, xp, yp); xp += FONT_CHAR_W; break;
		}
	}
}


void EndFrameDraw(int conn)
{
	CNRFBs[conn].update_requested = 0;
	CNRFBEndSend();
}

