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
#include "inventory.h"

#define IMPLEMENTATION
#include "images.h"

int gMode = 8; 
int gScreenWidth = 320;
int gScreenHeight = 240;

int gTileSize = 16;

#define SCROLL_RIGHT 0
#define SCROLL_LEFT 1
#define SCROLL_UP 2
#define SCROLL_DOWN 3

#define BOB_DOWN 0
#define BOB_UP 1
#define BOB_LEFT 2
#define BOB_RIGHT 3

bool debug = false;

//------------------------------------------------------------
// status related variables
// need to be saved / loaded

// Position of top-left of screen in world coords (pixel)
int xpos=0, ypos=0;
// Position of character in world coords (pixel)
int bobx=0, boby=0;
// cursor position position in world coords
int cursorx=0, cursory=0;
// cursor position in tile coords
int cursor_tx=0, cursor_ty=0;

// maps
int gMapWidth = 200;
int gMapHeight = 200;
uint8_t* tilemap;
int8_t* layer_belts;

// first node (head) of the Item list
ItemNodePtr itemlist = NULL;

// inventory
INV_ITEM inventory[MAX_INVENTORY_ITEMS];

//------------------------------------------------------------


//------------------------------------------------------------
// variables containing state that can be reset after game load

// previous cursor position position in world coords
int old_cursorx=0, old_cursory=0;
// previous cursor position in tile coords
int oldcursor_tx=0, oldcursor_ty=0;
// whether we are placing an item or are just moving the cursor
bool bPlace=false;
// belt selection changes on rotate key press
int place_belt_index=0;
int place_belt_selected=-1;
// Character state - direction and frame
int bob_facing = BOB_DOWN;
int bob_frame = 0;
// belt-layer animation
int belt_layer_frame = 0;

// Inventory position
int inv_items_wide = 5;
int inv_items_high = 4;
int inv_selected = 0;
//------------------------------------------------------------

//------------------------------------------------------------
// Configuration vars
int belt_speed = 7;
int key_wait = 15;
#define TILE_INFO_FILE "img/tileinfo.txt"
//------------------------------------------------------------

// counters
clock_t key_wait_ticks;
clock_t bob_anim_ticks;
clock_t move_wait_ticks;
clock_t item_move_wait_ticks;
clock_t layer_wait_ticks;

void game_loop();

int getWorldCoordX(int sx) { return (xpos + sx); }
int getWorldCoordY(int sy) { return (ypos + sy); }
int getTileX(int sx) { return (sx/gTileSize); }
int getTileY(int sy) { return (sy/gTileSize); }
int getTilePosInScreenX(int tx) { return ((tx * gTileSize) - xpos); }
int getTilePosInScreenY(int ty) { return ((ty * gTileSize) - ypos); }

void draw_tile(int tx, int ty, int tposx, int tposy);
void draw_screen();
void scroll_screen(int dir, int step);
bool can_scroll_screen(int dir, int step);
void draw_horizontal(int tx, int ty, int len);
void draw_vertical(int tx, int ty, int len);
void recentre();

int load_map(char *mapname);
void place_feature_overlay(uint8_t *data, int sx, int sy, int tile, int tx, int ty);

int readTileInfoFile(char *path, TileInfoFile *tif, int items);

void draw_bob(bool draw, int bx, int by, int px, int py);
bool move_bob(int dir, int speed);
bool bobAtEdge();

void start_place();
void stop_place();
void draw_place(bool draw);

void do_place();
void get_belt_neighbours(BELT_PLACE *bn, int tx, int ty);

void draw_layer();
void draw_horizontal_layer(int tx, int ty, int len);
void draw_box(int x,int y, int w, int h, int col);
void draw_corners(int x1,int y1, int w, int h, int col);
void draw_filled_box(int x,int y, int w, int h, int col, int bgcol);

void drop_item();

void move_items_on_belts();
void print_item_pos();

