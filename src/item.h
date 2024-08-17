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

// (x, y) is absolute pixel position in map
// (x - fax.xpos, y - fac.ypos) gives sceen pixel position
// 
struct ItemNode {
	uint8_t item;
	int x;
	int y;
	struct ItemNode *next;
};
typedef struct ItemNode *ItemNodePtr;

typedef struct {
	uint8_t item;
	int x;
	int y;
} ItemNodeSave;

// check if the list is empty
bool isEmptyItemList(ItemNodePtr *listptr);

// Insert data at the front of the list
void insertAtFrontItemList(ItemNodePtr *listptr, uint8_t item, int x, int y);
	
// insert data at the back of the linked list
void insertAtBackItemList(ItemNodePtr *listptr, uint8_t item, int x, int y);
	
// returns the data at first node 
ItemNodePtr topFrontItem(ItemNodePtr *listptr);

// returns the data at last node 
ItemNodePtr topBackItem(ItemNodePtr *listptr);

// removes the item at front of the linked list and return 
ItemNodePtr popFrontItem(ItemNodePtr *listptr);

// remove the item at the back of the linked list and return 
ItemNodePtr popBackItem(ItemNodePtr *listptr);

// print the linked list 
void printItemList(ItemNodePtr *listptr);

// check if a full key is in the list
bool isItemInList(ItemNodePtr *listptr, uint8_t key, int keyx, int keyy);

// check if a anything is at X,Y in the list
bool isAnythingAtXY(ItemNodePtr *listptr, int keyx, int keyy);

// return list of items at a tile and remove them from the itemlist
ItemNodePtr popItemsAtTile(ItemNodePtr *listptr,  int tx, int ty );

void deleteItem(ItemNodePtr *listptr, ItemNodePtr ptr);
ItemNodePtr popItem(ItemNodePtr *listptr, ItemNodePtr ptr);
int getItemCount(ItemNodePtr *listptr);

void clearItemList(ItemNodePtr *listptr);

#define BM_SIZE16 0
#define BM_SIZE8 1

typedef struct {
	uint8_t item;
	char desc[40];
	bool isMachine;		// can convert resources
	bool isResource;	// general resource type
	bool isOverlay;		// background feature - can't place these
	bool isProduct;
	int bmID;
	int size; 			// 16x16=0, 8x8=1
	int fuel_value;		
} ItemType; 

enum ItemTypesEnum {
	IT_BELT,

	IT_TYPES_RAW,
	IT_STONE = IT_TYPES_RAW,
	IT_IRON_ORE,
	IT_COPPER_ORE,
	IT_COAL,
	IT_WOOD,

	IT_TYPES_PROCESSED,
	IT_IRON_PLATE = IT_TYPES_PROCESSED,
	IT_COPPER_PLATE,
	IT_STONE_BRICK,

	IT_TYPES_MACHINE,
	IT_FURNACE = IT_TYPES_MACHINE,
	IT_MINER,
	IT_ASSEMBLER,
	IT_INSERTER,
	IT_BOX,
	IT_GENERATOR,
	IT_TSPLITTER,
	IT_ESCPOD,

	IT_TYPES_FEATURES,
	IT_FEAT_STONE = IT_TYPES_FEATURES,
	IT_FEAT_IRON,
	IT_FEAT_COPPER,
	IT_FEAT_COAL,
	IT_FEAT_WOOD,

	IT_TYPES_PRODUCT,
	IT_GEARWHEEL = IT_TYPES_PRODUCT,
	IT_WIRE,
	IT_CIRCUIT,
	IT_PAVING,
	IT_COMPUTER,
	IT_MINI_BELT,
	IT_TYPES_MINI_MACHINES,
	IT_PROD_FURNACE = IT_TYPES_MINI_MACHINES,
	IT_PROD_MINER,
	IT_PROD_ASSEMBLER,
	IT_PROD_INSERTER,
	IT_PROD_BOX,
	IT_PROD_GENERATOR,
	IT_PROD_TSPLITTER,
	IT_PROD_ESCPOD,

