/* pnmpad.c - add border to sides of a portable anymap
 ** AJCD 4/9/90
 */

/*
 * Changelog
 *
 * 2002/01/25 - Rewrote options parsing code.
 *		Added pad-to-width and pad-to-height with custom
 *		alignment.  MVB.
 */

#include <stdio.h>
#include <string.h>
#include "pnm.h"


struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    const char *input_filespec;  /* Filespecs of input files */
    unsigned int xsize;
    unsigned int ysize;
    unsigned int lpad;
    unsigned int rpad;
    unsigned int tpad;
    unsigned int bpad;
    float xalign;
    float yalign;
    bool white;		/* TRUE: pad white; FALSE: pad black */
    bool verbose;
};

static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info * const cmdline_p) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions2 on how to parse our options.
         */
    optStruct2 opt;

    unsigned int option_def_index;
    bool black = FALSE;	/* trap -black option (ignored as black is default) */

    cmdline_p->xsize = cmdline_p->ysize = 0;
    cmdline_p->lpad = cmdline_p->rpad = cmdline_p->tpad = cmdline_p->bpad = 0;
    cmdline_p->xalign = cmdline_p->yalign = 0.5;
    cmdline_p->white = cmdline_p->verbose = FALSE;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY(0,   "xsize",     OPT_UINT,    &cmdline_p->xsize,          0);
    OPTENTRY(0,   "width",     OPT_UINT,    &cmdline_p->xsize,          0);
    OPTENTRY(0,   "ysize",     OPT_UINT,    &cmdline_p->ysize,          0);
    OPTENTRY(0,   "height",    OPT_UINT,    &cmdline_p->ysize,          0);
    OPTENTRY(0,   "left",      OPT_UINT,    &cmdline_p->lpad,           0);
    OPTENTRY(0,   "right",     OPT_UINT,    &cmdline_p->rpad,           0);
    OPTENTRY(0,   "top",       OPT_UINT,    &cmdline_p->tpad,           0);
    OPTENTRY(0,   "bottom",    OPT_UINT,    &cmdline_p->bpad,           0);
    OPTENTRY(0,   "xalign",    OPT_FLOAT,   &cmdline_p->xalign,         0);
    OPTENTRY(0,   "halign",    OPT_FLOAT,   &cmdline_p->xalign,         0);
    OPTENTRY(0,   "yalign",    OPT_FLOAT,   &cmdline_p->yalign,         0);
    OPTENTRY(0,   "valign",    OPT_FLOAT,   &cmdline_p->yalign,         0);
    OPTENTRY(0,   "black",     OPT_FLAG,    &black,         0);
    OPTENTRY(0,   "white",     OPT_FLAG,    &cmdline_p->white,          0);
    OPTENTRY(0,   "verbose",   OPT_FLAG,    &cmdline_p->verbose,        0);

    /* Set the defaults. -1 = unspecified */
    cmdline_p->verbose = FALSE;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions2(&argc, argv, opt, 0);
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */


    /* reject invalid values */
    if (cmdline_p->xalign < 0)
        pm_error("You have specified a negative halign value (%f)", 
                 cmdline_p->xalign);
    if (cmdline_p->xalign > 1)
        pm_error("You have specified a halign value (%f) greater than 1", 
                 cmdline_p->xalign);
    if (cmdline_p->yalign < 0)
        pm_error("You have specified a negative halign value (%f)", 
                 cmdline_p->yalign);
    if (cmdline_p->yalign > 1)
        pm_error("You have specified a valign value (%f) greater than 1", 
                 cmdline_p->yalign);

    /* get optional input filename */
    if (argc-1 > 1)
        pm_error("This program takes at most 1 parameter.  You specified %d",
                 argc-1);
    else if (argc-1 == 1) 
        cmdline_p->input_filespec = argv[1];
    else 
        cmdline_p->input_filespec = "-";
}



static void
parse_command_line_old(int argc, char ** argv,
                       struct cmdline_info * const cmdline_p) {

    /* This syntax was abandonned in February 2002. */
    pm_message("Warning: old style options are deprecated!");

    cmdline_p->xsize = cmdline_p->ysize = 0;
    cmdline_p->lpad = cmdline_p->rpad = cmdline_p->tpad = cmdline_p->bpad = 0;
    cmdline_p->xalign = cmdline_p->yalign = 0.5;
    cmdline_p->white = cmdline_p->verbose = FALSE;

    while (argc >= 2 && argv[1][0] == '-') {
        if (strcmp(argv[1]+1,"black") == 0) cmdline_p->white = FALSE;
        else if (strcmp(argv[1]+1,"white") == 0) cmdline_p->white = TRUE;
        else switch (argv[1][1]) {
        case 'l':
            if ((cmdline_p->lpad = atoi(argv[1]+2)) < 0)
                pm_error("left border too small");
            break;
        case 'r':
            if ((cmdline_p->rpad = atoi(argv[1]+2)) < 0)
                pm_error("right border too small");
            break;
        case 'b':
            if ((cmdline_p->bpad = atoi(argv[1]+2)) < 0)
                pm_error("bottom border too small");
            break;
        case 't':
            if ((cmdline_p->tpad = atoi(argv[1]+2)) < 0)
                pm_error("top border too small");
            break;
        default:
            pm_usage("[-white|-black] [-l#] [-r#] [-t#] [-b#] [pnmfile]");
        }
        argc--, argv++;
    }

    if (argc > 2)
        pm_usage("[-white|-black] [-l#] [-r#] [-t#] [-b#] [pnmfile]");

    if (argc == 2)
        cmdline_p->input_filespec = argv[1];
    else
        cmdline_p->input_filespec = "-";
}



