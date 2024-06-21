/*
  vim:ts=4
  vim:sw=4
*/
#include "../src/colmap.h"

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
//#include "../../../agon_ccode/common/util.h"

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

int gMapTW = 16;
int gMapTH = 12;

typedef struct {
	int tx;
	int ty;		// Position of top-left of screen in world coords (pixel)
	int mapWidth;
	int mapHeight;
} FacState;

FacState fac;

// map - terrain and overlay (resources)
uint8_t* tilemap;

// cursor position in tile coords
int cursor_tx=0, cursor_ty=0;

bool bGridLines = true;

clock_t key_wait_ticks;
int key_wait = 15;
clock_t move_wait_ticks;

static volatile SYSVAR *sys_vars = NULL;

int getTilePosInScreenX(int tx) { return ((tx - fac.tx) * gTileSize); }
int getTilePosInScreenY(int ty) { return ((ty - fac.ty) * gTileSize); }
void game_loop();
int getOverlayAtOffset( int tileoffset );
int getOverlayAtCursor();
void show_filedialog();
int load_map( char *filename, int width, int height );
void draw_cursor();
void draw_screen();
void draw_horizontal(int tx, int ty, int len);
void scroll_screen(int dir);

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
	fac.tx = 0;
	fac.ty = 0;

	load_map( "./maps/fmap.data", 45, 45 );
	if ( ! load_images(true) )
	{
		printf("Failed to load images\n");
		goto my_exit2;
	}
	create_sprites();

	cursor_tx = 0;
	cursor_ty = 0;

	draw_screen();

	game_loop();

	change_mode(0);

my_exit2:
	vdp_logical_scr_dims( true );
	vdp_cursor_enable( true );
	return 0;
}

void game_loop()
{
	int exit=0;
	key_wait_ticks = clock();
	move_wait_ticks = clock();


	do {
		int dir=-1;
		int cursor_dir=0;

		if ( vdp_check_key_press( KEY_w ) ) { dir=SCROLL_UP; }
		if ( vdp_check_key_press( KEY_a ) ) { dir=SCROLL_RIGHT; }
		if ( vdp_check_key_press( KEY_s ) ) { dir=SCROLL_DOWN; }
		if ( vdp_check_key_press( KEY_d ) ) { dir=SCROLL_LEFT; }

		// scroll the screen
		if (dir>=0 && ( move_wait_ticks < clock() ) ) {
			move_wait_ticks = clock()+15;
			scroll_screen(dir);
			draw_screen();
		}

		// cursor movement
		if ( vdp_check_key_press( KEY_LEFT ) ) {cursor_dir |= BITS_LEFT; }
		if ( vdp_check_key_press( KEY_RIGHT ) ) {cursor_dir |= BITS_RIGHT; }
		if ( vdp_check_key_press( KEY_UP ) ) {cursor_dir |= BITS_UP; }
		if ( vdp_check_key_press( KEY_DOWN ) ) {cursor_dir |= BITS_DOWN; }

		// move the cursor
		if (cursor_dir>0 && ( key_wait_ticks < clock() ) ) {
			key_wait_ticks = clock() + key_wait;

			if ( cursor_dir & BITS_UP ) cursor_ty--;
			if ( cursor_dir & BITS_RIGHT ) cursor_tx++;
			if ( cursor_dir & BITS_DOWN ) cursor_ty++;
			if ( cursor_dir & BITS_LEFT ) cursor_tx--;

			draw_cursor();
		}
		// keep cursor on screen
		if ( cursor_tx < fac.tx ) { cursor_tx = fac.tx; draw_cursor(); }
		if ( cursor_ty < fac.ty ) { cursor_ty = fac.ty; draw_cursor(); }
		if ( cursor_tx > fac.tx + gScreenWidthTiles ) { cursor_tx = fac.tx + gScreenWidthTiles; draw_cursor(); }
		if ( cursor_ty > fac.ty + gScreenHeightTiles ) { cursor_ty = fac.ty + gScreenHeightTiles; draw_cursor(); }


		if ( vdp_check_key_press( KEY_g ) ) // grid lines
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;

				bGridLines = !bGridLines;
				draw_screen();
			}
		}

		if ( vdp_check_key_press( KEY_f ) ) // file dialog
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;

				show_filedialog();
			}
		}

		if ( vdp_check_key_press( KEY_x ) ) { // x - exit
			TAB(6,8);printf("Are you sure?");
			char k=getchar(); 
			if (k=='y' || k=='Y') exit=1;
		}

		vdp_update_key_state();

	} while (exit==0);

}

int getOverlayAtOffset( int tileoffset )
{
	return (tilemap[tileoffset] & 0xF0) >> 4;
}

int getOverlayAtCursor()
{
	return (tilemap[cursor_ty*fac.mapWidth +  cursor_tx] & 0xF0) >> 4;
}

int load_map( char *filename, int width, int height )
{
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

void save_map( char *filename )
{
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
		if ( isload )
		{
			printf("Loading %s ... \n",filename);
			load_map( filename, 45, 45 );
		} else {
			printf("Saving %s ... \n",filename);
			save_map( filename );
		}

		printf("\nPress any key\n");
		wait();
	}

	change_mode(gMode);
	vdp_cursor_enable( false );
	vdp_logical_scr_dims( false );

	draw_screen();

	vdp_select_sprite( CURSOR_SPRITE );
	vdp_show_sprite();

	vdp_refresh_sprites();
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
		draw_horizontal(fac.tx, fac.ty+i, gMapTW);
	}

	if (bGridLines)
	{
		vdp_gcol(0,8);
		for (int x=0; x<gMapTW*gTileSize; x+=gTileSize)
		{
			vdp_move_to(x,0);
			vdp_line_to(x,gMapTH*gTileSize-1);
		}	
		for (int y=0; y<gMapTH*gTileSize; y+=gTileSize)
		{
			vdp_move_to(0,y);
			vdp_line_to(gMapTW*gTileSize-1,y);
		}	
	}
	draw_cursor();
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
void scroll_screen(int dir)
{
	switch (dir) {
		case SCROLL_RIGHT: // scroll screen to right, view moves left
			if (fac.tx > 1)
			{
				fac.tx -= 1;
			}
			break;
		case SCROLL_LEFT: // scroll screen to left, view moves right
			if ((fac.tx + gScreenWidthTiles + 1) < fac.mapWidth)
			{
				fac.tx += 1;
			}
			break;
		case SCROLL_UP:
			if (fac.ty > 1)
			{
				fac.ty -= 1;
			}
			break;
		case SCROLL_DOWN:
			if ((fac.ty + gScreenHeightTiles + 1) < fac.mapHeight)
			{
				fac.ty += 1;
			}
			break;
		default:
			break;
	}
}


