/* pamcut.c - cut a rectangle out of a PAM (portable arbitrary map) image
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

#include <limits.h>
#include "pam.h"

#define UNSPEC INT_MAX
    /* UNSPEC is the value we use for an argument that is not specified
       by the user.  Theoretically, the user could specify this value,
       but we hope not.
       */

struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *inputFilespec;  /* Filespecs of input files */

    /* The following describe the rectangle the user wants to cut out. 
       the value UNSPEC for any of them indicates that value was not
       specified.  A negative value means relative to the far edge.
       'width' and 'height' are not negative.  These specifications 
       do not necessarily describe a valid rectangle; they are just
       what the user said.
       */
    int left;
    int right;
    int top;
    int bottom;
    int width;
    int height;
    unsigned int pad;

    unsigned int verbose;
};



tuple blackTuple;  
   /* A "black" tuple.  Unless our input image is PBM, PGM, or PPM, we
      don't really know what "black" means, so this is just something
      arbitrary in that case.
      */


static void
parseCommandLine(int argc, char ** argv,
                 struct cmdlineInfo *cmdlineP) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optEntry *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions2 on how to parse our options.
         */
    optStruct3 opt;

    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENT3(0,   "left",       OPT_INT,    &cmdlineP->left,     NULL,      0);
    OPTENT3(0,   "right",      OPT_INT,    &cmdlineP->right,    NULL,      0);
    OPTENT3(0,   "top",        OPT_INT,    &cmdlineP->top,      NULL,      0);
    OPTENT3(0,   "bottom",     OPT_INT,    &cmdlineP->bottom,   NULL,      0);
    OPTENT3(0,   "width",      OPT_INT,    &cmdlineP->width,    NULL,      0);
    OPTENT3(0,   "height",     OPT_INT,    &cmdlineP->height,   NULL,      0);
    OPTENT3(0,   "pad",        OPT_FLAG,   NULL, &cmdlineP->pad,           0);
    OPTENT3(0,   "verbose",    OPT_FLAG,   NULL, &cmdlineP->verbose,       0);

    /* Set the defaults */
    cmdlineP->left = UNSPEC;
    cmdlineP->right = UNSPEC;
    cmdlineP->top = UNSPEC;
    cmdlineP->bottom = UNSPEC;
    cmdlineP->width = UNSPEC;
    cmdlineP->height = UNSPEC;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = TRUE;  /* We may have parms that are negative numbers */

    pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);
        /* Uses and sets argc, argv, and some of *cmdlineP and others. */

    if (cmdlineP->width < 0)
        pm_error("-width may not be negative.");
    if (cmdlineP->height < 0)
        pm_error("-height may not be negative.");

    if ((argc-1) != 0 && (argc-1) != 1 && (argc-1) != 4 && (argc-1) != 5)
        pm_error("Wrong number of arguments.  "
                 "Must be 0, 1, 4, or 5 arguments.");

    switch (argc-1) {
    case 0:
        cmdlineP->inputFilespec = "-";
        break;
    case 1:
        cmdlineP->inputFilespec = argv[1];
        break;
    case 4:
    case 5: {
        int warg, harg;  /* The "width" and "height" command line arguments */

        if (sscanf(argv[1], "%d", &cmdlineP->left) != 1)
            pm_error("Invalid number for left column argument");
        if (sscanf(argv[2], "%d", &cmdlineP->top) != 1)
            pm_error("Invalid number for right column argument");
        if (sscanf(argv[3], "%d", &warg) != 1)
            pm_error("Invalid number for width argument");
        if (sscanf(argv[4], "%d", &harg) != 1)
            pm_error("Invalid number for height argument");

        if (warg > 0) {
            cmdlineP->width = warg;
            cmdlineP->right = UNSPEC;
        } else {
            cmdlineP->width = UNSPEC;
            cmdlineP->right = warg -1;
        }
        if (harg > 0) {
            cmdlineP->height = harg;
            cmdlineP->bottom = UNSPEC;
        } else {
            cmdlineP->height = UNSPEC;
            cmdlineP->bottom = harg - 1;
        }

        if (argc-1 == 4)
            cmdlineP->inputFilespec = "-";
        else
            cmdlineP->inputFilespec = argv[5];
        break;
    }
    }
}



