/* pnmgamma.c - perform gamma correction on a portable pixmap
**
** Copyright (C) 1991 by Bill Davidson and Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <math.h>
#include <ctype.h>
#include "pnm.h"


enum transferFunction {XF_EXP, XF_CIERAMP, XF_SRGBRAMP};

struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *filespec;  /* '-' if stdin */
    enum transferFunction transferFunction;
    double rgamma, ggamma, bgamma;
    unsigned int ungamma;
};


static void
parseCommandLine(int argc, char ** argv, 
                 struct cmdlineInfo * const cmdlineP) {

    optEntry *option_def = malloc( 100*sizeof( optEntry ) );
        /* Instructions to optParseOptions3 on how to parse our options.
         */
    optStruct3 opt;

    unsigned int cieramp, srgbramp;

    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0, "ungamma",     OPT_FLAG,   NULL,                  
            &cmdlineP->ungamma,       0 );
    OPTENT3(0, "cieramp",     OPT_FLAG,   NULL,                  
            &cieramp,       0 );
    OPTENT3(0, "srgbramp",    OPT_FLAG,   NULL,                  
            &srgbramp,      0 );

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = TRUE; 

    pm_optParseOptions3( &argc, argv, opt, sizeof(opt), 0 );
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (cieramp && srgbramp)
        pm_error("You can't specify both -cieramp and -srgbramp");
    else if (cieramp) 
        cmdlineP->transferFunction = XF_CIERAMP;
    else if (srgbramp)
        cmdlineP->transferFunction = XF_SRGBRAMP;
    else
        cmdlineP->transferFunction = XF_EXP;

    if (argc-1 == 0) {
        if (cmdlineP->transferFunction == XF_SRGBRAMP)
            cmdlineP->rgamma = cmdlineP->ggamma = cmdlineP->bgamma = 2.2;
        else
            cmdlineP->rgamma = cmdlineP->ggamma = cmdlineP->bgamma = 1.0/0.45;
        cmdlineP->filespec = "-";
    } else if (argc-1 == 1) {
        cmdlineP->rgamma = cmdlineP->ggamma = cmdlineP->bgamma = atof(argv[1]);
        cmdlineP->filespec = "-";
    } else if (argc-1 == 2) {
        cmdlineP->rgamma = cmdlineP->ggamma = cmdlineP->bgamma = atof(argv[1]);
        cmdlineP->filespec = argv[2];
    } else if (argc-1 == 3) {
        cmdlineP->rgamma = atof(argv[1]);
        cmdlineP->ggamma = atof(argv[2]);
        cmdlineP->bgamma = atof(argv[3]);
        cmdlineP->filespec = "-";
    } else if (argc-1 == 4) {
        cmdlineP->rgamma = atof(argv[1]);
        cmdlineP->ggamma = atof(argv[2]);
        cmdlineP->bgamma = atof(argv[3]);
        cmdlineP->filespec = argv[4];
    } else 
        pm_error("Wrong number of arguments.  "
                 "You may have 0, 1, or 3 gamma values "
                 "plus zero or one filename");

    if (cmdlineP->rgamma <= 0.0 || 
        cmdlineP->ggamma <= 0.0 || 
        cmdlineP->bgamma <= 0.0 )
        pm_error("Invalid gamma value.  Must be positive floating point "
                 "number.");
}


static void
buildGamma(xelval table[], xelval const maxval, double const gamma) {
/*----------------------------------------------------------------------------
  
   Build a gamma table of size maxval+1 for the given gamma value.
  
   This function depends on pow(3m).  If you don't have it, you can
   simulate it with '#define pow(x,y) exp((y)*log(x))' provided that
   you have the exponential function exp(3m) and the natural logarithm
   function log(3m).  I can't believe I actually remembered my log
   identities.
-----------------------------------------------------------------------------*/
    xelval i;
    double const one_over_gamma = 1.0 / gamma;
    double const q = (double) maxval;

    for (i = 0 ; i <= maxval; ++i) {
        double const ind = ((double) i) / q;
            /* Xel sample value normalized to 0..1 */
        double const v = (q * pow(ind, one_over_gamma));
        table[i] = min((xelval)(v + 0.5), maxval);  /* round and clip */
    }
}



