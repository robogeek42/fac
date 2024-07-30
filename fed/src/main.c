/*
  vim:ts=4
  vim:sw=4
*/
#include "colmap.h"

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

#define _IMAGE_IMPLEMENTATION
#include "images.h"
#define _FILEDIALOG_IMPLEMENTATION
#include "filedialog.h"

#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3

#define BITS_UP 1
#define BITS_RIGHT 2
#define BITS_DOWN 4
#define BITS_LEFT 8

// scrolling direction values for VDP
#define SCROLL_RIGHT 0
#define SCROLL_LEFT 1
#define SCROLL_UP 2
#define SCROLL_DOWN 3

int gMode = 8; 
int gScreenWidth = 320;
int gScreenHeight = 240;
int gTileSize = 16;
int gScreenWidthTiles = 20;
int gScreenHeightTiles = 15;

int gMapTW = 14;
int gMapTH = 14;
int gViewOffX = 8;
int gViewOffY = 8;

int gMinimapScale = 2;

typedef struct {
	int mapWidth;
	int mapHeight;
} FacInfo;

FacInfo fac;

int scr_tx;		// Position of top-left of screen in world coords (pixel)
int scr_ty;

// map - terrain and overlay (resources)
uint8_t* tilemap;

// cursor position in tile coords
int cursor_tx=0, cursor_ty=0;
int old_cursor_tx=0, old_cursor_ty=0;

bool bGridLines = true;
bool bTileNumbers = true;

int bank = 0; // selected bank of 10 tiles
int bankMax = 3; // maximum number of banks
int tselect = 0; // selected tile withing a bank (0-9)
int total_bms = 0; // total number of bitmaps to select from

bool bBlockSelect = false;
int block_tx = 0;
int block_ty = 0;

// tilesets
typedef struct {
	int set[4];
	int num;
	bool bOverlay;
} Tileset;

#define NUM_TILESETS 8
Tileset tileset[NUM_TILESETS] = {
	//{ {1,2,3,4}, 4, false },
	{ {0+BMOFF_FEAT16,5+BMOFF_FEAT16,10+BMOFF_FEAT16}, 3, true }, // stone
	{ {1+BMOFF_FEAT16,6+BMOFF_FEAT16,11+BMOFF_FEAT16}, 3, true }, // Iron
	{ {2+BMOFF_FEAT16,7+BMOFF_FEAT16,12+BMOFF_FEAT16}, 3, true }, // Copper
	{ {3+BMOFF_FEAT16,8+BMOFF_FEAT16,13+BMOFF_FEAT16}, 3, true }, // Coal
	{ {4+BMOFF_FEAT16,9+BMOFF_FEAT16,14+BMOFF_FEAT16}, 3, true }, // Trees
	{ {1, 2, 3, 4},4,false }, // grass
	{ {5, 6, 7, 8},4,false }, // desert
	{ {9,10,11,12},4,false }, // mud
};

int ts = 0; // selected tileset

clock_t key_wait_ticks;
int key_wait = 14;
clock_t move_wait_ticks;

static volatile SYSVAR *sys_vars = NULL;

int getTilePosInScreenX(int tx) { return ((tx - scr_tx) * gTileSize)+gViewOffX; }
int getTilePosInScreenY(int ty) { return ((ty - scr_ty) * gTileSize)+gViewOffY; }
void game_loop();
void draw_all();
void show_help();
int getOverlayAtOffset( int tileoffset );
int getOverlayAtCursor();
void show_filedialog();
int load_map( char *filename, int width, int height );
int save_map( char *filename );
void draw_cursor();
void draw_tile(int tx, int ty, int tposx, int tposy);
void draw_tile_at_cursor();
void draw_screen();
void draw_block();
void draw_horizontal(int tx, int ty, int len);
void scroll_screen(int dir, int tiles);
void draw_digit(int i, int px, int py);
void draw_number(int n, int px, int py);
void draw_number_lj(int n, int px, int py);
void draw_tile_menu();
void delete_tile(bool bOverlayOnly);
void set_tile(bool bRandom);
int newmap_dialog();
void fill_random();
void draw_mini_map();
void circle_random(bool bRandom);
void new_mode();

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

