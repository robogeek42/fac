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
	uint8_t in[3];
	uint8_t out;
	uint8_t incnt[3];
	uint8_t outcnt;
	uint8_t innum;
} ProcessType;

typedef struct {
	uint8_t machine_type; // enum ItemTypesEnum
	int tx;
	int ty;
	uint8_t outdir;
	int processtime;
	ProcessType process_type;
	clock_t ticksTillProduce;
	int countIn[2];
} Machine;

// use furnaceProcessTypes[ rawtype - IT_TYPES_RAW ]
// to get the correct process type

ProcessType furnaceProcessTypes[3] = {
	{ {IT_STONE,		0,	0}, IT_STONE_BRICK,	{1,0,0},1,1},
	{ {IT_IRON_ORE,		0,	0}, IT_IRON_PLATE,	{1,0,0},1,1},
	{ {IT_COPPER_ORE,	0,	0}, IT_COPPER_PLATE,{1,0,0},1,1},
};

#define NUM_ASM_PROCESSES 2
ProcessType assemblerProcessTypes[NUM_ASM_PROCESSES] = {
	{ {IT_STONE_BRICK,		IT_COPPER_PLATE, 0}, IT_CIRCUIT, {1, 1, 0}, 1, 2 },
	{ {IT_IRON_PLATE,		IT_WOOD, 0}, IT_GEARWHEEL, {1, 1, 0}, 1, 2 },
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
		printf("realloc %d\n", machinesAllocated+machinesBlockSize);
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

void deleteMachine( Machine *machines, int mnum )
{
	if ( mnum < machinesAllocated )
	{
		machines[mnum].machine_type = 0;
		--machineCount;
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

int addMachine( Machine **machines, uint8_t machine_type, int tx, int ty, uint8_t direction, int speed, uint8_t intype[3], uint8_t outtype, uint8_t incnt[3], uint8_t outcnt, uint8_t innum  )
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
	(*machines)[mnum].machine_type = machine_type; 
	(*machines)[mnum].tx = tx; 
	(*machines)[mnum].ty = ty;
	(*machines)[mnum].process_type.in[0] = intype[0];
	(*machines)[mnum].process_type.in[1] = intype[1];
	(*machines)[mnum].process_type.in[2] = intype[2];
	(*machines)[mnum].process_type.out = outtype;
	(*machines)[mnum].process_type.incnt[0] = incnt[0];
	(*machines)[mnum].process_type.incnt[1] = incnt[1];
	(*machines)[mnum].process_type.incnt[2] = incnt[2];
	(*machines)[mnum].process_type.outcnt = outcnt;
	(*machines)[mnum].process_type.innum = innum;
	(*machines)[mnum].processtime = speed;
	(*machines)[mnum].outdir = direction;
	(*machines)[mnum].ticksTillProduce = 0;
	(*machines)[mnum].countIn[0] = 0;
	(*machines)[mnum].countIn[1] = 0;
	machineCount++;
	//TAB(0,4);printf("added miner %d %d,%d\n",mnum,tx,ty);
	
	return mnum;
}

int addMiner( Machine **machines, int tx, int ty, uint8_t rawtype, uint8_t direction)
{
	uint8_t intype[3] = {0, 0, 0};
	uint8_t incnt[3] = {1, 0, 0};
	int m = addMachine( machines, IT_MINER, tx, ty, direction, 300, 
			intype, rawtype, incnt, 1, 0);
	(*machines)[m].ticksTillProduce = clock() + (*machines)[m].processtime;
	return m;
}

int addFurnace( Machine **machines, int tx, int ty, uint8_t direction )
{
	uint8_t intype[3] = {0, 0, 0};
	uint8_t incnt[3] = {1, 0, 0};
	return addMachine( machines, IT_FURNACE, tx, ty, direction, 200, 
			intype, 0, incnt, 1, 1);
}

int addAssembler( Machine **machines, int tx, int ty, uint8_t direction, ProcessType recipe )
{
	return addMachine( machines, IT_ASSEMBLER, tx, ty, direction, 400, 
			recipe.in, recipe.out, 
			recipe.incnt,
			recipe.outcnt,
			recipe.innum );
}

// special case for machine producer for 0 raw materials
bool minerProduce( Machine *machines, int m, ItemNodePtr *itemlist, int *numItems )
{
	if ( machines[m].machine_type == 0 ) return false;
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
			(*numItems)++;
			return true;
		}
	}
	return false;
}

// machine producer for 1 raw materials
bool furnaceProduce( Machine *machines, int m, ItemNodePtr *itemlist, int *numItems )
{
	if ( machines[m].machine_type == 0 ) return true;
	if ( machines[m].ticksTillProduce == 0 && 
			machines[m].countIn[0] > 0 )
	{
		machines[m].ticksTillProduce = clock() + machines[m].processtime;
		machines[m].countIn[0]--;
	}
	if ( machines[m].ticksTillProduce != 0 &&
		 machines[m].ticksTillProduce < clock() )
	{
		machines[m].ticksTillProduce = 0;
		if ( ! isAnythingAtXY(itemlist, 
					machines[m].tx*gTileSize+4, machines[m].ty*gTileSize+4) )
		{
			int  outx = machines[m].tx*gTileSize+4;
			int  outy = machines[m].ty*gTileSize+4;
			switch ( machines[m].outdir )
			{
				case DIR_UP: outy-=8; break;
				case DIR_RIGHT: outx+=8; break;
				case DIR_DOWN: outy+=8; break;
				case DIR_LEFT: outx-=8; break;
				default: break;
			}

			insertAtFrontItemList(itemlist, 
					machines[m].process_type.out, outx, outy);
			(*numItems)++;
			return true;
		}
	}
	return false;
}

// machine producer for N raw materials
bool assemblerProduce( Machine *machines, int m, ItemNodePtr *itemlist, int *numItems )
{
	if ( machines[m].machine_type == 0 ) return true;
	if ( machines[m].ticksTillProduce == 0 && 
			machines[m].countIn[0] > 0 &&
			machines[m].countIn[1] > 0 )
	{
		machines[m].ticksTillProduce = clock() + machines[m].processtime;
		machines[m].countIn[0]--;
		machines[m].countIn[1]--;
	}
	if ( machines[m].ticksTillProduce != 0 &&
		 machines[m].ticksTillProduce < clock() )
	{
		machines[m].ticksTillProduce = 0;
		if ( ! isAnythingAtXY(itemlist, 
					machines[m].tx*gTileSize+4, machines[m].ty*gTileSize+4) )
		{
			int  outx = machines[m].tx*gTileSize+4;
			int  outy = machines[m].ty*gTileSize+4;
			switch ( machines[m].outdir )
			{
				case DIR_UP: outy-=8; break;
				case DIR_RIGHT: outx+=8; break;
				case DIR_DOWN: outy+=8; break;
				case DIR_LEFT: outx-=8; break;
				default: break;
			}

			insertAtFrontItemList(itemlist, 
					machines[m].process_type.out, outx, outy);
			(*numItems)++;
			return true;
		}
	}
	return false;
}

#endif
