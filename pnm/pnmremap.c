/******************************************************************************
                               pnmremap.c
*******************************************************************************

  Replace colors in an input image with colors from a given colormap image.

  For PGM input, do the equivalent.

  By Bryan Henderson, San Jose, CA 2001.12.17

  Derived from ppmquant, originally by Jef Poskanzer.

  Copyright (C) 1989, 1991 by Jef Poskanzer.
  Copyright (C) 2001 by Bryan Henderson.

  Permission to use, copy, modify, and distribute this software and its
  documentation for any purpose and without fee is hereby granted, provided
  that the above copyright notice appear in all copies and that both that
  copyright notice and this permission notice appear in supporting
  documentation.  This software is provided "as is" without express or
  implied warranty.
******************************************************************************/

#include <limits.h>

#include "pam.h"
#include "pammap.h"

#define MAXCOLORS 32767u

enum missingMethod {
    MISSING_FIRST,
    MISSING_SPECIFIED,
    MISSING_CLOSE
};

#define FS_SCALE 1024

struct fserr {
    long** thiserr;
    long** nexterr;
    bool fsForward;
};


struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *inputFilespec;  /* Filespec of input file */
    char *mapFilespec;    /* Filespec of colormap file; NULL if none */
    unsigned int floyd;   /* Boolean: -floyd/-fs option */
    enum missingMethod missingMethod;
    char * missingcolor;      
        /* -missingcolor value.  Null if not specified */
    unsigned int verbose;
};



static int __inline__ sqr(const int x) {
    return x*x;
}



static void
parseCommandLine (int argc, char ** argv,
                  struct cmdlineInfo *cmdlineP) {
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

    unsigned int nofloyd, firstisdefault;
    unsigned int missingSpec, mapfileSpec;
    
    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0,   "floyd",        OPT_FLAG,   
            NULL,                       &cmdlineP->floyd, 0);
    OPTENT3(0,   "fs",           OPT_FLAG,   
            NULL,                       &cmdlineP->floyd, 0);
    OPTENT3(0,   "nofloyd",      OPT_FLAG,   
            NULL,                       &nofloyd, 0);
    OPTENT3(0,   "nofs",         OPT_FLAG,   
            NULL,                       &nofloyd, 0);
    OPTENT3(0,   "firstisdefault", OPT_FLAG,   
            NULL,                       &firstisdefault, 0);
    OPTENT3(0,   "mapfile",      OPT_STRING, 
            &cmdlineP->mapFilespec,    &mapfileSpec, 0);
    OPTENT3(0,   "missingcolor", OPT_STRING, 
            &cmdlineP->missingcolor,   &missingSpec, 0);
    OPTENT3(0, "verbose",        OPT_FLAG,   NULL,                  
            &cmdlineP->verbose,        0 );

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    cmdlineP->missingcolor = NULL;  /* default value */
    
    pm_optParseOptions3( &argc, argv, opt, sizeof(opt), 0 );
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (cmdlineP->floyd && nofloyd)
        pm_error("You cannot specify both -floyd and -nofloyd options.");

    if (firstisdefault && missingSpec)
        pm_error("You cannot specify both -missing and -firstisdefault.");

    if (firstisdefault)
        cmdlineP->missingMethod = MISSING_FIRST;
    else if (missingSpec)
        cmdlineP->missingMethod = MISSING_SPECIFIED;
    else
        cmdlineP->missingMethod = MISSING_CLOSE;

    if (!mapfileSpec)
        pm_error("You must specify the -mapfile option.");

    if (argc-1 > 1)
        pm_error("Program takes at most one argument: the input file "
                 "specification.  "
                 "You specified %d arguments.", argc-1);
    if (argc-1 < 1)
        cmdlineP->inputFilespec = "-";
    else
        cmdlineP->inputFilespec = argv[1];
}



