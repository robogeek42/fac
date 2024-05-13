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
#include "../../agon_ccode/common/util.h"
#include "item.h"
#include "inventory.h"
#include "filedialog.h"

#define _IMAGE_IMPLEMENTATION
#include "images.h"
#define _MINER_IMPLEMENTATION
#include "miner.h"

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

#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3

bool debug = false;

char sigref[4] = "FAC";
#define SAVE_VERSION 1

//------------------------------------------------------------
// status related variables
// need to be saved / loaded

typedef struct {
	int version;
	int xpos;
	int ypos;		// Position of top-left of screen in world coords (pixel)
	int bobx;
	int boby;		// Position of character in world coords (pixel)
	int mapWidth;
	int mapHeight;
} FacState;

FacState fac;

// maps
uint8_t* tilemap;
int8_t* layer_belts;

// machine layer 
// top bit set means no machine at location
// can hold 8 different types of machine each with 4 in and 4 out variants
// bit     7    6 5   4 3   2 1 0
//     valid outdir indir      ID   
uint8_t* layer_machines;

// first node (head) of the Item list
ItemNodePtr itemlist = NULL;

// inventory
INV_ITEM inventory[MAX_INVENTORY_ITEMS];

MINER* miners = NULL;

//------------------------------------------------------------


//------------------------------------------------------------
// variables containing state that can be reset after game load

// cursor position position in screen pixel coords
int cursorx=0, cursory=0;
// cursor position in tile coords
int cursor_tx=0, cursor_ty=0;

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
// miners have a 3 frame animation
int miner_frame = 0;

// Inventory position
int inv_items_wide = 5;
int inv_items_high = 4;
int inv_selected = 0;

// item selected
uint8_t item_selected = 0;

// info
bool bInfoDisplayed = false;

// Mining state
bool bIsMining = false;
int mining_time = 200;
clock_t mining_time_ticks;
int bob_mining_anim_time = 40;
clock_t bob_mining_anim_ticks;
int bob_mining_anim_frame = 0;

// miner
clock_t miner_anim_timeout_ticks;
int miner_anim_speed = 30;
clock_t miner_update_ticks;
int miner_update_rate = 10;

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
clock_t layer_wait_ticks;

// message
bool bMessage = false;
int message_len = 0;
int message_posx = 0;
int message_posy = 0;
clock_t message_timeout_ticks;

// FPS 
int frame_time_in_ticks;
clock_t ticks_start;
clock_t display_fps_ticks;

clock_t func_start;
int func_time[4] = { 0 };

void wait();
void change_mode(int mode);

void game_loop();

int getWorldCoordX(int sx) { return (fac.xpos + sx); }
int getWorldCoordY(int sy) { return (fac.ypos + sy); }
int getTileX(int sx) { return (sx/gTileSize); }
int getTileY(int sy) { return (sy/gTileSize); }
int getTilePosInScreenX(int tx) { return ((tx * gTileSize) - fac.xpos); }
int getTilePosInScreenY(int ty) { return ((ty * gTileSize) - fac.ypos); }

void draw_tile(int tx, int ty, int tposx, int tposy);
void draw_screen();
void scroll_screen(int dir, int step);
bool can_scroll_screen(int dir, int step);
void draw_horizontal(int tx, int ty, int len);
void draw_vertical(int tx, int ty, int len);

bool check_dir_exists(char *path);
int load_map(char *mapname);
void place_feature_overlay(uint8_t *data, int sx, int sy, int tile, int tx, int ty);

int readTileInfoFile(char *path, TileInfoFile *tif, int items);

void draw_bob(int bx, int by, int px, int py);
bool move_bob(int dir, int speed);

void start_place();
void stop_place();
void draw_cursor(bool draw);
void draw_place_belt();

void do_place();
void get_belt_neighbours(BELT_PLACE *bn, int tx, int ty);

void draw_layer(bool draw_items);
void draw_horizontal_layer(int tx, int ty, int len, bool draw_belts, bool draw_machines, bool draw_items);

void drop_item(int item);

void move_items_on_belts();
void move_items_on_machines();
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
bool check_can_mine();
void removeAtCursor();
void pickupItemsAtTile(int tx, int ty);
void message_with_bm8(char *message, int bmID, int timeout);
void show_filedialog();
bool isMachineValid( int machine_byte );
int getMachineBMID(uint8_t machine_byte);
int getMachineItemType(uint8_t machine_byte);
int getOverlayAtOffset( int offset );
int getOverlayAtCursor();

static volatile SYSVAR *sys_vars = NULL;

void wait()
{
	char k=getchar();
	if (k=='q') exit(0);
}

void change_mode(int mode)
{
	sys_vars->vpd_pflags = 0;
	vdp_mode(mode);
	while ( !(sys_vars->vpd_pflags & vdp_pflag_mode) );
}


bool alloc_map()
{
	tilemap = (uint8_t *) malloc(sizeof(uint8_t) * fac.mapWidth * fac.mapHeight);
	if (tilemap == NULL)
	{
		printf("Out of memory\n");
		return false;
	}

	layer_belts = (int8_t *) malloc(sizeof(int8_t) * fac.mapWidth * fac.mapHeight);
	if (layer_belts == NULL)
	{
		printf("Out of memory\n");
		return false;
	}
	memset(layer_belts, (int8_t)-1, fac.mapWidth * fac.mapHeight);

	layer_machines = (uint8_t *) malloc(sizeof(uint8_t) * fac.mapWidth * fac.mapHeight);
	if (layer_machines == NULL)
	{
		printf("Out of memory\n");
		return false;
	}
	memset(layer_machines, (uint8_t) 1<<7, fac.mapWidth * fac.mapHeight);

	if ( !allocMiners(&miners) )
	{
		printf("Failed to alloc miners\n");
	}

	return true;
}

