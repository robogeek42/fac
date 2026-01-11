/*
  vim:ts=4
  vim:sw=4
*/
#include "colmap.h"
#include "belt.h"

#include "agon/vdp_vdu.h"
#include "agon/vdp_key.h"
#include <mos_api.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include "util.h"

extern uint8_t key_pressed_code;

#include "item.h"
#include "thinglist.h"

#define _IMAGE_IMPLEMENTATION
#include "images.h"
#define _HUD_IMPLEMENTATION
#include "hud.h"

int gMode = 8; 
int gScreenWidth = 320;
int gScreenHeight = 240;

bool debug = false;

static volatile SYSVAR *sys_vars = NULL;

//------------------------------------------------------------
// Function defs
//------------------------------------------------------------
int load_sound_sample(char *fname, int sample_id);
bool load_sound_samples(int vert_pos);
int choose();
int reload_graphics();
void display_bitmaps(int id, int y);
int create_file(char *fname);

void wait()
{
	char k=getchar();
	if (k=='q') exit(0);
}

void change_mode(int mode)
{
	sys_vars->vdp_pflags = 0;
	vdp_mode(mode);
	while ( !(sys_vars->vdp_pflags & vdp_pflag_mode) );
}

bool bSoundEnabled = false;
bool bSoundSamplesLoaded = false;
int sound_volume = 40; // 0-127
bool bPlayingWalkSound = false;
#define SOUND_CHAN_STEPS 1
#define SOUND_CHAN_PICKAXE 2

bool bIsTest;

//------------------------------------------------------------
// MAIN
//------------------------------------------------------------
int main(int argc, char *argv[])
{
	vdp_vdu_init();
	sys_vars = (SYSVAR *)mos_sysvars();

	if ( vdp_key_init() == -1 ) return 1;
	vdp_set_key_event_handler( key_event_handler );

	bIsTest = false;
	if (argc > 1)
	{
		if (strcmp("test",argv[1]) == 0)
		{
			bIsTest = true;
		}
	}

	change_mode(gMode);
	vdp_cursor_enable( false );
	vdp_logical_scr_dims( false );

    int choice;

    do {
        vdp_cls();
        display_bitmaps(BMOFF_FEAT16, 8);
        choice = choose();
    } while (choice == 1);

	vdp_logical_scr_dims( true );
	vdp_cursor_enable( true );
	return 0;
}

bool load_sound_samples(int vert_pos)
{
	PROGBAR *progbar;
	int cnt=1;
	int prog_max = 2;

	TAB(10,vert_pos);printf("Loading Sound Samples");
	progbar = init_horiz_bar(10,8*(vert_pos+2),300,24,0,prog_max,1,3);

	update_bar(progbar, 0);

	int sample_len = load_sound_sample("sounds/steps.raw",-2);
	if (sample_len == 0 )
	{
		return false;
	}
	update_bar(progbar, ++cnt);

	vdp_audio_set_sample( SOUND_CHAN_STEPS, 64257 );
	vdp_audio_play_note( SOUND_CHAN_STEPS, sound_volume, 435, -1 ); // channel 1, loop
	vdp_audio_set_volume( SOUND_CHAN_STEPS, 0 );
	bPlayingWalkSound = false;

	sample_len = load_sound_sample("sounds/pick-axe2.pcm",-3);
	if (sample_len == 0 )
	{
		return false;
	}
	update_bar(progbar, ++cnt);

	vdp_audio_set_sample( SOUND_CHAN_PICKAXE, 64258 );
	vdp_audio_play_note( SOUND_CHAN_PICKAXE, sound_volume, 435, -1 ); // channel 2, loop
	vdp_audio_set_volume( SOUND_CHAN_PICKAXE, 0 );

	delete_bar(&progbar);

	return true;
}
// returns length of sound sample
int load_sound_sample(char *fname, int sample_id)
{
	unsigned int file_len = 0;
	FILE *fp;
	fp = fopen(fname, "rb");
	if ( !fp )
	{
		printf("Fail to Open %s\n",fname);
		return 0;
	}
	// get length of file
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	uint8_t *data = (uint8_t*) malloc(file_len);
	if ( !data )
	{
		fclose(fp);
		printf("Mem alloc error\n");
		return 0;
	}
	unsigned int bytes_read = fread( data, 1, file_len, fp );
	if ( bytes_read != file_len )
	{
		fclose(fp);
		printf("Err read %d bytes expected %d\n", bytes_read, file_len);
		return 0;
	}

	fclose(fp);

	if (file_len)
	{
		vdp_audio_load_sample(sample_id, file_len, data);
	}

	return file_len;
}

int choose()
{
	int choice = -1;
    int ret = 0;
	while (choice < 0 || choice > 3)
	{
        TAB(2,4);printf("0 Exit");
		TAB(2,5);printf("1 Reload graphics");
		TAB(2,6);printf("2 Start FAC game");
		TAB(2,7);printf("3 Run Editor");
		choice = input_int_noclear(2,9,"Enter choice 0-3");
	}
	switch (choice)
	{
		case 0:
		default:
            printf("Bye!\n");
			break;
		case 1:
            vdp_cls();
            ret = reload_graphics();
            if (ret < 0) return ret;
			break;
		case 2:
            create_file("lfac");
			break;
		case 3:
            create_file("lfed");
			break;
    }
    return choice;
}

int reload_graphics()
{
	// load all the bitmaps and sprites
	if ( ! load_images(true, 2) )
	{
		printf("Failed to load images\n");
        return -1;
	}

	// sound samples
	bSoundSamplesLoaded = load_sound_samples( 8 );
	if (!bSoundSamplesLoaded)
	{
		printf("\nFailed to load sound samples\n");
	} else {
		bSoundEnabled = true;
	}

	create_sprites(16);

	create_hud(19);
    return 0;
}

void display_bitmaps(int id, int y)
{
    for (int i=0; i<16; i++)
    {
        vdp_adv_select_bitmap(id + i);
        vdp_draw_bitmap( i*16, y);
    }
}

int create_file(char *fname)
{
    FILE *fp;
	if ( !(fp = fopen( fname, "wb" ) ) ) {
		printf( "Error opening file \"%s\"a\n.", fname );
		return -1;
	}
	fwrite( "FAC", sizeof(char), 4, fp);
	fclose(fp);
    return 0;
}