void show_inventory(int X, int Y);
void draw_digit(int i, int px, int py);
void draw_number(int n, int px, int py);
void draw_number_lj(int n, int px, int py);

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

	layer_belts = (int8_t *) malloc(sizeof(int8_t) * gMapWidth * gMapHeight);
	if (layer_belts == NULL)
	{
		printf("Out of memory\n");
		return -1;
	}
	memset(layer_belts, (int8_t)-1, gMapWidth * gMapHeight);

	/* start screen centred */
	xpos = gTileSize*(gMapWidth - gScreenWidth/gTileSize)/2; 
	ypos = gTileSize*(gMapHeight - gScreenHeight/gTileSize)/2; 
	bobx = (xpos + gTileSize*(gScreenWidth/gTileSize)/2) & 0xFFFFF0;
	boby = (ypos + gTileSize*(gScreenHeight/gTileSize)/2) & 0xFFFFF0;
	cursor_tx = getTileX(bobx+gTileSize/2);
	cursor_ty = getTileY(boby+gTileSize/2)+1;
	old_cursorx=cursorx;
	old_cursory=cursory;
	oldcursor_tx = cursor_tx;
	oldcursor_ty = cursor_ty;

	// setup complete
	vdp_mode(gMode);
	vdp_logical_scr_dims(false);
	//vdu_set_graphics_viewport()

	load_images();

	init_inventory(inventory);
	// for test add some items
	add_item(inventory, 0, 8);
	add_item(inventory, 1, 121);
	add_item(inventory, 2, 287);
	add_item(inventory, 7, 94);
	add_item(inventory, 9, 6);
	add_item(inventory, 3, 9876);

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
	bob_anim_ticks = clock();
	layer_wait_ticks = clock();
	key_wait_ticks = clock();
	item_move_wait_ticks = clock()+60;

	recentre();

	do {
		int dir=-1;
		int bob_dir = -1;
		int place_dir=-1;
		if ( vdp_check_key_press( KEY_w ) ) { bob_dir = BOB_UP; dir=SCROLL_UP; }
		if ( vdp_check_key_press( KEY_a ) ) { bob_dir = BOB_LEFT; dir=SCROLL_RIGHT; }
		if ( vdp_check_key_press( KEY_s ) ) { bob_dir = BOB_DOWN; dir=SCROLL_DOWN; }
		if ( vdp_check_key_press( KEY_d ) ) { bob_dir = BOB_RIGHT; dir=SCROLL_LEFT; }

		// scroll the screen AND/OR move Bob
		if (dir>=0 && ( move_wait_ticks < clock() ) ) {
			move_wait_ticks = clock()+1;
			// screen can scroll, move Bob AND screen
			if (can_scroll_screen(dir, 1) && move_bob(bob_dir, 1) )
			{
				draw_place(false); // remove cursor or it will smeer
				scroll_screen(dir,1);
				// Bob only needs to be re-drawn if he was at the edge of the screen (and so was removed)
				if (bobAtEdge()) {
					draw_bob(true,bobx,boby,xpos,ypos);
				}
				draw_place(true); // re-daw cursor
			}
			// can't scroll screen, just move Bob around
			if (!can_scroll_screen(dir, 1))
			{
				move_bob(bob_dir, 1);
			}
		}

		// cursor movement
		if ( vdp_check_key_press( KEY_LEFT ) ) {place_dir=SCROLL_LEFT; }
		if ( vdp_check_key_press( KEY_RIGHT ) ) {place_dir=SCROLL_RIGHT; }
		if ( vdp_check_key_press( KEY_UP ) ) {place_dir=SCROLL_UP; }
		if ( vdp_check_key_press( KEY_DOWN ) ) {place_dir=SCROLL_DOWN; }

		// keep cursor on screen
		if ( cursorx < 0 ) { place_dir=SCROLL_RIGHT; }
		if ( cursory < 0 ) { place_dir=SCROLL_DOWN; }
		if ( cursorx > gScreenWidth-gTileSize ) { place_dir=SCROLL_LEFT; }
		if ( cursory > gScreenHeight-gTileSize ) { place_dir=SCROLL_UP; }

		// move the cursor OR place rectangle
		if (place_dir>=0 && ( key_wait_ticks < clock() ) ) {
			draw_place(false);
			switch(place_dir) {
				case SCROLL_RIGHT: cursor_tx++; break;
				case SCROLL_LEFT: cursor_tx--; break;
				case SCROLL_UP: cursor_ty--; break;
				case SCROLL_DOWN: cursor_ty++; break;
				default: break;
			}
			draw_place(true);
			key_wait_ticks = clock() + key_wait;
		}

		if (layer_wait_ticks < clock()) 
		{
			draw_place(false);
			draw_layer();
			layer_wait_ticks = clock() + belt_speed; // belt anim speed
			draw_bob(true,bobx,boby,xpos,ypos);
			draw_place(true);
		}
			
		if ( vdp_check_key_press( KEY_c ) ) { recentre(); }
		if ( vdp_check_key_press( KEY_p ) ) { start_place(); }
		if ( vdp_check_key_press( KEY_q ) ) { stop_place(); }
		if ( key_pressed_code == KEY_r ) { // "r" Check actual key code - distinguishes lower/upper case
			if (bPlace && key_wait_ticks < clock() ) {
				place_belt_index++;  
				place_belt_index = place_belt_index % 4;
				key_wait_ticks = clock() + key_wait;
			}
		}
		if ( key_pressed_code == KEY_R ) { // "R" Check actual key code - distinguishes lower/upper case
			if (bPlace && key_wait_ticks < clock() ) {
				place_belt_index--;  
				if (place_belt_index < 0) place_belt_index += 4;
				key_wait_ticks = clock() + key_wait;
			}
		}
		if ( vdp_check_key_press( 0x8F ) ) // ENTER
		{
			do_place();
		}
		if ( vdp_check_key_press( KEY_z ) ) // z - drop an item
		{
			if (key_wait_ticks < clock()) 
			{
				drop_item();
				key_wait_ticks = clock() + key_wait;
			}
		}
		if ( vdp_check_key_press( 0x4A ) )  // ' - toggle debug
		{
			if (key_wait_ticks < clock()) 
			{
				debug = !debug;
				draw_screen();
				key_wait_ticks = clock() + key_wait;
			}
		}

		if ( vdp_check_key_press( KEY_x ) ) { // x
			TAB(6,8);printf("Are you sure?");
			char k=getchar(); 
			if (k=='y') exit=1;
			draw_screen();
		}

		if ( vdp_check_key_press( KEY_e ) ) 
		{
			if (key_wait_ticks < clock()) 
			{
				show_inventory(20,20);
				key_wait_ticks = clock() + key_wait;
			}
		}


		if ( item_move_wait_ticks <  clock() ) {
			move_items_on_belts();
			//if (debug) print_item_pos();
			item_move_wait_ticks = clock()+belt_speed;
		}

		vdp_update_key_state();
	} while (exit==0);

}

