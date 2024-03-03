/*
  vim:ts=4
  vim:sw=4
*/
#include "colmap.h"
#include "belt.h"

#include "agon/vdp_vdu.h"
#include "agon/vdp_key.h"
#include <mos_api.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include "util.h"
#include "item.h"

#define CHUNK_SIZE 1024

int gMode = 8; 
int gScreenWidth = 320;
int gScreenHeight = 240;

int gMapWidth = 200;
int gMapHeight = 200;
int gTileSize = 16;

#define BMOFF_TILE16 0
#define NUM_BM_TERR16 13

#define BMOFF_FEAT16 BMOFF_TILE16 + NUM_BM_TERR16 
#define NUM_BM_FEAT16 5

#define BMOFF_BOB16 BMOFF_FEAT16 + NUM_BM_FEAT16
#define NUM_BM_BOB16 16

#define BMOFF_BELT16 BMOFF_BOB16 + NUM_BM_BOB16
#define NUM_BM_BELT16 4*4
#define NUM_BM_BBELT16 8*4

#define BMOFF_ITEM8 BMOFF_BELT16 + NUM_BM_BELT16 + NUM_BM_BBELT16
#define NUM_BM_ITEM8 1

#define TOTAL_BM BMOFF_ITEM8 + NUM_BM_ITEM8

#define SCROLL_RIGHT 0
#define SCROLL_LEFT 1
#define SCROLL_UP 2
#define SCROLL_DOWN 3

#define BOB_DOWN 0
#define BOB_UP 1
#define BOB_LEFT 2
#define BOB_RIGHT 3

#define COL(C) vdp_set_text_colour(C)
#define TAB(X,Y) vdp_cursor_tab(X,Y)

#define KEY_LEFT 0x9A
#define KEY_RIGHT 0x9C
#define KEY_UP 0x96
#define KEY_DOWN 0x98
#define KEY_w 0x2C
#define KEY_a 0x16
#define KEY_s 0x28
#define KEY_d 0x19
#define KEY_c 0x18
#define KEY_p 0x25
#define KEY_q 0x26
#define KEY_W 0x46
#define KEY_A 0x30
#define KEY_S 0x42
#define KEY_D 0x33
#define KEY_C 0x32
#define KEY_P 0x3F
#define KEY_Q 0x40
#define KEY_comma 0x5B
#define KEY_dot 0x59
#define KEY_lt 0x6E
#define KEY_gt 0x6F

// Position of top-left of screen in world coords (pixel)
int xpos=0, ypos=0;

uint8_t* tilemap;
int8_t* layer1;

FILE *open_file( const char *fname, const char *mode);
int close_file( FILE *fp );
int read_str(FILE *fp, char *str, char stop);
int load_bitmap_file( const char *fname, int width, int height, int bmap_id );

void key_event_handler( KEY_EVENT key_event );
void wait_clock( clock_t ticks );
double my_atof(char *str);

void load_images();
void game_loop();

int getWorldCoordX(int sx) { return (xpos + sx); }
int getWorldCoordY(int sy) { return (ypos + sy); }
int getTileX(int sx) { return (sx/gTileSize); }
int getTileY(int sy) { return (sy/gTileSize); }
int getTilePosInScreenX(int tx) { return ((tx * gTileSize) - xpos); }
int getTilePosInScreenY(int ty) { return ((ty * gTileSize) - ypos); }

void draw_tile(int tx, int ty, int tposx, int tposy);
void draw_screen();
void scroll_screen(int dir, int step, bool updatepos);
void draw_horizontal(int tx, int ty, int len);
void draw_vertical(int tx, int ty, int len);
void recentre();

int load_map(char *mapname);
void place_feature_overlay(uint8_t *data, int sx, int sy, int tile, int tx, int ty);

#define TILE_INFO_FILE "img/tileinfo.txt"
int readTileInfoFile(char *path, TileInfoFile *tif, int items);

int bobx=0;
int boby=0;
bool bShowBob=true;
void draw_bob(bool draw, int bx, int by, int px, int py);
void move_bob(int dir, int speed);
bool bobAtEdge();

int bob_facing = BOB_DOWN;
int bob_frame = 0;

clock_t bob_wait_ticks;
clock_t bob_anim_ticks;
clock_t move_wait_ticks;

bool belt_debug = false;

bool bPlace=false;
void start_place();
void stop_place();
void draw_place(bool draw);
int placex=0;
int placey=0;
int place_selected=-1;
int oldplacex=0;
int oldplacey=0;
int place_tx=0;
int place_ty=0;
int oldplace_tx=0;
int oldplace_ty=0;
clock_t place_select_wait_ticks;
clock_t place_wait_ticks;
int poss_belt[20];
int poss_belt_num=0;
void do_place();
void get_belt_neighbours(BELT_PLACE *bn, int tx, int ty);

