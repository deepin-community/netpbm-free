/* pnmcrop.c - crop a portable anymap
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

/* IDEA FOR EFFICIENCY IMPROVEMENT:

   If we have to read the input into a regular file because it is not
   seekable (a pipe), find the borders as we do the copy, so that we
   do 2 passes through the file instead of 3.  Also find the background
   color in that pass to save yet another pass with -sides.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "pnm.h"

enum bg_choice {BG_BLACK, BG_WHITE, BG_DEFAULT, BG_SIDES};

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filespec;  /* Filespecs of input files */
    enum bg_choice background;
    int left, right, top, bottom;
    int verbose;
} cmdline;



static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions2 on how to parse our options.
         */
    optStruct2 opt;

    unsigned int option_def_index;

    int black_opt, white_opt, sides_opt;
    
    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY(0,   "black",      OPT_FLAG,   &black_opt,                 0);
    OPTENTRY(0,   "white",      OPT_FLAG,   &white_opt,                 0);
    OPTENTRY(0,   "sides",      OPT_FLAG,   &sides_opt,                 0);
    OPTENTRY(0,   "left",       OPT_FLAG,   &cmdline_p->left,           0);
    OPTENTRY(0,   "right",      OPT_FLAG,   &cmdline_p->right,          0);
    OPTENTRY(0,   "top",        OPT_FLAG,   &cmdline_p->top,            0);
    OPTENTRY(0,   "bottom",     OPT_FLAG,   &cmdline_p->bottom,         0);
    OPTENTRY(0,   "verbose",    OPT_FLAG,   &cmdline_p->verbose,        0);

    /* Set the defaults */
    cmdline_p->left = cmdline_p->right = cmdline_p->top = cmdline_p->bottom 
        = FALSE;
    cmdline_p->verbose = FALSE;
    black_opt = FALSE;
    white_opt = FALSE;
    sides_opt = FALSE;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions2(&argc, argv, opt, 0);
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (argc-1 == 0)
        cmdline_p->input_filespec = "-";  /* stdin */
    else if (argc-1 == 1)
        cmdline_p->input_filespec = argv[1];
    else 
        pm_error("Too many arguments (%d).  "
                 "Only need one: the input filespec", argc-1);

    if (black_opt && white_opt)
        pm_error("You cannot specify both -black and -white");
    else if (sides_opt &&( black_opt || white_opt ))
        pm_error("You cannot specify both -sides and either -black or -white");
    else if (black_opt)
        cmdline_p->background = BG_BLACK;
    else if (white_opt)
        cmdline_p->background = BG_WHITE;
    else if (sides_opt)
        cmdline_p->background = BG_SIDES;
    else
        cmdline_p->background = BG_DEFAULT;

    if (!cmdline_p->left && !cmdline_p->right && !cmdline_p->top
        && !cmdline_p->bottom) {
        cmdline_p->left = cmdline_p->right = cmdline_p->top 
            = cmdline_p->bottom = TRUE;
    }
}



static xel
background_3_corners(FILE * image_file, const int rows, const int cols,
                     const pixval maxval, const int format) {
/*----------------------------------------------------------------------------
  Read in the whole image, and check all the corners to determine the
  background color.  This is a quite reliable way to determine the
  background color.
----------------------------------------------------------------------------*/
    int filepos;
    int seek_rc;
    int row;
    xel** xels;
    xel background;   /* our return value */

    filepos = ftell(image_file);
    if (filepos < 0)
        pm_error("ftell() was unable to tell the position of the file, "
                 "for purposes of resetting after checking the "
                 "background color.  Errno = %s (%d)",
                 strerror(errno), errno);
    xels = pnm_allocarray( cols, rows );
    for ( row = 0; row < rows; ++row )
        pnm_readpnmrow( image_file, xels[row], cols, maxval, format );
    background = pnm_backgroundxel(xels, cols, rows, maxval, format);
    pnm_freearray(xels, rows);
    seek_rc = fseek(image_file, filepos, SEEK_SET);
    if (seek_rc < 0)
        pm_error("lseek() failed to reposition file after checking "
                 "background color.  "
                 "Errno = %s (%d)", strerror(errno), errno);

    return background;
}



