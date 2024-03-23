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

// add count items to inventory in slot containing that item
// if not found find a slot and add
// return -1 if inventory is full
int add_item(INV_ITEM *inv, uint8_t item, int count)
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
		/*
		if (index != MAX_INVENTORY_ITEMS)
		{
			printf("found slot %d\n",index);
		}
		*/
	}

	if (index == MAX_INVENTORY_ITEMS)
	{
		// inventory full
		return -1;
	}

	// found an empty slot or slot with this item in it
	inv[index].item = item;
	inv[index].count += count;
	//printf("add item %d at ind %d: bm %d\n",item, index, itemtypes[item].bmID);

	return index;
}

// reduce item count by count
// if not found return -1, or index of item in inventory
int remove_item(INV_ITEM *inv, uint8_t item, int count)
{
	int index = find_item(inv, item);
	if ( index < 0 )
	{
		return index;
	}
	inv[index].count -= count;
	if ( inv[index].count == 0 )
	{
		inv[index].item = 255;
	}
	return index;
}

