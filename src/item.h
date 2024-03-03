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


#endif

