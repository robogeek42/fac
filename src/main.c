#include "agon/vdp_vdu.h"
#include "agon/vdp_key.h"
#include <stdio.h>
#include <stdlib.h>
#include <mos_api.h>
#include <string.h>

#define DEBUG 0

#define SCR_WIDTH 320
#define SCR_HEIGHT 240
#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define WIDTH_TILES (SCR_WIDTH / TILE_WIDTH)
#define HEIGHT_TILES (SCR_HEIGHT / TILE_HEIGHT)
#define NUM_TILES   (WIDTH_TILES * HEIGHT_TILES)
#define WORLD_TWIDTH (WIDTH_TILES * 4)
#define WORLD_THEIGHT (HEIGHT_TILES * 4)

#define BM_OFFSET 0xFA00

#define GRS 0
#define SEA 1

typedef struct {
	int tile;
	struct TILE_LIST_NODE *next_tile;
} TILE_LIST_NODE; 

typedef struct {
	uint8_t id;
	char name[12];
	int bufid;
	int edge[4]; // Edge type. N=0, E=1, S=2, W=3
	int land_weight; // amount of land in tile 0-4
} TILE_TYPE;

#define NUM_TILE_TYPES 16
TILE_TYPE tiles[] = {
	{ 0, "ts01.rgb2", 0,{GRS|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4)},4*20},
	{ 1, "ts02.rgb2", 1,{GRS|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4),GRS|(GRS<<4)},2},
	{ 2, "ts03.rgb2", 2,{SEA|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4),SEA|(SEA<<4)},2},
	{ 3, "ts04.rgb2", 3,{SEA|(SEA<<4),SEA|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4)},2},
	{ 4, "ts05.rgb2", 4,{GRS|(GRS<<4),GRS|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4)},2},
	{ 5, "ts06.rgb2", 5,{SEA|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4),SEA|(GRS<<4)},1},
	{ 6, "ts07.rgb2", 6,{GRS|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4),GRS|(SEA<<4)},1},
	{ 7, "ts08.rgb2", 7,{SEA|(GRS<<4),GRS|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4)},1},
	{ 8, "ts09.rgb2", 8,{SEA|(SEA<<4),SEA|(GRS<<4),SEA|(GRS<<4),SEA|(SEA<<4)},1},
	{ 9, "ts10.rgb2", 9,{SEA|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4),SEA|(SEA<<4)},0},
	{10, "ts11.rgb2",11,{GRS|(SEA<<4),SEA|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4)},3},
	{11, "ts12.rgb2",12,{GRS|(GRS<<4),GRS|(SEA<<4),GRS|(SEA<<4),GRS|(GRS<<4)},3},
	{12, "ts13.rgb2",13,{GRS|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4),GRS|(SEA<<4)},3},
	{13, "ts14.rgb2",14,{SEA|(GRS<<4),GRS|(GRS<<4),GRS|(GRS<<4),SEA|(GRS<<4)},3},
	{14, "ts15.rgb2",15,{GRS|(SEA<<4),SEA|(GRS<<4),SEA|(GRS<<4),GRS|(SEA<<4)},2},
	{15, "ts16.rgb2",16,{SEA|(GRS<<4),GRS|(SEA<<4),GRS|(SEA<<4),SEA|(GRS<<4)},2},
};

#define MAX_POSSIBLES 6
typedef struct {
	uint8_t id;
	int entropy;
	uint8_t possibles[MAX_POSSIBLES]; // list of possibles. If entropy is MAX, this is empty
	int posx;
	int posy;
} TILE;

static TILE* screen[HEIGHT_TILES][WIDTH_TILES];
TILE* screen_tiles;

#define MAX_CANDIDATES 10
TILE* candidate_arr[MAX_CANDIDATES];

FILE *open_file( const char *fname, const char *mode);
int close_file( FILE *fp );
int read_str(FILE *fp, char *str, char stop);
void init_screen();
void deinit_screen();
int load_bitmaps();
void set_tile(int x, int y, uint8_t id);
void reduce_entropy(int x, int y, int nb, uint8_t res_id);
void display_tile(int x, int y);
int mymin(int a, int b);
TILE* find_tile();
uint8_t get_rand_poss(TILE *tile);
void show_debug_screen();
void show_screen();
static int line = 0;

