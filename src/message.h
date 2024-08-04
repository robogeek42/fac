/*
  vim:ts=4
  vim:sw=4
*/
#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "item.h"

typedef struct {
	int tx; int ty;
	char text[40];
	int len;
	int item;
	int bgcol;
	int width_box;
	int height_tiles;
	clock_t timeout_ticks;
} MessageInfo;

MessageInfo messageinfo;
bool bMessage = false;

#endif

//--------------------------------------------------------------------
#ifdef _MESSAGE_IMPLEMENTATION

void message_clear(MessageInfo *msg)
{
	int width_tiles = 4 + msg->width_box / gTileSize;

	draw_horizontal( msg->tx-2, msg->ty-1, width_tiles );
	draw_horizontal_layer( msg->tx-2, msg->ty-1, width_tiles, true, true, true );

	draw_horizontal( msg->tx-2, msg->ty, width_tiles );
	draw_horizontal_layer( msg->tx-2, msg->ty, width_tiles, true, true, true );

	draw_horizontal( msg->tx-2, msg->ty+1, width_tiles );
	draw_horizontal_layer( msg->tx-2, msg->ty+1, width_tiles, true, true, true );
	
	if (msg->height_tiles==2)
	{
		draw_horizontal( msg->tx-2, msg->ty+2, width_tiles );
		draw_horizontal_layer( msg->tx-2, msg->ty+2, width_tiles, true, true, true );
	}

}

void new_message(MessageInfo *msg, char *message, int item, int timeout, int bgcol)
{
	if (bMessage) message_clear(msg);

	// place message above and to right of bob
	msg->tx = fac.bobx/gTileSize + 1;
	msg->ty = fac.boby/gTileSize - 1;
	
	msg->bgcol = bgcol;

	msg->len = strlen(message);
	strcpy(msg->text, message);

	msg->width_box = msg->len * 8 + 8;

	msg->item = item;

	msg->height_tiles = 1;
	if ( item >= 0 )
	{
		if ( itemtypes[item].size == 0 )
		{
			msg->height_tiles = 2;
			msg->width_box += 20;
		} else {
			msg->width_box += 12;
		}
	}

	bMessage = true;
	msg->timeout_ticks = clock()+timeout;
}

void display_message(MessageInfo *msg)
{
	if (!bMessage) return;

	// current screen position for drawing
	int sx = msg->tx*gTileSize - fac.xpos;
	int sy = msg->ty*gTileSize - fac.ypos;

	vdp_write_at_graphics_cursor();
	draw_filled_box(sx-4, sy-4, msg->width_box, (msg->height_tiles+1)*8, 11, msg->bgcol);

	if (msg->item >= 0)
	{
		vdp_adv_select_bitmap( itemtypes[msg->item].bmID );
		vdp_draw_bitmap( sx, sy );
		sx += 4 + msg->height_tiles * 8;
		sy += (msg->height_tiles - 1) * 4;
	}

	vdp_move_to(sx, sy);
	vdp_gcol(0,15);
	printf("%s",msg->text);
	vdp_write_at_text_cursor();

}

#endif
