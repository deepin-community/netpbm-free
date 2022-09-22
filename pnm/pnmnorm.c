/******************************************************************************
                                  Pnmnorm
*******************************************************************************

  This program normalizes the contrast in a Netpbm image.

  by Bryan Henderson bryanh@giraffe-data.com San Jose CA March 2002.
  Adapted from Ppmnorm.

  Ppmnorm is by Wilson H. Bent, Jr. (whb@usc.edu)
  Extensively hacked from pgmnorm.c, which carries the following note:
  
  Copyright (C) 1989, 1991 by Jef Poskanzer.
  
  Permission to use, copy, modify, and distribute this software and its
  documentation for any purpose and without fee is hereby granted, provided
  that the above copyright notice appear in all copies and that both that
  copyright notice and this permission notice appear in supporting
  documentation.  This software is provided "as is" without express or
  implied warranty.
  
  (End of note from pgmnorm.c)

  Pgmnorm's man page also said:
  
  Partially based on the fbnorm filter in Michael Mauldin's "Fuzzy Pixmap"
  package.
*****************************************************************************/

#include "pnm.h"

enum brightMethod {BRIGHT_MAX, BRIGHT_LUMIN};

struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *inputFilespec;  /* Filespec of input file */
    unsigned int bvalueSpec;
    xelval bvalue;
    float bpercent;
    unsigned int wvalueSpec;
    xelval wvalue;
    float wpercent;
    enum brightMethod brightMethod;
    unsigned int keephues;
};



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

    unsigned int bpercentSpec, wpercentSpec;
    unsigned int brightmax;

    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0,   "bpercent",      OPT_FLOAT,   
            &cmdlineP->bpercent,   &bpercentSpec, 0);
    OPTENT3(0,   "wpercent",      OPT_FLOAT,   
            &cmdlineP->wpercent,   &wpercentSpec, 0);
    OPTENT3(0,   "bvalue",        OPT_UINT,   
            &cmdlineP->bvalue,     &cmdlineP->bvalueSpec, 0);
    OPTENT3(0,   "wvalue",        OPT_UINT,   
            &cmdlineP->wvalue,     &cmdlineP->wvalueSpec, 0);
    OPTENT3(0,   "keephues",      OPT_FLAG,   
            NULL,                  &cmdlineP->keephues, 0);
    OPTENT3(0,   "brightmax",      OPT_FLAG,   
            NULL,                  &brightmax, 0);

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions3( &argc, argv, opt, sizeof(opt), 0 );
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if(wpercentSpec && cmdlineP->wvalueSpec)
        pm_error("You may specify only one of -wpercent and -wvalue");

    if(bpercentSpec && cmdlineP->bvalueSpec)
        pm_error("You may specify only one of -bpercent and -bvalue");

    if (!wpercentSpec)
        cmdlineP->wpercent = 1.0;
    if (!bpercentSpec)
        cmdlineP->bpercent = 2.0;

    if (cmdlineP->wpercent < 0.0)
        pm_error("You specified a negative value for wpercent: %f",
                 cmdlineP->wpercent);
    if (cmdlineP->bpercent < 0.0)
        pm_error("You specified a negative value for bpercent: %f",
                 cmdlineP->bpercent);
    if (cmdlineP->wpercent > 100.0)
        pm_error("You specified a per centage > 100 for wpercent: %f",
                 cmdlineP->wpercent);
    if (cmdlineP->bpercent > 100.0)
        pm_error("You specified a per centage > 100 for bpercent: %f",
                 cmdlineP->bpercent);

    cmdlineP->brightMethod = brightmax > 0 ? BRIGHT_MAX : BRIGHT_LUMIN;

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
buildHistogram(FILE *   const ifp, 
               int      const cols,
               int      const rows,
               xelval   const maxval,
               int      const format,
               unsigned int   hist[],
               enum brightMethod const brightMethod) {
/*----------------------------------------------------------------------------
   Build the histogram of brightness values for the image that is in file
   'ifp', which is positioned just after the header (at the raster).

   The histogram is the array hist[] such that hist[x] is the number of
   xels in the image that have brightness x.  That brightness is either
   the intensity of most intense component of the xel or it is the 
   luminosity of the xel, depending on 'brightMethod'.  In either case,
   it is based on the same maxval as the image, which is 'maxval'.  The
   image is 'cols' columns wide by 'rows' rows high.

   Leave the file positioned arbitrarily.
-----------------------------------------------------------------------------*/
    int row;
    xel * xelrow;
    
    xelrow = pnm_allocrow(cols);

    {
        unsigned int i;
        for (i = 0; i <= maxval; ++i)
            hist[i] = 0;
    }
    for (row = 0; row < rows; ++row) {
        int col;
        pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
        for (col = 0; col < cols; ++col) {
            xelval brightness;
            xel const p = xelrow[col];
            if (PNM_FORMAT_TYPE(format) == PPM_TYPE) {
                if (brightMethod == BRIGHT_MAX)
                    brightness = max(PPM_GETR(p), 
                                     max(PPM_GETG(p), PPM_GETB(p)));
                else
                    brightness = PPM_LUMIN(p);
            } else
                brightness = PNM_GET1(p);
            ++hist[brightness];
        }
    }
    pnm_freerow(xelrow);
}