int main(/*int argc, char *argv[]*/)
{
	vdp_vdu_init();
	sys_vars = (SYSVAR *)mos_sysvars();

	if ( vdp_key_init() == -1 ) return 1;
	vdp_set_key_event_handler( key_event_handler );

	fac.version = SAVE_VERSION;

	// custom map which is 256x256 tiles
	fac.mapWidth = 45;
	fac.mapHeight = 45;

	if ( !alloc_map() ) return -1;

	if ( ! check_dir_exists("maps") )
	{
		printf("No maps directory\n");
		goto my_exit2;
	}
	if (load_map("maps/fmap.data") != 0)
	{
		printf("Failed to load map\n");
		goto my_exit2;
	}

	/* start bob and screen centred in map */
	fac.bobx = (fac.mapWidth * gTileSize / 2) & 0xFFFFF0;
	fac.boby = (fac.mapHeight * gTileSize / 2) & 0xFFFFF0;
	fac.xpos = fac.bobx - gScreenWidth/2;
	fac.ypos = fac.boby - gScreenHeight/2;

	cursor_tx = getTileX(fac.bobx+gTileSize/2);
	cursor_ty = getTileY(fac.boby+gTileSize/2)+1;
	cursorx = getTilePosInScreenX(cursor_tx);
	cursory = getTilePosInScreenY(cursor_ty);
	old_cursorx=cursorx;
	old_cursory=cursory;
	oldcursor_tx = cursor_tx;
	oldcursor_ty = cursor_ty;

	// setup complete
	change_mode(gMode);
	vdp_cursor_enable( false );
	vdp_logical_scr_dims( false );
	//vdu_set_graphics_viewport()

	if ( ! load_images(true) )
	{
		printf("Failed to load images\n");
		goto my_exit2;
	}
	create_sprites();

	init_inventory(inventory);
	// for test add some items
	add_item(inventory, IT_BELT, 112); // belt
	add_item(inventory, IT_IRON_ORE, 12);
	add_item(inventory, IT_COAL, 7);
	add_item(inventory, IT_COPPER_PLATE, 255);
	add_item(inventory, IT_FURNACE, 8);
	add_item(inventory, IT_MINER, 6);
	add_item(inventory, IT_ASSEMBLER, 1);
	add_item(inventory, IT_INSERTER, 1);

	inv_selected = 0; // belts
	item_selected = 0; // belts

	game_loop();

	change_mode(0);
my_exit2:
	free(tilemap);
	vdp_logical_scr_dims( true );
	vdp_cursor_enable( true );
	return 0;
}

void game_loop()
{
	int exit=0;
	
	draw_screen();
	draw_layer(true);

	bob_anim_ticks = clock();
	layer_wait_ticks = clock();
	key_wait_ticks = clock();
	display_fps_ticks = clock();
	frame_time_in_ticks = 0;
	miner_anim_timeout_ticks = clock() + miner_anim_speed;
	miner_update_ticks = clock() +miner_update_rate;

	select_bob_sprite( bob_facing );
	vdp_move_sprite_to( fac.bobx - fac.xpos, fac.boby - fac.ypos );
	vdp_refresh_sprites();

	do {
		int dir=-1;
		int bob_dir = -1;
		int place_dir=0;
		ticks_start = clock();

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
				if (debug)
				{
					int tx=getTileX(fac.xpos);
					int ty=getTileY(fac.ypos + gScreenHeight -1);
					draw_horizontal(tx,ty-1,6);
					draw_horizontal(tx,ty,6);
				}

				scroll_screen(dir,1);
				draw_cursor(true); // re-draw cursor
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
			if ( bPlace) draw_cursor(false);

			if ( place_dir & BITS_UP ) cursor_ty--;
			if ( place_dir & BITS_RIGHT ) cursor_tx++;
			if ( place_dir & BITS_DOWN ) cursor_ty++;
			if ( place_dir & BITS_LEFT ) cursor_tx--;

			draw_cursor(true);
		}

		if (layer_wait_ticks < clock()) 
		{
			layer_wait_ticks = clock() + belt_speed; // belt anim speed
			//draw_cursor(false);
			move_items_on_belts();
			draw_layer(true);
			draw_cursor(true);
		}
		if (debug)
		{
			COL(15);COL(128);
			TAB(0,29); printf("%d %d  ",frame_time_in_ticks,func_time[0]);
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
		if ( key_wait_ticks<clock() && key_pressed_code == KEY_r ) { // "r" rotate belt. Check actual key code - distinguishes lower/upper case
			if (bPlace && key_wait_ticks < clock() ) {
				key_wait_ticks = clock() + key_wait;
				if ( bInfoDisplayed ) clear_info();
				place_belt_index++;  
				place_belt_index = place_belt_index % 4;
				draw_tile( cursor_tx, cursor_ty, cursorx, cursory );
			}
		}
		if ( key_wait_ticks<clock() && key_pressed_code == KEY_R ) { // "R" rotate belt. Check actual key code - distinguishes lower/upper case
			if (bPlace && key_wait_ticks < clock() ) {
				key_wait_ticks = clock() + key_wait;
				if ( bInfoDisplayed ) clear_info();
				place_belt_index--;  
				if (place_belt_index < 0) place_belt_index += 4;
			}
		}
		if (  key_wait_ticks<clock() &&vdp_check_key_press( KEY_enter ) ) // ENTER - start placement state
		{
			if ( bInfoDisplayed ) clear_info();
			do_place();
		}

		if ( key_wait_ticks<clock() && vdp_check_key_press( KEY_delete ) ) // DELETE - delete item at cursor
		{
			key_wait_ticks = clock() + key_wait;
			removeAtCursor();
		}


		if ( vdp_check_key_press( KEY_backtick ) )  // ' - toggle debug
		{
			debug = !debug;
			while ( vdp_check_key_press( KEY_backtick ) );
			draw_screen();
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
				if ( ! bIsMining && check_can_mine() )
				{
					mining_time_ticks = clock() + mining_time;;
					bob_mining_anim_ticks = clock() + bob_mining_anim_time;;
					bIsMining = true;
					select_bob_sprite( BOB_SPRITE_ACT_DOWN+bob_facing );
					vdp_move_sprite_to( fac.bobx - fac.xpos, fac.boby - fac.ypos );
					vdp_refresh_sprites();
				}

			}
		}

		if ( bIsMining )
		{
			if ( bob_mining_anim_ticks < clock() )
			{
				bob_mining_anim_ticks = clock() + bob_mining_anim_time;
				bob_mining_anim_frame = ( bob_mining_anim_frame+1 ) % 2;
				vdp_select_sprite( BOB_SPRITE_ACT_DOWN + bob_facing );
				vdp_nth_sprite_frame( bob_mining_anim_frame );
				vdp_refresh_sprites();
			}
		  	if ( mining_time_ticks < clock() )
			{
				bIsMining = false;
				do_mining();
				select_bob_sprite( bob_facing );
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

		if ( bMessage && message_timeout_ticks < clock() )
		{
			bMessage = false;
			int tx=getTileX(message_posx);
			int ty=getTileY(message_posy);
			draw_horizontal( tx, ty, message_len+1 );
			draw_horizontal_layer( tx, ty, message_len+1, true, true, true );
			draw_horizontal( tx, ty+1, message_len+1 );
			draw_horizontal_layer( tx, ty+1, message_len+1, true, true, true );
		}

		if ( miner_anim_timeout_ticks < clock() )
		{
			miner_anim_timeout_ticks = clock() + miner_anim_speed;
			miner_frame = (miner_frame +1) % 3;
			move_items_on_machines();
		}

		if ( vdp_check_key_press( KEY_f ) ) // file dialog
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;

				show_filedialog();
			}
			
		}

		if ( miner_update_ticks < clock() )
		{
			miner_update_ticks = clock() + miner_update_rate;
			for (int m=0; m<minersAllocated; m++)
			{
				minerProduce( miners, m, &itemlist);
			}
		}
		vdp_update_key_state();

		frame_time_in_ticks = clock() - ticks_start;
	} while (exit==0);

}

