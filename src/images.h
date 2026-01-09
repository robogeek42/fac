#ifndef _IMAGE_H
#define _IMAGE_H

#include "progbar.h"

#define FNCC_TERR16 "img/tr_concat01-16.rgb2"
#define NUM_BM_TERR16 16
#define FNCC_FEAT16 "img/tf_concat01-15.rgb2"
#define NUM_BM_FEAT16 15
#define FNCC_BOB16  "img/fb_concat01-24.rgb2"
#define NUM_BM_BOB16 24
#define FNCC_BELT16 "img/belt_concat01-16.rgb2"
#define NUM_BM_BELT16 4*4
#define FNCC_BBELT16 "img/bbelt_concat01-32.rgb2"
#define NUM_BM_BBELT16 8*4
#define FNCC_ITEM8  "img/ti_concat01-08.rgb2"
#define NUM_BM_ITEM8 8
#define FNCC_MACH16 "img/tm_concat01-08.rgb2"
#define NUM_BM_MACH16 8
#define FNCC_NUMS "img/nums4x5_concat0-9.rgb2"
#define NUM_BM_NUMS 10
#define FNCC_CURSORS "img/cursor_concat01-04.rgb2"
#define NUM_BM_CURSORS 4
#define FNCC_MINERS "img/miners_concat01-12.rgb2"
#define NUM_BM_MINERS 12
#define FNCC_FURNACES "img/fur_concat01-12.rgb2"
#define NUM_BM_FURNACES 12
#define FNCC_ASSEMBLERS "img/asmb_concat01-12.rgb2"
#define NUM_BM_ASSEMBLERS 12
#define FNCC_PROD8 "img/tp_concat01-05.rgb2"
#define NUM_BM_PROD8 5
#define FNCC_INSERTERS "img/ins_concat01-12.rgb2"
#define NUM_BM_INSERTERS 12
#define FNCC_MACH_MINI "img/mini_concat01-08.rgb2"
#define NUM_BM_MACH_MINI 8
#define FNCC_TSPLIT "img/split_concat01-08.rgb2"
#define NUM_BM_TSPLIT 8
#define FNCC_TSPLIT_ICON "img/split_icon_concat01-04.rgb2"
#define NUM_BM_TSPLIT_ICON 4

#define FN_BELT_MINI "img/bmini.rgb2"
#define NUM_BM_BELT_MINI 1
#define FN_ZAP "img/zap8x8.rgb2"
#define NUM_BM_ZAP 1


#define BMOFF_TERR16 0
#define BMOFF_FEAT16 ( BMOFF_TERR16 + NUM_BM_TERR16 )
#define BMOFF_BOB16  ( BMOFF_FEAT16 + NUM_BM_FEAT16 )
#define BMOFF_BELT16 ( BMOFF_BOB16 + NUM_BM_BOB16)
#define BMOFF_ITEM8 ( BMOFF_BELT16 + NUM_BM_BELT16 + NUM_BM_BBELT16)
#define BMOFF_MACH16 ( BMOFF_ITEM8 + NUM_BM_ITEM8 )
#define BMOFF_NUMS ( BMOFF_MACH16 + NUM_BM_MACH16 )
#define BMOFF_CURSORS ( BMOFF_NUMS + NUM_BM_NUMS )
#define BMOFF_MINERS ( BMOFF_CURSORS + NUM_BM_CURSORS )
#define BMOFF_FURNACES ( BMOFF_MINERS + NUM_BM_MINERS )
#define BMOFF_ASSEMBLERS ( BMOFF_FURNACES + NUM_BM_FURNACES )
#define BMOFF_PROD8 ( BMOFF_ASSEMBLERS + NUM_BM_ASSEMBLERS)
#define BMOFF_INSERTERS ( BMOFF_PROD8 + NUM_BM_PROD8 )
#define BMOFF_MACH_MINI ( BMOFF_INSERTERS + NUM_BM_INSERTERS)
#define BMOFF_TSPLIT ( BMOFF_MACH_MINI + NUM_BM_MACH_MINI )
#define BMOFF_TSPLIT_ICON ( BMOFF_TSPLIT + NUM_BM_TSPLIT )
#define BMOFF_BELT_MINI ( BMOFF_TSPLIT_ICON + NUM_BM_TSPLIT_ICON )
#define BMOFF_ZAP ( BMOFF_BELT_MINI + NUM_BM_BELT_MINI )

#define TOTAL_BM ( BMOFF_ZAP + NUM_BM_ZAP )


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
#define HUD_SPRITE 9

#define NUM_SPRITES ( HUD_SPRITE + 1 )

bool load_images(bool progress, int vert_pos);
void create_sprites(int vert_pos);
int get_current_sprite();
void select_bob_sprite( int sprite );
void show_bob();
void hide_bob();

static int current_bob_sprite = -1;

#endif

