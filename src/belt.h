#ifndef _BELT_H
#define _BELT_H

#include <stdint.h>

// Directions
//    0
// 3     1
//    2
typedef struct {
	uint8_t beltID;// index
	uint8_t bmID;  // bitmap number
	uint8_t from;  // from direction
	uint8_t to;    // to direction
} BELT_CONFIG;

typedef struct {
	int8_t beltID;
	int locX;
	int locY;
	int contents;
	struct BELT_LINK *nextLink;
	struct BELT_LINK *prevLink;
} BELT_LINK;

typedef struct {
	uint8_t beltID;
	int locX;
	int locY;
} BELT_PLACE;


#define NUM_BELT_TYPES 12
//    0
// 3     1
//    2
static BELT_CONFIG belts[NUM_BELT_TYPES] = {
	{ 0,  0, 1, 3}, // straight belts
	{ 1,  4, 2, 0},
	{ 2,  8, 3, 1},
	{ 3, 12, 0, 2},
	{ 4, 16, 1, 0}, // bent belts
	{ 5, 20, 3, 0},
	{ 6, 24, 0, 1},
	{ 7, 28, 0, 3},
	{ 8, 32, 1, 2},
	{ 9, 36, 2, 1},
	{10, 40, 2, 3},
	{11, 48, 3, 2},
};
typedef struct {
	uint8_t beltID;
	uint8_t connIn[4];
	uint8_t connOut[4];
} BELT_CONNECT;

static BELT_CONNECT bconn[NUM_BELT_TYPES];

#endif

