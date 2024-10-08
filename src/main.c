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

extern uint8_t key_pressed_code;

#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3

#define DIR_OPP(D) ((D+2)%4)

//------------------------------------------------------------
int target_item_type = -1;
int target_item_count = 0;
int target_item_current = 0;
bool bWinMessageDisplayed = false;

#define MAX_MAPNAME_SIZE 40
typedef struct {
	char name[MAX_MAPNAME_SIZE];
	int target_item_type;
	int target_item_count;
	char desc[60];
} Maps;
int selected_map = 0;

//------------------------------------------------------------

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
#define _HUD_IMPLEMENTATION
#include "hud.h"

#include "message.h"

//#define SPLASH_EDIT
#ifdef SPLASH_EDIT
#define NUM_AVAILABLE_MAPS 3
#else
#define NUM_AVAILABLE_MAPS 2
#endif
Maps available_maps[NUM_AVAILABLE_MAPS] = {
	{
		"maps/m5res",
		IT_COMPUTER,
		10,
		"Small."
	},
	{
		"maps/m4mod",
		IT_ESCPOD,
		1,
		"Large."
	},
#ifdef SPLASH_EDIT
	{
		"maps/splash2",
		IT_COMPUTER,
		1,
		"Splash"
	},
#endif
};


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
#define SAVE_VERSION 2

#define MACH_NEED_ENERGY
#define CURSOR_RANGE

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
	char mapname[MAX_MAPNAME_SIZE];
	int resourceMultiplier;
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

// cursor position position in screen in pixels
int cursorx=0, cursory=0;

// cursor position in world in tile coords (0 --> mapsize)
int cursor_tx=0, cursor_ty=0;

// previous cursor position position in screen in pixels
int old_cursorx=0, old_cursory=0;
// previous cursor position in world in tile coords
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

// Inventory size
int inv_items_wide = 5;
int inv_items_high = 5;
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
int machine_update_rate = 15;

//------------------------------------------------------------
// Configuration vars
int belt_speed = 7;
int key_wait = 15;
int cursor_range = 4;
//------------------------------------------------------------

// counters
clock_t key_wait_ticks;
clock_t bob_anim_ticks;
clock_t move_wait_ticks;
clock_t layer_wait_ticks;

// FPS 
int frame_time_in_ticks;
clock_t ticks_start;
clock_t display_fps_ticks;

clock_t func_start;
int func_time[4] = { 0 };

// generator
bool bGenerating;
clock_t generate_timeout_ticks;
int generating_item;
int generating_item_count;

bool bIsTest = false;

bool bSoundEnabled = false;
bool bSoundSamplesLoaded = false;
int sound_volume = 40; // 0-127
bool bPlayingWalkSound = false;
#define SOUND_CHAN_STEPS 1
#define SOUND_CHAN_PICKAXE 2

bool bShowHud = true;
int hud_posx = 4;
int hud_posy = 4;

static volatile SYSVAR *sys_vars = NULL;

//------------------------------------------------------------
// Inline function defs
//------------------------------------------------------------
inline int getWorldCoordX(int sx) { return (fac.xpos + sx); }
inline int getWorldCoordY(int sy) { return (fac.ypos + sy); }
inline int getTileX(int sx) { return (sx/gTileSize); }
inline int getTileY(int sy) { return (sy/gTileSize); }
inline int getTilePosInScreenX(int tx) { return ((tx * gTileSize) - fac.xpos); }
inline int getTilePosInScreenY(int ty) { return ((ty * gTileSize) - fac.ypos); }

// machine byte is
// bit     7    6 5  4 3 2 1 0
//     valid outdir         ID   
inline bool isValid( int byte ) { return ( byte & 0x80 ) == 0; }
inline int getMachineItemType(uint8_t machine_byte) { return (machine_byte & 0x7) + IT_TYPES_MACHINE; }
inline int getOverlayAtOffset( int tileoffset ) { return (tilemap[tileoffset] & 0xF0) >> 4; }
inline int getOverlayAtCursor() { return (tilemap[cursor_ty*mapinfo.width +  cursor_tx] & 0xF0) >> 4; }

void wait()
{
	char k=getchar();
	if (k=='q') exit(0);
}

void change_mode(int mode)
{
	sys_vars->vdp_pflags = 0;
	vdp_mode(mode);
	while ( !(sys_vars->vdp_pflags & vdp_pflag_mode) );
}

//------------------------------------------------------------
// file handling functions 

#include "file_handling.h"

//------------------------------------------------------------
// Function defs
//------------------------------------------------------------
void load_custom_chars();

void game_loop();

void reshow_sprites();
void do_new_game();

int getMachineBMID(int tx, int ty);
int getMachineBMIDmh(MachineHeader *mh);
int getOverlayAtCursor();
int convertProductionToMachine( int it );
int convertMachineToProduction( int it );

void draw_tile(int tx, int ty, int tposx, int tposy);
void draw_machines(int tx, int ty, int tposx, int tposy, bool bDrawExtensionTile);
void draw_horizontal(int tx, int ty, int len);
void draw_vertical(int tx, int ty, int len);
void draw_screen();
void scroll_screen(int dir, int step);
bool can_scroll_screen(int dir, int step);

void draw_bob(int bx, int by, int px, int py);
bool check_tile(int px, int py);
bool move_bob(int dir, int speed);

void reset_cursor();
bool cursor_in_range();
void start_place();
void stop_place();
void draw_cursor(bool draw);
void draw_place_belt();
void draw_place_resource();
void draw_place_machine();
void do_place();

bool itemIsOnScreen(ItemNodePtr itemptr);
bool itemIsInHorizontal(ItemNodePtr itemptr, int tx, int ty, int len);
bool itemIsInRows(ItemNodePtr itemptr, int from_row, int to_row);
void draw_layer(bool draw_items);
void draw_horizontal_layer(int tx, int ty, int len, bool bdraw_belts, bool bdraw_machines, bool bdraw_items);
void get_belt_neighbours(BELT_PLACE *bn, int tx, int ty);
void drop_item(int item);
bool move_item_on_belt(ItemNodePtr currPtr, int in, int out, bool *predraw);
void move_items(bool bDraw);
void inserters_pull_from_box();
void move_items_on_inserters(bool bDraw);
void move_items_on_machines(bool bDraw);
void draw_items();
void draw_items_at_tile(int tx, int ty);

void draw_digit(int i, int px, int py);
void draw_number(int n, int px, int py);
void draw_number_lj(int n, int px, int py);

void show_info();
bool check_can_mine();
int getResourceCount(int tx, int ty);
ItemNodePtr getResource(int tx, int ty);
bool reduceResourceCount(int tx, int ty);
void do_mining();
void removeAtCursor();
void pickupItemsAtTile(int tx, int ty);
void insertItemIntoMachine(int machine_type, int tx, int ty, int item );
void check_items_on_machines();

void show_inventory(int X, int Y);
bool show_filedialog(bool bLoadOnly);
int show_recipe_dialog(int machine_type, bool bManual);
int show_item_dialog();
void select_game_map();

void show_help(bool bShowWin);

bool splash_loop();

bool load_sound_samples(int vert_pos);
void walking_sound_enable( bool enable );


#define _MESSAGE_IMPLEMENTATION
#include "message.h"

// redefine "~" character as arrow
#define CHAR_RIGHTARROW 0x7E
void load_custom_chars()
{
	// right arrow
	vdp_redefine_character( CHAR_RIGHTARROW, 0x00, 0x04, 0x02, 0xFF, 0x02, 0x04, 0x00, 0x00); // right arrow
}

//------------------------------------------------------------
// MAIN
//------------------------------------------------------------
int main(int argc, char *argv[])
{
	vdp_vdu_init();
	sys_vars = (SYSVAR *)mos_sysvars();

	if ( vdp_key_init() == -1 ) return 1;
	vdp_set_key_event_handler( key_event_handler );

	bIsTest = false;
	if (argc > 1)
	{
		if (strcmp("test",argv[1]) == 0)
		{
			bIsTest = true;
		}
	}
	fac.version = SAVE_VERSION;
	fac.resourceMultiplier = 16; // default

	load_custom_chars();

	if ( ! check_dir_exists("maps") )
	{
		printf("No maps directory\n");
		goto my_exit2;
	}

	if ( !load_game_map("maps/splash2") )
	{
		goto my_exit2;
	}

	fac.energy = 0;
	bGenerating = false;

	// setup complete
	change_mode(gMode);
	vdp_cursor_enable( false );
	vdp_logical_scr_dims( false );

	// load all the bitmaps and sprites
	if ( ! load_images(true, 2) )
	{
		printf("Failed to load images\n");
		goto my_exit2;
	}

	// sound samples
	bSoundSamplesLoaded = load_sound_samples( 8 );
	if (!bSoundSamplesLoaded)
	{
		printf("\nFailed to load sound samples\n");
	} else {
		bSoundEnabled = true;
	}

	create_sprites(16);

	create_hud(19);

	inv_selected = 0; // belts
	item_selected = 0; // belts

	vdp_activate_sprites(0);
	
	load_game("maps/splash_save.data", true);

	// default map/target after the splash screen
	selected_map = 0;
	target_item_type = available_maps[ selected_map ].target_item_type;
	target_item_count = available_maps[ selected_map ].target_item_count;

	if (splash_loop())
	{
		do_new_game();
	}

	game_loop();

	change_mode(0);
my_exit2:
	free(tilemap);
	vdp_logical_scr_dims( true );
	vdp_cursor_enable( true );
	return 0;
}

//------------------------------------------------------------
// CENTRAL GAME LOOP
//------------------------------------------------------------