bool isMachineValid( int machine_byte )
{
	return ( machine_byte & 0x80 ) == 0;
}

int getMachineItemType(uint8_t machine_byte)
{
	// machine byte is
	// bit     7    6 5   4 3   2 1 0
	//     valid outdir indir      ID   

	return (machine_byte & 0x7) + IT_TYPES_MACHINE;
}
int getMachineBMID(uint8_t machine_byte)
{
	// machine byte is
	// bit     7    6 5   4 3   2 1 0
	//     valid outdir indir      ID   

	int machine_itemtype = getMachineItemType(machine_byte);

	if ( machine_itemtype == IT_MINER)
	{
		int machine_outdir = (machine_byte & 0x60) >> 5;
		int bmid = BMOFF_MINERS + 3*machine_outdir;
		bmid += miner_frame;
		return bmid;
	} else {
		return itemtypes[machine_itemtype].bmID;
	}
}

int getOverlayAtOffset( int offset )
{
	return (tilemap[offset] & 0xF0) >> 4;
}
int getOverlayAtCursor()
{
	return (tilemap[cursor_ty*fac.mapWidth +  cursor_tx] & 0xF0) >> 4;
}

void draw_tile(int tx, int ty, int tposx, int tposy)
{
	int offset = ty*fac.mapWidth + tx;
	uint8_t tile = tilemap[offset] & 0x0F;
	uint8_t overlay = getOverlayAtOffset(offset);
	uint8_t machine_byte = layer_machines[offset];


	vdp_adv_select_bitmap( tile + BMOFF_TERR16);
	vdp_draw_bitmap( tposx, tposy );
	if (overlay > 0)
	{
		int feat = overlay - 1;
		vdp_adv_select_bitmap( feat + BMOFF_FEAT16);
		vdp_draw_bitmap( tposx, tposy );
	}
	if ( isMachineValid(machine_byte) )
	{
		int bmid = getMachineBMID(machine_byte);
		vdp_adv_select_bitmap( bmid );
		vdp_draw_bitmap( tposx, tposy );
	}
}

