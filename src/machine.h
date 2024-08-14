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

// common machine header (common to all objects, e.g. inserters)
typedef struct {
	uint8_t machine_type; // enum ItemTypesEnum
	uint8_t dir;
	uint16_t tx;
	uint16_t ty;
	uint16_t end_tx;
	uint16_t end_ty;
} MachineHeader;

// Machine instance
typedef struct {
	uint8_t machine_type; // enum ItemTypesEnum
	uint8_t dir;
	uint16_t tx;
	uint16_t ty;
	uint16_t end_tx;
	uint16_t end_ty;
	int processTime;
	int8_t ptype;
	int countIn[3];
	int countOut;
	uint8_t energyCost;
	clock_t ticksTillProduce;
	
	ItemNodePtr itemlist;
} Machine;

// Matching machine instance without itemlist for saving
typedef struct {
	uint8_t machine_type; // enum ItemTypesEnum
	uint8_t dir;
	uint16_t tx;
	uint16_t ty;
	uint16_t end_tx;
	uint16_t end_ty;
	int processTime;
	int8_t ptype;
	int countIn[3];			// count of objects inserted into machine
	int countOut;			// outputs ready to send out (not used?)
	clock_t ticksTillProduce;
} MachineSave;

// Machine can process up to 3 items to produce 1
typedef struct {
	uint8_t in[3];		// up to 3 input types
	uint8_t out;		// one output type
	uint8_t incnt[3];	// how many of each input is required
	uint8_t outcnt;		// how many of each output is produced
	uint8_t innum;		// number of valid inputs 
	bool manual;		// can be made manually
} ProcessType;

#define NUM_MINER_PROCESSES 5
ProcessType minerProcessTypes[NUM_MINER_PROCESSES] = {
	{ {0, 0, 0}, IT_STONE,	{0,0,0},1,1, false},
	{ {0, 0, 0}, IT_IRON_ORE,	{0,0,0},1,1, false},
	{ {0, 0, 0}, IT_COPPER_ORE,	{0,0,0},1,1, false},
	{ {0, 0, 0}, IT_COAL,	{0,0,0},1,1, false},
	{ {0, 0, 0}, IT_WOOD,	{0,0,0},1,1, false},
};

#define NUM_FURNACE_PROCESSES 3
ProcessType furnaceProcessTypes[NUM_FURNACE_PROCESSES] = {
	{ {IT_STONE,		0,	0}, IT_STONE_BRICK,	{1,0,0},1,1, false},
	{ {IT_IRON_ORE,		0,	0}, IT_IRON_PLATE,	{1,0,0},1,1, false},
	{ {IT_COPPER_ORE,	0,	0}, IT_COPPER_PLATE,{1,0,0},1,1, false},
};

#define NUM_ASM_PROCESSES 13
#define NUM_MANUAL_ASM_PROCESSES 9
ProcessType assemblerProcessTypes[NUM_ASM_PROCESSES] = {
	{ {IT_STONE,			IT_WOOD,		0			}, IT_PROD_FURNACE,		{2, 1, 0}, 1, 2, true },
	{ {IT_IRON_PLATE,		0,				0			}, IT_GEARWHEEL,		{1, 0, 0}, 2, 1, true },
	{ {IT_COPPER_PLATE,		0, 				0			}, IT_WIRE,				{1, 0, 0}, 2, 1, true },
	{ {IT_IRON_PLATE,		IT_WIRE, 		0			}, IT_CIRCUIT,			{1, 2, 0}, 1, 2, true },
	{ {IT_IRON_PLATE,		IT_STONE_BRICK,	0			}, IT_PROD_MINER,		{1, 1, 0}, 1, 2, true },
	{ {IT_WOOD,				0,				0			}, IT_PROD_BOX,			{4, 0, 0}, 1, 1, true },
	{ {IT_IRON_PLATE,		IT_GEARWHEEL,	IT_CIRCUIT	}, IT_PROD_ASSEMBLER,	{2, 2, 1}, 1, 3, true },
	{ {IT_IRON_PLATE,		IT_STONE_BRICK,	IT_WIRE		}, IT_PROD_GENERATOR,	{2, 2, 2}, 1, 3, true },
	{ {IT_WOOD,				IT_GEARWHEEL,	0			}, IT_MINI_BELT,		{1, 2, 0}, 1, 2, true },
	{ {IT_STONE_BRICK,		0,				0			}, IT_PAVING,			{4, 0, 0}, 1, 1, false },
	{ {IT_GEARWHEEL,		IT_MINI_BELT,	IT_CIRCUIT	}, IT_PROD_INSERTER,	{2, 1, 1}, 1, 3, false },
	{ {IT_PROD_ASSEMBLER,	IT_WIRE,		IT_CIRCUIT	}, IT_COMPUTER,			{1, 2, 2}, 1, 3, false },
	{ {IT_MINI_BELT,		IT_IRON_PLATE,	IT_CIRCUIT	}, IT_PROD_TSPLITTER,	{1, 2, 1}, 1, 3, false },
};

