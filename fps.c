#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "heap.h"
#include "cnrfb.h"
#include "mathfuncts.h"
#include "fps.h"

struct FPS FPSs[MAX_RFB_CONNS];
uint8_t  RankingHeap[MAX_RFB_CONNS];
uint8_t  RankingHeapPlace;
uint16_t PlayerPoints[MAX_RFB_CONNS]; //Separate so it can be used with the heap for the leaderboard.
const uint8_t PlayerColors[] = { 0x06, 0x24, 0x20, 0xF0, 0xC0, 0xC6, 0x5b, 0x01 };
const uint8_t keymap[] = { 81, 83, 82, 84, 'a', 'd', 'w', 's', 227, ' ' }; //227 = left control

struct Thing Things[MAX_THINGS];
uint8_t PylonHealths[MAX_PYLONS]; // <128 = dead, animating out to 0.  >= 128 = alive, with health

int last_boolet = MAX_THINGS;
uint8_t gtickcount = 0;
uint16_t gameover = 0;
uint8_t  pylons_remaining = 0;


static uint32_t GetRandomPos( uint8_t i )
{
	return ((i*1000004249)^0xA48C4B19)&0x1fff1fff;
}

void FreeFrameForClient( int conn )
{
	struct CNRFBSt * c = &CNRFBs[conn];
	struct FPS * f = &FPSs[conn];
	int i;

	if( !gameover )
	{
		//Actually only needs to be MAX_VISBLE_THINGS
		int16_t  Thetas[MAX_VISIBLE_THINGS];
		uint16_t Distances[MAX_VISIBLE_THINGS];
		uint8_t  Heap[MAX_VISIBLE_THINGS];
		uint8_t  ThingMapping[MAX_VISIBLE_THINGS];
		uint8_t  heaplen = 0;

		int qty = 0;

		for( i = 0; i < MAX_THINGS + MAX_PYLONS; i++ )
		{
			int16_t x, y;

			if( qty >= MAX_VISIBLE_THINGS ) break;

			if( i >= MAX_THINGS )
			{
				int pylon = i-MAX_THINGS;

				if( PylonHealths[pylon]<=0 ) continue;

				uint32_t p = GetRandomPos(i-MAX_THINGS);

				x = p>>16;
				y = p&0xffff;
			}
			else
			{
				if( Things[i].size <= 0 ) continue;
				x = Things[i].x;
				y = Things[i].y;
			}

			int16_t dtx = x - f->x;
			int16_t dty = y - f->y;

			uint32_t sq = (dtx * dtx) + (dty * dty);
			uint16_t dist = tasqrt( sq );

			int16_t theta = tatan2( dtx, dty );

			theta -= f->pointangle;

			if( theta < FOV+1 && theta > -FOV-1 && dist > 6 )
			{
				Thetas[qty] = (int16_t)theta;
				Distances[qty] = dist;
				ThingMapping[qty] = i;
				HeapAdd( Heap, &heaplen, qty, Distances );
				qty++;
			}
		}

		StartFrameDraw( conn, 2 );

		//If was just hit, turn sky red.
		StartRRE( qty+2, 0, 0,  RFB_WIDTH,  RFB_HEIGHT-16, (f->wasjusthit)?0x07:0x80 );
		f->wasjusthit = 0;
		DrawRectAtAsPartOfRRE( 0, RFB_HEIGHT/2 + (f->z>>3), RFB_WIDTH, RFB_HEIGHT/2-16-(f->z>>3), 0x20 );

		int oheap = heaplen;
		for( i = 0; i < oheap; i++ )
		{
			int j = HeapRemove( Heap, &heaplen, Distances );
			int t = ThingMapping[j];
			uint16_t dist = Distances[j];
			int16_t  theta = Thetas[j];
			int16_t left = theta;
			int16_t l = left;
			int16_t siz;
			uint8_t z;
			uint8_t color;

			if( t >= MAX_THINGS )
			{
				uint8_t health = PylonHealths[t-MAX_THINGS];
				siz = ((health>=128)?65536:(health<<9)) / dist;
				z = 0;
				color = (health==255)?255:((health>=128)?( (health-128)>>1 ):0); //Change color of pylons based on damage here?
			}
			else
			{
				siz = Things[t].size*50/dist;
				z = Things[t].z;
				color = (t<MAX_RFB_CONNS)?PlayerColors[t]:(PlayerColors[Things[t].player_associated]+0x49);
			}

			l = ((long)l * RFB_WIDTH) / FOV / 2;
			l += RFB_WIDTH / 2 - siz/2;
			int16_t r = l + siz;
			if( l < 0 ) l = 0;
			if( r > RFB_WIDTH ) r = RFB_WIDTH;
			if( siz > RFB_HEIGHT ) siz = RFB_HEIGHT;

			int16_t top = RFB_HEIGHT/2-siz/2;
			top += (f->z-z)*400/dist;
			if( top < 0 ) top = 0;
			if( top > RFB_HEIGHT-16 ) top = RFB_HEIGHT-16;
			if( top+siz > RFB_HEIGHT-16 ) siz = RFB_HEIGHT-16-top;
			if( siz < 0 ) siz = 0;
			DrawRectAtAsPartOfRRE( l, top, r-l, siz, color );		
		}
		//Sight dot.
		DrawRectAtAsPartOfRRE( RFB_WIDTH/2-2, RFB_HEIGHT/2-2, 4, 4, 0x0f );
	}
	else //If game over.
	{
		StartFrameDraw( conn, 1 );
	}

	//Happens every other frame.
	struct FPS * show = f;
	int which = (f->sendmode >> 1)%6;

	char stabuffer[80];

	int ypos = RFB_HEIGHT-16;

	int showplayer = conn;
	if( gameover )
	{
		showplayer = ((f->sendmode >> 1)>>3)&7;
		ypos = ( f->rank + 2) * 36;
		show = &FPSs[showplayer];
	}

	switch( which )
	{
	case 0:	sprintf( stabuffer, "%5d \003", show->health ); break;
	case 1:	sprintf( stabuffer, "%5d \007", PlayerPoints[showplayer] ); break;
	case 2:	sprintf( stabuffer, "%5d \002", show->kills ); break;
	case 3:	sprintf( stabuffer, "%5d \001", show->deaths ); break;
	case 4:	sprintf( stabuffer, "%5d \017", show->boolets ); break;
	case 5:	sprintf( stabuffer, "%5d \xfe", pylons_remaining ); break;
	}

	PrintText( stabuffer, 0xff, PlayerColors[showplayer], (gameover==0)?0:6, which*100+40, ypos );

	EndFrameDraw( conn );

	f->sendmode++;
}