// draw full screen at World position in fac.xpos/fac.ypos
void draw_screen()
{
	int tx=getTileX(fac.xpos);
	int ty=getTileY(fac.ypos);

	for (int i=0; i < (1+gScreenHeight/gTileSize); i++) 
	{
		draw_horizontal(tx, ty+i, 1+(gScreenWidth/gTileSize));
	}

	draw_items();

	draw_bob(fac.bobx,fac.boby,fac.xpos,fac.ypos);
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
			if (fac.xpos > step)
			{
				fac.xpos -= step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx,ty+len)
				int tx=getTileX(fac.xpos);
				int ty=getTileY(fac.ypos);
				draw_vertical(tx,ty, 1+(gScreenHeight/gTileSize));
			}
			break;
		case SCROLL_LEFT: // scroll screen to left, view moves right
			if ((fac.xpos + gScreenWidth + step) < (fac.mapWidth * gTileSize))
			{
				fac.xpos += step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx,ty+len)
				int tx=getTileX(fac.xpos + gScreenWidth -1);
				int ty=getTileY(fac.ypos);
				draw_vertical(tx,ty, 1+(gScreenHeight/gTileSize));
			}
			break;
		case SCROLL_UP:
			if (fac.ypos > step)
			{
				fac.ypos -= step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx+len,ty)
				int tx=getTileX(fac.xpos);
				int ty=getTileY(fac.ypos);
				draw_horizontal(tx,ty, 1+(gScreenWidth/gTileSize));
			}
			break;
		case SCROLL_DOWN:
			if ((fac.ypos + gScreenHeight + step) < (fac.mapHeight * gTileSize))
			{
				fac.ypos += step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx+len,ty)
				int tx=getTileX(fac.xpos);
				int ty=getTileY(fac.ypos + gScreenHeight -1);
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
		if ((fac.bobx - fac.xpos) < gScreenWidth/2) { return false; }
		if ((fac.bobx - fac.xpos) > gScreenWidth/2) { return false; }
	}
	if (dir == SCROLL_UP || dir == SCROLL_DOWN) {
		if ((fac.boby - fac.ypos) < gScreenHeight/2) { return false; }
		if ((fac.boby - fac.ypos) > gScreenHeight/2) { return false; }
	}

	switch (dir) {
		case SCROLL_RIGHT: // scroll screen to right, view moves left
			if (fac.xpos > step) { return true; }
			break;
		case SCROLL_LEFT: // scroll screen to left, view moves right
			if ((fac.xpos + gScreenWidth + step) < (fac.mapWidth * gTileSize)) { return true; }
			break;
		case SCROLL_UP:
			if (fac.ypos > step) { return true; }
			break;
		case SCROLL_DOWN:
			if ((fac.ypos + gScreenHeight + step) < (fac.mapHeight * gTileSize)) { return true; }
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


bool check_dir_exists(char *path)
{
    FRESULT        fr = FR_OK;
    DIR            dir;
    fr = ffs_dopen(&dir, path);
	if ( fr == FR_OK )
	{
		return true;
	}
	return false;
}
int load_map(char *mapname)
{
	uint8_t ret = mos_load( mapname, (uint24_t) tilemap,  fac.mapWidth * fac.mapHeight );
	if ( ret != 0 )
	{
		return ret;
	}
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
				tilemap[(tx+x) + (ty+y)*fac.mapWidth] &= 0x0F;
				tilemap[(tx+x) + (ty+y)*fac.mapWidth] |= (tile+(data[x+(y*sx)]-1)*5+1)<<4;
			}
		}
	}
}


void draw_bob(int bx, int by, int px, int py)
{
	if ( bIsMining )
	{
		select_bob_sprite( bob_facing + BOB_SPRITE_ACT_DOWN );
	} else {
		select_bob_sprite( bob_facing );
	}
	vdp_move_sprite_to( bx - px, by - py );
	return;
}

// can tile be moved into?
bool check_tile(int px, int py)
{
	int tx=getTileX(px);
	int ty=getTileY(py);

	int offset = tx+ty*fac.mapWidth;
	return tilemap[offset]<16 && !isMachineValid(layer_machines[offset]);
}

bool move_bob(int dir, int speed)
{
	int newx=fac.bobx, newy=fac.boby;
	bool moved = false;

	if ( bob_facing != dir )
	{
		bob_facing = dir;
		select_bob_sprite( bob_facing );
	}

	switch (dir) {
		case BOB_LEFT:
			if (fac.bobx > speed 
					&& check_tile(fac.bobx-speed,fac.boby)
					&& check_tile(fac.bobx-speed,fac.boby+gTileSize-1)
					) {
				newx -= speed;
			}
			break;
		case BOB_RIGHT:
			if (fac.bobx < fac.mapWidth*gTileSize - speed 
					&& check_tile(fac.bobx+speed+gTileSize-1,fac.boby)
					&& check_tile(fac.bobx+speed+gTileSize-1, fac.boby+gTileSize-1)
						) {
				newx += speed;
			}
			break;
		case BOB_UP:
			if (fac.boby > speed 
					&& check_tile(fac.bobx,fac.boby-speed)
					&& check_tile(fac.bobx+gTileSize-1,fac.boby-speed)
					) {
				newy -= speed;
			}
			break;
		case BOB_DOWN:
			if (fac.boby < fac.mapHeight*gTileSize - speed 
				&& check_tile(fac.bobx,fac.boby+speed+gTileSize-1)
				&& check_tile(fac.bobx+gTileSize-1,fac.boby+speed+gTileSize-1)
				) {
				newy += speed;
			}
			break;
		default: break;
	}
	if (newx != fac.bobx || newy != fac.boby)
	{
		fac.bobx=newx;
		fac.boby=newy;
		vdp_select_sprite( bob_facing );
		vdp_move_sprite_to(fac.bobx-fac.xpos, fac.boby-fac.ypos);
		moved = true;
	}

	if (bob_anim_ticks > clock() ) return moved;
	bob_frame=(bob_frame+1)%4; 
	vdp_nth_sprite_frame( bob_frame );
	bob_anim_ticks = clock()+10;
	vdp_refresh_sprites();

	return moved;
}

void start_place()
{
	bPlace=true;
	draw_cursor(true);
}
void stop_place()
{
	if (!bPlace) return;
	draw_cursor(false);
	bPlace=false;
}

#define DIR_OPP(D) ((D+2)%4)

// Function draws the appropriate belt pice that fits in the place
// where the cursor is
void draw_place_belt()
{
	BELT_PLACE bn[4];
	get_belt_neighbours(bn, cursor_tx, cursor_ty);

	int in_connection = -1;
	int out_connection = -1;

	// work out what might connect to this piece of belt
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

	// update the displayed belt piece according to connection rules
	if (out_connection >= 0)
	{
		place_belt_selected = belt_rules_out[out_connection+1].rb[place_belt_index];
	} else {
		place_belt_selected = belt_rules_in[in_connection+1].rb[place_belt_index];
	}

	// draw
	if (place_belt_selected>=0)
	{
		vdp_adv_select_bitmap( BMOFF_BELT16 + place_belt_selected*4 );
		vdp_draw_bitmap( cursorx, cursory );
	}

}

