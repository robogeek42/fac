#ifndef _TILES_H
#define _TILES_H

#include <stdio.h>
#include <stdlib.h>
#include <mos_api.h>
#include <string.h>
#include "agon/vdp_vdu.h"
#include "agon/vdp_key.h"

#define DEBUG 0

#define SCR_WIDTH 320
#define SCR_HEIGHT 240
#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define WIDTH_TILES (SCR_WIDTH / TILE_WIDTH)
#define HEIGHT_TILES (SCR_HEIGHT / TILE_HEIGHT)
#define NUM_TILES   (WIDTH_TILES * HEIGHT_TILES)
#define WORLD_TWIDTH (WIDTH_TILES * 4)
#define WORLD_THEIGHT (HEIGHT_TILES * 4)

#define BM_OFFSET 0xFA00

#define GRS 0
#define SEA 1

typedef struct {
	int tile;
	struct TILE_LIST_NODE *next_tile;
} TILE_LIST_NODE; 

typedef struct {
	uint8_t id;
	char name[12];
	int bufid;
	int edge[4]; // Edge type. N=0, E=1, S=2, W=3
	int land_weight; // amount of land in tile 0-4
} TILE_TYPE;

#define NUM_TILE_TYPES 16
extern TILE_TYPE mytiles[NUM_TILE_TYPES];

#define MAX_POSSIBLES 6
typedef struct {
	uint8_t id;
	int entropy;
	uint8_t possibles[MAX_POSSIBLES]; // list of possibles. If entropy is MAX, this is empty
	int posx;
	int posy;
} TILE;


void display_tile(TILE* screen[HEIGHT_TILES][WIDTH_TILES], int x, int y);



#endif
