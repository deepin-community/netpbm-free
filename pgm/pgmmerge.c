/* pgmmerge.c - wrapper program for PGM
**
** Copyright (C) 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

/* Note: be careful using any Netpbm library functions in here, since
   we don't call pgm_init()
*/

#include <stdio.h>
#include <string.h>
#include "pgm.h"

int
main(int argc, char *argv[]) {

    char* cp;
    
    if (strcmp(pm_arg0toprogname(argv[0]), "pgmmerge") == 0) {
        ++argv;
        --argc;
        if (!*argv)	{
            fprintf(stderr, "Usage: pgmmerge pgm_program_name [args ...]\n");
            exit(1);
		}
	}

    cp = pm_arg0toprogname(argv[0]);
    
    /* merge.h is an automatically generated file that generates code to
       match the string 'cp' against the name of every program that is part
       of this merge and, upon finding a match, invoke that program.
    */

#include "merge.h"   

    fprintf(stderr,"'%s' is a PGM program name unknown to pgmmerge\n", cp );
    exit(1);
}



