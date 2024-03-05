/*
  vim:ts=4
  vim:sw=4
*/
#ifndef _BELT_H
#define _BELT_H

#include <stdint.h>

typedef struct {
	uint8_t beltID;// index
	uint8_t bmID;  // bitmap number
	uint8_t in;  // from direction
	uint8_t out;    // to direction
} BELT_CONFIG;

#define NUM_BELT_TYPES 12

// Directions
//    0
// 3     1
//    2

static BELT_CONFIG belts[NUM_BELT_TYPES] = {
// straight belts
	{ 0,  0, 1, 3 }, // left
	{ 1,  4, 2, 0 }, // up
	{ 2,  8, 3, 1 }, // right
	{ 3, 12, 0, 2 }, // down
// bent belts
	{ 4, 16, 1, 0 }, 
	{ 5, 20, 3, 0 },
	{ 6, 24, 0, 1 },
	{ 7, 28, 0, 3 },
	{ 8, 32, 1, 2 },
	{ 9, 36, 2, 1 },
	{10, 40, 2, 3 },
	{11, 48, 3, 2 },
};

typedef struct {
	int nb; // which neighbour (0-3 u,r,d,l) connects their out to our in
	uint8_t rb[4]; // belt rotate selected (0-3 = u,r,d,l) mapping to beltID 
} BELT_DIR_RULES;

#define NUM_RULES 5
static BELT_DIR_RULES belt_rules_in[NUM_RULES] = {
	{ -1, { 1, 2, 3, 0} },
	{  0, { 1, 9, 3,10} },
	{  1, { 5, 2,11, 0} },
	{  2, { 1, 6, 3, 7} },
	{  3, { 4, 2, 8, 0} },
};
static BELT_DIR_RULES belt_rules_out[NUM_RULES] = {
	{ -1, { 1, 2, 3, 0} },
	{  0, { 1, 5, 3, 4} },
	{  1, { 9, 2, 6, 0} },
	{  2, { 1,11, 3, 8} },
	{  3, {10, 2, 7, 0} },
};

typedef struct {
	uint8_t beltID;
	int locX;
	int locY;
} BELT_PLACE;

#endif

