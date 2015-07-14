#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <linux/sockios.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "cnrfb.h"


#define MAX_SOCKETS 10

int sockets[MAX_SOCKETS];

uint8_t sendbuffer[16384];
uint16_t sendptr;
uint8_t sendconn;

uint8_t readbuffer[65536];
uint16_t readbufferpos = 0;
uint16_t readbuffersize = 0;


void CNRFBDump( int i )
{
	while( i-- ) CNRFBRead1();
}

int  CNRFBCanSendData( int conn )
{
	int unsent;
	int error = ioctl( sockets[conn], SIOCOUTQ, &unsent );

	if( error || ( unsent > 1530 ) )
	{
		return 0;
	}
	else
	{
		return 1;
	}
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
	if( sendconn >= 0 )
	{
		int r = send( sockets[sendconn], sendbuffer, sendptr, MSG_NOSIGNAL | MSG_DONTWAIT );
		if( r != sendptr )
		{
			fprintf( stderr, "Error: could not send (%d) code %d (%p %d)\n", sockets[sendconn], r, sendbuffer, sendptr);
			CNRFBConnectionWasClosed( sockets[sendconn] );
		}
		sendptr = 0;
	}
}


void CNRFBCloseConnection( int conn )
{
	DisconnectEvent( conn );
	close( sockets[conn] );
	sockets[conn] = -1;
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


int main()
{
	int i;
	struct linger lin;
	int serversocket;
	struct sockaddr_in servaddr;
	struct pollfd fds[1];
	int lastticktime = 0;

	lin.l_onoff=1;
	lin.l_linger=0;

	for( i = 0; i < MAX_SOCKETS; i++ )
		sockets[i] = -1;

	if( ( serversocket = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
	{
		fprintf( stderr, "Error: Could not create socket.\n" );
		return -1;
	}

	memset( &servaddr, 0, sizeof( servaddr ) );
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
	servaddr.sin_port = htons( 5900 );
	printf( "Listening on port: %d\n", htons( servaddr.sin_port ) );

	if( bind( serversocket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 )
	{
		fprintf( stderr, "Error: could not bind to socket.\n" );
		return -1;
	}

	if( listen( serversocket, 5 ) < 0 )
	{
		fprintf( stderr, "Error: could not listen to socket.\n" );
		return -1;
	}

	setsockopt( serversocket, SOL_SOCKET, SO_LINGER,(void*)(&lin), sizeof(lin) );  

	CNRFBInit();

	ToolStart();

	while(1)
	{
		int rc;
//		double thistime = OGGetAbsoluteTime();
		struct timeval tv;
		gettimeofday( &tv, 0 );

		memset( fds, 0, sizeof( fds ) );
		fds[0].fd = serversocket;
		fds[0].events = POLLIN;

		rc = poll( fds, 1, 0 );

		if( rc < 0 )
		{
			fprintf( stderr, "Error: poll failed on server socket.\n" );
			return -1;
		}
		if( rc != 0 )
		{
			int foundsocketspot;
			int clientsocket = accept( serversocket, 0, 0 );
			setsockopt( clientsocket, SOL_SOCKET, SO_LINGER,(void*)(&lin), sizeof(lin) );  

			if( clientsocket > 0 )
			{
				int r = CNRFBNewConnection( );
				if( r >= MAX_SOCKETS )
				{
					fprintf( stderr, "Error: Got a connection ID too high from new connection.\n" );
					CNRFBCloseConnection( r );
					close( clientsocket );
				}
				else if( r >= 0 )
				{
					sockets[r] = clientsocket;
				}
				else
				{
					close( clientsocket );
				}
				ConnectEvent( r );
			}
		}

		for( i = 0; i < MAX_SOCKETS; i++ )
		{
			int sock = sockets[i];

			if( sock < 0 ) continue;

			fds[0].fd = sock;
			fds[0].events = POLLIN;

			rc = poll( fds, 1, 0 );

			if( rc < 0 )
			{
				CNRFBConnectionWasClosed( i );
				continue;
			}
			else if( rc == 0 )
				continue;

			rc = recv( sock, readbuffer, 65535, 0 );
			if( rc <= 0 )
			{
				CNRFBConnectionWasClosed( i );
				continue;
			}
			readbufferpos = 0;
			readbuffersize = rc;

			if( rc > 0 )
			{
				CNRFBGotData( i, rc );
			}
		}

		CNRFBTick( 0 );
		UpdateEvent( 0 );

		if( ( ( tv.tv_usec - lastticktime + 1000000 ) % 1000000 ) > 20000 ) //50 Hz.
		{
			CNRFBTick( 1 );
			UpdateEvent( 1 );
			lastticktime = tv.tv_usec;
		}

		usleep( 1000 );
	}
	return 0;
}


