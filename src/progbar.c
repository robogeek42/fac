/*
  vim:ts=4
  vim:sw=4
*/
#include "progbar.h"

PROGBAR* init_horiz_bar(int x, int y, int w, int h, 
		int start, int target,
		int border_col, int col)
{
	PROGBAR *bar = (PROGBAR*) malloc(sizeof(PROGBAR));
	if (!bar)
	{
		printf("Out of memory\n");
		return NULL;
	}
	
	bar->x = x;
	bar->y = y;
	bar->w = w;
	bar->h = h;
	bar->val = start;
	bar->target = target;
	bar->border_col = border_col;
	bar->col = col;

	return bar;
}
void update_bar(PROGBAR *bar, int val)
{
	if (!bar) return;
	bar->val = val;

	// draw border
	draw_box(bar->x, bar->y, bar->w, bar->h, bar->border_col);

	float size = (float)((bar->w-2)*bar->val)/ (float)bar->target;
	draw_filled_box(
			bar->x+1,
			bar->y+1,
			(int) size,
			bar->h-2, 
			bar->col,
			bar->col);
}

void delete_bar(PROGBAR **bar)
{
	if (*bar)
	{
		free(*bar);
		*bar=NULL;
	}
}

void test_progbar()
{
	PROGBAR *progbar;
	progbar = init_horiz_bar(10,10,300,32,0,10,1,3);
	if (!progbar) return;

	clock_t wait_ticks = clock()+50;
	TAB(0,8);
	for (int i=0; i<=10; i++)
	{
		update_bar(progbar, i);
		printf("%d ",i);

		while (clock() < wait_ticks) { vdp_update_key_state(); }
		wait_ticks = clock()+100;
	}
	delete_bar(&progbar);
}
