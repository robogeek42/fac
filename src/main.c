#include "agon/vdp_vdu.h"
#include "agon/vdp_key.h"
#include <stdio.h>
#include <stdlib.h>
#include <mos_api.h>
#include <string.h>
#include <time.h>

// VDU 22, n: Mode n
typedef struct { uint8_t A; uint8_t n; } MY_VDU_mode;
static MY_VDU_mode my_mode = { 22, 0 };
// VDU 12: CLS
typedef struct { uint8_t A; } MY_VDU_cls;
static MY_VDU_cls my_cls = { 12 };

// VDU 23, 27, 0, n : REM Select bitmap n (equating to buffer ID numbered 64000+n)
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t n; } MY_VDU_select_bitmap;
static MY_VDU_select_bitmap	my_select_bitmap = { 23, 27, 0, 0 };
void select_bitmap( int n );

// VDU 23, 27, 3, x; y; : REM Draw current bitmap on screen at pixel position x, y (a valid bitmap must be selected first)
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t x; uint16_t y; } MY_VDU_draw_bitmap;
static MY_VDU_draw_bitmap	my_draw_bitmap 	= { 23, 27, 3, 0, 0 };
void draw_bitmap( int x, int y );

// VDU 23, 0, &C0, n : REM Turn logical screen scaling on and off, where 1=on and 0=off (Requires MOS 1.03 or above)
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t flag; } MY_VDU_logical_scr_dims;
static MY_VDU_logical_scr_dims	my_logical_scr_dims = { 23,  0, 0xC0, 0 }; 
void logical_scr_dims( bool flag );

// VDU 23, 0 &A0, bufferId; 0 : REM write block to buffer
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t bid; uint8_t op; uint16_t length; } MY_VDU_adv_write_block;
static MY_VDU_adv_write_block  	my_adv_write_block = { 23,  0, 0xA0, 0xFA00, 0, 0};
void adv_write_block(int bufferID, int length);

// VDU 23, 0 &A0, bufferId; 2 : REM clear bitmap using bufferid
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t bid; uint8_t op; } MY_VDU_adv_clear_buffer;
static MY_VDU_adv_clear_buffer  	my_adv_clear_buffer = { 23,  0, 0xA0, 0xFA00, 2};
void adv_clear_buffer(int bufferID);

// VDU 23, 27, &20, bufferId;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t bid; } MY_VDU_adv_select_bitmap;
static MY_VDU_adv_select_bitmap	my_adv_select_bitmap = { 23, 27, 0x20, 0,};
void adv_select_bitmap(int bufferId);

// VDU 23, 27, &21, width; height; format  : REM Create bitmap from buffer
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t width; uint16_t height; uint8_t format; } MY_VDU_adv_bitmap_from_buffer;
static MY_VDU_adv_bitmap_from_buffer my_adv_bitmap_from_buffer = { 23, 27, 0x21, 0, 0, 1};
void adv_bitmap_from_buffer(int width, int height, int format);

// VDU 31, x, y: TAB(x,y)
typedef struct { uint8_t A; uint8_t x; uint8_t y; } MY_VDU_tab;
static MY_VDU_tab my_tab = { 31, 0, 0 };
void tab(int x,int y);

// VDU 17, c: COLOUR(c)
typedef struct { uint8_t A; uint8_t c; } MY_VDU_colour;
static MY_VDU_colour my_colour = { 17, 0 };
void colour(int c);

#define DEBUG 0

#define SCR_WIDTH 320
#define SCR_HEIGHT 240
#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define WIDTH_TILES (SCR_WIDTH / TILE_WIDTH)
#define HEIGHT_TILES (SCR_HEIGHT / TILE_HEIGHT)
#define NUM_TILES   (WIDTH_TILES * HEIGHT_TILES)

#define GRS 0
#define SEA 1

typedef struct {
	int tile;
	struct TILE_LIST_NODE *next_tile;
} TILE_LIST_NODE; 

typedef struct {
	int id;
	char name[12];
	int bufid;
	int edge[4]; // Edge type. N=0, E=1, S=2, W=3
} TILE_TYPE;

