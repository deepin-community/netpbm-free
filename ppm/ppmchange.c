/* ppmchange.c - change a given color to another
**
** Copyright (C) 1991 by Wilson H. Bent, Jr.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
** Modified by Alberto Accomazzi (alberto@cfa.harvard.edu).
**     28 Jan 94 -  Added support for multiple color substitution.
*/

#include "ppm.h"
#define TCOLS 256
#define SQRT3 1.73205080756887729352
    /* The square root of 3 */

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filespec;  /* Filespecs of input files */
    int ncolors;      /* Number of valid entries in color0[], color1[] */
    char * colorname0[TCOLS];  /* colors user wants replaced */
    char * colorname1[TCOLS];  /* colors with which he wants them replaced */
    int closeness;    
       /* -closeness option value.  Zero if no -closeness option */
    char * remainder_colorname;  
      /* Color user specified for -remainder.  Null pointer if he didn't
         specify -remainder.
         */
};



static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdlineP) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions2 on how to parse our options.
         */
    optStruct2 opt;

    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY(0,   "closeness",     OPT_INT,    &cmdlineP->closeness,     0);
    OPTENTRY(0,   "remainder",     OPT_STRING, 
             &cmdlineP->remainder_colorname,         0);

    /* Set the defaults */
    cmdlineP->closeness = 0;
    cmdlineP->remainder_colorname = NULL;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We may have parms that are negative numbers */

    pm_optParseOptions2(&argc, argv, opt, 0);
        /* Uses and sets argc, argv, and some of *cmdlineP and others. */

    if ((argc-1) % 2 == 0) 
        cmdlineP->input_filespec = "-";
    else
        cmdlineP->input_filespec = argv[argc-1];

    {
        int argn;
        cmdlineP->ncolors = 0;  /* initial value */
        for (argn = 1; 
             argn+1 < argc && cmdlineP->ncolors < TCOLS; 
             argn += 2) {
            cmdlineP->colorname0[cmdlineP->ncolors] = argv[argn];
            cmdlineP->colorname1[cmdlineP->ncolors] = argv[argn+1];
            cmdlineP->ncolors++;
        }
    }
}



static int
sqri(const int N) {
    return N*N;
}


static double
sqrf(const float F) {
    return F*F;
}



static int 
colormatch(const pixel comparand, const pixel comparator, 
           const float closeness) {
/*----------------------------------------------------------------------------
   Return true iff 'comparand' matches 'comparator' in color within the
   fuzz factor 'closeness'.
-----------------------------------------------------------------------------*/
    /* Fast path for usual case */
    if (closeness == 0)
        return PPM_EQUAL(comparand, comparator);

    return (sqri(PPM_GETR(comparand) - PPM_GETR(comparator)) +
            sqri(PPM_GETG(comparand) - PPM_GETG(comparator)) +
            sqri(PPM_GETB(comparand) - PPM_GETB(comparator)) 
            <= sqrf(closeness)
        );
}



static void
change_row(const pixel * const inrow, pixel * const outrow, 
           const int cols,
           int ncolors, const pixel colorfrom[], const pixel colorto[],
           const int remainder_specified, 
           const pixel remainder_color, const float closeness) {
/*----------------------------------------------------------------------------
   Replace the colors in a single row.  There are 'ncolors' colors to 
   replace.  The to-replace colors are in the array colorfrom[], and the
   replace-with colors are in corresponding elements of colorto[].
   Iff 'remainder_specified' is true, replace all colors not mentioned
   in colorfrom[] with 'remainder_color'.  Use the closeness factor
   'closeness' in determining if something in the input row matches
   a color in colorfrom[].

   The input row is 'inrow'.  The output is returned as 'outrow', in
   storage which must be already allocated.  Both are 'cols' columns wide.
-----------------------------------------------------------------------------*/
    int col;

    for ( col = 0; col < cols; ++col ) {
        int i;
        int have_match; /* logical: It's a color user said to change */
        pixel newcolor;  
        /* Color to which we must change current pixel.  Undefined unless
           'have_match' is true.
        */

        have_match = FALSE;  /* haven't found a match yet */
        for (i = 0; i < ncolors && !have_match; i++) {
            have_match = 
                colormatch(inrow[col], colorfrom[i], closeness);
            newcolor = colorto[i];
        }
        if (have_match)
            outrow[col] = newcolor;
        else if (remainder_specified)
            outrow[col] = remainder_color;
        else
            outrow[col] = inrow[col];
    }
}



int
main(int argc, char *argv[]) {
    struct cmdline_info cmdline;
    FILE* ifp;
    int format;
    int rows, cols;
    pixval maxval;
    float closeness;
    int row;
    pixel* inrow;
    pixel* outrow;

    pixel color0[TCOLS];  /* colors user wants replaced */
    pixel color1[TCOLS];  /* colors with which he wants them replaced */
    pixel remainder_color;
      /* Color user specified for -remainder.  Undefined if he didn't
         specify -remainder.
         */

    ppm_init( &argc, argv );

    parse_command_line(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.input_filespec);

    ppm_readppminit(ifp, &cols, &rows, &maxval, &format);

    if (cmdline.remainder_colorname)
        remainder_color = ppm_parsecolor(cmdline.remainder_colorname, maxval);
    { 
        int i;
        for (i = 0; i < cmdline.ncolors; i++) {
            color0[i] = ppm_parsecolor(cmdline.colorname0[i], maxval);
            color1[i] = ppm_parsecolor(cmdline.colorname1[i], maxval);
        }
    }
    closeness = SQRT3 * maxval * cmdline.closeness/100;

    ppm_writeppminit( stdout, cols, rows, maxval, 0 );
    inrow = ppm_allocrow(cols);
    outrow = ppm_allocrow(cols);

    /* Scan for the desired color */
    for (row = 0; row < rows; row++) {
        ppm_readppmrow(ifp, inrow, cols, maxval, format);

        change_row(inrow, outrow, cols, cmdline.ncolors, color0, color1,
                   cmdline.remainder_colorname != NULL,
                   remainder_color, closeness);

        ppm_writeppmrow(stdout, outrow, cols, maxval, 0);
    }

    pm_close(ifp);

    exit(0);
}
