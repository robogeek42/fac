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

#define CHUNK_SIZE 1024

bool db = false;
int gMode = 8; 
int gScreenWidth = 320;
int gScreenHeight = 240;

int gMapWidth = 200;
int gMapHeight = 200;
int gTileSize = 16;

#define BMOFF_TILE16 0
#define BMOFF_BOB16 64
#define BMOFF_BELT16 80

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
#define KEY_W 0x46
#define KEY_A 0x30
#define KEY_S 0x42
#define KEY_D 0x33
#define KEY_C 0x32
#define KEY_P 0x3F
#define KEY_comma 0x5B
#define KEY_dot 0x59
#define KEY_lt 0x6E
#define KEY_gt 0x6F

// Position of top-left of screen in world coords (pixel)
int xpos=0, ypos=0;

uint8_t* tilemap;
uint8_t* layer1;

FILE *open_file( const char *fname, const char *mode);
int close_file( FILE *fp );
int read_str(FILE *fp, char *str, char stop);
int load_bitmap_file( const char *fname, int width, int height, int bmap_id );

void key_event_handler( KEY_EVENT key_event );
void wait_clock( clock_t ticks );
double my_atof(char *str);

void load_images();
void show_map();
void game_loop();

int getWorldCoordX(int sx) { return (xpos + sx); }
int getWorldCoordY(int sy) { return (ypos + sy); }
int getTileX(int sx) { return (sx/gTileSize); }
int getTileY(int sy) { return (sy/gTileSize); }
int getTilePosInScreenX(int tx) { return ((tx * gTileSize) - xpos); }
int getTilePosInScreenY(int ty) { return ((ty * gTileSize) - ypos); }

void draw_screen();
void scroll_screen(int dir, int step, bool updatepos);
void draw_horizontal(int tx, int ty, int len);
void draw_vertical(int tx, int ty, int len);
void recentre();

int load_map(char *mapname);

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

bool bPlace=false;
void start_place();

clock_t layer_wait_ticks;
int layer_frame = 0;
void draw_layer();
void draw_horizontal_layer(int tx, int ty, int len);

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

	//if (load_map("maps/mapmap_dry_256x256.data") != 0)
	//if (load_map("maps/myworld_wet_256x256.data") != 0)
	//if (load_map("maps/map_400x400_1234_0.4000_2_-0.130_0.000_0.400.data") != 0)
	//if (load_map("maps/tm_1234_200x200.data") != 0)
	if (load_map("maps/fmap.data") != 0)
	{
		printf("Failed to load map\n");
		goto my_exit;
	}

	layer1 = (uint8_t *) malloc(sizeof(uint8_t) * gMapWidth * gMapHeight);
	if (layer1 == NULL)
	{
		printf("Out of memory\n");
		return -1;
	}
	memset(layer1,255, gMapWidth * gMapHeight);
	layer1[20*45+20]=0;
	layer1[20*45+21]=0;
	layer1[20*45+22]=0;
	layer1[20*45+23]=7*4;
	layer1[19*45+23]=3*4;
	layer1[18*45+23]=3*4;
	layer1[17*45+23]=11*4;
	layer1[17*45+22]=2*4;
	layer1[17*45+21]=2*4;
	layer1[17*45+20]=2*4;


	for (int i=0;i<16;i++)
	{
		layer1[26*45+13+i]=i*4;
	}

	/* start screen centred */
	xpos = gTileSize*(gMapWidth - gScreenWidth/gTileSize)/2; 
	ypos = gTileSize*(gMapHeight - gScreenHeight/gTileSize)/2; 
	bobx = xpos + gTileSize*(gScreenWidth/gTileSize)/2;
	boby = ypos + gTileSize*(gScreenHeight/gTileSize)/2;

	// setup complete
	vdp_mode(gMode + (db?128:0));
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
	if (db) 
	{
		vdp_swap();
		draw_screen();
		draw_layer();
	}
	bob_wait_ticks = clock();
	bob_anim_ticks = clock();
	layer_wait_ticks = clock();

	do {
		int dir=-1;
		if ( vdp_check_key_press( KEY_LEFT ) ) {dir=SCROLL_RIGHT; }
		if ( vdp_check_key_press( KEY_RIGHT ) ) {dir=SCROLL_LEFT; }
		if ( vdp_check_key_press( KEY_UP ) ) {dir=SCROLL_UP; }
		if ( vdp_check_key_press( KEY_DOWN ) ) {dir=SCROLL_DOWN; }
		if (dir>=0 && ( move_wait_ticks < clock() ) ) {
			int bx=bobx;
			int by=boby;
			int px=xpos; int nx=xpos;
			int py=ypos; int ny=ypos;
			switch (dir) {
				case SCROLL_RIGHT: nx-=1;break;
				case SCROLL_LEFT: nx+=1;break;
				case SCROLL_UP: ny-=1;break;
				case SCROLL_DOWN: ny+=1;break;
			}
			if (db) {
				draw_bob(false,bx,by,px,py);
				scroll_screen(dir,1,false);
				draw_bob(true,bx,by,nx,ny);
				//draw_screen();
				vdp_swap();
			}
			// draw_bob(false,bx,by,px,py);
			scroll_screen(dir,1,true);
			if (bobAtEdge()) draw_bob(true,bx,by,nx,ny);
			//move_wait_ticks = clock() + 1;
		}
		if (layer_wait_ticks < clock()) 
		{
			draw_layer();
			layer_wait_ticks = clock() + 5;
		}
			
		if ( vdp_check_key_press( KEY_w ) || vdp_check_key_press( KEY_W ) ) {move_bob(BOB_UP, 1); }
		if ( vdp_check_key_press( KEY_a ) || vdp_check_key_press( KEY_A ) ) {move_bob(BOB_LEFT, 1); }
		if ( vdp_check_key_press( KEY_s ) || vdp_check_key_press( KEY_S ) ) {move_bob(BOB_DOWN, 1); }
		if ( vdp_check_key_press( KEY_d ) || vdp_check_key_press( KEY_D ) ) {move_bob(BOB_RIGHT, 1); }
		if ( vdp_check_key_press( KEY_c ) || vdp_check_key_press( KEY_C ) ) { recentre(); }
		if ( vdp_check_key_press( KEY_p ) || vdp_check_key_press( KEY_P ) ) { start_place(); }
		if ( vdp_check_key_press( 0x26 ) || vdp_check_key_press( 0x2D ) ) exit=1; // q or x

		vdp_update_key_state();
	} while (exit==0);

}