//negative numbers = color override.
void EmitBoolet( int client, int size )
{
	struct FPS * f = &FPSs[client];
	if( f->boolets <= 0 ) return;
	f->boolets--;
	if( last_boolet < 0 ) last_boolet = BOOLET_START-1;
	last_boolet++;
	if( last_boolet >= MAX_THINGS ) last_boolet = BOOLET_START;
	struct Thing * t = &Things[last_boolet];
	t->x = f->x; t->y = f->y;
	t->size = size;
	t->z = f->z;
	t->dirx = tsin( (f->pointangle>>8) )>>3;
	t->diry = tcos( (f->pointangle>>8) )>>3;
	t->player_associated = client;
}


void GotKeyPress( int client, int key, int down )
{
	struct FPS * f = &FPSs[client];
	int i;

	if( gameover ) return;
	

	if( key == '/' ) key = 227; //xxx hack make space also be control.

	for( i = 0; i < MAX_KEYS; i++ )
	{
		if( key == keymap[i] )
		{
			if( down )
			{
				f->time_since_down[i] = 1;
			}
			else
			{
				f->time_since_down[i] = 3;
			}
			break;
		}
	}
//	printf( "%d %d %d\n", client, key, down );
}

void GotMouseEvent( int client, int mask, int x, int y )
{
//	printf( "%d  %d/%d,%d\n", client, mask, x, y );
}