static void
adjustDepth(struct pam * const pamP,
            tuple *      const tupleRow,
            unsigned int const newDepth) {
/*----------------------------------------------------------------------------
   Change the depth of the raster row tupleRow[] of the image
   described by 'pamP' to newDepth to match a user-supplied color map.

   The only depth change we allow is from an RGB image of depth 3 to
   depth 1.  In that case, we change it to grayscale or black and white.
   tuplerow[] remains allocated for the depth of 3, but only the first
   plane is meaningful after this.

   For any other depth change request, we issue an error message and abort
   the program.  (It is technically possible to go from grayscale to RGB,
   but there seems to be little requirement for that, so we save the 
   programming effort).
-----------------------------------------------------------------------------*/
    if (newDepth != pamP->depth) {

        if (!stripeq(pamP->tuple_type, "RGB"))
            pm_error("Map file depth of %d is incompatible with image "
                     "tuple type of '%*s'.  Only type RGB images can have "
                     "their depths changed.", newDepth, 
                     sizeof(pamP->tuple_type), pamP->tuple_type);
        else if (newDepth != 1)
            pm_error("Map file depth is %d, while image depth is %d.  "
                     "The only depth change allowed is from 3 to 1.",
                     newDepth, pamP->depth);
        else {
            int col;
            for (col = 0; col < pamP->width; ++col) {
                unsigned int plane;
                double grayvalue;
                grayvalue = 0.0;  /* initial value */
                for (plane = 0; plane < pamP->depth; ++plane)
                    grayvalue += 
                        pnm_lumin_factor[plane] * tupleRow[col][plane];
                tupleRow[col][0] = (int) (grayvalue + 0.5);
            }
        }
    }
}




static void
computeColorMapFromMap(struct pam *   const mappamP, 
                       tuple **       const maptuples, 
                       tupletable *   const colormapP,
                       unsigned int * const newcolorsP) {
/*----------------------------------------------------------------------------
   Produce a colormap containing the colors that we will use in the output.

   Make it include exactly those colors that are in the image
   described by *mappamP and maptuples[][].

   Return the number of colors in the returned colormap as *newcolorsP.
-----------------------------------------------------------------------------*/
    unsigned int colors; 

    if (mappamP->width == 0 || mappamP->height == 0)
        pm_error("colormap file contains no pixels");

    *colormapP = 
        pnm_computetuplefreqtable(mappamP, maptuples, MAXCOLORS, &colors);
    if (*colormapP == NULL)
        pm_error("too many colors in colormap!");
    pm_message("%d colors found in colormap", colors);
    *newcolorsP = colors;
}



static void
initFserr(struct pam * const pamP, struct fserr * const fserrP) {
/*----------------------------------------------------------------------------
   Initialize the Floyd-Steinberg error vectors
-----------------------------------------------------------------------------*/
    unsigned int plane;

    unsigned int const fserrSize = pamP->width + 2;
	overflow_add(pamP->width, 2);

    fserrP->thiserr = malloc2(pamP->depth , sizeof(long*));
    if (fserrP->thiserr == NULL)
        pm_error("Out of memory allocating Floyd-Steinberg structures");
    fserrP->nexterr = malloc2(pamP->depth , sizeof(long*));
    if (fserrP->nexterr == NULL)
        pm_error("Out of memory allocating Floyd-Steinberg structures");
    
    for (plane = 0; plane < pamP->depth; ++plane) {
        fserrP->thiserr[plane] = (long*) malloc2(fserrSize, sizeof(long));
        if (fserrP->thiserr[plane] == NULL)
            pm_error("Out of memory allocating Floyd-Steinberg structures");
        fserrP->nexterr[plane] = (long*) malloc2(fserrSize, sizeof(long));
        if (fserrP->nexterr[plane] == NULL)
            pm_error("Out of memory allocating Floyd-Steinberg structures");
    }

    srand((int)(time(0) ^ getpid()));

    {
        int col;

        for (col = 0; col < fserrSize; ++col) {
            unsigned int plane;
            for (plane = 0; plane < pamP->depth; ++plane) 
                fserrP->thiserr[plane][col] = 
                    rand() % (FS_SCALE * 2) - FS_SCALE;
                    /* (random errors in [-1 .. 1]) */
        }
    }
    fserrP->fsForward = TRUE;
}



