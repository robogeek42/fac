/*
 vim:ts=4
 vim:sw=4
 */
#ifndef _MINER_H
#define _MINER_H

#include <stdlib.h>
#include <stdio.h>

typedef struct {
	bool valid;
	int x;
	int y;
	int tx;
	int ty;
	int speed;
	int type;
	int direction;
	clock_t ticksTillProduce;
} MINER;

int minersNum = 0;
int minersAllocated = 0;
int minersBlockSize = 10;

bool allocMiners(MINER **miners);
int addMiner( MINER **miners, int tx, int ty, int type, int direction );
void deleteMiner( MINER *miners, int mnum );
void minerProduce( MINER *miners, int m, ItemNodePtr *itemlist );
int getMinerAtTileXY( MINER *miners, int tx, int ty );

#endif

#ifdef _MINER_IMPLEMENTATION

bool allocMiners(MINER **miners)
{
	if ( !(*miners) )
	{
		(*miners) = (MINER*) calloc( minersBlockSize, sizeof(MINER) );
		if (*miners) 
		{
			minersAllocated = minersBlockSize;
			for ( int i=0;i<minersAllocated;i++)
			{
				(*miners)[i].valid = false;
			}
		}
	} else {
		(*miners) = (MINER*) realloc( (void*) (*miners), minersAllocated+minersBlockSize);
		if (*miners) 
		{
			for ( int i=minersAllocated;i<minersAllocated+minersBlockSize;i++)
			{
				(*miners)[i].valid = false;
			}
			minersAllocated += minersBlockSize;
		}
	}
	if (*miners)
	{
		return true;
	}
	return false;
}

int addMiner( MINER **miners, int tx, int ty, int type, int direction )
{
	if (minersNum >= minersAllocated)
	{
		if ( !allocMiners( miners ) )
		{
			printf("Memory\n");
			return -1;
		}
	}

	// find the next allocated miner that isn't being used (valid==false)
	int mnum = 0;
	while( (*miners)[mnum++].valid && mnum < minersAllocated );
	// did we find one?
	if ( (*miners)[mnum].valid )
	{
		printf("Error\n");
		return -1;
	}

	// set-up the miner
	(*miners)[mnum].valid = true;
	(*miners)[mnum].tx = tx; 
	(*miners)[mnum].ty = ty;
	(*miners)[mnum].x = tx*gTileSize; 
	(*miners)[mnum].y = ty*gTileSize;
	(*miners)[mnum].type = type;
	(*miners)[mnum].speed = 300;
	(*miners)[mnum].direction = direction;
	(*miners)[mnum].ticksTillProduce = clock() + (*miners)[mnum].speed;
	minersNum++;
	//TAB(0,4);printf("added miner %d %d,%d\n",mnum,tx,ty);
	
	return mnum;
}

void deleteMiner( MINER *miners, int mnum )
{
	if ( mnum < minersAllocated )
	{
		miners[mnum].valid = false;
		--minersNum;
	}
}

void minerProduce( MINER *miners, int m, ItemNodePtr *itemlist )
{
	if ( !miners[m].valid ) return;
	if ( miners[m].ticksTillProduce < clock() )
	{
		miners[m].ticksTillProduce = clock() + miners[m].speed;
		if ( ! isAnythingAtXY(itemlist, miners[m].x+4, miners[m].y+4) )
		{
			insertAtFrontItemList(itemlist, miners[m].type, miners[m].x+4, miners[m].y+4);
		}
	}
}

int getMinerAtTileXY( MINER *miners, int tx, int ty )
{
	for (int m=0;m<minersAllocated;m++)
	{
		if ( !miners[m].valid ) continue;
		if ( miners[m].tx == tx && miners[m].ty == ty ) return m;
	}
	return -1;
}

#endif