static void
buildCie709Gamma(xelval table[], xelval const maxval,
                 double const gamma) {
/*----------------------------------------------------------------------------
   Build a gamma table of size maxval+1 for the CIE Rec. 709 gamma
   transfer function.

   'gamma' must be 1/0.45 for true Rec. 709.
-----------------------------------------------------------------------------*/
    double const oneOverGamma = 1.0 / gamma;
    xelval i;

    /* This transfer function is linear for sample values 0
       .. maxval*.018 and an exponential for larger sample values.
       The exponential is slightly stretched and translated, though,
       unlike the popular pure exponential gamma transfer function.
    */
    xelval const linearCutoff = (xelval) maxval * 0.018 + 0.5;
    double const linearExpansion = 
        (1.099 * pow(0.018, oneOverGamma) - 0.099) / 0.018;

    for (i = 0; i <= linearCutoff; ++i) 
        table[i] = i * linearExpansion;
    for (; i <= maxval; ++i) {
        double const normalized = ((double) i) / maxval;
            /* Xel sample value normalized to 0..1 */
        double const v = 1.099 * pow(normalized, oneOverGamma) - 0.099;
        table[i] = min((xelval)(v * maxval + 0.5), maxval);  
            /* denormalize, round, and clip */
    }
}



static void
buildCie709GammaInverse(xelval table[], xelval const maxval,
                        double const gamma) {
/*----------------------------------------------------------------------------
   Build a gamma table of size maxval+1 for the Inverse of the CIE
   Rec. 709 gamma transfer function.

   'gamma' must be 1/0.45 for true Rec. 709.
-----------------------------------------------------------------------------*/
    xelval i;

    /* This transfer function is linear for sample values 0
       .. maxval*.018 and an exponential for larger sample values.
       The exponential is slightly stretched and translated, though,
       unlike the popular pure exponential gamma transfer function.
    */
    xelval const linearCutoff = (xelval) maxval * 0.018 + 0.5;
    double const linearExpansion = 
        (1.099 * pow(0.018, gamma) - 0.099) / 0.018;

    for (i = 0; i <= linearCutoff * linearExpansion; ++i) 
        table[i] = i / linearExpansion;
    for (; i <= maxval; ++i) {
        double const normalized = ((double) i) / maxval;
            /* Xel sample value normalized to 0..1 */
        double const v = pow((normalized + 0.099) / 1.099, gamma);
        table[i] = min((xelval)(v * maxval + 0.5), maxval);  
            /* denormalize, round, and clip */
    }
}



static void
buildSrgbGamma(xelval table[], xelval const maxval,
                 double const gamma) {
/*----------------------------------------------------------------------------
   Build a gamma table of size maxval+1 for the IEC SRGB gamma
   transfer function (Standard IEC 61966-2-1).

   'gamma' must be 2.4 for true SRGB
-----------------------------------------------------------------------------*/
    double const oneOverGamma = 1.0 / gamma;
    xelval i;

    /* This transfer function is linear for sample values 0
       .. maxval*.040405 and an exponential for larger sample values.
       The exponential is slightly stretched and translated, though,
       unlike the popular pure exponential gamma transfer function.
    */
    xelval const linearCutoff = (xelval) maxval * 0.040405 + 0.5;
    double const linearExpansion = 
        (1.055 * pow(0.040405, oneOverGamma) - 0.055) / 0.040405;

    for (i = 0; i <= linearCutoff; ++i) 
        table[i] = i * linearExpansion;
    for (; i <= maxval; ++i) {
        double const normalized = ((double) i) / maxval;
            /* Xel sample value normalized to 0..1 */
        double const v = 1.055 * pow(normalized, oneOverGamma) - 0.055;
        table[i] = min((xelval)(v * maxval + 0.5), maxval);  
            /* denormalize, round, and clip */
    }
}



static void
buildSrgbGammaInverse(xelval table[], xelval const maxval,
                        double const gamma) {
/*----------------------------------------------------------------------------
   Build a gamma table of size maxval+1 for the Inverse of the IEC SRGB gamma
   transfer function (Standard IEC 61966-2-1).

   'gamma' must be 2.4 for true SRGB
-----------------------------------------------------------------------------*/
    double const oneOverGamma = 1.0 / gamma;
    xelval i;

    /* This transfer function is linear for sample values 0
       .. maxval*.040405 and an exponential for larger sample values.
       The exponential is slightly stretched and translated, though,
       unlike the popular pure exponential gamma transfer function.
    */
    xelval const linearCutoff = (xelval) maxval * 0.040405 + 0.5;
    double const linearExpansion = 
        (1.055 * pow(0.040405, oneOverGamma) - 0.055) / 0.040405;

    for (i = 0; i <= linearCutoff * linearExpansion; ++i) 
        table[i] = i / linearExpansion;
    for (; i <= maxval; ++i) {
        double const normalized = ((double) i) / maxval;
            /* Xel sample value normalized to 0..1 */
        double const v = pow((normalized + 0.055) / 1.055, gamma);
        table[i] = min((xelval)(v * maxval + 0.5), maxval);  
            /* denormalize, round, and clip */
    }
}



