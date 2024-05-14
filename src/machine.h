/*
  vim:ts=4
  vim:sw=4
*/
#ifndef _MACHINE_H
#define _MACHINE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define MACHINE_FURNACE 0

typedef struct {
	uint8_t machine;
	uint8_t in_items[5];
	uint8_t out_items[5];
	int time;
} Furnace;

Furnace furnace[] = {
	{ MACHINE_FURNACE, 
		{ IT_STONE, IT_IRON_ORE, IT_COPPER_ORE, -1, -1 },
   		{ IT_STONE_BRICK, IT_IRON_PLATE, IT_COPPER_PLATE, -1, -1 },
		4
	},
};
#endif

#ifdef _MACHINE_IMPLEMENTATION

#endif
