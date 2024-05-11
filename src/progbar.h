#ifndef _PROGBAR_H
#define _PROGBAR_H

#include "globals.h"
#include "../../agon_ccode/common/util.h"

typedef struct {
	int x;
	int y;
	int w;
	int h;
	int val;
	int target;
	int border_col;
	int col;
} PROGBAR;


PROGBAR* init_horiz_bar(int x, int y, int w, int h, 
		int start, int target, 
		int border_col, int col);
void update_bar(PROGBAR *bar, int val);
void delete_bar(PROGBAR *bar);

#endif
