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

#define IT_BELT 0
#define IT_STONE 1
#define IT_IRON_ORE 2
#define IT_COPPER_ORE 3
#define IT_COAL 4
#define IT_WOOD 5
#define IT_IRON_PLATE 6
#define IT_COPPER_PLATE 7
#define IT_STONE_BRICK 8
#define IT_FURNACE 9
#define IT_MINER 10
#define IT_ASSEMBLER 11
#define IT_FEAT_STONE 12
#define IT_FEAT_IRON 13
#define IT_FEAT_COPPER 14
#define IT_FEAT_COAL 15
#define IT_FEAT_WOOD 16

static ItemType itemtypes[] = {
//                                     Belt  Machine Resouce Overlay
// belts are special
	{IT_BELT, 			"Belts", 		true, false, false, false, BMOFF_BELT16, BM_SIZE16 },
// raw items
	{IT_STONE,			"Stone",		false, false, true, false, BMOFF_ITEM8+4, BM_SIZE8 },
	{IT_IRON_ORE,		"Iron Ore", 	false, false, true, false, BMOFF_ITEM8+5, BM_SIZE8 },
	{IT_COPPER_ORE,		"Copper Ore",	false, false, true, false, BMOFF_ITEM8+6, BM_SIZE8 },
	{IT_COAL,			"Coal",			false, false, true, false, BMOFF_ITEM8+7, BM_SIZE8 },
	{IT_WOOD,			"Wood",			false, false, true, false, BMOFF_ITEM8+0, BM_SIZE8 },
// processed items
	{IT_IRON_PLATE,		"Iron Plate",	false, false, true, false, BMOFF_ITEM8+1, BM_SIZE8 },
	{IT_COPPER_PLATE,	"Copper Plate", false, false, true, false, BMOFF_ITEM8+2, BM_SIZE8 },
	{IT_STONE_BRICK,	"Stone brick",	false, false, true, false, BMOFF_ITEM8+3, BM_SIZE8 },
// Machines
    {IT_FURNACE,		"Furnace",		false, true, false, false, BMOFF_MACH16+0, BM_SIZE16 },
    {IT_MINER,			"Miner",		false, true, false, false, BMOFF_MACH16+1, BM_SIZE16 },
    {IT_ASSEMBLER,		"Assembler",	false, true, false, false, BMOFF_MACH16+3, BM_SIZE16 },
// Overlays
	{IT_FEAT_STONE,		"Stone",		false, false, false, true, BMOFF_FEAT16+0, BM_SIZE16 },
	{IT_FEAT_IRON,		"Iron Ore", 	false, false, false, true, BMOFF_FEAT16+1, BM_SIZE16 },
	{IT_FEAT_COPPER,	"Copper Ore",	false, false, false, true, BMOFF_FEAT16+2, BM_SIZE16 },
	{IT_FEAT_COAL,		"Coal",			false, false, false, true, BMOFF_FEAT16+3, BM_SIZE16 },
	{IT_FEAT_WOOD,		"Tree",			false, false, false, true, BMOFF_FEAT16+4, BM_SIZE16 },

};

bool isBelt(int item);
bool isMachine(int item);
bool isResource(int item);
bool isOverlay(int item);

#endif

