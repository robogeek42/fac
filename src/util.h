/*
  vim:ts=4
  vim:sw=4
*/
#ifndef _UTIL_H
#define _UTIL_H

#include "agon/vdp_vdu.h"
#include "agon/vdp_key.h"
#include <mos_api.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "util.h"
#include "assert.h"
#include "keydefines.h"

#define CHUNK_SIZE 1024

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define COL(C) vdp_set_text_colour(C)
#define TAB(X,Y) vdp_cursor_tab(X,Y)

typedef struct {
	char fname[20];
	uint8_t id;
	int nb[4];
	uint8_t key;
} TileInfoFile;

FILE *open_file( const char *fname, const char *mode );
int close_file( FILE *fp );

int read_str(FILE *fp, char *str, char stop) ;

extern uint8_t key_pressed_code;
extern uint8_t key_pressed_ascii;

void key_event_handler( KEY_EVENT key_event );
void wait_clock( clock_t ticks );
double my_atof(char *str);

int load_bitmap_file( const char *fname, int width, int height, int bmap_id );
int readTileInfoFile(char *path, TileInfoFile *tif, int items);

void draw_box(int x,int y, int w, int h, int col);
void draw_corners(int x1,int y1, int x2, int y2, int col);
void draw_filled_box(int x,int y, int w, int h, int col, int bgcol);
void draw_filled_box_centre(int x,int y, int w, int h, int col, int bgcol);

extern float *sinLUT;
extern int LUTslots;
extern float LUT_ANGLE_MULT;

void pop_sin_lookup();
float sinLU(float angle);
float cosLU(float angle);

void test_progbar();

#endif