#define NUM_TILE_TYPES 16
TILE_TYPE tiles[] = {
	{ 0, "ts01.rgb2", 0,{GRS|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4)}},
	{ 1, "ts02.rgb2", 1,{GRS|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4),GRS|(GRS<<4)}},
	{ 2, "ts03.rgb2", 2,{SEA|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4),SEA|(SEA<<4)}},
	{ 3, "ts04.rgb2", 3,{SEA|(SEA<<4),SEA|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4)}},
	{ 4, "ts05.rgb2", 4,{GRS|(GRS<<4),GRS|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4)}},
	{ 5, "ts06.rgb2", 5,{SEA|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4),SEA|(GRS<<4)}},
	{ 6, "ts07.rgb2", 6,{GRS|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4)}},
	{ 7, "ts08.rgb2", 7,{SEA|(GRS<<4),GRS|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4)}},
	{ 8, "ts09.rgb2", 8,{SEA|(SEA<<4),SEA|(GRS<<4),SEA|(GRS<<4),SEA|(SEA<<4)}},
	{ 9, "ts10.rgb2", 9,{SEA|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4)}},
	{10, "ts11.rgb2",11,{GRS|(SEA<<4),SEA|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4)}},
	{11, "ts12.rgb2",12,{GRS|(GRS<<4),GRS|(SEA<<4),GRS|(SEA<<4),GRS|(GRS<<4)}},
	{12, "ts13.rgb2",13,{GRS|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4),GRS|(SEA<<4)}},
	{13, "ts14.rgb2",14,{SEA|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4)}},
	{14, "ts15.rgb2",15,{GRS|(SEA<<4),SEA|(GRS<<4),SEA|(GRS<<4),GRS|(SEA<<4)}},
	{15, "ts16.rgb2",16,{SEA|(GRS<<4),GRS|(SEA<<4),GRS|(SEA<<4),SEA|(GRS<<4)}},
};

#define MAX_POSSIBLES 6
typedef struct {
	int id;
	int entropy;
	uint8_t possibles[MAX_POSSIBLES]; // list of possibles. If entropy is MAX, this is empty
	int posx;
	int posy;
} TILE;

static TILE* screen[HEIGHT_TILES][WIDTH_TILES];
TILE* screen_tiles;

FILE *open_file( const char *fname, const char *mode);
int close_file( FILE *fp );
int read_str(FILE *fp, char *str, char stop);
void init_screen();
void deinit_screen();
int load_bitmaps();
void set_tile(int x, int y, int id);
void reduce_entropy(int x, int y, int nb, int res_id);
void display_tile(int x, int y);
int mymin(int a, int b);
TILE* find_tile();
int get_rand_poss(TILE *tile);
void show_debug_screen();
static int line = 0;

void wait()
{
	char k=getchar();
	if (k=='q') exit(0);
}

int main()
{
	srand(time(NULL));
	vdp_vdu_init();
	if ( vdp_key_init() == -1 ) return 1;

	int iMode = 8;

	my_mode.n = iMode; VDP_PUTS( my_mode );

	logical_scr_dims(false);

	if (load_bitmaps() <0) return -1;

	init_screen();
	
	//colour(15); tab(0,line++); printf("start: %dx%d\n", (WIDTH_TILES/2), (HEIGHT_TILES/2));
	set_tile((WIDTH_TILES/2), (HEIGHT_TILES/2), rand()%NUM_TILE_TYPES);

	TILE *next_tile = find_tile();
	while (next_tile!=NULL)
	{
		set_tile(next_tile->posx, next_tile->posy, get_rand_poss(next_tile));
		next_tile = find_tile();
#if DEBUG==1
		wait(); show_debug_screen(); wait();
#endif
	}

#if DEBUG==1
	show_debug_screen();
#endif

	deinit_screen();
	// wait for a key press
	wait(); 

	return 0;
}

void init_screen()
{
	//printf("Init screen mem %d bytes... ",NUM_TILES * sizeof(TILE));
	screen_tiles = (TILE*) malloc(NUM_TILES * sizeof(TILE));
	// init tiles to max entropy
	for (int y=0;y<HEIGHT_TILES;y++)
	{
		for (int x=0;x<WIDTH_TILES;x++)
		{
			screen[y][x]=&screen_tiles[y*WIDTH_TILES+x];
			screen[y][x]->id = -1;
			screen[y][x]->entropy = NUM_TILE_TYPES;
			screen[y][x]->posx = x;
			screen[y][x]->posy = y;
		}
	}
	//printf("done.\n");
}

void deinit_screen()
{
	//printf("Deinit screen ... ");
	free(screen_tiles);
	//printf("done.\n");

}