void game_loop()
{
	int loopexit=0;

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
		bool bUpdated=false;
		bool bMoved=false;
		ticks_start = clock();

		vdp_update_key_state();

		if ( vdp_check_key_press( KEY_w ) ) { bob_dir = BOB_UP; dir=SCROLL_UP; }
		if ( vdp_check_key_press( KEY_a ) ) { bob_dir = BOB_LEFT; dir=SCROLL_RIGHT; }
		if ( vdp_check_key_press( KEY_s ) ) { bob_dir = BOB_DOWN; dir=SCROLL_DOWN; }
		if ( vdp_check_key_press( KEY_d ) ) { bob_dir = BOB_RIGHT; dir=SCROLL_LEFT; }

		// scroll the screen AND/OR move Bob
		if ( dir>=0 && ( move_wait_ticks < clock() ) ) {
			move_wait_ticks = clock()+1;

			int run_mult = 1;
			int tileoffset = getTileX(fac.bobx+7) + getTileY(fac.boby+7)*mapinfo.width;
			if ( (tilemap[tileoffset] & 0x0F) == 13)
			{
				run_mult = 2;
			}
			int move_amount = run_mult;
			if (frame_time_in_ticks>=4) move_amount=2*run_mult;
			if (frame_time_in_ticks>=8) move_amount=4*run_mult;
			if (frame_time_in_ticks>=16) move_amount=8*run_mult;
			
			// screen can scroll, move Bob AND screen
			if ( can_scroll_screen(dir, move_amount) )
			{
				if ( move_bob(bob_dir, move_amount) )
				{
					if (debug) // clear the data at the bottom of the screen
					{
						int tx=getTileX(fac.xpos);
						int ty=getTileY(fac.ypos + gScreenHeight -1);
						draw_horizontal(tx,ty-1,6);
						draw_horizontal(tx,ty,6);
					}

					scroll_screen(dir,move_amount);
					draw_cursor(true); // re-draw cursor
					//bMoved = true;
				}
			}
			// can't scroll screen, just move Bob around
			else
			{
				move_bob(bob_dir, move_amount);
			}
			bUpdated = true;
		}
		if ( bSoundEnabled && 
		     !vdp_check_key_press( KEY_w ) && 
		     !vdp_check_key_press( KEY_a ) && 
		     !vdp_check_key_press( KEY_s ) && 
		     !vdp_check_key_press( KEY_d ) )
		{
			walking_sound_enable( false );
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
			bUpdated = true;
		}

		if (layer_wait_ticks < clock()) 
		{
			layer_wait_ticks = clock() + belt_speed; // belt anim speed
			//draw_cursor(false);
			move_items(false);
			inserters_pull_from_box();
			move_items_on_inserters(false);
			move_items_on_machines(false);
			check_items_on_machines();
			draw_layer(true);
			draw_cursor(true);
			bUpdated = true;
		}

		if (debug)
		{
			COL(15);COL(128);
			//TAB(0,29); printf("%d %d  ",frame_time_in_ticks,func_time[0]);
			TAB(0,29); printf("%d %d i:%d E:%d  ",frame_time_in_ticks,func_time[0], numItems, fac.energy);
		}
			
		if ( vdp_check_key_press( KEY_p ) ) { // do place - get ready to place an item from inventory
			if (key_wait_ticks < clock() && cursor_in_range() ) 
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
			key_wait_ticks = clock() + key_wait;
			stop_place(); 
			while ( vdp_check_key_press(KEY_q) );
		}

		vdp_update_key_state();
		if ( bPlace && vdp_check_key_press(KEY_r) ) {
			while ( vdp_check_key_press(KEY_r) );

			draw_cursor(false);
			place_belt_index++;  
			place_belt_index = place_belt_index % 4;
			draw_cursor(true);
		}
		if ( bPlace && vdp_check_key_press(KEY_t) ) {
			while ( vdp_check_key_press(KEY_t) );

			draw_cursor(false);
			place_belt_index--;  
			place_belt_index = (place_belt_index+4) % 4;
			draw_cursor(true);
		}

		if (  vdp_check_key_press( KEY_enter ) && cursor_in_range()  ) // ENTER - start placement state
		{
			walking_sound_enable( false );
			do_place();
			while ( vdp_check_key_press(KEY_enter) );
		}

		if ( vdp_check_key_press( KEY_delete ) && cursor_in_range()  ) // DELETE - delete item at cursor
		{
			removeAtCursor();
			bUpdated = true;
			while ( vdp_check_key_press(KEY_delete) );
		}


		if ( vdp_check_key_press( KEY_backtick ) )  // ' - toggle debug
		{
			debug = !debug;
			while ( vdp_check_key_press( KEY_backtick ) );
			draw_screen();
			bUpdated = true;
		}

		if ( vdp_check_key_press( KEY_x ) || vdp_check_key_press( KEY_escape ) ) { // x - exit
			walking_sound_enable( false );
			vdp_activate_sprites(0);
			draw_filled_box( 70, 84, 180, 30, 11, 0 );
			COL(128+0);COL(15);TAB(10,12);printf("EXIT: Are you sure?");
			char k=getchar(); 
			if (k=='y' || k=='Y') loopexit=1;
			draw_screen();
			vdp_activate_sprites( NUM_SPRITES );
			vdp_select_sprite( CURSOR_SPRITE );
			vdp_show_sprite();
			vdp_refresh_sprites();
		}

		if ( vdp_check_key_press( KEY_e ) ) // Bring up inventory
		{
			walking_sound_enable( false );
			show_inventory(12,12);
			while ( vdp_check_key_press(KEY_e) );
		}
	
		if ( vdp_check_key_press( KEY_i ) ) // i for item info
		{
			walking_sound_enable( false );
			show_info();
			while ( vdp_check_key_press(KEY_i) );
		}

		if ( vdp_check_key_press( KEY_m ) )  // m for MINE
		{
			if (key_wait_ticks < clock() && cursor_in_range() ) 
			{
				key_wait_ticks = clock() + key_wait;
				if ( ! bIsMining )
				{
					if ( check_can_mine() )
					{
						mining_time_ticks = clock() + mining_time;;
						bob_mining_anim_ticks = clock() + bob_mining_anim_time;;
						bIsMining = true;
						select_bob_sprite( BOB_SPRITE_ACT_DOWN+bob_facing );
						vdp_move_sprite_to( fac.bobx - fac.xpos, fac.boby - fac.ypos );
						vdp_refresh_sprites();
						if ( bSoundEnabled )
						{
							vdp_audio_sample_seek( SOUND_CHAN_PICKAXE, 0 );
							vdp_audio_set_volume( SOUND_CHAN_PICKAXE, sound_volume );
						}
					} else {
						new_message(&messageinfo, "can't mine", -1, 100, 25);
					}
					bUpdated = true;
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
				bUpdated = true;
			}
		  	if ( mining_time_ticks < clock() )
			{
				bIsMining = false;
				if ( bSoundEnabled )
				{
					vdp_audio_set_volume( SOUND_CHAN_PICKAXE, 0 );
				}
				do_mining();
				select_bob_sprite( bob_facing );
				bUpdated = true;
			}
		}

		vdp_update_key_state();
		if ( vdp_check_key_press( KEY_z ) && cursor_in_range()  )  // z - pick-up items under cursor
		{
			pickupItemsAtTile(cursor_tx, cursor_ty);	
			draw_screen();
			bUpdated = true;
			while ( vdp_check_key_press(KEY_z) );
		}

		if ( vdp_check_key_press( KEY_g ) ) // g - generate
		{
			walking_sound_enable( false );
			int recipe = show_recipe_dialog(IT_ASSEMBLER, true);
			if ( recipe >= 0 )
			{
				if (assemblerProcessTypes[recipe].manual)
				{
					bool bInputsSatisfied = true;
					for (int i=0; i<assemblerProcessTypes[recipe].innum;i++)
					{
						int index = inventory_find_item(inventory, assemblerProcessTypes[recipe].in[i]);
						if (index >= 0)
						{
							if ( inventory[index].count < assemblerProcessTypes[recipe].incnt[i] )
							{
								bInputsSatisfied = false;
								break;
							}
						} else {
							bInputsSatisfied = false;
							break;
						}
					}
					if ( bInputsSatisfied )
					{
						for (int i=0; i<assemblerProcessTypes[recipe].innum;i++)
						{
							inventory_remove_item(inventory, assemblerProcessTypes[recipe].in[i], assemblerProcessTypes[recipe].incnt[i]);
						}
						generating_item = assemblerProcessTypes[recipe].out;
						generating_item = convertProductionToMachine(generating_item);
						generating_item_count = assemblerProcessTypes[recipe].outcnt;

						char buf[20]; snprintf(buf,20,"generate +%d",generating_item_count);
						new_message(&messageinfo, buf, generating_item, 290, 17);
						generate_timeout_ticks = clock() + 300;
						bGenerating = true;
					} else {
						new_message(&messageinfo, "Inputs?", -1, 150,25);
					}
				} else {
					new_message(&messageinfo, "Need assembler", -1, 150,25);
				}
			}
			key_wait_ticks = clock() + key_wait;
			bUpdated = true;
		}

		if (bGenerating && generate_timeout_ticks < clock() )
		{
			inventory_add_item( inventory,  generating_item, generating_item_count );
			new_message(&messageinfo, "done", -1, 80,17);
			bGenerating = false;
			bUpdated = true;
		}

		// only animate machines if there is active energy
		if ( 
#if defined MACH_NEED_ENERGY
				fac.energy > 0 &&  
#endif
				machine_anim_timeout_ticks < clock() )
		{
			machine_anim_timeout_ticks = clock() + machine_anim_speed;
			machine_frame = (machine_frame +1) % 3;
		}

		if ( vdp_check_key_press( KEY_f ) ) // file dialog
		{
			walking_sound_enable( false );
			show_filedialog( false ); // load or save
			key_wait_ticks = clock() + key_wait;
		}

		if ( vdp_check_key_press( KEY_h ) ) // help dialog
		{
			walking_sound_enable( false );
			while ( vdp_check_key_press( KEY_h ) );
			show_help(true);
		}

		if ( vdp_check_key_press( KEY_l ) ) // refresh screen
		{
			while ( vdp_check_key_press( KEY_l ) );
			draw_screen();
			bUpdated = true;
		}

		if ( vdp_check_key_press( KEY_c ) ) // recentre cursor on Bob
		{
			stop_place(); 
			while ( vdp_check_key_press( KEY_c ) );
			reset_cursor();
		}

		if ( vdp_check_key_press( KEY_f9 ) )
		{
			while ( vdp_check_key_press( KEY_f9 ) );
			if (bSoundSamplesLoaded)
			{
				bSoundEnabled = !bSoundEnabled;
				char buf[12];
				sprintf(buf,"Sound %s",bSoundEnabled?"On ":"Off");
				new_message(&messageinfo, buf, -1, 100, 16);
			}
		}
		if ( vdp_check_key_press( KEY_f7 ) )
		{
			while ( vdp_check_key_press( KEY_f7 ) );
			if (bSoundSamplesLoaded && bSoundEnabled)
			{
				sound_volume -= 10;
				sound_volume = MAX(sound_volume, 10);
				char buf[12];
				sprintf(buf,"Volume %d",sound_volume);
				new_message(&messageinfo, buf, -1, 60, 16);
			}
		}
		if ( vdp_check_key_press( KEY_f8 ) )
		{
			while ( vdp_check_key_press( KEY_f8 ) );
			if (bSoundSamplesLoaded && bSoundEnabled)
			{
				sound_volume += 10;
				sound_volume = MIN(sound_volume, 127);
				char buf[12];
				sprintf(buf,"Volume %d",sound_volume);
				new_message(&messageinfo, buf, -1, 60, 16);
			}
		}

		if ( vdp_check_key_press( KEY_f2 ) )
		{
			while ( vdp_check_key_press( KEY_f2 ) );
			if ( bShowHud )
			{
				bShowHud = false;
				hide_hud();
			} else {
				bShowHud = true;
				show_hud(hud_posx, hud_posy);
			}
		}

		if ( vdp_check_key_press( KEY_n ) ) // new game
		{
			while ( vdp_check_key_press( KEY_n ) );
			// are you sure
			walking_sound_enable( false );
			vdp_activate_sprites(0);
			draw_filled_box( 70, 84, 200, 30, 11, 17 );
			COL(128+17);COL(15);TAB(10,12);printf("NEW GAME: Are you sure?");
			char k=getchar(); 
			if (k=='y' || k=='Y') {
				// select game map dialog
				select_game_map();
				// change to new map
				do_new_game();
			}
			COL(128);COL(15);
			// refresh screen
			draw_screen();
			vdp_activate_sprites( NUM_SPRITES );
			vdp_select_sprite( CURSOR_SPRITE );
			vdp_show_sprite();
			vdp_refresh_sprites();
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

				// possible to reach energy < 0 due to simultaneous updates?
				if ( fac.energy < 0 ) fac.energy = 0;

				pushOutputs( mach );

				tlistp = tlistp->next;
			}

			if ( !bWinMessageDisplayed && target_item_count == target_item_current )
			{
				vdp_activate_sprites(0);
				draw_filled_box( 70, 84, 180, 30, 11, 1 );
				COL(128+1);COL(15);TAB(10,12);printf("  !!! You Win !!! ");
				bWinMessageDisplayed = true;
				delay(500);
				wait_for_any_key();
				draw_screen();
				vdp_activate_sprites( NUM_SPRITES );
				vdp_select_sprite( CURSOR_SPRITE );
				vdp_show_sprite();
				vdp_refresh_sprites();
			}
			bUpdated = true;

			if (bShowHud) hud_update_count(fac.energy);
			vdp_refresh_sprites();
		}
		
		if (bUpdated && bMessage)
		{
			// clear messages when they timeout
			if ( messageinfo.timeout_ticks < clock() )
			{
				message_clear( &messageinfo );
				bMessage = false;
			} else {
				if (bMoved) message_clear( &messageinfo );
				display_message( &messageinfo );
			}
		}

		frame_time_in_ticks = clock() - ticks_start;
	} while (loopexit==0);

}


