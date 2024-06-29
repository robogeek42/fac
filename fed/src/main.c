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

int gMapTW = 14;
int gMapTH = 14;
int gViewOffX = 8;
int gViewOffY = 8;

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

clock_t key_wait_ticks;
int key_wait = 14;
clock_t move_wait_ticks;

static volatile SYSVAR *sys_vars = NULL;

int getTilePosInScreenX(int tx) { return ((tx - fac.tx) * gTileSize)+gViewOffX; }
int getTilePosInScreenY(int ty) { return ((ty - fac.ty) * gTileSize)+gViewOffY; }
void game_loop();
int getOverlayAtOffset( int tileoffset );
int getOverlayAtCursor();
void show_filedialog();
int load_map( char *filename, int width, int height );
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
void set_tile();

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
	total_bms = NUM_BM_TERR16 + NUM_BM_FEAT16;

	cursor_tx = 0;
	cursor_ty = 0;
	old_cursor_tx = 0;
	old_cursor_ty = 0;

	draw_screen();
	draw_cursor();

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

	draw_tile_menu();
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
		if ( cursor_tx < fac.tx ) { 
			old_cursor_tx=cursor_tx; cursor_tx = fac.tx; 
			dir=SCROLL_RIGHT; bRedrawCursor=true;
		}
		if ( cursor_ty < fac.ty ) {
			old_cursor_ty=cursor_ty; cursor_ty = fac.ty; 
			dir=SCROLL_UP; bRedrawCursor=true;
		}
		if ( cursor_tx >= fac.tx + gMapTW ) {
			old_cursor_tx=cursor_tx; cursor_tx = fac.tx + gMapTW-1;
			dir=SCROLL_LEFT; bRedrawCursor=true;
		}

		if ( cursor_ty >= fac.ty + gMapTH ) {
			old_cursor_ty=cursor_ty; cursor_ty = fac.ty + gMapTH-1;
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


		if ( vdp_check_key_press( KEY_g ) ) // toggle grid lines
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;

				bGridLines = !bGridLines;
				draw_screen();
			}
		}

		if ( vdp_check_key_press( KEY_d ) ) // change bank
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
				bank++; bank %= bankMax;
				draw_tile_menu();
			}
		}
		if ( vdp_check_key_press( KEY_a ) ) // change bank
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
				bank--; 
				if (bank < 0) bank += bankMax;
				draw_tile_menu();
			}
		}

		if ( vdp_check_key_press( KEY_s ) )
		{
			if (key_wait_ticks < clock()) 
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
		}
		if ( vdp_check_key_press( KEY_w ) )
		{
			if (key_wait_ticks < clock()) 
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
		}

		for ( int k = KEY_0; k <= KEY_9; k++)
		{
			if ( vdp_check_key_press( k ) )
			{
				if (key_wait_ticks < clock()) 
				{
					key_wait_ticks = clock() + key_wait;
					tselect = k - KEY_0;
					draw_tile_menu();
				}
			}
		}

		if ( vdp_check_key_press( KEY_space ) ) 
		{
			if (key_wait_ticks < clock()) 
			{
				key_wait_ticks = clock() + key_wait;
				set_tile();
			}
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
		for (int x=0; x<(gMapTW+1)*gTileSize; x+=gTileSize)
		{
			vdp_move_to(x+gViewOffX,0+gViewOffY);
			vdp_line_to(x+gViewOffX,gMapTH*gTileSize-1+gViewOffY);
		}	
		for (int y=0; y<(gMapTH+1)*gTileSize; y+=gTileSize)
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
			draw_number_lj( tx+fac.tx, (tx*gTileSize)+gViewOffX+4, gViewOffY-6); 
		}
		draw_filled_box(0,0,gViewOffX, gViewOffY+gMapTH*gTileSize,0,0);
		for (int ty=0; ty < gMapTH; ty++)
		{
			draw_number( ty+fac.ty, gViewOffX, (ty*gTileSize)+gViewOffY+6); 
		}
	}
	if (bBlockSelect)
	{
		int startx = (block_tx - fac.tx)*gTileSize + gViewOffX;
		int starty = (block_ty - fac.ty)*gTileSize + gViewOffY;
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
		// vertical lines
		for (int tx=block_tx-fac.tx; tx<MAX(cursor_tx,old_cursor_tx)+2-fac.tx; tx++)
		{
			vdp_move_to(
					(tx*gTileSize) + gViewOffX,
					(block_ty-fac.ty)*gTileSize + gViewOffY );
			vdp_line_to(
					(tx*gTileSize) + gViewOffX,
					(MAX(cursor_ty,old_cursor_ty)-fac.ty+1)*gTileSize + gViewOffY );
		}	
		// horizontal lines
		for (int ty=block_ty-fac.ty; ty<(MAX(cursor_ty,old_cursor_ty)+2)-fac.ty; ty++)
		{
			vdp_move_to(
					(block_tx-fac.tx)*gTileSize + gViewOffX,
					(ty*gTileSize) + gViewOffY);
			vdp_line_to(
					(MAX(cursor_tx,old_cursor_tx)-fac.tx+1)*gTileSize + gViewOffX,
					(ty*gTileSize) + gViewOffY);
		}	
	}
	if (bBlockSelect)
	{
		int startx = (block_tx - fac.tx)*gTileSize + gViewOffX;
		int starty = (block_ty - fac.ty)*gTileSize + gViewOffY;
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
			if (fac.tx >= tiles)
			{
				fac.tx -= tiles;
			}
			else
			{
				fac.tx = 0;
			}
			break;
		case SCROLL_LEFT: // scroll screen to left, view moves right
			if ((fac.tx + gMapTW + tiles ) < fac.mapWidth)
			{
				fac.tx += tiles;
			}
			else
			{
				fac.tx = fac.mapWidth - gMapTW;
			}
			break;
		case SCROLL_UP:
			if (fac.ty >= tiles)
			{
				fac.ty -= tiles;
			}
			else
			{
				fac.ty = 0;
			}
			break;
		case SCROLL_DOWN:
			if ((fac.ty + gMapTH + tiles ) < fac.mapHeight)
			{
				fac.ty += tiles;
			}
			else
			{
				fac.ty = fac.mapHeight - gMapTH;
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
		for (int x=cursor_tx*gTileSize; x<(cursor_tx+1)*gTileSize; x+=gTileSize)
		{
			vdp_move_to(x+gViewOffX, block_ty+gViewOffY);
			vdp_line_to(x+gViewOffX, (cursor_ty+1)*gTileSize+gViewOffY);
		}	
		// horizontal lines
		for (int y=cursor_ty*gTileSize; y<(cursor_ty+1)*gTileSize; y+=gTileSize)
		{
			vdp_move_to(block_tx+gViewOffX, y+gViewOffY);
			vdp_line_to((cursor_tx+1)*gTileSize+gViewOffX, y+gViewOffY);
		}	
	}
}

void set_tile()
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
			draw_screen();
		} else {
			int offset = cursor_tx+cursor_ty*fac.mapWidth;
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
			draw_tile_at_cursor();
		}
	}
}

