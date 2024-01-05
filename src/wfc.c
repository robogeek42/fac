#include "wfc.h"

static int line = 0;

int wfc_min(int a, int b)
{
	return (a<b)?a:b;
}

void wfc_set_tile(TILE* screen[HEIGHT_TILES][WIDTH_TILES], int x, int y, uint8_t id)
{
#if DEBUG==1
	vdp_set_text_colour(15); vdp_cursor_tab(0,line++); printf("Set tile: %dx%d to %d\n", x,y,id);
#endif

	screen[y][x]->id = id;
	screen[y][x]->entropy = 0;

	display_tile(screen, x, y);

	if (y-1 >= 0) { 
		if (screen[y-1][x]->entropy>0) wfc_reduce_entropy(screen, x, y-1, 2, id); 
	}
	if (x+1 < WIDTH_TILES) { 
		if (screen[y][x+1]->entropy>0) wfc_reduce_entropy(screen, x+1, y, 3, id); 
	}
	if (y+1 < HEIGHT_TILES) { 
		if (screen[y+1][x]->entropy>0) wfc_reduce_entropy(screen, x, y+1, 0, id); 
	}
	if (x-1 >= 0) { 
		if (screen[y][x-1]->entropy>0) wfc_reduce_entropy(screen, x-1, y, 1, id); 
	}
}

/* for given tile screen[y][x], reduce entropy based on neighbour nb (0-3).
 * res_id is the resolved id of the neighbour nb
 */
void wfc_reduce_entropy(TILE* screen[HEIGHT_TILES][WIDTH_TILES], int x, int y, int nb, uint8_t res_id)
{
	int opp = (nb+2)%4; // opposite neighbour position
#if DEBUG==1
	vdp_set_text_colour(2); vdp_cursor_tab(0,line++); printf("Red: %dx%d nb=%d -> %d ent %d", x,y,nb,res_id,screen[y][x]->entropy);
#endif
	
	// case maximum entropy. possibles is not populated
	if (screen[y][x]->entropy == NUM_TILE_TYPES)
	{
		int num_match=0;
		// calculate tiles that can go here from ALL possible types
		for (uint8_t t=0;t<NUM_TILE_TYPES;t++)
		{
			if (mytiles[t].edge[nb] == mytiles[res_id].edge[opp])
			{
				screen[y][x]->possibles[num_match] = t;
				num_match++;
			}
		}
		screen[y][x]->entropy = num_match;
		if (num_match == 0)
		{
			vdp_set_text_colour(1); vdp_cursor_tab(0,line++);printf("error num_match=0"); exit(-1);
		}
	} else {
		uint8_t arr[MAX_POSSIBLES];
		int num_match=0;
		int num_possibles =  wfc_min(screen[y][x]->entropy,MAX_POSSIBLES);
#if DEBUG==1
		vdp_set_text_colour(3); vdp_cursor_tab(0,line++); printf("poss "); for (int i=0; i < num_possibles; i++) {printf("%i,",screen[y][x]->possibles[i]);}
#endif

		for (int i=0; i < num_possibles; i++)
		{
			uint8_t t=screen[y][x]->possibles[i];
			if (mytiles[t].edge[nb] == mytiles[res_id].edge[opp])
			{
				arr[num_match] = t;
				num_match++;
			}
		}
		screen[y][x]->entropy = num_match;

		if (num_match == 0)
		{
			vdp_set_text_colour(1); vdp_cursor_tab(0,line++);printf("error num_match=0"); exit(-1);
		}

		for (int i=0;i<num_match;i++)
		{
			screen[y][x]->possibles[i]=arr[i];
		}

	}

	if (screen[y][x]->entropy == 1)
	{
#if DEBUG==1
		vdp_set_text_colour(1); vdp_cursor_tab(0,line++); printf("Resolve: %dx%d-> %d\n", x,y,screen[y][x]->possibles[0]);
#endif
		wfc_set_tile(screen, x,y,screen[y][x]->possibles[0]);
	}
	else
	{
#if DEBUG==1
		vdp_set_text_colour(5); vdp_cursor_tab(0,line++); printf("now "); for (int i=0; i < screen[y][x]->entropy; i++) {printf("%i,",screen[y][x]->possibles[i]);}
#endif
	}
}


// Find a tile that needs resolving next. 
// Choose first unresolved tile with entropy < max
TILE *wfc_find_tile(TILE* screen[HEIGHT_TILES][WIDTH_TILES])
{
	TILE *candidate = NULL;

	for (int i=0;i<NUM_TILES;i++) {
		int x=i%WIDTH_TILES;
		int y=i/WIDTH_TILES;
		if (screen[y][x]->entropy>0 && screen[y][x]->entropy<NUM_TILE_TYPES)
		{
			candidate = screen[y][x];
			break;
		}
	}
#if DEBUG==1
	if (candidate!=NULL) {
		vdp_set_text_colour(15); vdp_cursor_tab(0,line++); printf("find %dx%d %d\n",candidate->posx, candidate->posy, candidate->entropy);;
	}
#endif
	return candidate;
}

// weighted choice
uint8_t wfc_get_rand_poss(TILE *tile)
{
	int num_choices = 0; // tile->entropy;
	uint8_t choice = tile->possibles[0];
	int index, index_real, sum;

#if DEBUG==1
	if (tile->entropy == NUM_TILE_TYPES)
	{
		vdp_set_text_colour(6); vdp_cursor_tab(0,line++); printf("choices ALL");
	} else {
		vdp_set_text_colour(6); vdp_cursor_tab(0,line++); printf("choices "); for (int i=0; i < tile->entropy; i++) {printf("%i,",tile->possibles[i]);}
	}
#endif

	for (int i=0;i<tile->entropy; i++)
	{
		num_choices += mytiles[tile->possibles[i]].land_weight+1;
	}
	index = rand() % num_choices;
#if DEBUG==1
	vdp_set_text_colour(15); vdp_cursor_tab(0,line++); printf("num_choices %d rand %d ",num_choices, index);;
#endif
	index_real = 0;
	sum=0;

	do {
		sum += mytiles[tile->possibles[index_real]].land_weight+1;
		index_real++;
	} while (sum <= index);

	choice = tile->possibles[index_real-1];

#if DEBUG==1
	printf("choose n=%d %d",index_real-1, choice);;
#endif

	return choice;
}

void wfc_show_debug_screen(TILE* screen[HEIGHT_TILES][WIDTH_TILES]) 
{
	vdp_clear_screen();
	for (int y=0;y<HEIGHT_TILES;y++)
	{
		for (int x=0;x<WIDTH_TILES;x++)
		{
			if (screen[y][x]->entropy == 0)
			{
				display_tile(screen, x,y);
			} else if (screen[y][x]->entropy > 0 && screen[y][x]->entropy < NUM_TILE_TYPES)
			{
				vdp_cursor_tab(x*2,y*2); printf("%02X",screen[y][x]->entropy);
			}
		}
	}
	line=0;
}