//------------------------------------------------------------
// new game
//------------------------------------------------------------
void reshow_sprites()
{
	reset_cursor();

	if (bShowHud) hud_update_count(fac.energy);

	// Turn on sprites and move bob sprite to his set location
	vdp_activate_sprites( NUM_SPRITES );
	vdp_select_sprite( CURSOR_SPRITE );
	vdp_show_sprite();
	select_bob_sprite( bob_facing );
	vdp_move_sprite_to( fac.bobx - fac.xpos, fac.boby - fac.ypos );

	if (bShowHud) show_hud(hud_posx, hud_posy);

	vdp_refresh_sprites();
}
void do_new_game()
{
	clear_map_and_lists();
	load_game_map( available_maps[selected_map].name );

	/* start bob and screen centred in map */
	fac.bobx = (mapinfo.width * gTileSize / 2) & 0xFFFFF0;
	fac.boby = (mapinfo.height * gTileSize / 2) & 0xFFFFF0;
	fac.xpos = fac.bobx - gScreenWidth/2;
	fac.ypos = fac.boby - gScreenHeight/2;

	fac.energy=0;

	reshow_sprites();

	// Add some default items 
	inventory_init(inventory);
	// for test add some items
	inventory_add_item(inventory, IT_BELT, 16); // belt
	inventory_add_item(inventory, IT_GENERATOR, 1);
	if (bIsTest)
	{
		inventory_add_item(inventory, IT_BELT, 100);
		inventory_add_item(inventory, IT_STONE, 80);
		inventory_add_item(inventory, IT_WOOD, 80);
		inventory_add_item(inventory, IT_COAL, 100);
		inventory_add_item(inventory, IT_FURNACE, 40);
		inventory_add_item(inventory, IT_IRON_PLATE, 60);
		inventory_add_item(inventory, IT_COPPER_PLATE, 60);
		inventory_add_item(inventory, IT_ASSEMBLER, 50);
		inventory_add_item(inventory, IT_GEARWHEEL, 60);
		inventory_add_item(inventory, IT_CIRCUIT, 60);
		inventory_add_item(inventory, IT_WIRE, 60);
		inventory_add_item(inventory, IT_STONE_BRICK, 40);
		inventory_add_item(inventory, IT_MINER, 40);
		inventory_add_item(inventory, IT_INSERTER, 40);
		inventory_add_item(inventory, IT_BOX, 40);
		inventory_add_item(inventory, IT_TSPLITTER, 40);
		fac.energy = 1000;
		if (bShowHud) hud_update_count(fac.energy);
	}

}

//------------------------------------------------------------
// Bitmap lookup  Functions
//------------------------------------------------------------
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
	} else
	if ( machine_itemtype == IT_TSPLITTER)
	{
		Machine *mach = (Machine*) mh;
		int bmid = BMOFF_TSPLIT + 2*(mach->dir);
		bmid += mach->ptype; // ptype holds the switch state for a splitter

		return bmid;
	} else {
		return itemtypes[machine_itemtype].bmID;
	}
}

int convertProductionToMachine( int it )
{
	if (it >= IT_TYPES_MINI_MACHINES && it < NUM_ITEMTYPES)
		it = it - IT_TYPES_MINI_MACHINES + IT_TYPES_MACHINE;
	if (it == IT_MINI_BELT)
		it = IT_BELT;
	return it;
}
int convertMachineToProduction( int it )
{
	if (it >= IT_TYPES_MACHINE && it < IT_TYPES_FEATURES)
		it = it - IT_TYPES_MACHINE + IT_TYPES_MINI_MACHINES;
	if (it == IT_BELT)
		it = IT_MINI_BELT;
	return it;
}


//------------------------------------------------------------
// Draw tile, horizontals and screen
//------------------------------------------------------------

// draws terrain and features at a tile
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
void draw_machines(int tx, int ty, int tposx, int tposy, bool bDrawExtensionTile)
{
	int tileoffset = ty*mapinfo.width + tx;
	MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
	if (mh && mh->machine_type > 0)
	{
		int bmid = getMachineBMIDmh(mh);
		vdp_adv_select_bitmap( bmid );
		vdp_draw_bitmap( tposx, tposy );
#ifdef MACH_NEED_ENERGY
		if (fac.energy <= 0 && ( mh->machine_type < IT_INSERTER ))
		{
			vdp_adv_select_bitmap( BMOFF_ZAP );
			vdp_draw_bitmap( tposx, tposy );
		}
#endif
		if (bDrawExtensionTile)
		{
			if ( mh->machine_type < IT_INSERTER ) // items can move out of these
			{
				// get machine object: holds end (output) tile
				Machine *mach = (Machine*) objectmap[tileoffset];
				int px = getTilePosInScreenX(mach->end_tx);
				int py = getTilePosInScreenY(mach->end_ty);

				// new tile offset for end of machine
				int end_tileoffset = mach->end_ty*mapinfo.width + mach->end_tx;

				// draw the terrain and overlay resource
				draw_tile(mach->end_tx, mach->end_ty, px, py);

				// draw any belts at that tile
				if (  layer_belts[end_tileoffset] >= 0 )
				{
					vdp_adv_select_bitmap( layer_belts[end_tileoffset]*4 + BMOFF_BELT16 + belt_layer_frame);
					vdp_draw_bitmap( px, py );
				}
				// draw any machines at the output tile
				MachineHeader *mh_end = (MachineHeader*) objectmap[end_tileoffset];
				if (mh_end && mh_end->machine_type > 0)
				{
					int bmid = getMachineBMIDmh(mh_end);
					vdp_adv_select_bitmap( bmid );
					vdp_draw_bitmap( px, py );
#ifdef MACH_NEED_ENERGY
					if (fac.energy <= 0 && ( mh_end->machine_type < IT_INSERTER ))
					{
						vdp_adv_select_bitmap( BMOFF_ZAP );
						vdp_draw_bitmap( px, py );
					}
#endif
				}
				// draw items on the output tile
				draw_items_at_tile(mach->end_tx, mach->end_ty);
			}

			// Draw input and output tiles of inserter
			if ( mh->machine_type == IT_INSERTER ) // items can move in and out of these
			{
				// get inserter object
				Inserter *insp = (Inserter*) objectmap[tileoffset];
				int start_px = getTilePosInScreenX(insp->start_tx);
				int start_py = getTilePosInScreenY(insp->start_ty);
				int end_px = getTilePosInScreenX(insp->end_tx);
				int end_py = getTilePosInScreenY(insp->end_ty);

				// new tile offset for start and end of machine
				int start_tileoffset = insp->start_ty*mapinfo.width + insp->start_tx;
				int end_tileoffset = insp->end_ty*mapinfo.width + insp->end_tx;

				// draw the terrain and overlay resource
				draw_tile(insp->start_tx, insp->start_ty, start_px, start_py);
				draw_tile(insp->end_tx, insp->end_ty, end_px, end_py);

				// draw any belts at those tiles
				if (  layer_belts[start_tileoffset] >= 0 )
				{
					vdp_adv_select_bitmap( layer_belts[start_tileoffset]*4 + BMOFF_BELT16 + belt_layer_frame);
					vdp_draw_bitmap( start_px, start_py );
				}
				if (  layer_belts[end_tileoffset] >= 0 )
				{
					vdp_adv_select_bitmap( layer_belts[end_tileoffset]*4 + BMOFF_BELT16 + belt_layer_frame);
					vdp_draw_bitmap( end_px, end_py );
				}
				// draw any machines at those tiles
				MachineHeader *mh_start = (MachineHeader*) objectmap[start_tileoffset];
				MachineHeader *mh_end = (MachineHeader*) objectmap[end_tileoffset];
				if (mh_start && mh_start->machine_type > 0)
				{
					int bmid = getMachineBMIDmh(mh_start);
					vdp_adv_select_bitmap( bmid );
					vdp_draw_bitmap( start_px, start_py );
#ifdef MACH_NEED_ENERGY
					if (fac.energy <= 0 && ( mh_start->machine_type < IT_INSERTER ))
					{
						vdp_adv_select_bitmap( BMOFF_ZAP );
						vdp_draw_bitmap( start_px, start_py );
					}
#endif
				}
				if (mh_end && mh_end->machine_type > 0)
				{
					int bmid = getMachineBMIDmh(mh_end);
					vdp_adv_select_bitmap( bmid );
					vdp_draw_bitmap( end_px, end_py );
#ifdef MACH_NEED_ENERGY
					if (fac.energy <= 0 && ( mh_end->machine_type < IT_INSERTER ))
					{
						vdp_adv_select_bitmap( BMOFF_ZAP );
						vdp_draw_bitmap( end_px, end_py );
					}
#endif
				}

				// draw items on the input and output tiles
				draw_items_at_tile(insp->start_tx, insp->start_ty);
				draw_items_at_tile(insp->end_tx, insp->end_ty);
			}
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
		draw_machines(tx + i, ty, px + i*gTileSize, py, false); 
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
		draw_machines(tx, ty + i, px, py + i*gTileSize, false);
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
				int tx=getTileX(fac.xpos);
				int ty=getTileY(fac.ypos);
				fac.xpos -= step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx,ty+len)
				draw_vertical(tx,ty, 1+(gScreenHeight/gTileSize));
			}
			break;
		case SCROLL_LEFT: // scroll screen to left, view moves right
			if ((fac.xpos + gScreenWidth + step) < (mapinfo.width * gTileSize))
			{
				int tx=getTileX(fac.xpos + gScreenWidth -1);
				int ty=getTileY(fac.ypos);
				fac.xpos += step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx,ty+len)
				draw_vertical(tx,ty, 1+(gScreenHeight/gTileSize));
			}
			break;
		case SCROLL_UP:
			if (fac.ypos > step)
			{
				int tx=getTileX(fac.xpos);
				int ty=getTileY(fac.ypos);
				fac.ypos -= step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx+len,ty)
				draw_horizontal(tx,ty, 1+(gScreenWidth/gTileSize));
			}
			break;
		case SCROLL_DOWN:
			if ((fac.ypos + gScreenHeight + step) < (mapinfo.height * gTileSize))
			{
				int tx=getTileX(fac.xpos);
				int ty=getTileY(fac.ypos + gScreenHeight -1);
				fac.ypos += step;
				vdp_scroll_screen(dir, step);
				// draw tiles (tx,ty) to (tx+len,ty)
				draw_horizontal(tx,ty, 1+(gScreenWidth/gTileSize));
			}
			break;
		default:
			break;
	}
}

bool can_scroll_screen(int dir, int step)
{
	// let Bob return to centre if he has moved away
	int btx = (fac.bobx - fac.xpos)/gTileSize;
	int bty = (fac.boby - fac.ypos)/gTileSize;
	int cx = (gScreenWidth/2)/gTileSize;
	int cy = (gScreenHeight/2)/gTileSize;

	if (dir == SCROLL_RIGHT && btx > cx) { return false; }
	if (dir == SCROLL_LEFT  && btx < cx) { return false; }
	if (dir == SCROLL_UP    && bty > cy) { return false; }
	if (dir == SCROLL_DOWN  && bty < cy) { return false; }

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

//------------------------------------------------------------
// Character movement handling
//------------------------------------------------------------

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
					&& check_tile(fac.bobx-speed, fac.boby+2)
					&& check_tile(fac.bobx-speed, fac.boby+gTileSize-3)
					) {
				newx -= speed;
			}
			break;
		case BOB_RIGHT:
			if (fac.bobx < mapinfo.width*gTileSize - speed 
					&& check_tile(fac.bobx+speed+gTileSize-1, fac.boby+2)
					&& check_tile(fac.bobx+speed+gTileSize-1, fac.boby+gTileSize-3)
						) {
				newx += speed;
			}
			break;
		case BOB_UP:
			if (fac.boby > speed 
					&& check_tile(fac.bobx+2,           fac.boby-speed)
					&& check_tile(fac.bobx+gTileSize-3, fac.boby-speed)
					) {
				newy -= speed;
			}
			break;
		case BOB_DOWN:
			if (fac.boby < mapinfo.height*gTileSize - speed 
				&& check_tile(fac.bobx+2,           fac.boby+speed+gTileSize-1)
				&& check_tile(fac.bobx+gTileSize-3, fac.boby+speed+gTileSize-1)
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

	if (moved) {
		if ( bSoundEnabled && !bPlayingWalkSound ) {
			walking_sound_enable( true );
		}
	}

	if (bob_anim_ticks > clock() ) return moved;
	bob_frame=(bob_frame+1)%4; 
	vdp_nth_sprite_frame( bob_frame );
	bob_anim_ticks = clock()+10;
	vdp_refresh_sprites();
	
	return moved;
}


