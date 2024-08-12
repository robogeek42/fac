/*
vim: ts=4
vim: sw=4
*/
#ifndef _HUD_H
#define _HUD_H

typedef struct {
	int w;
	int h;
	int bmID;
	int spriteID;
	int bgcol;
} Hud;

#define BMOFF_HUD 500
#define BMOFF_HUD_IMAGES 501
#define BMOFF_HUD_NUMS 520

#define PUTW(w) putch((w)&0xFF); putch((w)>>8);

#define BUFF_OP_NOT 0
#define BUFF_OP_NEG 1
#define BUFF_OP_SET 2
#define BUFF_OP_ADD 3
#define BUFF_OP_ADDC 4
#define BUFF_OP_AND 5
#define BUFF_OP_OR 6
#define BUFF_OP_XOR 7

typedef struct {
	uint8_t A;
	uint8_t B;
	uint8_t C;
	uint16_t BID;
	uint8_t CMD;
	uint8_t op;
	uint16_t w0;
	uint16_t w1;
	uint16_t w2;
	uint16_t w3;
} VDU_ADV_CMD_B_W_W_W_W;

#endif

//-----------------------------------------------------------------

#ifdef _HUD_IMPLEMENTATION

Hud hud;

int load_hud_bitmap_file( const char *fname, int width, int height, int bmap_id, int bgcol )
{
	FILE *fp = NULL;
	char *buffer;
	int bytes_remain = width * height;

	if ( !(buffer = (char *)malloc( CHUNK_SIZE ) ) ) {
		printf( "Failed to allocate %d bytes for buffer.\n",CHUNK_SIZE );
		return -1;
	}

	fp = fopen( fname, "rb" );
	if ( fp == NULL )  {
		printf( "Error opening file \"%s\". Quitting.\n", fname );
		getch();
		return -1;
	}

	vdp_adv_clear_buffer(bmap_id);

	bytes_remain = width * height;

	while (bytes_remain > 0)
	{
		int size = (bytes_remain>CHUNK_SIZE)?CHUNK_SIZE:bytes_remain;

		vdp_adv_write_block(bmap_id, size);

		if ( fread( buffer, 1, size, fp ) != (size_t)size ) return -1;

		// make transparent colour all HUD bg colour
		for (int i=0; i<size; i++)
		{
			if (buffer[i]==0x33) buffer[i]=bgcol;
		}

		mos_puts( buffer, size, 0 );
		//printf(".");

		bytes_remain -= size;
	}
	vdp_adv_consolidate(bmap_id);

	vdp_adv_select_bitmap(bmap_id);
	vdp_adv_bitmap_from_buffer(width, height, 1); // RGBA2
	
	fclose( fp );
	free( buffer );

	return bmap_id;
}

void load_hud_images()
{
	char fname[50];
	for (int fn=0; fn<10; fn++)
	{
		sprintf(fname, "img/nums4x5/num%01d.rgb2",fn);
		load_hud_bitmap_file(fname, 4,5, BMOFF_HUD_NUMS + fn, hud.bgcol);
	}
	sprintf(fname, "img/nums4x5/num_space.rgb2");
	load_hud_bitmap_file(fname, 4,5, BMOFF_HUD_NUMS + 10, hud.bgcol);
	sprintf(fname, "img/nums4x5/num_minus.rgb2");
	load_hud_bitmap_file(fname, 4,5, BMOFF_HUD_NUMS + 11, hud.bgcol);

	sprintf(fname, "img/zap8x8.rgb2");
	load_hud_bitmap_file(fname, 8,8, BMOFF_HUD_IMAGES, hud.bgcol);
}

static VDU_ADV_CMD_B_W_W_W_W vdu_adv_copy_from_buffer = { 23, 0, 0xA0, 0xFA00, 5, 0, 0, 0, 0, 0 };

