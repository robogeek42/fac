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
	uint8_t dir;
	uint16_t tx;
	uint16_t ty;
	uint16_t start_tx;
	uint16_t start_ty;
	uint16_t end_tx;
	uint16_t end_ty;
} Inserter;

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

#endif
