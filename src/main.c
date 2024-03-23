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

#define _IMAGE_IMPLEMENTATION
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

#define BITS_UP 1
#define BITS_RIGHT 2
#define BITS_DOWN 4
#define BITS_LEFT 8

bool debug = false;

//------------------------------------------------------------
// status related variables
// need to be saved / loaded

// Position of top-left of screen in world coords (pixel)
int xpos=0, ypos=0;
// Position of character in world coords (pixel)
int bobx=0, boby=0;
// cursor position position in screen pixel coords
int cursorx=0, cursory=0;
// cursor position in tile coords
int cursor_tx=0, cursor_ty=0;

// maps
int gMapWidth = 200;
int gMapHeight = 200;
uint8_t* tilemap;
int8_t* layer_belts;
int8_t* layer_machines;

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

// item selected
uint8_t item_selected = 0;

// info
bool bInfoDisplayed = false;

//------------------------------------------------------------

//------------------------------------------------------------
// Configuration vars
int belt_speed = 7;
int key_wait = 20;
#define TILE_INFO_FILE "img/tileinfo.txt"
//------------------------------------------------------------

// counters
clock_t key_wait_ticks;
clock_t bob_anim_ticks;
clock_t move_wait_ticks;
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

int load_map(char *mapname);
void place_feature_overlay(uint8_t *data, int sx, int sy, int tile, int tx, int ty);

int readTileInfoFile(char *path, TileInfoFile *tif, int items);

void draw_bob(int bx, int by, int px, int py);
bool move_bob(int dir, int speed);

void start_place();
void stop_place();
void draw_place(bool draw);
void draw_place_belt();

void do_place();
void get_belt_neighbours(BELT_PLACE *bn, int tx, int ty);

void draw_layer();
void draw_horizontal_layer(int tx, int ty, int len);

void drop_item(int item);

void move_items_on_belts();
void print_item_pos();
void draw_items();
void draw_items_at_tile(int tx, int ty);