#define NUM_GENERATOR_PROCESSES 2
ProcessType generatorProcessTypes[NUM_GENERATOR_PROCESSES] = {
	{ {IT_WOOD, 0, 0}, 0,	{1,0,0},30,1, false},
	{ {IT_COAL, 0, 0}, 0,	{1,0,0},120,1, false},
};


// splitter switch states
typedef struct {
	uint8_t in;		// input direction
	uint8_t out[2]; // state 0/1 output direction
} TSplitSwitchStates;

TSplitSwitchStates tsplit_states[8] = {
	{ 2, {1, 3} },
	{ 3, {2, 0} },
	{ 0, {1, 3} },
	{ 1, {2, 0} }
};

#endif

//--------------------------------------------------------------------
#ifdef _MACHINE_IMPLEMENTATION

Machine* findMachine(ThingNodePtr *machinelist,  int tx, int ty)
{
	if (isEmptyThingList(machinelist)) {
		return false;
	}

	ThingNodePtr currPtr = (*machinelist);
	Machine *mach = NULL;
	while (currPtr != NULL ) {
		mach = (Machine*) currPtr->thing;
		if ( mach != NULL && mach->tx == tx && mach->ty == ty )
		{
			break;
		}
		currPtr = currPtr->next;
	}

	if (currPtr == NULL) {
		return NULL;
	}

	return mach;

}