static void
computeCutBounds(const int cols, const int rows,
                 const int leftarg, const int rightarg, 
                 const int toparg, const int bottomarg,
                 const int widtharg, const int heightarg,
                 int * const leftcolP, int * const rightcolP,
                 int * const toprowP, int * const bottomrowP) {
/*----------------------------------------------------------------------------
   From the values given on the command line 'leftarg', 'rightarg',
   'toparg', 'bottomarg', 'widtharg', and 'heightarg', determine what
   rectangle the user wants cut out.

   Any of these arguments may be UNSPEC to indicate "not specified".
   Any except 'widtharg' and 'heightarg' may be negative to indicate
   relative to the far edge.  'widtharg' and 'heightarg' are positive.

   Return the location of the rectangle as *leftcolP, *rightcolP,
   *toprowP, and *bottomrowP.  
-----------------------------------------------------------------------------*/

    int leftcol, rightcol, toprow, bottomrow;
        /* The left and right column numbers and top and bottom row numbers
           specified by the user, except with negative values translated
           into the actual values.

           Note that these may very well be negative themselves, such
           as when the user says "column -10" and there are only 5 columns
           in the image.
           */

    /* Translate negative column and row into real column and row */
    /* Exploit the fact that UNSPEC is a positive number */

    if (leftarg >= 0)
        leftcol = leftarg;
    else
        leftcol = cols + leftarg;
    if (rightarg >= 0)
        rightcol = rightarg;
    else
        rightcol = cols + rightarg;
    if (toparg >= 0)
        toprow = toparg;
    else
        toprow = rows + toparg;
    if (bottomarg >= 0)
        bottomrow = bottomarg;
    else
        bottomrow = rows + bottomarg;

    /* Sort out left, right, and width specifications */

    if (leftcol == UNSPEC && rightcol == UNSPEC && widtharg == UNSPEC) {
        *leftcolP = 0;
        *rightcolP = cols - 1;
    }
    if (leftcol == UNSPEC && rightcol == UNSPEC && widtharg != UNSPEC) {
        *leftcolP = 0;
        *rightcolP = 0 + widtharg - 1;
    }
    if (leftcol == UNSPEC && rightcol != UNSPEC && widtharg == UNSPEC) {
        *leftcolP = 0;
        *rightcolP = rightcol;
    }
    if (leftcol == UNSPEC && rightcol != UNSPEC && widtharg != UNSPEC) {
        *leftcolP = rightcol - widtharg + 1;
        *rightcolP = rightcol;
    }
    if (leftcol != UNSPEC && rightcol == UNSPEC && widtharg == UNSPEC) {
        *leftcolP = leftcol;
        *rightcolP = cols - 1;
    }
    if (leftcol != UNSPEC && rightcol == UNSPEC && widtharg != UNSPEC) {
        *leftcolP = leftcol;
        *rightcolP = leftcol + widtharg - 1;
    }
    if (leftcol != UNSPEC && rightcol != UNSPEC && widtharg == UNSPEC) {
        *leftcolP = leftcol;
        *rightcolP = rightcol;
    }
    if (leftcol != UNSPEC && rightcol != UNSPEC && widtharg != UNSPEC) {
        pm_error("You may not specify left, right, and width.\n"
                 "Choose at most two of these.");
    }


    /* Sort out top, bottom, and height specifications */

    if (toprow == UNSPEC && bottomrow == UNSPEC && heightarg == UNSPEC) {
        *toprowP = 0;
        *bottomrowP = rows - 1;
    }
    if (toprow == UNSPEC && bottomrow == UNSPEC && heightarg != UNSPEC) {
        *toprowP = 0;
        *bottomrowP = 0 + heightarg - 1;
    }
    if (toprow == UNSPEC && bottomrow != UNSPEC && heightarg == UNSPEC) {
        *toprowP = 0;
        *bottomrowP = bottomrow;
    }
    if (toprow == UNSPEC && bottomrow != UNSPEC && heightarg != UNSPEC) {
        *toprowP = bottomrow - heightarg + 1;
        *bottomrowP = bottomrow;
    }
    if (toprow != UNSPEC && bottomrow == UNSPEC && heightarg == UNSPEC) {
        *toprowP = toprow;
        *bottomrowP = rows - 1;
    }
    if (toprow != UNSPEC && bottomrow == UNSPEC && heightarg != UNSPEC) {
        *toprowP = toprow;
        *bottomrowP = toprow + heightarg - 1;
    }
    if (toprow != UNSPEC && bottomrow != UNSPEC && heightarg == UNSPEC) {
        *toprowP = toprow;
        *bottomrowP = bottomrow;
    }
    if (toprow != UNSPEC && bottomrow != UNSPEC && heightarg != UNSPEC) {
        pm_error("You may not specify top, bottom, and height.\n"
                 "Choose at most two of these.");
    }

}