static void
computeBottomPercentile(unsigned int         hist[], 
                        unsigned int   const highest,
                        unsigned int   const total,
                        float          const percent, 
                        unsigned int * const percentileP) {
/*----------------------------------------------------------------------------
   Compute the lowest index of hist[] such that the sum of the hist[]
   values with that index and lower represent at least 'percent' per cent of
   'n' (which is assumed to be the sum of all the values in hist[],
   given to us to save us the time of computing it).
-----------------------------------------------------------------------------*/
    unsigned int cutoff = total * percent / 100.0;
    unsigned int count;
    unsigned int percentile;

    percentile = 0; /* initial value */
    count = hist[0];  /* initial value */

    while (count < cutoff) {
        if (percentile == highest)
            pm_error("Internal error: computeBottomPercentile() received"
                     "a 'total' value greater than the sum of the hist[]"
                     "values");
        ++percentile;
        count += hist[percentile];
    }        
    *percentileP = percentile;
}



static void
computeTopPercentile(unsigned int         hist[], 
                     unsigned int   const highest, 
                     unsigned int   const total,
                     float          const percent, 
                     unsigned int * const percentileP) {
/*----------------------------------------------------------------------------
   Compute the highest index of hist[] such that the sum of the hist[]
   values with that index and higher represent 'percent' per cent of
   'n' (which is assumed to be the sum of all the values in hist[],
   given to us to save us the time of computing it).
-----------------------------------------------------------------------------*/
    unsigned int cutoff = total * percent / 100.0;
    unsigned int count;
    unsigned int percentile;

    percentile = highest; /* initial value */
    count = hist[highest];

    while (count < cutoff) {
        --percentile;
        count += hist[percentile];
    }
    *percentileP = percentile;
}



static void
computeEndValues(FILE *   const ifp,
                 int      const cols,
                 int      const rows,
                 xelval   const maxval,
                 int      const format,
                 struct cmdlineInfo const cmdline,
                 xelval * const bvalueP,
                 xelval * const wvalueP) {
/*----------------------------------------------------------------------------
   Figure out what original values will be translated to full white and
   full black -- thus defining to what all the other values get translated.

   This may involve looking at the image.  The image is in the file
   'ifp', which is positioned just past the header (at the raster).
   Leave it positioned arbitrarily.
-----------------------------------------------------------------------------*/
    unsigned int hist[PNM_OVERALLMAXVAL+1];

    buildHistogram(ifp, cols, rows, maxval, format, hist, BRIGHT_LUMIN);
    
    if (cmdline.bvalueSpec)
        *bvalueP = cmdline.bvalue;
    else 
        computeBottomPercentile(hist, maxval, cols*rows, cmdline.bpercent, 
                                bvalueP);
    if (cmdline.wvalueSpec) 
        *wvalueP = cmdline.wvalue;
    else
        computeTopPercentile(hist, maxval, cols*rows, cmdline.wpercent, 
                             wvalueP);
}