ThingNodePtr getMachineNode(ThingNodePtr *machinelist, Machine* mach)
{
	if (isEmptyThingList(machinelist)) {
		return false;
	}

	ThingNodePtr currPtr = (*machinelist);
	while (currPtr != NULL ) {
		if ( (Machine*)currPtr->thing == mach )
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

int countMachine(ThingNodePtr *machinelist)
{
	int cnt = 0;
	if (isEmptyThingList(machinelist)) {
		return 0;
	}

	ThingNodePtr currPtr = (*machinelist);
	while (currPtr != NULL ) {
		cnt++;
		currPtr = currPtr->next;
	}

	return cnt;
}

void clearMachines(ThingNodePtr *machinelist)
{
	ThingNodePtr currPtr = (*machinelist);
	ThingNodePtr nextPtr = NULL;
	Machine *mach = NULL;
	while (currPtr != NULL ) {
		nextPtr = currPtr->next;
		mach = (Machine*) currPtr->thing;
		
		clearItemList(&mach->itemlist);
		free(mach); currPtr->thing = NULL;
		free(currPtr); 
		currPtr = nextPtr;
	}
	
}

ProcessType* getProcessType(Machine *mach)
{
	ProcessType *pt = NULL;
	switch (mach->machine_type)
	{
		case IT_MINER:
			pt = &minerProcessTypes[mach->ptype];
			break;
		case IT_FURNACE:
			pt = &furnaceProcessTypes[mach->ptype];
			break;
		case IT_ASSEMBLER:
			pt = &assemblerProcessTypes[mach->ptype];
			break;
		case IT_GENERATOR:
			pt = &generatorProcessTypes[mach->ptype];
			break;
		default:
			break;
	}
	return pt;
}

Machine* addMachine( ThingNodePtr *machinelist, uint8_t machine_type, uint16_t tx, uint16_t ty, uint8_t direction, int speed, int ptype, uint8_t energyCost )
{
	Machine *mach = (Machine*) malloc(sizeof(Machine));
	if ( !mach )
	{
		return NULL;
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
	// set-up the machine 
	mach->machine_type = machine_type; 
	mach->dir = direction;
	mach->tx = tx; 
	mach->ty = ty;
	mach->end_tx = end_tx; 
	mach->end_ty = end_ty;
	mach->ptype = ptype;
	mach->countIn[0] = 0;
	mach->countIn[1] = 0;
	mach->countIn[2] = 0;
	mach->countOut = 0;
	mach->processTime = speed;
	mach->ticksTillProduce = 0;
	mach->energyCost = energyCost;
	mach->itemlist = NULL;
	//TAB(0,4);printf("addmach %d,%d dir%d\n",tx,ty, direction);

	insertAtFrontThingList(machinelist, mach);

	return mach;
}

Machine* addMiner( ThingNodePtr *machinelist, int tx, int ty, uint8_t rawtype, uint8_t direction)
{
	int ptype=0;
	for (ptype=0; ptype<NUM_MINER_PROCESSES; ptype++)
	{
		if ( minerProcessTypes[ptype].out == rawtype ) { break; }
	}
	if (ptype == NUM_MINER_PROCESSES) return NULL;

	Machine* mach = addMachine( machinelist, IT_MINER, tx, ty, direction, 300, ptype, 5);
	mach->ticksTillProduce = clock() + mach->processTime;
	return mach;
}

Machine* addFurnace( ThingNodePtr *machinelist, int tx, int ty, uint8_t direction, int ptype )
{
	return addMachine( machinelist, IT_FURNACE, tx, ty, direction, 200, ptype, 10 );
}

Machine* addAssembler( ThingNodePtr *machinelist, int tx, int ty, uint8_t direction, int ptype )
{
	return addMachine( machinelist, IT_ASSEMBLER, tx, ty, direction, 400, ptype, 20 );
}

Machine* addGenerator( ThingNodePtr *machinelist, int tx, int ty, uint8_t direction, int ptype )
{
	return addMachine( machinelist, IT_GENERATOR, tx, ty, direction, 100, ptype, 1 );
}

// special case for machine producer for 0 raw materials
// continuously produces (no inputs)
bool minerProduce( Machine* mach, int *energy)
{
	if ( mach->machine_type == 0 ) return false;
	if ( mach->ticksTillProduce < clock() )
	{
		mach->ticksTillProduce = clock() + mach->processTime;
#if defined MACH_NEED_ENERGY
		if ( (*energy) >= mach->energyCost )
#endif
		{
			(*energy) -= mach->energyCost;
			if ( ! isAnythingAtXY(&mach->itemlist, 
						mach->tx*gTileSize+4, mach->ty*gTileSize+4) )
			{
				insertAtFrontItemList(&mach->itemlist, 
						minerProcessTypes[mach->ptype].out, 
						mach->tx*gTileSize+4, 
						mach->ty*gTileSize+4);
				return true;
			}
		}
	}
	return false;
}

// machine producer for 1 raw materials
// ticksTillProduce==0 means machine has not started - needs inputs
// then ticks starts counting before output is produced
bool furnaceProduce( Machine *mach, int *energy )
{
	if ( mach->machine_type == 0 ) return true;

	ProcessType *pt = &furnaceProcessTypes[mach->ptype];

	if ( mach->ticksTillProduce == 0 && 
			mach->countIn[0] > 0 )
#if defined MACH_NEED_ENERGY
	if ( (*energy) >= mach->energyCost )
#endif
	{
		mach->ticksTillProduce = clock() + mach->processTime;
		mach->countIn[0]--;
		(*energy) -= mach->energyCost;
	}

	if ( mach->ticksTillProduce != 0 &&
		 mach->ticksTillProduce < clock() )
	{
		mach->ticksTillProduce = 0;
		mach->countOut += pt->outcnt; 
		return true;
	}
	return false;
}

// machine producer for N raw materials
bool assemblerProduce( Machine *mach, int *energy )
{
	if ( mach->machine_type == 0 ) return true;

	int ptype = mach->ptype;
	ProcessType *pt = &assemblerProcessTypes[ptype];
	if ( mach->ticksTillProduce == 0 )
	{
		// check to see if recipe inputs are satisfied
		bool materials_present = true;
		for (int k=0; k < pt->innum; k++)
		{
			if (mach->countIn[k] < pt->incnt[k] )
			{
				materials_present = false;
				break;
			}
		}
		if ( materials_present )
#if defined MACH_NEED_ENERGY
		if ( (*energy) >= mach->energyCost )
#endif
		{
			// consume inputd
			for (int k=0; k < pt->innum; k++)
			{
				mach->countIn[k] -= pt->incnt[k];
			}
			// start timer
			mach->ticksTillProduce = clock() + mach->processTime;
			(*energy) -= mach->energyCost;
		}
	}

	// are we producing? Has timer expired?
	if ( mach->ticksTillProduce != 0 &&
		 mach->ticksTillProduce < clock() )
	{
		// reset timer
		mach->ticksTillProduce = 0;
		mach->countOut += pt->outcnt; 
		return true;
	}
	return false;
}

bool generatorProduce( Machine *mach, int *energy )
{
	if ( mach->machine_type == 0 ) return true;

	ProcessType *pt = &generatorProcessTypes[mach->ptype];
	if ( mach->ticksTillProduce == 0 )
	{
		// check to see if recipe inputs are satisfied
		bool materials_present = true;
		for (int k=0; k < pt->innum; k++)
		{
			if (mach->countIn[k] < pt->incnt[k] )
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
				mach->countIn[k] -= pt->incnt[k];
			}
			// start timer
			mach->ticksTillProduce = clock() + mach->processTime;
		}
	}

	// are we producing? Has timer expired?
	if ( mach->ticksTillProduce != 0 &&
		 mach->ticksTillProduce < clock() )
	{
		// reset timer
		mach->ticksTillProduce = 0;

		(*energy) += pt->outcnt;
	}
	return false;
}

void pushOutputs(Machine *mach )
{
	if (mach->countOut > 0)
	{
		ProcessType *pt = NULL;
		switch (mach->machine_type)
		{
			case IT_FURNACE:
				pt = &furnaceProcessTypes[mach->ptype];
				break;
			case IT_ASSEMBLER:
				pt = &assemblerProcessTypes[mach->ptype];
				break;
			default:
				break;
		}
		if (pt)
		{
			int  outx = mach->tx*gTileSize+4;
			int  outy = mach->ty*gTileSize+4;
			switch ( mach->dir )
			{
				case DIR_UP: outy-=8; break;
				case DIR_RIGHT: outx+=8; break;
				case DIR_DOWN: outy+=8; break;
				case DIR_LEFT: outx-=8; break;
				default: break;
			}

			if ( ! isAnythingAtXY(&mach->itemlist, outx, outy) )
			{
				mach->countOut -= 1;
				insertAtFrontItemList(&mach->itemlist, pt->out, outx, outy);
			}
		}
	}
}
#endif
