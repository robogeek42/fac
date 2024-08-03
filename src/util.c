/*
  vim:ts=4
  vim:sw=4
*/
#include "util.h"

#include <stdint.h>
#include "agon/vdp_vdu.h"
#include "agon/vdp_key.h"
#include <mos_api.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include "util.h"

typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint16_t x; uint16_t y; } VDU_A_B_CMD_x_y;
static VDU_A_B_CMD_x_y vdu_read_pixel_colour = { 23, 0, 0x84, 0, 0 };

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

uint8_t key_pressed_code;
uint8_t key_pressed_ascii;
	
static KEY_EVENT prev_key_event = { 0 };
void key_event_handler( KEY_EVENT key_event )
{
	/*
	if ( key_event.code == 0x7d ) {
		vdp_mode(0);
		vdp_cursor_enable( true );
		vdp_logical_scr_dims(true);
		exit( 1 );						// Exit program if esc pressed
	}
	*/

	if ( key_event.key_data == prev_key_event.key_data ) return;
	prev_key_event = key_event;

	if (key_event.down)
	{	
		key_pressed_ascii = key_event.ascii; 
		key_pressed_code = key_event.code; 
	} else {
		key_pressed_ascii = 0;
		key_pressed_code = 0;
	}
}

void wait_clock( clock_t ticks )
{
	clock_t ticks_now = clock();

	do {
		vdp_update_key_state();
	} while ( clock() - ticks_now < ticks );
}

double my_atof(char *str)
{
	double f = 0.0;
	sscanf(str, "%lf", &f);
	return f;
}


int load_bitmap_file( const char *fname, int width, int height, int bmap_id )
{
	FILE *fp = NULL;
	char *buffer;
	int bytes_remain = width * height;

	if ( !(buffer = (char *)malloc( CHUNK_SIZE ) ) ) {
		printf( "Failed to allocate %d bytes for buffer.\n",CHUNK_SIZE );
		return -1;
	}

	fp = fopen( fname, "rb" );
	if ( fp == NULL )  {
		printf( "Error opening file \"%s\". Quitting.\n", fname );
		getch();
		return -1;
	}

	vdp_adv_clear_buffer(bmap_id);

	bytes_remain = width * height;

	while (bytes_remain > 0)
	{
		int size = (bytes_remain>CHUNK_SIZE)?CHUNK_SIZE:bytes_remain;

		vdp_adv_write_block(bmap_id, size);

		if ( fread( buffer, 1, size, fp ) != (size_t)size ) return -1;
		mos_puts( buffer, size, 0 );
		//printf(".");

		bytes_remain -= size;
	}
	vdp_adv_consolidate(bmap_id);

	vdp_adv_select_bitmap(bmap_id);
	vdp_adv_bitmap_from_buffer(width, height, 1); // RGBA2
	
	fclose( fp );
	free( buffer );

	return bmap_id;
}

int readTileInfoFile(char *path, TileInfoFile *tif, int items)
{
	char line[40];
	int tif_lines = items;
	FILE *fp = open_file(path, "r");
	if (fp == NULL) { return false; }

	// mode where we tell caller how many lines are in the file so it can malloc
	if (tif == NULL)
	{
		tif_lines = 0;
		while (fgets(line, 40, fp))
		{
			if (line[0] == '#') continue;
			tif_lines++;
		}
		close_file(fp);
		return tif_lines;
	}
	
	// write lines to tif
	fp = open_file(path, "r");
	if (fp == NULL) { return false; }

	int item=0;
	while (fgets(line, 40, fp))
	{
		if (line[0] == '#') continue;

		char *pch[8]; int i=0;

		// get first token
		pch[i] = strtok(line, ",");
		while (pch[i] != NULL)
		{
			// get next token
			i++;
			pch[i] = strtok(NULL,",");
		}
		strncpy(tif[item].fname,pch[0],20);
		tif[item].id = (uint8_t) atoi(pch[1]);
		tif[item].nb[0] = atoi(pch[2]);
		tif[item].nb[1] = atoi(pch[3]);
		tif[item].nb[2] = atoi(pch[4]);
		tif[item].nb[3] = atoi(pch[5]);

		item++;
	}

	close_file(fp);
	return item;
}