void GotClipboard( int client, int length, unsigned char * st )
{
//	printf( "%d   %d:%s\n", client, length, st );
}


void ToolStart()
{
	int i;

	memset( PylonHealths, 0xff, sizeof( PylonHealths ) );
}

void DisconnectEvent( int conn )
{
	FPSs[conn].in_use = 0;
}


void RespawnPlayer( int c )
{
	uint8_t health;
	uint8_t boolets;

	struct FPS * f = &FPSs[c];
	uint32_t i = GetRandomPos(c+1000);

	f->x = i>>16;
	f->y = i&0xffff;

	f->health = 100;
	f->boolets = 150;
}

void ConnectEvent( int c )
{
	int i;
	struct FPS * f = &FPSs[c];
	memset( &FPSs[c], 0, sizeof( struct FPS ) );

	for( i = 0; i < MAX_KEYS; i++ )
	{
		f->time_since_down[i] = 255;
	}
	RespawnPlayer( c );
	FPSs[c].in_use = 1;
}

void HitPylon( int pylon, int boolet )
{
	struct Thing * b = &Things[boolet];
	struct Thing * p = &Things[pylon];

	int ph = PylonHealths[pylon];

	if( ph < 128 ) return;

	b->size = 0;
	ph-=8;
	PylonHealths[pylon] = ph;

	if( ph < 128 )
	{
		FPSs[b->player_associated].boolets += 100;
		PlayerPoints[b->player_associated] += 50;	
	}
	else
	{
		PlayerPoints[b->player_associated] ++;
	}

}

void HitPlayer( int player_hit, int boolet )
{
	struct Thing * t = &Things[boolet];
	struct FPS * fFrom = &FPSs[t->player_associated];
	struct FPS * fTo = &FPSs[player_hit];

	if( fFrom == fTo ) return; //Can't hit self.

	//Erase boolet.
	t->size = 0;

	fTo->health -= 5;
	if( fTo->health <= 0 )
	{
		fTo->deaths++;

		if( PlayerPoints[player_hit] > 10 )
			PlayerPoints[player_hit] = 0;
		else 
			PlayerPoints[player_hit] -= 100;

		PlayerPoints[t->player_associated] += 100;

		fFrom->kills++;
		RespawnPlayer( player_hit );
	}

	//Add knockback
	fTo->wasjusthit = 1;
	fTo->x += t->dirx*4;
	fTo->y += t->diry*4;
}

