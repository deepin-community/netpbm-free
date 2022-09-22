/* libpbm1.c - pbm utility library part 1
**
** Copyright (C) 1988 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <stdio.h>
#include "pbm.h"
#include "version.h"
#include "compile.h"
#include "libpbm.h"
#include "shhopt.h"

void
pbm_init(int *argcP, char *argv[]) {
    pm_proginit(argcP, argv);
}



void
pbm_nextimage(FILE *file, int * const eofP) {
    pm_nextimage(file, eofP);
}



void
pbm_check(FILE * file, const enum pm_check_type check_type, 
          const int format, const int cols, const int rows,
          enum pm_check_code * const retval_p) {

    if (rows < 0 || cols < 0)
        pm_error("invalid image");
    if (check_type != PM_CHECK_BASIC) {
        if (retval_p) *retval_p = PM_CHECK_UNKNOWN_TYPE;
    } else if (format != RPBM_FORMAT) {
        if (retval_p) *retval_p = PM_CHECK_UNCHECKABLE;
    } else {        
        /* signed to unsigned so wont wrap */
        const unsigned int bytes_per_row = (cols+7)/8;
        const unsigned int need_raster_size = rows * bytes_per_row;

	overflow2(bytes_per_row, rows);
        
        pm_check(file, check_type, need_raster_size, retval_p);
    }
}