//------------------------------------------------------------
// Cursor movement and place selection
//------------------------------------------------------------

void reset_cursor()
{
	cursor_tx = getTileX(fac.bobx+gTileSize/2);
	cursor_ty = getTileY(fac.boby+gTileSize/2);
	cursorx = getTilePosInScreenX(cursor_tx);
	cursory = getTilePosInScreenY(cursor_ty);
	old_cursorx=cursorx;
	old_cursory=cursory;
	oldcursor_tx = cursor_tx;
	oldcursor_ty = cursor_ty;
}

bool cursor_in_range()
{
#ifndef CURSOR_RANGE
	return true;
#else
	int bob_tx = getTileX(fac.bobx+gTileSize/2);
	int bob_ty = getTileY(fac.boby+gTileSize/2)+1;
	if ( cursor_tx >= ( bob_tx - cursor_range ) && cursor_tx <= ( bob_tx + cursor_range ) && 
	     cursor_ty >= ( bob_ty -1 - cursor_range ) && cursor_ty <= ( bob_ty -1 + cursor_range ) )
	{
		return true;
	}
	return false;
#endif
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
	} else
	if ( item_selected == IT_TSPLITTER )
	{
		vdp_adv_select_bitmap( BMOFF_TSPLIT_ICON + place_belt_index );
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

	int tileoffset = mapinfo.width * cursor_ty + cursor_tx;
	MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
	int machine_itemtype = -1;
	if (mh && mh->machine_type > 0)
	{
		machine_itemtype = mh->machine_type;
	}

	if (bPlace) // placement cursor is active
	{
		if ( isBelt( item_selected ) )
		{
			if (layer_belts[ cursor_tx+cursor_ty*mapinfo.width ] >= 0 || machine_itemtype==IT_ASSEMBLER)
			{
				vdp_adv_select_bitmap( itemtypes[IT_MINI_BELT].bmID );
				vdp_draw_bitmap( 
						cursorx + itemtypes[IT_MINI_BELT].size*4, 
						cursory + itemtypes[IT_MINI_BELT].size*4);
			} else {
				draw_place_belt();
			}
		}
		if ( isResource( item_selected ) || isProduct( item_selected ) )
		{
			draw_place_resource();
		}
		if ( isMachine( item_selected ) )
		{
			if (layer_belts[ cursor_tx+cursor_ty*mapinfo.width ] >= 0 || machine_itemtype==IT_ASSEMBLER)
			{
				int it = convertMachineToProduction( item_selected );
				vdp_adv_select_bitmap( itemtypes[it].bmID );
				vdp_draw_bitmap( 
						cursorx + itemtypes[it].size*4, 
						cursory + itemtypes[it].size*4);
			} else {
				draw_place_machine();
			}
		}
		
		vdp_select_sprite( CURSOR_SPRITE );
		vdp_move_sprite_to( cursorx, cursory );
		int frame=1;
#ifdef CURSOR_RANGE
		if (!cursor_in_range()) frame=3;
#endif
		vdp_nth_sprite_frame( frame );
	}
	else
	{
		vdp_select_sprite( CURSOR_SPRITE );
		vdp_move_sprite_to( cursorx, cursory );
		int frame=0;
#ifdef CURSOR_RANGE
		if (!cursor_in_range()) frame=2;
#endif
		vdp_nth_sprite_frame( frame );
	}
	vdp_refresh_sprites();

	old_cursorx=cursorx;
	old_cursory=cursory;
	oldcursor_tx = cursor_tx;
	oldcursor_ty = cursor_ty;
}

// drop what is in the placement cursor. 
void do_place()
{
	if ( !bPlace ) return;
	int tileoffset = mapinfo.width * cursor_ty + cursor_tx;

	MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
	int machine_itemtype = -1;
	if (mh && mh->machine_type > 0)
	{
		// there is a machine where we are dropping
		machine_itemtype = mh->machine_type;
	}

	int beltID = layer_belts[ tileoffset ];

	// dropping a belt
   	if ( isBelt(item_selected) ) {
		if ( place_belt_selected<0 ) return;

		int ret = inventory_remove_item( inventory, IT_BELT, 1 );
		if ( ret >= 0 )
		{
			// dropping a belt into a machine - use the mini-belt
			if ( machine_itemtype == IT_ASSEMBLER )
			{
				drop_item(IT_MINI_BELT);
			} else {
				// same for dropping onto a belt
				if (beltID>=0)
				{
					drop_item(IT_MINI_BELT);
				} else {
					// don't place on water
					if ( (tilemap[tileoffset] & 0x0F) != 0 )
					{
						// otherwise, just place the actual belt
						layer_belts[tileoffset] = place_belt_selected;
					}
				}
			}
		}
	}
	// dropping a resource or a product
	if ( isResource(item_selected) || isProduct(item_selected) ) {
		bool placed=false;
		// paving - these are placed as terrain
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
		// all others are just dtopped
		if (!placed)
		{
			int ret = inventory_remove_item( inventory, item_selected, 1 );
			if ( ret >= 0 )
			{
				drop_item(item_selected);
			}
		}
	}
	// dropping a machine
	if ( isMachine(item_selected) ) {
		int mach_mini_it = convertMachineToProduction(item_selected);
		int mach_maxi_it = convertProductionToMachine(item_selected);
		if ( beltID >= 0 )
		{
			if (inventory_remove_item( inventory, item_selected, 1 ) >= 0)
			{
				// place the 8x8 resource item in the centre of the square
				insertAtFrontItemList(&itemlist, mach_mini_it, cursor_tx*gTileSize + 4, cursor_ty*gTileSize + 4);
				numItems++;
			}
		} else
		// dropping a machine into an assembler?
		if (machine_itemtype == IT_ASSEMBLER) 
		{
			if (inventory_remove_item( inventory, item_selected, 1 ) >= 0)
			{
				insertItemIntoMachine( machine_itemtype, cursor_tx, cursor_ty, mach_maxi_it );
				numItems++;
			}
		} else
		// drop the machine. In most cases this means creating the machine.
		// - make sure there isn't anything already there
		if ( objectmap[tileoffset]==NULL )
		{
			// miners can only be placed where there is an overlay resource
			if ( item_selected == IT_MINER )
			{
				uint8_t overlay = getOverlayAtCursor();
				if ( overlay > 0 )
				{
					if (inventory_remove_item( inventory, item_selected, 1 ) >= 0)
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
				int recipe = show_recipe_dialog(IT_FURNACE, false);
				if (inventory_remove_item( inventory, item_selected, 1 ) >= 0)
				{
					Machine *mach = addFurnace( &machinelist, cursor_tx, cursor_ty, place_belt_index, recipe );
					objectmap[tileoffset] = mach;
				}
			} else 
			if ( item_selected == IT_ASSEMBLER )
			{
				int recipe = show_recipe_dialog(IT_ASSEMBLER, false);
				if (inventory_remove_item( inventory, item_selected, 1 ) >= 0)
				{
					Machine *mach = addAssembler( &machinelist, cursor_tx, cursor_ty, place_belt_index, recipe );
					objectmap[tileoffset] = mach;
				}
			} else 
			if ( item_selected == IT_INSERTER )
			{
				int filter_item = show_item_dialog();
				if ( inventory_remove_item( inventory, item_selected, 1 ) >= 0 ) // take from inventory
				{
					Inserter *insp = addInserter(&inserterlist, cursor_tx, cursor_ty, place_belt_index, filter_item);
					objectmap[tileoffset] = insp;
				}
			} else 
			if ( item_selected == IT_GENERATOR )
			{
				int recipe = show_recipe_dialog(IT_GENERATOR, false);
				if (inventory_remove_item( inventory, item_selected, 1 ) >= 0)
				{
					Machine *mach = addGenerator( &machinelist, cursor_tx, cursor_ty, place_belt_index, recipe );
					objectmap[tileoffset] = mach;
				}
			} else 
			if ( item_selected == IT_TSPLITTER )
			{
				// not making a splitter a separate object type as yet 
				// reuse some of the Machine fields for splitter purposes
				// ptype - splitter state. Use to lookup TSplitSwitchStates.out[] direction
				// processTime - (3-bytes int) store ItemNodePtr if an object is moving though it
				if (inventory_remove_item( inventory, item_selected, 1 ) >= 0)
				{
					objectmap[tileoffset] = addMachine( &machinelist, item_selected, cursor_tx, cursor_ty, place_belt_index, 0, 0, 0);
				}
			} else
			if ( inventory_remove_item( inventory, item_selected, 1 ) >= 0 )
			{
				// generic machine ... e.g. Box
				objectmap[tileoffset] = addMachine( &machinelist, item_selected, cursor_tx, cursor_ty, 0, 100, 0, 0);
			}
		}
	}

	stop_place();
}

//------------------------------------------------------------
// Item (small object) movement on belts, etc
//------------------------------------------------------------

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
bool itemIsInRows(ItemNodePtr itemptr, int from_row, int to_row)
{
	int from_y = from_row * gTileSize - 15 ;
	int to_y = to_row * gTileSize ;

	if (itemptr->x > fac.xpos - 8 &&
		itemptr->x < fac.xpos + gScreenWidth &&
		itemptr->y > fac.ypos + from_y &&
		itemptr->y < fac.ypos + to_y )
	{
		return true;
	}
	return false;
}

// draws the additional layers: belts, machines and items
//
// Modified to draw screen in 3 horizontal sections to reduce flickering of items on belts
// If we draw all the belts and then all the items, the VDP doesn't keep up
// Ideally we would draw each horizontal row of tiles and their tiles to get minimum flickering
// but that is way too slow, so a comprimise is to draw 3 sections.
void draw_layer(bool draw_items)
{
	int tx=getTileX(fac.xpos);
	int ty=getTileY(fac.ypos);

	int num_sections = 3;
	int from_row = 0;
	int num_rows = (gScreenHeight/gTileSize) / num_sections;

	for ( int section = 0 ; section < num_sections ; section++)
	{
		for (int i=from_row; i < from_row+num_rows; i++) 
		{
			draw_horizontal_layer(tx, ty+i, 1+(gScreenWidth/gTileSize), true, true, false );
		}

		if ( draw_items )
		{
			ItemNodePtr currPtr = itemlist;
			int cnt=0;
			while (currPtr != NULL) {
				if ( itemIsInRows(currPtr, from_row, from_row+num_rows) )
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
					if ( itemIsInRows(currPtr, from_row, from_row+num_rows) )
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
					if ( itemIsInRows(currPtr, from_row, from_row+num_rows) )
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
		from_row += num_rows;
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
			draw_machines(tx + i, ty, px + i*gTileSize, py, true); 
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

// move a given item along a belt or a splitter
// in and out are the directions in and out of the belt/splitter
// redraw = indicate that the out tile needs to be redrawn
// return if the item has moved or not
bool move_item_on_belt(ItemNodePtr currPtr, int in, int out, bool *predraw)
{
	bool moved = false;

	int centrex = currPtr->x + ITEM_CENTRE_OFFSET;
	int centrey = currPtr->y + ITEM_CENTRE_OFFSET;

	// (tx,ty) is the tile the centre of the item is in
	int tx = centrex >> 4;
	int ty = centrey >> 4;

	// (dx,dy) offset of centre of item in relation to tile
	int dx = centrex % gTileSize;
	int dy = centrey % gTileSize;

	int current_offset =  tx + ty*mapinfo.width;

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

			// jump the tile to the centre of the belt it just moved into
			// bit hacky but avoids the item moving outside of the belt
			tx = (currPtr->x + ITEM_CENTRE_OFFSET) >> 4;
			ty = (currPtr->y + ITEM_CENTRE_OFFSET) >> 4;

			int new_offset = tx + ty*mapinfo.width;
			if ( new_offset != current_offset )
			{
				int newbeltID = layer_belts[ new_offset ];
				if (newbeltID >= 0)
				{
					int newin = belts[newbeltID].in;
					switch ( out ) 
					{
						case DIR_UP:
							if ( newin != DIR_DOWN ) { (*predraw)=true; currPtr->y = ty*gTileSize + 4; }
							break;
						case DIR_RIGHT:
							if ( newin != DIR_LEFT ) { (*predraw)=true; currPtr->x = tx*gTileSize + 4; }
							break;
						case DIR_DOWN:
							if ( newin != DIR_UP ) { (*predraw)=true; currPtr->y = ty*gTileSize + 4; }
							break;
						case DIR_LEFT:
							if ( newin != DIR_RIGHT ) { (*predraw)=true; currPtr->x = tx*gTileSize + 4; }
							break;
						default:
							break;
					}
				}
			}
		}
	}
	return moved;
}

// Move items along a belt or into an inserter or machine
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
			if ( ( (insp->start_tx == tx && insp->start_ty == ty) )
				 && insp->itemcnt < insp->maxitemcnt
				 && currPtr->item == insp->filter_item )
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
					break;
				}
			}
			thptr = thptr->next;
		}

		// check movements on belts
		int beltOffset =  tx + ty*mapinfo.width;
		int beltID = layer_belts[ beltOffset ];
		bool redraw=false;
		if (!moved && beltID >= 0)
		{
			int in = belts[beltID].in;
			int out = belts[beltID].out;

			moved = move_item_on_belt(currPtr, in, out, &redraw);
		}

		// check movements on splitters
		MachineHeader *mhp = (MachineHeader*) objectmap[ beltOffset ];
		if ( !moved && mhp && mhp->machine_type == IT_TSPLITTER )
		{
			Machine *mach = (Machine*) mhp;
			int in = tsplit_states[mach->dir].in;
			int out = tsplit_states[mach->dir].out[mach->ptype];
			if ( mach->processTime == 0 )
			{
				mach->processTime = (int) currPtr;
			}
		
			if ( mach->processTime == (int) currPtr )
			{
				moved = move_item_on_belt(currPtr, in, out, &redraw);
				
				if ( moved )
				{
					int newtx = (currPtr->x + ITEM_CENTRE_OFFSET) >> 4;
					int newty = (currPtr->y + ITEM_CENTRE_OFFSET) >> 4;
					// has it moved out of the splitter tile?
					if ( newtx != tx || newty != ty )
					{
						// we are ready for a new object
						mach->processTime = 0;
						// switch the splitter
						mach->ptype = 1 - mach->ptype;
					}
				}
			}
		}

		if ( redraw )
		{
			int px=getTilePosInScreenX(tx);
			int py=getTilePosInScreenY(ty);
			draw_tile(tx, ty, px, py);
			//draw_machines(tx, ty, px, py);
			draw_items_at_tile(tx, ty);
		}

		if ( (bDraw && moved) || redraw )
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

