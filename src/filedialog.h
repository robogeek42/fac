/*
  vim:ts=4
  vim:sw=4
*/
#ifndef __FILEDIALOG_H
#define __FILEDIALOG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

bool file_dialog( char *dir, char *filenamestr, int maxstr, bool *isload );

#endif

#ifdef _FILEDIALOG_IMPLEMENTATION

#include "listdir.h"
#include "globals.h"

#define MENU_TEXT_COL 2
#define MENU_HILI_COL 5

#define MAX_FILENAME_LENGTH 160
#define MAX_OUTFILE_LENGTH 120

static SmallFilInfo *sfinfos = NULL;
int fno_num = 0;
char curr_file[MAX_FILENAME_LENGTH+2];
char linestr[MAX_FILENAME_LENGTH+2];
char spaces[MAX_FILENAME_LENGTH]; 
bool curr_file_set;
int scrwidth_text=0;
int scrheight_text=0;
int scrwidth_pix=0;
int scrheight_pix=0;
int menux = 0;
int dir_line = 2;
int files_start_col = 1;
int files_start_row = 0;
int files_width_chars = 0;
int files_height_chars = 0;
int file_line = 0;
int selected_line = 0;
int selected_fno = 0;
int selected_fno_start = 0;
int max_linestr = 0;

int key_wait_time = 20;
int key_scroll_wait_time = 10;

void print_key(int x, int y, char *pre, char *key, char* post)
{
	vdp_set_text_colour(MENU_TEXT_COL);
	TAB(x,y); printf("%s",pre);
	vdp_set_text_colour(MENU_HILI_COL);
	printf("%s",key);
	vdp_set_text_colour(MENU_TEXT_COL);
	printf("%s",post);
}

void clear_partline( int row, int fromx, int tox )
{
	TAB(fromx,row);
	for (int i=fromx; i<tox ; i++)
	{
		printf(" ");
	}
}
void clear_files_area()
{
	vdp_gcol(0,15);
	draw_filled_box(files_start_col*8-4, files_start_row*8-2,
			 files_width_chars*8+12, files_height_chars*8+12, 14,16); // box for filelist
}
void show_files(int *line, int select_offset, int fno_start )
{
	if ( !sfinfos || fno_num == 0 ) 
	{
		clear_files_area();
		if ( !sfinfos ) {
			TAB(1,files_start_row);printf("No such directory");
		}
		return;
	}

	curr_file_set = false;

	clear_files_area();

	for (int i=fno_start; i<fno_num; i++)
	{
		int bgcol = 128+16;
		if ( (*line) > files_start_row+files_height_chars ) break;
		if ( i == select_offset ) 
		{
			bgcol = 128+8;
			if (sfinfos[i].fattrib & 0x10)  // directory
			{
				sprintf(curr_file, "");
				curr_file_set = false;
			} else {
				sprintf(curr_file, "%s", sfinfos[i].fname);
				curr_file_set = true;
			}
		}
		COL(15);COL(bgcol);

		if (sfinfos[i].fattrib & 0x10)  // directory
		{
			COL(10); // green
			snprintf(linestr, max_linestr, "<%s>%s", sfinfos[i].fname, spaces);
		} else {
			COL(15); // white
			snprintf(linestr, max_linestr, "%s%s", sfinfos[i].fname, spaces);
		}
		linestr[MAX_FILENAME_LENGTH-1] = '\0';
		TAB(files_start_col, (*line)++);
		printf("%s", linestr);
	}          
	snprintf(linestr, max_linestr, "%s%s", curr_file, spaces);
	TAB( 1, file_line);
	printf("%s", linestr);
}

