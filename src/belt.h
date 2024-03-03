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
	uint8_t from;  // from direction
	uint8_t to;    // to direction
	uint8_t pos[5]; // content at position. (from)0->1->2->3->4->5(to)
} BELT_CONFIG;

#define NUM_BELT_TYPES 12

// Directions
//    0
// 3     1
//    2

// content position 
//     0   
//     1
// 5 6 2 8 9 
//     3
//     4

static BELT_CONFIG belts[NUM_BELT_TYPES] = {
	{ 0,  0, 1, 3,    {9,8,2,6,5}}, // straight belts
	{ 1,  4, 2, 0,    {4,3,2,1,0}},
	{ 2,  8, 3, 1,    {5,6,2,8,9}},
	{ 3, 12, 0, 2,    {0,1,2,3,4}},
	{ 4, 16, 1, 0,    {9,8,2,1,0}}, // bent belts
	{ 5, 20, 3, 0,    {5,6,2,1,0}},
	{ 6, 24, 0, 1,    {0,1,2,8,9}},
	{ 7, 28, 0, 3,    {0,1,2,6,5}},
	{ 8, 32, 1, 2,    {8,7,2,3,4}},
	{ 9, 36, 2, 1,    {4,3,2,8,9}},
	{10, 40, 2, 3,    {4,3,2,6,5}},
	{11, 48, 3, 2,    {5,6,2,3,4}},
};
// contents contain item-type at that pos
typedef struct {
	int8_t beltID;
	int locX;
	int locY;
	uint8_t contents[10]; 
	struct BELT_LINK *nextLink;
	struct BELT_LINK *prevLink;
} BELT_LINK;

typedef struct {
	uint8_t beltID;
	int locX;
	int locY;
} BELT_PLACE;


typedef struct {
	uint8_t beltID;
	uint8_t connIn[4];
	uint8_t connOut[4];
} BELT_CONNECT;

static BELT_CONNECT bconn[NUM_BELT_TYPES];

#endif