static void
floydInitRow(struct pam * const pamP, struct fserr * const fserrP) {

    int col;
    
    for (col = 0; col < pamP->width + 2; ++col) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane) 
            fserrP->nexterr[plane][col] = 0;
    }
}



static void
floydAdjustColor(struct pam *   const pamP, 
                 tuple          const tuple, 
                 struct fserr * const fserrP, 
                 int            const col) {
/*----------------------------------------------------------------------------
  Use Floyd-Steinberg errors to adjust actual color.
-----------------------------------------------------------------------------*/
    unsigned int plane;

    for (plane = 0; plane < pamP->depth; ++plane) {
        long int const s =
            tuple[plane] + fserrP->thiserr[plane][col+1] / FS_SCALE;
        tuple[plane] = min(pamP->maxval, max(0,s));
    }
}



static void
floydPropagateErr(struct pam *   const inpamP, 
                  struct fserr * const fserrP, 
                  int            const col, 
                  tuple          const oldtuple, 
                  struct pam *   const outpamP,
                  tuple          const newtuple) {
/*----------------------------------------------------------------------------
  Propagate Floyd-Steinberg error terms.

  The error is due to substituting the tuple value 'newtuple'
  (described by *inpamP) for the tuple value 'oldtuple' (described by
  *oldpamP).  The error terms are meant to be used to introduce a
  compensating error into the future selection of tuples nearby in the
  image.

  We assume *inpamP and *outpamP specify the same tuple depth.
-----------------------------------------------------------------------------*/
    float const maxvalNormalizer = inpamP->maxval / outpamP->maxval;
    
    unsigned int plane;
    for (plane = 0; plane < inpamP->depth; ++plane) {
        long const newSampleNormalized = (long)
            (maxvalNormalizer*newtuple[plane] + 0.5);
        long const oldSample = oldtuple[plane];
        long const err = (oldSample - newSampleNormalized) * FS_SCALE;
            
        if (fserrP->fsForward) {
            fserrP->thiserr[plane][col + 2] += ( err * 7 ) / 16;
            fserrP->nexterr[plane][col    ] += ( err * 3 ) / 16;
            fserrP->nexterr[plane][col + 1] += ( err * 5 ) / 16;
            fserrP->nexterr[plane][col + 2] += ( err     ) / 16;
        } else {
            fserrP->thiserr[plane][col    ] += ( err * 7 ) / 16;
            fserrP->nexterr[plane][col + 2] += ( err * 3 ) / 16;
            fserrP->nexterr[plane][col + 1] += ( err * 5 ) / 16;
            fserrP->nexterr[plane][col    ] += ( err     ) / 16;
        }
    }
}



static void
floydSwitchDir(struct pam * const pamP, struct fserr * const fserrP) {

    unsigned int plane;

    for (plane = 0; plane < pamP->depth; ++plane) {
        long * const temperr = fserrP->thiserr[plane];
        fserrP->thiserr[plane] = fserrP->nexterr[plane];
        fserrP->nexterr[plane] = temperr;
    }
    fserrP->fsForward = ! fserrP->fsForward;
}



static void
searchColormapCloseSimple(struct pam * const pamP,
                          tupletable   const colormap, 
                          int          const colors,
                          tuple        const tuple,
                          int *        const colormapIndexP) {

    unsigned int i;
    unsigned long dist;
    dist = ULONG_MAX;
    for (i = 0; i < colors; ++i) {
        unsigned long newdist;  /* candidate for new 'dist' value */
        unsigned int plane;

        newdist = 0;
        for (plane=0; plane < pamP->depth; ++plane) 
            newdist += sqr(tuple[plane] - colormap[i]->tuple[plane]);
        if (newdist < dist) {
            *colormapIndexP = i;
            dist = newdist;
        }
    }
}