void draw_tile(int tx, int ty, int tposx, int tposy)
{
	uint8_t tile = tilemap[ty*gMapWidth + tx] & 0x0F;
	uint8_t overlay = (tilemap[ty*gMapWidth + tx] & 0xF0) >> 4;
	vdp_select_bitmap( tile + BMOFF_TERR16);
	vdp_draw_bitmap( tposx, tposy );
	if (overlay > 0)
	{
		int feat = overlay - 1;
		vdp_select_bitmap( feat + BMOFF_FEAT16);
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
void scroll_screen(int dir, int step)
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
			}
			break;
		default:
			break;
	}
}

bool can_scroll_screen(int dir, int step)
{
	if (dir == SCROLL_RIGHT || dir == SCROLL_LEFT) {
		if ((bobx - xpos) < gScreenWidth/2) { return false; }
		if ((bobx - xpos) > gScreenWidth/2) { return false; }
	}
	if (dir == SCROLL_UP || dir == SCROLL_DOWN) {
		if ((boby - ypos) < gScreenHeight/2) { return false; }
		if ((boby - ypos) > gScreenHeight/2) { return false; }
	}

	switch (dir) {
		case SCROLL_RIGHT: // scroll screen to right, view moves left
			if (xpos > step) { return true; }
			break;
		case SCROLL_LEFT: // scroll screen to left, view moves right
			if ((xpos + gScreenWidth + step) < (gMapWidth * gTileSize)) { return true; }
			break;
		case SCROLL_UP:
			if (ypos > step) { return true; }
			break;
		case SCROLL_DOWN:
			if ((ypos + gScreenHeight + step) < (gMapHeight * gTileSize)) { return true; }
			break;
		default:
			break;
	}
	return false;
}
uint8_t oval1_block6x6[6*6] = {
	0,0,3,3,0,0,
	0,1,2,1,2,0,
	3,2,1,1,2,0,
	3,1,2,1,1,3,
	0,3,1,3,2,0,
	0,0,2,0,0,0};