void show_inventory(int X, int Y);
void draw_digit(int i, int px, int py);
void draw_number(int n, int px, int py);
void draw_number_lj(int n, int px, int py);
void show_info();
void clear_info();
void do_mining();
void removeAtCursor();
void pickupItemsAtTile(int tx, int ty);

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

	layer_machines = (int8_t *) malloc(sizeof(int8_t) * gMapWidth * gMapHeight);
	if (layer_machines == NULL)
	{
		printf("Out of memory\n");
		return -1;
	}
	memset(layer_machines, (int8_t)-1, gMapWidth * gMapHeight);

	/* start bob and screen centred in map */
	bobx = (gMapWidth * gTileSize / 2) & 0xFFFFF0;
	boby = (gMapHeight * gTileSize / 2) & 0xFFFFF0;
	xpos = bobx - gScreenWidth/2;
	ypos = boby - gScreenHeight/2;

	cursor_tx = getTileX(bobx+gTileSize/2);
	cursor_ty = getTileY(boby+gTileSize/2)+1;
	cursorx = getTilePosInScreenX(cursor_tx);
	cursory = getTilePosInScreenY(cursor_ty);
	old_cursorx=cursorx;
	old_cursory=cursory;
	oldcursor_tx = cursor_tx;
	oldcursor_ty = cursor_ty;

	// setup complete
	vdp_mode(gMode);
	vdp_logical_scr_dims(false);
	//vdu_set_graphics_viewport()

	load_images();
	create_sprites();

	init_inventory(inventory);
	// for test add some items
	add_item(inventory, IT_BELT, 8); // belt
	add_item(inventory, IT_STONE, 121);
	add_item(inventory, IT_IRON_ORE, 287);
	add_item(inventory, IT_COPPER_PLATE, 94);
	add_item(inventory, IT_FURNACE, 6);
	add_item(inventory, IT_COPPER_ORE, 9876);
	inv_selected = 0; // belts
	item_selected = 0; // belts

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


	select_bob_sprite( bob_facing );
	vdp_move_sprite_to( bobx - xpos, boby - ypos );

	do {
		int dir=-1;
		int bob_dir = -1;
		int place_dir=0;

		if ( vdp_check_key_press( KEY_w ) ) { bob_dir = BOB_UP; dir=SCROLL_UP; }
		if ( vdp_check_key_press( KEY_a ) ) { bob_dir = BOB_LEFT; dir=SCROLL_RIGHT; }
		if ( vdp_check_key_press( KEY_s ) ) { bob_dir = BOB_DOWN; dir=SCROLL_DOWN; }
		if ( vdp_check_key_press( KEY_d ) ) { bob_dir = BOB_RIGHT; dir=SCROLL_LEFT; }

		// scroll the screen AND/OR move Bob
		if (dir>=0 && ( move_wait_ticks < clock() ) ) {
			if ( bInfoDisplayed ) clear_info();
			move_wait_ticks = clock()+1;
			// screen can scroll, move Bob AND screen
			if (can_scroll_screen(dir, 1) && move_bob(bob_dir, 1) )
			{
				//draw_place(false); // remove cursor or it will smeer
				scroll_screen(dir,1);
				draw_place(true); // re-draw cursor
			}
			// can't scroll screen, just move Bob around
			if (!can_scroll_screen(dir, 1))
			{
				move_bob(bob_dir, 1);
			}
		}

		// cursor movement
		if ( vdp_check_key_press( KEY_LEFT ) ) {place_dir |= BITS_LEFT; }
		if ( vdp_check_key_press( KEY_RIGHT ) ) {place_dir |= BITS_RIGHT; }
		if ( vdp_check_key_press( KEY_UP ) ) {place_dir |= BITS_UP; }
		if ( vdp_check_key_press( KEY_DOWN ) ) {place_dir |= BITS_DOWN; }

		// keep cursor on screen
		if ( cursorx < 0 ) { place_dir = BITS_RIGHT; }
		if ( cursory < 0 ) { place_dir = BITS_DOWN; }
		if ( cursorx > gScreenWidth-gTileSize ) { place_dir = BITS_LEFT; }
		if ( cursory > gScreenHeight-gTileSize ) { place_dir = BITS_UP; }

		// move the cursor OR place rectangle
		if (place_dir>0 && ( key_wait_ticks < clock() ) ) {
			key_wait_ticks = clock() + key_wait;
			if ( bInfoDisplayed ) clear_info();
			if ( bPlace) draw_place(false);

			if ( place_dir & BITS_UP ) cursor_ty--;
			if ( place_dir & BITS_RIGHT ) cursor_tx++;
			if ( place_dir & BITS_DOWN ) cursor_ty++;
			if ( place_dir & BITS_LEFT ) cursor_tx--;

			draw_place(true);
		}

		if (layer_wait_ticks < clock()) 
		{
			layer_wait_ticks = clock() + belt_speed; // belt anim speed
			//draw_place(false);
			draw_layer();
			move_items_on_belts();
			draw_place(true);
		}
			
		if ( vdp_check_key_press( KEY_p ) ) { // do place - get ready to place an item from inventory
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
				if ( bInfoDisplayed ) clear_info();
				if ( !bPlace ) {
					start_place();
				} else {
					stop_place();
				}
			}
		}
		if ( vdp_check_key_press( KEY_q ) ) { // quit out of place state
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
				if ( bInfoDisplayed ) clear_info();
				stop_place(); 
			}
		}
		if ( key_pressed_code == KEY_r ) { // "r" rotate belt. Check actual key code - distinguishes lower/upper case
			if (bPlace && key_wait_ticks < clock() ) {
				key_wait_ticks = clock() + key_wait;
				if ( bInfoDisplayed ) clear_info();
				place_belt_index++;  
				place_belt_index = place_belt_index % 4;
			}
		}
		if ( key_pressed_code == KEY_R ) { // "R" rotate belt. Check actual key code - distinguishes lower/upper case
			if (bPlace && key_wait_ticks < clock() ) {
				key_wait_ticks = clock() + key_wait;
				if ( bInfoDisplayed ) clear_info();
				place_belt_index--;  
				if (place_belt_index < 0) place_belt_index += 4;
			}
		}
		if ( vdp_check_key_press( KEY_enter ) ) // ENTER - start placement state
		{
			if ( bInfoDisplayed ) clear_info();
			do_place();
		}

		if ( vdp_check_key_press( KEY_delete ) ) // DELETE - delete item at cursor
		{
			key_wait_ticks = clock() + key_wait;
			removeAtCursor();
		}


		if ( vdp_check_key_press( KEY_backtick ) )  // ' - toggle debug
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
				debug = !debug;
				draw_screen();
			}
		}

		if ( vdp_check_key_press( KEY_x ) ) { // x - exit
			TAB(6,8);printf("Are you sure?");
			char k=getchar(); 
			if (k=='y' || k=='Y') exit=1;
			draw_screen();
		}

		if ( vdp_check_key_press( KEY_e ) ) // Brung up inventory
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
				if ( bInfoDisplayed ) clear_info();
				show_inventory(20,20);
			}
		}
		if ( vdp_check_key_press( KEY_i ) ) // i for item info
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
				if ( bInfoDisplayed ) 
				{
					clear_info();
				} else {
					show_info();
				}
			}
		}
		if ( vdp_check_key_press( KEY_m ) )  // m for MINE
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
				do_mining();

			}
		}
		if ( vdp_check_key_press( KEY_z ) )  // z - pick-up items under cursor
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
			
				pickupItemsAtTile(cursor_tx, cursor_ty);	
				draw_screen();
			}
		}

		vdp_update_key_state();
	} while (exit==0);

}

