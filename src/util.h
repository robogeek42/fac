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

#define CHUNK_SIZE 1024


typedef struct {
	char fname[20];
	uint8_t id;
	int nb[4];
	uint8_t key;
} TileInfoFile;

FILE *open_file( const char *fname, const char *mode );
int close_file( FILE *fp );

int read_str(FILE *fp, char *str, char stop) ;
	
void key_event_handler( KEY_EVENT key_event );
void wait_clock( clock_t ticks );
double my_atof(char *str);

int load_bitmap_file( const char *fname, int width, int height, int bmap_id );
int readTileInfoFile(char *path, TileInfoFile *tif, int items);

#endif