static void
computeTransferFunction(xelval const bvalue, 
                        xelval const wvalue,
                        xelval const maxval,
                        xelval newIntensity[]) {
/*----------------------------------------------------------------------------
   Compute the transfer function, i.e. the array newIntensity[] such that
   newIntensity[x] is the intensity of the xel that should replace a
   xel with intensity x.

   Define function only for values 0..maxval.
-----------------------------------------------------------------------------*/
    xelval i;

    /* Map the lowest intensities to zero */
    if (bvalue > 0)
        for (i = 0; i <= bvalue-1; ++i)
            newIntensity[i] = 0;

    if (wvalue > bvalue) {
        /* Map the middle intensities to 0..maxval */
        /* Size: wvalue, bvalue are xelval, so the diff fits also in xelval */
        xelval const range = wvalue - bvalue;
        /* As val is at maximum (wvalue - bvalue - 1) * maxval <
         * xelval_max * xelval_max this fits into double
         */
        double val;
        /* The following for loop is a hand optimization of this one:
           for (i = bvalue; i <= wvalue; ++i)
           newIntensity[i] = (i-bvalue)*maxval/range);
           (with proper rounding)
           the rounding is aequivalent to (i-bvalue)*maxval/range + .5
           <=> ((i-bvalue)*maxval + .5*range)/range, so we set the starting
           point to range/2
        */
        for (i = bvalue, val = range/2; 
             i <= wvalue; 
             ++i, val += maxval)
            newIntensity[i] = val / range;
    }

    /* Map the highest intensities to maxval */
    for (i = wvalue+1; i <= maxval; ++i)
        newIntensity[i] = maxval;
}
            


static void
writeRowNormalized(xel *  const xelrow,
                   int    const cols,
                   xelval const maxval,
                   int    const format,
                   bool   const keephues,
                   xelval const newIntensity[],
                   xel *  const rowbuf) {
/*----------------------------------------------------------------------------
   Write to Standard Output a normalized version of the xel row 
   'xelrow'.  Normalize it via the transfer function newIntensity[].

   Use 'rowbuf' as a work buffer.  It is at least 'cols' columns wide.
-----------------------------------------------------------------------------*/
    xel * const outrow = rowbuf;
                
    unsigned int col;
    for (col = 0; col < cols; ++col) {
        xel const p = xelrow[col];

        if (PPM_FORMAT_TYPE(format) == PPM_TYPE) {
            if (keephues) {
                xelval const oldIntensity = PPM_LUMIN(p);
                float const scaler = 
                    (float)newIntensity[oldIntensity]/oldIntensity;
            
                xelval const r = min((int)(PPM_GETR(p)*scaler+0.5), maxval);
                xelval const g = min((int)(PPM_GETG(p)*scaler+0.5), maxval);
                xelval const b = min((int)(PPM_GETB(p)*scaler+0.5), maxval);
                PNM_ASSIGN(outrow[col], r, g, b);
            } else 
                PNM_ASSIGN(outrow[col], 
                           newIntensity[PPM_GETR(p)], 
                           newIntensity[PPM_GETG(p)], 
                           newIntensity[PPM_GETB(p)]);
        } else 
            PNM_ASSIGN1(outrow[col], newIntensity[PNM_GET1(p)]);
    }
    pnm_writepnmrow(stdout, outrow, cols, maxval, format, 0);
}



int
main(int argc, char *argv[]) {

    struct cmdlineInfo cmdline;
    FILE *ifp;
    int imagePos;
    xelval maxval;
    int rows, cols, format;
    xelval bvalue, wvalue;
    
    pnm_init(&argc, argv);

    parseCommandLine(argc, argv, &cmdline);

    ifp = pm_openr_seekable(cmdline.inputFilespec);

    /* Rescale so that bvalue maps to 0, wvalue maps to maxval. */
    pnm_readpnminit(ifp, &cols, &rows, &maxval, &format);
    imagePos = pm_tell(ifp);

    computeEndValues(ifp, cols, rows, maxval, format, cmdline, 
                     &bvalue, &wvalue);
        
    if (wvalue < bvalue)
        pm_error("The colors which become black would overlap the "
                 "colors which become white.");
    else {
        xelval newIntensity[PNM_OVERALLMAXVAL+1];
        int row;
        xel * xelrow;
        xel * rowbuf;
        
        xelrow = pnm_allocrow(cols);

        pm_message("remapping %d..%d to %d..%d", bvalue, wvalue, 0, maxval);

        computeTransferFunction(bvalue, wvalue, maxval, newIntensity);

        pm_seek(ifp, imagePos);
        pnm_writepnminit(stdout, cols, rows, maxval, format, 0);

        rowbuf = pnm_allocrow(cols);

        for (row = 0; row < rows; ++row) {
            pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
            writeRowNormalized(xelrow, cols, maxval, format,
                               cmdline.keephues, newIntensity, rowbuf);
        }
        pnm_freerow(rowbuf);
        pnm_freerow(xelrow);
    }
    pm_close(ifp);
    exit(0);
}