void load_images() 
{
	char fname[40];
	/*
	for (int fn=1; fn<=24; fn++)
	{
		sprintf(fname, "img/t16/tt%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_TILE16 + fn-1);
	}
	*/
	for (int fn=1; fn<=13; fn++)
	{
		sprintf(fname, "img/terr16/tr%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_TILE16 + fn-1);
	}
	for (int fn=1; fn<=16; fn++)
	{
		sprintf(fname, "img/b16/bob%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BOB16 + fn-1);
	}
	for (int fn=1; fn<=4*4; fn++)
	{
		sprintf(fname, "img/belt16/belt%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BELT16 + fn-1);
	}
	for (int fn=1; fn<=8*4; fn++)
	{
		sprintf(fname, "img/belt16/bbelt%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BELT16 + 4*4 + fn-1);
	}
	
}

void load_belts()
{
	char fname[40];
	for (int fn=1; fn<=16; fn++)
	{
		sprintf(fname, "img/belt16/belt%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BELT16 + fn-1);
	}
	for (int fn=1; fn<=32; fn++)
	{
		sprintf(fname, "img/belt16/bbelt%02d.rgb2",fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BELT16 + 16 + fn-1);
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
		vdp_select_bitmap( tilemap[ty*gMapWidth + tx+i] + BMOFF_TILE16);
		vdp_draw_bitmap( px + i*gTileSize, py );
		vdp_update_key_state();
	}
	
}
void draw_vertical(int tx, int ty, int len)
{
	int px = getTilePosInScreenX(tx);
	int py = getTilePosInScreenY(ty);

	for (int i=0; i<len; i++)
	{
		vdp_select_bitmap( tilemap[(ty+i)*gMapWidth + tx] + BMOFF_TILE16);
		vdp_draw_bitmap( px, py + i*gTileSize );
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

int load_map(char *mapname)
{
	uint8_t ret = mos_load( mapname, (uint24_t) tilemap,  gMapWidth * gMapHeight );
	return ret;
}


void draw_bob(bool draw, int bx, int by, int px, int py)
{
	if (!bShowBob) return;
	if (draw) {
		vdp_select_bitmap( BMOFF_BOB16 + bob_facing + bob_frame);
		vdp_draw_bitmap( bx-px, by-py );
	} else {
		int tx=getTileX(bx);
		int ty=getTileY(by);
		int tposx = tx*gTileSize - px;
		int tposy = ty*gTileSize - py;

		// have to re-draw the 4 tiles that are under the sprite
		vdp_select_bitmap( tilemap[ty*gMapWidth + tx] + BMOFF_TILE16 );
		vdp_draw_bitmap( tposx, tposy );

		vdp_select_bitmap( tilemap[ty*gMapWidth + tx + 1] + BMOFF_TILE16 );
		vdp_draw_bitmap( tposx + gTileSize, tposy );

		vdp_select_bitmap( tilemap[ (ty+1)*gMapWidth + tx] + BMOFF_TILE16 );
		vdp_draw_bitmap( tposx, tposy + gTileSize );

		vdp_select_bitmap( tilemap[ (ty+1)*gMapWidth + tx + 1] + BMOFF_TILE16 );
		vdp_draw_bitmap( tposx + gTileSize, tposy + gTileSize );
	}
	if (bPlace) {
	}
}

void move_bob(int dir, int speed)
{
	int newx=bobx, newy=boby;

	if ( bob_wait_ticks > clock() ) return;

	//if (bob_facing != dir*4) bob_frame=0;
	bob_facing = dir*4;
	speed *= (gTileSize/8);

	switch (dir) {
		case BOB_LEFT:
			if (bobx > speed) {
				newx -= speed;
			}
			break;
		case BOB_RIGHT:
			if (bobx < gMapWidth*gTileSize - speed) {
				newx += speed;
			}
			break;
		case BOB_UP:
			if (boby > speed) {
				newy -= speed;
			}
			break;
		case BOB_DOWN:
			if (boby < gMapHeight*gTileSize - speed) {
				newy += speed;
			}
			break;
		default: break;
	}
	if (db)
	{
		draw_bob(false,bobx,boby,xpos,ypos);
		draw_bob(true,newx,newy,xpos,ypos);
		vdp_swap();
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
	if (db) 
	{
		vdp_swap();
		draw_screen();
	}
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
	layer_frame = (layer_frame +1) % 4;
}

void draw_horizontal_layer(int tx, int ty, int len)
{
	int px=getTilePosInScreenX(tx);
	int py=getTilePosInScreenY(ty);

	for (int i=0; i<len; i++)
	{
		if ( layer1[ty*gMapWidth + tx+i] != 255 )
		{
			vdp_select_bitmap( layer1[ty*gMapWidth + tx+i] + BMOFF_BELT16 + layer_frame);
			vdp_draw_bitmap( px + i*gTileSize, py );
		}
	}
	
}