void draw_box(int x,int y, int w, int h, int col)
{
	vdp_gcol( 0, col );
	vdp_move_to( x, y );
	vdp_line_to( x, y+h );
	vdp_line_to( x+w, y+h );
	vdp_line_to( x+w, y );
	vdp_line_to( x, y );
}

void draw_corners(int x1,int y1, int w, int h, int col)
{
	int cl=2;
	int x2 = x1+w;
	int y2 = y1+h;
	vdp_gcol( 0, col );
	vdp_move_to( x1, y1 ); vdp_line_to( x1+cl, y1 ); vdp_move_to( x1, y1 ); vdp_line_to( x1, y1+cl );
	vdp_move_to( x2, y1 ); vdp_line_to( x2-cl, y1 ); vdp_move_to( x2, y1 ); vdp_line_to( x2, y1+cl );
	vdp_move_to( x1, y2 ); vdp_line_to( x1, y2-cl ); vdp_move_to( x1, y2 ); vdp_line_to( x1+cl, y2 );
	vdp_move_to( x2, y2 ); vdp_line_to( x2, y2-cl ); vdp_move_to( x2, y2 ); vdp_line_to( x2-cl, y2 );
}

void draw_filled_box(int x,int y, int w, int h, int col, int bgcol)
{
	vdp_gcol( 0, bgcol );
	vdp_move_to( x, y );
	vdp_filled_rect( x+w, y+h );
	// border
	if (col != bgcol)
	{
		vdp_gcol( 0, col );
		vdp_move_to( x, y );
		vdp_line_to( x, y+h );
		vdp_line_to( x+w, y+h );
		vdp_line_to( x+w, y );
		vdp_line_to( x, y );
	}
}

void draw_filled_box_centre(int x,int y, int w, int h, int col, int bgcol)
{
	int x1 = x-w/2;
	int y1 = y-h/2;

	draw_filled_box(x1, y1, w, h, col, bgcol);
}

void pop_sin_lookup()
{
	LUT_ANGLE_MULT = LUTslots / M_PI_2;
	sinLUT = (float*) malloc(LUTslots*sizeof(float));
	if (!sinLUT)
	{
		printf("out of memory\n");
		exit(-1);
	}
	float angle = 0.0f;
	float inc = M_PI_2/LUTslots;
	int index=0;
	while ( index < LUTslots )
	{
		sinLUT[index++] = sin(angle);
		angle += inc;
	}
}