void UpdateEvent( int slowtick )
{
	int i;
	int j;

	if( !slowtick )
	{
		return;
	}

	if( !gameover )
	{
		pylons_remaining = 0;
		for( i = 0; i < MAX_PYLONS; i++ )
		{
			if( PylonHealths[i] ) pylons_remaining++;
		}
		if( pylons_remaining == 0 )
			gameover = 1000;
	}
	else
	{
		gameover--;
		//Restart game.
		if( gameover == 0 )
		{
			//Reset all pylons.
			ToolStart();
			for( i = 0; i < MAX_RFB_CONNS; i++ )
			{
				if( FPSs[i].in_use )
				{
					ConnectEvent( i );
				}
			}
		}
	}


	int rankcompplace = gtickcount % (MAX_RFB_CONNS*2);
	if( rankcompplace < MAX_RFB_CONNS )
	{
		HeapAdd( RankingHeap, &RankingHeapPlace, rankcompplace, PlayerPoints );
	}
	else
	{
		uint8_t hr = HeapRemove( RankingHeap, &RankingHeapPlace, PlayerPoints );
		FPSs[hr].rank = rankcompplace - 8;
	}

	gtickcount++;


	for( i = 0; i < MAX_RFB_CONNS; i++ )
	{
		struct Thing * t = &Things[i];
		struct FPS * f = &FPSs[i];
		int dxmotion = 0;
		int dymotion = 0;

		if( !f->in_use )  { t->size = 0; continue; }

//			printf( "%d %p\n", f->playercolor, &f->playercolor );
		if( f->time_since_down[KEY_TURN_LEFT] <= 3 )  f->pointangle-=250;
		if( f->time_since_down[KEY_TURN_RIGHT] <= 3 ) f->pointangle+=250;
		if( f->time_since_down[KEY_MOVE_FWD] <= SLACKTIMING )   dymotion+=SLACKTIMING-f->time_since_down[KEY_MOVE_FWD];
		if( f->time_since_down[KEY_MOVE_BKD] <= SLACKTIMING )   dymotion-=SLACKTIMING-f->time_since_down[KEY_MOVE_BKD];
		if( f->time_since_down[KEY_MOVE_LEFT] <= SLACKTIMING )  dxmotion-=SLACKTIMING-f->time_since_down[KEY_MOVE_LEFT];
		if( f->time_since_down[KEY_MOVE_RIGHT] <= SLACKTIMING ) dxmotion+=SLACKTIMING-f->time_since_down[KEY_MOVE_RIGHT];

		int pointangle = f->pointangle;
		pointangle = pointangle >> 8;
		int omotionx = tcos( pointangle ) * dxmotion + tsin( pointangle ) * dymotion;
		int omotiony = tcos( pointangle ) * dymotion - tsin( pointangle ) * dxmotion;

		f->x += omotionx / 150;
		f->y += omotiony / 150;

		for( j = 0; j < MAX_KEYS; j++ )
		{
			int tsd = f->time_since_down[j];
			if( tsd == 1 )
			{
				tsd = 2;
			}
			else if( tsd >= 3 && f->time_since_down[j] < 255 )
			{
				f->time_since_down[j]++;
			}
		}
		if( f->z <= 0 )
		{
			if( f->time_since_down[KEY_JUMP] < 2 )
			{
				f->speedz = 21;
			}
		}


		if( f->z || f->speedz );
		{
			f->speedz--;
			int newz = f->z;
			newz += f->speedz;
			if( newz < 0 )
			{
				f->z = 0;
				f->speedz = 0;
			}
			else
				f->z = newz;
		}

		t->x = f->x;
		t->z = f->z;
		t->y = f->y;
		t->dirx = 0; t->diry = 0;
		f->shakepos += 1500/(f->health+10);
		t->size = 1500 + tsin( f->shakepos>>2 );

		if( f->time_since_down[KEY_SHOOT] == 1 && f->shoot_cooldown == 0 )
		{
			EmitBoolet( i, 400 );
			f->shoot_cooldown = 6;
		}

		if( f->shoot_cooldown ) f->shoot_cooldown--;

	}


	for( i = BOOLET_START; i < MAX_THINGS; i++ )
	{
		struct Thing * t = &Things[i];

		if( t->size <= 0 ) continue;

		t->x += t->dirx;
		t->y += t->diry;

		t->size--;
		if( t->size < 200 ) { t->size = 0; continue; }

		//Check collision with pylons.
		int j;
		for( j = 0; j < MAX_PYLONS; j++ )
		{
			if( !PylonHealths[j] ) continue;
			uint32_t pp = GetRandomPos(j);
			int x = pp>>16;
			int y = pp&0xFFFF;
			int dx = x - t->x;
			int dy = y - t->y;
			int dz = t->z;
			if( dx * dx + dy * dy + dz * dz < MIN_DIST_HIT_SQ ) HitPylon( j, i );
		}

		for( j = 0; j < MAX_RFB_CONNS; j++ )
		{
			struct Thing * ot = &Things[j];
			if( !ot->size ) continue;
			int dx = ot->x - t->x;
			int dy = ot->y - t->y;
			int dz = ot->z - t->z;
			if( dx * dx + dy * dy + dz * dz < MIN_DIST_HIT_SQ ) HitPlayer( j, i );
		}
	}

	//Animate pylon destruction
	for( i = 0; i < MAX_PYLONS; i++ )
	{
		if( PylonHealths[i] < 128 && PylonHealths[i] )
			PylonHealths[i]--;
	}

}