clock_t layer_wait_ticks;
int layer_frame = 0;
void draw_layer();
void draw_horizontal_layer(int tx, int ty, int len);
void draw_box(int x1,int y1, int x2, int y2, int col);

BELT_LINK **belt_groups = NULL;
int num_belt_groups = 0;
int max_num_belt_groups = 10;
BELT_LINK *make_new_belt_group(int x, int y, int belt);
int find_belt_group(BELT_PLACE *bp);
void add_to_group_at_end(int bg, int beltID, int tx, int ty);
void add_to_group_at_front(int bg, int beltID, int tx, int ty);
void print_groups();
void join_groups(int in_bg, int out_bg);
void delete_belt_part(BELT_PLACE *bp);
BELT_LINK* find_belt(int tx, int ty);

void drop_near_bob();
void drop_item(int item, int tx, int ty);
clock_t drop_item_wait_ticks;

// global variable head. It points to the 
// first node of the Item list
ItemNodePtr itemlist = NULL;

clock_t item_move_wait_ticks;

void wait()
{
	char k=getchar();
	if (k=='q') exit(0);
}

int main(/*int argc, char *argv[]*/)
{
	vdp_vdu_init();
	if ( vdp_key_init() == -1 ) return 1;
	vdp_set_key_event_handler( key_event_handler );

	// custom map which is 256x256 tiles
	gMapWidth = 45;
	gMapHeight = 45;
	tilemap = (uint8_t *) malloc(sizeof(uint8_t) * gMapWidth * gMapHeight);
	if (tilemap == NULL)
	{
		printf("Out of memory\n");
		return -1;
	}

	if (load_map("maps/fmap.data") != 0)
	{
		printf("Failed to load map\n");
		goto my_exit;
	}

	layer1 = (int8_t *) malloc(sizeof(int8_t) * gMapWidth * gMapHeight);
	if (layer1 == NULL)
	{
		printf("Out of memory\n");
		return -1;
	}
	memset(layer1, (int8_t)-1, gMapWidth * gMapHeight);

	/* start screen centred */
	xpos = gTileSize*(gMapWidth - gScreenWidth/gTileSize)/2; 
	ypos = gTileSize*(gMapHeight - gScreenHeight/gTileSize)/2; 
	bobx = (xpos + gTileSize*(gScreenWidth/gTileSize)/2) & 0xFFFFF0;
	boby = (ypos + gTileSize*(gScreenHeight/gTileSize)/2) & 0xFFFFF0;

	// setup complete
	vdp_mode(gMode);
	vdp_logical_scr_dims(false);
	//vdu_set_graphics_viewport()

	load_images();

	game_loop();

my_exit:
	free(tilemap);
	vdp_mode(0);
	vdp_logical_scr_dims(true);
	vdp_cursor_enable( true );
	return 0;
}