static void
createGammaTables(bool const ungamma, 
                  enum transferFunction const transferFunction,
                  xelval const maxval,
                  double const rgamma, 
                  double const ggamma, 
                  double const bgamma,
                  xelval **rtableP, xelval **gtableP, xelval **btableP) {

    /* Allocate space for the tables. */

    overflow_add(maxval, 1);
    *rtableP = (xelval*) malloc2( (maxval+1) , sizeof(xelval) );
    *gtableP = (xelval*) malloc2( (maxval+1) , sizeof(xelval) );
    *btableP = (xelval*) malloc2( (maxval+1) , sizeof(xelval) );
    if (*rtableP == NULL || *gtableP == NULL || *btableP == NULL)
        pm_error( "out of memory" );

    /* Build the gamma corection tables. */
    switch (transferFunction) {
    case XF_CIERAMP: {
        if (ungamma) {
            buildCie709GammaInverse(*rtableP, maxval, rgamma);
            buildCie709GammaInverse(*gtableP, maxval, ggamma);
            buildCie709GammaInverse(*btableP, maxval, bgamma);
        } else {
            buildCie709Gamma(*rtableP, maxval, rgamma);
            buildCie709Gamma(*gtableP, maxval, ggamma);
            buildCie709Gamma(*btableP, maxval, bgamma);
        }
    }
    break;
    case XF_SRGBRAMP: {
        if (ungamma) {
            buildSrgbGammaInverse(*rtableP, maxval, rgamma);
            buildSrgbGammaInverse(*gtableP, maxval, ggamma);
            buildSrgbGammaInverse(*btableP, maxval, bgamma);
        } else {
            buildSrgbGamma(*rtableP, maxval, rgamma);
            buildSrgbGamma(*gtableP, maxval, ggamma);
            buildSrgbGamma(*btableP, maxval, bgamma);
        }
    }
    break;
    case XF_EXP: {
        if (ungamma) {
            buildGamma(*rtableP, maxval, 1.0/rgamma);
            buildGamma(*gtableP, maxval, 1.0/ggamma);
            buildGamma(*btableP, maxval, 1.0/bgamma);
        } else {
            buildGamma(*rtableP, maxval, rgamma);
            buildGamma(*gtableP, maxval, ggamma);
            buildGamma(*btableP, maxval, bgamma);
        }
    }
    break;
    }
}



int
main(int argc, char *argv[] ) {
    struct cmdlineInfo cmdline;
    FILE* ifp;
    xel* xelrow;
    xelval maxval;
    int rows, cols, format, row;
    int outputFormat;
    xelval* rtable;
    xelval* gtable;
    xelval* btable;

    pnm_init( &argc, argv );

    parseCommandLine(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.filespec);

    pnm_readpnminit(ifp, &cols, &rows, &maxval, &format);
    xelrow = pnm_allocrow(cols);

    if (PNM_FORMAT_TYPE(format) == PPM_TYPE)
        outputFormat = PPM_TYPE;
    else if (cmdline.rgamma != cmdline.ggamma 
             || cmdline.ggamma != cmdline.bgamma) 
        outputFormat = PPM_TYPE;
    else 
        outputFormat = PGM_TYPE;

    if (PNM_FORMAT_TYPE(format) != outputFormat) {
        if (outputFormat == PPM_TYPE)
            pm_message("Promoting to PPM");
        if (outputFormat == PGM_TYPE)
            pm_message("Promoting to PGM");
    }

    createGammaTables(cmdline.ungamma, cmdline.transferFunction, maxval,
                      cmdline.rgamma, cmdline.ggamma, cmdline.bgamma,
                      &rtable, &gtable, &btable);

    pnm_writepnminit( stdout, cols, rows, maxval, outputFormat, 0 );
    for (row = 0; row < rows; ++row) {
        int col;
        pnm_readpnmrow(ifp, xelrow, cols, maxval, format);

        pnm_promoteformatrow(xelrow, cols, maxval, format, 
                             maxval, outputFormat);

        switch (PNM_FORMAT_TYPE(outputFormat)) {
        case PPM_TYPE:
            for (col = 0; col < cols; ++col) {
                xelval r, g, b;

                r = PPM_GETR(xelrow[col]);
                g = PPM_GETG(xelrow[col]);
                b = PPM_GETB(xelrow[col]);
                PPM_ASSIGN(xelrow[col], rtable[r], gtable[g], btable[b]);
            }
            break;

        case PGM_TYPE:
            for (col = 0; col < cols; ++col) {
                register xelval xel;

                xel = PNM_GET1(xelrow[col]);
                PNM_ASSIGN1(xelrow[col], gtable[xel]);
            }
            break;
        default:
            pm_error("Internal error");
        }
        pnm_writepnmrow(stdout, xelrow, cols, maxval, outputFormat, 0);
    }

    pm_close(ifp);
    pm_close(stdout);

    exit(0);
}