	NUM_ITEMTYPES
};

static ItemType itemtypes[] = {
//                                     Machine Resouce Overlay Product
// belts are special
	{IT_BELT, 			"Belts", 		false, false, false, false, BMOFF_BELT16, BM_SIZE16, 0 },
// raw items
	{IT_STONE,			"Stone",		false, true, false, false, BMOFF_ITEM8+4, BM_SIZE8, 0 },
	{IT_IRON_ORE,		"Iron Ore", 	false, true, false, false, BMOFF_ITEM8+5, BM_SIZE8, 0 },
	{IT_COPPER_ORE,		"Copper Ore",	false, true, false, false, BMOFF_ITEM8+6, BM_SIZE8, 0 },
	{IT_COAL,			"Coal",			false, true, false, false, BMOFF_ITEM8+7, BM_SIZE8, 10 },
	{IT_WOOD,			"Wood",			false, true, false,  false,BMOFF_ITEM8+0, BM_SIZE8, 5 },
// processed items
	{IT_IRON_PLATE,		"Iron Plate",	false, true, false, false, BMOFF_ITEM8+1, BM_SIZE8, 0 },
	{IT_COPPER_PLATE,	"Copper Plate", false, true, false, false, BMOFF_ITEM8+2, BM_SIZE8, 0 },
	{IT_STONE_BRICK,	"Stone brick",	false, true, false, false, BMOFF_ITEM8+3, BM_SIZE8, 0 },
// Machines
    {IT_FURNACE,		"Furnace",		true, false, false, false, BMOFF_MACH16+0, BM_SIZE16, 0 },
    {IT_MINER,			"Miner",		true, false, false, false, BMOFF_MACH16+1, BM_SIZE16, 0 },
    {IT_ASSEMBLER,		"Assembler",	true, false, false, false, BMOFF_MACH16+2, BM_SIZE16, 0 },
    {IT_INSERTER,		"Inserter", 	true, false, false, false, BMOFF_MACH16+3, BM_SIZE16, 0 },
    {IT_BOX,			"Box",		 	true, false, false, false, BMOFF_MACH16+4, BM_SIZE16, 0 },
    {IT_GENERATOR,		"Generator", 	true, false, false, false, BMOFF_MACH16+5, BM_SIZE16, 0 },
    {IT_TSPLITTER,		"T-Splitter", 	true, false, false, false, BMOFF_MACH16+6, BM_SIZE16, 0 },
    {IT_ESCPOD,			"Escape Pod", 	true, false, false, false, BMOFF_MACH16+7, BM_SIZE16, 0 },
// Overlays
	{IT_FEAT_STONE,		"Stone",		false, false, true, false, BMOFF_FEAT16+0, BM_SIZE16, 0 },
	{IT_FEAT_IRON,		"Iron Ore", 	false, false, true, false, BMOFF_FEAT16+1, BM_SIZE16, 0 },
	{IT_FEAT_COPPER,	"Copper Ore",	false, false, true, false, BMOFF_FEAT16+2, BM_SIZE16, 0 },
	{IT_FEAT_COAL,		"Coal",			false, false, true, false, BMOFF_FEAT16+3, BM_SIZE16, 0 },
	{IT_FEAT_WOOD,		"Tree",			false, false, true, false, BMOFF_FEAT16+4, BM_SIZE16, 0 },
// Products
    {IT_GEARWHEEL,		"Gear Wheel",	false, false, false, true, BMOFF_PROD8+0, BM_SIZE8, 0 },
    {IT_WIRE,			"Copper Wire",	false, false, false, true, BMOFF_PROD8+1, BM_SIZE8, 0 },
    {IT_CIRCUIT,		"Circuit Board",false, false, false, true, BMOFF_PROD8+2, BM_SIZE8, 0 },
    {IT_PAVING,			"Paving",		false, false, false, true, BMOFF_PROD8+3, BM_SIZE8, 0 },
    {IT_COMPUTER,		"Computer",		false, false, false, true, BMOFF_PROD8+4, BM_SIZE8, 0 },
    {IT_MINI_BELT,		"Belt",			false, false, false, true, BMOFF_BELT_MINI, BM_SIZE8, 0 },
    {IT_PROD_FURNACE,	"Furnace",		false, false, false, true, BMOFF_MACH_MINI+0, BM_SIZE8, 0 },
    {IT_PROD_MINER,		"Miner",		false, false, false, true, BMOFF_MACH_MINI+1, BM_SIZE8, 0 },
    {IT_PROD_ASSEMBLER,	"Assembler",	false, false, false, true, BMOFF_MACH_MINI+2, BM_SIZE8, 0 },
    {IT_PROD_INSERTER,	"Inserter",		false, false, false, true, BMOFF_MACH_MINI+3, BM_SIZE8, 0 },
    {IT_PROD_BOX,		"Box",			false, false, false, true, BMOFF_MACH_MINI+4, BM_SIZE8, 0 },
    {IT_PROD_GENERATOR,	"Generator",	false, false, false, true, BMOFF_MACH_MINI+5, BM_SIZE8, 0 },
    {IT_PROD_TSPLITTER,	"T-Splitter",	false, false, false, true, BMOFF_MACH_MINI+6, BM_SIZE8, 0 },
    {IT_PROD_ESCPOD,	"Escape Pod",	false, false, false, true, BMOFF_MACH_MINI+7, BM_SIZE8, 0 },
	
};

