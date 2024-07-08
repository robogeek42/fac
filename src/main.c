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

#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3

#include "item.h"
#include "thinglist.h"

#define _INVENTORY_IMPLEMENTATION
#include "inventory.h"
#define _FILEDIALOG_IMPLEMENTATION
#include "filedialog.h"
#define _IMAGE_IMPLEMENTATION
#include "images.h"
#define _MACHINE_IMPLEMENTATION
#include "machine.h"
#define _INSERTER_IMPLEMENTATION
#include "inserter.h"

int gMode = 8; 
int gScreenWidth = 320;
int gScreenHeight = 240;

int gTileSize = 16;

int numItems;

#define ITEM_CENTRE_OFFSET 4

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

char sigref[4] = "FAC";
#define SAVE_VERSION 1

#define MACH_NEED_ENERGY

//------------------------------------------------------------
// status related variables
// need to be saved / loaded

typedef struct {
	int version;
	int xpos;
	int ypos;		// Position of top-left of screen in world coords (pixel)
	int bobx;
	int boby;		// Position of character in world coords (pixel)
	int energy;
} FacState;

FacState fac;

typedef struct {
	int width;
	int height;
} MapInfo;
MapInfo mapinfo;


// maps
uint8_t* tilemap;
int8_t* layer_belts;
void** objectmap;

// object lists
ThingNodePtr machinelist = NULL;
ThingNodePtr inserterlist = NULL;

// resource list
ItemNodePtr resourcelist = NULL;

// first node (head) of the Item list
ItemNodePtr itemlist = NULL;

// inventory
INV_ITEM inventory[MAX_INVENTORY_ITEMS];


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

// machines have a 3 frame animation
int machine_frame = 0;

// Inventory position
int inv_items_wide = 5;
int inv_items_high = 4;
int inv_selected = 0;

// item selected
uint8_t item_selected = 0;

// Mining state
bool bIsMining = false;
int mining_time = 200;
clock_t mining_time_ticks;
int bob_mining_anim_time = 40;
clock_t bob_mining_anim_ticks;
int bob_mining_anim_frame = 0;

// machine animation
clock_t machine_anim_timeout_ticks;
int machine_anim_speed = 30;

// machine update rate - at which it checks if it can prodcuce
clock_t machine_update_ticks;
int machine_update_rate = 10;

int resourceMultiplier = 32;

//------------------------------------------------------------

//------------------------------------------------------------
// Configuration vars
int belt_speed = 7;
int key_wait = 15;
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

void load_custom_chars();
int getWorldCoordX(int sx) { return (fac.xpos + sx); }
int getWorldCoordY(int sy) { return (fac.ypos + sy); }
int getTileX(int sx) { return (sx/gTileSize); }
int getTileY(int sy) { return (sy/gTileSize); }
int getTilePosInScreenX(int tx) { return ((tx * gTileSize) - fac.xpos); }
int getTilePosInScreenY(int ty) { return ((ty * gTileSize) - fac.ypos); }

void draw_tile(int tx, int ty, int tposx, int tposy);
void draw_machines(int tx, int ty, int tposx, int tposy);
void draw_screen();
void scroll_screen(int dir, int step);
bool can_scroll_screen(int dir, int step);
void draw_horizontal(int tx, int ty, int len);
void draw_vertical(int tx, int ty, int len);

bool check_dir_exists(char *path);
int load_map(char *mapname);
int load_map_info( char *filename);
void load_resource_data();
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
void draw_horizontal_layer(int tx, int ty, int len, bool bdraw_belts, bool bdraw_machines, bool bdraw_items);

void drop_item(int item);

void move_items(bool bDraw);
void move_items_on_inserters(bool bDraw);
void move_items_on_machines(bool bDraw);
void draw_items();
void draw_items_at_tile(int tx, int ty);

void show_inventory(int X, int Y);
void draw_digit(int i, int px, int py);
void draw_number(int n, int px, int py);
void draw_number_lj(int n, int px, int py);
void show_info();
void do_mining();
bool check_can_mine();
void removeAtCursor();
void pickupItemsAtTile(int tx, int ty);
void message_with_bm8(char *message, int bmID, int timeout);
void show_filedialog();
bool isValid( int machine_byte );
int getMachineBMID(int tx, int ty);
int getMachineBMIDmh(MachineHeader *mh);
int getMachineItemType(uint8_t machine_byte);
int getOverlayAtOffset( int tileoffset );
int getOverlayAtCursor();
void insertItemIntoMachine(int machine_type, int tx, int ty, int item );
void check_items_on_machines();
int getResourceCount(int tx, int ty);
ItemNodePtr getResource(int tx, int ty);
bool reduceResourceCount(int tx, int ty);
int show_assembler_dialog(bool bGenerator);

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

#define CHAR_RIGHTARROW 128
void load_custom_chars()
{
	// right arrow
	vdp_redefine_character( CHAR_RIGHTARROW, 0x00, 0x04, 0x02, 0xFF, 0x02, 0x04, 0x00, 0x00); // right arrow
}

bool alloc_map()
{
	tilemap = (uint8_t *) malloc(sizeof(uint8_t) * mapinfo.width * mapinfo.height);
	if (tilemap == NULL)
	{
		printf("Out of memory\n");
		return false;
	}

	layer_belts = (int8_t *) malloc(sizeof(int8_t) * mapinfo.width * mapinfo.height);
	if (layer_belts == NULL)
	{
		printf("Out of memory\n");
		return false;
	}
	memset(layer_belts, (int8_t)-1, mapinfo.width * mapinfo.height);

	objectmap = (void**) malloc(sizeof(void*) * mapinfo.width * mapinfo.height);
	if (objectmap == NULL)
	{
		printf("Out of memory\n");
		return false;
	}
	memset(objectmap, 0, 3*mapinfo.width * mapinfo.height);

	return true;
}

