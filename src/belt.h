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
	uint8_t beltID;
	int locX;
	int locY;
	int contents;
	struct BELT_LINK *nextLink;
	struct BELT_LINK *prevLink;
} BELT_LINK;

#endif