int main(/*int argc, char *argv[]*/)
{
	vdp_vdu_init();
	sys_vars = (SYSVAR *)mos_sysvars();

	if ( vdp_key_init() == -1 ) return 1;
	vdp_set_key_event_handler( key_event_handler );

	change_mode(gMode);
	vdp_cursor_enable( false );
	vdp_logical_scr_dims( false );

	fac.mapWidth = -1;
	fac.mapHeight = -1;
	scr_tx = 0;
	scr_ty = 0;

	load_map( "./maps/newmap1", 45, 45 );
	if ( ! load_images(true) )
	{
		printf("Failed to load images\n");
		goto my_exit2;
	}
	create_sprites();
	total_bms = NUM_BM_TERR16 + NUM_BM_FEAT16;

	cursor_tx = 0;
	cursor_ty = 0;
	old_cursor_tx = 0;
	old_cursor_ty = 0;

	draw_screen();
	draw_cursor();
	draw_tile_menu();

	game_loop();

	change_mode(0);

my_exit2:
	vdp_logical_scr_dims( true );
	vdp_cursor_enable( true );
	return 0;
}

void draw_all()
{
	draw_screen();
	draw_tile_menu();
	vdp_activate_sprites( NUM_SPRITES );
	draw_cursor();
}

void game_loop()
{
	int exit=0;
	key_wait_ticks = clock();
	move_wait_ticks = clock();

	do {
		int dir=-1;
		int cursor_dir=0;
		bool bRedrawCursor = false;
		bool bRedrawBlock = false;
		int scroll_tiles=1;

		// cursor movement
		if ( vdp_check_key_press( KEY_LEFT ) ) {cursor_dir |= BITS_LEFT; }
		if ( vdp_check_key_press( KEY_RIGHT ) ) {cursor_dir |= BITS_RIGHT; }
		if ( vdp_check_key_press( KEY_UP ) ) {cursor_dir |= BITS_UP; }
		if ( vdp_check_key_press( KEY_DOWN ) ) {cursor_dir |= BITS_DOWN; }

		// move the cursor
		if (cursor_dir>0 && ( key_wait_ticks < clock() ) ) {
			key_wait_ticks = clock() + key_wait;

			if ( cursor_dir & BITS_UP ) { old_cursor_ty=cursor_ty; cursor_ty--; }
			if ( cursor_dir & BITS_RIGHT ) { old_cursor_tx=cursor_tx; cursor_tx++; }
			if ( cursor_dir & BITS_DOWN ) { old_cursor_ty=cursor_ty; cursor_ty++; }
			if ( cursor_dir & BITS_LEFT ) { old_cursor_tx=cursor_tx; cursor_tx--; }
			bRedrawCursor = true;
			if (bBlockSelect) 
			{
				bRedrawBlock = true;
			}
		}
		// keep cursor on screen
		if ( cursor_tx < scr_tx ) { 
			old_cursor_tx=cursor_tx; cursor_tx = scr_tx; 
			dir=SCROLL_RIGHT; bRedrawCursor=true;
		}
		if ( cursor_ty < scr_ty ) {
			old_cursor_ty=cursor_ty; cursor_ty = scr_ty; 
			dir=SCROLL_UP; bRedrawCursor=true;
		}
		if ( cursor_tx >= scr_tx + gMapTW ) {
			old_cursor_tx=cursor_tx; cursor_tx = scr_tx + gMapTW-1;
			dir=SCROLL_LEFT; bRedrawCursor=true;
		}

		if ( cursor_ty >= scr_ty + gMapTH ) {
			old_cursor_ty=cursor_ty; cursor_ty = scr_ty + gMapTH-1;
			dir=SCROLL_DOWN; bRedrawCursor=true;
		}

		/*
		if ( vdp_check_key_press( KEY_w ) ) { dir=SCROLL_UP; }
		if ( vdp_check_key_press( KEY_a ) ) { dir=SCROLL_RIGHT; }
		if ( vdp_check_key_press( KEY_s ) ) { dir=SCROLL_DOWN; }
		if ( vdp_check_key_press( KEY_d ) ) { dir=SCROLL_LEFT; }
		*/

		if ( vdp_check_key_press( KEY_pageup ) ) { dir=SCROLL_UP; scroll_tiles = gMapTH; }
		if ( vdp_check_key_press( KEY_home ) ) { dir=SCROLL_RIGHT; scroll_tiles = gMapTW; }
		if ( vdp_check_key_press( KEY_pagedown ) ) { dir=SCROLL_DOWN; scroll_tiles = gMapTH; }
		if ( vdp_check_key_press( KEY_end ) ) { dir=SCROLL_LEFT; scroll_tiles = gMapTW; }

		if (bRedrawCursor)
		{
			draw_cursor();
		}

		// scroll the screen
		if (dir>=0 && ( move_wait_ticks < clock() ) ) {
			move_wait_ticks = clock()+15;
			scroll_screen(dir, scroll_tiles);
			draw_screen();
		} else if (bRedrawBlock)
		{
			draw_block();
		}


		if ( vdp_check_key_press( KEY_g ) && key_wait_ticks < clock() ) // toggle grid lines
		{
			key_wait_ticks = clock() + key_wait;

			bGridLines = !bGridLines;
			draw_screen();
		}

		if ( vdp_check_key_press( KEY_d ) && key_wait_ticks < clock() ) // WASD move tile select/bank
		{
			key_wait_ticks = clock() + key_wait;
			bank++; bank %= bankMax;
			draw_tile_menu();
		}
		if ( vdp_check_key_press( KEY_a ) && key_wait_ticks < clock() ) // WASD move tile select/bank
		{
			key_wait_ticks = clock() + key_wait;
			bank--; 
			if (bank < 0) bank += bankMax;
			draw_tile_menu();
		}

		if ( vdp_check_key_press( KEY_s ) && key_wait_ticks < clock() ) // WASD move tile select/bank
		{
			key_wait_ticks = clock() + key_wait;
			if (tselect < 9)
			{
				tselect++;
			}
			else
			{
				tselect=0;
				bank++;
			}
			draw_tile_menu();
		}
		if ( vdp_check_key_press( KEY_w ) && key_wait_ticks < clock() ) // WASD move tile select/bank
		{
			key_wait_ticks = clock() + key_wait;
			if ( tselect > 0 || bank > 0 )
			{
				tselect--;
				if ( tselect < 0 )
				{
					tselect = 9;
					bank--;
				}
				draw_tile_menu();
			}
		}

		// number keys select a tile in a bank
		if (key_wait_ticks < clock()) 
		{
			for ( int k = KEY_0; k <= KEY_9; k++)
			{
				if ( vdp_check_key_press( k ) )
				{
						key_wait_ticks = clock() + key_wait;
						tselect = k - KEY_0;
						draw_tile_menu();
						break;
				}
			}
		}

		if ( vdp_check_key_press( KEY_space ) && key_wait_ticks < clock() ) // set tile
		{
			key_wait_ticks = clock() + key_wait;
			set_tile( false );
		}

		if ( vdp_check_key_press( KEY_b ) && key_wait_ticks < clock() ) // block select
		{
			key_wait_ticks = clock() + key_wait;
			if (bBlockSelect == false)
			{
				block_tx = cursor_tx;
				block_ty = cursor_ty;
				bBlockSelect = true;
				draw_block();
			}
			else
			{
				bBlockSelect = false;
				draw_screen();
			}
		}

		if ( vdp_check_key_press( KEY_backspace ) && key_wait_ticks < clock() ) // delete overlay
		{
			key_wait_ticks = clock() + key_wait;
			delete_tile(true);
		}
		if ( vdp_check_key_press( KEY_delete ) && key_wait_ticks < clock() ) // delete tile
		{
			key_wait_ticks = clock() + key_wait;
			delete_tile(false);
		}

		if ( vdp_check_key_press( KEY_t ) && key_wait_ticks < clock() ) // select tileset
		{
			key_wait_ticks = clock() + key_wait;
			ts++; ts %= NUM_TILESETS;
			draw_tile_menu();
		}

		if ( vdp_check_key_press( KEY_r ) && key_wait_ticks < clock() ) // fill with random tiles from tileset
		{
			key_wait_ticks = clock() + key_wait;
			set_tile( true );
		}

		if ( vdp_check_key_press( KEY_o ) && key_wait_ticks < clock() ) // circle random
		{
			key_wait_ticks = clock() + key_wait;
			circle_random( true );
		}


		if ( vdp_check_key_press( KEY_m ) && key_wait_ticks < clock() ) // show minimap
		{
			key_wait_ticks = clock() + key_wait;
			draw_mini_map();
		}


		if ( vdp_check_key_press( KEY_n ) && key_wait_ticks < clock() ) // new map
		{
			key_wait_ticks = clock() + key_wait;
			if (!newmap_dialog())
			{
				wait_for_any_key();
				draw_all();
			}
		}

		if ( vdp_check_key_press( KEY_f12 ) && key_wait_ticks < clock() ) // new map
		{
			key_wait_ticks = clock() + key_wait;
			new_mode();
		}

		if ( vdp_check_key_press( KEY_f ) && key_wait_ticks < clock() ) // file dialog
		{
			key_wait_ticks = clock() + key_wait;
			show_filedialog();
		}

		if ( vdp_check_key_press( KEY_h ) && key_wait_ticks < clock() ) // help
		{
			key_wait_ticks = clock() + key_wait;
			show_help();
		}

		if ( vdp_check_key_press( KEY_x ) && key_wait_ticks < clock() ) { // x - exit
			TAB(6,8);printf("Are you sure?");
			char k=getchar(); 
			if (k=='y' || k=='Y') exit=1;
		}

		vdp_update_key_state();

	} while (exit==0);

}

