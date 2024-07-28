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

#define MAX_INVENTORY_ITEMS 25
typedef struct {
	int item;
	int count;
} INV_ITEM;

#define INVENTORY_EMPTY_SLOT -1

void inventory_init(INV_ITEM *);
int inventory_find_item(INV_ITEM *inv, uint8_t item);
int inventory_add_item(INV_ITEM *inv, uint8_t item, int count);
int inventory_remove_item(INV_ITEM *inv, uint8_t item, int count);

#endif

#ifdef _INVENTORY_IMPLEMENTATION

void inventory_init(INV_ITEM *inv)
{
	for (int i=0; i<MAX_INVENTORY_ITEMS; i++)
	{
		inv[i].item = INVENTORY_EMPTY_SLOT;
		inv[i].count = 0;
	}
}

int inventory_find_item(INV_ITEM *inv, uint8_t item)
{
	int index = 0;
	while (inv[index].item != item && index<MAX_INVENTORY_ITEMS)
	{
		index++;
	}

	if (index == MAX_INVENTORY_ITEMS)
	{
		// not found
		return -1;
	}

	return index;
}

// add count items to inventory in slot containing that item
// if not found find a slot and add
// return -1 if inventory is full
int inventory_add_item(INV_ITEM *inv, uint8_t item, int count)
{
	int index = inventory_find_item(inv, item);
	if ( count <= 0) return index;

	// could not find item type in inventory
	if (index < 0)
	{
		// find an empty slot
		index = 0;
		while (inv[index].item != INVENTORY_EMPTY_SLOT && index<MAX_INVENTORY_ITEMS)
		{
			index++;
		}
		/*
		if (index != MAX_INVENTORY_ITEMS)
		{
			printf("found slot %d\n",index);
		}
		*/
	}

	// inventory full
	if (index == MAX_INVENTORY_ITEMS)
	{
		return -1;
	}

	// found an empty slot or slot with this item in it
	inv[index].item = item;
	inv[index].count += count;
	//printf("add item %d at ind %d: bm %d\n",item, index, itemtypes[item].bmID);

	// update the global count of the target item
	if ( item == target_item_type ) 
	{
		target_item_count += count;
	}
	return index;
}

// reduce item count by count
// if not found return -1, or index of item in inventory
int inventory_remove_item(INV_ITEM *inv, uint8_t item, int count)
{
	int index = inventory_find_item(inv, item);
	// not found
	if ( index < 0 )
	{
		return index;
	}
	inv[index].count -= count;

	// update the global count of the target item
	if ( item == target_item_type ) 
	{
		target_item_count -= count;
	}

	// deleted all the items ...
	if ( inv[index].count <= 0 )
	{
		inv[index].item = INVENTORY_EMPTY_SLOT;
		inv[index].count = 0;
	}
	return index;
}


#endif