int main(/*int argc, char *argv[]*/)
{
	vdp_vdu_init();
	sys_vars = (SYSVAR *)mos_sysvars();

	if ( vdp_key_init() == -1 ) return 1;
	vdp_set_key_event_handler( key_event_handler );

	fac.version = SAVE_VERSION;

	load_custom_chars();

	if ( ! check_dir_exists("maps") )
	{
		printf("No maps directory\n");
		goto my_exit2;
	}

	if ( load_map_info("maps/newmap1.info") != 0 )
	{
		printf("Failed to load map info");
		goto my_exit2;
	}

	if ( !alloc_map() ) 
	{
		printf("Failed to alloc map layers\n");
		return -1;
	}

	if (load_map("maps/newmap1") != 0)
	{
		printf("Failed to load map\n");
		goto my_exit2;
	}

	fac.energy = 0;

	/* start bob and screen centred in map */
	fac.bobx = (mapinfo.width * gTileSize / 2) & 0xFFFFF0;
	fac.boby = (mapinfo.height * gTileSize / 2) & 0xFFFFF0;
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

	inventory_init(inventory);
	// for test add some items
	inventory_add_item(inventory, IT_BELT, 112); // belt
	inventory_add_item(inventory, IT_IRON_ORE, 12);
	inventory_add_item(inventory, IT_COAL, 7);
	inventory_add_item(inventory, IT_COPPER_PLATE, 100);
	inventory_add_item(inventory, IT_FURNACE, 20);
	inventory_add_item(inventory, IT_MINER, 20);
	inventory_add_item(inventory, IT_ASSEMBLER, 10);
	inventory_add_item(inventory, IT_INSERTER, 25);
	inventory_add_item(inventory, IT_BOX, 8);
	inventory_add_item(inventory, IT_GENERATOR, 4);
	inventory_add_item(inventory, IT_GEARWHEEL, 20);
	inventory_add_item(inventory, IT_WIRE, 20);
	inventory_add_item(inventory, IT_CIRCUIT, 20);
	inventory_add_item(inventory, IT_IRON_PLATE, 40);
	inventory_add_item(inventory, IT_PAVING, 22);

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
	machine_anim_timeout_ticks = clock() + machine_anim_speed;
	machine_update_ticks = clock() + machine_update_rate;

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
		if ( dir>=0 && ( move_wait_ticks < clock() ) ) {
			move_wait_ticks = clock()+1;
			// screen can scroll, move Bob AND screen
			if ( can_scroll_screen(dir, 1) )
			{
				if ( move_bob(bob_dir, 1) )
				{
					if (debug) // clear the data at the bottom of the screen
					{
						int tx=getTileX(fac.xpos);
						int ty=getTileY(fac.ypos + gScreenHeight -1);
						draw_horizontal(tx,ty-1,6);
						draw_horizontal(tx,ty,6);
					}

					scroll_screen(dir,1);
					draw_cursor(true); // re-draw cursor
				}
			}
			// can't scroll screen, just move Bob around
			else
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
			move_items(false);
			move_items_on_inserters(true);
			move_items_on_machines(true);
			check_items_on_machines();
			draw_layer(true);
			draw_cursor(true);
		}

		if (debug)
		{
			COL(15);COL(128);
			//TAB(0,29); printf("%d %d  ",frame_time_in_ticks,func_time[0]);
			TAB(0,29); printf("%d %d i:%d E:%d  ",frame_time_in_ticks,func_time[0], numItems, fac.energy);
		}
			
		if ( vdp_check_key_press( KEY_p ) ) { // do place - get ready to place an item from inventory
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
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
				stop_place(); 
			}
		}
		if ( key_wait_ticks<clock() && key_pressed_code == KEY_r ) { // "r" rotate belt. Check actual key code - distinguishes lower/upper case
			if (bPlace && key_wait_ticks < clock() ) {
				key_wait_ticks = clock() + key_wait;
				draw_cursor(false);
				place_belt_index++;  
				place_belt_index = place_belt_index % 4;
				//draw_tile( cursor_tx, cursor_ty, cursorx, cursory );
				draw_cursor(true);
			}
		}
		if ( key_wait_ticks<clock() && key_pressed_code == KEY_R ) { // "R" rotate belt. Check actual key code - distinguishes lower/upper case
			if (bPlace && key_wait_ticks < clock() ) {
				key_wait_ticks = clock() + key_wait;
				draw_cursor(false);
				place_belt_index--;  
				if (place_belt_index < 0) place_belt_index += 4;
				//draw_tile( cursor_tx, cursor_ty, cursorx, cursory );
				draw_cursor(true);
			}
		}
		if (  key_wait_ticks<clock() &&vdp_check_key_press( KEY_enter ) ) // ENTER - start placement state
		{
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
				show_inventory(20,20);
			}
		}
		if ( vdp_check_key_press( KEY_i ) ) // i for item info
		{
			if (key_wait_ticks < clock()) 
			{
				show_info();
				key_wait_ticks = clock() + key_wait;
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

		if ( fac.energy > 0 &&  machine_anim_timeout_ticks < clock() )
		{
			machine_anim_timeout_ticks = clock() + machine_anim_speed;
			machine_frame = (machine_frame +1) % 3;
		}

		if ( vdp_check_key_press( KEY_f ) ) // file dialog
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;

				show_filedialog();
			}
			
		}

		if ( machine_update_ticks < clock() )
		{
			machine_update_ticks = clock() + machine_update_rate;
			// update all the machines in turn
			ThingNodePtr tlistp = machinelist;
			while (tlistp != NULL)
			{
				if (tlistp->thing == NULL) break;
				Machine *mach = (Machine*) tlistp->thing;
				// miners reduce the resource count that they are mining
				if ( mach->machine_type == IT_MINER )
				{
					if ( getResourceCount( mach->tx, mach->ty ) > 0 
#if defined MACH_NEED_ENERGY
							&& fac.energy > 0
#endif
					)
					{
						if ( minerProduce( mach, &fac.energy ) )
						{
							reduceResourceCount( mach->tx, mach->ty );
						}
					}
				}

				// call producer for each of these
				if ( mach->machine_type == IT_FURNACE
#if defined MACH_NEED_ENERGY
						&& fac.energy > 0
#endif
				)
				{
					furnaceProduce( mach, &fac.energy );
				}
				if ( mach->machine_type == IT_ASSEMBLER
#if defined MACH_NEED_ENERGY
						&& fac.energy > 0
#endif
				)
				{
					assemblerProduce( mach, &fac.energy );
				}

				if ( mach->machine_type == IT_GENERATOR )
				{
					generatorProduce( mach, &fac.energy );
				}

				tlistp = tlistp->next;
			}
		}
		vdp_update_key_state();

		frame_time_in_ticks = clock() - ticks_start;
	} while (exit==0);

}

inline bool isValid( int byte )
{
	return ( byte & 0x80 ) == 0;
}

inline int getMachineItemType(uint8_t machine_byte)
{
	// machine byte is
	// bit     7    6 5  4 3 2 1 0
	//     valid outdir         ID   

	return (machine_byte & 0x7) + IT_TYPES_MACHINE;
}

int getMachineBMID(int tx, int ty)
{
	int tileoffset = ty*mapinfo.width + tx;
	MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
	return getMachineBMIDmh(mh);
}
int getMachineBMIDmh(MachineHeader *mh)
{
	int machine_itemtype = mh->machine_type;

	if ( machine_itemtype == IT_MINER)
	{
		int bmid = BMOFF_MINERS + 3*(mh->dir);
		bmid += machine_frame;
		return bmid;
	} else 
	if ( machine_itemtype == IT_FURNACE)
	{
		int bmid = BMOFF_FURNACES + 3*(mh->dir);

		Machine* mach = (Machine*)mh;
		if ( mach->ticksTillProduce>0)
		{
			bmid += machine_frame;
		}
		return bmid;
	} else 
	if ( machine_itemtype == IT_ASSEMBLER)
	{
		int bmid = BMOFF_ASSEMBLERS + 3*(mh->dir);

		Machine* mach = (Machine*)mh;
		if ( mach->ticksTillProduce>0)
		{
			bmid += machine_frame;
		}
		return bmid;
	} else 
	if ( machine_itemtype == IT_INSERTER)
	{
		int bmid = BMOFF_INSERTERS + 3*(mh->dir);
		bmid += machine_frame;

		return bmid;
	} else {
		return itemtypes[machine_itemtype].bmID;
	}
}

int getOverlayAtOffset( int tileoffset )
{
	return (tilemap[tileoffset] & 0xF0) >> 4;
}
int getOverlayAtCursor()
{
	return (tilemap[cursor_ty*mapinfo.width +  cursor_tx] & 0xF0) >> 4;
}

// draws terrain, features and machines at a tile
// (tx,ty) tile in map 
// (tposx,tposy) tile position in screen
void draw_tile(int tx, int ty, int tposx, int tposy)
{
	int tileoffset = ty*mapinfo.width + tx;
	uint8_t tile = tilemap[tileoffset] & 0x0F;
	uint8_t overlay = getOverlayAtOffset(tileoffset);

	vdp_adv_select_bitmap( tile + BMOFF_TERR16);
	vdp_draw_bitmap( tposx, tposy );
	if (overlay > 0)
	{
		int feat = overlay - 1;
		vdp_adv_select_bitmap( feat + BMOFF_FEAT16);
		vdp_draw_bitmap( tposx, tposy );
	}
}

// (tx,ty) tile in map 
// (tposx,tposy) tile position in screen
void draw_machines(int tx, int ty, int tposx, int tposy)
{
	int tileoffset = ty*mapinfo.width + tx;
	MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
	if (mh && mh->machine_type > 0)
	{
		int bmid = getMachineBMIDmh(mh);
		vdp_adv_select_bitmap( bmid );
		vdp_draw_bitmap( tposx, tposy );
		if (fac.energy == 0 && ( mh->machine_type < IT_INSERTER ))
		{
			vdp_adv_select_bitmap( BMOFF_ZAP );
			vdp_draw_bitmap( tposx, tposy );
		}
	}
}

void draw_horizontal(int tx, int ty, int len)
{
	int px=getTilePosInScreenX(tx);
	int py=getTilePosInScreenY(ty);

	for (int i=0; i<len; i++)
	{
		draw_tile(tx + i, ty, px + i*gTileSize, py); 
		draw_machines(tx + i, ty, px + i*gTileSize, py); 
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
		draw_machines(tx, ty + i, px, py + i*gTileSize);
		vdp_update_key_state();
	}
}

// draw full screen at World position in fac.xpos/fac.ypos
// does not draw belts
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
			if ((fac.xpos + gScreenWidth + step) < (mapinfo.width * gTileSize))
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
			if ((fac.ypos + gScreenHeight + step) < (mapinfo.height * gTileSize))
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
			if ((fac.xpos + gScreenWidth + step) < (mapinfo.width * gTileSize)) { return true; }
			break;
		case SCROLL_UP:
			if (fac.ypos > step) { return true; }
			break;
		case SCROLL_DOWN:
			if ((fac.ypos + gScreenHeight + step) < (mapinfo.height * gTileSize)) { return true; }
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
int load_map_info( char *filename)
{
	uint8_t ret = mos_load( filename, (uint24_t) &mapinfo,  2 );
	if ( ret != 0 )
	{
		printf("Failed to load %s\n",filename);
	}
	return ret;
}

