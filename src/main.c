#include "tiles.h"
#include "wfc.h"
#include <mos_api.h>
#include <string.h>

static TILE* screen[HEIGHT_TILES][WIDTH_TILES];
TILE* screen_tiles;

FILE *open_file( const char *fname, const char *mode);
int close_file( FILE *fp );
int read_str(FILE *fp, char *str, char stop);
void init_screen();
void deinit_screen();
int load_bitmaps();
void show_screen();

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
	
	wfc_set_tile(screen, 0, 0, rand()%NUM_TILE_TYPES);

	TILE *next_tile = wfc_find_tile(screen);

	while (next_tile!=NULL)
	{
		wfc_set_tile(screen, next_tile->posx, next_tile->posy, wfc_get_rand_poss(next_tile));
		next_tile = wfc_find_tile(screen);
#if DEBUG==1
		wait(); wfc_show_debug_screen(screen); //wait();
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
		strcat(fname,mytiles[b].name);
		
		fp = open_file(fname,"rb");
		if (fp==NULL) {
			printf("Failed to open %s\n",fname);
			return -1;
		}
		if ( !(buffer = (char *)malloc( file_size ) ) ) {
			printf( "Failed to allocate %d bytes for buffer.\n",file_size );
			return -1;
		}
		mytiles[b].bufid = BM_OFFSET + b;
		vdp_adv_clear_buffer(mytiles[b].bufid);
		vdp_adv_write_block(mytiles[b].bufid, file_size);
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
	
void show_screen() 
{
	vdp_clear_screen();
	for (int y=0;y<HEIGHT_TILES;y++)
	{
		for (int x=0;x<WIDTH_TILES;x++)
		{
			display_tile(screen, x,y);
		}
	}
}