static void
searchColormapCloseComplex(struct pam * const pamP,
                           tupletable   const colormap, 
                           int          const colors,
                           sample       const colormapMaxval,
                           tuple        const tuple,
                           int *        const colormapIndexP) {

    unsigned int i;
    unsigned int const divider = colormapMaxval < 256 ? 1 : 2;
    /* We divide our sum by this to make sure it fits in a "long" */
    unsigned long dist;
    /* The closest distance we've found so far between the value of
           tuple 'tuple' and a tuple in the colormap.  This is measured as
           the square of the cartesian distance between the tuples, except
           that it's divided by 'divider' (above) to make sure it will fit
           in a 'long' (minimum: 32 bits).  Note that each sample can be
           up to 65535 (PPM_OVERALLMAXVAL).  
        */
    float const maxvalNormalizer = (float) colormapMaxval/pamP->maxval;

    dist = ULONG_MAX;  /* initial value */

    for (i = 0; i < colors; ++i) {
        unsigned long newdist;  /* candidate for new 'dist' value */
        unsigned int plane;

        newdist = 0;

        for (plane=0; plane < pamP->depth; ++plane) {
            sample const normalizedSample = (sample)
                (tuple[plane] * maxvalNormalizer) + 0.5;
            
            newdist += sqr(normalizedSample - colormap[i]->tuple[plane])
                / divider;
        }
        if (newdist < dist) {
            *colormapIndexP = i;
            dist = newdist;
        }
    }
}



static void
searchColormapClose(struct pam * const pamP,
                    tupletable   const colormap, 
                    int          const colors,
                    sample       const colormapMaxval,
                    tuple        const tuple,
                    int *        const colormapIndexP) {
/*----------------------------------------------------------------------------
   Search colormap 'colormap', which contains 'colors' colors (at
   least 1) for the color closest to that of tuple 'tuple'.  Return
   its index as *colormapIndexP.
-----------------------------------------------------------------------------*/
    if (pamP->maxval == colormapMaxval && colormapMaxval < 256) 
        /* Do a fast computation for this usual case */
        searchColormapCloseSimple(pamP, colormap, colors, 
                                  tuple, colormapIndexP);
    else 
        /* Do a general computation that works for all maxvals */
        searchColormapCloseComplex(pamP, colormap, colors, colormapMaxval,
                                   tuple, colormapIndexP);
}



static void
searchColormapExact(struct pam * const pamP,
                    tupletable   const colormap, 
                    int          const colormapSize,
                    tuple        const tuple,
                    int *        const colormapIndexP,
                    bool *       const foundP) {
/*----------------------------------------------------------------------------
   Search colormap 'colormap', which contains 'colors' colors (at
   least 1) for the color of tuple 'tuple'.  If it's in the map, return its
   index as *colormapIndexP and return *foundP == TRUE.  Otherwise,
   return *foundP = FALSE.

   We assume the maxval of the colormap and of 'tuple' are the same,
   to wit pamP->maxval.  It doesn't make a lot of sense to call this 
   subroutine otherwise, because an exact match would be difficult.
-----------------------------------------------------------------------------*/
    unsigned int i;
    bool found;
    
    found = FALSE;  /* initial value */
    for (i = 0; i < colormapSize && !found; ++i) {
        unsigned int plane;
        found = TRUE;  /* initial assumption */
        for (plane=0; plane < pamP->depth; ++plane) 
            if (tuple[plane] != colormap[i]->tuple[plane]) 
                found = FALSE;
        if (found) 
            *colormapIndexP = i;
    }
    *foundP = found;
}