int
main(int argc, char ** argv) {

    struct cmdline_info cmdline;
    FILE *ifP;
    xel *xelrow, *bgrow, background;
    xelval maxval;
    int rows, cols, newcols, row, col, format;
    bool depr_cmd = FALSE; /* use deprecated commandline interface */

    pnm_init( &argc, argv );

    /* detect deprecated options */
    if (argc > 1 && argv[1][0] == '-') {
        if (argv[1][1] == 't' || argv[1][1] == 'b'
            || argv[1][1] == 'l' || argv[1][1] == 'r') {
            if (argv[1][2] >= '0' && argv[1][2] <= '9')
                depr_cmd = TRUE;
        }
    }
    if (argc > 2 && argv[2][0] == '-') {
        if (argv[2][1] == 't' || argv[2][1] == 'b'
            || argv[2][1] == 'l' || argv[2][1] == 'r') {
            if (argv[2][2] >= '0' && argv[2][2] <= '9')
                depr_cmd = TRUE;
        }
    }

    if (depr_cmd) 
        parse_command_line_old(argc, argv, &cmdline);
    else 
        parse_command_line(argc, argv, &cmdline);

    ifP = pm_openr(cmdline.input_filespec);

    pnm_readpnminit(ifP, &cols, &rows, &maxval, &format);
    if (cmdline.white)
        background = pnm_whitexel(maxval, format);
    else
        background = pnm_blackxel(maxval, format);

    if (cols == 0 || rows == 0) {
        pm_message("empty bitmap");
        cmdline.lpad = cmdline.rpad = cmdline.tpad = cmdline.bpad = 0;
    }

    if (cmdline.verbose) pm_message("image WxH = %dx%d", cols, rows);

    if (cmdline.xsize > 0) {
        if (cmdline.xsize < cols) {
            cmdline.lpad = cmdline.rpad = 0;
        }
        else {
            cmdline.lpad = (float)(cmdline.xsize - cols) * cmdline.xalign;
            cmdline.rpad = cmdline.xsize - cols - cmdline.lpad;
            if (cmdline.verbose) 
                pm_message("calculated hpad: %d - %d - %d",
                           cmdline.lpad, cols, cmdline.rpad);
        }
    }

    if (cmdline.ysize > 0) {
        if (cmdline.ysize < rows) {
            cmdline.bpad = cmdline.tpad = 0;
        }
        else {
            cmdline.bpad = (float)(cmdline.ysize - rows) * cmdline.yalign;
            cmdline.tpad = cmdline.ysize - rows - cmdline.bpad;
            if (cmdline.verbose) 
                pm_message("calculated vpad: %d - %d - %d",
                           cmdline.bpad, rows, cmdline.tpad);
        }
    }

	overflow_add(cols, cmdline.lpad);
	overflow_add(cols + cmdline.lpad, cmdline.rpad);
    newcols = cols + cmdline.lpad + cmdline.rpad;
    xelrow = pnm_allocrow(newcols);
    bgrow = pnm_allocrow(newcols);

    for (col = 0; col < newcols; col++)
        xelrow[col] = bgrow[col] = background;

    pnm_writepnminit(stdout, newcols, rows + cmdline.tpad + cmdline.bpad, 
                     maxval, format, 0);

    for (row = 0; row < cmdline.tpad; row++)
        pnm_writepnmrow(stdout, bgrow, newcols, maxval, format, 0);

    for (row = 0; row < rows; row++) {
        pnm_readpnmrow(ifP, &xelrow[cmdline.lpad], cols, maxval, format);
        pnm_writepnmrow(stdout, xelrow, newcols, maxval, format, 0);
    }

    for (row = 0; row < cmdline.bpad; row++)
        pnm_writepnmrow(stdout, bgrow, newcols, maxval, format, 0);

    pnm_freerow(xelrow);
    pnm_freerow(bgrow);

    pm_close(ifP);

    exit(0);
}