void inserters_pull_from_box()
{
	ThingNodePtr tlistp = inserterlist;
	while ( tlistp != NULL )
	{
		Inserter *insp = (Inserter*)tlistp->thing;
		int tx = insp->start_tx;
		int ty = insp->start_ty;
		int tileoffset = tx + ty * mapinfo.width;

		MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
		if (mh && mh->machine_type > 0 )
		{
			if ( mh->machine_type == IT_BOX &&
			     insp->itemcnt < insp->maxitemcnt )
			{
				int it = convertProductionToMachine(insp->filter_item);
				int index = inventory_remove_item( inventory, it, 1 );
				if (index >=0)
				{
					insertAtFrontItemList( &insp->itemlist, insp->filter_item, tx*gTileSize+4, ty*gTileSize+4 );
					insp->itemcnt++;
				}
			}
		}


		tlistp = tlistp->next;
	}
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
			}
			if (putback)
			{
				ItemNodePtr it = popItem( &insp->itemlist, currPtr );
				if ( it )
				{
					// special case it gets put into another inserter (middle)
					int tileoffset = insp->end_tx + insp->end_ty * mapinfo.width;
					MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
					if (mh && mh->machine_type == IT_INSERTER )
					{
						Inserter *insp2 = objectmap[tileoffset];
						if ( insp2->filter_item == currPtr->item &&
							 insp2->itemcnt < insp2->maxitemcnt )
						{
							insertAtFrontItemList( &insp2->itemlist, currPtr->item, currPtr->x, currPtr->y );
							insp2->itemcnt++;
							insp->itemcnt--;
						}
					} else {
						// add to front of itemlist;
						it->next = itemlist;
						itemlist = it;
						insp->itemcnt--;
						numItems++;
					}
				}
			}
			if ( bDraw && moved )
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

//------------------------------------------------------------
// Small number/digit drawing utilities (move out of here?)
//------------------------------------------------------------

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

	// get resource and machine info
	
	int info_item_bmid = -1;
	int info_item_type = 0;
	int resource_count = -1;
	int resource_type = -1;
	int resource_bmid = -1;

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
			// assembler needs more lines for info
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
		resource_type = (info_item_type - IT_FEAT_STONE) + IT_STONE;
		resource_bmid = itemtypes[ resource_type ].bmID;
	}

	resource_count = getResourceCount(cursor_tx, cursor_ty);

	// Pointing to Bob - don't bother with the other info
	if (cursor_tx == getTileX(fac.bobx) && cursor_ty == getTileY(fac.boby) )
	{
		char text[]="This is BOB";
		int textlen = MAX(strlen(text) * 8, 108);
		draw_filled_box( infox, infoy, textlen+4, boxh, 11, 8);
		vdp_write_at_graphics_cursor();
		vdp_move_to( infox+4, infoy+8 );
		printf("%s",text);
		vdp_write_at_text_cursor();
		while( vdp_check_key_press( KEY_i ) ) { vdp_update_key_state(); }; 
		wait_for_any_key_press();
		draw_screen();
	} else

	if ( info_item_bmid >= 0 ) // have something to show
	{
		char * text = itemtypes[ info_item_type ].desc;
		int textlen = MAX(strlen(text) * 8, 108);
		draw_filled_box( infox, infoy, textlen+4, boxh, 11, 8);
		vdp_adv_select_bitmap( info_item_bmid );
		vdp_draw_bitmap( infox+4, infoy+4 );
		vdp_write_at_graphics_cursor();

		// show machine and recipe
		if ( info_item_type == IT_FURNACE || info_item_type == IT_ASSEMBLER || info_item_type == IT_GENERATOR )
		{
			Machine* mach = findMachine(&machinelist, cursor_tx, cursor_ty);
			if ( mach && mach->ptype >= 0 )
			{
				ProcessType *pt = getProcessType( mach );
				int x=infox+4+20;
				int leftx = x;
				int maxx = 0;
				for (int it = 0; it < pt->innum; it++)
				{
					x = leftx;
					if ( pt->in[it] > 0 )
					{
						int itemBMID = itemtypes[ convertMachineToProduction(pt->in[it]) ].bmID;
						vdp_adv_select_bitmap( itemBMID );
						vdp_draw_bitmap( x, infoy+4 + 8*it );
					} else {
						vdp_move_to( x, infoy+4 + 8*it );
						printf("?");
					}
					x+=10;
					vdp_move_to( x, infoy+4 + 8*it );
					printf("%d",mach->countIn[it]);
					x+=8;
					if (mach->countIn[it]>9) x += 8;
					if (mach->countIn[it]>99) x += 8;
					if (x > maxx) maxx = x;
				}
				x = maxx;
				if ( pt->out > 0 )
				{
					vdp_move_to( x, infoy+4 + 4 ); putch(CHAR_RIGHTARROW);
					x += 10;
					int itemBMID = itemtypes[ pt->out ].bmID;
					vdp_adv_select_bitmap( itemBMID );
					for (int it = 0; it < pt->outcnt; it++)
					{
						vdp_draw_bitmap( x, infoy+4 + 4 );
						x+= itemtypes[ pt->out ].size==0?18:10;
					}
					vdp_move_to( x, infoy+4 + 4 );
					printf("%d",mach->countOut);
				}
				if ( info_item_type == IT_GENERATOR )
				{
					vdp_move_to( infox+24, infoy+12);
					printf("TotEn:%d",fac.energy);
				}
			}
		}
		if ( info_item_type == IT_INSERTER )
		{
			Inserter *insp = (Inserter*)objectmap[tileoffset];
			if (insp)
			{
				int x=infox+4+20;
				vdp_move_to( x, infoy );
				int itemBMID = itemtypes[ convertMachineToProduction(insp->filter_item) ].bmID;
				vdp_adv_select_bitmap( itemBMID );
				vdp_draw_bitmap( x, infoy+8 );
			}
		}
		// show resource info
		if ( resource_count > 0 )
		{
			int x = infox+32;
			vdp_move_to( x, infoy+4 + 4 ); putch(CHAR_RIGHTARROW);
			x+=10;
			vdp_adv_select_bitmap( resource_bmid );
			vdp_draw_bitmap( x, infoy+4 );
			x+=10;
			vdp_move_to( x, infoy+4 );
			printf("%d",resource_count);
		}

		// item description
		vdp_move_to( infox+3, infoy+4+18 + yadj );
		vdp_gcol(0, 15);
		printf("%s",itemtypes[ info_item_type ].desc);
		vdp_write_at_text_cursor();

		while( vdp_check_key_press( KEY_i ) ) { vdp_update_key_state(); }; 

		wait_for_any_key_press();
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
			if ( cursor_tx == bx - 1 && (cursor_ty == by || cursor_ty == by -1 || cursor_ty == by +1) ) bNext=true;
			break;
		case BOB_RIGHT:
			if ( cursor_tx == bx + 1 && (cursor_ty == by || cursor_ty == by -1 || cursor_ty == by +1) ) bNext=true;
			break;
		case BOB_UP:
			if ( cursor_ty == by - 1 && (cursor_tx == bx || cursor_tx == bx -1 || cursor_tx == bx +1) ) bNext=true;
			break;
		case BOB_DOWN:
			if ( cursor_ty == by + 1 && (cursor_tx == bx || cursor_tx == bx -1 || cursor_tx == bx +1) ) bNext=true;
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
			draw_machines(tx, ty, px, py, false);
			deleteItem(&resourcelist, rp);
		} else if ( (rp->item % fac.resourceMultiplier) == 0 )
		{
			// size is reduced - change icon in tilemap
			overlay += 5;
			tilemap[tx + ty*mapinfo.width] &= 0x0F;
			tilemap[tx + ty*mapinfo.width] |= (overlay << 4);
			int px=getTilePosInScreenX(tx);
			int py=getTilePosInScreenY(ty);
			draw_tile(tx, ty, px, py);
			draw_machines(tx, ty, px, py, false);
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
			int ret = inventory_add_item(inventory, raw_item, 1);
			if (ret < 0)
			{
				new_message(&messageinfo, "Inventory full!", -1, 100, 1);
			} else {
				new_message(&messageinfo, "+1", raw_item, 100, 17);
			}
		}
	}
}