static xel
background_2_corners(FILE * image_file, const int cols,
                     const pixval maxval, const int format) {
/*----------------------------------------------------------------------------
  Look at just the top row of pixels and determine the background
  color from the top corners; often this is enough to accurately
  determine the background color.
----------------------------------------------------------------------------*/
    int filepos;
    int seek_rc;
    xel *xelrow;
    xel background;   /* our return value */
    
    filepos = ftell(image_file);
    if (filepos < 0)
        pm_error("ftell() was unable to tell the position of the file, "
                 "for purposes of resetting after checking the "
                 "background color.  Errno = %s (%d)",
                 strerror(errno), errno);
    
    xelrow = pnm_allocrow(cols);
    pnm_readpnmrow(image_file, xelrow, cols, maxval, format);
    background = pnm_backgroundxelrow(xelrow, cols, maxval, format);
    pnm_freerow(xelrow);
    
    seek_rc = fseek(image_file, filepos, SEEK_SET);
    if (seek_rc < 0) 
        pm_error("lseek() failed to reposition file after checking "
                 "background color.  "
                 "Errno = %s (%d)", strerror(errno), errno);

    return background;
}



static xel
compute_background(const enum bg_choice background_choice,
                   FILE * image_file, const int cols, const int rows,
                   const xelval maxval, const int format, const int verbose) {

    xel background;  /* Our return value */
    
    switch (background_choice) {
    case BG_WHITE:
	    background = pnm_whitexel(maxval, format);
        break;
    case BG_BLACK:
	    background = pnm_blackxel(maxval, format);
        break;
    case BG_SIDES: 
        background = 
            background_3_corners(image_file, rows, cols, maxval, format);
        break;
    case BG_DEFAULT: 
        background = 
            background_2_corners(image_file, cols, maxval, format);
        break;
    default:
        pm_error("Internal error: impossible value for background_choice");
    }

    if (verbose)
        pm_message("Background color is %s", 
                   ppm_colorname(&background, maxval, TRUE /*hexok*/));

    return(background);
}



static void
find_borders(FILE *image_file, const enum bg_choice background_choice, 
             const int verbose, 
             int * const left_p, int * const right_p, 
             int * const top_p, int * const bottom_p) {
/*----------------------------------------------------------------------------
   Find the left, right, top, and bottom borders in the image
   'image_file'.  Return as *left_p the column number of the leftmost
   column that contains something besides background color.  Return as
   *right_p the column number of the rightmost such column.  Return as
   *top_p the row number of the topmost not-all-background row, and
   return as *bottom_p the bottommost such row.
   
   Note that these are rown and column numbers, not border sizes, and
   all refer to a row or column that is part of the image, not the
   border.

   Iff the image is all background, *right_p == -1.

   Expect the input file to be positioned to the beginning of an image
   and leave it positioned just after that image.
-----------------------------------------------------------------------------*/

    xel* xelrow;        /* A row of the input image */
    xel background;
    xelval maxval;
    int format;
    int rows, cols;
    int row, gottop;

    pnm_readpnminit(image_file, &cols, &rows, &maxval, &format);

    background = compute_background(background_choice, image_file,
                                    cols, rows, maxval, format, verbose);

    xelrow = pnm_allocrow(cols);
    
    *left_p = cols;  /* initial value */
    *right_p = -1;   /* initial value */
    *top_p = rows;   /* initial value */
    *bottom_p = -1;  /* initial value */

    gottop = FALSE;
    for (row = 0; row < rows; row++) {
        int col;
        int gotleft;
        int this_row_right, this_row_left;

        gotleft = FALSE;  /* initial value */
        this_row_right = -1;  /* initial value */
        this_row_left = cols;
        pnm_readpnmrow(image_file, xelrow, cols, maxval, format);
        for (col = 0; col < cols; col++) {
            if (!PNM_EQUAL(xelrow[col], background)) {
                if (!gotleft) {
                    gotleft = TRUE;
                    this_row_left = col;
                }
                this_row_right = col;   /* New candidate */
            }
        }
        *right_p = max(this_row_right, *right_p);
        *left_p = min(this_row_left, *left_p);

        if (this_row_left < cols) {
            /* This row is not entirely background */
            if (!gottop) {
                gottop = TRUE;
                *top_p = row;
            }
            *bottom_p = row;   /* New candidate */
        }
    }
}



