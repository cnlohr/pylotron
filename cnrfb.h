#ifndef _CNRFB_H
#define _CNRFB_H



#include <stdint.h>

#define MAX_RFB_CONNS 8
#define MAX_RFB_TIMEOUT 10000

#define MAX_SEND_BUFFER 512

#define RFB_WIDTH 640
#define RFB_HEIGHT 480
#define RFB_SERVER_NAME "Test"

#define CHARBANKX 0
#define CHARBANKY (RFB_HEIGHT-32)

#define RFB_COLS 80
#define RFB_ROWS 60

#define CNRFBNote( x... ) printf( x );

// TCP/IP Interface (You must provide this)

int  CNRFBNewConnection( ); //If returns negative, indicates failure.
void CNRFBGotData( int conn, int recvqty );
void CNRFBConnectionWasClosed( int conn );
void CNRFBTick( int slow_tick ); //Slow ticks happen at 100 Hz?


int  CNRFBCanSendData( int conn ); //If returns nonzero, can send.
int  CNRFBStartSend( int conn );   //Returns nonzero if can't send.
void CNRFBSendData( const uint8_t * data, int len );
void CNRFBSend1( uint8_t data );
void CNRFBSend2( uint16_t data );
void CNRFBSend4( uint32_t data );
void CNRFBEndSend( );
void CNRFBCloseConnection( int conn );

int     CNRFBReadRemain();  //value of 0 indicates no more can read.
uint8_t CNRFBRead1();
uint16_t CNRFBRead2();
uint32_t CNRFBRead4();
void	CNRFBDump( int i );
void	CNRFBFinishCB(); //Call if you want to stop receiving and start sending.

//Callback

void FreeFrameForClient( int i );
void GotKeyPress( int client, int key, int down );
void GotMouseEvent( int client, int mask, int x, int y );
void GotClipboard( int client, int length, unsigned char * st );
// CNRFB Code

struct CNRFBSt
{
	int in_use:1;
	int use_zrle:1;
	int got_encodings:1;
	int update_requested:1;
	int need_to_send_colormap:2;
	uint8_t state:4;
	uint16_t timeout;
	uint8_t refresh_state:2; //0 = no refresh, 1,2,3 need to send respective.
	uint16_t refresh;
};

extern struct CNRFBSt CNRFBs[MAX_RFB_CONNS];

// Calls to the actual RFB system.

// Draw calls (can only be performed from within a screen update.

//Return value is 0 if ok, otherwise, can't start frame draw.
int StartFrameDraw( int conn, int qty );

//Its own thing.
void DrawRect( uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color );

//Treated as 1 rect, 17 bytes.
void StartRRE( int qty, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t bg ); //Treated as 1 rect. (4+sizeof(px)+(8+sizeof(px))*subrects) NOTE: qty can be 0!

int CountCharForRRE( uint8_t ch ); //returns count of subrects.
void DrawCharAtAsPartOfRRE( uint8_t ch, uint8_t fg, uint16_t offsetx, uint16_t offsety );
void DrawRectAtAsPartOfRRE( uint16_t x, uint16_t y, uint16_t w, uint16_t h,  uint8_t fg );  //9 bytes.
void PrintText( const char * text, uint8_t fg, uint8_t bg, uint16_t border, uint16_t x, uint16_t y ); //Treated as a rect.
void EndFrameDraw(int conn);

#endif


