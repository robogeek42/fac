/*
  vim:ts=4
  vim:sw=4
*/
#include "item.h"

extern int gTileSize;

// check if the list is empty
bool isEmptyItemList(ItemNodePtr* listptr) {
	return (*listptr) == NULL;
}

// Insert data at the front of the list
void insertAtFrontItemList(ItemNodePtr *listptr, uint8_t item, int x, int y) 
{
	ItemNodePtr node = (ItemNodePtr) malloc(sizeof(struct ItemNode));
	if (node==NULL) {
		printf("Out of memory\n");
		return;
	}
	node->item = item;
	node->x = x;
	node->y = y;
	node->next = NULL;
	//printf("Insert %p: %u %d,%d\n", node, node->item, node->x, node->y);
	if (isEmptyItemList(listptr)) {
		(*listptr) = node;
		//printf("empty\n");
	} else {
		node->next = (*listptr);
		(*listptr) = node;
		//printf("next %p\n",node->next);
	}

}
	
// insert data at the back of the linked list
void insertAtBackItemList(ItemNodePtr *listptr, uint8_t item, int x, int y) 
{
	ItemNodePtr node = (ItemNodePtr) malloc(sizeof(struct ItemNode));
	if (node==NULL) {
		printf("Out of memory\n");
		return;
	}
	node->item = item;
	node->x = x;
	node->y = y;
	node->next = NULL;
	
	if (isEmptyItemList(listptr)) {
		(*listptr) = node;
	} else {
		// find the last node 
		ItemNodePtr currPtr = (*listptr);
		while (currPtr->next != NULL) {
			currPtr = currPtr->next;
		}
		
		// insert it 
		currPtr->next = node;
	}
}
	
// returns the data at first node 
ItemNodePtr topFrontItem(ItemNodePtr *listptr) 
{
	if (isEmptyItemList(listptr)) {
		//printf("%s", "List is empty");
		return NULL;
	} else {
		return (*listptr);
	}
}

// returns the data at last node 
ItemNodePtr topBackItem(ItemNodePtr *listptr) 
{
	if (isEmptyItemList(listptr)) {
		//printf("%s", "List is empty");
		return NULL;
	} else if ((*listptr)->next == NULL) {
		return (*listptr);
	} else {
		ItemNodePtr currPtr = (*listptr);
		while (currPtr->next != NULL) {
			currPtr = currPtr->next;
		}
		return currPtr;
	}
}

// removes the item at front of the linked list and return 
ItemNodePtr popFrontItem(ItemNodePtr *listptr) 
{
	ItemNodePtr itemPtr = NULL;
	if (isEmptyItemList(listptr)) {
		//printf("%s", "List is empty");
	} else {
		ItemNodePtr nextPtr = (*listptr)->next;
		itemPtr = (*listptr);
		itemPtr->next = NULL;
		
		// make nextptr head 
		(*listptr) = nextPtr;
	}
	
	return itemPtr;
}

// remove the item at the back of the linked list and return 
ItemNodePtr popBackItem(ItemNodePtr *listptr) 
{
	ItemNodePtr itemPtr = NULL;
	if (isEmptyItemList(listptr)) {
		//printf("%s", "List is empty");
		return NULL;
	} else if ((*listptr)->next == NULL) {
	   itemPtr = (*listptr);
	} else {
		ItemNodePtr currPtr = (*listptr);
		ItemNodePtr prevPtr = NULL;
		while (currPtr->next != NULL) {
			prevPtr = currPtr;
			currPtr = currPtr->next;
		}
		itemPtr = currPtr;
		prevPtr->next = NULL;
	} 
	
	return itemPtr;
}

// print the linked list 
void printItemList(ItemNodePtr *listptr) 
{
	ItemNodePtr currPtr = (*listptr);
	while (currPtr != NULL) {
		printf("%p: %u %d,%d\n", currPtr, currPtr->item, currPtr->x, currPtr->y);
		currPtr = currPtr->next;
	}
}

// check if a key is in the list
bool isItemInList(ItemNodePtr *listptr, uint8_t key, int keyx, int keyy) 
{
	if (isEmptyItemList(listptr)) {
		return false;
	}

	ItemNodePtr currPtr = (*listptr);
	while (currPtr != NULL ) {
		if (currPtr->item == key && currPtr->x == keyx && currPtr->y == keyy)
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
bool isAnythingAtXY(ItemNodePtr *listptr, int keyx, int keyy) 
{
	if (isEmptyItemList(listptr)) {
		return false;
	}

	ItemNodePtr currPtr = (*listptr);
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

ItemNodePtr popItemsAtTile(ItemNodePtr *listptr, int tx, int ty )
{
	if (isEmptyItemList(listptr)) {
		return NULL;
	}
	ItemNodePtr tilelist = NULL; // head of list of items on this tile

	ItemNodePtr prevPtr = NULL;
	ItemNodePtr currPtr = (*listptr);
	ItemNodePtr currNext = NULL;
	while (currPtr != NULL ) {
		currNext = currPtr->next;

		if ( currPtr->x >= tx*gTileSize &&
			 currPtr->x < (tx+1)*gTileSize &&
		  	 currPtr->y >= ty*gTileSize &&
		  	 currPtr->y < (ty+1)*gTileSize )
		{
			// cut the current item from the main list
			if (prevPtr)
			{
				// case where we are at middle or end (currNext==NUL)
				prevPtr->next = currNext;
			} else {
				// case where we were at begining or only item 
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

bool isBelt(int item) {
	return itemtypes[item].isBelt;
}
bool isMachine(int item) {
	return itemtypes[item].isMachine;
}
bool isResource(int item) {
	return itemtypes[item].isResource;
}
bool isOverlay(int item) {
	return itemtypes[item].isOverlay;
}




