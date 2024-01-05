#ifndef _WFC_H
#define _WFC_H

#include "tiles.h"
#include "agon/vdp_vdu.h"
#include "agon/vdp_key.h"
#include <stdio.h>
#include <stdlib.h>

int wfc_min(int a, int b);
void wfc_set_tile(TILE* screen[HEIGHT_TILES][WIDTH_TILES], int x, int y, uint8_t id);
void wfc_reduce_entropy(TILE* screen[HEIGHT_TILES][WIDTH_TILES], int x, int y, int nb, uint8_t res_id);
TILE *wfc_find_tile(TILE* screen[HEIGHT_TILES][WIDTH_TILES]);
uint8_t wfc_get_rand_poss(TILE *tile);
void wfc_show_debug_screen(TILE* screen[HEIGHT_TILES][WIDTH_TILES]);

#endif