static void
rejectOutOfBounds(const int cols, const int rows, 
                  const int leftcol, const int rightcol, 
                  const int toprow, const int bottomrow) {

    /* Reject coordinates off the edge */

    if (leftcol < 0)
        pm_error("You have specified a left edge (%d) that is beyond\n"
                 "the left edge of the image (0)", leftcol);
    if (rightcol > cols-1)
        pm_error("You have specified a right edge (%d) that is beyond\n"
                 "the right edge of the image (%d)", rightcol, cols-1);
    if (rightcol < 0)
        pm_error("You have specified a right edge (%d) that is beyond\n"
                 "the left edge of the image (0)", rightcol);
    if (rightcol > cols-1)
        pm_error("You have specified a right edge (%d) that is beyond\n"
                 "the right edge of the image (%d)", rightcol, cols-1);
    if (leftcol > rightcol) 
        pm_error("You have specified a left edge (%d) that is to the right\n"
                 "of the right edge you specified (%d)", 
                 leftcol, rightcol);
    
    if (toprow < 0)
        pm_error("You have specified a top edge (%d) that is above the top "
                 "edge of the image (0)", toprow);
    if (bottomrow > rows-1)
        pm_error("You have specified a bottom edge (%d) that is below the\n"
                 "bottom edge of the image (%d)", bottomrow, rows-1);
    if (bottomrow < 0)
        pm_error("You have specified a bottom edge (%d) that is above the\n"
                 "top edge of the image (0)", bottomrow);
    if (bottomrow > rows-1)
        pm_error("You have specified a bottom edge (%d) that is below the\n"
                 "bottom edge of the image (%d)", bottomrow, rows-1);
    if (toprow > bottomrow) 
        pm_error("You have specified a top edge (%d) that is below\n"
                 "the bottom edge you specified (%d)", 
                 toprow, bottomrow);
}



static void
writeBlackRows(struct pam outpam, const int rows, tuple * const outputRow){
/*----------------------------------------------------------------------------
   Write out 'rows' rows of black tuples of the image described by 'outpam'.

   Use *output_row as a buffer.  It is wide enough for the 'outpam' image.
-----------------------------------------------------------------------------*/
    int row;
    for (row = 0; row < rows; row++) {
        int col;
        for (col = 0; col < outpam.width; col++) outputRow[col] = blackTuple;
        pnm_writepamrow(&outpam, outputRow);
    }
}



int
main(int argc, char *argv[]) {

    FILE* ifp;
    tuple* inputRow;   /* Row from input image */
    tuple* outputRow;  /* Row of output image */
    int row;
    int leftcol, rightcol, toprow, bottomrow;
    struct cmdlineInfo cmdline;
    struct pam pam;   /* Input PAM image */
    struct pam outpam;  /* Output PAM image */

    pnm_init( &argc, argv );

    parseCommandLine(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.inputFilespec);

    pnm_readpaminit(ifp, &pam, sizeof(pam));

    inputRow = pnm_allocpamrow(&pam);

    createBlackTuple(&pam, &blackTuple);

    computeCutBounds(pam.width, pam.height, 
                     cmdline.left, cmdline.right, 
                     cmdline.top, cmdline.bottom, 
                     cmdline.width, cmdline.height, 
                     &leftcol, &rightcol, &toprow, &bottomrow);

    if (!cmdline.pad)
        rejectOutOfBounds(pam.width, pam.height, leftcol, rightcol, 
                          toprow, bottomrow);

    if (cmdline.verbose) {
        pm_message("Image goes from Row 0, Column 0 through Row %d, Column %d",
                   pam.height-1, pam.width-1);
        pm_message("Cutting from Row %d, Column %d through Row %d Column %d",
                   toprow, leftcol, bottomrow, rightcol);
    }

    outpam = pam;    /* Initial value -- most fields should be same */
    outpam.file = stdout;
    outpam.width = rightcol-leftcol+1;
    outpam.height = bottomrow-toprow+1;

    overflow_add(rightcol, 1);
    overflow_add(toprow, 1);
    pnm_writepaminit(&outpam);

    outputRow = pnm_allocpamrow(&outpam);

    /* Implementation note:  If speed is ever an issue, we can probably
       speed up significantly the non-padding case by writing a special
       case loop here for the case cmdline.pad == FALSE.
       */

    /* Write out top padding */
    writeBlackRows(outpam, 0 - toprow, outputRow);
    
    /* Read input and write out rows extracted from it */
    for (row = 0; row < pam.height; row++) {
        pnm_readpamrow(&pam, inputRow);
        if (row >= toprow && row <= bottomrow) {
            int col;
            /* Put in left padding */
            for (col = leftcol; col < 0; col++) { 
                outputRow[col-leftcol] = blackTuple;
            }
            /* Put in extracted columns */
            for (col = max(leftcol, 0); col <= min(rightcol, pam.width-1); 
                 col++) {
                outputRow[col-leftcol] = inputRow[col];
            }
            /* Put in right padding */
            for (col = max(pam.width, leftcol); col <= rightcol; col++) {
                outputRow[col-leftcol] = blackTuple;
            }
            pnm_writepamrow(&outpam, outputRow);
        }
    }
    /* Note that we may be tempted just to quit after reaching the bottom
       of the extracted image, but that would cause a broken pipe problem
       for the process that's feeding us the image.
       */
    /* Write out bottom padding */
    writeBlackRows(outpam, bottomrow - (pam.height-1), outputRow);

    pnm_freepamtuple(blackTuple);
    pnm_freepamrow(outputRow);
    pnm_freepamrow(inputRow);
    pm_close(pam.file);
    pm_close(outpam.file);
    
    exit(0);
}