void help_line(int line, char *keystr, char* helpstr)
{
	TAB(2,line);
	COL(11);
	printf("%s",keystr);
	TAB(13,line);
	COL(13);
	printf("%s",helpstr);
}
void show_help()
{
	vdp_activate_sprites(0);
	draw_filled_box( 10, 10, 300, 220, 11, 0 );
	COL(11);
	TAB(3,2); printf("--------	FAC Editor HELP --------");
	int line = 4;
	help_line(line++, "WASD", "Select tile");
	help_line(line++, "dir keys", "Move cursor");
	help_line(line++, "page u/d", "Move view");
	help_line(line++, "Home/End", "Move view");
	line++;
	help_line(line++, "H", "Show this help screen");
	help_line(line++, "X", "Exit");
	help_line(line++, "F", "File dialog");
	help_line(line++, "N", "New map");
	help_line(line++, "G", "Toggle grid lines");
	line++;
	help_line(line++, "B", "Block select start");
	help_line(line++, "Space", "Place tile (block)");
	help_line(line++, "Delete", "Delete tile (block)");
	help_line(line++, "Backspace", "Delete overlay (block)");
	line++;
	help_line(line++, "T", "Select tileset");
	help_line(line++, "R", "Place random tile (block)");
	help_line(line++, "O", "Circle random in block");

	while( vdp_check_key_press( KEY_h ) )
	{
		vdp_update_key_state();
	}
	wait_for_any_key();
	key_wait_ticks = clock() + key_wait;
	vdp_cls();
	draw_all();
	COL(15);
}