void load_resource_data()
{
	for (int y=0; y<mapinfo.height; y++)
	{
		for (int x=0; x<mapinfo.width; x++)
		{
			int offset = x + mapinfo.width*y;
			int overlay = getOverlayAtOffset( offset );
			if (overlay>0)
			{
				int val = resourceMultiplier * 3;
				if (overlay > 5) val -= resourceMultiplier;
				if (overlay > 10) val -= resourceMultiplier;
				insertAtFrontItemList( &resourcelist, val, x, y);
			}
		}
	}
}
int load_map(char *mapname)
{
	uint8_t ret = mos_load( mapname, (uint24_t) tilemap,  mapinfo.width * mapinfo.height );
	if ( ret != 0 )
	{
		return ret;
	}
	
	load_resource_data();
	return ret;
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

	int tileoffset = tx+ty*mapinfo.width;
	MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
	return tilemap[tileoffset]>0 && tilemap[tileoffset]<16 && 
		(!mh || mh->machine_type == 0) &&
		(tilemap[tileoffset]&0x0F)!=14; // lake
}

bool move_bob(int dir, int speed)
{
	int newx=fac.bobx, newy=fac.boby;
	bool moved = false;

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
			if (fac.bobx < mapinfo.width*gTileSize - speed 
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
			if (fac.boby < mapinfo.height*gTileSize - speed 
				&& check_tile(fac.bobx,fac.boby+speed+gTileSize-1)
				&& check_tile(fac.bobx+gTileSize-1,fac.boby+speed+gTileSize-1)
				) {
				newy += speed;
			}
			break;
		default: break;
	}
	if (newx != fac.bobx || newy != fac.boby || bob_facing != dir )
	{
		fac.bobx=newx;
		fac.boby=newy;
		if ( bob_facing != dir )
		{
			bob_facing = dir;
			select_bob_sprite( bob_facing );
		}

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
		
	} else
	if ( item_selected == IT_FURNACE )
	{
		vdp_adv_select_bitmap( BMOFF_FURNACES + place_belt_index*3 );
		vdp_draw_bitmap( cursorx, cursory);
	} else
	if ( item_selected == IT_ASSEMBLER )
	{
		vdp_adv_select_bitmap( BMOFF_ASSEMBLERS + place_belt_index*3 );
		vdp_draw_bitmap( cursorx, cursory);
	} else
	if ( item_selected == IT_INSERTER )
	{
		vdp_adv_select_bitmap( BMOFF_INSERTERS + place_belt_index*3 );
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
		if ( isResource( item_selected ) || isProduct( item_selected ) )
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
		draw_horizontal_layer(tx, ty+i, 1+(gScreenWidth/gTileSize), true, true, false );
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
		
		ThingNodePtr tlistp = inserterlist;
		while ( tlistp != NULL )
		{
			if (tlistp->thing == NULL) break;
			Inserter *insp = (Inserter*)tlistp->thing;

			currPtr = insp->itemlist;
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
			tlistp = tlistp->next;
		}
		tlistp = machinelist;
		while ( tlistp != NULL )
		{
			if (tlistp->thing == NULL) break;
			Machine *machp = (Machine*)tlistp->thing;

			currPtr = machp->itemlist;
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
			tlistp = tlistp->next;
		}
	}		

	vdp_update_key_state();
	belt_layer_frame = (belt_layer_frame +1) % 4;
}

// draws the a single row of the additional layers: belts, machines and items
void draw_horizontal_layer(int tx, int ty, int len, bool bdraw_belts, bool bdraw_machines, bool bdraw_items)
{
	int px=getTilePosInScreenX(tx);
	int py=getTilePosInScreenY(ty);

	for (int i=0; i<len; i++)
	{
		int tileoffset = ty*mapinfo.width + tx+i;
		MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
		if (bdraw_machines && mh && mh->machine_type > 0)
		{
			draw_tile(tx + i, ty, px + i*gTileSize, py); 
		}
		if ( bdraw_belts && layer_belts[tileoffset] >= 0 )
		{
			vdp_adv_select_bitmap( layer_belts[tileoffset]*4 + BMOFF_BELT16 + belt_layer_frame);
			vdp_draw_bitmap( px + i*gTileSize, py );
		}
		if (bdraw_machines && mh && mh->machine_type > 0)
		{
			draw_machines(tx + i, ty, px + i*gTileSize, py); 
		}
	}

	if ( bdraw_items )
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

		ThingNodePtr tlistp = inserterlist;
		while ( tlistp != NULL )
		{
			if (tlistp->thing == NULL) break;
			Inserter *insp = (Inserter*)tlistp->thing;

			currPtr = insp->itemlist;
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
			tlistp = tlistp->next;
		}
		tlistp = machinelist;
		while ( tlistp != NULL )
		{
			if (tlistp->thing == NULL) break;
			Machine *machp = (Machine*)tlistp->thing;

			currPtr = machp->itemlist;
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
			tlistp = tlistp->next;
		}
	}
}

void get_belt_neighbours(BELT_PLACE *bn, int tx, int ty)
{
	bn[0].beltID = layer_belts[tx   + (ty-1)*mapinfo.width];
	bn[0].locX = tx;
	bn[0].locY = ty-1;

	bn[1].beltID = layer_belts[tx+1 + (ty)*mapinfo.width];
	bn[1].locX = tx+1;
	bn[1].locY = ty;

	bn[2].beltID = layer_belts[tx   + (ty+1)*mapinfo.width];
	bn[2].locX = tx;
	bn[2].locY = ty+1;

	bn[3].beltID = layer_belts[tx-1 + (ty)*mapinfo.width];
	bn[3].locX = tx-1;
	bn[3].locY = ty;

}

void do_place()
{
	if ( !bPlace ) return;
	int tileoffset = mapinfo.width * cursor_ty + cursor_tx;
   	if ( isBelt(item_selected) ) {
		if ( place_belt_selected<0 ) return;

		int ret = inventory_remove_item( inventory, IT_BELT, 1 );
		if ( ret >= 0 )
		{
			layer_belts[tileoffset] = place_belt_selected;
		}
	}
	if ( isResource(item_selected) || isProduct(item_selected) ) {
		bool placed=false;
		if ( item_selected == IT_PAVING )
		{
			// no features or machine or anything here?
			uint8_t overlay = (tilemap[tileoffset] & 0xF0) >> 4;
			if ( objectmap[ tileoffset ] == NULL && overlay == 0 && layer_belts[tileoffset] < 0 )
			{
				int ret = inventory_remove_item( inventory, item_selected, 1 );
				if (ret >= 0)
				{
					tilemap[ tileoffset ] = 13; // paving terrain
					placed=true;
				}
			}
		} 
		if (!placed)
		{
			int ret = inventory_remove_item( inventory, item_selected, 1 );
			if ( ret >= 0 )
			{
				drop_item(item_selected);
			}
		}
	}
	if ( isMachine(item_selected) ) {
		if ( objectmap[tileoffset]==NULL )
		{
			// miners can only be placed where there is an overlay resource
			if ( item_selected == IT_MINER )
			{
				uint8_t overlay = getOverlayAtCursor();
				if ( overlay > 0 )
				{
					if (inventory_remove_item( inventory, item_selected, 1 ))
					{
						int feat_type = item_feature_map[overlay-1].feature_type;
						uint8_t raw_item = process_map[feat_type - IT_FEAT_STONE].raw_type;
						Machine *mach = addMiner( &machinelist, cursor_tx, cursor_ty, raw_item, place_belt_index );
						objectmap[tileoffset] = mach;
					}
				}
			} else 
			if ( item_selected == IT_FURNACE )
			{
				if (inventory_remove_item( inventory, item_selected, 1 ))
				{
					Machine *mach = addFurnace( &machinelist, cursor_tx, cursor_ty, place_belt_index );
					objectmap[tileoffset] = mach;
				}
			} else 
			if ( item_selected == IT_ASSEMBLER )
			{
				int recipe = show_assembler_dialog(false);
				if (inventory_remove_item( inventory, item_selected, 1 ))
				{
					Machine *mach = addAssembler( &machinelist, cursor_tx, cursor_ty, place_belt_index, recipe );
					objectmap[tileoffset] = mach;
				}
			} else 
			if ( item_selected == IT_INSERTER )
			{
				if ( inventory_remove_item( inventory, item_selected, 1 ) ) // take from inventory
				{
					Inserter *insp = addInserter(&inserterlist, cursor_tx, cursor_ty, place_belt_index);
					objectmap[tileoffset] = insp;
				}
			} else 
			if ( item_selected == IT_GENERATOR )
			{
				int recipe = show_assembler_dialog(true);
				if (inventory_remove_item( inventory, item_selected, 1 ))
				{
					Machine *mach = addGenerator( &machinelist, cursor_tx, cursor_ty, place_belt_index, recipe );
					objectmap[tileoffset] = mach;
				}
			} else 
			if ( inventory_remove_item( inventory, item_selected, 1 ) )
			{
				// generic machine ... e.g. Box
				objectmap[tileoffset] = addMachine( &machinelist, item_selected, cursor_tx, cursor_ty, 0, 100, 0, 0);
			}
		}
	}

	stop_place();
}

