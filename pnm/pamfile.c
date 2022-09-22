/* pamfile.c - describe a portable anymap
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

#include "pam.h"


struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    int n_input_file;  /* Number of input files */
    char **input_filespec;  /* Filespecs of input files */
    int allimages;  /* He wants info on all images in each file */
};



static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to as as the argv array.
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions2 on how to parse our options.
         */
    optStruct2 opt;

    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY(0,   "allimages", OPT_FLAG,   &cmdline_p->allimages,     0);

    /* Set the defaults */
    cmdline_p->allimages = FALSE;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions2(&argc, argv, opt, 0);
        /* Uses and sets argc, argv, and all of *cmdline_p. */

    cmdline_p->input_filespec = argv + 1;
    cmdline_p->n_input_file = argc - 1;
}



static void
dump_header(const struct pam pam) {
    switch (pam.format) {
    case PAM_FORMAT:
        printf("PAM, %d by %d by %d maxval %ld\n", 
               pam.width, pam.height, pam.depth, pam.maxval);
        printf("    Tuple type: %s\n", pam.tuple_type);
        break;

	case PBM_FORMAT:
        printf("PBM plain, %d by %d\n", pam.width, pam.height );
        break;

	case RPBM_FORMAT:
        printf("PBM raw, %d by %d\n", pam.width, pam.height);
        break;

	case PGM_FORMAT:
        printf("PGM plain, %d by %d  maxval %ld\n", 
               pam.width, pam.height, pam.maxval);
        break;

	case RPGM_FORMAT:
        printf("PGM raw, %d by %d  maxval %ld\n", 
               pam.width, pam.height, pam.maxval);
        break;

	case PPM_FORMAT:
        printf("PPM plain, %d by %d  maxval %ld\n", 
               pam.width, pam.height, pam.maxval);
        break;

	case RPPM_FORMAT:
        printf("PPM raw, %d by %d  maxval %ld\n", 
               pam.width, pam.height, pam.maxval);
        break;
    }
}



static void
describe_one_file(const char * const name, FILE *file, const int allimages) {

    struct pam pam;
    enum pm_check_code check_retval;
    int image_no;  /* Sequence number of current image in file.  First = 0 */
    int done;  /* No more images we must describe */
    
    done = FALSE;  /* At least one image is required */
    for (image_no = 0; !done; image_no++) {
        if (allimages)
            printf("%s:\tImage %d:\t", name, image_no);
        else 
            printf( "%s:\t", name );

        pnm_readpaminit(file, &pam, sizeof(pam));
        
        dump_header(pam);

        pnm_checkpam(&pam, PM_CHECK_BASIC, &check_retval);
        if (allimages) {
            tuple *tuplerow;
            int eof;
            int row;

            tuplerow = pnm_allocpamrow(&pam);
            for (row = 0; row < pam.height; row++) 
                pnm_readpamrow(&pam, tuplerow);
            pnm_freepamrow(tuplerow);
            pnm_nextimage(file, &eof);
            done = eof;
        } else
            done = TRUE;
    }
}



int
main(int argc, char *argv[]) {

    struct cmdline_info cmdline;

    pnm_init(&argc, argv);

    parse_command_line(argc, argv, &cmdline);

    if (cmdline.n_input_file == 0)
        describe_one_file("stdin", stdin, cmdline.allimages);
    else {
        int i;
        for (i = 0; i < cmdline.n_input_file; i++) {
            FILE* ifp;
            ifp = pm_openr(cmdline.input_filespec[i]);
            describe_one_file(cmdline.input_filespec[i], ifp, 
                              cmdline.allimages);
            pm_close(ifp);
	    }
	}
    
    exit(0);
}
