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
	if ( key_event.code == 0x7d ) {
		vdp_mode(0);
		vdp_cursor_enable( true );
		vdp_logical_scr_dims(true);
		exit( 1 );						// Exit program if esc pressed
	}

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
	FILE *fp;
	char *buffer;
	int bytes_remain = width * height;

	if ( !(buffer = (char *)malloc( CHUNK_SIZE ) ) ) {
		printf( "Failed to allocate %d bytes for buffer.\n",CHUNK_SIZE );
		return -1;
	}
	if ( !(fp = fopen( fname, "rb" ) ) ) {
		printf( "Error opening file \"%s\". Quitting.\n", fname );
		return -1;
	}

	vdp_adv_clear_buffer(bmap_id);

	bytes_remain = width * height;

	while (bytes_remain > 0)
	{
		int size = (bytes_remain>CHUNK_SIZE)?CHUNK_SIZE:bytes_remain;

		vdp_adv_write_block(bmap_id, size);

		if ( fread( buffer, 1, size, fp ) != (size_t)size ) return 0;
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

