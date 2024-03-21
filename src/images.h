#ifndef _IMAGE_H
#define _IMAGE_H

#define FN_TERR16 "img/terr16/tr%02d.rgb2"
#define BMOFF_TERR16 0
#define NUM_BM_TERR16 13

#define FN_FEAT16 "img/tf16/tf%02d.rgb2"
#define BMOFF_FEAT16 BMOFF_TERR16 + NUM_BM_TERR16 
#define NUM_BM_FEAT16 15

#define FN_BOB16  "img/b16/bob%02d.rgb2"
#define BMOFF_BOB16 BMOFF_FEAT16 + NUM_BM_FEAT16
#define NUM_BM_BOB16 16

#define FN_BELT16 "img/belt16/belt%02d.rgb2"
#define FN_BBELT16 "img/belt16/bbelt%02d.rgb2"
#define BMOFF_BELT16 BMOFF_BOB16 + NUM_BM_BOB16
#define NUM_BM_BELT16 4*4
#define NUM_BM_BBELT16 8*4

#define FN_ITEM8  "img/ti8/ti%02d.rgb2"
#define BMOFF_ITEM8 BMOFF_BELT16 + NUM_BM_BELT16 + NUM_BM_BBELT16
#define NUM_BM_ITEM8 8

#define FN_MACH16 "img/tm16/tm%02d.rgb2"
#define BMOFF_MACH16 BMOFF_ITEM8 + NUM_BM_ITEM8 
#define NUM_BM_MACH16 4

#define FN_NUMS "img/nums4x5/num%01d.rgb2"
#define BMOFF_NUMS BMOFF_MACH16 + NUM_BM_MACH16 
#define NUM_BM_NUMS 10

#define FN_CURSORS "img/cursor%02d.rgb2"
#define BMOFF_CURSORS BMOFF_NUMS + NUM_BM_NUMS 
#define NUM_BM_CURSORS 2

#define TOTAL_BM BMOFF_CURSORS + NUM_BM_CURSORS

#define BOB_SPRITE_DOWN 0
#define BOB_SPRITE_UP 1
#define BOB_SPRITE_LEFT 2
#define BOB_SPRITE_RIGHT 3

#define NUM_BOB_SPRITES 4

#define CURSOR_SPRITE 4

void load_images();
void create_sprites();
int get_current_sprite();
void select_bob_sprite( int sprite );

static int current_bob_sprite = -1;

#endif

#ifdef _IMAGE_IMPLEMENTATION
void load_images() 
{
	//TAB(0,0);
	char fname[40];
	for (int fn=1; fn<=NUM_BM_TERR16; fn++)
	{
		sprintf(fname, FN_TERR16, fn);
		load_bitmap_file(fname, 16, 16, BMOFF_TERR16 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_FEAT16; fn++)
	{
		sprintf(fname, FN_FEAT16, fn);
		load_bitmap_file(fname, 16, 16, BMOFF_FEAT16 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_BOB16; fn++)
	{
		sprintf(fname, FN_BOB16, fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BOB16 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_BELT16; fn++)
	{
		sprintf(fname, FN_BELT16, fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BELT16 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_BBELT16; fn++)
	{
		sprintf(fname, FN_BBELT16, fn);
		load_bitmap_file(fname, 16, 16, BMOFF_BELT16 + NUM_BM_BELT16 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_ITEM8; fn++)
	{
		sprintf(fname, FN_ITEM8, fn);
		load_bitmap_file(fname, 8, 8, BMOFF_ITEM8 + fn-1);
	}
	for (int fn=1; fn<=NUM_BM_MACH16; fn++)
	{
		sprintf(fname, FN_MACH16, fn);
		load_bitmap_file(fname, 16, 16, BMOFF_MACH16 + fn-1);
	}
	for (int fn=0; fn<=NUM_BM_NUMS-1; fn++)
	{
		sprintf(fname, FN_NUMS, fn);
		load_bitmap_file(fname, 4, 5, BMOFF_NUMS + fn);
	}
	
	for (int fn=1; fn<=NUM_BM_CURSORS; fn++)
	{
		sprintf(fname, FN_CURSORS, fn);
		load_bitmap_file(fname, 16, 16, BMOFF_CURSORS + fn-1);
	}
	
	/*
	printf("TILES start %d count %d\n",BMOFF_TERR16,NUM_BM_TERR16);
	printf("FEATS start %d count %d\n",BMOFF_FEAT16,NUM_BM_FEAT16);
	printf("BOB   start %d count %d\n",BMOFF_BOB16,NUM_BM_BOB16);
	printf("BELTS start %d count %d + %d\n",BMOFF_BELT16,NUM_BM_BELT16,NUM_BM_BBELT16);
	printf("ITEMS start %d count %d\n",BMOFF_ITEM8,NUM_BM_ITEM8);
	printf("MACHS start %d count %d\n",BMOFF_MACH16,NUM_BM_MACH16);
	printf("NUMS  start %d count %d\n",BMOFF_NUMS,NUM_BM_NUMS);
	printf("CURS  start %d count %d\n",BMOFF_CURSORS,NUM_BM_CURSORS);
	printf("Total %d\n",TOTAL_BM);
	*/
}

// Create sprites for Bob moving in each direction with 4 frames each
void create_sprites() 
{
	vdp_adv_create_sprite( BOB_SPRITE_DOWN, BMOFF_BOB16 + BOB_SPRITE_DOWN*4, 4 );
	vdp_adv_create_sprite( BOB_SPRITE_UP, BMOFF_BOB16 + BOB_SPRITE_UP*4, 4 );
	vdp_adv_create_sprite( BOB_SPRITE_LEFT, BMOFF_BOB16 + BOB_SPRITE_LEFT*4, 4 );
	vdp_adv_create_sprite( BOB_SPRITE_RIGHT, BMOFF_BOB16 + BOB_SPRITE_RIGHT*4, 4 );
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
	vdp_refresh_sprites();
}

#endif