bool file_dialog( char *dir, char *filenamestr, int maxstr, bool *isload )
{
	bool ret = false;

	int line = 0;
	char curr_directory[254+1];
	char outfilename[MAX_OUTFILE_LENGTH+1];

	if ( !dir || !filenamestr || !isload) return false;

	// place everything on the screen according to mode
	scrwidth_text = getsysvar_scrCols();
	scrheight_text = getsysvar_scrRows();
	scrwidth_pix = getsysvar_scrwidth();
	scrheight_pix = getsysvar_scrheight();

	menux = scrwidth_text - 24;
	files_start_row = dir_line + 2;
	files_width_chars = scrwidth_text - 4;
	files_height_chars = scrheight_text - 10;
	file_line = scrheight_text - 4;

	selected_line = 0; // selected screen line in directory list
	selected_fno = 0;
	selected_fno_start = 0;

	for(int i=0;i<MAX_FILENAME_LENGTH;i++) spaces[i]=' '; 
	spaces[MAX_FILENAME_LENGTH-1]=0;
	max_linestr = files_width_chars;

	snprintf(curr_directory, 254, "%s", dir);

	vdp_cls();

	// title line and menu
	COL(15);COL(128+16);
	TAB(0,line);printf("FILE DIALOG");
	print_key(menux, line, "ch","D","ir");
	print_key(menux+6, line, "","U","p");
	print_key(menux+9,line,"","L","oad");
	print_key(menux+14,line,"","S","ave");
	print_key(menux+19,line,"e","X","it");

	vdp_move_to(0,1*8+2);vdp_line_to(scrwidth_pix,1*8+2); // line under title/menu
	draw_box(4,dir_line*8-2, scrwidth_pix-18, 12, 11); // box for dir

	// Draw boxes for file list and selected file
	clear_files_area();

	draw_box(4,file_line*8-2, scrwidth_pix-18, 12, 11); // box for file selected
													  
	// game loop for interacting with file dialog
	clock_t key_wait_ticks = clock() + key_wait_time;
	bool dir_changed = true;
	bool finish = false;
	bool bsave = false;
	bool bload = false;

	do {

		if ( dir_changed )
		{
			clear_partline( dir_line, 1, scrwidth_text-2 );
			TAB(1,dir_line);printf("%s",curr_directory);
			dir_changed = false;

			// get directory contents
			if ( sfinfos != NULL ) 
			{
				free_fnos( &sfinfos, fno_num);
			}
			sfinfos = get_files( curr_directory, &fno_num );
			
			// display directory contents
			line = files_start_row;
			selected_fno = 0;
			selected_line = 0;
			selected_fno_start = 0;
			TAB(files_start_col, files_start_row);
			show_files( &line, selected_fno, selected_fno_start );
			
		}
		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_DOWN ) )
		{
			key_wait_ticks = clock() + key_scroll_wait_time;
			bool refresh = false;
			if ( sfinfos )
			{
				if ( selected_line < files_height_chars -1 &&
					 selected_line < fno_num - selected_fno_start - 1 )
				{
					selected_line++;
					selected_fno++;
					refresh = true;
				} 
				else if ( selected_fno < fno_num &&
					   selected_line < fno_num - selected_fno_start - 1	)
				{
					selected_fno_start++;
					selected_fno++;
					refresh = true;
				}
				if ( refresh )
				{
					// display directory contents
					line = files_start_row;
					TAB(files_start_col, files_start_row);
					show_files( &line, selected_fno, selected_fno_start );
				}

			}
		}

		if ( key_wait_ticks < clock() &&vdp_check_key_press( KEY_UP ) )
		{
			key_wait_ticks = clock() + key_scroll_wait_time;

			bool refresh = false;
			if ( sfinfos )
			{
				if ( selected_line > 0 ) 
				{
					selected_line--;
					selected_fno--;
					refresh = true;
				} else if ( selected_fno_start > 0 ) {
					selected_fno_start--;
					selected_fno--;
					refresh = true;
				}

				if ( refresh )
				{
					// display directory contents
					line = files_start_row;
					TAB(files_start_col, files_start_row);
					show_files( &line, selected_fno, selected_fno_start );
				}
			}
		}

		if ( vdp_check_key_press( KEY_x ) )
		{
			while ( vdp_check_key_press( KEY_x ) ) vdp_update_key_state();
			finish=true;
		}

		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_s ) )
		{
			char buffer[255];
			key_wait_ticks = clock() + key_wait_time;
			// save
			clear_partline( file_line, 1, scrwidth_text-2 );
			input_string_noclear(1,file_line,"Filename",buffer,MAX_FILENAME_LENGTH);
			// remove return
			buffer[strlen(buffer)-1] = '\0';
			snprintf( outfilename, MAX_OUTFILE_LENGTH, "%s/%s", curr_directory, buffer );
			bsave=true;
			finish = true;
		}
		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_l ) )
		{
			key_wait_ticks = clock() + key_wait_time;
			// load
			if ( curr_file_set )
			{
				bload = true;
				snprintf(outfilename, MAX_OUTFILE_LENGTH, "%s/%s", curr_directory, curr_file);
				finish = true;
			}
			else printf("error");
		}
		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_enter ) )
		{
			key_wait_ticks = clock() + key_wait_time;
			if ( sfinfos && fno_num )
			{
				if ( sfinfos[selected_fno].fattrib & 0x10 )
				{
					int l = strlen(curr_directory);
					int rem = 254-l;
					if ( curr_directory[l-1]=='/' )
					{
						snprintf(curr_directory+l, rem, "%s", sfinfos[selected_fno].fname);
					} else {
						snprintf(curr_directory+l, rem, "/%s", sfinfos[selected_fno].fname);
					}

					dir_changed = true;
				}
			}
			else printf("error");
		}
		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_d ) )
		{
			key_wait_ticks = clock() + key_wait_time;
			clear_partline( dir_line, 1, scrwidth_text-2 );
			input_string_noclear(1,dir_line,"Dir",curr_directory,MAX_FILENAME_LENGTH);
			// remove return
			curr_directory[strlen(curr_directory)-1] = '\0';
			dir_changed = true;
			key_wait_ticks = clock() + key_wait_time;
		}

		if ( key_wait_ticks < clock() && vdp_check_key_press( KEY_u ) )
		{
			key_wait_ticks = clock() + key_wait_time;

			//if ( strcmp("/",curr_directory) != 0 )
			{
				// shorten directory at last '/'
				int len=strlen(curr_directory);
			   	for (int i=len-1;i>=0;i--)
				{
					if ( curr_directory[i] == '/' )
					{
						if ( i==0 )
						{
							curr_directory[1] = '\0';
						} else {
							curr_directory[i] = '\0';
						}
						break;
					}
				}
				dir_changed = true;
			}
		}

		vdp_update_key_state();
	} while ( !finish );
	
	if ( bsave || bload )
	{
		ret = true;
		strncpy( filenamestr, outfilename, maxstr );
		(*isload) = bload;
	}

	if ( sfinfos ) free_fnos( &sfinfos, fno_num);
	sfinfos = NULL;

	return ret;
}
#endif