void wait()
{
	char k=getchar();
	if (k=='q') exit(0);
}

int main(int argc, char *argv[])
{
	int seed=0;
	if (argc>1) {
		seed=atoi(argv[1]);
	}
	srand(seed);
	vdp_vdu_init();
	if ( vdp_key_init() == -1 ) return 1;

	vdp_mode(8);

	vdp_logical_scr_dims(false);

	if (load_bitmaps() < 0) return -1;

	init_screen();
	
	//colour(15); vdp_cursor_tab(0,line++); printf("start: %dx%d\n", (WIDTH_TILES/2), (HEIGHT_TILES/2));
	//set_tile((WIDTH_TILES/2), (HEIGHT_TILES/2), rand()%NUM_TILE_TYPES);
	set_tile(0, 0, rand()%NUM_TILE_TYPES);

	TILE *next_tile = find_tile();

	while (next_tile!=NULL)
	{
		set_tile(next_tile->posx, next_tile->posy, get_rand_poss(next_tile));
		next_tile = find_tile();
#if DEBUG==1
		wait(); show_debug_screen(); //wait();
#endif
	}

	show_screen();

	// wait for a key press
	wait(); 

	deinit_screen();
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

void set_tile(int x, int y, uint8_t id)
{
#if DEBUG==1
	vdp_set_text_colour(15); vdp_cursor_tab(0,line++); printf("Set tile: %dx%d to %d\n", x,y,id);
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
void reduce_entropy(int x, int y, int nb, uint8_t res_id)
{
	int opp = (nb+2)%4; // opposite neighbour position
#if DEBUG==1
	vdp_set_text_colour(2); vdp_cursor_tab(0,line++); printf("Red: %dx%d nb=%d -> %d ent %d", x,y,nb,res_id,screen[y][x]->entropy);
#endif
	
	// case maximum entropy. possibles is not populated
	if (screen[y][x]->entropy == NUM_TILE_TYPES)
	{
		int num_match=0;
		// calculate tiles that can go here from ALL possible types
		for (uint8_t t=0;t<NUM_TILE_TYPES;t++)
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
			vdp_set_text_colour(1); vdp_cursor_tab(0,line++);printf("error num_match=0"); exit(-1);
		}
	} else {
		uint8_t arr[MAX_POSSIBLES];
		int num_match=0;
		int num_possibles =  mymin(screen[y][x]->entropy,MAX_POSSIBLES);
#if DEBUG==1
		vdp_set_text_colour(3); vdp_cursor_tab(0,line++); printf("poss "); for (int i=0; i < num_possibles; i++) {printf("%i,",screen[y][x]->possibles[i]);}
#endif

		for (int i=0; i < num_possibles; i++)
		{
			uint8_t t=screen[y][x]->possibles[i];
			if (tiles[t].edge[nb] == tiles[res_id].edge[opp])
			{
				arr[num_match] = t;
				num_match++;
			}
		}
		screen[y][x]->entropy = num_match;

		if (num_match == 0)
		{
			vdp_set_text_colour(1); vdp_cursor_tab(0,line++);printf("error num_match=0"); exit(-1);
		}

		for (int i=0;i<num_match;i++)
		{
			screen[y][x]->possibles[i]=arr[i];
		}

	}

	if (screen[y][x]->entropy == 1)
	{
#if DEBUG==1
		vdp_set_text_colour(1); vdp_cursor_tab(0,line++); printf("Resolve: %dx%d-> %d\n", x,y,screen[y][x]->possibles[0]);
#endif
		set_tile(x,y,screen[y][x]->possibles[0]);
	}
	else
	{
#if DEBUG==1
		vdp_set_text_colour(5); vdp_cursor_tab(0,line++); printf("now "); for (int i=0; i < screen[y][x]->entropy; i++) {printf("%i,",screen[y][x]->possibles[i]);}
#endif
	}
}

void display_tile(int x, int y)
{
	vdp_adv_select_bitmap(BM_OFFSET + screen[y][x]->id);
	vdp_draw_bitmap(x*TILE_WIDTH, y*TILE_HEIGHT);
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
		tiles[b].bufid = BM_OFFSET + b;
		vdp_adv_clear_buffer(tiles[b].bufid);
		vdp_adv_write_block(tiles[b].bufid, file_size);
		if ( fread( buffer, 1, file_size, fp ) != (size_t)file_size ) return 0;
		mos_puts( buffer, file_size, 0 );
		vdp_adv_select_bitmap(BM_OFFSET + b);
		vdp_adv_bitmap_from_buffer(TILE_WIDTH, TILE_HEIGHT, 1);
		close_file(fp);
	}
	free(buffer);
	//printf("done.\n");
	
/*
#if DEBUG==1
	for (int x=0; x<NUM_TILE_TYPES; x++)
	{
		vdp_cursor_tab(x*2,3); printf("%i",x);
		vdp_adv_select_bitmap(BM_OFFSET + x);
		vdp_draw_bitmap(x*TILE_WIDTH,4*8);
	}
	wait();
#endif
*/	
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
	
int mymin(int a, int b)
{
	return (a<b)?a:b;
}

// Find a tile that needs resolving next. 
// Choose first unresolved tile with entropy < max
TILE *find_tile()
{
	TILE *candidate = NULL;

	for (int i=0;i<NUM_TILES;i++) {
		int x=i%WIDTH_TILES;
		int y=i/WIDTH_TILES;
		if (screen[y][x]->entropy>0 && screen[y][x]->entropy<NUM_TILE_TYPES)
		{
			candidate = screen[y][x];
			break;
		}
	}
#if DEBUG==1
	if (candidate!=NULL) {
		vdp_set_text_colour(15); vdp_cursor_tab(0,line++); printf("find %dx%d %d\n",candidate->posx, candidate->posy, candidate->entropy);;
	}
#endif
	return candidate;
}

// weighted choice
uint8_t get_rand_poss(TILE *tile)
{
	int num_choices = 0; // tile->entropy;
	uint8_t choice = tile->possibles[0];
	int index, index_real, sum;

#if DEBUG==1
	if (tile->entropy == NUM_TILE_TYPES)
	{
		vdp_set_text_colour(6); vdp_cursor_tab(0,line++); printf("choices ALL");
	} else {
		vdp_set_text_colour(6); vdp_cursor_tab(0,line++); printf("choices "); for (int i=0; i < tile->entropy; i++) {printf("%i,",tile->possibles[i]);}
	}
#endif

	for (int i=0;i<tile->entropy; i++)
	{
		num_choices += tiles[tile->possibles[i]].land_weight+1;
	}
	index = rand() % num_choices;
#if DEBUG==1
	vdp_set_text_colour(15); vdp_cursor_tab(0,line++); printf("num_choices %d rand %d ",num_choices, index);;
#endif
	index_real = 0;
	sum=0;

	do {
		sum += tiles[tile->possibles[index_real]].land_weight+1;
		index_real++;
	} while (sum <= index);

	choice = tile->possibles[index_real-1];

#if DEBUG==1
	printf("choose n=%d %d",index_real-1, choice);;
#endif

	return choice;
}

void show_debug_screen() 
{
	vdp_clear_screen();
	for (int y=0;y<HEIGHT_TILES;y++)
	{
		for (int x=0;x<WIDTH_TILES;x++)
		{
			if (screen[y][x]->entropy == 0)
			{
				display_tile(x,y);
			} else if (screen[y][x]->entropy > 0 && screen[y][x]->entropy < NUM_TILE_TYPES)
			{
				vdp_cursor_tab(x*2,y*2); printf("%02X",screen[y][x]->entropy);
			}
		}
	}
	line=0;
}

void show_screen() 
{
	vdp_clear_screen();
	for (int y=0;y<HEIGHT_TILES;y++)
	{
		for (int x=0;x<WIDTH_TILES;x++)
		{
			display_tile(x,y);
		}
	}
	line=0;
}


