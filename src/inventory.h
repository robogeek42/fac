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
#include "item.h"
#include "util.h"

#define MAX_INVENTORY_ITEMS 20
typedef struct {
	uint8_t item;
	int count;
} INV_ITEM;

void init_inventory(INV_ITEM *);
int find_item(INV_ITEM *inv, uint8_t item);
int add_item(INV_ITEM *inv, uint8_t item, int count);
int remove_item(INV_ITEM *inv, uint8_t item, int count);

#endif