void drop_item(int item)
{
	int tileoffset = cursor_tx + cursor_ty*mapinfo.width;
	MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
	if (mh && mh->machine_type > 0)
	{
		int machine_itemtype = mh->machine_type;
		if ( machine_itemtype == IT_FURNACE || machine_itemtype == IT_BOX || machine_itemtype == IT_ASSEMBLER || machine_itemtype == IT_GENERATOR )
		{
			insertItemIntoMachine( machine_itemtype, cursor_tx, cursor_ty, item );
			return;
		}
	}
	// place the 8x8 resource item in the centre of the square
	insertAtFrontItemList(&itemlist, item, cursor_tx*gTileSize + 4, cursor_ty*gTileSize + 4);
	numItems++;
}

/// @brief Move items along a belt or into an inserter or machine
void move_items(bool bDraw)
{
	// function timer
	func_start = clock();

	ItemNodePtr currPtr = itemlist;
	ItemNodePtr nextPtr = NULL;
	while (currPtr != NULL) {
		nextPtr = currPtr->next;

		bool moved = false;
		int centrex = currPtr->x + ITEM_CENTRE_OFFSET;
		int centrey = currPtr->y + ITEM_CENTRE_OFFSET;

		// (tx,ty) is the tile the centre of the item is in
		int tx = centrex >> 4; //getTileX(centrex);
		int ty = centrey >> 4; //getTileY(centrey);

		// (dx,dy) offset of centre of item in relation to tile
		int dx = centrex % gTileSize;
		int dy = centrey % gTileSize;

		// Check if item can move to an inserter
		ThingNodePtr thptr = inserterlist;
		while (thptr != NULL)
		{
			Inserter *insp = (Inserter*) thptr->thing;
			if ( ( (insp->start_tx == tx && insp->start_ty == ty) || 
			       (insp->tx == tx && insp->ty == ty))
			 && insp->itemcnt < insp->maxitemcnt)
			{
				bool pop = false;
				switch (insp->dir)
				{
				case DIR_UP:
				case DIR_DOWN:
					if (dx == 7 || dx == 8 || dx == 9) pop=true;
					break;
				case DIR_LEFT:
				case DIR_RIGHT:
					if (dy == 7 || dy == 8 || dy == 9) pop=true;
					break;
				
				default:
					break;
				}
				if (pop)
				{
					// pop item off global itemlist and put into inserter's own list
					ItemNodePtr ip = popItem( &itemlist, currPtr );
					numItems--;
					ip->next = insp->itemlist;
					insp->itemlist = ip;
					insp->itemcnt++;
				}
			}
			thptr = thptr->next;
			continue;
		}

		int beltOffset =  tx + ty*mapinfo.width;
		int beltID = layer_belts[ beltOffset ];
		bool redraw=false;
		if (!moved && beltID >= 0)
		{
			int in = belts[beltID].in;
			int out = belts[beltID].out;

			// calculate next x,y and next-next x,y
			int nextx = centrex;
			int nexty = centrey;
			int nnx = nextx;
			int nny = nexty;
			// check movement on belt towards the centre
			switch (in)
			{
				case DIR_UP:  // in from top - move down
					if ( !moved && dy < 8 ) { nexty++; nny+=2; moved=true; }
					break;
				case DIR_RIGHT: // in from right - move left
					if ( !moved && dx >= 8 ) { nextx--; nnx-=2;  moved=true; }
					break;
				case DIR_DOWN: // in from bottom - move up
					if ( !moved && dy >= 8 ) { nexty--; nny-=2; moved=true; }
					break;
				case DIR_LEFT: // in from left - move right
					if ( !moved && dx < 8 ) { nextx++; nnx+=2; moved=true; }
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
				bool found = isAnythingAtXY(&itemlist, nextx-ITEM_CENTRE_OFFSET, nexty-ITEM_CENTRE_OFFSET );
				found |= isAnythingAtXY(&itemlist, nnx-ITEM_CENTRE_OFFSET, nny-ITEM_CENTRE_OFFSET );
				if (!found) 
				{
					currPtr->x = nextx - ITEM_CENTRE_OFFSET;
					currPtr->y = nexty - ITEM_CENTRE_OFFSET;
#if 1
					// jump the tile to the centre of the belt it just moved into
					// bit hacky but avoids the item moving outside of the belt
					tx = (currPtr->x + ITEM_CENTRE_OFFSET) >> 4;
					ty = (currPtr->y + ITEM_CENTRE_OFFSET) >> 4;

					int newbeltOffset = tx + ty*mapinfo.width;
					if ( newbeltOffset != beltOffset )
					{
						int newbeltID = layer_belts[ newbeltOffset ];
						if (newbeltID >= 0)
						{
							int in = belts[newbeltID].in;
							switch ( belts[beltID].out ) 
							{
								case DIR_UP:
									if ( in != DIR_DOWN ) { redraw=true; currPtr->y = ty*gTileSize + 4; }
									break;
								case DIR_RIGHT:
									if ( in != DIR_LEFT ) { redraw=true; currPtr->x = tx*gTileSize + 4; }
									break;
								case DIR_DOWN:
									if ( in != DIR_UP ) { redraw=true; currPtr->y = ty*gTileSize + 4; }
									break;
								case DIR_LEFT:
									if ( in != DIR_RIGHT ) { redraw=true; currPtr->x = tx*gTileSize + 4; }
									break;
								default:
									break;
							}
						}
					}
#endif
				}
			}
		}


		if ( bDraw && moved )
		{
			// get new tx/ty, if there is a belt there fine, otherwise, draw it
			tx = (currPtr->x + ITEM_CENTRE_OFFSET) >> 4;
			ty = (currPtr->y + ITEM_CENTRE_OFFSET) >> 4;
			
			int tileoffset = tx + ty*mapinfo.width;
			int newbeltID = layer_belts[ tileoffset ];

			// draw tiles where there is no belt that the item has moved into
			if (itemIsOnScreen(currPtr) )
			{
				if ( ( newbeltID<0 && objectmap[tileoffset]==NULL ) || redraw )
				{
					int px=getTilePosInScreenX(tx);
					int py=getTilePosInScreenY(ty);
					draw_tile(tx, ty, px, py);
					//draw_machines(tx, ty, px, py);
					draw_items_at_tile(tx, ty);
				}
			}
		}
		// next
		currPtr = nextPtr;
	}

	// function timer
	func_time[0]=clock()-func_start;
}

