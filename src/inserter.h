/*
  vim:ts=4
  vim:sw=4
*/
#ifndef _INSERTER_H
#define _INSERTER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "thinglist.h"

typedef struct {
	uint8_t type;
	uint8_t dir;
	uint16_t tx;
	uint16_t ty;
	uint16_t end_tx;
	uint16_t end_ty;
	uint16_t start_tx;
	uint16_t start_ty;
	int itemcnt;
	int maxitemcnt;
	ItemNodePtr itemlist;
} Inserter;

typedef struct {
	uint8_t dir;
	uint16_t tx;
	uint16_t ty;
	uint16_t start_tx;
	uint16_t start_ty;
	uint16_t end_tx;
	uint16_t end_ty;
	int itemcnt;
	int maxitemcnt;
} InserterSave;

#endif

#ifdef _INSERTER_IMPLEMENTATION

Inserter* findInserter(ThingNodePtr *inserterlist,  int tx, int ty)
{
	if (isEmptyThingList(inserterlist)) {
		return false;
	}

	ThingNodePtr currPtr = (*inserterlist);
	Inserter *insp = NULL;
	while (currPtr != NULL ) {
		insp = (Inserter*) currPtr->thing;
		if ( insp != NULL &&
			 ( (insp->tx == tx && insp->ty == ty) ||
			   (insp->start_tx == tx && insp->start_ty == ty) ||
			   (insp->end_tx == tx && insp->end_ty == ty)
			 ) )
		{
			break;
		}
		currPtr = currPtr->next;
	}

	if (currPtr == NULL) {
		return NULL;
	}

	return insp;

}
ThingNodePtr findInserterNode(ThingNodePtr *inserterlist,  int tx, int ty)
{
	if (isEmptyThingList(inserterlist)) {
		return false;
	}

	ThingNodePtr currPtr = (*inserterlist);
	Inserter *insp = NULL;
	while (currPtr != NULL ) {
		insp = (Inserter*) currPtr->thing;
		if ( insp != NULL &&
			 ( (insp->tx == tx && insp->ty == ty) ||
			   (insp->start_tx == tx && insp->start_ty == ty) ||
			   (insp->end_tx == tx && insp->end_ty == ty)
			 ) )
		{
			break;
		}
		currPtr = currPtr->next;
	}

	if (currPtr == NULL) {
		return NULL;
	}

	return currPtr;

}

int countInserters(ThingNodePtr *inserterlist)
{
	int cnt = 0;
	if (isEmptyThingList(inserterlist)) {
		return 0;
	}

	ThingNodePtr currPtr = (*inserterlist);
	while (currPtr != NULL ) {
		cnt++;
		currPtr = currPtr->next;
	}

	return cnt;
}

void clearInserters(ThingNodePtr *inserterlist)
{
	ThingNodePtr currPtr = (*inserterlist);
	ThingNodePtr nextPtr = NULL;
	Inserter *insp = NULL;
	while (currPtr != NULL ) {
		nextPtr = currPtr->next;
		insp = (Inserter*) currPtr->thing;
		
		clearItemList(&insp->itemlist);
		free(insp);
		free(currPtr);
		currPtr = nextPtr;
	}
	
}

#endif