int getOverlayAtOffset( int tileoffset )
{
	return (tilemap[tileoffset] & 0xF0) >> 4;
}

int getOverlayAtCursor()
{
	return (tilemap[cursor_ty*fac.mapWidth +  cursor_tx] & 0xF0) >> 4;
}

int load_map_info( char *filename)
{
	uint8_t ret = mos_load( filename, (uint24_t) &fac,  2*sizeof(uint24_t) );
	if ( ret != 0 )
	{
		printf("Failed to load map info\n");
	}
	return ret;
}

int save_map_info( char *filename)
{
	uint8_t ret = mos_save( filename, (uint24_t) &fac,  2*sizeof(uint24_t) );
	if ( ret != 0 )
	{
		printf("Failed to load map info\n");
	}
	return ret;
}

int load_map( char *filename, int width, int height )
{
	if (width <1 || height <1)
	{
		width = input_int_noclear(2,3,"Map Width:");
		if (width<1) return -1;
		height = input_int_noclear(2,4,"Map Height:");
		if (height<1) return -1;
	}
	free(tilemap);
	tilemap = (uint8_t *) malloc(sizeof(uint8_t) * width * height);
	if (tilemap == NULL)
	{
		printf("Out of memory\n");
		return -1;
	}
	uint8_t ret = mos_load( filename, (uint24_t) tilemap,  width * height );
	if ( ret != 0 )
	{
		printf("Failed to load map\n");
	}
	fac.mapWidth = width;
	fac.mapHeight = height;
	return ret;
}

int save_map( char *filename )
{
	uint8_t ret = mos_save( filename, (uint24_t) tilemap,  fac.mapWidth * fac.mapHeight );
	if ( ret != 0 )
	{
		printf("Failed to save map\n");
	}
	return ret;
}

// Show file dialog
void show_filedialog()
{
	vdp_select_sprite( CURSOR_SPRITE );
	vdp_hide_sprite();
	vdp_refresh_sprites();

	char filename[80];
	bool isload = false;

	change_mode(3);
	vdp_logical_scr_dims( false );
	vdp_cursor_enable( false );

	int fd_return = file_dialog("./maps", filename, 80, &isload);

	COL(11);COL(128+16);
	vdp_cls();
	TAB(0,0);
	if (fd_return)
	{
		char infofilename[85];
		strcpy( infofilename, filename );
		strcat( infofilename,".info" );
		if ( isload )
		{
			printf("Loading %s ... ",infofilename);
			load_map_info( infofilename );
			printf("load %s %dx%d \n",filename,fac.mapWidth, fac.mapHeight);
			load_map( filename, fac.mapWidth, fac.mapHeight );
			scr_tx = 0; scr_ty = 0;
			cursor_tx = 0; cursor_ty = 0;
		} else {
			printf("Saving %s ... \n",infofilename);
			save_map_info( infofilename );
			printf("save %s ... \n",filename);
			save_map( filename );
		}

		printf("\nPress any key\n");
		wait();
	}

	change_mode(gMode);
	vdp_cursor_enable( false );
	vdp_logical_scr_dims( false );

	draw_screen();
	draw_tile_menu();

	vdp_activate_sprites( NUM_SPRITES );
	vdp_select_sprite( CURSOR_SPRITE );
	vdp_show_sprite();
	vdp_refresh_sprites();

	draw_cursor();
}