// remove Belt or Machine at cursor (with delete key)
void removeAtCursor()
{
	int tileoffset = cursor_tx + cursor_ty * mapinfo.width;
	if ( layer_belts[ tileoffset ] >= 0 )
	{
		int ret = inventory_add_item( inventory, IT_BELT, 1 );
		if (ret < 0)
		{
			new_message(&messageinfo, "Inventory full!", -1, 100, 1);
		} else {
			layer_belts[ tileoffset ] = -1;
		}
	}
	MachineHeader *mh = (MachineHeader*) objectmap[tileoffset];
	if (mh && mh->machine_type > 0)
	{
		int machine_type = mh->machine_type;
		if ( machine_type == IT_INSERTER )
		{
			int ret = inventory_add_item( inventory, IT_INSERTER, 1 );
			if (ret < 0)
			{
				new_message(&messageinfo, "Inventory full!", -1, 100, 1);
			} else {
				// delete inserter object
				ThingNodePtr thingnode = getInserterNode(&inserterlist, objectmap[tileoffset]);
				free( thingnode->thing );
				thingnode->thing=NULL;
				objectmap[tileoffset]=NULL;
				ThingNodePtr delme = popThing( &inserterlist, thingnode );
				free( delme );
			}
		} else 
		{
			// add machine
			int ret = inventory_add_item( inventory, machine_type, 1 );
			if (ret < 0)
			{
				new_message(&messageinfo, "Inventory full!", -1, 100, 1);
			} 
			else 
			{
				// add all of the machine's stored items
				Machine *mach = (Machine*) objectmap[tileoffset];
				ProcessType *pt = getProcessType(mach);
				if ( mach )
				{
					if ( pt )
					{
						for ( int inputs=0; inputs < pt->innum; inputs++)
						{
							if (mach->countIn[inputs] > 0) 
							{
								int it = convertProductionToMachine( pt->in[inputs] );
								int ret = inventory_add_item( inventory, it, mach->countIn[inputs]);
								if (ret < 0)
								{
									new_message(&messageinfo, "Inventory full!", -1, 100, 1);
								}

							}
						}
						if (mach->countOut > 0) 
						{
							int it = convertProductionToMachine( pt->out );
							int ret = inventory_add_item( inventory, it, mach->countOut);
							if (ret < 0)
							{
								new_message(&messageinfo, "Inventory full!", -1, 100, 1);
							}
						}

						// delete machine
						ThingNodePtr thingnode = getMachineNode( &machinelist, objectmap[tileoffset] );
						if (thingnode)
						{
							deleteThing(&machinelist, thingnode);
							objectmap[tileoffset] = NULL;
						}
					} 
					// no process type - can be a box

					// delete machine
					ThingNodePtr thingnode = getMachineNode( &machinelist, objectmap[tileoffset] );
					if (thingnode)
					{
						deleteThing(&machinelist, thingnode);
						objectmap[tileoffset] = NULL;
					}
				}
			}
		}
	}
	draw_tile( cursor_tx, cursor_ty, cursorx, cursory );
	draw_machines( cursor_tx, cursor_ty, cursorx, cursory, false );

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
				int it = convertProductionToMachine(ip->item);
				
				int ret = inventory_add_item( inventory, it, 1 );
				if (ret < 0)
				{
					new_message(&messageinfo, "Inventory full!", -1, 100, 1);
				} else {
					free(ip);
					ip=NULL;
				}
				
				ip = popFrontItem(&itptr);
			}
		}	
	}
}

void insertItemIntoMachine(int machine_type, int tx, int ty, int item )
{
	bool bInserted = false;
	// Box is a kind of machine which transfers all items entering it
	// to the inventory
	if ( machine_type == IT_BOX )
	{
		// if the item is a machine, convert to the actual machine
		item = convertProductionToMachine(item);
		int ret = inventory_add_item(inventory, item, 1);
		if (ret < 0)
		{
			new_message(&messageinfo, "Inventory full!", -1, 100, 1);
		} else {
			bInserted = true;
		}
	}
	if ( machine_type == IT_FURNACE )
	{
		Machine* mach = findMachine(&machinelist, tx, ty);
		int ptype = mach->ptype;
		if ( ptype >= 0 )
		{
			if ( furnaceProcessTypes[ptype].in[0] == item )
			{
				mach->countIn[0] += 1;
				bInserted = true;
			} 
				
		}
	}
	if ( machine_type == IT_ASSEMBLER )
	{
		//TAB(0,0);
		Machine* mach = findMachine(&machinelist, tx, ty);
		int ptype = mach->ptype;

		if ( ptype >= 0 )
		{
			if (item == IT_BELT)
			{
				item = IT_MINI_BELT;
			}
			// find which of the inputs matches this item
			for (int k=0; k < assemblerProcessTypes[ptype].innum; k++)
			{
				if ( assemblerProcessTypes[ptype].in[k] == item )
				{
					// increment count of that item in the assembler
					mach->countIn[k]++;
					bInserted = true;
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
					bInserted = true;
					break;
				}
			}
		}
	}
	if (!bInserted)
	{
		// machine didn't accept it, bung the item into the inventory
		int it = convertProductionToMachine(item);
		int ret = inventory_add_item(inventory, it, 1);
		if (ret < 0)
		{
			new_message(&messageinfo, "Inventory full!", -1, 100, 1);
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
				int maxi_item = convertProductionToMachine(item);
				insertItemIntoMachine( machine_itemtype, tx, ty, maxi_item );
				ItemNodePtr popped = popItem( &itemlist, currPtr );
				numItems--;
				free(popped);
			}
		}
		currPtr = nextPtr;
	}
}

//------------------------------------------------------------
// Dialogs
//------------------------------------------------------------

#define INV_EXT_BORDER 3
#define INV_INT_BORDER 4
// Show inventory and run it's own mini game-loop
void show_inventory(int X, int Y)
{
	int offx = X;
	int offy = Y;
	int inv_offsetsX[MAX_INVENTORY_ITEMS] = {0};
	int inv_offsetsY[MAX_INVENTORY_ITEMS] = {0};
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
		// zero count shouldn't show anything
		if ( inventory[ii].count <= 0 )
		{
			inventory[ii].item = INVENTORY_EMPTY_SLOT;
			inventory[ii].count = 0;
		}

		// if anything is in this slot, draw it
		if ( inventory[ii].item >= 0 && inventory[ii].item != INVENTORY_EMPTY_SLOT )
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
// Show file dialog
bool show_filedialog(bool bLoadOnly)
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

	bool fd_return = file_dialog("./saves", filename, 80, &isload, bLoadOnly);

	COL(11);COL(128+16);
	vdp_cls();
	TAB(0,0);
	if (fd_return)
	{
		if ( isload )
		{
			printf("Loading %s ... \n",filename);
			load_game( filename, false );
		} else {
			if ( !bLoadOnly)
			{
				printf("Saving %s ... \n",filename);
				save_game( filename );
			}
		}

		printf("\nPress any key\n");
		wait();
	}

	change_mode(gMode);
	vdp_cursor_enable( false );
	vdp_logical_scr_dims( false );

	show_bob();
	reshow_sprites();

	draw_screen();

	vdp_refresh_sprites();

	return fd_return;
}

#define RECIPE_EXT_BORDER 5
#define RECIPE_INT_BORDER 2
#define RECIPE_NUM_IN_VIEW 6
#define RECIPE_BOX_WIDTH 128
#define RECIPE_SELECT_HEIGHT 22
#define RECIPE_TITLE_HEIGHT 10
int show_recipe_dialog(int machine_type, bool bManual)
{
	int offx = 10;
	int offy = 20;
	if ( cursorx > 160 ) offx = 180;

	int box_offsetsY[20] = {0};
	int cursor_border_on = 15; // white
	int cursor_border_off = 0; // black

	int recipe_selected = 0;
	int old_recipe_selected = 0;
 
	// UI sze
	int boxw = RECIPE_EXT_BORDER + RECIPE_BOX_WIDTH + RECIPE_EXT_BORDER;
	int boxh = RECIPE_EXT_BORDER + RECIPE_TITLE_HEIGHT + 
		(RECIPE_SELECT_HEIGHT + RECIPE_INT_BORDER) * MIN(RECIPE_NUM_IN_VIEW,NUM_ASM_PROCESSES) 
		+ 8
		- RECIPE_INT_BORDER + RECIPE_EXT_BORDER;

	vdp_select_sprite( CURSOR_SPRITE );
	vdp_hide_sprite();
	vdp_refresh_sprites();

	vdp_write_at_graphics_cursor();

	// game loop for interacting with recipe dialog
	clock_t key_wait_ticks = clock() + 20;
	bool finish = false;
	bool finish_exit = false;
	bool redisplay_cursor = true;
	bool redisplay_recipes = true;
	int num_processes = 0;
	ProcessType *processType;
    switch (machine_type)
	{
		case IT_GENERATOR:
			num_processes = NUM_GENERATOR_PROCESSES;
			processType = generatorProcessTypes;
			break;
		case IT_ASSEMBLER:
			if (bManual)
			{
				num_processes = NUM_MANUAL_ASM_PROCESSES;
			} else
			{
				num_processes = NUM_ASM_PROCESSES;
			}
			processType = assemblerProcessTypes;
			break;
		case IT_FURNACE:
			num_processes = NUM_FURNACE_PROCESSES;
			processType = furnaceProcessTypes;
			break;
		default:
			finish_exit = true;
			goto show_recipe_dialog_exit;
			break;
	}
	int from_process = 0;
	int to_process = MIN(RECIPE_NUM_IN_VIEW, num_processes);
	do {

		if ( redisplay_recipes )
		{
			// dark-blue filled box with yellow line border
			draw_filled_box( offx, offy, boxw, boxh, 11, 16 );
			vdp_move_to( offx + RECIPE_EXT_BORDER, offy + RECIPE_EXT_BORDER);
			COL(2); printf("Select Recipe");
			vdp_move_to( offx + RECIPE_EXT_BORDER, boxh+8);
			if (bManual)
			{
				COL(2); printf("Enter or X:Exit");
			} else {
				COL(2); printf("Enter to select");
			}

			redisplay_recipes = false;
			for (int j = from_process; j < to_process; j++)
			{
				box_offsetsY[j-from_process] = offy + RECIPE_EXT_BORDER + RECIPE_TITLE_HEIGHT + 
					(j-from_process)*(RECIPE_SELECT_HEIGHT+RECIPE_INT_BORDER);

				// cells with dark grey fill with (cursor_border_off=black) border
				draw_filled_box(
						offx + RECIPE_EXT_BORDER,
						box_offsetsY[j-from_process],
						RECIPE_BOX_WIDTH, RECIPE_SELECT_HEIGHT, cursor_border_off, 8);

				vdp_gcol(0, 11);
				int xx = offx + RECIPE_EXT_BORDER + 2;
				int yy = box_offsetsY[j-from_process] + 2;
				int overflowx = 0; int overflowy = 0;
				// recipe number
				draw_number_lj( j, xx, yy+2 );
				xx+=8;
				// inputs
				for (int i=0; i < processType[j].innum; i++ )
				{
					int itemBMID = itemtypes[ convertMachineToProduction(processType[j].in[i]) ].bmID;
					if ( xx > offx + 88 ) {
						overflowx = xx; overflowy = yy;
						xx = offx + RECIPE_EXT_BORDER + 10;
						yy += 8;
					}
					// either show icons
					if ( processType[j].incnt[i] < 4 )
					{
						for (int k=0; k<processType[j].incnt[i]; k++)
						{
							vdp_adv_select_bitmap( itemBMID );
							vdp_draw_bitmap( xx, yy );
							xx += 10;
						}
					} else {
						// or the icon and a count
						vdp_adv_select_bitmap( itemBMID );
						vdp_draw_bitmap( xx, yy );
						xx += 8;
						vdp_move_to( xx, yy ); printf("x%d",processType[j].incnt[i]);
						xx +=18; if ( processType[j].incnt[i] >=10 ) xx += 10;
					}
					// plus sign between inputs
					if ( i <  processType[j].innum -1 )
					{
						vdp_move_to( xx, yy ); printf("+");
						xx += 10;
					}
				}
				if (overflowx==0) 
				{
					// arrow
					vdp_move_to( xx, yy ); printf("%c",CHAR_RIGHTARROW);
					xx += 10;
				} else {
					xx = overflowx;
					yy = overflowy;
					vdp_move_to( xx, yy+4 ); printf("%c",CHAR_RIGHTARROW);
					xx += 10;
				}

				// outputs
				int out_itemtype = convertProductionToMachine(processType[j].out);
				int itemBMID = itemtypes[ out_itemtype ].bmID;

				// generators show energy produced
				if ( machine_type == IT_GENERATOR )
				{
					vdp_move_to( xx, yy ); printf("%dE",processType[j].outcnt);
					xx += 10;
				} else {
					// otherwise show output bitmaps
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
				box_offsetsY[old_recipe_selected-from_process], 
				RECIPE_BOX_WIDTH, 
				RECIPE_SELECT_HEIGHT,
				cursor_border_off);
			// cursor
			draw_box( 
				offx + RECIPE_EXT_BORDER, 
				box_offsetsY[recipe_selected-from_process], 
				RECIPE_BOX_WIDTH, 
				RECIPE_SELECT_HEIGHT,
				cursor_border_on);
			old_recipe_selected = recipe_selected;
		}

		if (bManual)
		{
			if ( key_wait_ticks < clock() && 
					( vdp_check_key_press( KEY_g ) || vdp_check_key_press( KEY_x ) || vdp_check_key_press( KEY_enter ) ) )
			{
				if ( vdp_check_key_press( KEY_g ) || vdp_check_key_press( KEY_x ) ) finish_exit = true;
				finish=true;
				// delay otherwise x will cause exit from program
				while ( vdp_check_key_press( KEY_g ) || vdp_check_key_press( KEY_x ) || vdp_check_key_press( KEY_enter ) ) vdp_update_key_state();
			}
		} else {
			if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_enter ) )
			{
				finish=true;
				// delay otherwise x will cause exit from program
				while ( vdp_check_key_press( KEY_enter ) ) vdp_update_key_state();
			}
		}

		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_UP ) )
		{
			key_wait_ticks = clock() + key_wait;
			if ( recipe_selected > from_process )
			{
				recipe_selected--;
				redisplay_cursor = true;
			} else 
			if ( recipe_selected > 0 ) {
				recipe_selected--;
				redisplay_cursor = true;
				from_process--;
				to_process--;
				redisplay_recipes=true;
			}
		}
		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_DOWN ) )
		{
			key_wait_ticks = clock() + key_wait;
			if ( recipe_selected < MIN(RECIPE_NUM_IN_VIEW+from_process, num_processes)-1 ) {
				recipe_selected++;
				redisplay_cursor = true;
			} else 
			if ( recipe_selected < num_processes-1 )
			{
				recipe_selected++;
				from_process += 1;
				to_process = MIN(RECIPE_NUM_IN_VIEW+from_process, num_processes);
				redisplay_recipes = true;
				redisplay_cursor = true;
			}
		}

		vdp_update_key_state();
	} while (finish==false);

