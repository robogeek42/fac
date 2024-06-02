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

// Machine can process up to 3 items to produce 1
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
	int end_tx;
	int end_ty;
	uint8_t outDir;
	int processTime;
	int ptype;
	int countIn[3];
	int countOut;
	clock_t ticksTillProduce;
	ItemNodePtr itemlist;
} Machine;

// use furnaceProcessTypes[ rawtype - IT_TYPES_RAW ]
// to get the correct process type

#define NUM_MINER_PROCESSES 5
ProcessType minerProcessTypes[NUM_MINER_PROCESSES] = {
	{ {0, 0, 0}, IT_STONE,	{0,0,0},1,1},
	{ {0, 0, 0}, IT_IRON_ORE,	{0,0,0},1,1},
	{ {0, 0, 0}, IT_COPPER_ORE,	{0,0,0},1,1},
	{ {0, 0, 0}, IT_COAL,	{0,0,0},1,1},
	{ {0, 0, 0}, IT_WOOD,	{0,0,0},1,1},
};

#define NUM_FURNACE_PROCESSES 3
ProcessType furnaceProcessTypes[NUM_FURNACE_PROCESSES] = {
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

int addMachine( Machine **machines, uint8_t machine_type, int tx, int ty, uint8_t direction, int speed, int ptype )
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
	int end_tx = tx; 
	int end_ty = ty;
	switch(direction)
	{
		case DIR_UP:
			end_ty -= 1; break;
		case DIR_RIGHT:
			end_tx += 1; break;
		case DIR_DOWN:
			end_ty += 1; break;
		case DIR_LEFT:
			end_tx -= 1; break;
		default:
			break;
	}
	// set-up the machine as a miner
	(*machines)[mnum].machine_type = machine_type; 
	(*machines)[mnum].tx = tx; 
	(*machines)[mnum].ty = ty;
	(*machines)[mnum].end_tx = end_tx; 
	(*machines)[mnum].end_ty = end_ty;
	(*machines)[mnum].ptype = ptype;
	(*machines)[mnum].countIn[0] = 0;
	(*machines)[mnum].countIn[1] = 0;
	(*machines)[mnum].countIn[2] = 0;
	(*machines)[mnum].countOut = 0;
	(*machines)[mnum].processTime = speed;
	(*machines)[mnum].outDir = direction;
	(*machines)[mnum].ticksTillProduce = 0;
	(*machines)[mnum].itemlist = NULL;
	machineCount++;
	TAB(0,4);printf("addmach %d %d,%d dir%d\n",mnum,tx,ty, direction);
	
	return mnum;
}

int addMiner( Machine **machines, int tx, int ty, uint8_t rawtype, uint8_t direction)
{
	int ptype=0;
	for (ptype=0; ptype<NUM_MINER_PROCESSES; ptype++)
	{
		if ( minerProcessTypes[ptype].out == rawtype ) { break; }
	}
	if (ptype == NUM_MINER_PROCESSES) return -1;

	int m = addMachine( machines, IT_MINER, tx, ty, direction, 300, ptype);
	(*machines)[m].ticksTillProduce = clock() + (*machines)[m].processTime;
	return m;
}

int addFurnace( Machine **machines, int tx, int ty, uint8_t direction )
{
	return addMachine( machines, IT_FURNACE, tx, ty, direction, 200, -1);
}

int addAssembler( Machine **machines, int tx, int ty, uint8_t direction, int ptype )
{
	return addMachine( machines, IT_ASSEMBLER, tx, ty, direction, 400, ptype );
}

// special case for machine producer for 0 raw materials
bool minerProduce( Machine *machines, int m)
{
	if ( machines[m].machine_type == 0 ) return false;
	if ( machines[m].ticksTillProduce < clock() )
	{
		machines[m].ticksTillProduce = clock() + machines[m].processTime;
		if ( ! isAnythingAtXY(&machines[m].itemlist, 
					machines[m].tx*gTileSize+4, machines[m].ty*gTileSize+4) )
		{
			insertAtFrontItemList(&machines[m].itemlist, 
					minerProcessTypes[machines[m].ptype].out, 
					machines[m].tx*gTileSize+4, 
					machines[m].ty*gTileSize+4);
			return true;
		}
	}
	return false;
}

// machine producer for 1 raw materials
bool furnaceProduce( Machine *machines, int m )
{
	if ( machines[m].machine_type == 0 ) return true;
	if ( machines[m].ticksTillProduce == 0 && 
			machines[m].countIn[0] > 0 )
	{
		machines[m].ticksTillProduce = clock() + machines[m].processTime;
		machines[m].countIn[0]--;
	}
	if ( machines[m].ticksTillProduce != 0 &&
		 machines[m].ticksTillProduce < clock() )
	{
		machines[m].ticksTillProduce = 0;
		if ( ! isAnythingAtXY(&machines[m].itemlist, 
					machines[m].tx*gTileSize+4, machines[m].ty*gTileSize+4) )
		{
			int  outx = machines[m].tx*gTileSize+4;
			int  outy = machines[m].ty*gTileSize+4;
			/*
			switch ( machines[m].outDir )
			{
				case DIR_UP: outy-=8; break;
				case DIR_RIGHT: outx+=8; break;
				case DIR_DOWN: outy+=8; break;
				case DIR_LEFT: outx-=8; break;
				default: break;
			}
			*/

			insertAtFrontItemList(&machines[m].itemlist, 
					furnaceProcessTypes[machines[m].ptype].out, outx, outy);
			return true;
		}
	}
	return false;
}

// machine producer for N raw materials
bool assemblerProduce( Machine *machines, int m )
{
	if ( machines[m].machine_type == 0 ) return true;

	int ptype = machines[m].ptype;
	ProcessType *pt = &assemblerProcessTypes[ptype];
	if ( machines[m].ticksTillProduce == 0 )
	{
		// check to see if recipe inputs are satisfied
		bool materials_present = true;
		for (int k=0; k < pt->innum; k++)
		{
			if (machines[m].countIn[k] < pt->incnt[k] )
			{
				materials_present = false;
				break;
			}
		}
		if ( materials_present )
		{
			// got all inputs? consume them
			for (int k=0; k < pt->innum; k++)
			{
				machines[m].countIn[k] -= pt->incnt[k];
			}
			// start timer
			machines[m].ticksTillProduce = clock() + machines[m].processTime;
		}
	}

	// are we producing? Has timer expired?
	if ( machines[m].ticksTillProduce != 0 &&
		 machines[m].ticksTillProduce < clock() )
	{
		// reset timer
		machines[m].ticksTillProduce = 0;
		machines[m].countOut += pt->outcnt; // usually 1

		int  outx = machines[m].tx*gTileSize+4;
		int  outy = machines[m].ty*gTileSize+4;
		switch ( machines[m].outDir )
		{
			case DIR_UP: outy-=8; break;
			case DIR_RIGHT: outx+=8; break;
			case DIR_DOWN: outy+=8; break;
			case DIR_LEFT: outx-=8; break;
			default: break;
		}
		//TAB(0,1+m);printf("m%d:d%d %d,%d\nprod %d,%d ",m, machines[m].outDir, machines[m].tx*gTileSize,  machines[m].ty*gTileSize, outx,outy);
		if ( ! isAnythingAtXY(&machines[m].itemlist, outx, outy) )
		{
			machines[m].countOut -= 1;
			insertAtFrontItemList(&machines[m].itemlist, 
					assemblerProcessTypes[ptype].out, outx, outy);
			return true;
		}
	}
	return false;
}

#endif
