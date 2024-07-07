#ifndef _IMAGE_H
#define _IMAGE_H

#include "progbar.h"

#define FN_TERR16 "img/trnew/tr%02d.rgb2"
#define BMOFF_TERR16 0
#define NUM_BM_TERR16 16

#define FN_FEAT16 "img/tf16/tf%02d.rgb2"
#define BMOFF_FEAT16 ( BMOFF_TERR16 + NUM_BM_TERR16 )
#define NUM_BM_FEAT16 15

#define FN_BOB16  "img/facbob/fb%02d.rgb2"
#define BMOFF_BOB16 ( BMOFF_FEAT16 + NUM_BM_FEAT16)
#define NUM_BM_BOB16 24

#define FN_BELT16 "img/belt16/belt%02d.rgb2"
#define FN_BBELT16 "img/belt16/bbelt%02d.rgb2"
#define BMOFF_BELT16 ( BMOFF_BOB16 + NUM_BM_BOB16)
#define NUM_BM_BELT16 4*4
#define NUM_BM_BBELT16 8*4

#define FN_ITEM8  "img/ti8/ti%02d.rgb2"
#define BMOFF_ITEM8 ( BMOFF_BELT16 + NUM_BM_BELT16 + NUM_BM_BBELT16)
#define NUM_BM_ITEM8 8

#define FN_MACH16 "img/tm16/tm%02d.rgb2"
#define BMOFF_MACH16 ( BMOFF_ITEM8 + NUM_BM_ITEM8 )
#define NUM_BM_MACH16 6

#define FN_NUMS "img/nums4x5/num%01d.rgb2"
#define BMOFF_NUMS ( BMOFF_MACH16 + NUM_BM_MACH16 )
#define NUM_BM_NUMS 10

#define FN_CURSORS "img/cursor%02d.rgb2"
#define BMOFF_CURSORS ( BMOFF_NUMS + NUM_BM_NUMS )
#define NUM_BM_CURSORS 2

#define FN_MINERS "img/tm16/miner%s%02d.rgb2"
#define BMOFF_MINERS ( BMOFF_CURSORS + NUM_BM_CURSORS )
#define NUM_BM_MINERS 12

#define FN_FURNACES "img/tm16/fur%02d.rgb2"
#define BMOFF_FURNACES ( BMOFF_MINERS + NUM_BM_MINERS )
#define NUM_BM_FURNACES 12

#define FN_ASSEMBLERS "img/tm16/asmb%02d.rgb2"
#define BMOFF_ASSEMBLERS ( BMOFF_FURNACES + NUM_BM_FURNACES )
#define NUM_BM_ASSEMBLERS 12

#define FN_PROD8 "img/tp8/tp%02d.rgb2"
#define BMOFF_PROD8 ( BMOFF_ASSEMBLERS + NUM_BM_ASSEMBLERS)
#define NUM_BM_PROD8 3

#define FN_INSERTERS "img/ins/ins%02d.rgb2"
#define BMOFF_INSERTERS ( BMOFF_PROD8 + NUM_BM_PROD8 )
#define NUM_BM_INSERTERS 12

#define FN_ZAP "img/zap8x8.rgb2"
#define BMOFF_ZAP ( BMOFF_INSERTERS + NUM_BM_INSERTERS )
#define NUM_BM_ZAP 1

#define TOTAL_BM ( BMOFF_ZAP + NUM_BM_ZAP)

#define BOB_SPRITE_DOWN 0
#define BOB_SPRITE_UP 1
#define BOB_SPRITE_LEFT 2
#define BOB_SPRITE_RIGHT 3
#define BOB_SPRITE_ACT_DOWN 4
#define BOB_SPRITE_ACT_UP 5
#define BOB_SPRITE_ACT_LEFT 6
#define BOB_SPRITE_ACT_RIGHT 7


#define NUM_BOB_SPRITES 8

#define CURSOR_SPRITE 8

#define NUM_SPRITES ( CURSOR_SPRITE + 1 )

bool load_images(bool progress);
void create_sprites();
int get_current_sprite();
void select_bob_sprite( int sprite );
void show_bob();
void hide_bob();

static int current_bob_sprite = -1;

#endif

