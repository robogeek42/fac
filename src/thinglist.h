/*
  vim:ts=4
  vim:sw=4
*/
#ifndef _THINGLIST_H
#define _THINGLIST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// data structure for Node
// data -> the actual value
// next -> address of the next node

// (x, y) is absolute pixel position in map
// (x - fax.xpos, y - fac.ypos) gives sceen pixel position
// 
struct ThingNode {
	void* thing;
	struct ThingNode *next;
};
typedef struct ThingNode *ThingNodePtr;

typedef struct {
	void* thing;
} ThingNodeSave;

// check if the list is empty
bool isEmptyThingList(ThingNodePtr *listptr);

// Insert data at the front of the list
void insertAtFrontThingList(ThingNodePtr *listptr, void* thing);
	
// insert data at the back of the linked list
void insertAtBackThingList(ThingNodePtr *listptr, void* thing);
	
// returns the data at first node 
ThingNodePtr topFrontThing(ThingNodePtr *listptr);

// returns the data at last node 
ThingNodePtr topBackThing(ThingNodePtr *listptr);

// removes the thing at front of the linked list and return 
ThingNodePtr popFrontThing(ThingNodePtr *listptr);

// remove the thing at the back of the linked list and return 
ThingNodePtr popBackThing(ThingNodePtr *listptr);

void deleteThing(ThingNodePtr *listptr, ThingNodePtr ptr);
ThingNodePtr popThing(ThingNodePtr *listptr, ThingNodePtr ptr);

#if 0
// check if a full key is in the list
bool isThingInList(ThingNodePtr *listptr, uint8_t key, int keyx, int keyy);

// check if a anything is at X,Y in the list
bool isAnythingAtXY(ThingNodePtr *listptr, int keyx, int keyy);

// return list of things at a tile and remove them from the thinglist
ThingNodePtr popThingsAtTile(ThingNodePtr *listptr,  int tx, int ty );
#endif

#endif