void game_loop()
{
	int exit=0;
	draw_screen();
	draw_layer();
	bob_wait_ticks = clock();
	bob_anim_ticks = clock();
	layer_wait_ticks = clock();
	place_wait_ticks = clock();
	place_select_wait_ticks = clock();
	item_move_wait_ticks = clock()+60;
	drop_item_wait_ticks = clock();

	do {
		int dir=-1;
		int place_dir=-1;
		if (!bPlace) {
			if ( vdp_check_key_press( KEY_LEFT ) ) {dir=SCROLL_RIGHT; }
			if ( vdp_check_key_press( KEY_RIGHT ) ) {dir=SCROLL_LEFT; }
			if ( vdp_check_key_press( KEY_UP ) ) {dir=SCROLL_UP; }
			if ( vdp_check_key_press( KEY_DOWN ) ) {dir=SCROLL_DOWN; }
		} else {
			if ( vdp_check_key_press( KEY_LEFT ) ) {place_dir=SCROLL_RIGHT; }
			if ( vdp_check_key_press( KEY_RIGHT ) ) {place_dir=SCROLL_LEFT; }
			if ( vdp_check_key_press( KEY_UP ) ) {place_dir=SCROLL_UP; }
			if ( vdp_check_key_press( KEY_DOWN ) ) {place_dir=SCROLL_DOWN; }
		}
		if (layer_wait_ticks < clock()) 
		{
			draw_place(false);
			draw_layer();
			layer_wait_ticks = clock() + 7; // belt anim speed
			draw_bob(true,bobx,boby,xpos,ypos);
			draw_place(true);
		}
			
		if (dir>=0 && ( move_wait_ticks < clock() ) ) {
			// draw_bob(false,bx,by,px,py);
			draw_place(false);
			scroll_screen(dir,1,true);
			if (bobAtEdge()) {
				draw_bob(true,bobx,boby,xpos,ypos);
			}
			draw_place(true);
			//move_wait_ticks = clock() + 1;
		}
		if (place_dir>=0 && ( place_wait_ticks < clock() ) ) {
			draw_place(false);
			switch(place_dir) {
				case SCROLL_RIGHT: place_tx--; break;
				case SCROLL_LEFT: place_tx++; break;
				case SCROLL_UP: place_ty--; break;
				case SCROLL_DOWN: place_ty++; break;
				default: break;
			}
			draw_place(true);
			place_wait_ticks = clock() + 20;
		}
		if ( vdp_check_key_press( KEY_w ) || vdp_check_key_press( KEY_W ) ) { move_bob(BOB_UP, 1); }
		if ( vdp_check_key_press( KEY_a ) || vdp_check_key_press( KEY_A ) ) { move_bob(BOB_LEFT, 1); }
		if ( vdp_check_key_press( KEY_s ) || vdp_check_key_press( KEY_S ) ) { move_bob(BOB_DOWN, 1); }
		if ( vdp_check_key_press( KEY_d ) || vdp_check_key_press( KEY_D ) ) { move_bob(BOB_RIGHT, 1); }
		if ( vdp_check_key_press( KEY_c ) || vdp_check_key_press( KEY_C ) ) { recentre(); }
		if ( vdp_check_key_press( KEY_p ) || vdp_check_key_press( KEY_P ) ) { start_place(); }
		if ( vdp_check_key_press( KEY_q ) || vdp_check_key_press( KEY_Q ) ) { stop_place(); }
		if ( vdp_check_key_press( KEY_comma ) ) { 
			if (bPlace && place_select_wait_ticks < clock() ) {
				place_selected--;  
				if (place_selected < -1) place_selected += NUM_BELT_TYPES+1;
				//place_selected = ((place_selected + 1) % (NUM_BELT_TYPES+1)) -1;
				place_select_wait_ticks = clock() + 20;
			}
		}
		if ( vdp_check_key_press( KEY_dot ) ) { 
			if (bPlace && place_select_wait_ticks < clock() ) {
				place_selected++;  
				place_selected = ((place_selected + 1) % (NUM_BELT_TYPES+1)) -1;
				place_select_wait_ticks = clock() + 20;
			}
		}
		if ( vdp_check_key_press( 0x8F ) ) // ENTER
		{
			do_place();
		}
		if ( vdp_check_key_press( 0x2F ) ) // z - drop an item
		{
			if (drop_item_wait_ticks < clock()) 
			{
				drop_near_bob();
				drop_item_wait_ticks = clock() + 10;
			}
		}
		if ( vdp_check_key_press( 0x4A ) )  // ' - toggle belt_debug
		{
			if (drop_item_wait_ticks < clock()) 
			{
				belt_debug = !belt_debug;
				draw_screen();
				drop_item_wait_ticks = clock() + 20;
			}
		}

		if ( vdp_check_key_press( 0x2D ) ) exit=1; // x

		if ( item_move_wait_ticks <  clock() ) {
			item_move_wait_ticks = clock()+100;
		}

		vdp_update_key_state();
	} while (exit==0);

}