// drawing resources is simple - no rotate
void draw_place_resource()
{
	vdp_adv_select_bitmap( itemtypes[item_selected].bmID );
	vdp_draw_bitmap( 
			cursorx + itemtypes[item_selected].size*4, 
			cursory + itemtypes[item_selected].size*4);
}

// Draw machines. These can be rotated, so draw approriate rotation.
void draw_place_machine()
{
	if ( item_selected == IT_MINER )
	{
		vdp_adv_select_bitmap( BMOFF_MINERS + place_belt_index*3 );
		vdp_draw_bitmap( cursorx, cursory);
		
	} else {
		vdp_adv_select_bitmap( itemtypes[item_selected].bmID );
		vdp_draw_bitmap( cursorx, cursory);
	}
}


// draw cursor or placement cursor along with the item selected
void draw_cursor(bool draw) 
{
	// undraw
	if (!draw) {
		draw_tile(oldcursor_tx, oldcursor_ty, old_cursorx, old_cursory);
		return;
	}

	cursorx = getTilePosInScreenX(cursor_tx);
	cursory = getTilePosInScreenY(cursor_ty);

	if (bPlace) // placement cursor is active
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
	vdp_refresh_sprites();

	old_cursorx=cursorx;
	old_cursory=cursory;
	oldcursor_tx = cursor_tx;
	oldcursor_ty = cursor_ty;
}

bool itemIsOnScreen(ItemNodePtr itemptr)
{
	if (itemptr->x > fac.xpos - 8 &&
		itemptr->x < fac.xpos + gScreenWidth &&
		itemptr->y > fac.ypos - 8 &&
		itemptr->y < fac.ypos + gScreenHeight)
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

// draws the additional layers: belts, machines and items
void draw_layer(bool draw_items)
{
	int tx=getTileX(fac.xpos);
	int ty=getTileY(fac.ypos);

	for (int i=0; i < (1+gScreenHeight/gTileSize); i++) 
	{
		draw_horizontal_layer(tx, ty+i, 1+(gScreenWidth/gTileSize), true, true, false);
	}

	if ( draw_items )
	{
		ItemNodePtr currPtr = itemlist;
		int cnt=0;
		while (currPtr != NULL) {
			if ( itemIsOnScreen(currPtr) )
			{
				vdp_adv_select_bitmap( itemtypes[currPtr->item].bmID );
				vdp_draw_bitmap( currPtr->x - fac.xpos, currPtr->y - fac.ypos );
			}
			currPtr = currPtr->next;
			cnt++;
			if (cnt % 4 == 0) vdp_update_key_state();
		}
	}		

	vdp_update_key_state();
	belt_layer_frame = (belt_layer_frame +1) % 4;
	//miner_frame = (miner_frame +1) % 3;
}

// draws the a single row of the additional layers: belts, machines and items
void draw_horizontal_layer(int tx, int ty, int len, bool draw_belts, bool draw_machines, bool draw_items)
{
	int px=getTilePosInScreenX(tx);
	int py=getTilePosInScreenY(ty);

	for (int i=0; i<len; i++)
	{
		int offset = ty*fac.mapWidth + tx+i;
		if ( draw_belts && layer_belts[offset] >= 0 )
		{
			vdp_adv_select_bitmap( layer_belts[offset]*4 + BMOFF_BELT16 + belt_layer_frame);
			vdp_draw_bitmap( px + i*gTileSize, py );
		}
		if ( draw_machines && isMachineValid(layer_machines[offset]) )
		{
			draw_tile(tx + i, ty, px + i*gTileSize, py); 
			//int bmid = getMachineBMID( layer_machines[offset] );
			//vdp_adv_select_bitmap( bmid );
			//vdp_draw_bitmap( px + i*gTileSize, py );
		}
	}

	if ( draw_items )
	{
		ItemNodePtr currPtr = itemlist;
		int cnt=0;
		while (currPtr != NULL) {
			if ( itemIsInHorizontal(currPtr, tx, ty, len) )
			{
				vdp_adv_select_bitmap( itemtypes[currPtr->item].bmID );
				vdp_draw_bitmap( currPtr->x - fac.xpos, currPtr->y - fac.ypos );
			}
			currPtr = currPtr->next;
			cnt++;
			if (cnt % 4 == 0) vdp_update_key_state();
		}
	}
}

void get_belt_neighbours(BELT_PLACE *bn, int tx, int ty)
{
	bn[0].beltID = layer_belts[tx   + (ty-1)*fac.mapWidth];
	bn[0].locX = tx;
	bn[0].locY = ty-1;

	bn[1].beltID = layer_belts[tx+1 + (ty)*fac.mapWidth];
	bn[1].locX = tx+1;
	bn[1].locY = ty;

	bn[2].beltID = layer_belts[tx   + (ty+1)*fac.mapWidth];
	bn[2].locX = tx;
	bn[2].locY = ty+1;

	bn[3].beltID = layer_belts[tx-1 + (ty)*fac.mapWidth];
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
			layer_belts[fac.mapWidth * cursor_ty + cursor_tx] = place_belt_selected;
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
		// miners can only be placed where there is an overlay resource
		if ( item_selected == IT_MINER )
		{
			uint8_t overlay = getOverlayAtCursor();
			if ( overlay > 0 )
			{
				if (remove_item( inventory, item_selected, 1 ))
				{
					layer_machines[fac.mapWidth * cursor_ty + cursor_tx] = 
						(item_selected - IT_TYPES_MACHINE) + (place_belt_index << 5);

					int feat_type = item_feature_map[overlay-1].feature_type;
					uint8_t raw_item = process_map[feat_type - IT_FEAT_STONE].raw_type;
					addMiner( &miners, cursor_tx, cursor_ty, raw_item, place_belt_index );
				}
			}
		} else 
		if ( remove_item( inventory, item_selected, 1 ) )
		{
			layer_machines[fac.mapWidth * cursor_ty + cursor_tx] =
				item_selected - IT_TYPES_MACHINE;
		}
	}
	
	stop_place();
}