void vdp_adv_copy_from_buffer(int op, int target_bid, int target_offset, int source_bid, int source_offset, int numbytes)
{
	vdu_adv_copy_from_buffer.BID = target_bid;
	vdu_adv_copy_from_buffer.op  = 0xE0 | op;
	vdu_adv_copy_from_buffer.w0  = target_offset;
	vdu_adv_copy_from_buffer.w1  = numbytes;
	vdu_adv_copy_from_buffer.w2  = source_bid;
	vdu_adv_copy_from_buffer.w3  = source_offset;
	VDP_PUTS( vdu_adv_copy_from_buffer );
}

void copy_bitmap_to_hud(int bmID, int bmw, int bmh, int x, int y)
{
	for (int j=0; j < bmh; j++)
	{
		vdp_adv_copy_from_buffer( BUFF_OP_SET,
				hud.bmID,			// target bmID is HUD
				x + (y+j) * hud.w,	// target offset
				bmID,				// source bmID 
				j * bmw,			// source offset
				bmw					// bytes to copy (one line)
		);
	}
}

void create_hud(int vert_pos)
{
	hud.w = 4*3 + 8 + 7*5;
	hud.h = 12;
	hud.bmID = BMOFF_HUD;
	hud.spriteID = HUD_SPRITE;
	hud.bgcol = 0xc4;

	TAB(10,vert_pos);printf("Creating HUD");
	load_hud_images();
	
	// buffer
	vdp_adv_clear_buffer(hud.bmID);
	vdp_adv_create(hud.bmID, hud.h * hud.w); // 1-byte-per-pixel

	vdp_adv_adjust(hud.bmID, 0x42, 0); PUTW(hud.h * hud.w); putch(hud.bgcol); // fill with bgcol
	
	vdp_adv_adjust(hud.bmID, 0x42, 0); PUTW(hud.w); putch(0xcf); // yellow line across top
	vdp_adv_adjust(hud.bmID, 0x42, (hud.h-1)*hud.w); PUTW(hud.w); putch(0xcf); // yellow line across bottom
	for (int j=0; j<hud.h; j++)
	{
		vdp_adv_adjust(hud.bmID, 0x42, j * hud.w); PUTW(1); putch(0xcf); // yellow line at left
		vdp_adv_adjust(hud.bmID, 0x42, j * hud.w + hud.w -1); PUTW(1); putch(0xcf); // yellow line at right
	}

	copy_bitmap_to_hud(BMOFF_HUD_IMAGES, 8,8, 2,2);

	// create the bitmap from the buffer
	vdp_adv_select_bitmap( hud.bmID );
	vdp_adv_bitmap_from_buffer(hud.w, hud.h, 1); // RGBA2

	// and use it in a sprite
	vdp_adv_create_sprite( hud.spriteID, hud.bmID, 1);
	//vdp_activate_sprites(1);
}

void hud_update_count(int count)
{
	char count_str[10];
	snprintf(count_str, 7, "%d  ", count );
	int x = 16;
	for (unsigned int i=0; i < strlen(count_str); i++)
	{
		if (count_str[i] == ' ')
		{
			copy_bitmap_to_hud(BMOFF_HUD_NUMS + 10, 4,5, x, 4);
		} 
		else if ( count_str[i] == '-')
		{
			copy_bitmap_to_hud(BMOFF_HUD_NUMS + 11, 4,5, x, 4);
		} 
		else {
			copy_bitmap_to_hud(BMOFF_HUD_NUMS + count_str[i]-'0', 4,5, x, 4);
		}
		x += 5;
	}
	vdp_refresh_sprites();
}

void show_hud(int x, int y)
{
	vdp_select_sprite( HUD_SPRITE );
	vdp_show_sprite();
	vdp_move_sprite_to( x, y );
	vdp_refresh_sprites();
}

void hide_hud()
{
	vdp_select_sprite( HUD_SPRITE );
	vdp_hide_sprite();
	vdp_refresh_sprites();
}

#endif
