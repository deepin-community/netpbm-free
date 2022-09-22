/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, David Koblas.                                     | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

#define _USE_BSD    /* Make sure strcasecmp() is in string.h */
#include <string.h>

#include "pnm.h"
#include "pgm.h"
#include "ppm.h"

enum horizPos {LEFT, CENTER, RIGHT, NOHORIZ};
enum vertPos {TOP, MIDDLE, BOTTOM, NOVERT};


struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *underlyingFilespec;  /* '-' if stdin */
    char *overlayFilespec;
    char *alphaFilespec;
    char *outputFilespec;  /* '-' if stdout */
    int xoff, yoff;   /* value of xoff, yoff options */
    unsigned int alphaInvert;
    enum horizPos align;
    enum vertPos valign;
};




static void
parseCommandLine ( int argc, char ** argv,
                   struct cmdlineInfo *cmdlineP ) {
/*----------------------------------------------------------------------------
   parse program command line described in Unix standard form by argc
   and argv.  Return the information in the options as *cmdlineP.  

   If command line is internally inconsistent (invalid options, etc.),
   issue error message to stderr and abort program.

   Note that the strings we return are stored in the storage that
   was passed to us as the argv array.  We also trash *argv.
-----------------------------------------------------------------------------*/
    optEntry *option_def = malloc( 100*sizeof( optEntry ) );
        /* Instructions to optParseOptions3 on how to parse our options.
         */
    optStruct3 opt;

    unsigned int option_def_index;

    char *align, *valign;
    unsigned int xoffSpec, yoffSpec, alignSpec, valignSpec;

    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0, "invert",     OPT_FLAG,   NULL,                  
            &cmdlineP->alphaInvert,       0 );
    OPTENT3(0, "xoff",       OPT_INT,    &cmdlineP->xoff,       
            &xoffSpec,       0 );
    OPTENT3(0, "yoff",       OPT_INT,    &cmdlineP->yoff,       
            &yoffSpec,       0 );
    OPTENT3(0, "alpha",      OPT_STRING, &cmdlineP->alphaFilespec,
            NULL,  0 );
    OPTENT3(0, "align",      OPT_STRING, &align,
            &alignSpec,  0 );
    OPTENT3(0, "valign",     OPT_STRING, &valign,
            &valignSpec,  0 );

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    /* set defaults */
    cmdlineP->alphaFilespec = NULL;   
    cmdlineP->xoff = 0;
    cmdlineP->yoff = 0;

    pm_optParseOptions3( &argc, argv, opt, sizeof(opt), 0 );
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */


    if (xoffSpec & alignSpec)
        pm_error("You can't specify both xoff and align options");
    if (yoffSpec & valignSpec)
        pm_error("You can't specify both yoff and valign options");

    if (alignSpec) {
        if (strcasecmp(align, "LEFT") == 0)
            cmdlineP->align = LEFT;
        else if (strcasecmp(align, "CENTER") == 0)
            cmdlineP->align = CENTER;
        else if (strcasecmp(align, "RIGHT") == 0)
            cmdlineP->align = RIGHT;
        else
            pm_error("Invalid value for align option: '%s'.  Only LEFT, "
                     "RIGHT, and CENTER are valid.", align);
    } else 
        cmdlineP->align = NOHORIZ;

    if (valignSpec) {
        if (strcasecmp(valign, "TOP") == 0)
            cmdlineP->valign = TOP;
        else if (strcasecmp(valign, "MIDDLE") == 0)
            cmdlineP->valign = MIDDLE;
        else if (strcasecmp(valign, "BOTTOM") == 0)
            cmdlineP->valign = BOTTOM;
        else
            pm_error("Invalid value for valign option: '%s'.  Only TOP, "
                     "BOTTOM, and MIDDLE are valid.", align);
    } else 
        cmdlineP->valign = NOVERT;


    if (argc-1 < 1)
        pm_error("Need at least one argument: file specification of the "
                 "overlay image.");

    cmdlineP->overlayFilespec = argv[1];

    if (argc-1 >= 2)
        cmdlineP->underlyingFilespec = argv[2];
    else
        cmdlineP->underlyingFilespec = "-";

    if (argc-1 >= 3)
        cmdlineP->outputFilespec = argv[3];
    else
        cmdlineP->outputFilespec = "-";

    if (argc-1 > 3)
        pm_error("Too many arguments.  Only acceptable arguments are: "
                 "overlay image, underlying image, output image");
}




static void
computeOverlayPosition(const int cols, const int rows,
                       const int imageCols, const int imageRows,
                       const struct cmdlineInfo cmdline, 
                       int * const originLeftP,
                       int * const originTopP) {
/*----------------------------------------------------------------------------
   Determine where to overlay the overlay image, based on the options the
   user specified and the realities of the image dimensions.

   The origin may be outside the underlying image (so e..g *originLeftP may
   be negative or > image width).  That means not all of the overlay image
   actually gets used.  In fact, there may be no overlap at all.
-----------------------------------------------------------------------------*/

    switch (cmdline.align) {
    case NOHORIZ:
        *originLeftP = cmdline.xoff;
        break;
    case LEFT:
        *originLeftP = 0;
        break;
    case RIGHT:
        *originLeftP = cols-imageCols;
        break;
    case CENTER:
        *originLeftP = (cols-imageCols)/2;
        break;
    }
    switch (cmdline.valign) {
    case NOVERT:
        *originTopP = cmdline.yoff;
        break;
    case TOP:
        *originTopP = 0;
        break;
    case BOTTOM:
        *originTopP = rows-imageRows;
        break;
    case MIDDLE:
        *originTopP = (rows-imageRows)/2;
        break;
    }
}