uint8_t oval2_block6x6[6*6] = {
	0,3,2,1,0,0,
	0,1,0,2,3,0,
	3,2,1,1,0,0,
	0,1,0,1,2,3,
	0,3,2,1,3,0,
	0,0,2,3,0,0};

uint8_t rnd1_block5x5[5*5] = {
	0,3,0,2,3,
	0,0,2,1,0,
	3,0,0,0,0,
	0,1,2,3,0,
	0,2,0,2,0,
};
uint8_t rnd2_block5x5[5*5] = {
	0,0,0,1,0,
	0,3,1,2,0,
	2,1,0,0,1,
	0,0,2,0,0,
	3,2,0,0,3,
};

uint8_t rnd1_block4x4[4*4] = {
	0,3,2,0,
	3,2,1,0,
	0,1,0,2,
	3,2,0,0,
};


int load_map(char *mapname)
{
	uint8_t ret = mos_load( mapname, (uint24_t) tilemap,  gMapWidth * gMapHeight );
	place_feature_overlay(oval1_block6x6,6,6,0,30,10); // stone    0:5:10
	place_feature_overlay(oval2_block6x6,6,6,1,10,23); // iron ore 1:6:11
	place_feature_overlay(oval1_block6x6,6,6,2,22,29); // copp ore 2:7:12
	place_feature_overlay(rnd1_block5x5,6,6,3,15,8);   // coal     3:8:13

	place_feature_overlay(rnd1_block5x5,5,5,4,5,5);    // tree     4:9:14
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
				tilemap[(tx+x) + (ty+y)*gMapWidth] |= (tile+(data[x+(y*sx)]-1)*5+1)<<4;
			}
		}
	}
}