float sinLU(float angle)
{
	int ind;
	if (!sinLUT) return 0;
	while (angle>M_PI*2)
	{
		angle -= M_PI*2;
	}
	while (angle < 0.0f)
	{
		angle += M_PI*2;
	}
	if (angle > -0.005 && angle < 0.005) return 0.0f;
	if (angle > M_PI_2-0.005 && angle < M_PI_2+0.005) return 1.0f;
	if (angle > M_PI-0.005 && angle < M_PI+0.005) return 0.0f;
	if (angle > M_PI+M_PI_2-0.005 && angle < M_PI+M_PI_2+0.005) return -1.0f;
	if (angle > M_PI+M_PI-0.005 && angle < M_PI+M_PI+0.005) return 0.0f;
	if (angle < M_PI_2)
	{
		ind = (int) floorf(angle * LUT_ANGLE_MULT);
		assert(ind < LUTslots);
		return sinLUT[ind];
	}
	if (angle < M_PI)
	{
		ind = (int) floorf((M_PI- angle) * LUT_ANGLE_MULT);
		assert(ind < LUTslots);
		return sinLUT[ind];
	}
	angle -= M_PI;
	if (angle < M_PI_2)
	{
		ind = (int) floorf(angle * LUT_ANGLE_MULT);
		assert(ind < LUTslots);
		return -1.0f*sinLUT[ind];
	}
	if (angle < M_PI)
	{
		ind = (int) floorf((M_PI - angle) * LUT_ANGLE_MULT);
		assert(ind < LUTslots);
		return -1.0f*sinLUT[ind];
	}
	return 0;
}
float cosLU(float angle)
{
	int ind;
	if (!sinLUT) return 0;
	while (angle>M_PI*2)
	{
		angle -= M_PI*2;
	}
	while (angle < 0.0f)
	{
		angle += M_PI*2;
	}
	if (angle > -0.005 && angle < 0.005) return 1.0f;
	if (angle > M_PI_2-0.005 && angle < M_PI_2+0.005) return 0.0f;
	if (angle > M_PI-0.005 && angle < M_PI+0.005) return -1.0f;
	if (angle > M_PI+M_PI_2-0.005 && angle < M_PI+M_PI_2+0.005) return 0.0f;
	if (angle > M_PI+M_PI-0.005 && angle < M_PI+M_PI+0.005) return 1.0f;
	if (angle < M_PI_2)
	{
		ind = (int) floorf((M_PI_2 - angle) * LUT_ANGLE_MULT);
		assert(ind < LUTslots);
		return sinLUT[ind];
	}
	if (angle < M_PI)
	{
		ind = (int) floorf((angle - M_PI_2) * LUT_ANGLE_MULT);
		assert(ind < LUTslots);
		return -1.0f*sinLUT[ind];
	}
	angle -= M_PI;
	if (angle < M_PI_2)
	{
		ind = (int) floorf((M_PI_2 - angle) * LUT_ANGLE_MULT);
		assert(ind < LUTslots);
		return -1.0f*sinLUT[ind];
	}
	if (angle < M_PI)
	{
		ind = (int) floorf((angle - M_PI_2) * LUT_ANGLE_MULT);
		assert(ind < LUTslots);
		return sinLUT[ind];
	}
	return 0;
}

uint24_t readPixelColour(volatile SYSVAR *sys_vars, int x, int y)
{
	uint24_t pixel = 0;
	sys_vars->vdp_pflags = 0;

	vdu_read_pixel_colour.x = x;
	vdu_read_pixel_colour.y = y;
	VDP_PUTS( vdu_read_pixel_colour );
	while ( !(sys_vars->vdp_pflags & vdp_pflag_point) ) {
		vdp_update_key_state();
	};

	pixel = getsysvar_scrpixel();
	return pixel;
}

void clear_line(int y)
{
	TAB(0,y);
	for ( int i=0; i<40; i++ )
	{
		printf(" ");
	}
}
int input_int(int x, int y, char *msg)
{
	int num;
	TAB(x,y);
	printf("%s:",msg);
	scanf("%d",&num);
	clear_line(y);
	return num;
}
int input_int_noclear(int x, int y, char *msg)
{
	int num;
	TAB(x,y);
	printf("%s:",msg);
	scanf("%d",&num);
	return num;
}
char input_char(int x, int y, char *msg)
{
	char buf[30];
	TAB(x,y);
	printf("%s:",msg);
	scanf("%[^\n]s",buf);
	clear_line(y);
	return buf[0];
}
char input_char_noclear(int x, int y, char *msg)
{
	char buf[30];
	TAB(x,y);
	printf("%s:",msg);
	scanf("%[^\n]s",buf);
	return buf[0];
}