typedef struct {
	int bmID;
	uint8_t feature_type;
} ItemBMFeatureMap;

	//			stone    0:5:10 
	//			iron ore 1:6:11
	//			copp ore 2:7:12
	//			coal     3:8:13
static ItemBMFeatureMap item_feature_map[] = {
	{ BMOFF_FEAT16 +  0, IT_FEAT_STONE },
	{ BMOFF_FEAT16 +  1, IT_FEAT_IRON },
	{ BMOFF_FEAT16 +  2, IT_FEAT_COPPER },
	{ BMOFF_FEAT16 +  3, IT_FEAT_COAL },
	{ BMOFF_FEAT16 +  4, IT_FEAT_WOOD },
	{ BMOFF_FEAT16 +  5, IT_FEAT_STONE },
	{ BMOFF_FEAT16 +  6, IT_FEAT_IRON },
	{ BMOFF_FEAT16 +  7, IT_FEAT_COPPER },
	{ BMOFF_FEAT16 +  8, IT_FEAT_COAL },
	{ BMOFF_FEAT16 +  9, IT_FEAT_WOOD },
	{ BMOFF_FEAT16 + 10, IT_FEAT_STONE },
	{ BMOFF_FEAT16 + 11, IT_FEAT_IRON },
	{ BMOFF_FEAT16 + 12, IT_FEAT_COPPER },
	{ BMOFF_FEAT16 + 13, IT_FEAT_COAL },
	{ BMOFF_FEAT16 + 14, IT_FEAT_WOOD },
};

typedef struct {
	uint8_t feature_type;
	uint8_t raw_type;
	uint8_t processed_type;
} ProcessMap;

/* Process item map: 
 * From feature, to raw, then to processed */
static ProcessMap process_map[] = {
	{ IT_FEAT_STONE, IT_STONE, IT_STONE_BRICK },
	{ IT_FEAT_IRON, IT_IRON_ORE, IT_IRON_PLATE },
	{ IT_FEAT_COPPER, IT_COPPER_ORE, IT_COPPER_PLATE },
	{ IT_FEAT_COAL, IT_COAL, IT_COAL },
	{ IT_FEAT_WOOD, IT_WOOD, IT_WOOD },
};

bool isBelt(int item);
bool isMachine(int item);
bool isResource(int item);
bool isOverlay(int item);
bool isFuel(int item);
bool isProduct(int item);

#endif

