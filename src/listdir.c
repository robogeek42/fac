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

static int cmp_filinfo(const void *pa, const void *pb) {
    const SmallFilInfo *a = (const SmallFilInfo *)pa;
    const SmallFilInfo *b = (const SmallFilInfo *)pb;
    if ((a->fattrib & AM_DIR) == (b->fattrib & AM_DIR)) {
        return strcasecmp(a->fname, b->fname);
    } else if (a->fattrib & AM_DIR) {
        return -1;
    } else {
        return 1;
    }
}

uint24_t get_num_dirents(const char* path, int* cnt) {
    FRESULT        fr = FR_OK;
    DIR            dir;
    FILINFO fno;

    (*cnt) = 0;
    fr = ffs_dopen(&dir, path);

    if (fr == FR_OK) {
        for (;;) {
            fr = ffs_dread(&dir, &fno);
            if (fr != FR_OK || fno.fname[0] == 0) {
                if ((*cnt) == 0 && fr == FR_DISK_ERR) {
                    fr = FR_NO_PATH;
                }
                break; // Break on error or end of dir
            }
            (*cnt)++;
        }
    } else {
		//printf("\nError\n");
	}
    ffs_dclose(&dir);

    return fr;
}

SmallFilInfo* get_files(const char* path, int *num_files) {

    FRESULT res;
    FILINFO filinfo;
    DIR dir;
    int num_dirent = 0;
	SmallFilInfo *sfinfo;
    int fno_num = 0;

	// pass an int ptr to store the number of entries in the returned fileinfo list
	if ( !num_files ) return NULL;
	(*num_files) = 0;

    uint24_t fr = get_num_dirents(path, &num_dirent);
	if ( fr != FR_OK )
	{
		printf("Error FR=%d\n",fr);
		return NULL;
	}
    sfinfo = malloc(sizeof(SmallFilInfo) * num_dirent);
	if ( !sfinfo )
	{
		return NULL;
	}
    res = ffs_dopen(&dir, path);
    
    if (res == FR_OK) {

        while ((ffs_dread(&dir, &filinfo) == FR_OK) && filinfo.fname[0]) {
            sfinfo[fno_num].fsize = filinfo.fsize;
            sfinfo[fno_num].fdate = filinfo.fdate;
            sfinfo[fno_num].ftime = filinfo.ftime;
            sfinfo[fno_num].fattrib = filinfo.fattrib;
            sfinfo[fno_num].fname = malloc(strlen(filinfo.fname) + 1);
			if ( sfinfo[fno_num].fname )
			{
				strcpy(sfinfo[fno_num].fname, filinfo.fname);
			} else {
				return NULL;
			}
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