void draw_tile(int tx, int ty, int tposx, int tposy)
{
	uint8_t tile = tilemap[ty*gMapWidth + tx] & 0x0F;
	uint8_t overlay = (tilemap[ty*gMapWidth + tx] & 0xF0) >> 4;
	uint8_t machine = layer_machines[ty*gMapWidth + tx];


	vdp_adv_select_bitmap( tile + BMOFF_TERR16);
	vdp_draw_bitmap( tposx, tposy );
	if (overlay > 0)
	{
		int feat = overlay - 1;
		vdp_adv_select_bitmap( feat + BMOFF_FEAT16);
		vdp_draw_bitmap( tposx, tposy );
	}
	if ( machine >= 0 )
	{
		vdp_adv_select_bitmap( itemtypes[machine].bmID );
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

	draw_items();

	draw_bob(bobx,boby,xpos,ypos);
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


void draw_bob(int bx, int by, int px, int py)
{
	select_bob_sprite( bob_facing );
	vdp_move_sprite_to( bx - px, by - py );
	return;
}

// can tile be moved into?
bool check_tile(int px, int py)
{
	int tx=getTileX(px);
	int ty=getTileY(py);

	return tilemap[tx+ty*gMapWidth]<16 && layer_machines[tx+ty*gMapWidth]<0;
}

bool move_bob(int dir, int speed)
{
	int newx=bobx, newy=boby;
	bool moved = false;

	if ( bob_facing != dir )
	{
		bob_facing = dir;
		select_bob_sprite( bob_facing );
	}

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
		bobx=newx;
		boby=newy;
		vdp_select_sprite( bob_facing );
		vdp_move_sprite_to(bobx-xpos, boby-ypos);
		moved = true;
	}

	if (bob_anim_ticks > clock() ) return moved;
	bob_frame=(bob_frame+1)%4; 
	vdp_nth_sprite_frame( bob_frame );
	bob_anim_ticks = clock()+10;

	return moved;
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

void draw_place_belt()
{
	BELT_PLACE bn[4];
	get_belt_neighbours(bn, cursor_tx, cursor_ty);

	int in_connection = -1;
	int out_connection = -1;

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
		vdp_adv_select_bitmap( BMOFF_BELT16 + place_belt_selected*4 );
		vdp_draw_bitmap( cursorx, cursory );
	}

}

void draw_place_resource()
{
	vdp_adv_select_bitmap( itemtypes[item_selected].bmID );
	vdp_draw_bitmap( 
			cursorx + itemtypes[item_selected].size*4, 
			cursory + itemtypes[item_selected].size*4);
}

void draw_place_machine()
{
	vdp_adv_select_bitmap( itemtypes[item_selected].bmID );
	vdp_draw_bitmap( cursorx, cursory);
}


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
		if ( isBelt( item_selected ) )
		{
			draw_place_belt();
		}
		if ( isResource( item_selected ) )
		{
			draw_place_resource();
		}
		if ( isMachine( item_selected ) )
		{
			draw_place_machine();
		}
		
		vdp_select_sprite( CURSOR_SPRITE );
		vdp_move_sprite_to( cursorx, cursory );
		vdp_nth_sprite_frame( 1 );
	}
	else
	{
		vdp_select_sprite( CURSOR_SPRITE );
		vdp_move_sprite_to( cursorx, cursory );
		vdp_nth_sprite_frame( 0 );
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
bool itemIsInHorizontal(ItemNodePtr itemptr, int tx, int ty, int len)
{
	if ( itemptr->y < (ty-2)*gTileSize || itemptr->y > (ty+2)*gTileSize ) return false;
	if ( itemptr->x < tx*gTileSize || itemptr->x > (tx+len+1)*gTileSize ) return false;
	return true;
}

void draw_horizontal_layer(int tx, int ty, int len)
{
	int px=getTilePosInScreenX(tx);
	int py=getTilePosInScreenY(ty);

	for (int i=0; i<len; i++)
	{
		if ( layer_belts[ty*gMapWidth + tx+i] >= 0 )
		{
			vdp_adv_select_bitmap( layer_belts[ty*gMapWidth + tx+i]*4 + BMOFF_BELT16 + belt_layer_frame);
			vdp_draw_bitmap( px + i*gTileSize, py );
		}
		if ( layer_machines[ty*gMapWidth + tx+i] >= 0 )
		{
			vdp_adv_select_bitmap( itemtypes[layer_machines[ty*gMapWidth + tx+i]].bmID );
			vdp_draw_bitmap( px + i*gTileSize, py );
		}
	}

	ItemNodePtr currPtr = itemlist;
	int cnt=0;
	while (currPtr != NULL) {
		if ( itemIsInHorizontal(currPtr, tx, ty, len) )
		{
			vdp_adv_select_bitmap( itemtypes[currPtr->item].bmID );
			vdp_draw_bitmap( currPtr->x - xpos, currPtr->y - ypos );
		}
		currPtr = currPtr->next;
		cnt++;
		if (cnt % 4 == 0) vdp_update_key_state();
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
	if ( !bPlace ) return;
   	if ( isBelt(item_selected) ) {
		if ( place_belt_selected<0 ) return;

		int ret = remove_item( inventory, IT_BELT, 1 );
		if ( ret >= 0 )
		{
			layer_belts[gMapWidth * cursor_ty + cursor_tx] = place_belt_selected;
		}
	}
	if ( isResource(item_selected) ) {
		int ret = remove_item( inventory, item_selected, 1 );
		if ( ret >= 0 )
		{
			drop_item(item_selected);
		}
	}
	if ( isMachine(item_selected) ) {
		int ret = remove_item( inventory, item_selected, 1 );
		if ( ret >= 0 )
		{
			layer_machines[gMapWidth * cursor_ty + cursor_tx] = item_selected;
		}
	}
	
	stop_place();
}

void drop_item(int item)
{
	insertAtFrontItemList(&itemlist, item, cursor_tx*gTileSize+4, cursor_ty*gTileSize+4);
}

void move_items_on_belts()
{
	ItemNodePtr currPtr = itemlist;
	int offset = 4;
	while (currPtr != NULL) {
		bool moved = false;
		if (itemIsOnScreen(currPtr))
		{
			int centrex = currPtr->x + offset;
			int centrey = currPtr->y + offset;
			int tx = getTileX(centrex);
			int ty = getTileY(centrey);
			int beltID = layer_belts[ tx + ty*gMapWidth ];
			if (beltID >= 0)
			{
				int dx = centrex % gTileSize;
				int dy = centrey % gTileSize;
				int in = belts[beltID].in;
				int out = belts[beltID].out;
				int nextx = centrex;
				int nexty = centrey;
				int nnx = nextx;
				int nny = nexty;
				switch (in)
				{
					case 0:  // in from top - move down
						if ( !moved && dy < (offset+4) ) { nexty++; nny+=2; moved=true; }
						break;
					case 1: // in from right - move left
						if ( !moved && (8-dx) < ((8-offset)-4) ) { nextx--; nnx-=2;  moved=true; }
						break;
					case 2: // in from bottom - move up
						if ( !moved && (8-dy) < ((8-offset)-4) ) { nexty--; nny-=2; moved=true; }
						break;
					case 3: // in from left - move right
						if ( !moved && dx < (offset+4) ) { nextx++; nnx+=2; moved=true; }
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
					// check next pixel and the one after in the same direction
					bool found = isAnythingAtXY(&itemlist, nextx-offset, nexty-offset );
					found |= isAnythingAtXY(&itemlist, nnx-offset, nny-offset );
					if (!found) 
					{
						currPtr->x = nextx - offset;
						currPtr->y = nexty - offset;
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
			draw_items_at_tile(tx, ty);
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

void draw_items() 
{
	ItemNodePtr currPtr = itemlist;
	int cnt=0;
	while (currPtr != NULL) {
		if (itemIsOnScreen(currPtr))
		{
			vdp_adv_select_bitmap( itemtypes[currPtr->item].bmID );
			vdp_draw_bitmap( currPtr->x - xpos, currPtr->y - ypos );
		}
		currPtr = currPtr->next;
		cnt++;
		if (cnt % 4 == 0) vdp_update_key_state();
	}
}
void draw_items_at_tile(int tx, int ty) 
{
	ItemNodePtr currPtr = itemlist;
	while (currPtr != NULL) {
		if ( currPtr->x >= tx*gTileSize &&
			 currPtr->x < (tx+1)*gTileSize &&
		  	 currPtr->y >= ty*gTileSize &&
		  	 currPtr->y < (ty+1)*gTileSize )
		{
			vdp_adv_select_bitmap( itemtypes[currPtr->item].bmID );
			vdp_draw_bitmap( currPtr->x - xpos, currPtr->y - ypos );
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
	int cursor_border_off = 0;
	// yellow border of UI with dark-grey fill
	int boxw = INV_EXT_BORDER*2 + (gTileSize + INV_INT_BORDER*2) * inv_items_wide;
	int boxh = INV_EXT_BORDER*2 + (gTileSize + INV_INT_BORDER*2) * inv_items_high;

	vdp_select_sprite( CURSOR_SPRITE );
	vdp_hide_sprite();

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

			vdp_adv_select_bitmap(type->bmID);

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
	bool bDoPlaceAfterInventory = false;
	do {

		if ( key_wait_ticks < clock() && 
				(vdp_check_key_press( KEY_e ) ||
				 vdp_check_key_press( KEY_x ) ||
				 vdp_check_key_press( KEY_enter ) ) )
		{
			key_wait_ticks = clock()+10;
			if ( vdp_check_key_press( KEY_enter ) ) bDoPlaceAfterInventory=true;
			finish=true;
			item_selected = inventory[inv_selected].item;
			// delay otherwise x will cause exit from program
			while (key_wait_ticks > clock()) vdp_update_key_state();
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

	vdp_select_sprite( CURSOR_SPRITE );
	vdp_show_sprite();
	if ( bDoPlaceAfterInventory ) start_place();
}

void draw_digit(int i, int px, int py)
{
	vdp_adv_select_bitmap( BMOFF_NUMS + i );
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

void show_info()
{
	int info_item_bmid = -1;
	int info_item_type = 0;

	if ( layer_belts[ cursor_ty*gMapWidth + cursor_tx ] >=0 )
	{
		// belt
		info_item_type = IT_BELT;
		info_item_bmid = itemtypes[ IT_BELT ].bmID;
	} else if ( layer_machines[  cursor_ty*gMapWidth + cursor_tx ] >=0 )
	{
		// machine
		info_item_type = layer_machines[  cursor_ty*gMapWidth + cursor_tx ];
		info_item_bmid = itemtypes[ info_item_type ].bmID;
	} else if ( tilemap[ cursor_ty*gMapWidth + cursor_tx ] > 15 )
	{
		// feature
		info_item_bmid = ( ( tilemap[ cursor_ty*gMapWidth + cursor_tx ] & 0xF0 ) >> 4 ) - 1 + BMOFF_FEAT16;
		info_item_type = ((info_item_bmid - BMOFF_FEAT16 -1) % 5) + IT_FEAT_STONE;
	}
	if ( info_item_bmid >= 0 )
	{
		bInfoDisplayed = true;
		char * text = itemtypes[ info_item_type ].desc;
		int textlen = strlen(text) * 8;
		draw_filled_box( cursorx, cursory - 32, textlen+4, 31, 11, 8);
		vdp_adv_select_bitmap( info_item_bmid );
		vdp_draw_bitmap( cursorx+4, cursory - 28 );
		putch(0x05); // print at graphics cursor
		vdp_move_to( cursorx+3, cursory-10 );
		vdp_gcol(0, 15);
		printf("%s",itemtypes[ info_item_type ].desc);
		putch(0x04); // print at text cursor
	}
}

void clear_info()
{
	if ( !bInfoDisplayed ) return;
	draw_screen();
	bInfoDisplayed = false;
	return;
}

void do_mining()
{
	// Mining/Tree-cutting
	// 1) cursor must be next o bob in direction they are facing
	// 2) cursor must be on an ore or tree (currently all FEATURES)
	// 3) reduce feature count by 1 every n ticks and add to inventory
	// 4) if count is < 0, remove

	bool bNext=false;
	int bx = getTileX(bobx);
	int by = getTileY(boby);
	switch ( bob_facing )
	{
		case BOB_LEFT:
			if ( cursor_tx == bx - 1 ) bNext=true;
			break;
		case BOB_RIGHT:
			if ( cursor_tx == bx + 1 ) bNext=true;
			break;
		case BOB_UP:
			if ( cursor_ty == by - 1 ) bNext=true;
			break;
		case BOB_DOWN:
			if ( cursor_ty == by + 1 ) bNext=true;
			break;
		default:
			break;
	}

	int feature = (tilemap[ cursor_ty*gMapWidth +  cursor_tx] >> 4) -1;
	if ( !bNext || feature < 0 ) return;
	
	// resource count not implemented yet

	int feat_type = item_feature_map[feature].feature_type;
	uint8_t raw_item = process_map[feat_type - IT_FEAT_STONE].raw_type;

	if ( raw_item > 0)
	{
		// add to inventory
		/* int ret = */add_item(inventory, raw_item, 1);
		// not doing anything if inventory is full ...
	}
}

// remove Belt or Machine at cursor (eith delete key)
void removeAtCursor()
{
	if ( layer_belts[ cursor_tx + cursor_ty * gMapWidth ] >= 0 )
	{
		add_item( inventory, IT_BELT, 1 );
		layer_belts[ cursor_tx + cursor_ty * gMapWidth ] = -1;
	}
	if ( layer_machines[ cursor_tx + cursor_ty * gMapWidth ] >= 0 )
	{
		int machine = layer_machines[ cursor_tx + cursor_ty * gMapWidth ];
		add_item( inventory, machine, 1 );
		layer_machines[ cursor_tx + cursor_ty * gMapWidth ] = -1;
	}
	draw_tile( cursor_tx, cursor_ty, cursorx, cursory );
}

void pickupItemsAtTile(int tx, int ty)
{
	if ( !isEmptyItemList(&itemlist) ) 
	{
		//TAB(0,0);
		//printItemList(&itemlist);

		ItemNodePtr itptr = popItemsAtTile(&itemlist, tx, ty);

		//printf("after\n");
		//printItemList(&itemlist);
		//printf("in new list\n");
		//printItemList(&itptr);
		//wait();

		if ( itptr != NULL )
		{
			ItemNodePtr ip = popFrontItem(&itptr);
			//printf("pop %p ip->next %p \n",ip, ip->next);
			while (ip)
			{
				//printf("remove %p %d %d,%d\n", ip, ip->item, ip->x, ip->y);
				add_item( inventory, ip->item, 1 );
				free(ip);
				ip=NULL;
				
				ip = popFrontItem(&itptr);
			}
			//wait();
		}	
	}
}
