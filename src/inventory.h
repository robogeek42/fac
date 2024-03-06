/*
  vim:ts=4
  vim:sw=4
*/
#ifndef _INVENTORY_H
#define _INVENTORY_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_INVENTORY_ITEMS 20
typedef struct {
	uint8_t type;
	int count;
	uint8_t bmID;
} INV_ITEM;

void init_inventory(INV_ITEM *);
int find_item(INV_ITEM *inv, uint8_t type);
int add_item(INV_ITEM *inv, uint8_t type, int count, uint8_t bmID);

#endif
