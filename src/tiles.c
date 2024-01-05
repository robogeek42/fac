#include "tiles.h"

TILE_TYPE mytiles[NUM_TILE_TYPES] = {
	{ 0, "ts01.rgb2", 0,{GRS|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4)},4*20},
	{ 1, "ts02.rgb2", 1,{GRS|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4),GRS|(GRS<<4)},2},
	{ 2, "ts03.rgb2", 2,{SEA|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4),SEA|(SEA<<4)},2},
	{ 3, "ts04.rgb2", 3,{SEA|(SEA<<4),SEA|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4)},2},
	{ 4, "ts05.rgb2", 4,{GRS|(GRS<<4),GRS|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4)},2},
	{ 5, "ts06.rgb2", 5,{SEA|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4),SEA|(GRS<<4)},1},
	{ 6, "ts07.rgb2", 6,{GRS|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4)},1},
	{ 7, "ts08.rgb2", 7,{SEA|(GRS<<4),GRS|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4)},1},
	{ 8, "ts09.rgb2", 8,{SEA|(SEA<<4),SEA|(GRS<<4),SEA|(GRS<<4),SEA|(SEA<<4)},1},
	{ 9, "ts10.rgb2", 9,{SEA|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4)},0},
	{10, "ts11.rgb2",11,{GRS|(SEA<<4),SEA|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4)},3},
	{11, "ts12.rgb2",12,{GRS|(GRS<<4),GRS|(SEA<<4),GRS|(SEA<<4),GRS|(GRS<<4)},3},
	{12, "ts13.rgb2",13,{GRS|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4),GRS|(SEA<<4)},3},
	{13, "ts14.rgb2",14,{SEA|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4)},3},
	{14, "ts15.rgb2",15,{GRS|(SEA<<4),SEA|(GRS<<4),SEA|(GRS<<4),GRS|(SEA<<4)},2},
	{15, "ts16.rgb2",16,{SEA|(GRS<<4),GRS|(SEA<<4),GRS|(SEA<<4),SEA|(GRS<<4)},2},
};
void display_tile(TILE* screen[HEIGHT_TILES][WIDTH_TILES], int x, int y)
{
	vdp_adv_select_bitmap(BM_OFFSET + screen[y][x]->id);
	vdp_draw_bitmap(x*TILE_WIDTH, y*TILE_HEIGHT);
}

