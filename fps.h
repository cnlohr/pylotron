#ifndef _FPS_H
#define _FPS_H

#include "cnrfb.h"

#define MAX_PYLONS 55

#define MAX_VISIBLE_THINGS 100 //Can't send too many bytes per packet.
#define BOOLETS 64
#define BOOLET_START (MAX_RFB_CONNS)
#define MAX_THINGS (MAX_RFB_CONNS+BOOLETS)

#define MIN_DIST_HIT_SQ 12000

#define FOV (30*256)
#define SLACKTIMING 10


//A totally incomplete FPS demo.

#define KEY_TURN_LEFT 0
#define KEY_TURN_RIGHT 1
#define KEY_TURN_FWD 2
#define KEY_TURN_BKD 3

#define KEY_MOVE_LEFT 4
#define KEY_MOVE_RIGHT 5
#define KEY_MOVE_FWD 6
#define KEY_MOVE_BKD 7

#define KEY_SHOOT 8
#define KEY_JUMP  9
#define MAX_KEYS 10

//20 bytes per player * 8 players = 160 bytes
struct FPS
{
	int wasjusthit:1;
	int in_use:1;
	int shoot_cooldown:3;
	uint8_t rank:3;
	uint8_t sendmode;
	int16_t x, y;
	int16_t pointangle; //-32768,32768, 0 is in front.
	uint8_t time_since_down[MAX_KEYS];
	int8_t health;
	uint16_t boolets;
	uint8_t kills;
	uint8_t deaths;
	uint16_t shakepos;
	uint8_t z;
	int8_t speedz;
};

extern const uint8_t PlayerColors[MAX_RFB_CONNS];

extern struct FPS FPSs[MAX_RFB_CONNS];

extern uint8_t  RankingHeap[MAX_RFB_CONNS];
extern uint8_t  RankingHeapPlace;
extern uint16_t PlayerPoints[MAX_RFB_CONNS]; //Separate so it can be used with the heap for the leaderboard.

extern const uint8_t PlayerColors[];
extern const uint8_t keymap[];

struct Thing //10 bytes
{
	uint8_t player_associated; 
	int16_t x, y;
	int8_t  dirx, diry;
	int16_t size; //if 0, inactive.
	uint8_t z;
};

extern struct Thing Things[MAX_THINGS];

extern uint8_t PylonHealths[MAX_PYLONS]; // <128 = dead, animating out to 0.  >= 128 = alive, with health

extern int last_boolet;
extern uint8_t gtickcount;
extern uint16_t gameover;
extern uint8_t  pylons_remaining;


#endif

