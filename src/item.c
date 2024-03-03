/*
  vim:ts=4
  vim:sw=4
*/
#include "item.h"

extern ItemNodePtr itemlist;

// check if the list is empty
bool isEmptyItemList() {
	return itemlist == NULL;
}

// Insert data at the front of the list
void insertAtFrontItemList(uint8_t item, int x, int y) {
	ItemNodePtr node = (ItemNodePtr) malloc(sizeof(ItemNodePtr));
	node->item = item;
	node->x = x;
	node->y = y;
	node->next = NULL;
	printf("Insert %p ",node);	
	if (isEmptyItemList()) {
		itemlist = node;
		printf("empty\n");
	} else {
		node->next = itemlist;
		itemlist = node;
		printf("next %p\n",node->next);
	}

}
	
// insert data at the back of the linked list
void insertAtBackItemList(uint8_t item, int x, int y) {
	ItemNodePtr node = (ItemNodePtr) malloc(sizeof(ItemNodePtr));
	node->item = item;
	node->x = x;
	node->y = y;
	node->next = NULL;
	
	if (isEmptyItemList()) {
		itemlist = node;
	} else {
		// find the last node 
		ItemNodePtr currPtr = itemlist;
		while (currPtr->next != NULL) {
			currPtr = currPtr->next;
		}
		
		// insert it 
		currPtr->next = node;
	}
}
	
// returns the data at first node 
ItemNodePtr topFrontItem() {
	if (isEmptyItemList()) {
		printf("%s", "List is empty");
		return NULL;
	} else {
		return itemlist;
	}
}

// returns the data at last node 
ItemNodePtr topBackItem() {
	if (isEmptyItemList()) {
		printf("%s", "List is empty");
		return NULL;
	} else if (itemlist->next == NULL) {
		return itemlist;
	} else {
		ItemNodePtr currPtr = itemlist;
		while (currPtr->next != NULL) {
			currPtr = currPtr->next;
		}
		return currPtr;
	}
}

// removes the item at front of the linked list and return 
ItemNodePtr popFrontItem() {
	ItemNodePtr itemPtr = NULL;
	if (isEmptyItemList()) {
		printf("%s", "List is empty");
	} else {
		ItemNodePtr nextPtr = itemlist->next;
		itemPtr = itemlist;
		// remove head
		//free(itemlist);
		
		// make nextptr head 
		itemlist = nextPtr;
	}
	
	return itemPtr;
}

// remove the item at the back of the linked list and return 
ItemNodePtr popBackItem() {
	ItemNodePtr itemPtr = NULL;
	if (isEmptyItemList()) {
		printf("%s", "List is empty");
		return NULL;
	} else if (itemlist->next == NULL) {
	   itemPtr = itemlist;
	   //free(itemlist);
	   //itemlist = NULL;
	} else {
		ItemNodePtr currPtr = itemlist;
		ItemNodePtr prevPtr = NULL;
		while (currPtr->next != NULL) {
			prevPtr = currPtr;
			currPtr = currPtr->next;
		}
		itemPtr = currPtr;
		//free(currPtr);
		//currPtr = NULL;
		prevPtr->next = NULL;
	} 
	
	return itemPtr;
}

// print the linked list 
void printItemList() {
	ItemNodePtr currPtr = itemlist;
	while (currPtr != NULL) {
		printf("%p: %d %d,%d\n", currPtr, currPtr->item, currPtr->x, currPtr->y);
		currPtr = currPtr->next;
	}
}

// check if a key is in the list
bool findItem(uint8_t key, int keyx, int keyy) {
	if (isEmptyItemList()) {
		return false;
	}

	ItemNodePtr currPtr = itemlist;
	while (currPtr != NULL 
			&& currPtr->item != key 
			&& currPtr->x != keyx
			&& currPtr->y != keyy
			) {
		currPtr = currPtr->next;
	}

	if (currPtr == NULL) {
		return false;
	}

	return true;
}