static void
lookupThroughHash(struct pam * const pamP, 
                  tuple        const tuple, 
                  bool         const needExactMatch,
                  tupletable   const colormap, 
                  unsigned int const colormapSize,
                  sample       const colormapMaxval,
                  tuplehash    const colorhash,       
                  int *        const colormapIndexP,
                  bool *       const usehashP) {
/*----------------------------------------------------------------------------
   Look up the color of tuple 'tuple' in the color map 'colormap' and, if
   it's in there, return its index as *colormapIndexP.  If not, return
   *colormapIndexP == -1.

   If 'needExactMatch' is true, we assume colormapMaxval == pamP->maxval,
   i.e. the colormap and the input image have the same maxval.

   If 'needExactMatch' isn't true, we find the closest color in the color map,
   and never return *colormapIndex == -1.

   lookaside at the hash table 'colorhash' to possibly avoid the cost of
   a full lookup.  If we do a full lookup, we add the result to 'colorhash'
   unless *usehashP is false, and if that makes 'colorhash' full, we set
   *usehashP false.
-----------------------------------------------------------------------------*/
    bool found;

    /* Check hash table to see if we have already matched this color. */
    pnm_lookuptuple(pamP, colorhash, tuple, &found, colormapIndexP);
    if (!found) {
        /* No, have to do a full lookup */
        if (needExactMatch) {
            bool found;

            searchColormapExact(pamP, colormap, colormapSize, tuple,
                                colormapIndexP, &found);
            if (!found)
                *colormapIndexP = -1;
        } else 
            searchColormapClose(pamP, colormap, colormapSize, 
                                colormapMaxval, tuple, colormapIndexP);
    }        
    if (*usehashP) {
        bool fits;
        pnm_addtotuplehash(pamP, colorhash, tuple, *colormapIndexP, 
                           &fits);
        if (!fits) {
            pm_message("out of memory adding to hash table; "
                       "proceeding without it");
            *usehashP = FALSE;
        }
    }
}



static void
convertRow(struct pam * const inpamP, tuple tuplerow[],
           struct pam * const outpamP,
           tupletable const colormap, unsigned int newcolors,
           tuplehash const colorhash, bool *usehashP,
           bool const floyd, enum missingMethod const missingMethod, 
           tuple const defaultColor, struct fserr * const fserrP,
           unsigned int * const missingCountP) {
/*----------------------------------------------------------------------------
  Replace the colors in row tuplerow[] with the new colors.

  Use and update the Floyd-Steinberg state *fserrP.

  Return the number of pixels that were not matched in the color map as
  *missingCountP.
-----------------------------------------------------------------------------*/
    int col;
    int limitcol;
        /* The column at which to stop processing the row.  If we're scanning
           forwards, this is the rightmost column.  If we're scanning 
           backward, this is the leftmost column.
        */
    
    if (floyd)
        floydInitRow(inpamP, fserrP);

    *missingCountP = 0;  /* initial value */
    
    if ((!floyd) || fserrP->fsForward) {
        col = 0;
        limitcol = inpamP->width;
    } else {
        col = inpamP->width - 1;
        limitcol = -1;
    }
    do {
        int colormapIndex;
            /* Index into the colormap of the replacement color, or -1 if
               there is no usable color in the color map.
            */

        if (floyd) 
            floydAdjustColor(inpamP, tuplerow[col], fserrP, col);

        lookupThroughHash(inpamP, tuplerow[col], 
                          missingMethod != MISSING_CLOSE, colormap, 
                          newcolors, outpamP->maxval,
                          colorhash, &colormapIndex, usehashP);
        if (floyd)
            floydPropagateErr(inpamP, fserrP, col, tuplerow[col], 
                              outpamP, colormap[colormapIndex]->tuple);

        if (colormapIndex == -1) {
            ++*missingCountP;
            switch (missingMethod) {
            case MISSING_SPECIFIED:
                pnm_assigntuple(outpamP, tuplerow[col], defaultColor);
                break;
            case MISSING_FIRST:
                pnm_assigntuple(outpamP, tuplerow[col], colormap[0]->tuple);
                break;
            default:
                pm_error("Internal error: invalid value of missingMethod");
            }
        } else 
            pnm_assigntuple(outpamP, tuplerow[col], 
                            colormap[colormapIndex]->tuple);

        if (floyd && !fserrP->fsForward) 
            --col;
        else
            ++col;
    } while (col != limitcol);

    if (floyd)
        floydSwitchDir(inpamP, fserrP);
}



