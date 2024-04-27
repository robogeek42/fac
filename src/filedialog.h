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
#include "../../agon_ccode/common/util.h"

bool file_dialog( char *dir, char *filenamestr, int maxstr, bool *isload );

#endif