void draw_bob(bool draw, int bx, int by, int px, int py)
{
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

bool move_bob(int dir, int speed)
{
	int newx=bobx, newy=boby;
	bool moved = false;

	bob_facing = dir;

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
	if (newx != bobx || newy != boby)
	{
		draw_bob(false,bobx,boby,xpos,ypos);
		bobx=newx;
		boby=newy;
		draw_bob(true,newx,newy,xpos,ypos);
		moved = true;
	}

	if (bob_anim_ticks > clock() ) return moved;
	bob_frame=(bob_frame+1)%4; 
	bob_anim_ticks = clock()+10;

	return moved;
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
	bPlace=true;
	draw_place(true);
}
void stop_place()
{
	if (!bPlace) return;
	draw_place(false);
	bPlace=false;
}

#define DIR_OPP(D) ((D+2)%4)

void draw_place(bool draw) 
{
	// undraw
	if (!draw) {
		draw_tile(oldcursor_tx, oldcursor_ty, old_cursorx, old_cursory);
		return;
	}

	cursorx = getTilePosInScreenX(cursor_tx);
	cursory = getTilePosInScreenY(cursor_ty);

	if (bPlace)
	{
		BELT_PLACE bn[4];
		get_belt_neighbours(bn, cursor_tx, cursor_ty);

		int in_connection = -1;
		int out_connection = -1;

		TAB(0,0);
		for (int i=0; i<4; i++)
		{
			if (bn[i].beltID != 255 && DIR_OPP(belts[bn[i].beltID].out) == i)
			{
				in_connection = DIR_OPP(i);
				break;
			}
			if (bn[i].beltID != 255 && DIR_OPP(belts[bn[i].beltID].in) == i)
			{
				out_connection = i;
				break;
			}
		}

		if (out_connection >= 0)
		{
			place_belt_selected = belt_rules_out[out_connection+1].rb[place_belt_index];
		} else {
			place_belt_selected = belt_rules_in[in_connection+1].rb[place_belt_index];
		}

		if (place_belt_selected>=0)
		{
			vdp_select_bitmap( BMOFF_BELT16 + place_belt_selected*4 );
			vdp_draw_bitmap( cursorx, cursory );
		}

		draw_box(cursorx, cursory, gTileSize-1, gTileSize-1, 15);
	}
	else
	{
		draw_corners(cursorx, cursory, gTileSize-1, gTileSize-1, 11);
	}

	old_cursorx=cursorx;
	old_cursory=cursory;
	oldcursor_tx = cursor_tx;
	oldcursor_ty = cursor_ty;
}

void draw_layer()
{
	int tx=getTileX(xpos);
	int ty=getTileY(ypos);

	for (int i=0; i < (1+gScreenHeight/gTileSize); i++) 
	{
		draw_horizontal_layer(tx, ty+i, 1+(gScreenWidth/gTileSize));
	}

	vdp_update_key_state();
	belt_layer_frame = (belt_layer_frame +1) % 4;
}

bool itemIsOnScreen(ItemNodePtr itemptr)
{
	if (itemptr->x > xpos - 8 &&
		itemptr->x < xpos + gScreenWidth &&
		itemptr->y > ypos - 8 &&
		itemptr->y < ypos + gScreenHeight)
	{
		return true;
	}
	return false;
}

void draw_horizontal_layer(int tx, int ty, int len)
{
	int px=getTilePosInScreenX(tx);
	int py=getTilePosInScreenY(ty);

	for (int i=0; i<len; i++)
	{
		if ( layer_belts[ty*gMapWidth + tx+i] >= 0 )
		{
			vdp_select_bitmap( layer_belts[ty*gMapWidth + tx+i]*4 + BMOFF_BELT16 + belt_layer_frame);
			vdp_draw_bitmap( px + i*gTileSize, py );
		}
	}

	ItemNodePtr currPtr = itemlist;
	while (currPtr != NULL) {
		if (itemIsOnScreen(currPtr))
		{
			vdp_select_bitmap( currPtr->item + BMOFF_ITEM8 );
			vdp_draw_bitmap( currPtr->x - xpos, currPtr->y - ypos );
		}
		currPtr = currPtr->next;
	}
}

void draw_box(int x,int y, int w, int h, int col)
{
	vdp_gcol( 0, col );
	vdp_move_to( x, y );
	vdp_line_to( x, y+h );
	vdp_line_to( x+w, y+h );
	vdp_line_to( x+w, y );
	vdp_line_to( x, y );
}
void draw_corners(int x1,int y1, int w, int h, int col)
{
	int cl=2;
	int x2 = x1+w;
	int y2 = y1+h;
	vdp_gcol( 0, col );
	vdp_move_to( x1, y1 ); vdp_line_to( x1+cl, y1 ); vdp_move_to( x1, y1 ); vdp_line_to( x1, y1+cl );
	vdp_move_to( x2, y1 ); vdp_line_to( x2-cl, y1 ); vdp_move_to( x2, y1 ); vdp_line_to( x2, y1+cl );
	vdp_move_to( x1, y2 ); vdp_line_to( x1, y2-cl ); vdp_move_to( x1, y2 ); vdp_line_to( x1+cl, y2 );
	vdp_move_to( x2, y2 ); vdp_line_to( x2, y2-cl ); vdp_move_to( x2, y2 ); vdp_line_to( x2-cl, y2 );
}

void draw_filled_box(int x,int y, int w, int h, int col, int bgcol)
{
	vdp_gcol( 0, bgcol );
	vdp_move_to( x, y );
	vdp_filled_rect( x+w, y+h );
	// border
	if (col != bgcol)
	{
		vdp_gcol( 0, col );
		vdp_move_to( x, y );
		vdp_line_to( x, y+h );
		vdp_line_to( x+w, y+h );
		vdp_line_to( x+w, y );
		vdp_line_to( x, y );
	}
}

void get_belt_neighbours(BELT_PLACE *bn, int tx, int ty)
{
	bn[0].beltID = layer_belts[tx   + (ty-1)*gMapWidth];
	bn[0].locX = tx;
	bn[0].locY = ty-1;

	bn[1].beltID = layer_belts[tx+1 + (ty)*gMapWidth];
	bn[1].locX = tx+1;
	bn[1].locY = ty;

	bn[2].beltID = layer_belts[tx   + (ty+1)*gMapWidth];
	bn[2].locX = tx;
	bn[2].locY = ty+1;

	bn[3].beltID = layer_belts[tx-1 + (ty)*gMapWidth];
	bn[3].locX = tx-1;
	bn[3].locY = ty;

}

void do_place()
{
	if (!bPlace || place_belt_selected<0) return;
		       
	layer_belts[gMapWidth * cursor_ty + cursor_tx] = place_belt_selected;
	
	stop_place();
}

void drop_item()
{
	// only one item type currently
	insertAtFrontItemList(2, cursor_tx*gTileSize+4, cursor_ty*gTileSize+4);
}

void move_items_on_belts()
{
	ItemNodePtr currPtr = itemlist;
	while (currPtr != NULL) {
		bool moved = false;
		if (itemIsOnScreen(currPtr))
		{
			int tx = getTileX(currPtr->x);
			int ty = getTileY(currPtr->y);
			int beltID = layer_belts[ tx + ty*gMapWidth ];
			if (beltID >= 0)
			{
				int dx = currPtr->x % gTileSize;
				int dy = currPtr->y % gTileSize;
				int in = belts[beltID].in;
				int out = belts[beltID].out;
				int nextx = currPtr->x;
				int nexty = currPtr->y;
				int nnx = nextx;
				int nny = nexty;
				switch (in)
				{
					case 0:  // in from top - move down
						if ( !moved && dy < 4 ) { nexty++; nny+=2; moved=true; }
						break;
					case 1: // in from right - move left
						if ( !moved && dx >= 5 ) { nextx--; nnx-=2;  moved=true; }
						break;
					case 2: // in from bottom - move up
						if ( !moved && dy >= 4 ) { nexty--; nny-=2; moved=true; }
						break;
					case 3: // in from left - move right
						if ( !moved && dx < 5 ) { nextx++; nnx+=2; moved=true; }
						break;
					default:
						break;
				}
				if (!moved) switch (out)
				{
					case 0: // out to top - move up
						if ( !moved ) { nexty--; nny-=2; moved=true; }
						break;
					case 1: // out to right - move right
						if ( !moved ) { nextx++; nnx+=2; moved=true; }
						break;
					case 2: // out to bottom - move down
						if ( !moved ) { nexty++; nny+=2; moved=true; }
						break;
					case 3: // out to left - move left
						if ( !moved ) { nextx--; nnx-=2; moved=true; }
						break;
					default:
						break;
				}

				if (moved)
				{
					bool found = findItem(currPtr->item, nextx, nexty);
					found |= findItem(currPtr->item, nnx, nny);
					if (!found) 
					{
						currPtr->x = nextx;
						currPtr->y = nexty;
					}
				}
			}
		}
		int tx = getTileX(currPtr->x);
		int ty = getTileY(currPtr->y);
		int beltID = layer_belts[ tx + ty*gMapWidth ];
		if (moved && beltID<0)
		{
			int px=getTilePosInScreenX(tx);
			int py=getTilePosInScreenY(ty);
			draw_tile(tx, ty, px, py);
		}	
		currPtr = currPtr->next;
	}

}

void print_item_pos()
{
	TAB(0,0);
	ItemNodePtr currPtr = itemlist;
	while (currPtr != NULL) {
		if (itemIsOnScreen(currPtr))
		{
			int tx = getTileX(currPtr->x);
			int ty = getTileY(currPtr->y);
			int beltID = layer_belts[ tx + ty*gMapWidth ];
			if (beltID >= 0)
			{
				int dx = currPtr->x % gTileSize;
				int dy = currPtr->y % gTileSize;
				int in = belts[beltID].in;
				int out = belts[beltID].out;

				printf("%d,%d : %d,%d (I%d O%d)   \n",currPtr->x, currPtr->y, dx, dy, in, out);
			}
		}
		currPtr = currPtr->next;
	}
}

#define INV_EXT_BORDER 3
#define INV_INT_BORDER 5
// Show inventory and run it's own mini game-loop
void show_inventory(int X, int Y)
{
	int offx = X;
	int offy = Y;
	int inv_offsetsX[20] = {0};
	int inv_offsetsY[20] = {0};
	int cursor_border_on = 15;
	int cursor_border_off = 7;
	// yellow border of UI with dark-grey fill
	int boxw = INV_EXT_BORDER*2 + (gTileSize + INV_INT_BORDER*2) * inv_items_wide;
	int boxh = INV_EXT_BORDER*2 + (gTileSize + INV_INT_BORDER*2) * inv_items_high;
	draw_filled_box( offx, offy, boxw, boxh, 11, 16 );

	for (int j=0; j<inv_items_high; j++)
	{
		for (int i=0; i<inv_items_wide; i++)
		{
			inv_offsetsX[i+j*inv_items_wide] = offx + INV_EXT_BORDER + i*(gTileSize+INV_INT_BORDER*2);
			inv_offsetsY[i+j*inv_items_wide] = offy + INV_EXT_BORDER + j*(gTileSize+INV_INT_BORDER*2);

			// grey border of cells
			draw_filled_box(
					inv_offsetsX[i+j*inv_items_wide],
					inv_offsetsY[i+j*inv_items_wide],
					gTileSize+INV_INT_BORDER*2, gTileSize+INV_INT_BORDER*2, cursor_border_off, 8);
		}
	}

	// draw inventory items
	for (int ii=0; ii<MAX_INVENTORY_ITEMS; ii++)
	{
		if (inventory[ii].item<255)
		{
			int item = inventory[ii].item;
			ItemType *type = &itemtypes[item];

			vdp_select_bitmap(type->bmID);

			vdp_draw_bitmap(
					inv_offsetsX[ii]+INV_INT_BORDER + 4*type->size,
					inv_offsetsY[ii]+INV_INT_BORDER + 4*type->size -2);

			draw_number(inventory[ii].count, 
					inv_offsetsX[ii]+INV_INT_BORDER*2+gTileSize-1,
					inv_offsetsY[ii]+INV_INT_BORDER*2+10);
		}
	}

	draw_box( 
			inv_offsetsX[inv_selected], 
			inv_offsetsY[inv_selected], 
			gTileSize + 2*INV_INT_BORDER, 
			gTileSize + 2*INV_INT_BORDER,
			cursor_border_on);

	// game loop for interacting with inventory
	clock_t key_wait_ticks = clock() + 20;
	bool finish = false;
	do {

		if ( key_wait_ticks < clock() && (vdp_check_key_press( KEY_e )||vdp_check_key_press( KEY_x )) ) 
		{
			key_wait_ticks = clock()+20;
			finish=true;
		}
		// cursor movement
		bool update_selected = false;
		int new_selected = inv_selected;

		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_RIGHT ) ) { 
			key_wait_ticks = clock()+20;
			if ( inv_selected < MAX_INVENTORY_ITEMS ) {
				new_selected++; 
				update_selected = true;
			}
		}
		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_LEFT ) ) {
	  		key_wait_ticks = clock()+20;
			if ( inv_selected > 0 ) {
				new_selected--; 
				update_selected = true;
			}
		}
		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_DOWN ) ) {
	   		key_wait_ticks = clock()+20;
			if ( inv_selected < MAX_INVENTORY_ITEMS - inv_items_wide ) {
				new_selected += inv_items_wide;
				update_selected = true;
			}
		}
		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_UP ) ) { 
			key_wait_ticks = clock()+20;
			if ( inv_selected >= inv_items_wide ) {
				new_selected -= inv_items_wide;
				update_selected = true;
			}
		}
		if ( update_selected )
		{
			draw_box( 
					inv_offsetsX[inv_selected], 
					inv_offsetsY[inv_selected], 
					gTileSize + 2*INV_INT_BORDER, 
					gTileSize + 2*INV_INT_BORDER,
					cursor_border_off);
			inv_selected = new_selected;
			draw_box( 
					inv_offsetsX[inv_selected], 
					inv_offsetsY[inv_selected], 
					gTileSize + 2*INV_INT_BORDER, 
					gTileSize + 2*INV_INT_BORDER,
					cursor_border_on);
		}
		vdp_update_key_state();
	} while (finish==false);
	draw_screen();
}

void draw_digit(int i, int px, int py)
{
	vdp_select_bitmap( BMOFF_NUMS + i );
	vdp_draw_bitmap( px, py );
}
// draw reverse digits
// right justified, so specify x,y at right edge of number
void draw_number(int n, int px, int py)
{
	int x = px-4;
	int y = py;

	if (n==0)
	{
		draw_digit(0, x, y);
		return;
	}
	while (n>0)
	{
		int dig = n % 10;
		draw_digit( dig, x, y);
		n /= 10;
		x -= 4;
	}
}

void draw_number_lj(int n, int px, int py)
{
	// left-just. x,y specifies left border 
	int diglen = 0;
	if (n<10) { 
		diglen = 1; 
	} else if (n<100) {
		diglen = 2;
	} else if (n<1000) {
		diglen = 3;
	}

	draw_number(n, px+diglen*4, py);
}