#ifdef _IMAGE_IMPLEMENTATION
bool load_images(bool progress) 
{
	PROGBAR *progbar;
	int cnt=1;
	int prog_max = TOTAL_BM;

	if (progress)
	{
		progbar = init_horiz_bar(10,100,300,32,0,prog_max,1,3);
		
		update_bar(progbar, 0);
		TAB(10,5);printf("Loading Images");
	}

	//TAB(0,0);
	char fname[40];
	for (int fn=1; fn<=NUM_BM_TERR16; fn++)
	{
		sprintf(fname, FN_TERR16, fn);
		int ret = load_bitmap_file(fname, 16, 16, BMOFF_TERR16 + fn-1);
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	for (int fn=1; fn<=NUM_BM_FEAT16; fn++)
	{
		sprintf(fname, FN_FEAT16, fn);
		int ret = load_bitmap_file(fname, 16, 16, BMOFF_FEAT16 + fn-1);
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	for (int fn=1; fn<=NUM_BM_BOB16; fn++)
	{
		sprintf(fname, FN_BOB16, fn);
		int ret = load_bitmap_file(fname, 16, 16, BMOFF_BOB16 + fn-1);
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	for (int fn=1; fn<=NUM_BM_BELT16; fn++)
	{
		sprintf(fname, FN_BELT16, fn);
		int ret = load_bitmap_file(fname, 16, 16, BMOFF_BELT16 + fn-1);
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	for (int fn=1; fn<=NUM_BM_BBELT16; fn++)
	{
		sprintf(fname, FN_BBELT16, fn);
		int ret = load_bitmap_file(fname, 16, 16, BMOFF_BELT16 + NUM_BM_BELT16 + fn-1);
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	for (int fn=1; fn<=NUM_BM_ITEM8; fn++)
	{
		sprintf(fname, FN_ITEM8, fn);
		int ret = load_bitmap_file(fname, 8, 8, BMOFF_ITEM8 + fn-1);
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	for (int fn=1; fn<=NUM_BM_MACH16; fn++)
	{
		sprintf(fname, FN_MACH16, fn);
		int ret = load_bitmap_file(fname, 16, 16, BMOFF_MACH16 + fn-1);
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	for (int fn=0; fn<=NUM_BM_NUMS-1; fn++)
	{
		sprintf(fname, FN_NUMS, fn);
		int ret = load_bitmap_file(fname, 4, 5, BMOFF_NUMS + fn);
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	
	for (int fn=1; fn<=NUM_BM_CURSORS; fn++)
	{
		sprintf(fname, FN_CURSORS, fn);
		int ret = load_bitmap_file(fname, 16, 16, BMOFF_CURSORS + fn-1);
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}

	// 12 miners each with 3 images for animation
	char *dir[] = {"U","R","D","L"};
	for (int d=0;d<4;d++)
	{
		for (int fn=1; fn<=NUM_BM_MINERS/4; fn++)
		{
			sprintf(fname, FN_MINERS, dir[d], fn);
			//printf("Load %s to BMID %d\n",fname, BMOFF_MINERS + fn-1 + d*3);
			int ret = load_bitmap_file(fname, 16, 16, BMOFF_MINERS + fn-1 + d*3 );
			if ( ret < 0 ) return false;
			if (progress) update_bar(progbar, cnt++);
		}
	}
	// 12 furnaces each with 3 images for animation
	for (int fn=1; fn<=NUM_BM_FURNACES; fn++)
	{
		sprintf(fname, FN_FURNACES, fn);
		int ret = load_bitmap_file(fname, 16, 16, BMOFF_FURNACES + fn-1 );
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	// 12 assemblers each with 3 images for animation
	for (int fn=1; fn<=NUM_BM_ASSEMBLERS; fn++)
	{
		sprintf(fname, FN_ASSEMBLERS, fn);
		int ret = load_bitmap_file(fname, 16, 16, BMOFF_ASSEMBLERS + fn-1 );
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	
	for (int fn=1; fn<=NUM_BM_PROD8; fn++)
	{
		sprintf(fname, FN_PROD8, fn);
		int ret = load_bitmap_file(fname, 8, 8, BMOFF_PROD8 + fn-1 );
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}

	// 12 inserters each with 3 image anim
	for (int fn=1; fn<=NUM_BM_INSERTERS; fn++)
	{
		sprintf(fname, FN_INSERTERS, fn);
		int ret = load_bitmap_file(fname, 16, 16, BMOFF_INSERTERS + fn-1 );
		if ( ret < 0 ) return false;
		if (progress) update_bar(progbar, cnt++);
	}
	int ret = load_bitmap_file(FN_ZAP, 8, 8, BMOFF_ZAP );
	if ( ret < 0 ) return false;

#if 0
	printf("TILES start %d count %d\n",BMOFF_TERR16,NUM_BM_TERR16);
	printf("FEATS start %d count %d\n",BMOFF_FEAT16,NUM_BM_FEAT16);
	printf("BOB   start %d count %d\n",BMOFF_BOB16,NUM_BM_BOB16);
	printf("BELTS start %d count %d + %d\n",BMOFF_BELT16,NUM_BM_BELT16,NUM_BM_BBELT16);
	printf("ITEMS start %d count %d\n",BMOFF_ITEM8,NUM_BM_ITEM8);
	printf("MACHS start %d count %d\n",BMOFF_MACH16,NUM_BM_MACH16);
	printf("NUMS  start %d count %d\n",BMOFF_NUMS,NUM_BM_NUMS);
	printf("CURS  start %d count %d\n",BMOFF_CURSORS,NUM_BM_CURSORS);
	printf("MINER start %d count %d\n",BMOFF_MINERS,NUM_BM_MINERS);
	printf("FURNA start %d count %d\n",BMOFF_FURNACES,NUM_BM_FURNACES);
	printf("ASSMB start %d count %d\n",BMOFF_ASSEMBLERS,NUM_BM_ASSEMBLERS);
	printf("PROD  start %d count %d\n",BMOFF_PROD8,NUM_BM_PROD8);
	printf("INSER start %d count %d\n",BMOFF_INSERTERS,NUM_BM_INSERTERS);
	printf("Total %d\n",TOTAL_BM);
	wait_for_any_key();
#endif

	return true;
}

// Create sprites for Bob moving in each direction with 4 frames each
void create_sprites() 
{
	vdp_cls();
	TAB(10,5);printf("Creating Sprites");
	vdp_adv_create_sprite( BOB_SPRITE_DOWN, BMOFF_BOB16 + BOB_SPRITE_DOWN*4, 4 );
	vdp_adv_create_sprite( BOB_SPRITE_UP, BMOFF_BOB16 + BOB_SPRITE_UP*4, 4 );
	vdp_adv_create_sprite( BOB_SPRITE_LEFT, BMOFF_BOB16 + BOB_SPRITE_LEFT*4, 4 );
	vdp_adv_create_sprite( BOB_SPRITE_RIGHT, BMOFF_BOB16 + BOB_SPRITE_RIGHT*4, 4 );

	vdp_adv_create_sprite( BOB_SPRITE_ACT_DOWN, BMOFF_BOB16 + 16 + BOB_SPRITE_DOWN*2, 2 );
	vdp_adv_create_sprite( BOB_SPRITE_ACT_UP, BMOFF_BOB16 + 16 + BOB_SPRITE_UP*2, 2 );
	vdp_adv_create_sprite( BOB_SPRITE_ACT_LEFT, BMOFF_BOB16 + 16 + BOB_SPRITE_LEFT*2, 2 );
	vdp_adv_create_sprite( BOB_SPRITE_ACT_RIGHT, BMOFF_BOB16 + 16 + BOB_SPRITE_RIGHT*2, 2 );

	vdp_adv_create_sprite( CURSOR_SPRITE, BMOFF_CURSORS, 2 );

	vdp_activate_sprites( NUM_BOB_SPRITES + 1 );

	for (int s=0; s<NUM_BOB_SPRITES; s++)
	{
		vdp_select_sprite( s );
		vdp_hide_sprite();
	}
	vdp_select_sprite( CURSOR_SPRITE );
	vdp_show_sprite();
	vdp_refresh_sprites();

}
int get_current_sprite()
{
	return current_bob_sprite;
}

void select_bob_sprite( int sprite )
{
	if ( sprite >= NUM_BOB_SPRITES ) return;
	if ( sprite != current_bob_sprite )
	{
		current_bob_sprite = sprite;
		for (int s=0; s < NUM_BOB_SPRITES; s++)
		{
			vdp_select_sprite(s);
			if (s == current_bob_sprite)
			{
				vdp_show_sprite();
			} 
			else 
			{
				vdp_hide_sprite();
			}
		}
	}
	vdp_select_sprite(current_bob_sprite);
	vdp_show_sprite();
	vdp_refresh_sprites();
}

void hide_bob( )
{
	for (int s=0; s<NUM_BOB_SPRITES; s++)
	{
		vdp_select_sprite( s );
		vdp_hide_sprite();
	}
	vdp_refresh_sprites();
}

void show_bob( )
{
	vdp_select_sprite( current_bob_sprite );
	vdp_show_sprite();
	vdp_refresh_sprites();
}

#endif