void load_images() 
{
	char fname[40];
	for (int fn=1; fn<=NUM_BM_TERR16; fn++)
	{
		sprintf(fname, "img/terr16/tr%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_TILE16 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_FEAT16; fn++)
	{
		sprintf(fname, "img/tf16/tf%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_FEAT16 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_BOB16; fn++)
	{
		sprintf(fname, "img/b16/bob%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BOB16 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_BELT16; fn++)
	{
		sprintf(fname, "img/belt16/belt%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BELT16 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_BBELT16; fn++)
	{
		sprintf(fname, "img/belt16/bbelt%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BELT16 + NUM_BM_BELT16 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_ITEM8; fn++)
	{
		sprintf(fname, "img/ti8/ti%02d.rgb2",fn);
		load_bitmap_file(fname, 8, 8, BMOFF_ITEM8 + fn-1);
	}
	
	TAB(0,0);
	printf("TILES start %d count %d\n",BMOFF_TILE16,NUM_BM_TERR16);
	printf("FEATS start %d count %d\n",BMOFF_FEAT16,NUM_BM_FEAT16);
	printf("BOB   start %d count %d\n",BMOFF_BOB16,NUM_BM_BOB16);
	printf("BELTS start %d count %d + %d\n",BMOFF_BELT16,NUM_BM_BELT16,NUM_BM_BBELT16);
	printf("ITEMS start %d count %d\n",BMOFF_ITEM8,NUM_BM_ITEM8);
	printf("Total %d\n",TOTAL_BM);
	wait();
}

void draw_tile(int tx, int ty, int tposx, int tposy)
{
	uint8_t tile = tilemap[ty*gMapWidth + tx] & 0x0F;
	uint8_t overlay = (tilemap[ty*gMapWidth + tx] & 0xF0) >> 4;
	vdp_select_bitmap( tile + BMOFF_TILE16);
	vdp_draw_bitmap( tposx, tposy );
	if (overlay > 0)
	{
		vdp_select_bitmap( overlay - 1 + BMOFF_FEAT16);
		vdp_draw_bitmap( tposx, tposy );
	}
}

// draw full screen at World position in xpos/ypos
void draw_screen()
{
	int tx=getTileX(xpos);
	int ty=getTileY(ypos);

	for (int i=0; i < (1+gScreenHeight/gTileSize); i++) 
	{
		draw_horizontal(tx, ty+i, 1+(gScreenWidth/gTileSize));
	}

	draw_bob(true,bobx,boby,xpos,ypos);
}

void draw_horizontal(int tx, int ty, int len)
{
	int px=getTilePosInScreenX(tx);
	int py=getTilePosInScreenY(ty);

	for (int i=0; i<len; i++)
	{
		draw_tile(tx + i, ty, px + i*gTileSize, py); 
		vdp_update_key_state();
	}
	
}
void draw_vertical(int tx, int ty, int len)
{
	int px = getTilePosInScreenX(tx);
	int py = getTilePosInScreenY(ty);

	for (int i=0; i<len; i++)
	{
		draw_tile(tx, ty + i, px, py + i*gTileSize);
		vdp_update_key_state();
	}
}

/* 0=right, 1=left, 2=up, 3=down */
void scroll_screen(int dir, int step, bool updatepos)
{
	switch (dir) {
		case SCROLL_RIGHT: // scroll screen to right, view moves left
			if (xpos > step)
			{
				xpos -= step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx,ty+len)
				int tx=getTileX(xpos);
				int ty=getTileY(ypos);
				draw_vertical(tx,ty, 1+(gScreenHeight/gTileSize));
				if (!updatepos) xpos += step;
			}
			break;
		case SCROLL_LEFT: // scroll screen to left, view moves right
			if ((xpos + gScreenWidth + step) < (gMapWidth * gTileSize))
			{
				xpos += step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx,ty+len)
				int tx=getTileX(xpos + gScreenWidth -1);
				int ty=getTileY(ypos);
				draw_vertical(tx,ty, 1+(gScreenHeight/gTileSize));
				if (!updatepos) xpos -= step;
			}
			break;
		case SCROLL_UP:
			if (ypos > step)
			{
				ypos -= step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx+len,ty)
				int tx=getTileX(xpos);
				int ty=getTileY(ypos);
				draw_horizontal(tx,ty, 1+(gScreenWidth/gTileSize));
				if (!updatepos) ypos += step;
			}
			break;
		case SCROLL_DOWN:
			if ((ypos + gScreenHeight + step) < (gMapHeight * gTileSize))
			{
				ypos += step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx+len,ty)
				int tx=getTileX(xpos);
				int ty=getTileY(ypos + gScreenHeight -1);
				draw_horizontal(tx,ty, 1+(gScreenWidth/gTileSize));
				if (!updatepos) ypos -= step;
			}
			break;
		default:
			break;
	}
}

uint8_t oval1_block6x6[6*6] = {
	0,0,1,1,0,0,
	0,1,1,1,1,0,
	1,1,1,1,1,0,
	1,1,1,1,1,1,
	0,1,1,1,1,0,
	0,0,1,0,0,0};

uint8_t oval2_block6x6[6*6] = {
	0,1,1,1,0,0,
	0,1,0,1,1,0,
	1,1,1,1,0,0,
	0,1,0,1,1,1,
	0,1,1,1,1,0,
	0,0,1,1,0,0};

uint8_t rnd1_block5x5[5*5] = {
	0,1,0,1,1,
	0,0,1,0,0,
	1,0,0,0,0,
	0,1,0,1,0,
	0,1,0,1,0,
};
uint8_t rnd2_block5x5[5*5] = {
	0,0,0,1,0,
	0,1,1,1,0,
	1,1,0,0,1,
	0,0,1,0,0,
	1,1,0,0,1,
};

uint8_t rnd1_block4x4[4*4] = {
	0,1,1,0,
	1,0,1,0,
	0,0,0,1,
	1,1,0,0,
};


int load_map(char *mapname)
{
	uint8_t ret = mos_load( mapname, (uint24_t) tilemap,  gMapWidth * gMapHeight );
	place_feature_overlay(oval1_block6x6,6,6,0,30,10);
	place_feature_overlay(oval2_block6x6,6,6,1,10,23);
	place_feature_overlay(oval1_block6x6,6,6,2,22,29);

	place_feature_overlay(rnd1_block5x5,5,5,4,5,5);
	place_feature_overlay(rnd1_block5x5,5,5,4,15,8);
	place_feature_overlay(rnd2_block5x5,5,5,4,20,30);
	place_feature_overlay(rnd1_block5x5,5,5,4,7,26);
	place_feature_overlay(rnd2_block5x5,5,5,4,32,22);

	place_feature_overlay(rnd1_block4x4,4,4,4,28,19);
	place_feature_overlay(rnd1_block4x4,4,4,4,12,3);
	return ret;
}

void place_feature_overlay(uint8_t *data, int sx, int sy, int tile, int tx, int ty)
{
	for (int y=0; y<sy; y++) 
	{
		for (int x=0; x<sx; x++) 
		{
			if (data[x+(y*sx)] > 0)
			{
				tilemap[(tx+x) + (ty+y)*gMapWidth] &= 0x0F;
				tilemap[(tx+x) + (ty+y)*gMapWidth] |= (tile+1)<<4;
			}
		}
	}
}


void draw_bob(bool draw, int bx, int by, int px, int py)
{
	if (!bShowBob) return;
	if (draw) {
		vdp_select_bitmap( BMOFF_BOB16 + bob_facing*4 + bob_frame);
		vdp_draw_bitmap( bx-px, by-py );
	} else {
		int tx=getTileX(bx);
		int ty=getTileY(by);
		int tposx = tx*gTileSize - px;
		int tposy = ty*gTileSize - py;

		// have to re-draw the 4 tiles that are under the sprite
		draw_tile(tx, ty, tposx, tposy);
		draw_tile(tx+1, ty, tposx+gTileSize, tposy);
		draw_tile(tx, ty+1, tposx, tposy+gTileSize);
		draw_tile(tx+1, ty+1, tposx+gTileSize, tposy+gTileSize);
	}
}

bool check_tile(int px, int py)
{
	int tx=getTileX(px);
	int ty=getTileY(py);

	return tilemap[tx+ty*gMapWidth]<16;
}

void move_bob(int dir, int speed)
{
	int newx=bobx, newy=boby;

	if ( bob_wait_ticks > clock() ) return;

	//if (bob_facing != dir*4) bob_frame=0;
	bob_facing = dir;
	speed *= (gTileSize/8);

	switch (dir) {
		case BOB_LEFT:
			if (bobx > speed 
					&& check_tile(bobx-speed,boby)
					&& check_tile(bobx-speed,boby+gTileSize-1)
					) {
				newx -= speed;
			}
			break;
		case BOB_RIGHT:
			if (bobx < gMapWidth*gTileSize - speed 
					&& check_tile(bobx+speed+gTileSize-1,boby)
					&& check_tile(bobx+speed+gTileSize-1, boby+gTileSize-1)
						) {
				newx += speed;
			}
			break;
		case BOB_UP:
			if (boby > speed 
					&& check_tile(bobx,boby-speed)
					&& check_tile(bobx+gTileSize-1,boby-speed)
					) {
				newy -= speed;
			}
			break;
		case BOB_DOWN:
			if (boby < gMapHeight*gTileSize - speed 
				&& check_tile(bobx,boby+speed+gTileSize-1)
				&& check_tile(bobx+gTileSize-1,boby+speed+gTileSize-1)
				) {
				newy += speed;
			}
			break;
		default: break;
	}
	draw_bob(false,bobx,boby,xpos,ypos);
	bobx=newx;
	boby=newy;
	draw_bob(true,newx,newy,xpos,ypos);

	bob_wait_ticks = clock()+5;
	
	if (bob_anim_ticks > clock() ) return;
	bob_frame=(bob_frame+1)%4; 
	bob_anim_ticks = clock()+10;

}

void recentre()
{
	xpos = bobx - gScreenWidth/2;
	ypos = boby - gScreenHeight/2;
	draw_screen();
	move_wait_ticks = clock() + 1;
}

bool bobAtEdge()
{
	int bx = bobx-xpos;
	int by = boby-ypos;

	if (bx >= 0 && bx <= gTileSize*2 ) return true;
	if (by >= 0 && by <= gTileSize*2 ) return true;
	if (bx >= gScreenWidth - gTileSize*2 && bx <= gScreenWidth ) return true;
	if (by >= gScreenHeight - gTileSize*2 && by <= gScreenWidth ) return true;

	return false;
}

void start_place()
{
	if (bPlace) return;
	bPlace=true;
	place_tx = getTileX(bobx+gTileSize/2);
	place_ty = getTileY(boby+gTileSize/2);
	if (bob_facing == BOB_RIGHT) {
		place_tx++;
	} else if (bob_facing == BOB_LEFT) {
		place_tx--;
	} else if (bob_facing == BOB_UP) {
		place_ty--;
	} else if (bob_facing == BOB_DOWN) {
		place_ty++;
	}
	 
	draw_place(true);
}
void stop_place()
{
	if (!bPlace) return;
	draw_place(false);
	bPlace=false;
}

void draw_place(bool draw) 
{
	if (!bPlace) return;
	// undraw
	if (!draw) {
		draw_tile(oldplace_tx, oldplace_ty, oldplacex, oldplacey);
		return;
	}

	placex=getTilePosInScreenX(place_tx);
	placey=getTilePosInScreenY(place_ty);

	if (place_selected>=0)
	{
		vdp_select_bitmap( BMOFF_BELT16 + place_selected*4 );
		vdp_draw_bitmap( placex, placey );
	}

	draw_box(placex, placey, placex+gTileSize-1, placey+gTileSize-1, 15);
	oldplacex=placex;
	oldplacey=placey;
	oldplace_tx = place_tx;
	oldplace_ty = place_ty;
}

void draw_layer()
{
	int tx=getTileX(xpos);
	int ty=getTileY(ypos);

	for (int i=0; i < (1+gScreenHeight/gTileSize); i++) 
	{
		draw_horizontal_layer(tx, ty+i, 1+(gScreenWidth/gTileSize));
	}

	TAB(0,4); printItemList();

	vdp_update_key_state();
	layer_frame = (layer_frame +1) % 4;
}

void draw_horizontal_layer(int tx, int ty, int len)
{
	int px=getTilePosInScreenX(tx);
	int py=getTilePosInScreenY(ty);

	for (int i=0; i<len; i++)
	{
		if ( layer1[ty*gMapWidth + tx+i] >= 0 )
		{
			vdp_select_bitmap( layer1[ty*gMapWidth + tx+i]*4 + BMOFF_BELT16 + layer_frame);
			vdp_draw_bitmap( px + i*gTileSize, py );
		}
	}
}

void draw_box(int x1,int y1, int x2, int y2, int col)
{
	vdp_gcol( 0, col );
	vdp_move_to( x1, y1 );
	vdp_line_to( x1, y2 );
	vdp_line_to( x2, y2 );
	vdp_line_to( x2, y1 );
	vdp_line_to( x1, y1 );
}

void get_belt_neighbours(BELT_PLACE *bn, int tx, int ty)
{
	bn[0].beltID = layer1[tx   + (ty-1)*gMapWidth];
	bn[0].locX = tx;
	bn[0].locY = ty-1;

	bn[1].beltID = layer1[tx+1 + (ty)*gMapWidth];
	bn[1].locX = tx+1;
	bn[1].locY = ty;

	bn[2].beltID = layer1[tx   + (ty+1)*gMapWidth];
	bn[2].locX = tx;
	bn[2].locY = ty+1;

	bn[3].beltID = layer1[tx-1 + (ty)*gMapWidth];
	bn[3].locX = tx-1;
	bn[3].locY = ty;

}

#define DIR_OPP(D) ((D+2)%4)

void do_place()
{
	if (!bPlace || place_selected<0) return;
		       
	int belt_at_place = layer1[place_ty*gMapWidth + place_tx];;

	BELT_PLACE bn[4];
	get_belt_neighbours(bn, place_tx, place_ty);

	if (belt_debug) { 
		draw_screen(); // to clear debug messages
		TAB(0,0); printf("BN %d %d %d %d \n",bn[0].beltID,bn[1].beltID,bn[2].beltID,bn[3].beltID);
	}

	if (belt_at_place>=0)
	{
		// remove this belt
		BELT_PLACE bp;
		bp.beltID = belt_at_place;
		bp.locX = place_tx;
		bp.locY = place_ty;
		delete_belt_part(&bp);
	}

	int in_connection = -1;
	int out_connection = -1;
	for (int i=0; i<4; i++)
	{
		// check neighbour exit coming into selected
		if (bn[i].beltID >=0 && belts[bn[i].beltID].to == DIR_OPP(i)) {
			// check piece fits
			if (belts[place_selected].from == i) {
				in_connection = i;
			}
		}
		// check neighbour entry coming from selected
		if (bn[i].beltID >=0 && belts[bn[i].beltID].from == DIR_OPP(i)) {
			// check piece fits
			if (belts[place_selected].to == i) {
				out_connection = i;
			}
		}
	}

	if (in_connection == -1 && out_connection == -1)
	{
		make_new_belt_group(place_tx, place_ty, place_selected);
	} else {
		int in_bg = -1;
		int out_bg = -1;
		if (in_connection>=0) {
			in_bg = find_belt_group(&bn[in_connection]);
			if (in_bg>=0) {
				if (belt_debug) { TAB(25,0);printf("Add to end\n");}
				add_to_group_at_end(in_bg,place_selected, place_tx, place_ty);
			}
			if (out_connection>=0) {
				out_bg = find_belt_group(&bn[out_connection]);
				if (out_bg>=0) {
					join_groups(in_bg, out_bg);
				}
			}
				
		} else {
			if (out_connection>=0) {
				out_bg = find_belt_group(&bn[out_connection]);
				if (out_bg>=0) {
					if (belt_debug) { TAB(25,0);printf("Add to front\n"); }
					add_to_group_at_front(out_bg, place_selected, place_tx, place_ty);
				}
			}
		}

	}

	layer1[gMapWidth * place_ty + place_tx] = place_selected;
	
	print_groups();
	stop_place();
}

BELT_LINK *make_new_belt_group(int x, int y, int belt)
{
	if (belt_groups==NULL)
	{
		belt_groups = (BELT_LINK**) malloc(sizeof(BELT_LINK*) * max_num_belt_groups);
		if (belt_groups == NULL)
		{
			printf("out of memory\n");
			return NULL;
		}
		for (int i=0;i<max_num_belt_groups;i++)
		{
			belt_groups[i]=NULL;
		}
	}
	if (num_belt_groups>=max_num_belt_groups)
	{
		max_num_belt_groups += 10;
		belt_groups = (BELT_LINK**) realloc(belt_groups, max_num_belt_groups * sizeof(BELT_LINK*));
		if (belt_groups == NULL)
		{
			printf("out of memory on realloc\n");
			return NULL;
		}
		for (int i=num_belt_groups;i<max_num_belt_groups;i++)
		{
			belt_groups[i]=NULL;
		}
	}

	BELT_LINK *bl = malloc(sizeof(BELT_LINK));
	if (bl == NULL)
	{
		printf("out of memory\n");
		return NULL;
	}
	bl->beltID = belt;
	bl->locX = x;
	bl->locY = y;
	bl->nextLink = NULL;
	bl->prevLink = NULL;
	for (int i=0;i<10;i++) { bl->contents[i]=0; }
	belt_groups[num_belt_groups] = bl;

	if (belt_debug) { TAB(0,1); printf("New BG %d: %d %d,%d\n",num_belt_groups,bl->beltID,bl->locX,bl->locY); }

	num_belt_groups++;
	return bl;
}

int find_belt_group(BELT_PLACE *bp)
{
	bool found=false;
	BELT_LINK *bl = NULL;	
	int bg;

	if (belt_debug) {
		TAB(0,2); 
		//printf("find %d %d,%d\n",bp->beltID, bp->locX, bp->locY);
	}

	for (bg=0;bg<num_belt_groups;bg++)
	{
		bl = belt_groups[bg];
		while (bl != NULL)
		{
			if (bl->beltID == bp->beltID &&
			    bl->locX == bp->locX &&
			    bl->locY == bp->locY)
			{
				found=true;
				break;
			}
			bl = (BELT_LINK*)bl->nextLink;
		}
		if (found) break;
	}

	if (belt_debug)
	{
		if (found) {
			printf("found in group %d\n", bg);
		} else {
			printf("not found\n");
		}
	}

	return bg;
}

void add_to_group_at_end(int bg, int beltID, int tx, int ty)
{
	BELT_LINK * newbl = malloc(sizeof(BELT_LINK));
	if (newbl==NULL)
	{
		printf("out of mem\n");
		return;
	}
	newbl->beltID = beltID;
	newbl->locX = tx;
	newbl->locY = ty;
	newbl->nextLink = NULL;
	newbl->prevLink = NULL;
	for (int i=0;i<10;i++) { newbl->contents[i]=0; }

	BELT_LINK *bl = belt_groups[bg];
	while (bl->nextLink != NULL)
	{
		bl = (BELT_LINK*)bl->nextLink;
	}
	
	newbl->prevLink = (struct BELT_LINK*)bl;

	bl->nextLink = (struct BELT_LINK*)newbl;
}
void add_to_group_at_front(int bg, int beltID, int tx, int ty)
{
	BELT_LINK * newbl = malloc(sizeof(BELT_LINK));
	if (newbl==NULL)
	{
		printf("out of mem\n");
		return;
	}
	newbl->beltID = beltID;
	newbl->locX = tx;
	newbl->locY = ty;
	newbl->nextLink = NULL;
	newbl->prevLink = NULL;
	for (int i=0;i<10;i++) { newbl->contents[i]=0; }


	BELT_LINK *bl = belt_groups[bg];
	bl->prevLink = (struct BELT_LINK*)newbl;
	newbl->nextLink = (struct BELT_LINK*)bl;
	belt_groups[bg] = newbl;
}

void print_groups()
{
	if (!belt_debug) return;
	int line=2;
	TAB(25,line++); printf("groups %d\n",num_belt_groups);
	for (int bg=0;bg<num_belt_groups;bg++)
	{
		BELT_LINK *bl = belt_groups[bg];
		TAB(25,line++);
		printf("%d: ",bg);
		while(bl!=NULL)
		{
			printf("%d ",bl->beltID);
			bl = (BELT_LINK*)bl->nextLink;
		}
	}
}

void join_groups(int in_bg, int out_bg) 
{
	// add the out_bg to the end of in_bg
	
	BELT_LINK *out_bl = belt_groups[out_bg];

	BELT_LINK *bl = belt_groups[in_bg];
	while (bl->nextLink != NULL)
	{
		bl = (BELT_LINK*) bl->nextLink;
	}
	bl->nextLink = (struct BELT_LINK*) out_bl;
	out_bl->prevLink = (struct BELT_LINK*) bl;
	belt_groups[out_bg] = NULL;
}

void delete_belt_part(BELT_PLACE *bp)
{
	bool found=false;
	BELT_LINK *bl = NULL;	
	int bg;
	bool head=true;

	if (belt_debug) { TAB(0,1);printf("delete\n"); }
	for (bg=0;bg<num_belt_groups;bg++)
	{
		bl = belt_groups[bg];
		head=true;
		while (bl != NULL)
		{
			if (bl->beltID == bp->beltID &&
			    bl->locX == bp->locX &&
			    bl->locY == bp->locY)
			{
				found=true;
				break;
			}
			bl = (BELT_LINK*)bl->nextLink;
			head=false;
		}
		if (found) break;
	}

	if (found) {
		if (belt_debug) printf("in group %d%s\n", bg, head?" at head":"");

		if (head) {
			belt_groups[bg] = (BELT_LINK*)(bl->nextLink);
			belt_groups[bg]->prevLink = NULL;
		} else {
			// tail
			if (bl->prevLink && !bl->nextLink) {
				((BELT_LINK*)(bl->prevLink))->nextLink = bl->nextLink;
			} else if (bl->prevLink && bl->nextLink) {
				// middle
				((BELT_LINK*)(bl->prevLink))->nextLink = NULL;
				BELT_LINK *nl = (BELT_LINK*)(bl->nextLink);
				BELT_LINK *nnl = make_new_belt_group(nl->locX, nl->locY, nl->beltID);
				nnl->nextLink = nl->nextLink;
				free(nl);
			}
		}

		free(bl);
		bl=NULL;
	}

}

BELT_LINK* find_belt(int tx, int ty)
{
	bool found=false;
	int bg;
	BELT_LINK *bl = NULL;	

	BELT_PLACE bp;
	int l1 = layer1[tx + ty*gMapWidth];

	if ( l1 >= 0 ) {
		bp.beltID = l1;
		bp.locX = tx;
		bp.locY = ty;
	}
	if (belt_debug) {
		TAB(0,5); 
		//printf("find %d %d,%d\n",bp->beltID, bp->locX, bp->locY);
	}

	for (bg=0;bg<num_belt_groups;bg++)
	{
		bl = belt_groups[bg];
		while (bl != NULL)
		{
			if (bl->beltID == bp.beltID &&
			    bl->locX == bp.locX &&
			    bl->locY == bp.locY)
			{
				found=true;
				break;
			}
			bl = (BELT_LINK*)bl->nextLink;
		}
		if (found) break;
	}

	if (belt_debug)
	{
		if (found) {
			printf("found %d ID:%d %d,%d\n", bg, bl->beltID, bl->locX, bl->locY);
		} else {
			printf("not found\n");
		}
	}

	if (found) return bl;
	return NULL;
}


void drop_near_bob()
{
	int drop_tx = getTileX(bobx+gTileSize/2);
	int drop_ty = getTileY(boby+gTileSize/2);
	if (bob_facing == BOB_RIGHT) {
		drop_tx++;
	} else if (bob_facing == BOB_LEFT) {
		drop_tx--;
	} else if (bob_facing == BOB_UP) {
		drop_ty--;
	} else if (bob_facing == BOB_DOWN) {
		drop_ty++;
	}
	 
	// only one item type currently
	insertAtFrontItemList(1, drop_tx, drop_ty);
}