void set_tile(int x, int y, int id)
{
#if DEBUG==1
	colour(15); tab(0,line++); printf("Set tile: %dx%d to %d\n", x,y,id);
#endif

	screen[y][x]->id = id;
	screen[y][x]->entropy = 0;

	display_tile(x, y);

	if (y-1 >= 0) { 
		if (screen[y-1][x]->entropy>0) reduce_entropy(x, y-1, 2, id); 
	}
	if (x+1 < WIDTH_TILES) { 
		if (screen[y][x+1]->entropy>0) reduce_entropy(x+1, y, 3, id); 
	}
	if (y+1 < HEIGHT_TILES) { 
		if (screen[y+1][x]->entropy>0) reduce_entropy(x, y+1, 0, id); 
	}
	if (x-1 >= 0) { 
		if (screen[y][x-1]->entropy>0) reduce_entropy(x-1, y, 1, id); 
	}
}

/* for given tile screen[y][x], reduce entropy based on neighbour nb (0-3).
 * res_id is the resolved id of the neighbour nb
 */
void reduce_entropy(int x, int y, int nb, int res_id)
{
	int opp = (nb+2)%4; // opposite neighbour position
#if DEBUG==1
	colour(2); tab(0,line++); printf("Red: %dx%d nb=%d -> %d ent %d", x,y,nb,res_id,screen[y][x]->entropy);
#endif
	
	// case maximum entropy. possibles is not populated
	if (screen[y][x]->entropy == NUM_TILE_TYPES)
	{
		int num_match=0;
		// calculate tiles that can go here from ALL possible types
		for (int t=0;t<NUM_TILE_TYPES;t++)
		{
			if (tiles[t].edge[nb] == tiles[res_id].edge[opp])
			{
				screen[y][x]->possibles[num_match] = t;
				num_match++;
			}
		}
		screen[y][x]->entropy = num_match;
		if (num_match == 0)
		{
			colour(1); tab(0,line++);printf("error"); exit(-1);
		}
	} else {
		int arr[MAX_POSSIBLES];
		int num_match=0;
		int num_possibles =  mymin(screen[y][x]->entropy,MAX_POSSIBLES);
#if DEBUG==1
		colour(3); tab(0,line++); printf("poss "); for (int i=0; i < num_possibles; i++) {printf("%i,",screen[y][x]->possibles[i]);}
#endif

		for (int i=0; i < num_possibles; i++)
		{
			int t=screen[y][x]->possibles[i];
			if (tiles[t].edge[nb] == tiles[res_id].edge[opp])
			{
				arr[num_match] = t;
				num_match++;
			}
		}
		screen[y][x]->entropy = num_match;

		if (num_match == 0)
		{
			colour(1); tab(0,line++);printf("error"); exit(-1);
		}

		for (int i=0;i<num_match;i++)
		{
			screen[y][x]->possibles[i]=arr[i];
		}

	}

	if (screen[y][x]->entropy == 1)
	{
#if DEBUG==1
		colour(1); tab(0,line++); printf("Resolve: %dx%d-> %d\n", x,y,screen[y][x]->possibles[0]);
#endif
		set_tile(x,y,screen[y][x]->possibles[0]);
	}
	else
	{
#if DEBUG==1
		colour(5); tab(0,line++); printf("now "); for (int i=0; i < screen[y][x]->entropy; i++) {printf("%i,",screen[y][x]->possibles[i]);}
#endif
	}
}

void display_tile(int x, int y)
{
	select_bitmap(screen[y][x]->id+0xFA00);
	draw_bitmap(x*TILE_WIDTH, y*TILE_HEIGHT);
}

int load_bitmaps()
{
	char fname[20];
	char dir[] = "img/";
	FILE *fp;
	char *buffer;
	int file_size = TILE_WIDTH*TILE_HEIGHT;

	//printf("Load bitmaps ... ");
	for(int b=0;b<NUM_TILE_TYPES;b++)
	{
		fname[0]=0;
		strcat(fname,dir);
		strcat(fname,tiles[b].name);
		
		fp = open_file(fname,"rb");
		if (fp==NULL) {
			printf("Failed to open %s\n",fname);
			return -1;
		}
		if ( !(buffer = (char *)malloc( file_size ) ) ) {
			printf( "Failed to allocate %d bytes for buffer.\n",file_size );
			return -1;
		}
		tiles[b].bufid = 0xFA00+b;
		adv_clear_buffer(tiles[b].bufid);
		adv_write_block(tiles[b].bufid, file_size);
		if ( fread( buffer, 1, file_size, fp ) != (size_t)file_size ) return 0;
		mos_puts( buffer, file_size, 0 );
		adv_select_bitmap(0xFA00+b);
		adv_bitmap_from_buffer(TILE_WIDTH, TILE_HEIGHT, 1);
		close_file(fp);
	}
	free(buffer);
	//printf("done.\n");
	
#if DEBUG==1
	for (int x=0; x<NUM_TILE_TYPES; x++)
	{
		tab(x*2,3); printf("%i",x);
		select_bitmap(x+0xFA00);
		draw_bitmap(x*TILE_WIDTH,4*8);
	}
	wait();
#endif
	
	return 0;
}