static void
composite(const int originleft, const int origintop, 
          pixel ** const image, const int imageCols, const int imageRows,
          const xelval imageMax, const int imageType,
          const int cols, const int rows, const xelval maxval, const int type,
          gray ** alpha, const pixval alphaMax, const int InvertFlag,
          FILE *ifp, FILE *ofp) {
/*----------------------------------------------------------------------------
   Overlay the overlay image 'image' onto the underlying image which is in
   file 'ifp', and output the composite to file 'ofp'.

   The underlying image file 'ifp' is positioned after its header.  The
   width, height, format, and maxval of the underlying image are 'cols',
   'rows', 'type', and 'maxval'.

   The width, height, format, and maxval of the overlay image are
   imageCols, imageRows, imageType and imageMax.

   'originleft' and 'origintop' are the coordinates in the underlying
   image plane where the top left corner of the overlay image is
   to go.  It is not necessarily inside the underlying image (in fact,
   may be negative).  Only the part of the overlay that actually intersects
   the underlying image, if any, gets into the output.

   Note that we modify the overlay image 'image' to change its format and
   maxval to the format and maxval of the output.
-----------------------------------------------------------------------------*/
        int     x, y, x0, y0;
        int     r,g,b;
        xel     *pixels;
        double  f;
        xelval  omaxv;
        int     otype;

        pixels = pnm_allocrow(cols);

        /*
        **  Convert overlay image to common type & maxval
        */
        otype = (imageType < type) ? type : imageType;
        omaxv = pm_lcm(maxval, imageMax, 1, PNM_OVERALLMAXVAL);

        if (imageType != otype || imageMax != omaxv) {
                pnm_promoteformat(image, imageCols, imageRows,
                                imageMax, imageType, omaxv, otype);
        }

        pnm_writepnminit(ofp, cols, rows, omaxv, otype, 0);

        for (y = 0; y < rows; y++) {
                /*
                **  Read a row and convert it to the output type
                */
                pnm_readpnmrow(ifp, pixels, cols, maxval, type);

                if (type != otype || maxval != omaxv)
                        pnm_promoteformatrow(pixels, cols, maxval,
                                                type, omaxv, otype);

                /*
                **  Now overlay the overlay with alpha (if defined)
                */
                for (x = 0; x < cols; x++) {
                        x0 = x - originleft;
                        y0 = y - origintop;

                        if (x0 < 0 || x0 >= imageCols)
                                continue;
                        if (y0 < 0 || y0 >= imageRows)
                                continue;

                        if (alpha == NULL) {
                                f = 1.0;
                        } else {
                                f = (double)alpha[y0][x0] / (double)alphaMax;
                                if (InvertFlag)
                                        f = 1.0 - f;
                        }

                        r = PPM_GETR(pixels[x])     * (1.0 - f) +
                            PPM_GETR(image[y0][x0]) * f;
                        g = PPM_GETG(pixels[x])     * (1.0 - f) +
                            PPM_GETG(image[y0][x0]) * f;
                        b = PPM_GETB(pixels[x])     * (1.0 - f) +
                            PPM_GETB(image[y0][x0]) * f;

                        PPM_ASSIGN(pixels[x], r, g, b);
                }

                pnm_writepnmrow(ofp, pixels, cols, omaxv, otype, 0);
        }

        pnm_freerow(pixels);
}



int
main(int argc, char *argv[]) {

        FILE    *ifp, *ofp;
        pixel   **image;
        int     imageCols, imageRows, imageType;
        xelval  imageMax;
        int     cols, rows, type;
        xelval  maxval;
        gray    **alpha;
        int     alphaCols, alphaRows;
        xelval  alphaMax;
        struct cmdlineInfo cmdline;
        int originLeft, originTop;

        pnm_init(&argc, argv);

        parseCommandLine(argc, argv, &cmdline);
        
        { /* Read the overlay image into 'image' */
            FILE *fp;
            fp = pm_openr(cmdline.overlayFilespec);
            image = 
                pnm_readpnm(fp, &imageCols, &imageRows, &imageMax, &imageType);
            pm_close(fp);
        }

        if (cmdline.alphaFilespec) {
            /* Read the alpha mask file into 'alpha' */
            FILE *fp = pm_openr(cmdline.alphaFilespec);
            alpha = pgm_readpgm(fp, &alphaCols, &alphaRows, &alphaMax);
            pm_close(fp);
            
            if (imageCols != alphaCols || imageRows != alphaRows)
                pm_error("Alpha map and overlay image are not the same size");
        } else
            alpha = NULL;

        ifp = pm_openr(cmdline.underlyingFilespec);

        ofp = pm_openw(cmdline.outputFilespec);

        pnm_readpnminit(ifp, &cols, &rows, &maxval, &type);

        computeOverlayPosition(cols, rows, imageCols, imageRows, 
                               cmdline, &originLeft, &originTop);

        composite(originLeft, originTop,
                  image, imageCols, imageRows, imageMax, imageType, 
                  cols, rows, maxval, type, 
                  alpha, alphaMax, cmdline.alphaInvert,
                  ifp, ofp);

        pm_close(ifp);
        pm_close(ofp);

        /* If the program failed, it previously aborted with nonzero completion
           code, via various function calls.
        */
        return 0;
}


