/*
  vim:ts=4
  vim:sw=4
*/
/*----------------------------------------------------------------------------
 * Author:	Badre
 * Created:	06/04/2024
 * Modified by Assif to support a file select ui
 *----------------------------------------------------------------------------*/
#include "listdir.h"

static SmallFilInfo *fnos;
static uint16_t fno_num = 0;

int strcasecmp(const char *s1, const char *s2)
{
	const unsigned char *p1 = (const unsigned char *)s1;
	const unsigned char *p2 = (const unsigned char *)s2;
	int result;

	if (p1 == p2)
		return 0;

	while ((result = tolower (*p1) - tolower (*p2++)) == 0)
		if (*p1++ == '\0')
			break;
	return result;
}

static int cmp_filinfo(const SmallFilInfo* a, const SmallFilInfo* b) {
    if ((a->fattrib & AM_DIR) == (b->fattrib & AM_DIR)) {
        return strcasecmp(a->fname, b->fname);
    } else if (a->fattrib & AM_DIR) {
        return -1;
    } else {
        return 1;
    }
}

uint24_t get_num_dirents(const char* path, int* cnt) {
    FRESULT        fr;
    DIR            dir;
    static FILINFO fno;

    *cnt = 0;
    fr = ffs_dopen(&dir, path);

    if (fr == FR_OK) {
        for (;;) {
            fr = ffs_dread(&dir, &fno);
            if (fr != FR_OK || fno.fname[0] == 0) {
                if (*cnt == 0 && fr == FR_DISK_ERR) {
                    fr = FR_NO_PATH;
                }
                break; // Break on error or end of dir
            }
            *cnt = *cnt + 1;
        }
    }
    ffs_dclose(&dir);

    return fr;
}

void list_files(const char* path/*, uint16_t from_n, uint8_t for_n*/) {

    FRESULT res;
    static FILINFO filinfo;
    DIR dir;
    int num_dirent;

    get_num_dirents(path, &num_dirent);
    fnos = malloc(sizeof(SmallFilInfo) * num_dirent);
    res = ffs_dopen(&dir, path);
    
    fno_num = 0;

    if (res == FR_OK) {

        while ((ffs_dread(&dir, &filinfo) == FR_OK) && filinfo.fname[0]) {

            fnos[fno_num].fsize = filinfo.fsize;
            fnos[fno_num].fdate = filinfo.fdate;
            fnos[fno_num].ftime = filinfo.ftime;
            fnos[fno_num].fattrib = filinfo.fattrib;
            fnos[fno_num].fname = malloc(strlen(filinfo.fname) + 1);
            strcpy(fnos[fno_num].fname, filinfo.fname);
            fno_num++;

        }

        ffs_dclose(&dir);
        qsort(fnos, num_dirent, sizeof(SmallFilInfo), cmp_filinfo);

        for(int i = 0; i < fno_num; i++) {
            if (fnos[i].fattrib & 0x10) printf("<%s>\r\n", fnos[i].fname);
            else printf("%s\r\n", fnos[i].fname);
        }          
    }
}

SmallFilInfo* get_files(const char* path, int *num_files) {

    FRESULT res;
    static FILINFO filinfo;
    DIR dir;
    int num_dirent;
	SmallFilInfo *sfinfo;

	// pass an int ptr to store the number of entries in the returned fileinfo list
	if ( !num_files ) return NULL;

    get_num_dirents(path, &num_dirent);
    sfinfo = malloc(sizeof(SmallFilInfo) * num_dirent);
    res = ffs_dopen(&dir, path);
    
    fno_num = 0;
	(*num_files) = 0;

    if (res == FR_OK) {

        while ((ffs_dread(&dir, &filinfo) == FR_OK) && filinfo.fname[0]) {

            sfinfo[fno_num].fsize = filinfo.fsize;
            sfinfo[fno_num].fdate = filinfo.fdate;
            sfinfo[fno_num].ftime = filinfo.ftime;
            sfinfo[fno_num].fattrib = filinfo.fattrib;
            sfinfo[fno_num].fname = malloc(strlen(filinfo.fname) + 1);
			if ( sfinfo[fno_num].fname )
				strcpy(sfinfo[fno_num].fname, filinfo.fname);
            fno_num++;
        }

        ffs_dclose(&dir);
        qsort(sfinfo, fno_num, sizeof(SmallFilInfo), cmp_filinfo);

		(*num_files) = fno_num;

		return sfinfo;
    } else {
        ffs_dclose(&dir);
	    return NULL;
    }
}

void free_fnos( SmallFilInfo **pfnos, int n )
{
	for(int i = 0; i < n; i++) {
		free( (*pfnos)[i].fname );
		(*pfnos)[i].fname = NULL;
	}
	free( (*pfnos) );
	(*pfnos) = NULL;
}