#ifdef _IMAGE_IMPLEMENTATION
bool load_images(bool progress, int vert_pos) 
{
	PROGBAR *progbar;
	int cnt=1;
	int prog_max = TOTAL_BM;
        int ret = 0;

	if (progress)
	{
		TAB(10,vert_pos);printf("Loading %d Images", TOTAL_BM);
		progbar = init_horiz_bar(10,8*(vert_pos+2),300,24,0,prog_max,1,3);
		
		update_bar(progbar, 0);
	}

	//TAB(0,0);

        if ( load_concat_bitmaps(FNCC_TERR16, NUM_BM_TERR16, 16, 16, BMOFF_TERR16, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_TERR16; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_FEAT16, NUM_BM_FEAT16, 16, 16, BMOFF_FEAT16, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_FEAT16; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_BOB16, NUM_BM_BOB16, 16, 16, BMOFF_BOB16, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_BOB16; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_BELT16, NUM_BM_BELT16, 16, 16, BMOFF_BELT16, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_BELT16; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_BBELT16, NUM_BM_BBELT16, 16, 16, BMOFF_BELT16 + NUM_BM_BELT16, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_BBELT16; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_ITEM8, NUM_BM_ITEM8, 8, 8, BMOFF_ITEM8, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_ITEM8; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_MACH16, NUM_BM_MACH16, 16, 16, BMOFF_MACH16, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_MACH16; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_NUMS, NUM_BM_NUMS, 4, 5, BMOFF_NUMS, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_NUMS; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_CURSORS, NUM_BM_CURSORS, 16, 16, BMOFF_CURSORS, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_CURSORS; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_MINERS, NUM_BM_MINERS, 16, 16, BMOFF_MINERS, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_MINERS; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_FURNACES, NUM_BM_FURNACES, 16, 16, BMOFF_FURNACES, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_FURNACES; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_ASSEMBLERS, NUM_BM_ASSEMBLERS, 16, 16, BMOFF_ASSEMBLERS, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_ASSEMBLERS; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_PROD8, NUM_BM_PROD8, 8, 8, BMOFF_PROD8, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_PROD8; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_INSERTERS, NUM_BM_INSERTERS, 16, 16, BMOFF_INSERTERS, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_INSERTERS; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_MACH_MINI, NUM_BM_MACH_MINI, 8, 8, BMOFF_MACH_MINI, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_MACH_MINI; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_TSPLIT, NUM_BM_TSPLIT, 16, 16, BMOFF_TSPLIT, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_TSPLIT; update_bar(progbar, cnt); };

        if ( load_concat_bitmaps(FNCC_TSPLIT_ICON, NUM_BM_TSPLIT_ICON, 16, 16, BMOFF_TSPLIT_ICON, 1000) < 0) return false;
        if (progress) { cnt+=NUM_BM_TSPLIT_ICON; update_bar(progbar, cnt); };


	ret = load_bitmap_file(FN_BELT_MINI, 8, 8, BMOFF_BELT_MINI );
	if ( ret < 0 ) return false;
	if (progress) update_bar(progbar, cnt++);

	ret = load_bitmap_file(FN_ZAP, 8, 8, BMOFF_ZAP );
	if ( ret < 0 ) return false;
	if (progress) update_bar(progbar, cnt++);

	delete_bar(&progbar);

	return true;
}

// Create sprites for Bob moving in each direction with 4 frames each
void create_sprites(int vert_pos) 
{
	TAB(10,vert_pos);printf("Creating Sprites");
	vdp_adv_create_sprite( BOB_SPRITE_DOWN, BMOFF_BOB16 + BOB_SPRITE_DOWN*4, 4 );
	vdp_adv_create_sprite( BOB_SPRITE_UP, BMOFF_BOB16 + BOB_SPRITE_UP*4, 4 );
	vdp_adv_create_sprite( BOB_SPRITE_LEFT, BMOFF_BOB16 + BOB_SPRITE_LEFT*4, 4 );
	vdp_adv_create_sprite( BOB_SPRITE_RIGHT, BMOFF_BOB16 + BOB_SPRITE_RIGHT*4, 4 );

	vdp_adv_create_sprite( BOB_SPRITE_ACT_DOWN, BMOFF_BOB16 + 16 + BOB_SPRITE_DOWN*2, 2 );
	vdp_adv_create_sprite( BOB_SPRITE_ACT_UP, BMOFF_BOB16 + 16 + BOB_SPRITE_UP*2, 2 );
	vdp_adv_create_sprite( BOB_SPRITE_ACT_LEFT, BMOFF_BOB16 + 16 + BOB_SPRITE_LEFT*2, 2 );
	vdp_adv_create_sprite( BOB_SPRITE_ACT_RIGHT, BMOFF_BOB16 + 16 + BOB_SPRITE_RIGHT*2, 2 );

	vdp_adv_create_sprite( CURSOR_SPRITE, BMOFF_CURSORS, 4 );

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