static void
report_cropping_parameters(const int left, const int right, 
                           const int top, const int bottom,
                           const int rows, const int cols) {

#define ending(n) (((n) > 1) ? "s" : "")

    if (top > 0)
        pm_message("cropping %d row%s off the top", top, ending(top));
    if (bottom < rows - 1)
        pm_message("cropping %d row%s off the bottom", 
                   rows-1-bottom, ending(rows-1-bottom));
    if (left > 0)
        pm_message("cropping %d col%s off the left", left, ending(left));
    if (right < cols - 1)
        pm_message("cropping %d col%s off the right", 
                    cols-1-right, ending(cols-1-right));

    if (top == 0 && bottom == rows-1 && left == 0 && right == cols-1) {
        pm_message("Not cropping.  No border found.");
    }
}



int
main(int argc, char *argv[]) {

    FILE* ifp;   
        /* The program's input file.  Could be a seekable copy of it
           in a temporary file.
        */

    xelval maxval;
    int format;
    int rows, cols;   /* dimensions of input image */
    int left, right, top, bottom;
        /* The places at which we crop */
    int left_border, right_border, top_border, bottom_border;
        /* The locations of the borders in the input image */
    int seek_rc;  /* return code from a file seek operation */

    pnm_init(&argc, argv);

    parse_command_line(argc, argv, &cmdline);

    ifp = pm_openr_seekable(cmdline.input_filespec);

    find_borders(ifp, cmdline.background, cmdline.verbose, 
                 &left_border, &right_border, &top_border, &bottom_border);

    if (right_border == -1) {
        pm_error("The image is entirely background; "
                 "there is nothing to crop.");
    }

    seek_rc = fseek(ifp, 0, SEEK_SET);
    if (seek_rc != 0) 
        pm_error("fseek() failed to reset the image file to the beginning "
                 "after examining it for borders.  Errno = %s (%d)",
                 strerror(errno), errno);
    
    pnm_readpnminit(ifp, &cols, &rows, &maxval, &format);
    
    if (cmdline.left) left = left_border;
    else left = 0;
    if (cmdline.right) right = right_border;
    else right = cols-1;
    if (cmdline.top) top = top_border;
    else top = 0;
    if (cmdline.bottom) bottom = bottom_border;
    else bottom = rows-1;
        
    if (cmdline.verbose) 
        report_cropping_parameters(left, right, top, bottom, rows, cols);

    {
        /* Now we just do a Pnmcut */
        int row;
        int newcols, newrows;
        xel *xelrow;

        xelrow = pnm_allocrow(cols);

	overflow_add(right, 1);
	overflow_add(bottom, 1);
        newcols = right - left + 1;
        newrows = bottom - top + 1;
        pnm_writepnminit(stdout, newcols, newrows, maxval, format, 0);
        for (row = 0; row < rows; row++) {
            pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
            if (row >= top && row <= bottom)
                pnm_writepnmrow(stdout, &(xelrow[left]), newcols, 
                                maxval, format, 0);
        }
        pnm_freerow(xelrow);
    }

    pm_close(stdout);
    pm_close(ifp);

    exit(0);
    }
