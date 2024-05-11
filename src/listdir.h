#ifndef __LISTFILES_H
#define __LISTFILES_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <mos_api.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* File attribute bits for directory entry (FILINFO.fattrib) */
#define	AM_RDO	0x01	/* Read only */
#define	AM_HID	0x02	/* Hidden */
#define	AM_SYS	0x04	/* System */
#define AM_DIR	0x10	/* Directory */
#define AM_ARC	0x20	/* Archive */

typedef struct SmallFilInfo {
    uint64_t fsize;   /* File size */
    unsigned short    fdate;   /* Modified date */
    unsigned short    ftime;   /* Modified time */
    unsigned char    fattrib; /* File attribute */
    char*   fname;   /* malloc'ed */
} SmallFilInfo;

SmallFilInfo *get_files(const char* path, int *num_files);
void free_fnos( SmallFilInfo **fnos, int num_fnos );

#endif
