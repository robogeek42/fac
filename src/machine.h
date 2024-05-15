/*
  vim:ts=4
  vim:sw=4
*/
#ifndef _MACHINE_H
#define _MACHINE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Machine can process up to 2 items to produce 1
typedef struct {
	uint8_t in[2];
	uint8_t out;
} ProcessType;

typedef struct {
	uint8_t machine_type; // enum ItemTypesEnum
	int tx;
	int ty;
	uint8_t outdir;
	int processtime;
	ProcessType process_type;
	clock_t ticksTillProduce;
} Machine;

// use furnaceProcessTypes[ rawtype - IT_TYPES_RAW ]
// to get the correct process type

ProcessType furnaceProcessTypes[3] = {
	{ {IT_STONE,		0}, IT_STONE_BRICK	},
	{ {IT_IRON_ORE,		0}, IT_IRON_PLATE	},
	{ {IT_COPPER_ORE,	0}, IT_COPPER_PLATE	},
};

#endif
#ifdef _MACHINE_IMPLEMENTATION
int machineCount = 0;
int machinesAllocated = 0;
int machinesBlockSize = 10;

bool allocMachines(Machine **machines)
{
	if ( !(*machines) )
	{
		(*machines) = (Machine*) calloc( machinesBlockSize, sizeof(Machine) );
		if (*machines) 
		{
			machinesAllocated = machinesBlockSize;
			for ( int i=0;i<machinesAllocated;i++)
			{
				(*machines)[i].machine_type = 0; // not valid
			}
		}
	} else {
		(*machines) = (Machine*) realloc( (void*) (*machines), machinesAllocated+machinesBlockSize);
		if (*machines) 
		{
			for ( int i=machinesAllocated;i<machinesAllocated+machinesBlockSize;i++)
			{
				(*machines)[i].machine_type = 0; // not valid
			}
			machinesAllocated += machinesBlockSize;
		}
	}
	if (*machines) return true;
	return false;
}

int addMiner( Machine **machines, int tx, int ty, uint8_t rawtype, uint8_t direction )
{
	if (machineCount >= machinesAllocated)
	{
		if ( !allocMachines( machines ) ) {
			printf("Memory\n");
			return -1;
		}
	}

	// find the next allocated machine that isn't being used (valid==false)
	int mnum = 0;
	for (mnum=0;mnum<machinesAllocated;mnum++)
	{
		if ( (*machines)[mnum].machine_type == 0 ) break;
	}
	// did we find one?
	if ( mnum == machinesAllocated )
	{
		printf("Error\n");
		return -1;
	}
	// set-up the machine as a miner
	(*machines)[mnum].machine_type = IT_MINER; 
	(*machines)[mnum].tx = tx; 
	(*machines)[mnum].ty = ty;
	(*machines)[mnum].process_type.in[0] = 0;
	(*machines)[mnum].process_type.in[1] = 0;
	(*machines)[mnum].process_type.out = rawtype;
	(*machines)[mnum].processtime = 300;
	(*machines)[mnum].outdir = direction;
	(*machines)[mnum].ticksTillProduce = clock() + (*machines)[mnum].processtime;
	machineCount++;
	//TAB(0,4);printf("added miner %d %d,%d\n",mnum,tx,ty);
	
	return mnum;
}

void deleteMachine( Machine *machines, int mnum )
{
	if ( mnum < machinesAllocated )
	{
		machines[mnum].machine_type = 0;
		--machineCount;
	}
}

// special case for machine producer for 0 raw materials
// make generic later?
void minerProduce( Machine *machines, int m, ItemNodePtr *itemlist )
{
	if ( machines[m].machine_type == 0 ) return;
	if ( machines[m].ticksTillProduce < clock() )
	{
		machines[m].ticksTillProduce = clock() + machines[m].processtime;
		if ( ! isAnythingAtXY(itemlist, 
					machines[m].tx*gTileSize+4, machines[m].ty*gTileSize+4) )
		{
			insertAtFrontItemList(itemlist, 
					machines[m].process_type.out, 
					machines[m].tx*gTileSize+4, 
					machines[m].ty*gTileSize+4);
		}
	}
}

int getMachineAtTileXY( Machine *machines, int tx, int ty )
{
	for (int m=0;m<machinesAllocated;m++)
	{
		if ( machines[m].machine_type == 0 ) continue;
		if ( machines[m].tx == tx && machines[m].ty == ty ) return m;
	}
	return -1;
}

#endif