show_recipe_dialog_exit:
	vdp_write_at_text_cursor();
	draw_screen();
	COL(15);

	vdp_select_sprite( CURSOR_SPRITE );
	vdp_show_sprite();
	vdp_refresh_sprites();

	return finish_exit?-1:recipe_selected;
}

#define ITEMD_EXT_BORDER 5
#define ITEMD_INT_BORDER 2
#define ITEMD_NUM_IN_VIEW 6
#define ITEMD_BOX_WIDTH 120
#define ITEMD_SELECT_HEIGHT 16
#define ITEMD_TITLE_HEIGHT 10
int show_item_dialog()
{
	int offx = 10;
	int offy = 20;
	if ( cursorx > 160 ) offx = 180;

	int box_offsetsY[20] = {0};
	int cursor_border_on = 15; // white
	int cursor_border_off = 0; // black

	int item_selected = 0;
	int old_item_selected = 0;
 
	// UI sze
	int boxw = ITEMD_EXT_BORDER + ITEMD_BOX_WIDTH + ITEMD_EXT_BORDER;
	int boxh = ITEMD_EXT_BORDER + ITEMD_TITLE_HEIGHT + 
		(ITEMD_SELECT_HEIGHT + ITEMD_INT_BORDER) * MIN(ITEMD_NUM_IN_VIEW,NUM_ASM_PROCESSES) 
		+ 8
		- ITEMD_INT_BORDER + ITEMD_EXT_BORDER;

	vdp_select_sprite( CURSOR_SPRITE );
	vdp_hide_sprite();
	vdp_refresh_sprites();

	vdp_write_at_graphics_cursor();

	// game loop for interacting with dialog
	clock_t key_wait_ticks = clock() + 20;
	bool finish = false;
	bool redisplay_cursor = true;
	bool redisplay_items = true;
	int num_items = 0;
	int select_items[NUM_ITEMTYPES];
	for (int i=0; i< NUM_ITEMTYPES; i++)
	{
		if ( isResource(i) || isProduct(i) )
		{
			select_items[num_items++] = i;
		}
	}
	int from_item = 0;
	int to_item = MIN(ITEMD_NUM_IN_VIEW, num_items);
	do {
		if ( redisplay_items )
		{
			// dark-blue filled box with yellow line border
			draw_filled_box( offx, offy, boxw, boxh, 11, 16 );
			vdp_move_to( offx + ITEMD_EXT_BORDER, offy + ITEMD_EXT_BORDER);
			COL(2); printf("Select Filter");
			vdp_move_to( offx + ITEMD_EXT_BORDER, boxh+8);
			COL(2); printf("Enter to select");

			redisplay_items = false;
			for (int j = from_item; j < to_item; j++)
			{
				box_offsetsY[j-from_item] = offy + ITEMD_EXT_BORDER + ITEMD_TITLE_HEIGHT + 
					(j-from_item)*(ITEMD_SELECT_HEIGHT+ITEMD_INT_BORDER);

				// cells with dark grey fill with (cursor_border_off=black) border
				draw_filled_box(
						offx + ITEMD_EXT_BORDER,
						box_offsetsY[j-from_item],
						ITEMD_BOX_WIDTH, ITEMD_SELECT_HEIGHT, cursor_border_off, 8);

				vdp_gcol(0, 11);
				int xx = offx + ITEMD_EXT_BORDER + 2;
				int yy = box_offsetsY[j-from_item] + 2;

				draw_number_lj( j, xx, yy+2 );
				xx+=8;
				int itype = select_items[j];
				vdp_adv_select_bitmap(itemtypes[itype].bmID);
				vdp_draw_bitmap( xx, yy ); xx+=15 + (1-itemtypes[itype].size);
				vdp_move_to(xx, yy);
				vdp_gcol(0, 11);
				printf("%s",itemtypes[itype].desc);
			}
		}
		if ( redisplay_cursor )
		{
			redisplay_cursor = false;
			// clear old cursor
			draw_box( 
				offx + ITEMD_EXT_BORDER, 
				box_offsetsY[old_item_selected-from_item], 
				ITEMD_BOX_WIDTH, 
				ITEMD_SELECT_HEIGHT,
				cursor_border_off);
			// cursor
			draw_box( 
				offx + ITEMD_EXT_BORDER, 
				box_offsetsY[item_selected-from_item], 
				ITEMD_BOX_WIDTH, 
				ITEMD_SELECT_HEIGHT,
				cursor_border_on);
			old_item_selected = item_selected;
		}

		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_enter ) )
		{
			finish=true;
			// delay otherwise x will cause exit from program
			while ( vdp_check_key_press( KEY_enter ) ) vdp_update_key_state();
		}

		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_UP ) )
		{
			key_wait_ticks = clock() + key_wait;
			if ( item_selected > from_item )
			{
				item_selected--;
				redisplay_cursor = true;
			} else 
			if ( item_selected > 0 ) {
				item_selected--;
				redisplay_cursor = true;
				from_item--;
				to_item--;
				redisplay_items=true;
			}
		}
		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_DOWN ) )
		{
			key_wait_ticks = clock() + key_wait;
			if ( item_selected < MIN(ITEMD_NUM_IN_VIEW+from_item, num_items)-1 ) {
				item_selected++;
				redisplay_cursor = true;
			} else 
			if ( item_selected < num_items-1 )
			{
				item_selected++;
				from_item += 1;
				to_item = MIN(ITEMD_NUM_IN_VIEW+from_item, num_items);
				redisplay_items = true;
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

	return select_items[item_selected];
}

// mini dialog for selecting a game
void select_game_map()
{
	int bgcol = 4;
	int fgcol = 15;
	int hlcol = 11;

	draw_filled_box( 20, 40, 280, 120, fgcol, bgcol );

	COL(fgcol); COL(128+bgcol);

	int line = 8;
	TAB(4,line++);printf("Select Map:");
	line++;
	for ( int i=0; i< NUM_AVAILABLE_MAPS; i++)
	{
		COL(hlcol);TAB(4,line++);printf("%d: ",i+1);COL(fgcol);printf("%s ", available_maps[i].desc);
		COL(hlcol);printf("Target:");
		COL(fgcol);printf("%d %s%s",
				available_maps[i].target_item_count, 
				itemtypes[ available_maps[i].target_item_type ].desc,
				available_maps[i].target_item_count>1?"s":"");
	}
	line += 2;
	int selected = 1;
	bool bexit = false;
	COL(fgcol);TAB(4,line); printf("?: ");
	TAB(7,line); COL(hlcol);printf("%d",selected);
	while (vdp_check_key_press(KEY_enter));

	while (!bexit)
	{
		int x_keys[3] = { KEY_x, KEY_enter, KEY_escape };
		for ( int k = 0; k < 3; k++ )
		{
			if ( vdp_check_key_press( x_keys[k] ) )
			{
				while (vdp_check_key_press( x_keys[k] ));
				bexit = true;
			}
		}
		for ( int i=0; i< NUM_AVAILABLE_MAPS; i++)
		{
			if ( vdp_check_key_press(KEY_1+i) )
			{
				while (vdp_check_key_press(KEY_1+i));
				selected=1+i;
				TAB(7, line);COL(fgcol);printf("%d",selected);
				bexit = true;
			}
		}

		vdp_update_key_state();
	}
	selected_map = selected - 1;
	target_item_type = available_maps[ selected_map ].target_item_type;
	target_item_count = available_maps[ selected_map ].target_item_count;
}

//------------------------------------------------------------
// Help
//------------------------------------------------------------

void help_clear(int col)
{
	draw_filled_box( 10, 10, 300, 220, 11, 0 );
	if (col > 0)
	{
		draw_filled_box( 16, 26, 288, 184, 11, col );
	}
}

void help_feature(int x, int y, int itype, int col)
{
	int textoff = 4*(1 - itemtypes[itype].size);
	vdp_adv_select_bitmap(itemtypes[itype].bmID);
	vdp_draw_bitmap( x*8, y*8 ); x+=2;
	vdp_adv_select_bitmap(itemtypes[itype].bmID+5);
	vdp_draw_bitmap( x*8, y*8 ); x+=2;
	vdp_adv_select_bitmap(itemtypes[itype].bmID+10);
	vdp_draw_bitmap( x*8, y*8 ); x+=2;
	vdp_write_at_graphics_cursor();
	vdp_move_to(x*8, y*8 + textoff);
	vdp_gcol(0, col);
	printf("%s",itemtypes[itype].desc);
	vdp_write_at_text_cursor();
}
void help_item(int x, int y, int itype, int col, char *desc, int desccol)
{
	int off = 4*(1 - itemtypes[itype].size);
	vdp_adv_select_bitmap(itemtypes[itype].bmID);
	vdp_draw_bitmap( x*8, y*8 ); x+=2 + (1-itemtypes[itype].size);
	vdp_write_at_graphics_cursor();
	vdp_move_to(x*8, y*8 + off);
	vdp_gcol(0, col);
	printf("%s",itemtypes[itype].desc);
	vdp_gcol(0,desccol);
	vdp_move_to((x+strlen(itemtypes[itype].desc)+1)*8, y*8 + off);
	printf("%s",desc);
	vdp_write_at_text_cursor();
}

void help_items()
{
	int line = 4; int column = 3;
	help_clear(32);
	COL(128);
	COL(3);TAB(3,2); printf("------  F A C  HELP ");COL(9);printf(" Items");COL(3);printf(" ------");
	COL(128+32);
	COL(11); TAB(column,line++); printf("Features");
	for (int it=IT_TYPES_FEATURES; it<IT_TYPES_PRODUCT; it++)
	{
		help_feature(column,line,it,0); line+=2;
	}
	line++;
	COL(11); TAB(column,line++); printf("Raw (Mined)");
	for (int it=IT_TYPES_RAW; it<IT_TYPES_PROCESSED; it++)
	{
		help_item(column,line++,it,0,"",0);
	}

	line=4; column=20;
	COL(11); TAB(column,line); printf("Processed");
	vdp_adv_select_bitmap(itemtypes[IT_FURNACE].bmID);
	vdp_draw_bitmap( (column+11)*8, line*8-4 ); 
	line+=2;
	for (int it=IT_TYPES_PROCESSED; it<IT_TYPES_MACHINE; it++)
	{
		help_item(column,line++,it,0,"",0);
	}
	line++;
	COL(11); TAB(column,line); printf("Product");
	vdp_adv_select_bitmap(itemtypes[IT_ASSEMBLER].bmID);
	vdp_draw_bitmap( (column+11)*8, line*8-4 ); 
	line+=2;
	for (int it=IT_TYPES_PRODUCT; it<NUM_ITEMTYPES; it++)
	{
		help_item(column,line++,it,0,"",0);
	}
}
void help_machines()
{
	char *text[] = {
		"Process raw items",
		"Auto miner",
		"Produce using Recipes",
		"Grab and move items",
		"Items go to Inventory",
   		"Power machines",
		"Split to 2 outputs",
		"Escape planet"
	};
	int line = 4; int column = 3;
	help_clear(32);
	COL(128);
	COL(3);TAB(3,2); printf("-----  F A C  HELP ");COL(9);printf(" Machines");COL(3);printf(" -----");
	COL(128+32);

	COL(11); TAB(column,line++); printf("Belts");

	help_item(column,line,IT_BELT,0, "move items around map", 4); 
	line+=2;

	COL(11); TAB(column,line++); printf("Machines");
	COL(4);
	for (int it=IT_TYPES_MACHINE; it<IT_TYPES_FEATURES; it++)
	{
		if (it == IT_ESCPOD) {
			COL(11); TAB(column,line++); printf("Rockets");
		}
			
		help_item(column,line,it,0, text[it-IT_TYPES_MACHINE], 4); 
		line+=2;
	}

}

void help_line(int line, int column, int keywidth, char *keystr, char* helpstr, int col1, int col2)
{
	TAB(column,line);
	COL(col1);
	printf("%s",keystr);
	COL(8);
	for(int i=strlen(keystr);i<=keywidth;i++) printf(".");
	TAB(column+keywidth,line);
	COL(col2);
	printf("%s",helpstr);
}

void show_help(bool bShowWin)
{
	vdp_activate_sprites(0);
	help_clear(0);
	COL(128);
	COL(3);TAB(3,2); printf("--------  F A C   HELP   --------");
	int line = 4;
	help_line(line, 2, 6, "WASD", "Move Bob", 11, 13);
	help_line(line++, 18, 9, "Dir keys", "Move cursor", 11, 13);
	line++;
	help_line(line, 2, 6, "X", "Exit", 11, 13); help_line(line++, 21, 6, "F", "File dialog", 11, 13);
	help_line(line, 2, 6, "F2", "Toggle HUD", 11, 13); help_line(line++, 21, 6, "N", "New game", 11, 13);
	help_line(line, 2, 6, "L", "Refresh", 10, 14); help_line(line++, 21, 6, "C", "Centre curs", 10, 14);
	help_line(line, 2, 6, "F9", "Toggle Sound", 10, 14); help_line(line++, 21, 6, "F8,F7", "Vol Up/Down", 10, 14);
	line++;
	help_line(line++, 2, 6, "I", "Show info under cursor", 10, 14);
	help_line(line++, 2, 6, "E", "Show Inventory and select item", 10, 14);
	help_line(line++, 2, 6, "P/Q", "Begin/Quit placing item", 10, 14);
	help_line(line++, 2, 6, "Enter", "Place selected item", 10, 14);
	help_line(line++, 2, 6, "R/T", "Rotate selected item", 10, 14);
	help_line(line++, 2, 6, "Del", "Delete item", 10, 14);
	help_line(line++, 2, 6, "Z", "Pickup items under cursor", 10, 14);
	line++;
	help_line(line++, 2, 6, "M", "Mine. Facing resource & cursor", 11, 13);
	help_line(line++, 2, 6, "G", "Manually assemble items", 11, 13);

	line+=1;

	COL(6);TAB(2,line++);printf("   Explore and mine resources.");
	COL(6);TAB(2,line++);printf("Process using furnaces and assemble");
	COL(6);TAB(2,line++);printf("  into items and more machines.");

	if (bShowWin)
	{
		COL(15);TAB(2,line++);printf("     WIN: Make %d %s%s.", 
				target_item_count,
				itemtypes[target_item_type].desc,
				target_item_count > 1 ?"s":"");
	} else { 
		line++;
	}

	COL(3);TAB(5,27);printf("Next: any key     X: Exit help");

	while( vdp_check_key_press( KEY_h ) )
	{
		vdp_update_key_state();
	}

	if (!wait_for_any_key_with_exit(KEY_x)) goto leave_help;
	delay(5);

	help_items();
	COL(128);
	COL(3);TAB(5,27);printf("Next: any key     X: Exit help");

	if (!wait_for_any_key_with_exit(KEY_x)) goto leave_help;
	delay(5);

	help_machines();
	COL(128);
	COL(3);TAB(5,27);printf("Next: any key     X: Exit help");

	if (!wait_for_any_key_with_exit(KEY_x)) goto leave_help;

leave_help:
	delay(5);
	key_wait_ticks = clock() + key_wait;
	vdp_cls();
	
	draw_screen();
	COL(15);

	vdp_activate_sprites( NUM_SPRITES );
	vdp_select_sprite( CURSOR_SPRITE );
	vdp_show_sprite();
	vdp_refresh_sprites();
}

//------------------------------------------------------------
// Splash screen special loop
//------------------------------------------------------------

void splash_loop_info(int FG, int BG, int HL)
{
	COL(FG);COL(128+BG);
	TAB(2,28);COL(FG);printf("Map:");COL(HL);printf("%s", available_maps[selected_map].desc);
	TAB(30,28);COL(FG);printf("Sound:");COL(HL);printf("%s", bSoundEnabled?"On ":"Off");

	TAB(4,1);COL(FG);printf("FAC"); COL(HL);printf(" - A Factory Game for the AGON");
	TAB(14,2);COL(FG);printf("by robogeek");

	TAB(2,25);COL(HL);printf("Enter");COL(FG);printf(" Start");
	TAB(17,25);COL(HL);printf("H");COL(FG);printf(" Help");
	TAB(27,25);COL(HL);printf("L");COL(FG);printf(" Load Game");

	TAB(2,27);COL(HL);printf("M");COL(FG);printf(" Change Map");
	TAB(24,27);COL(HL);printf("S");COL(FG);printf(" Sound on/off");
}

bool splash_loop()
{
	int loopexit=0;
	bool exit_status = true; // after this, load a new game
	
	int bgcol = 4;
	int fgcol = 15;
	int hlcol = 11;

	COL(fgcol);COL(128+bgcol); vdp_cls();

	draw_screen();
	draw_layer(true);
	splash_loop_info(fgcol,bgcol,hlcol);

	layer_wait_ticks = clock();
	key_wait_ticks = clock();

	for (int i = 0; i< 4; i++)
	{
		insertAtFrontItemList(&itemlist, IT_COPPER_PLATE, (3+i*4)*gTileSize + 4, (2)*gTileSize + 4);
		insertAtFrontItemList(&itemlist, IT_IRON_PLATE, (3+i*4)*gTileSize + 4, (11)*gTileSize + 4);
	}
	for (int i = 0; i< 3; i++)
	{
		insertAtFrontItemList(&itemlist, IT_CIRCUIT, (1)*gTileSize + 4, (4+i*3)*gTileSize + 4);
		insertAtFrontItemList(&itemlist, IT_GEARWHEEL, (18)*gTileSize + 4, (4+i*3)*gTileSize + 4);
	}

	do {
		if (layer_wait_ticks < clock()) 
		{
			layer_wait_ticks = clock() + belt_speed; // belt anim speed
			//draw_cursor(false);
			move_items(false);
			move_items_on_inserters(true);
			draw_layer(true);
		}
		if ( vdp_check_key_press( KEY_h ) ) // help dialog
		{
			while ( vdp_check_key_press( KEY_h ) );
			show_help(false);
			vdp_activate_sprites(0);
			draw_screen();
			draw_layer(true);
			splash_loop_info(fgcol,bgcol,hlcol);
		}
		if ( vdp_check_key_press( KEY_x ) )
		{
			while ( vdp_check_key_press( KEY_x ) );
			return false;
		}
		if ( vdp_check_key_press( KEY_enter ) )
		{
			while ( vdp_check_key_press( KEY_enter ) );
			loopexit = true;
		}
		if ( vdp_check_key_press( KEY_s ) )
		{
			while ( vdp_check_key_press( KEY_s ) );
			if (bSoundSamplesLoaded)
			{
				bSoundEnabled = !bSoundEnabled;
				splash_loop_info(fgcol,bgcol,hlcol);
			}
		}
		if ( vdp_check_key_press( KEY_m ) )
		{
			while ( vdp_check_key_press( KEY_m ) );
			select_game_map();
			draw_screen();
			draw_layer(true);
			splash_loop_info(fgcol,bgcol,hlcol);
		}

		if ( vdp_check_key_press( KEY_l ) )
		{
			while ( vdp_check_key_press( KEY_l ) );
			// show the file dialog in load-only mode. Only exit the loop if the user chose to 
			// load a game
			loopexit = show_filedialog( true ); // load only
			if (loopexit) 
			{
				exit_status = false; // meaning don't load a new game!
			}
		}

		vdp_update_key_state();

		frame_time_in_ticks = clock() - ticks_start;
	} while (loopexit==0);

	return exit_status;
}

//------------------------------------------------------------
// Sounds
//------------------------------------------------------------

bool load_sound_samples(int vert_pos)
{
	PROGBAR *progbar;
	int cnt=1;
	int prog_max = 2;

	TAB(10,vert_pos);printf("Loading Sound Samples");
	progbar = init_horiz_bar(10,8*(vert_pos+2),300,24,0,prog_max,1,3);

	update_bar(progbar, 0);

	int sample_len = load_sound_sample("sounds/steps.raw",-2);
	if (sample_len == 0 )
	{
		return false;
	}
	update_bar(progbar, ++cnt);

	vdp_audio_set_sample( SOUND_CHAN_STEPS, 64257 );
	vdp_audio_play_note( SOUND_CHAN_STEPS, sound_volume, 435, -1 ); // channel 1, loop
	vdp_audio_set_volume( SOUND_CHAN_STEPS, 0 );
	bPlayingWalkSound = false;

	sample_len = load_sound_sample("sounds/pick-axe2.pcm",-3);
	if (sample_len == 0 )
	{
		return false;
	}
	update_bar(progbar, ++cnt);

	vdp_audio_set_sample( SOUND_CHAN_PICKAXE, 64258 );
	vdp_audio_play_note( SOUND_CHAN_PICKAXE, sound_volume, 435, -1 ); // channel 2, loop
	vdp_audio_set_volume( SOUND_CHAN_PICKAXE, 0 );

	delete_bar(&progbar);

	return true;
}

void walking_sound_enable( bool enable )
{
	if ( enable )
	{
		bPlayingWalkSound = true;
		vdp_audio_sample_seek( SOUND_CHAN_STEPS, 0 );
		vdp_audio_set_volume( SOUND_CHAN_STEPS, sound_volume );
	} else {
		bPlayingWalkSound = false;
		vdp_audio_set_volume( SOUND_CHAN_STEPS, 0 );
	}
}