static void
copyRaster(struct pam *   const inpamP, 
           struct pam *   const outpamP,
           tupletable     const colormap, 
           unsigned int   const colormapSize,
           bool           const floyd, 
           enum missingMethod const missingMethod,
           tuple          const defaultColor, 
           unsigned int * const missingCountP) {

    tuplehash const colorhash = pnm_createtuplehash();
    bool usehash;
    struct fserr fserr;
    tuple * tuplerow = pnm_allocpamrow(inpamP);
    int row;
    
    if (outpamP->maxval != inpamP->maxval && missingMethod != MISSING_CLOSE)
        pm_error("The maxval of the colormap (%u) is not equal to the "
                 "maxval of the input image (%u).  This allowable only "
                 "if you are doing an approximate mapping (i.e. you don't "
                 "specify -firstisdefault or -missingcolor)",
                 (unsigned int)outpamP->maxval, (unsigned int)inpamP->maxval);

    usehash = TRUE;

    if (floyd)
        initFserr(inpamP, &fserr);

    *missingCountP = 0;  /* initial value */

    for (row = 0; row < inpamP->height; ++row) {
        unsigned int missingCount;

        pnm_readpamrow(inpamP, tuplerow);

        /* The following modify tuplerow, to make it consistent with
           *outpamP instead of *inpamP.
        */
        adjustDepth(inpamP, tuplerow, outpamP->depth); 

        /* The following both consults and adds to 'colorhash' and
           its associated 'usehash'.  It modifies 'tuplerow' too.
        */
        convertRow(inpamP, tuplerow, outpamP, colormap, colormapSize,
                   colorhash, &usehash,
                   floyd, missingMethod, defaultColor, &fserr, &missingCount);
            
        *missingCountP += missingCount;
        
        pnm_writepamrow(outpamP, tuplerow);
    }
    pnm_freepamrow(tuplerow);
    pnm_destroytuplehash(colorhash);
}




int
main(int argc, char * argv[] ) {

    struct cmdlineInfo cmdline;
    FILE* ifp;
    struct pam inpam, outpam;
    tupletable colormap;
    unsigned int colormapSize;
    tuple defaultColor;
        /* A tuple of the color that should replace any input color that is 
           not in the colormap, if we're doing MISSING_SPECIFIED.
        */
    unsigned int missingCount;
        /* Number of pixels that were not matched in the color map (where
           missingMethod is MISSING_CLOSE, this is always zero).
        */

    pnm_init(&argc, argv);

    parseCommandLine(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.inputFilespec);

    pnm_readpaminit(ifp, &inpam, sizeof(inpam));
    
    {
        FILE * mapfile;
        struct pam mappam;
        tuple ** maptuples;

        mapfile = pm_openr(cmdline.mapFilespec);
        maptuples = pnm_readpam(mapfile, &mappam, sizeof(mappam));
        pm_close(mapfile);

        computeColorMapFromMap(&mappam, maptuples, &colormap, &colormapSize);
        pnm_freepamarray(maptuples, &mappam);

        outpam = mappam; 
        outpam.file = stdout;
        outpam.width = inpam.width;
        outpam.height = inpam.height;
    }

    defaultColor = pnm_allocpamtuple(&outpam);
    if (cmdline.missingcolor && outpam.depth == 3) {
        pixel const color = 
            ppm_parsecolor(cmdline.missingcolor, outpam.maxval);
        defaultColor[PAM_RED_PLANE] = PPM_GETR(color);
        defaultColor[PAM_GRN_PLANE] = PPM_GETG(color);
        defaultColor[PAM_BLU_PLANE] = PPM_GETB(color);
    }
    pnm_writepaminit(&outpam);
    
    copyRaster(&inpam, &outpam, colormap, colormapSize, cmdline.floyd,
               cmdline.missingMethod, defaultColor, &missingCount);
    
    if (cmdline.verbose)
        pm_message("%d pixels not matched in color map", missingCount);

    pnm_freepamtuple(defaultColor);

    pm_close(stdout);

    pm_close(ifp);

    exit(0);
}
