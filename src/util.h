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

#define KEY_LEFT 0x9A
#define KEY_RIGHT 0x9C
#define KEY_UP 0x96
#define KEY_DOWN 0x98

#define KEY_0 0x02
#define KEY_1 0x03
#define KEY_2 0x04
#define KEY_3 0x05
#define KEY_4 0x06
#define KEY_5 0x07
#define KEY_6 0x08
#define KEY_7 0x09
#define KEY_8 0x0A
#define KEY_9 0x0B

#define KEY_a 0x16
#define KEY_b 0x17
#define KEY_c 0x18
#define KEY_d 0x19
#define KEY_e 0x1A
#define KEY_f 0x1B
#define KEY_g 0x1C
#define KEY_h 0x1D
#define KEY_i 0x1E
#define KEY_j 0x1F
#define KEY_k 0x20
#define KEY_l 0x21
#define KEY_m 0x22
#define KEY_n 0x23
#define KEY_o 0x24
#define KEY_p 0x25
#define KEY_q 0x26
#define KEY_r 0x27
#define KEY_s 0x28
#define KEY_t 0x29
#define KEY_u 0x2A
#define KEY_v 0x2B
#define KEY_w 0x2C
#define KEY_x 0x2D
#define KEY_y 0x2E
#define KEY_z 0x2F

#define KEY_A 0x30
#define KEY_B 0x31
#define KEY_C 0x32
#define KEY_D 0x33
#define KEY_E 0x34
#define KEY_F 0x35
#define KEY_G 0x36
#define KEY_H 0x37
#define KEY_I 0x38
#define KEY_J 0x39
#define KEY_K 0x3A
#define KEY_L 0x3B
#define KEY_M 0x3C
#define KEY_N 0x3D
#define KEY_O 0x3E
#define KEY_P 0x3F
#define KEY_Q 0x40
#define KEY_R 0x41
#define KEY_S 0x42
#define KEY_T 0x43
#define KEY_U 0x44
#define KEY_V 0x45
#define KEY_W 0x46
#define KEY_X 0x47
#define KEY_Y 0x48
#define KEY_Z 0x49

#define KEY_comma 0x5B
#define KEY_dot 0x59
#define KEY_lt 0x6E
#define KEY_gt 0x6F


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

#endif
