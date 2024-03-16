/*
  vim:ts=4
  vim:sw=4
*/
#ifndef _ITEM_H
#define _ITEM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "images.h"

// data structure for Node
// data -> the actual value
// next -> address of the next node

struct ItemNode {
	uint8_t item;
	int x;
	int y;
	struct ItemNode *next;
};

typedef struct ItemNode *ItemNodePtr;

// check if the list is empty
bool isEmptyItemList();

// Insert data at the front of the list
void insertAtFrontItemList(uint8_t item, int x, int y);
	
// insert data at the back of the linked list
void insertAtBackItemList(uint8_t item, int x, int y);
	
// returns the data at first node 
ItemNodePtr topFrontItem();

// returns the data at last node 
ItemNodePtr topBackItem();

// removes the item at front of the linked list and return 
ItemNodePtr popFrontItem();

// remove the item at the back of the linked list and return 
ItemNodePtr popBackItem();

// print the linked list 
void printItemList();

// check if a key is in the list
bool findItem(uint8_t key, int keyx, int keyy);

#define BM_SIZE16 0
#define BM_SIZE8 1

typedef struct {
	uint8_t item;
	char desc[40];
	bool isBelt;		// special handling for belts
	bool isMachine;		// can convert resources
	bool isResource;	// general resource type
	bool isOverlay;		// background feature - can't place these
	int bmID;
	int size; // 16x16=0, 8x8=1
} ItemType; 

static ItemType itemtypes[] = {
// belts are special
	{0, "Belts", 		true, false, false, false, BMOFF_BELT16, BM_SIZE16 },
// raw items
	{1, "Stone",		false, false, true, false, BMOFF_ITEM8+4, BM_SIZE8 },
	{2, "Iron Ore", 	false, false, true, false, BMOFF_ITEM8+5, BM_SIZE8 },
	{3, "Copper Ore",	false, false, true, false, BMOFF_ITEM8+6, BM_SIZE8 },
	{4, "Coal",			false, false, true, false, BMOFF_ITEM8+7, BM_SIZE8 },
	{5, "Wood",			false, false, true, false, BMOFF_ITEM8+0, BM_SIZE8 },
// processed items
	{6, "Iron Plate",	false, false, true, false, BMOFF_ITEM8+1, BM_SIZE8 },
	{7, "Copper Plate", false, false, true, false, BMOFF_ITEM8+2, BM_SIZE8 },
	{8, "Stone brick",	false, false, true, false, BMOFF_ITEM8+3, BM_SIZE8 },
// Machines
    {9, "Furnace",		false, true, false, false, BMOFF_MACH16+0, BM_SIZE16 },
    {10, "Miner",		false, true, false, false, BMOFF_MACH16+1, BM_SIZE16 },
    {11, "Assembler",	false, true, false, false, BMOFF_MACH16+3, BM_SIZE16 },
// Overlays
	{12, "Stone",		false, false, false, true, BMOFF_FEAT16+0, BM_SIZE16 },
	{13, "Iron Ore", 	false, false, false, true, BMOFF_FEAT16+1, BM_SIZE16 },
	{14, "Copper Ore",	false, false, false, true, BMOFF_FEAT16+2, BM_SIZE16 },
	{15, "Coal",		false, false, false, true, BMOFF_FEAT16+3, BM_SIZE16 },
	{16, "Wood",		false, false, false, true, BMOFF_FEAT16+4, BM_SIZE16 },

};

#endif

