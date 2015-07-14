#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include <uart.h>
#include "osapi.h"
#include "espconn.h"
#include "mystuff.h"
#include <cnrfb.h>

void ICACHE_FLASH_ATTR SocketInit();

#define procTaskPrio        0
#define procTaskQueueLen    1

static struct espconn *prfbServer;

static volatile os_timer_t interval_timer;

//Tasks that happen all the time.
os_event_t    procTaskQueue[procTaskQueueLen];

static void ICACHE_FLASH_ATTR
procTask(os_event_t *events)
{
	system_os_post(procTaskPrio, 0, 0 );
	if( events->sig == 0 && events->par == 0 )
	{
		CNRFBTick( 0 );
		UpdateEvent( 0 );
	}
}

//Timer event.
static void ICACHE_FLASH_ATTR
myTimer(void *arg)
{
	int i;
	uart0_sendStr(".");

	ets_intr_lock(); 
	WS2812OutBuffer( "\x00\x04\x00", 3 );
	ets_intr_unlock(); 

	CNRFBTick( 1 );
	UpdateEvent( 1 );

}

void ICACHE_FLASH_ATTR
GotSerialByte( char c )
{
}

void ICACHE_FLASH_ATTR
user_init(void)
{
	ets_intr_lock(); 
	WS2812OutBuffer( "\x1f\x1f\x1f", 3 );
	ets_intr_unlock(); 

	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	uart0_sendStr("\r\nCustom Server\r\n");
	wifi_station_dhcpc_start();

	//Add a process
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);

	uart0_sendStr("\r\nCustom Server\r\n");

	//Timer example
	os_timer_disarm(&interval_timer);
	os_timer_setfn(&interval_timer, (os_timer_func_t *)myTimer, NULL);
	os_timer_arm(&interval_timer, 20, 1); //50 Hz.
 
	ets_intr_lock(); 
	WS2812OutBuffer( "\x1f\x1f\x00", 3 );
	ets_intr_unlock(); 

	//Hook for initialization
	SocketInit();
	CNRFBInit();
	ToolStart();


	system_os_post(procTaskPrio, 0, 0 );
}





///After here is the TCP/IP Interface

static struct espconn * ESPSockets[MAX_RFB_CONNS];

LOCAL uint8_t * readbuffer;
LOCAL uint16_t readbufferpos;
LOCAL uint16_t readbuffersize;

LOCAL uint8_t sendbuffer[1536];
LOCAL uint16_t  sendptr;
LOCAL int sendconn;

void CNRFBDump( int i )
{
	readbufferpos += i;
}


int CNRFBCanSendData( int sock )
{
	
	struct espconn * conn = ESPSockets[sock];
	struct espconn_packet infoarg;
	sint8 r = espconn_get_packet_info(conn, &infoarg);
	if( infoarg.snd_queuelen == 0 )
	{
		//This actually doesn't happen unless there's a fault.
		CNRFBConnectionWasClosed( sock );
		return 0;
	}
	if( infoarg.snd_buf_size < 1536 )
	{
		printf( "BU\n" );
		return 0;
	}

	return 1;
}

int  CNRFBStartSend( int conn )
{
	if( !CNRFBCanSendData( conn ) )
	{
		return -1;
	}

	sendconn = conn;

	sendptr = 0;
}

void CNRFBSendData( const uint8_t * data, int len )
{
	memcpy( sendbuffer + sendptr, data, len );
	sendptr += len;
}

void CNRFBSend1( uint8_t data )
{
	sendbuffer[sendptr++] = data;
}

void CNRFBSend2( uint16_t data )
{
	sendbuffer[sendptr++] = data>>8;
	sendbuffer[sendptr++] = data;
}

void CNRFBSend4( uint32_t data )
{
	sendbuffer[sendptr++] = data>>24;
	sendbuffer[sendptr++] = data>>16;
	sendbuffer[sendptr++] = data>>8;
	sendbuffer[sendptr++] = data;
}

void CNRFBEndSend( )
{
	if( sendconn >= 0 && sendptr )
	{
		espconn_sent(ESPSockets[sendconn],sendbuffer,sendptr);
	}
}


void CNRFBCloseConnection( int conn )
{
	int i;
	DisconnectEvent( conn );
	espconn_disconnect( ESPSockets[conn] );
	ESPSockets[conn] = 0;
}


int     CNRFBReadRemain()
{
	return readbuffersize - readbufferpos;
}

uint8_t CNRFBRead1()
{
	if( readbufferpos + 1 > readbuffersize ) return 0;
	return readbuffer[readbufferpos++];
}

uint16_t CNRFBRead2()
{
	uint16_t r;
	if( readbufferpos + 2 > readbuffersize ) return 0;
	r = readbuffer[readbufferpos++];
	r = (r<<8) | readbuffer[readbufferpos++];
	return r;
}

uint32_t CNRFBRead4()
{
	uint32_t r;
	if( readbufferpos + 4 > readbuffersize ) return 0;
	r = readbuffer[readbufferpos++];
	r = (r<<8) | readbuffer[readbufferpos++];
	r = (r<<8) | readbuffer[readbufferpos++];
	r = (r<<8) | readbuffer[readbufferpos++];
	return r;
}




LOCAL void ICACHE_FLASH_ATTR rfb_disconnetcb(void *arg)
{
    struct espconn *pespconn = (struct espconn *) arg;

	int st = (int)pespconn->reverse;

	CNRFBConnectionWasClosed( st );
}

LOCAL void ICACHE_FLASH_ATTR rfb_recvcb(void *arg, char *pusrdata, unsigned short length)
{
    struct espconn *pespconn = (struct espconn *) arg;
	int st = (int)pespconn->reverse;

	readbuffer = (uint8_t*)pusrdata;
	readbufferpos = 0;
	readbuffersize = length;

	CNRFBGotData( st, length );
}

void ICACHE_FLASH_ATTR rfbserver_connectcb(void *arg)
{
	int i;
    struct espconn *pespconn = (struct espconn *)arg;

	int r = CNRFBNewConnection();
	if( r >= MAX_RFB_CONNS || r < 0 )
	{
		espconn_disconnect( pespconn );
		return;
	}

	pespconn->reverse = (void*)r;
	ESPSockets[r] = pespconn;
	ConnectEvent( r );

    espconn_regist_recvcb( pespconn, rfb_recvcb );
    espconn_regist_disconcb( pespconn, rfb_disconnetcb );
}

void ICACHE_FLASH_ATTR SocketInit()
{
	espconn_tcp_set_max_con( 9 );
	prfbServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( prfbServer, 0, sizeof( struct espconn ) );
	espconn_create( prfbServer );
	prfbServer->type = ESPCONN_TCP;
    prfbServer->state = ESPCONN_NONE;
	prfbServer->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	prfbServer->proto.tcp->local_port = 5900;
    espconn_regist_connectcb(prfbServer, rfbserver_connectcb);
    espconn_accept(prfbServer);
    espconn_regist_time(prfbServer, 15, 0); //timeout
//	espconn_tcp_set_max_con_allow(prfbServer,8);  //???
}


