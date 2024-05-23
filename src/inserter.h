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
