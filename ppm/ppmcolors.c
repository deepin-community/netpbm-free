/******************************************************************************
                                ppmcolors
*******************************************************************************
  Generate a color map containing all the colors representable with a certain
  maxval.
  
  The color map is a one-row PPM image containing one pixel of each
  color.

  Written by Bryan Henderson in 2001
  Contributed to the public domain.
  (The license information was taken from the manpage.)

******************************************************************************/

#include "ppm.h"

struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    unsigned int maxval;   
};



static void
parseCommandLine(int argc, char ** argv, struct cmdlineInfo *cmdlineP) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct3 opt;  /* set by OPTENT3 */
    optEntry *option_def = malloc(100*sizeof(optEntry));
    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0,   "maxval",       OPT_UINT,   
            &cmdlineP->maxval,  NULL, 0);
    

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We may have parms that are negative numbers */

    /* defaults */
    cmdlineP->maxval = 5;

    pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);
        /* Uses and sets argc, argv, and some of *cmdlineP and others. */

    if (cmdlineP->maxval < 1)
        pm_error("-maxval must be at least 1");
    else if (cmdlineP->maxval > PPM_OVERALLMAXVAL)
        pm_error("-maxval too high.  The maximum allowable value is %u",
                 PPM_OVERALLMAXVAL);

    if (argc-1 > 0)
        pm_error("This program takes no arguments.  You specified %d",
                 argc-1);
}



int
main(int argc, char *argv[]) {

    struct cmdlineInfo cmdline;
    int col;
    pixval red, grn, blu;
    pixel * pixelrow;
    int cols;

    ppm_init(&argc, argv);

    parseCommandLine(argc, argv, &cmdline);

    {
        unsigned int const maxvalRange = cmdline.maxval + 1;
        cols = maxvalRange * maxvalRange * maxvalRange;
    }
    ppm_writeppminit(stdout, cols, 1, cmdline.maxval, FALSE);

    pixelrow = ppm_allocrow(cols);
    col = 0;  /* initial value */
    for (red = 0; red <= cmdline.maxval; ++red) {
        for (grn = 0; grn <= cmdline.maxval; ++grn) {
            for (blu = 0; blu <= cmdline.maxval; ++blu) {
                PPM_ASSIGN(pixelrow[col], red, grn, blu);
                ++col;
            }
        }
    }
    ppm_writeppmrow(stdout, pixelrow, cols, cmdline.maxval, FALSE);

    ppm_freerow(pixelrow);                
    return 0;
}




