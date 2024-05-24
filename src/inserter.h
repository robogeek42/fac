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
	uint16_t tx;
	uint16_t ty;
	uint16_t start_tx;
	uint16_t start_ty;
	uint16_t end_tx;
	uint16_t end_ty;
	uint16_t bmid; // start, middle+1, end+2
} Inserter;

#endif

#ifdef _INSERTER_IMPLEMENTATION

ThingNodePtr findInserter(ThingNodePtr *inserterlist,  int tx, int ty)
{
	if (isEmptyThingList(inserterlist)) {
		return false;
	}

	ThingNodePtr currPtr = (*inserterlist);
	Inserter *ins = (Inserter*) currPtr->thing;
	while (currPtr != NULL ) {
		if ( ins->tx == tx && 
			 ins->ty == ty)
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