void move_items_on_inserters(bool bDraw)
{
	// go through inserters and move their own items along
	ThingNodePtr tlistp = inserterlist;
	while ( tlistp != NULL )
	{
		Inserter *insp = (Inserter*)tlistp->thing;

		ItemNodePtr currPtr = insp->itemlist;
		ItemNodePtr nextPtr = NULL;
		while (currPtr != NULL) {
			nextPtr = currPtr->next;

			bool moved = false;
			int centrex = currPtr->x + ITEM_CENTRE_OFFSET;
			int centrey = currPtr->y + ITEM_CENTRE_OFFSET;
			int nextx = centrex;
			int nexty = centrey;
			int nnx = centrex;
			int nny = centrey;

			bool putback=false;
			switch (insp->dir )
			{
				case DIR_UP:
					nexty -= 1; nny -= 2; moved = true;
					if (nexty < ((insp->end_ty) * gTileSize + 8)) putback = true;
					break;
				case DIR_RIGHT:
					nextx += 1; nnx += 2; moved = true;
					if (nextx > ((insp->end_tx) * gTileSize + 7)) putback = true;
					break;
				case DIR_DOWN:
					nexty += 1; nny += 2; moved = true;
					if (nexty > ((insp->end_ty) * gTileSize + 7)) putback = true;
					break;
				case DIR_LEFT:
					nextx -= 1; nnx -= 2; moved = true;
					if (nextx < ((insp->end_tx) * gTileSize + 8)) putback = true;
					break;
				default:
					break;

			}

			if (  moved )
			{
				// check next pixel and the one after in the same direction
				bool found = isAnythingAtXY(&insp->itemlist, nextx-ITEM_CENTRE_OFFSET, nexty-ITEM_CENTRE_OFFSET );
				found |= isAnythingAtXY(&insp->itemlist, nnx-ITEM_CENTRE_OFFSET, nny-ITEM_CENTRE_OFFSET );
				if (!found) 
				{
					currPtr->x = nextx - ITEM_CENTRE_OFFSET;
					currPtr->y = nexty - ITEM_CENTRE_OFFSET;
				}
				if ( bDraw)
				{
					// re-draw tile covered by inserter
					int px = getTilePosInScreenX(insp->start_tx);
					int py = getTilePosInScreenY(insp->start_ty);
					draw_tile(insp->start_tx, insp->start_ty, px, py);
					draw_items_at_tile(insp->start_tx, insp->start_ty);

					px = getTilePosInScreenX(insp->tx);
					py = getTilePosInScreenY(insp->ty);
					draw_tile(insp->tx, insp->ty, px, py);
					draw_items_at_tile(insp->tx, insp->ty);

					px = getTilePosInScreenX(insp->end_tx);
					py = getTilePosInScreenY(insp->end_ty);
					draw_tile(insp->end_tx, insp->end_ty, px, py);
					draw_items_at_tile(insp->end_tx, insp->end_ty);
				}
			}
			if (putback)
			{
				ItemNodePtr it = popItem( &insp->itemlist, currPtr );
				if ( it )
				{
					// add to front of itemlist;
					it->next = itemlist;
					itemlist = it;
					insp->itemcnt--;
					numItems++;
				}
			}
			currPtr = nextPtr;
		}

		tlistp = tlistp->next;
	}
}