void drop_item(int item)
{
	insertAtFrontItemList(&itemlist, item, cursor_tx*gTileSize+4, cursor_ty*gTileSize+4);
}

void move_items_on_machines()
{
	ItemNodePtr currPtr = itemlist;
	while (currPtr != NULL) 
	{
		bool moved = false;
		int tx = (currPtr->x +4) >> 4;
		int ty = (currPtr->y +4) >> 4;

		int miner = getMinerAtTileXY( miners, tx, ty );

		if ( miner >= 0 ) 
		{
			int nextx = currPtr->x;
			int nexty = currPtr->y;
			int nnx = nextx;
			int nny = nexty;
			switch( miners[miner].direction )
			{
				case DIR_UP:	// exit to top, reduce Y
					if ( !moved ) { nexty--; nny-=2; moved=true; }
					break;
				case DIR_RIGHT:
					if ( !moved ) { nextx++; nnx+=2; moved=true; }
					break;
				case DIR_DOWN:
					if ( !moved ) { nexty++; nny+=2; moved=true; }
					break;
				case DIR_LEFT:
					if ( !moved ) { nextx--; nnx-=2; moved=true; }
					break;
				default:
					break;
			}
			if (moved)
			{
				// check next pixel and the one after in the same direction
				bool found = isAnythingAtXY(&itemlist, nextx, nexty);
				found |= isAnythingAtXY(&itemlist, nnx, nny);
				if (!found) 
				{
					currPtr->x = nextx;
					currPtr->y = nexty;
				}
			}
		}
		if ( moved )
		{
			// get new tx/ty, if there is a miner there fine, otherwise, draw it
			tx = currPtr->x >> 4;
			ty = currPtr->y >> 4;
			int miner = getMinerAtTileXY( miners, tx, ty );

			// draw tiles where there is no belt that the item has moved into
			if (itemIsOnScreen(currPtr)  && miner<0)
			{
				int px=getTilePosInScreenX(tx);
				int py=getTilePosInScreenY(ty);
				draw_tile(tx, ty, px, py);
				draw_items_at_tile(tx, ty);
			}
		}
		
		currPtr = currPtr->next;
	}
}