/*
 * File IO and parsing of config file
 */

// Open file
FILE *open_file( const char *fname, const char *mode )
{

	FILE *fp ;

	if ( !(fp = fopen( fname, mode )) ) {
		printf( "Error opening file %s\r\n", fname);
		return NULL;
	}
	return fp;
}

// Close file
int close_file( FILE *fp )
{
	if ( fclose( fp ) ){
		puts( "Error closing file.\r\n" );
		return EOF;
	}
	return 0;
}

int read_str(FILE *fp, char *str, char stop) {
	char c;
	int cnt=0, bytes=0;
	do {
		bytes = fread(&c, 1, 1, fp);
		if (bytes != 0 && c != stop) {
			str[cnt++]=c;
		}
	} while (c != stop && bytes != 0);
	str[cnt++]=0;
	return bytes;
}
	
/*
 * My VDP commands 
 */

void select_bitmap( int n )
{
	my_select_bitmap.n = n;
	VDP_PUTS(my_select_bitmap);
}

void logical_scr_dims( bool flag )
{
	my_logical_scr_dims.flag = 0;
	if ( flag ) my_logical_scr_dims.flag = 1;
	VDP_PUTS(my_logical_scr_dims);
}

void draw_bitmap( int x, int y )
{
	my_draw_bitmap.x = x;
	my_draw_bitmap.y = y;
	VDP_PUTS(my_draw_bitmap);
}

void adv_write_block(int bufferID, int length)
{
	my_adv_write_block.bid = bufferID;
	my_adv_write_block.length = length;
	VDP_PUTS(my_adv_write_block);
}

void adv_clear_buffer(int bufferID)
{
	my_adv_clear_buffer.bid = bufferID;
	VDP_PUTS(my_adv_clear_buffer);
}

void adv_select_bitmap(int bufferID)
{
	my_adv_select_bitmap.bid = bufferID;
	VDP_PUTS(my_adv_select_bitmap);
}

void adv_bitmap_from_buffer(int width, int height, int format)
{
	my_adv_bitmap_from_buffer.width = width;
	my_adv_bitmap_from_buffer.height = height;
	my_adv_bitmap_from_buffer.format = format; // RGBA2=1
	VDP_PUTS(my_adv_bitmap_from_buffer);
}

void tab(int x, int y)
{
	my_tab.x = x;
	my_tab.y = y;
	VDP_PUTS(my_tab);
}

void colour(int c)
{
	my_colour.c = c;
	VDP_PUTS(my_colour);
}

int mymin(int a, int b)
{
	return (a<b)?a:b;
}

TILE *find_tile()
{
	TILE *candidate = NULL;

	for (int y=0;y<HEIGHT_TILES;y++)
	{
		for (int x=0;x<WIDTH_TILES;x++)
		{
			if (screen[y][x]->entropy>0 && screen[y][x]->entropy<NUM_TILE_TYPES)
			{
				candidate = screen[y][x];
				break;
			}
		}
	}
#if DEBUG==1
	colour(15); tab(0,line++); printf("find %dx%d %d\n",candidate->posx, candidate->posy, candidate->entropy);;
#endif
	return candidate;
}

int get_rand_poss(TILE *tile)
{
	int index = rand() % tile->entropy;
	return tile->possibles[index];
}

void show_debug_screen() 
{
	VDP_PUTS(my_cls);
	for (int y=0;y<HEIGHT_TILES;y++)
	{
		for (int x=0;x<WIDTH_TILES;x++)
		{
			if (screen[y][x]->entropy == 0)
			{
				display_tile(x,y);
			} else if (screen[y][x]->entropy > 0 && screen[y][x]->entropy < NUM_TILE_TYPES)
			{
				tab(x*2,y*2); printf("%02X",screen[y][x]->entropy);
			}
		}
	}
	line=0;
}


