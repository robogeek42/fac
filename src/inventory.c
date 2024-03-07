/*
  vim:ts=4
  vim:sw=4
*/
#include "inventory.h"

void init_inventory(INV_ITEM *inv)
{
	for (int i=0; i<MAX_INVENTORY_ITEMS; i++)
	{
		inv[i].item = 255;
		inv[i].count = 0;
		inv[i].bmID = 0;
	}
}

int find_item(INV_ITEM *inv, uint8_t item)
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

int add_item(INV_ITEM *inv, uint8_t item, int count, uint8_t bmID)
{
	int index = find_item(inv, item);

	if (index < 0)
	{
		// could not find item type in inventory
		// find an empty slot
		index = 0;
		while (inv[index].item != 255 && index<MAX_INVENTORY_ITEMS)
		{
			index++;
		}
	}

	if (index == MAX_INVENTORY_ITEMS)
	{
		// inventory full
		return -1;
	}

	// found an empty slot or slot with this item in it
	inv[index].item = item;
	inv[index].count += count;
	inv[index].bmID = bmID;

	return index;
}