void move_items_on_machines(bool bDraw)
{
	// go through machines and move their own items along
	
	ThingNodePtr tlistp = machinelist;
	while (tlistp != NULL)
	{
		if (tlistp->thing == NULL) break;
		Machine *mach = (Machine*) tlistp->thing;
		ItemNodePtr currPtr = mach->itemlist;
		ItemNodePtr nextPtr = NULL;
		while (currPtr != NULL) {
			nextPtr = currPtr->next;

			bool moved = false;
			int centrex = currPtr->x + ITEM_CENTRE_OFFSET;
			int centrey = currPtr->y + ITEM_CENTRE_OFFSET;
			int nextx = centrex;
			int nexty = centrey;
			int nnx = centrex;
			int nny = centrey;

			bool putback=false;
			switch (mach->dir )
			{
				case DIR_UP:
					nexty -= 1; nny -= 2; moved = true;
					if (nexty < ((mach->ty) * gTileSize - 8)) putback = true;
					break;
				case DIR_RIGHT:
					nextx += 1; nnx += 2; moved = true;
					if (nextx > ((mach->tx+1) * gTileSize + 7)) putback = true;
					break;
				case DIR_DOWN:
					nexty += 1; nny += 2; moved = true;
					if (nexty > ((mach->ty+1) * gTileSize + 7)) putback = true;
					break;
				case DIR_LEFT:
					nextx -= 1; nnx -= 2; moved = true;
					if (nextx < ((mach->tx) * gTileSize - 8)) putback = true;
					break;
				default:
					break;

			}

			if ( moved )
			{
				// check next pixel and the one after in the same direction
				bool found = isAnythingAtXY(&mach->itemlist, nextx-ITEM_CENTRE_OFFSET, nexty-ITEM_CENTRE_OFFSET );
				found |= isAnythingAtXY(&mach->itemlist, nnx-ITEM_CENTRE_OFFSET, nny-ITEM_CENTRE_OFFSET );
				if (!found) 
				{
					currPtr->x = nextx - ITEM_CENTRE_OFFSET;
					currPtr->y = nexty - ITEM_CENTRE_OFFSET;
				}
				if ( bDraw)
				{
					// re-draw tile covered by machine
					int px = getTilePosInScreenX(mach->tx);
					int py = getTilePosInScreenY(mach->ty);
					draw_tile(mach->tx, mach->ty, px, py);
					draw_items_at_tile(mach->tx, mach->ty);

					px = getTilePosInScreenX(mach->end_tx);
					py = getTilePosInScreenY(mach->end_ty);
					draw_tile(mach->end_tx, mach->end_ty, px, py);
					draw_items_at_tile(mach->end_tx, mach->end_ty);
				}
			}
			if (putback)
			{
				ItemNodePtr it = popItem( &mach->itemlist, currPtr );
				if ( it )
				{
					// add to front of global itemlist;
					it->next = itemlist;
					itemlist = it;
					numItems++;
				}
			}
			currPtr = nextPtr;
		}
		tlistp = tlistp->next;
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
	
	// Items on inserters
	ThingNodePtr tlistp = inserterlist;
	while ( tlistp != NULL )
	{
		if (tlistp->thing == NULL) break;
		Inserter *insp = (Inserter*)tlistp->thing;

		// if inserter is on screen
		if ( (insp->tx +1) >= (fac.xpos >>4) && (insp->ty +1) >= (fac.ypos >>4) && 
		     (insp->tx -1) <= ((fac.xpos+gScreenWidth) >>4) && (insp->ty -1) <= ((fac.ypos+gScreenHeight) >>4) )
		{
			// draw its items
			currPtr = insp->itemlist;
			while (currPtr != NULL) {
				vdp_adv_select_bitmap( itemtypes[currPtr->item].bmID );
				vdp_draw_bitmap( currPtr->x - fac.xpos, currPtr->y - fac.ypos );
				currPtr = currPtr->next;
				cnt++;
				if (cnt % 4 == 0) vdp_update_key_state();
			}
		}
		tlistp = tlistp->next;
	}
	
	// Items on machines
	tlistp = machinelist;
	while (tlistp != NULL)
	{
		if (tlistp->thing == NULL) break;
		Machine *mach = (Machine*) tlistp->thing;
		if ( mach->tx >= (fac.xpos >>4) && mach->ty >= (fac.ypos >>4) &&
			 mach->tx <= ((fac.xpos+gScreenWidth) >>4) && mach->ty >= ((fac.ypos+gScreenHeight) >>4) )
		{
			currPtr = mach->itemlist;
			while (currPtr != NULL) {
				vdp_adv_select_bitmap( itemtypes[currPtr->item].bmID );
				vdp_draw_bitmap( currPtr->x - fac.xpos, currPtr->y - fac.ypos );
				currPtr = currPtr->next;
				cnt++;
				if (cnt % 4 == 0) vdp_update_key_state();
			}
		}
		tlistp = tlistp->next;
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
	int tileoffset = tx + ty * mapinfo.width;
	if ( objectmap[tileoffset] && ((MachineHeader*)objectmap[tileoffset])->machine_type == IT_INSERTER)
	{
		Inserter *insp = (Inserter*)objectmap[tileoffset];
		currPtr = insp->itemlist;
		while (currPtr != NULL) {
			vdp_adv_select_bitmap( itemtypes[currPtr->item].bmID );
			vdp_draw_bitmap( currPtr->x - fac.xpos, currPtr->y - fac.ypos );
			currPtr = currPtr->next;
		}
	}

	if ( objectmap[tileoffset] && ((MachineHeader*)objectmap[tileoffset])->machine_type != IT_INSERTER)
	{
		Machine *mach = (Machine*)objectmap[tileoffset];
		currPtr = mach->itemlist;
		while (currPtr != NULL) {
			vdp_adv_select_bitmap( itemtypes[currPtr->item].bmID );
			vdp_draw_bitmap( currPtr->x - fac.xpos, currPtr->y - fac.ypos );
			currPtr = currPtr->next;
		}
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
	int infox = cursorx;
	int infoy = cursory;
	int boxh = 31; int yadj = 0;

	if ( cursory < 120 ) {
		infoy -= 32;
	} else {
		infoy += 16;
	}

	int info_item_bmid = -1;
	int info_item_type = 0;
	int resource_count = -1;

	int tileoffset = cursor_ty*mapinfo.width + cursor_tx;
	if ( layer_belts[ tileoffset ] >=0 )
	{
		// belt
		info_item_type = IT_BELT;
		info_item_bmid = itemtypes[ IT_BELT ].bmID;
	} else if ( objectmap[tileoffset] )
	{
		// machine
		MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
		if (mh && mh->machine_type > 0)
		{
			info_item_type = mh->machine_type;
			info_item_bmid = getMachineBMIDmh(mh);
			if ( info_item_type == IT_ASSEMBLER )
			{
				boxh=39; yadj = 8;
				if ( cursory < 120 ) {
					infoy -= yadj;
				} else {
					infoy += yadj;
				}
			}
		}
	} else if ( tilemap[ tileoffset ] > 15 )
	{
		// feature
		info_item_bmid = getOverlayAtOffset(tileoffset) - 1 + BMOFF_FEAT16;
		info_item_type = ((info_item_bmid - BMOFF_FEAT16 ) % 5) + IT_FEAT_STONE;
	}

	resource_count = getResourceCount(cursor_tx, cursor_ty);

	if ( info_item_bmid >= 0 )
	{
		char * text = itemtypes[ info_item_type ].desc;
		int textlen = MAX(strlen(text) * 8, 64);
		draw_filled_box( infox, infoy, textlen+4, boxh, 11, 8);
		vdp_adv_select_bitmap( info_item_bmid );
		vdp_draw_bitmap( infox+4, infoy+4 );
		putch(0x05); // print at graphics cursor
		if ( info_item_type == IT_FURNACE || info_item_type == IT_ASSEMBLER || info_item_type == IT_GENERATOR )
		{
			Machine* mach = findMachine(&machinelist, cursor_tx, cursor_ty);
			int ptype = mach->ptype;
			if ( ptype >= 0 )
			{
				ProcessType *pt;
			   	if ( info_item_type == IT_FURNACE )
				{
					pt = &furnaceProcessTypes[ptype];
				}
			   	if ( info_item_type == IT_ASSEMBLER )
				{
					pt = &assemblerProcessTypes[ptype];
				}
			   	if ( info_item_type == IT_GENERATOR )
				{
					pt = &generatorProcessTypes[ptype];
				}
				for (int it = 0; it < pt->innum; it++)
				{
					if ( pt->in[it] > 0 )
					{
						int itemBMID = itemtypes[ pt->in[it] ].bmID;
						vdp_adv_select_bitmap( itemBMID );
						vdp_draw_bitmap( infox+4+20, infoy+4 + 8*it );
					} else {
						vdp_move_to( infox+4+20, infoy+4 + 8*it );
						printf("?");
					}
					vdp_move_to( infox+4+32, infoy+4 + 8*it );
					printf("%d",mach->countIn[it]);
				}
				if ( pt->out > 0 )
				{
					vdp_move_to( infox+4+44, infoy+4 + 4 ); putch(CHAR_RIGHTARROW);
					int itemBMID = itemtypes[ pt->out ].bmID;
					vdp_adv_select_bitmap( itemBMID );
					vdp_draw_bitmap( infox+4+44+8, infoy+4 + 4 );
				}
			}
		}
		if ( resource_count > 0 )
		{
			vdp_move_to( infox+4+20, infoy+4 );
			printf("%d",resource_count);
		}
		vdp_move_to( infox+3, infoy+4+18 + yadj );
		vdp_gcol(0, 15);
		printf("%s",itemtypes[ info_item_type ].desc);
		putch(0x04); // print at text cursor

		while( vdp_check_key_press( KEY_i ) ) { vdp_update_key_state(); }; 
		wait_for_any_key();
		draw_screen();
	}
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

int getResourceCount(int tx, int ty)
{
	// resource count
	ItemNodePtr rp = resourcelist;
	int resource_count = 0;
	while (rp != NULL)
	{
		if ( rp->x == tx && rp->y == ty)
		{
			resource_count = rp->item;
			break;
		}
		rp = rp->next;
	}
	return resource_count;
}
ItemNodePtr getResource(int tx, int ty)
{
	ItemNodePtr rp = resourcelist;
	while (rp != NULL)
	{
		if ( rp->x == tx && rp->y == ty)
		{
			break;
		}
		rp = rp->next;
	}
	return rp;
}

bool reduceResourceCount(int tx, int ty)
{
	ItemNodePtr rp = getResource( tx, ty );
	int offset = tx + ty * mapinfo.width;
	uint8_t overlay = ( tilemap[ offset ] & 0xF0 ) >> 4;
	if ( rp->item > 0 )
	{
		rp->item--;

		if ( rp->item == 0 )
		{
			// remove resource from tilemap and resourcelist
			tilemap[tx + ty*mapinfo.width] &= 0x0F;
			int px=getTilePosInScreenX(tx);
			int py=getTilePosInScreenY(ty);
			draw_tile(tx, ty, px, py);
			draw_machines(tx, ty, px, py);
			deleteItem(&resourcelist, rp);
		} else if ( (rp->item % resourceMultiplier) == 0 )
		{
			// size is reduced - change icon in tilemap
			overlay += 5;
			tilemap[tx + ty*mapinfo.width] &= 0x0F;
			tilemap[tx + ty*mapinfo.width] |= (overlay << 4);
			int px=getTilePosInScreenX(tx);
			int py=getTilePosInScreenY(ty);
			draw_tile(tx, ty, px, py);
			draw_machines(tx, ty, px, py);
		} 
		return true;
	}
	return false;
}
void do_mining()
{
	// Manual Mining/Tree-cutting
	// 1) cursor must be next to bob in direction they are facing
	// 2) cursor must be on an ore or tree (currently all FEATURES)
	// 3) reduce feature count by 1 every n ticks and add to inventory
	// 4) if count is < 0, remove

	if (bIsMining) return;
	if ( ! check_can_mine() ) return;

	int feature = getOverlayAtCursor() -1;

	int feat_type = item_feature_map[feature].feature_type;
	uint8_t raw_item = process_map[feat_type - IT_FEAT_STONE].raw_type;

	if ( raw_item > 0)
	{
		if ( reduceResourceCount(cursor_tx, cursor_ty) )
		{
			// add to inventory
			/* int ret = */inventory_add_item(inventory, raw_item, 1);
			// not doing anything if inventory is full ...
			message_with_bm8("+1",itemtypes[raw_item].bmID, 100);
		}
	}
}

// remove Belt or Machine at cursor (with delete key)
void removeAtCursor()
{
	int tileoffset = cursor_tx + cursor_ty * mapinfo.width;
	if ( layer_belts[ tileoffset ] >= 0 )
	{
		inventory_add_item( inventory, IT_BELT, 1 );
		layer_belts[ tileoffset ] = -1;
	}
	MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
	if (mh && mh->machine_type > 0)
	{
		int machine_type = mh->machine_type;
		if ( machine_type == IT_INSERTER )
		{
			inventory_add_item( inventory, IT_INSERTER, 1 );
			// delete inserter object
			ThingNodePtr thingnode = getInserterNode(&inserterlist, objectmap[tileoffset]);
			free( thingnode->thing );
			thingnode->thing=NULL;
			objectmap[tileoffset]=NULL;
			ThingNodePtr delme = popThing( &inserterlist, thingnode );
			free( delme );
		} else 
		{
			inventory_add_item( inventory, machine_type, 1 );
			// delete machine
			ThingNodePtr thingnode = getMachineNode( &machinelist, objectmap[tileoffset] );
			free( thingnode->thing );
			thingnode->thing=NULL;
			objectmap[tileoffset]=NULL;
			ThingNodePtr delme = popThing( &machinelist, thingnode );
			free( delme );
		}
	}
	draw_tile( cursor_tx, cursor_ty, cursorx, cursory );
	draw_machines( cursor_tx, cursor_ty, cursorx, cursory );

}

// z will pick up items at a tile and put them into the inventory
void pickupItemsAtTile(int tx, int ty)
{
	if ( !isEmptyItemList(&itemlist) ) 
	{
		ItemNodePtr itptr = popItemsAtTile(&itemlist, tx, ty);

		int cnt = 0;
		ItemNodePtr ip = itptr;
		while ( ip != NULL ) {
			cnt++;
			ip = ip->next;
		}
		numItems -= cnt;

		if ( itptr != NULL )
		{
			ItemNodePtr ip = popFrontItem(&itptr);
			while (ip)
			{
				inventory_add_item( inventory, ip->item, 1 );
				free(ip);
				ip=NULL;
				
				ip = popFrontItem(&itptr);
			}
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

	// 0. write file signature to 1st 3 bytes "FAC"	
	printf("Save: signature\n");
	for (int i=0;i<3;i++)
	{
		fputc( sigref[i], fp );
	}

	// 1. write the fac state
	printf("Save: fac state\n");
	objs_written = fwrite( (const void*) &fac, sizeof(FacState), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: fac state\n"; goto save_game_errexit;
	}

	// 2. write the tile map and layers
	printf("Save: tilemap\n");
	objs_written = fwrite( (const void*) tilemap, sizeof(uint8_t) * mapinfo.width * mapinfo.height, 1, fp);
	if (objs_written!=1) {
		msg = "Fail: tilemap\n"; goto save_game_errexit;
	}
	printf("Save: layer_belts\n");
	objs_written = fwrite( (const void*) layer_belts, sizeof(uint8_t) * mapinfo.width * mapinfo.height, 1, fp);
	if (objs_written!=1) {
		msg = "Fail: layer_belts\n"; goto save_game_errexit;
	}

	// 3. save the item list

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

	// 4. write the inventory
	printf("Save: inventory\n");
	objs_written = fwrite( (const void*) inventory, sizeof(INV_ITEM), MAX_INVENTORY_ITEMS, fp);
	if (objs_written!=MAX_INVENTORY_ITEMS) {
		msg = "Fail: inventory\n"; goto save_game_errexit;
	}

	// 5. write machine data
	printf("Save: machines\n");
	int num_machine_objects = countMachine(&machinelist);
	printf("%d objects\n",num_machine_objects);
	objs_written = fwrite( (const void*) &num_machine_objects, sizeof(int), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: machine count\n"; goto save_game_errexit;
	}

	ThingNodePtr thptr = machinelist;
	while (thptr != NULL )
	{
		// save the machine without the items
		Machine *mach = (Machine*)thptr->thing;

		objs_written = fwrite( (const void*) mach, sizeof(MachineSave), 1, fp);
		if (objs_written!=1) {
			msg = "Fail: machines\n"; goto save_game_errexit;
		}

		thptr = thptr->next;
	}

	// 6. write the resource data
	currPtr = resourcelist;
	cnt=0;
	while (currPtr != NULL) {
		currPtr = currPtr->next;
		cnt++;
	}
	printf("Save: num resources count %d\n",cnt);
	objs_written = fwrite( (const void*) &cnt, sizeof(int), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: resource count\n"; goto save_game_errexit;
	}

	// back to begining
	currPtr = resourcelist;
	nextPtr = NULL;

	// must write data and no ll pointers
	printf("Save: resources\n");
	while (currPtr != NULL) {
		nextPtr = currPtr->next;

		objs_written = fwrite( (const void*) currPtr, sizeof(ItemNodeSave), 1, fp);
		if (objs_written!=1) {
			msg = "Fail: item\n"; goto save_game_errexit;
		}
		currPtr = nextPtr;
	}

	// 7. write the Inserter objects
	printf("Save: inserters ");
	int num_inserter_objects = countInserters(&inserterlist);
	printf("%d objects\n",num_inserter_objects);
	objs_written = fwrite( (const void*) &num_inserter_objects, sizeof(int), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: inserter count\n"; goto save_game_errexit;
	}

	thptr = inserterlist;
	Inserter *insp = NULL;
	while (thptr != NULL )
	{
		// save the inserter without the items
		insp = (Inserter*)thptr->thing;

		objs_written = fwrite( (const void*) insp, sizeof(InserterSave), 1, fp);
		if (objs_written!=1) {
			msg = "Fail: inserter\n"; goto save_game_errexit;
		}

		thptr = thptr->next;
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
	// 0. write file signature to 1st 3 bytes "FAC"	
	// 1. write the fac state
	// 2. write the tile map and layers
	// 3. save the item list
	// 4. write the inventory
	// 5. write machine data
	// 6. write the resource data
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

	// 0. read file signature to 1st 3 bytes "FAC"	
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

	// 1. read the fac state
	printf("Load: fac state\n");
	objs_read = fread( &fac, sizeof(FacState), 1, fp );
	if ( objs_read != 1 || fac.version != SAVE_VERSION )
	{
		printf("Fail L %d!=1 v%d\n", objs_read, fac.version );
		fclose(fp);
		return NULL;
	}

	// 2. read the tile map and layers
	
	// clear and read tilemap and layers
	free(tilemap);
	free(layer_belts);
	free(objectmap);
	if ( ! alloc_map() ) return false;

	// read the tilemap
	printf("Load: tilemap\n");
	objs_read = fread( tilemap, sizeof(uint8_t) * mapinfo.width * mapinfo.height, 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read tilemap\n"; goto load_game_errexit;
	}
	// read the layer_belts
	printf("Load: layer_belts\n");
	objs_read = fread( layer_belts, sizeof(uint8_t) * mapinfo.width * mapinfo.height, 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read layer_belts\n"; goto load_game_errexit;
	}
	// objectmap is a list of pointers so will be filled in when we read the machines/inserters

	// 3. read the item list
	
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

	// 4. write the inventory
	printf("Load: inventory\n");
	for ( int i=0;i<MAX_INVENTORY_ITEMS; i++)
	{
		objs_read = fread( &inventory[i], sizeof(INV_ITEM), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read inv item\n"; goto load_game_errexit;
		}
	}

	// 5. read machine data
	printf("Load: Machines. clear ... ");
	clearMachines(&machinelist);

	int num_machs = 0;

	printf("num ");
	objs_read = fread( &num_machs, sizeof(int), 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read num machines\n"; goto load_game_errexit;
	}
	printf("%d ",num_machs);
	while (num_machs > 0 && !feof( fp ) )
	{
		Machine* newmachp = malloc(sizeof(Machine));
		if (newmachp == NULL)
		{
			msg="Alloc Error\n"; goto load_game_errexit;
		}

		objs_read = fread( newmachp, sizeof(MachineSave), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read machines\n"; goto load_game_errexit;
		}
		newmachp->itemlist = NULL;
		newmachp->ticksTillProduce = 0;
		insertAtBackThingList( &machinelist, newmachp);
		objectmap[newmachp->tx + newmachp->ty * mapinfo.width] = newmachp;
		num_machs--;
	}
	printf("done.\n");

	// 6. read the resource data
	
	printf("Load: resources. Clear ... ");
	// clear out resources list
	currPtr = resourcelist;
	nextPtr = NULL;
	while ( currPtr != NULL )
	{
		nextPtr = currPtr->next;
		ItemNodePtr pitem = popFrontItem(&resourcelist);
		free(pitem); pitem=NULL;
		currPtr = nextPtr;
	}
	resourcelist = NULL;

	// read number of resources in list
	num_items = 0;

	printf("num_items ");
	objs_read = fread( &num_items, sizeof(int), 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read num resources\n"; goto load_game_errexit;
	}
	printf("%d ",num_items);

	// add items in one by one
	while (num_items > 0 && !feof( fp ) )
	{
		ItemNodeSave newitem;

		objs_read = fread( &newitem, sizeof(ItemNodeSave), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read resource\n"; goto load_game_errexit;
		}
		insertAtBackItemList( &resourcelist, newitem.item, newitem.x, newitem.y );
		num_items--;
	}
	printf("done.\n");

	// 7. read the Inserter objects

	printf("Load: Inserters. clear ... ");
	clearInserters(&inserterlist);

	int num_ins = 0;

	printf("num ");
	objs_read = fread( &num_ins, sizeof(int), 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read num inserters\n"; goto load_game_errexit;
	}
	printf("%d ",num_ins);
	while (num_ins > 0 && !feof( fp ) )
	{
		Inserter* newinsp = malloc(sizeof(Inserter));
		if (newinsp == NULL)
		{
			msg="Alloc Error\n"; goto load_game_errexit;
		}

		objs_read = fread( newinsp, sizeof(InserterSave), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read inserter\n"; goto load_game_errexit;
		}
		newinsp->itemlist = NULL;
		newinsp->itemcnt = 0;

		insertAtBackThingList( &inserterlist, newinsp);
		objectmap[newinsp->tx + newinsp->ty * mapinfo.width] = newinsp;
		num_ins--;
	}
	printf("done.\n");

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

void insertItemIntoMachine(int machine_type, int tx, int ty, int item )
{
	// Box is a kind of machine which transfers all items entering it
	// to the inventory
	if ( machine_type == IT_BOX )
	{
		inventory_add_item(inventory, item, 1);
	}
	if ( machine_type == IT_FURNACE )
	{
		Machine* mach = findMachine(&machinelist, tx, ty);
		int ptype = mach->ptype;
		// if the furnace recipe is not set, set it based on the item inserted
		if ( ptype < 0 )
		{
			// find the correct furnace recipe
			for (int i=0; i<NUM_FURNACE_PROCESSES; i++)
			{
				if ( furnaceProcessTypes[i].in[0] == item )
				{
					mach->ptype = i;
					ptype=i; break;
				}
			}
		}
		if ( ptype >= 0 )
		{
			mach->countIn[0] += 1;
		}
	}
	if ( machine_type == IT_ASSEMBLER )
	{
		//TAB(0,0);
		Machine* mach = findMachine(&machinelist, tx, ty);
		int ptype = mach->ptype;
		//printf("it=%d, mach=%d innum %d : ", item, m, mach->process_type.innum);

		if ( ptype >= 0 )
		{
			// find which of the inputs matches this item
			for (int k=0; k < assemblerProcessTypes[ptype].innum; k++)
			{
				if ( assemblerProcessTypes[ptype].in[k] == item )
				{
					// increment count of that item in the assembler
					mach->countIn[k]++;
					break;
				}
			}
		}
	}
	if ( machine_type == IT_GENERATOR )
	{
		Machine* mach = findMachine(&machinelist, tx, ty);
		int ptype = mach->ptype;
		if ( ptype >= 0 )
		{
			// find which of the inputs matches this item
			for (int k=0; k < generatorProcessTypes[ptype].innum; k++)
			{
				if ( generatorProcessTypes[ptype].in[k] == item )
				{
					// increment count of that item in the assembler
					mach->countIn[k]++;
					break;
				}
			}
		}
	}
}

void check_items_on_machines()
{
	ItemNodePtr currPtr = itemlist;
	while (currPtr != NULL) {
		ItemNodePtr nextPtr = currPtr->next;
		int centrex = currPtr->x + ITEM_CENTRE_OFFSET;
		int centrey = currPtr->y + ITEM_CENTRE_OFFSET;

		// (tx,ty) is the tile the centre of the item is in
		int tx = centrex >> 4;
		int ty = centrey >> 4;

		int tileoffset = tx + ty*mapinfo.width;
		MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
		if (mh && mh->machine_type > 0)
		{
			int machine_itemtype = mh->machine_type;
			if ( machine_itemtype == IT_FURNACE || machine_itemtype == IT_BOX ||  machine_itemtype == IT_ASSEMBLER || machine_itemtype == IT_GENERATOR )
			{
				int item = currPtr->item;
				insertItemIntoMachine( machine_itemtype, tx, ty, item );
				ItemNodePtr popped = popItem( &itemlist, currPtr );
				numItems--;
				free(popped);
			}
		}
		currPtr = nextPtr;
	}
}

#define RECIPE_EXT_BORDER 5
#define RECIPE_INT_BORDER 2
#define RECIPE_NUM_SELECTIONS 4
#define RECIPE_BOX_WIDTH 120
#define RECIPE_SELECT_HEIGHT 24
#define RECIPE_TITLE_HEIGHT 10
int show_assembler_dialog(bool bGenerator)
{
	int offx = 10;
	int offy = 20;
	if ( cursorx > 160 ) offx = 170;

	int box_offsetsY[20] = {0};
	int cursor_border_on = 15; // white
	int cursor_border_off = 0; // black

	int recipe_selected = 0;
	int old_recipe_selected = 0;
 
	// UI sze
	int boxw = RECIPE_EXT_BORDER + RECIPE_BOX_WIDTH + RECIPE_EXT_BORDER;
	int boxh = RECIPE_EXT_BORDER + RECIPE_TITLE_HEIGHT + 
		(RECIPE_SELECT_HEIGHT + RECIPE_INT_BORDER) * MIN(RECIPE_NUM_SELECTIONS,NUM_ASM_PROCESSES) 
		- RECIPE_INT_BORDER + RECIPE_EXT_BORDER;

	vdp_select_sprite( CURSOR_SPRITE );
	vdp_hide_sprite();
	vdp_refresh_sprites();

	// dark-blue filled box with yellow line border
	draw_filled_box( offx, offy, boxw, boxh, 11, 16 );
	vdp_write_at_graphics_cursor();
	vdp_move_to( offx + RECIPE_EXT_BORDER, offy + RECIPE_EXT_BORDER);
	COL(2); printf("Select Recipe");
	// game loop for interacting with inventory
	clock_t key_wait_ticks = clock() + 20;
	bool finish = false;
	bool redisplay_cursor = true;
	bool redisplay_recipes = true;
	do {

		if ( redisplay_recipes )
		{
			redisplay_recipes = false;
			for (int j=0; j< MIN(RECIPE_NUM_SELECTIONS, NUM_ASM_PROCESSES); j++)
			{
				box_offsetsY[j] = offy + RECIPE_EXT_BORDER + RECIPE_TITLE_HEIGHT + 
					j*(RECIPE_SELECT_HEIGHT+RECIPE_INT_BORDER);

				// cells with dark grey fill with (cursor_border_off=black) border
				draw_filled_box(
						offx + RECIPE_EXT_BORDER,
						box_offsetsY[j],
						RECIPE_BOX_WIDTH, RECIPE_SELECT_HEIGHT, cursor_border_off, 8);

				vdp_gcol(0, 11);
				int xx = offx + RECIPE_EXT_BORDER + 2;
				int yy = box_offsetsY[j] + 2;

				ProcessType * processType = assemblerProcessTypes;
				if ( bGenerator )
				{
					processType = generatorProcessTypes;
				}
				for (int i=0; i < processType[j].innum; i++ )
				{
					int itemBMID = itemtypes[ processType[j].in[i] ].bmID;
					for (int k=0; k<processType[j].incnt[i]; k++)
					{
						vdp_adv_select_bitmap( itemBMID );
						vdp_draw_bitmap( xx, yy );
						xx += 10;
					}
					if ( i <  processType[j].innum -1 )
					{
						vdp_move_to( xx, yy ); printf("+");
						xx += 10;
					}
				}
				vdp_move_to( xx, yy ); printf("%c",CHAR_RIGHTARROW);
				xx += 10;
				int itemBMID = itemtypes[ processType[j].out ].bmID;
				if ( bGenerator )
				{
					vdp_move_to( xx, yy ); printf("%dE",processType[j].outcnt);
					xx += 10;
				} else {
					for (int k=0; k<processType[j].outcnt; k++)
					{
						vdp_adv_select_bitmap( itemBMID );
						vdp_draw_bitmap( xx, yy );
						xx += 10;
					}
				}
			}
		}
		if ( redisplay_cursor )
		{
			redisplay_cursor = false;
			// clear old cursor
			draw_box( 
				offx + RECIPE_EXT_BORDER, 
				box_offsetsY[old_recipe_selected], 
				RECIPE_BOX_WIDTH, 
				RECIPE_SELECT_HEIGHT,
				cursor_border_off);
			// cursor
			draw_box( 
				offx + RECIPE_EXT_BORDER, 
				box_offsetsY[recipe_selected], 
				RECIPE_BOX_WIDTH, 
				RECIPE_SELECT_HEIGHT,
				cursor_border_on);
			old_recipe_selected = recipe_selected;
		}

		if ( key_wait_ticks < clock() && 
				( vdp_check_key_press( KEY_x ) || vdp_check_key_press( KEY_enter ) ) )
		{
			finish=true;
			// delay otherwise x will cause exit from program
			while ( vdp_check_key_press( KEY_x ) || vdp_check_key_press( KEY_enter ) ) vdp_update_key_state();
		}

		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_UP ) )
		{
			key_wait_ticks = clock() + key_wait;
			if ( recipe_selected > 0 ) {
				recipe_selected--;
				redisplay_cursor = true;
			}
		}
		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_DOWN ) )
		{
			key_wait_ticks = clock() + key_wait;
			if ( recipe_selected < MIN(RECIPE_NUM_SELECTIONS, NUM_ASM_PROCESSES)-1 ) {
				recipe_selected++;
				redisplay_cursor = true;
			}
		}

		vdp_update_key_state();
	} while (finish==false);

	vdp_write_at_text_cursor();
	draw_screen();
	COL(15);

	vdp_select_sprite( CURSOR_SPRITE );
	vdp_show_sprite();
	vdp_refresh_sprites();

	return recipe_selected;
}