void draw_tile(int tx, int ty, int tposx, int tposy)
{
	int tileoffset = ty*fac.mapWidth + tx;
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
void draw_screen()
{
	if (fac.mapWidth<0) return;

	for (int i=0; i < gMapTH; i++) 
	{
		draw_horizontal(scr_tx, scr_ty+i, gMapTW);
	}

	if (bGridLines)
	{
		vdp_gcol(0,8);
		for (int x=0; x<(gMapTW)*gTileSize; x+=gTileSize)
		{
			vdp_move_to(x+gViewOffX,0+gViewOffY);
			vdp_line_to(x+gViewOffX,gMapTH*gTileSize-1+gViewOffY);
		}	
		for (int y=0; y<(gMapTH)*gTileSize; y+=gTileSize)
		{
			vdp_move_to(0+gViewOffX,y+gViewOffY);
			vdp_line_to(gMapTW*gTileSize-1+gViewOffX,y+gViewOffY);
		}	
	}
	if (bTileNumbers)
	{
		draw_filled_box(0,0,gViewOffX+gMapTW*gTileSize,gViewOffY,0,0);
		for (int tx=0; tx < gMapTW; tx++)
		{
			draw_number_lj( tx+scr_tx, (tx*gTileSize)+gViewOffX+4, gViewOffY-6); 
		}
		draw_filled_box(0,0,gViewOffX, gViewOffY+gMapTH*gTileSize,0,0);
		for (int ty=0; ty < gMapTH; ty++)
		{
			draw_number( ty+scr_ty, gViewOffX, (ty*gTileSize)+gViewOffY+6); 
		}
	}
	if (bBlockSelect)
	{
		int startx = (block_tx - scr_tx)*gTileSize + gViewOffX;
		int starty = (block_ty - scr_ty)*gTileSize + gViewOffY;
		int sizex = (cursor_tx-block_tx+1) * gTileSize;
		int sizey = (cursor_ty-block_ty+1) * gTileSize;
		if (sizex > 0 && sizey > 0 )
		{
			draw_box( startx, starty, sizex, sizey, 14 );
		} else {
			bBlockSelect = false;
		}
		old_cursor_tx = cursor_tx;
		old_cursor_ty = cursor_ty;
	}
}
void draw_block()
{
	if (fac.mapWidth<0) return;

	for (int i=block_ty; i < cursor_ty+1; i++) 
	{
		draw_horizontal(block_tx, i, (cursor_tx-block_tx)+1);
	}
	if (bGridLines)
	{
		vdp_gcol(0,8);
		for (int x=0; x<(gMapTW)*gTileSize; x+=gTileSize)
		{
			vdp_move_to(x+gViewOffX,0+gViewOffY);
			vdp_line_to(x+gViewOffX,gMapTH*gTileSize-1+gViewOffY);
		}	
		for (int y=0; y<(gMapTH)*gTileSize; y+=gTileSize)
		{
			vdp_move_to(0+gViewOffX,y+gViewOffY);
			vdp_line_to(gMapTW*gTileSize-1+gViewOffX,y+gViewOffY);
		}	
	}
	if (bBlockSelect)
	{
		int startx = (block_tx - scr_tx)*gTileSize + gViewOffX;
		int starty = (block_ty - scr_ty)*gTileSize + gViewOffY;
		int sizex = (cursor_tx-block_tx+1) * gTileSize;
		int sizey = (cursor_ty-block_ty+1) * gTileSize;
		if (sizex > 0 && sizey > 0 )
		{
			draw_box( startx, starty, sizex, sizey, 14 );
		} else {
			bBlockSelect = false;
			draw_screen();
		}
	}
}

void draw_cursor() 
{
	// cursor position position in screen pixel coords
	int cursorx=0, cursory=0;
	cursorx = getTilePosInScreenX(cursor_tx);
	cursory = getTilePosInScreenY(cursor_ty);

	vdp_select_sprite( CURSOR_SPRITE );
	vdp_move_sprite_to( cursorx, cursory );
	vdp_nth_sprite_frame( 0 );
	
	vdp_refresh_sprites();
}

/* 0=right, 1=left, 2=up, 3=down */
void scroll_screen(int dir, int tiles)
{
	switch (dir) {
		case SCROLL_RIGHT: // scroll screen to right, view moves left
			if (scr_tx >= tiles)
			{
				scr_tx -= tiles;
			}
			else
			{
				scr_tx = 0;
			}
			break;
		case SCROLL_LEFT: // scroll screen to left, view moves right
			if ((scr_tx + gMapTW + tiles ) < fac.mapWidth)
			{
				scr_tx += tiles;
			}
			else
			{
				scr_tx = fac.mapWidth - gMapTW;
			}
			break;
		case SCROLL_UP:
			if (scr_ty >= tiles)
			{
				scr_ty -= tiles;
			}
			else
			{
				scr_ty = 0;
			}
			break;
		case SCROLL_DOWN:
			if ((scr_ty + gMapTH + tiles ) < fac.mapHeight)
			{
				scr_ty += tiles;
			}
			else
			{
				scr_ty = fac.mapHeight - gMapTH;
			}
			break;
		default:
			break;
	}
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

void draw_tile_menu()
{
	int ytop = 0;
	int xleft = gMapTW*gTileSize+gViewOffX;
	int x = xleft;
	int y = ytop;

	draw_filled_box(xleft, ytop, (gTileSize+1)*(bankMax+1)+1, 11*(gTileSize+1)+1, 0, 0);
	
	for (int bank = 0; bank <= total_bms/10; bank++)
	{
		draw_number_lj(bank+1, x+(gTileSize+1)*(bank+1)+6,ytop);
	}
	ytop+=8;
	y = ytop;
	for (int tile = 0; tile < 10; tile++)
	{
		draw_number(tile, x+14, y+4);
		y += gTileSize+1;
	}
	x += gTileSize;
	y = ytop;
	for (int bank = 0; bank <= total_bms/10; bank++)
	{
		for (int tile = 0; tile < 10; tile++)
		{
			int index = tile + 10*bank;
			if (index == total_bms) break;
			vdp_adv_select_bitmap( index );
			vdp_draw_bitmap( x, y );
			y += gTileSize+1;
		}
		x += gTileSize+1;
		y = ytop;
	}

	//draw_box(xleft+gTileSize + bank*(gTileSize+1) -1, ytop-1, gTileSize+1, 10*(gTileSize+1), 7); 

	draw_box(xleft+gTileSize + bank*(gTileSize+1) -1, ytop+tselect*(gTileSize+1)-1, gTileSize+1, gTileSize+1, 11); 

	// draw selected tileset
	ytop = 8+17*10+4;
	draw_filled_box(xleft,ytop,190,16,0,0);
	draw_number_lj(ts, xleft+8, ytop+4);
	for (int i=0; i<tileset[ ts ].num; i++)
	{
		vdp_adv_select_bitmap( tileset[ts].set[i] );
		vdp_draw_bitmap(xleft+i*(gTileSize+1)+12, ytop);
	}
}

void delete_tile(bool bOverlayOnly)
{
	if (bBlockSelect)
	{
		for ( int y = block_ty; y < cursor_ty+1; y++ )
		{
			int offset = block_tx + y*fac.mapWidth;
			for ( int x = block_tx; x < cursor_tx+1; x++ )
			{
				if (bOverlayOnly)
				{
					tilemap[ offset ] &= 0x0F;
				} else {
					tilemap[ offset ] = 0;
				}
				offset++;
			}
		}
		draw_screen();
	} else {
		int offset = cursor_tx+cursor_ty*fac.mapWidth;
		if (bOverlayOnly)
		{
			tilemap[ offset ] &= 0x0F;
		} else {
			tilemap[ offset ] = 0;
		}
		draw_tile_at_cursor();
	}
}

void draw_tile_at_cursor()
{
	draw_tile(cursor_tx, cursor_ty, getTilePosInScreenX(cursor_tx), getTilePosInScreenY(cursor_ty));
	if (bGridLines)
	{
		vdp_gcol(0,8);
		// vertical lines
		for (int x=(cursor_tx-scr_tx)*gTileSize; x<(cursor_tx-scr_tx+1)*gTileSize; x+=gTileSize)
		{
			vdp_move_to(x+gViewOffX, (block_ty-scr_ty)+gViewOffY);
			vdp_line_to(x+gViewOffX, (cursor_ty-scr_ty+1)*gTileSize+gViewOffY);
		}	
		// horizontal lines
		for (int y=(cursor_ty-scr_ty)*gTileSize; y<(cursor_ty-scr_ty+1)*gTileSize; y+=gTileSize)
		{
			vdp_move_to((block_tx-scr_tx)+gViewOffX, y+gViewOffY);
			vdp_line_to((cursor_tx-scr_tx+1)*gTileSize+gViewOffX, y+gViewOffY);
		}	
	}
}

void set_tile(bool bRandom)
{
	int bm = bank*10+tselect;
	if (bm < total_bms)
	{
		if (bBlockSelect)
		{
			for ( int y = block_ty; y < cursor_ty+1; y++ )
			{
				int offset = block_tx + y*fac.mapWidth;
				for ( int x = block_tx; x < cursor_tx+1; x++ )
				{
					if (bRandom)
					{
						int r = rand() % tileset[ts].num;
						bm = tileset[ts].set[r];
					}
					uint8_t byte = tilemap[ offset ];
					if ( bm < NUM_BM_TERR16 )
					{
						byte &= 0xF0;
						byte |= bm-BMOFF_TERR16;
					} else {
						byte &= 0x0F;
						byte |= ( (bm-BMOFF_FEAT16+1) << 4 );
					}
					tilemap[ offset ] = byte;
					offset++;
				}
			}
			bBlockSelect = false;
			draw_screen();
		} else {
			int offset = cursor_tx+cursor_ty*fac.mapWidth;
			uint8_t byte = tilemap[ offset ];
			if (bRandom)
			{
				int r = rand() % tileset[ts].num;
				bm = tileset[ts].set[r];
			}
			if ( bm < NUM_BM_TERR16 )
			{
				byte &= 0xF0;
				byte |= bm-BMOFF_TERR16;
			} else {
				byte &= 0x0F;
				byte |= ( (bm-BMOFF_FEAT16+1) << 4 );
			}
			tilemap[ offset ] = byte;
			draw_tile_at_cursor();
		}
	}
}

int newmap_dialog()
{
	int width = 0; int height = 0;
	int tile = 0;

	vdp_cls();
	COL(11);TAB(0,0); printf("New Map");
	COL(2);TAB(0,1); printf("Enter 0 dimension to exit\n");
	COL(15);
	while (width < 15 || width > 255)
	{
		width = input_int_noclear(2,4,"Map Width (15-255)");
		if (width == 0) return -1;
	}
	while (height < 15 || height > 255)
	{
		height = input_int_noclear(2,5,"Map Height (15-255)");
		if (height == 0) return -1;
	}

	tile = input_int_noclear(2, 7, "Initial tile (0-27)");
	if (tile < 0 || tile > 27) return -1;

	free(tilemap);
	tilemap = (uint8_t *) malloc(sizeof(uint8_t) * width * height);
	if (tilemap == NULL)
	{
		printf("Out of memory\n");
		return -1;
	}
	fac.mapWidth = width;
	fac.mapHeight = height;

	for (int y=0;y<height;y++)
	{
		for (int x=0;x<width;x++)
		{
			tilemap[ x + y*width ] = tile;
		}
	}

	draw_all();

	return 0;
}

void draw_mini_map()
{
	vdp_cls();
	int offx = 8;
	int offy = 8;

	for( int ty=0; ty<fac.mapHeight; ty++)
	{
		for( int tx=0; tx<fac.mapWidth; tx++)
		{
			int offset = tx + ty * fac.mapWidth;
			int col = 4; // dark blue
			int terr = tilemap[ offset ] & 0x0F;
			if ( terr > 0 )
			{
				switch ( terr )
				{
					case 1: 
					case 2: 
					case 3: 
					case 4: 
						col = 10; // green
						break;
					case 5: 
					case 6: 
					case 7: 
					case 8: 
						col = 3; // yellow
						break;
					case 9: 
					case 10: 
					case 11: 
					case 12: 
						col = 1; //dark-red 
						break;
					case 13: 
						col = 8; // dark grey
						break;
					case 14: 
						col = 14; // cyan
						break;
					case 15: 
						col = 15; // white
						break;
					default:
						col = 4; // dark-blue
						break;
				}
			}

			uint8_t overlay = getOverlayAtOffset( offset );
			if ( overlay > 0 ) 
			{
				int feat = overlay -1;
				switch( feat )
				{
					case 0: // stone
					case 5:
					case 10:
						col = 7; // grey
						break;
					case 1: // iron
					case 6:
					case 11:
						col = 9; // red
						break;
					case 2: // copper
					case 7:
					case 12:
						col = 5; // dark-mag
						break;
					case 3: // coal
					case 8:
					case 13:
						col = 0; // black
						break;
					case 4: // tree
					case 9:
					case 14:
						col = 2; // dark green
						break;
					default:
						break;
				}
			}
			if ( tx==cursor_tx && ty==cursor_ty) col=11;
			vdp_gcol(0, col);
			vdp_move_to( tx * gMinimapScale + offx, ty * gMinimapScale + offy );
			vdp_filled_rect( tx * gMinimapScale + offx + gMinimapScale-1, ty * gMinimapScale + offy + gMinimapScale-1 );
		}
	}
	draw_box(scr_tx * gMinimapScale + offx, scr_ty * gMinimapScale + offy, gMapTW * gMinimapScale, gMapTH * gMinimapScale, 8 );
	
	while( vdp_check_key_press( KEY_h ) )
	{
		vdp_update_key_state();
	}

	wait_for_any_key();
	key_wait_ticks = clock() + key_wait;
	vdp_cls();
	draw_all();
	COL(15);
}

// do edging for whole map
void do_edging()
{
	// find any non sea tiles, check for sea border and
	// replace with correct edge tile
	for( int ty=0; ty<fac.mapHeight; ty++)
	{
		for( int tx=0; tx<fac.mapWidth; tx++)
		{
			int offset = tx + ty * fac.mapWidth;
			uint8_t terrain =  tilemap[ offset ] & 0x0F;
			
			if (terrain > 0)
			{
				int tilenbs[9] = {0};
				for (int tn=0; tn<9; tn++)
				{
					int tnx = (tn % 3)-1 + tx;
					int tny = (tn / 3)-1 + ty;
					tilenbs[tn] = tilemap[ tnx + tny*fac.mapWidth ] & 0x0F;
				}
			}
		}
	}
}

void circle_random(bool bRandom)
{
	if (!bBlockSelect) return;
printf("c");
	int bw = cursor_tx - block_tx;
	int bh = cursor_ty - block_ty;
	int cx = block_tx + bw/2;
	int cy = block_ty + bh/2;
	for ( int y = block_ty; y < cursor_ty+1; y++ )
	{
		int offset = block_tx + y*fac.mapWidth;
		for ( int x = block_tx; x < cursor_tx+1; x++ )
		{
			int a = (cx - x);
			int b = (cy - y);
			float dist = sqrt ( (a*a) + (b*b) );
			float dist_adj = 3.0f * (dist / (cx-block_tx));
			int r = MIN(MAX((int) dist_adj,0),3);
			int bm = 0;
		   	if ( r<3) 
			{
				bm= tileset[ts].set[r];
				uint8_t byte = tilemap[ offset ];
				byte &= 0x0F;
				byte |= ( (bm - BMOFF_FEAT16+1) << 4 );
				tilemap[ offset ] = byte;
			}
			offset++;
		}
	}
	bBlockSelect = false;
	draw_screen();
}

void new_mode()
{

	vdp_cls();
	TAB(2,2);printf(" 0 - Mode 8 320x240 64 cols ");
	TAB(2,3);printf(" 1 - Mode 0 640x480 16 cols ");
	TAB(2,4);printf(" 2 - Mode 3 640x240 64 cols ");
	int choice = input_int_noclear(2,6,"Enter choice 0-3");
	switch (choice)
	{
		case 0:
		default:
			gMode = 8; 
			gScreenWidth = 320;
			gScreenHeight = 240;
			gTileSize = 16;
			gScreenWidthTiles = 20;
			gScreenHeightTiles = 15;
			gMapTW = 14;
			gMapTH = 14;
			gMinimapScale= 2;
			break;
		case 1:
			gMode = 0; 
			gScreenWidth = 640;
			gScreenHeight = 480;
			gTileSize = 16;
			gScreenWidthTiles = 40;
			gScreenHeightTiles = 30;
			gMapTW = 35;
			gMapTH = 29;
			gMinimapScale= 4;
			break;
		case 2:
			gMode = 3; 
			gScreenWidth = 640;
			gScreenHeight = 240;
			gTileSize = 16;
			gScreenWidthTiles = 40;
			gScreenHeightTiles = 15;
			gMapTW = 35;
			gMapTH = 14;
			gMinimapScale= 2;
			break;
	}
	change_mode(gMode);
	vdp_cursor_enable( false );
	vdp_logical_scr_dims( false );
	draw_all();
}

