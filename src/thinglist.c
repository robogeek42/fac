/*
  vim:ts=4
  vim:sw=4
*/
#include "thinglist.h"

extern int gTileSize;

// check if the list is empty
bool isEmptyThingList(ThingNodePtr* listptr) {
	return (*listptr) == NULL;
}

// Insert data at the front of the list
void insertAtFrontThingList(ThingNodePtr *listptr, void* thing) 
{
	ThingNodePtr node = (ThingNodePtr) malloc(sizeof(struct ThingNode));
	if (node==NULL) {
		printf("Out of memory\n");
		return;
	}
	node->thing = thing;
	node->next = NULL;
	//printf("Insert %p: %u %d,%d\n", node, node->thing, node->x, node->y);
	if (isEmptyThingList(listptr)) {
		(*listptr) = node;
		//printf("empty\n");
	} else {
		node->next = (*listptr);
		(*listptr) = node;
		//printf("next %p\n",node->next);
	}
}
	
// insert data at the back of the linked list
void insertAtBackThingList(ThingNodePtr *listptr, void* thing) 
{
	ThingNodePtr node = (ThingNodePtr) malloc(sizeof(struct ThingNode));
	if (node==NULL) {
		printf("Out of memory\n");
		return;
	}
	node->thing = thing;
	node->next = NULL;
	
	if (isEmptyThingList(listptr)) {
		(*listptr) = node;
	} else {
		// find the last node 
		ThingNodePtr currPtr = (*listptr);
		while (currPtr->next != NULL) {
			currPtr = currPtr->next;
		}
		
		// insert it 
		currPtr->next = node;
	}
}
	
// returns the data at first node 
ThingNodePtr topFrontThing(ThingNodePtr *listptr) 
{
	if (isEmptyThingList(listptr)) {
		//printf("%s", "List is empty");
		return NULL;
	} else {
		return (*listptr);
	}
}

// returns the data at last node 
ThingNodePtr topBackThing(ThingNodePtr *listptr) 
{
	if (isEmptyThingList(listptr)) {
		//printf("%s", "List is empty");
		return NULL;
	} else if ((*listptr)->next == NULL) {
		return (*listptr);
	} else {
		ThingNodePtr currPtr = (*listptr);
		while (currPtr->next != NULL) {
			currPtr = currPtr->next;
		}
		return currPtr;
	}
}

// removes the thing at front of the linked list and return 
ThingNodePtr popFrontThing(ThingNodePtr *listptr) 
{
	ThingNodePtr thingPtr = NULL;
	if (isEmptyThingList(listptr)) {
		//printf("%s", "List is empty");
	} else {
		ThingNodePtr nextPtr = (*listptr)->next;
		thingPtr = (*listptr);
		thingPtr->next = NULL;
		
		// make nextptr head 
		(*listptr) = nextPtr;
	}
	
	return thingPtr;
}

// remove the thing at the back of the linked list and return 
ThingNodePtr popBackThing(ThingNodePtr *listptr) 
{
	ThingNodePtr thingPtr = NULL;
	if (isEmptyThingList(listptr)) {
		//printf("%s", "List is empty");
		return NULL;
	} else if ((*listptr)->next == NULL) {
	   thingPtr = (*listptr);
	} else {
		ThingNodePtr currPtr = (*listptr);
		ThingNodePtr prevPtr = NULL;
		while (currPtr->next != NULL) {
			prevPtr = currPtr;
			currPtr = currPtr->next;
		}
		thingPtr = currPtr;
		prevPtr->next = NULL;
	} 
	
	return thingPtr;
}

void deleteThing(ThingNodePtr *listptr, ThingNodePtr ptr)
{
	if (isEmptyThingList(listptr)) {
		return;
	}
	
	ThingNodePtr prevPtr = NULL;
	ThingNodePtr currPtr = (*listptr);
	ThingNodePtr currNext = NULL;
	while (currPtr != NULL ) {
		currNext = currPtr->next;

		if ( currPtr == ptr )
		{
			// cut the current thing from the main list
			if (prevPtr)
			{
				// case where we are at middle or end (currNext==NUL)
				prevPtr->next = currNext;
			} else {
				// case where we were at begining or only thing 
				(*listptr) = currNext;
				prevPtr = NULL;
			}
			break;
		} else {
			prevPtr = currPtr;
		}
		currPtr = currNext;
	}
	free(ptr);
}

ThingNodePtr popThing(ThingNodePtr *listptr, ThingNodePtr ptr)
{
	if (isEmptyThingList(listptr)) {
		return NULL;
	}
	
	ThingNodePtr prevPtr = NULL;
	ThingNodePtr currPtr = (*listptr);
	ThingNodePtr currNext = NULL;
	while (currPtr != NULL ) {
		currNext = currPtr->next;

		if ( currPtr == ptr )
		{
			// cut the current thing from the main list
			if (prevPtr)
			{
				// case where we are at middle or end (currNext==NUL)
				prevPtr->next = currNext;
			} else {
				// case where we were at begining or only thing 
				(*listptr) = currNext;
				prevPtr = NULL;
			}
			break;
		} else {
			prevPtr = currPtr;
		}
		currPtr = currNext;
	}
	if ( currPtr != NULL)
	{
		return currPtr;
	}
	return NULL;
}

#if 0
// check if a key is in the list
bool isThingInList(ThingNodePtr *listptr, uint8_t key, int keyx, int keyy) 
{
	if (isEmptyThingList(listptr)) {
		return false;
	}

	ThingNodePtr currPtr = (*listptr);
	while (currPtr != NULL ) {
		if (currPtr->thing == key && currPtr->x == keyx && currPtr->y == keyy)
		{
			break;
		}
		currPtr = currPtr->next;
	}

	if (currPtr == NULL) {
		return false;
	}

	return true;
}
bool isAnythingAtXY(ThingNodePtr *listptr, int keyx, int keyy) 
{
	if (isEmptyThingList(listptr)) {
		return false;
	}

	ThingNodePtr currPtr = (*listptr);
	while (currPtr != NULL ) {
		if ( currPtr->x == keyx && currPtr->y == keyy )
		{
			break;
		}
		currPtr = currPtr->next;
	}

	if (currPtr == NULL) {
		return false;
	}

	return true;
}

ThingNodePtr popThingsAtTile(ThingNodePtr *listptr, int tx, int ty )
{
	if (isEmptyThingList(listptr)) {
		return NULL;
	}
	ThingNodePtr tilelist = NULL; // head of list of things on this tile

	ThingNodePtr prevPtr = NULL;
	ThingNodePtr currPtr = (*listptr);
	ThingNodePtr currNext = NULL;
	while (currPtr != NULL ) {
		currNext = currPtr->next;

		if ( currPtr->x >= tx*gTileSize &&
			 currPtr->x < (tx+1)*gTileSize &&
		  	 currPtr->y >= ty*gTileSize &&
		  	 currPtr->y < (ty+1)*gTileSize )
		{
			// cut the current thing from the main list
			if (prevPtr)
			{
				// case where we are at middle or end (currNext==NUL)
				prevPtr->next = currNext;
			} else {
				// case where we were at begining or only thing 
				(*listptr) = currNext;
				prevPtr = NULL;
			}

			// insert at front of the new list
			currPtr->next = tilelist;
			tilelist = currPtr;

		} else {
			prevPtr = currPtr;
		}
		currPtr = currNext;
	}
	return tilelist;
}
#endif

