/* ppmhist.c - read a portable pixmap and compute a color histogram
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <string.h>
#include "ppm.h"
#include "ppmcmap.h"

enum sort {SORT_BY_FREQUENCY, SORT_BY_RGB};

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filespec;  /* Filespecs of input files */
    unsigned int map;         /* -map option */
    unsigned int noheader;    /* -noheader option */
    unsigned int hexcolor;    /* -hexcolor option */
    enum sort sort;           /* -sort option */
};



static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct3 opt;  /* set by OPTENT3 */
    optEntry *option_def = malloc(100*sizeof(optEntry));
    unsigned int option_def_index;
    
    unsigned int nomap;  /* dummy option for backward compatibility */
    char * sort_type;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENT3(0,   "map",      OPT_FLAG, NULL,  &cmdline_p->map,         0);
    OPTENT3(0,   "nomap",    OPT_FLAG, NULL,  &nomap,                  0);
    OPTENT3(0,   "noheader", OPT_FLAG, NULL,  &cmdline_p->noheader,    0);
    OPTENT3(0,   "hexcolor", OPT_FLAG, NULL,  &cmdline_p->hexcolor,    0);
    OPTENT3(0,   "sort",     OPT_STRING, &sort_type, NULL,             0);

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    /* Set defaults */
    sort_type = "frequency";

    pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (argc-1 == 0) 
        cmdline_p->input_filespec = "-";
    else if (argc-1 != 1)
        pm_error("Program takes zero or one argument (filename).  You "
                 "specified %d", argc-1);
    else
        cmdline_p->input_filespec = argv[1];

    if (cmdline_p->hexcolor && cmdline_p->map)
        pm_error("You cannot specify both -hexcolor and -map");

    if (strcmp(sort_type, "frequency") == 0)
        cmdline_p->sort = SORT_BY_FREQUENCY;
    else if (strcmp(sort_type, "rgb") == 0)
        cmdline_p->sort = SORT_BY_RGB;
    else
        pm_error("Invalid -sort value: '%s'.  The valid values are "
                 "'frequency' and 'rgb'.", sort_type);
}



static int
countcompare(const void *ch1, const void *ch2) {
    return ((colorhist_vector)ch2)->value - ((colorhist_vector)ch1)->value;
}


static int
rgbcompare(const void * arg1, const void * arg2) {

    colorhist_vector const ch1 = (colorhist_vector) arg1;
    colorhist_vector const ch2 = (colorhist_vector) arg2;

    int retval;

    retval = (PPM_GETR(ch1->color) - PPM_GETR(ch2->color));
    if (retval == 0) {
        retval = (PPM_GETG(ch1->color) - PPM_GETG(ch2->color));
        if (retval == 0)
            retval = (PPM_GETB(ch1->color) - PPM_GETB(ch2->color));
    }
    return retval;
}



int
main(int argc, char *argv[] ) {
    struct cmdline_info cmdline;
    FILE* ifp;
    pixel** pixels;
    colorhist_vector chv;
    int rows, cols, colors, i;
    pixval maxval;
    int (*compare_function)(const void *, const void *);
        /* The compare function to be used with qsort() to sort the
           histogram for output
        */

    ppm_init( &argc, argv );

    parse_command_line(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.input_filespec);

    pixels = ppm_readppm( ifp, &cols, &rows, &maxval );

    pm_close( ifp );

    chv = ppm_computecolorhist(pixels, cols, rows, 0, &colors);

    switch (cmdline.sort) {
    case SORT_BY_FREQUENCY:
        compare_function = countcompare; break;
    case SORT_BY_RGB:
        compare_function = rgbcompare; break;
    }

    qsort((char*) chv, colors, sizeof(struct colorhist_item), 
          compare_function);

    /* And print the histogram. */
    if (cmdline.map) 
        printf("P3\n# color map\n%d 1\n%d\n", colors, maxval);

    if (!cmdline.noheader) {
        printf( "%c  r     g     b   \t lum \t count\n",
                cmdline.map ? '#' : ' ');
        printf( "%c----- ----- ----- \t-----\t-------\n",
                cmdline.map ? '#' : ' ');
    }

    for ( i = 0; i < colors; i++ ) {
        printf(cmdline.hexcolor ? 
               "  %04x  %04x  %04x%s\t%5d\t%7d\n" :
               " %5d %5d %5d%s\t%5d\t%7d\n",
               
               PPM_GETR(chv[i].color),
               PPM_GETG(chv[i].color), 
               PPM_GETB(chv[i].color),
               (cmdline.map ? " #" : ""),
               (int) ( PPM_LUMIN( chv[i].color ) + 0.5 ),
               chv[i].value );
    }
    ppm_freecolorhist( chv );
    exit( 0 );
    }