char * getline(void) {
    char * line = malloc(100), * linep = line;
    size_t lenmax = 100, len = lenmax;
    int c;

    if(line == NULL)
        return NULL;

    for(;;) {
        c = fgetc(stdin);
        if(c == EOF)
            break;

		if(c == 0x7F)
		{
			if (line > linep)
			{
				line--;
				continue;
			}
		}

        if(--len == 0) {
            len = lenmax;
            char * linen = realloc(linep, lenmax *= 2);

            if(linen == NULL) {
                free(linep);
                return NULL;
            }
            line = linen + (line - linep);
            linep = linen;
        }

        if((*line++ = c) == '\n')
            break;
    }
    *line = '\0';
    return linep;
}
void input_string(int x, int y, char *msg, char *input, unsigned int max)
{
	char *buffer;

	TAB(x,y);
	printf("%s:",msg);
	buffer = getline();
	if (strlen(buffer)>max)
	{
		strncpy(input, buffer, max);
		input[max-1]=0;
	}
	else
	{
		strcpy(input, buffer);
	}
	free(buffer);
	clear_line(y);
}
void input_string_noclear(int x, int y, char *msg, char *input, unsigned int max)
{
	char *buffer;

	TAB(x,y);
	printf("%s:",msg);
	buffer = getline();
	if (strlen(buffer)>max)
	{
		strncpy(input, buffer, max);
		input[max-1]=0;
	}
	else
	{
		strcpy(input, buffer);
	}
	free(buffer);
}


uint8_t wait_for_key(uint8_t key)
{
	bool exit_loop = false;
	do 
	{
		if ( vdp_check_key_press( key ) )
		{
			exit_loop = true;
			// wait so we don't register this press multiple times
			do {
				vdp_update_key_state();
			} while ( vdp_check_key_press( key ) );
		}
		vdp_update_key_state();
	} while ( !exit_loop );

	return key;
}

uint8_t wait_for_key_with_exit(uint8_t key, uint8_t exit_key)
{
	bool exit_loop = false;
	bool pressed = false;
	do 
	{
		exit_loop = vdp_check_key_press( exit_key );

		if ( exit_loop )
		{
			// wait so we don't register this press multiple times
			do {
				vdp_update_key_state();
			} while ( vdp_check_key_press( exit_key ) );

		}

		pressed = vdp_check_key_press( key );

		if ( pressed )
		{
			exit_loop = true;
			
			// wait so we don't register this press multiple times
			do {
				vdp_update_key_state();
			} while ( vdp_check_key_press( key ) );
		}

		vdp_update_key_state();
	} while ( !exit_loop );

	if (pressed)
	{
		return key;
	} else {
		return 0;
	}
}

uint8_t wait_for_any_key()
{
	do 
	{
		vdp_update_key_state();
	} while(key_pressed_code == 0);
	int key = key_pressed_code;
	while( vdp_check_key_press(key)) vdp_update_key_state();
	return key;
}

bool wait_for_any_key_with_exit(uint8_t exit_key)
{
	do 
	{
		vdp_update_key_state();
	} while(key_pressed_code == 0);
	int key = key_pressed_code;
	while( vdp_check_key_press(key)) vdp_update_key_state();
	if ( key == exit_key) return false;
	return true;
}

bool wait_for_any_key_with_exit_timeout(uint8_t exit_key, int timeout)
{
	clock_t timeout_ticks = clock()+timeout;
	do 
	{
		vdp_update_key_state();
	} while( key_pressed_code == 0 && timeout_ticks > clock() );
	if ( key_pressed_code == exit_key) return false;
	return true;
}

void delay(int timeout)
{
	clock_t timeout_ticks = clock()+timeout;
	do 
	{
		vdp_update_key_state();
	} while( timeout_ticks > clock() );
}

Position copyPosition( Position src )
{
	Position dest;
	dest.x=src.x;
	dest.y=src.y;
	return dest;
}
Position setPosition( int x, int y )
{
	Position dest;
	dest.x = x;
	dest.y = y;
	return dest;
}
Position addConstPosition( int val )
{
	Position dest;
	dest.x += val;
	dest.y += val;
	return dest;
}
Position addPosition( Position a, Position b )
{
	Position dest;
	dest.x = a.x + b.x;
	dest.y = a.y + b.y;
	return dest;
}
Position addPositionXY( Position a, int x, int y )
{
	Position dest;
	dest.x = a.x + x;
	dest.y = a.y + y;
	return dest;
}
void addToPosition( Position dest, Position src )
{
	dest.x += src.x;
	dest.y += src.y;
}
void addToPositionXY( Position dest, int x, int y )
{
	dest.x += x;
	dest.y += y;
}