void move_items_on_belts()
{
	// function timer
	func_start = clock();

	ItemNodePtr currPtr = itemlist;
	int offset = 4;
	while (currPtr != NULL) {
		bool moved = false;
		int centrex = currPtr->x + offset;
		int centrey = currPtr->y + offset;

		int tx = centrex >> 4; //getTileX(centrex);
		int ty = centrey >> 4; //getTileY(centrey);

		int beltID = layer_belts[ tx + ty*fac.mapWidth ];

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
				case DIR_UP:  // in from top - move down
					if ( !moved && dy < (offset+4) ) { nexty++; nny+=2; moved=true; }
					break;
				case DIR_RIGHT: // in from right - move left
					if ( !moved && (8-dx) < ((8-offset)-4) ) { nextx--; nnx-=2;  moved=true; }
					break;
				case DIR_DOWN: // in from bottom - move up
					if ( !moved && (8-dy) < ((8-offset)-4) ) { nexty--; nny-=2; moved=true; }
					break;
				case DIR_LEFT: // in from left - move right
					if ( !moved && dx < (offset+4) ) { nextx++; nnx+=2; moved=true; }
					break;
				default:
					break;
			}
			if (!moved) switch (out)
			{
				case DIR_UP: // out to top - move up
					if ( !moved ) { nexty--; nny-=2; moved=true; }
					break;
				case DIR_RIGHT: // out to right - move right
					if ( !moved ) { nextx++; nnx+=2; moved=true; }
					break;
				case DIR_DOWN: // out to bottom - move down
					if ( !moved ) { nexty++; nny+=2; moved=true; }
					break;
				case DIR_LEFT: // out to left - move left
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
		if ( moved )
		{
			// get new tx/ty, if there is a belt there fine, otherwise, draw it
			tx = currPtr->x >> 4; // getTileX(currPtr->x);
			ty = currPtr->y >> 4; // getTileY(currPtr->y);
			beltID = layer_belts[ tx + ty*fac.mapWidth ];

			// draw tiles where there is no belt that the item has moved into
			if (itemIsOnScreen(currPtr)  && beltID<0)
			{
				int px=getTilePosInScreenX(tx);
				int py=getTilePosInScreenY(ty);
				draw_tile(tx, ty, px, py);
				draw_items_at_tile(tx, ty);
			}
		}
		// next
		currPtr = currPtr->next;
	}

	// function timer
	func_time[0]=clock()-func_start;
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
			int beltID = layer_belts[ tx + ty*fac.mapWidth ];
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
			vdp_draw_bitmap( currPtr->x - fac.xpos, currPtr->y - fac.ypos );
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
			vdp_draw_bitmap( currPtr->x - fac.xpos, currPtr->y - fac.ypos );
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
	vdp_refresh_sprites();

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
	vdp_refresh_sprites();
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

	int offset = cursor_ty*fac.mapWidth + cursor_tx;
	if ( layer_belts[ offset ] >=0 )
	{
		// belt
		info_item_type = IT_BELT;
		info_item_bmid = itemtypes[ IT_BELT ].bmID;
	} else if ( isMachineValid(layer_machines[offset]) )
	{
		// machine
		info_item_type = getMachineItemType(layer_machines[offset]);
		info_item_bmid = getMachineBMID(layer_machines[offset]);
	} else if ( tilemap[ offset ] > 15 )
	{
		// feature
		info_item_bmid = getOverlayAtOffset(offset) - 1 + BMOFF_FEAT16;
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

bool check_can_mine()
{
	bool bNext=false;
	int bx = getTileX(fac.bobx);
	int by = getTileY(fac.boby);
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

	int overlay = getOverlayAtCursor();
	if ( bNext && overlay > 0 ) return true;
	return false;
}

void do_mining()
{
	// Mining/Tree-cutting
	// 1) cursor must be next to bob in direction they are facing
	// 2) cursor must be on an ore or tree (currently all FEATURES)
	// 3) reduce feature count by 1 every n ticks and add to inventory
	// 4) if count is < 0, remove

	if (bIsMining) return;
	if ( ! check_can_mine() ) return;

	int feature = getOverlayAtCursor() -1;
	
	// resource count not implemented yet

	int feat_type = item_feature_map[feature].feature_type;
	uint8_t raw_item = process_map[feat_type - IT_FEAT_STONE].raw_type;

	if ( raw_item > 0)
	{
		// add to inventory
		/* int ret = */add_item(inventory, raw_item, 1);
		// not doing anything if inventory is full ...
		message_with_bm8("+1",itemtypes[raw_item].bmID, 100);
	}
}

// remove Belt or Machine at cursor (with delete key)
void removeAtCursor()
{
	int offset = cursor_tx + cursor_ty * fac.mapWidth;
	if ( layer_belts[ offset ] >= 0 )
	{
		add_item( inventory, IT_BELT, 1 );
		layer_belts[ offset ] = -1;
	}
	if ( isMachineValid(layer_machines[offset]) )
	{
		int machine = getMachineItemType(layer_machines[offset]);
		if ( machine == IT_MINER )
		{
			int mnum = getMinerAtTileXY( miners, cursor_tx, cursor_ty );
			if ( mnum >=0 )
			{
				deleteMiner( miners, mnum );
				add_item( inventory, machine, 1 );
				layer_machines[offset] = 0x80;
			}
		} else {
			add_item( inventory, machine, 1 );
			layer_machines[offset] = 0x80;
		}
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

void message_with_bm8(char *message, int bmID, int timeout)
{
	//int sx = gScreenWidth - 8*(strlen(message)+1);
	//int sy = 0;
	int sx = fac.bobx-fac.xpos +8;
	int sy = fac.boby-fac.ypos -8;
	message_posx = sx + fac.xpos;
	message_posy = sy + fac.ypos;

	TAB((sx/8),(sy/8)); 
	COL(15);COL(8+128);
	printf("  %s",message);
	COL(15);COL(128);

	vdp_adv_select_bitmap( bmID );
	vdp_draw_bitmap( sx, sy );

	bMessage = true;
	message_len = strlen(message)+1;
	message_timeout_ticks = clock()+timeout;
}

bool save_game( char *filepath )
{
	bool ret = true;
	FILE *fp;
	int objs_written = 0;
	char *msg;

	COL(15);

	// Open the file for writing
	if ( !(fp = fopen( filepath, "wb" ) ) ) {
		printf( "Error opening file \"%s\"a\n.", filepath );
		return false;
	}

	// write file signature to 1st 3 bytes "FAC"	
	printf("Save: signature\n");
	for (int i=0;i<3;i++)
	{
		fputc( sigref[i], fp );
	}

	// write the fac state
	printf("Save: fac state\n");
	objs_written = fwrite( (const void*) &fac, sizeof(FacState), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: fac state\n"; goto save_game_errexit;
	}
	
	// write the tile map and layers
	printf("Save: tilemap\n");
	objs_written = fwrite( (const void*) tilemap, sizeof(uint8_t) * fac.mapWidth * fac.mapHeight, 1, fp);
	if (objs_written!=1) {
		msg = "Fail: tilemap\n"; goto save_game_errexit;
	}
	printf("Save: layer_belts\n");
	objs_written = fwrite( (const void*) layer_belts, sizeof(uint8_t) * fac.mapWidth * fac.mapHeight, 1, fp);
	if (objs_written!=1) {
		msg = "Fail: layer_belts\n"; goto save_game_errexit;
	}
	printf("Save: layer_machines\n");
	objs_written = fwrite( (const void*) layer_machines, sizeof(uint8_t) * fac.mapWidth * fac.mapHeight, 1, fp);
	if (objs_written!=1) {
		msg = "Fail: layer_machines\n"; goto save_game_errexit;
	}

	// save the item list
	
	// get and write number of items
	ItemNodePtr currPtr = itemlist;
	int cnt=0;
	while (currPtr != NULL) {
		currPtr = currPtr->next;
		cnt++;
	}
	printf("Save: num item count %d\n",cnt);
	objs_written = fwrite( (const void*) &cnt, sizeof(int), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: item count\n"; goto save_game_errexit;
	}

	// back to begining
	currPtr = itemlist;
	ItemNodePtr nextPtr = NULL;

	// must write data and no ll pointers
	printf("Save: items\n");
	while (currPtr != NULL) {
		nextPtr = currPtr->next;
	
		objs_written = fwrite( (const void*) currPtr, sizeof(ItemNodeSave), 1, fp);
		if (objs_written!=1) {
			msg = "Fail: item\n"; goto save_game_errexit;
		}
		currPtr = nextPtr;
	}
	
	// write the inventory
	printf("Save: inventory\n");
	objs_written = fwrite( (const void*) inventory, sizeof(INV_ITEM), MAX_INVENTORY_ITEMS, fp);
	if (objs_written!=MAX_INVENTORY_ITEMS) {
		msg = "Fail: inventory\n"; goto save_game_errexit;
	}

	// write miner data
	printf("Save: miners\n");
	int num_miner_objects = minersAllocated;
	objs_written = fwrite( (const void*) &num_miner_objects, sizeof(int), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: miner count\n"; goto save_game_errexit;
	}
	objs_written = fwrite( (const void*) miners, sizeof(MINER), num_miner_objects, fp);
	if (objs_written!=num_miner_objects) {
		msg = "Fail: miners\n"; goto save_game_errexit;
	}
	

	printf("done.\n");
	fclose(fp);
	return ret;

save_game_errexit:
	COL(9);printf("%s",msg);
	fclose(fp);
	COL(15);
	return false;

}
bool load_game( char *filepath )
{
	bool ret = true;
	FILE *fp;
	int objs_read = 0;
	char *msg;

	COL(15);

	// open file for reading
	if ( !(fp = fopen( filepath, "rb" ) ) ) {
		printf("Error opening file \"%s\".\n", filepath );
		return false;
	}

	char sig[4];

	printf("Load: signature\n");
	fgets( sig, 4, fp );

	for (int i=0; i<3; i++)
	{
		if ( sig[i] != sigref[i] )
		{
			printf("Error reading signature. i=%d\n", i );
			fclose(fp);
			return false;
		}
	}

	// read the fac state
	printf("Load: fac state\n");
	objs_read = fread( &fac, sizeof(FacState), 1, fp );
	if ( objs_read != 1 || fac.version != SAVE_VERSION )
	{
		printf("Fail L %d!=1 v%d\n", objs_read, fac.version );
		fclose(fp);
		return NULL;
	}

	// clear and read tilemap and layers
	free(tilemap);
	free(layer_belts);
	free(layer_machines);
	if ( ! alloc_map() ) return false;

	// read the tilemap
	printf("Load: tilemap\n");
	objs_read = fread( tilemap, sizeof(uint8_t) * fac.mapWidth * fac.mapHeight, 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read tilemap\n"; goto load_game_errexit;
	}
	// read the layer_belts
	printf("Load: layer_belts\n");
	objs_read = fread( layer_belts, sizeof(uint8_t) * fac.mapWidth * fac.mapHeight, 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read layer_belts\n"; goto load_game_errexit;
	}
	// read the layer_machines
	printf("Load: layer_machines\n");
	objs_read = fread( layer_machines, sizeof(uint8_t) * fac.mapWidth * fac.mapHeight, 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read layer_machines\n"; goto load_game_errexit;
	}

	// clear out item list
	ItemNodePtr currPtr = itemlist;
	ItemNodePtr nextPtr = NULL;
	while ( currPtr != NULL )
	{
		nextPtr = currPtr->next;
		ItemNodePtr pitem = popFrontItem(&itemlist);
		free(pitem);
		currPtr = nextPtr;
	}
	itemlist = NULL;
	
	// read number of items in list
	int num_items = 0;

	printf("Load: items ");
	objs_read = fread( &num_items, sizeof(int), 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read num items\n"; goto load_game_errexit;
	}
	printf("%d\n",num_items);

	// add items in one by one
	while (num_items > 0 && !feof( fp ) )
	{
		ItemNodeSave newitem;

		objs_read = fread( &newitem, sizeof(ItemNodeSave), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read item\n"; goto load_game_errexit;
		}
		insertAtBackItemList( &itemlist, newitem.item, newitem.x, newitem.y );
		num_items--;
	}

	// read the inventory
	printf("Load: inventory\n");
	for ( int i=0;i<MAX_INVENTORY_ITEMS; i++)
	{
		objs_read = fread( &inventory[i], sizeof(INV_ITEM), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read inv item\n"; goto load_game_errexit;
		}
	}

	// read the miners
	printf("Load: miners ");
	int num_miner_objects=0;
	objs_read = fread( &num_miner_objects, sizeof(int), 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read num miners\n"; goto load_game_errexit;
	}
	printf("%d\n",num_miner_objects);
	if ( miners )
	{
		free(miners);
	}
	miners = malloc(sizeof(MINER) * num_miner_objects);
	if ( !miners ) { 
		msg="Alloc Error\n"; goto load_game_errexit;
	}
	minersAllocated = num_miner_objects;
	objs_read = fread( miners, sizeof(MINER), minersAllocated, fp );
	if ( objs_read != minersAllocated ) { 
		msg="Fail read miners\n"; goto load_game_errexit;
	}
	// calculate number of miners active
	minersNum=0; for(int m=0;m<minersAllocated;m++) if (miners[m].valid) minersNum++;

	printf("\nDone.\n");
	fclose(fp);
	return ret;

load_game_errexit:
	COL(9);printf("%s",msg);
	fclose(fp);
	COL(15);
	return false;
}


// Show file dialog
void show_filedialog()
{
	vdp_select_sprite( CURSOR_SPRITE );
	vdp_hide_sprite();
	hide_bob();
	vdp_refresh_sprites();

	char filename[80];
	bool isload = false;

	change_mode(3);
	vdp_logical_scr_dims( false );
	vdp_cursor_enable( false );

	int fd_return = file_dialog("./saves", filename, 80, &isload);

	COL(11);COL(128+16);
	vdp_cls();
	TAB(0,0);
	if (fd_return)
	{
		if ( isload )
		{
			printf("Loading %s ... \n",filename);
			load_game( filename );
		} else {
			printf("Saving %s ... \n",filename);
			save_game( filename );
		}

		printf("\nPress any key\n");
		wait();
	}

	change_mode(gMode);
	vdp_cursor_enable( false );
	vdp_logical_scr_dims( false );

	vdp_activate_sprites( NUM_BOB_SPRITES + 1 );
	vdp_select_sprite( CURSOR_SPRITE );
	vdp_show_sprite();
	show_bob();

	draw_screen();

	vdp_refresh_sprites();
}

