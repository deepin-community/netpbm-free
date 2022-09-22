/*****************************************************************************
                               pamdice
******************************************************************************
  Slice a Netpbm image vertically and/or horizontally into multiple images.

  By Bryan Henderson, San Jose CA 2001.01.31

  Contributed to the public domain.

*****************************************************************************/

#include "pam.h"
#include <string.h>

#define MAXFILENAMELEN 80
    /* Maximum number of characters we accept in filenames */

struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *inputFilespec;  /* '-' if stdin */
    char *outstem; 
    unsigned int sliceVertically;    /* boolean */
    unsigned int sliceHorizontally;  /* boolean */
    unsigned int width;   
    unsigned int height;  
    unsigned int verbose;
};


static void
parseCommandLine ( int argc, char ** argv,
                   struct cmdlineInfo * const cmdlineP ) {
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
    
    unsigned int outstemSpec;
    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENT3 */
    OPTENT3(0, "width",       OPT_UINT,    &cmdlineP->width,       
            &cmdlineP->sliceVertically,       0 );
    OPTENT3(0, "height",      OPT_UINT,    &cmdlineP->height,
            &cmdlineP->sliceHorizontally,     0 );
    OPTENT3(0, "outstem",     OPT_STRING,  &cmdlineP->outstem,
            &outstemSpec,                     0 );
    OPTENT3(0, "verbose",     OPT_FLAG,    NULL,
            &cmdlineP->verbose,               0 );

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    /* set defaults */

    pm_optParseOptions3( &argc, argv, opt, sizeof(opt), 0 );
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (!outstemSpec)
        pm_error("You must specify the -outstem option to indicate where to "
                 "put the output images.");
    else if (strlen(cmdlineP->outstem) > MAXFILENAMELEN - 10)
        pm_error("-outstem value is too long.  That would exceed the maximum "
                 "allowable filename length of %u", MAXFILENAMELEN);
    if (argc-1 < 1)
        cmdlineP->inputFilespec = "-";
    else if (argc-1 == 1)
        cmdlineP->inputFilespec = argv[1];
    else 
        pm_error("Progam takes at most 1 parameters: the file specification.  "
                 "You specified %d", argc-1);
}




static void
computeSliceGeometry(struct cmdlineInfo const cmdline,
                     struct pam     const inpam,
                     bool           const verbose,
                     unsigned int * const nHorizSliceP,
                     unsigned int * const sliceWidthP,
                     unsigned int * const nVertSliceP,
                     unsigned int * const sliceHeightP) {
/*----------------------------------------------------------------------------
   Compute the geometry of the common slices.  Edge slices may be smaller
   than this.
-----------------------------------------------------------------------------*/
    if (cmdline.sliceHorizontally) {
        *nHorizSliceP = ((inpam.height-1) / cmdline.height) + 1;
        *sliceHeightP = cmdline.height;
    } else {
        *nHorizSliceP = 1;
        *sliceHeightP = inpam.height;
    }

    if (cmdline.sliceVertically) {
        *nVertSliceP = ((inpam.width-1) / cmdline.width) + 1;
        *sliceWidthP = cmdline.width;
    } else {
        *nVertSliceP = 1;
        *sliceWidthP = inpam.width;
    }

    if (*nVertSliceP > 100)
        pm_error("You have %d vertical slices.  The maximum allowed is 100",
                 *nVertSliceP);
    if (*nHorizSliceP > 100)
        pm_error("You have %d horizontal slices.  The maximum allowed is 100",
                 *nHorizSliceP);

    if (verbose) {
        pm_message("Creating %u images, %u across by %u down",
                   *nVertSliceP * *nHorizSliceP,
                   *nVertSliceP, *nHorizSliceP);
    }
}



static void
openOutStreams(struct pam const inpam, struct pam outpam[],
               unsigned int const horizSlice, unsigned int const nVertSlice,
               unsigned int const sliceHeight, unsigned int sliceWidth,
               const char outstem[]) {
/*----------------------------------------------------------------------------
   Open the output files for a single horizontal slice (there's one file
   for each vertical slice) and write the Netpbm headers to them.  Also
   compute the pam structures to control each.

   Assume 'horizSlice' is less than 100 and 'nVertSlice' is less than or
   equal to 100.

   Assume the output filename (controlled by 'outstem') fits in 
   MAXFILENAMELEN characters.
-----------------------------------------------------------------------------*/
    unsigned int vertSlice;

    for (vertSlice = 0; vertSlice < nVertSlice; ++vertSlice) {
        char filename[MAXFILENAMELEN+1];
        const char * filenameSuffix;
        int rc;

        switch(PNM_FORMAT_TYPE(inpam.format)) {
        case PPM_TYPE: filenameSuffix = "ppm"; break;
        case PGM_TYPE: filenameSuffix = "pgm"; break;
        case PBM_TYPE: filenameSuffix = "pbm"; break;
        case PAM_TYPE: filenameSuffix = "pam"; break;
        default:       filenameSuffix = "";    break;
        }
        
        if (vertSlice > 99 || horizSlice > 99)
            pm_error("Internal error: slice number doesn't fit in filename");
        rc = snprintf(filename, sizeof(filename), "%s_%02d_%02d.%s",
                      outstem, horizSlice, vertSlice, filenameSuffix);
        if (rc > sizeof(filename))
            pm_error("Internal error: generated filename is too long.");

        outpam[vertSlice] = inpam;
        outpam[vertSlice].file = pm_openw(filename);

        outpam[vertSlice].width = 
            min(sliceWidth, inpam.width - vertSlice*sliceWidth);

        outpam[vertSlice].height = sliceHeight;

        pnm_writepaminit(&outpam[vertSlice]);
    }        
}



static void
closeOutFiles(struct pam pam[], unsigned int const nVertSlice) {

    unsigned int vertSlice;
    
    for (vertSlice = 0; vertSlice < nVertSlice; ++vertSlice)
        pm_close(pam[vertSlice].file);
}

static void
sliceRow(tuple inputRow[], struct pam outpam[], 
         unsigned int const nVertSlice) {
/*----------------------------------------------------------------------------
   Distribute the row inputRow[] across the output 'nVerticalSlice'
   files described by outpam[].  Each outpam[x] tells how many columns
   of inputRow[] to take and what their composition is.


-----------------------------------------------------------------------------*/
    tuple * outputRow;

    unsigned int vertSlice;

    for (vertSlice = 0, outputRow = inputRow; 
         vertSlice < nVertSlice; 
         outputRow += outpam[vertSlice].width, ++vertSlice) {
        pnm_writepamrow(&outpam[vertSlice], outputRow);
    }
}



int
main(int argc, char ** argv) {

    struct cmdlineInfo cmdline;
    FILE    *ifP;
    struct pam inpam;
    tuple* inputRow;   /* Row from input image */
    unsigned int rowsRead;
    unsigned int horizSlice;
        /* Number of the current horizontal slice.  Slices are numbered
           sequentially starting at 0.
        */
    unsigned int sliceWidth;
        /* Width in pam columns of each vertical slice, except that
           the rightmost slice may be narrower.  If we aren't slicing
           vertically, that means one slice, i.e. the slice width
           is the image width.  
        */
    unsigned int sliceHeight;
        /* Height in pam rows of each horizontal slice, except that
           the bottom slice may be shorter.  If we aren't slicing
           horizontally, that means one slice, i.e. the slice height
           is the image height.  
        */
    unsigned int nHorizSlice;
    unsigned int nVertSlice;
    
    struct pam outpam[100];
        /* outpam[x] is the pam structure that controls the current horizontal
           slice of vertical slice x.
        */

    pnm_init(&argc, argv);
    
    parseCommandLine(argc, argv, &cmdline);
        
    ifP = pm_openr(cmdline.inputFilespec);

    pnm_readpaminit(ifP, &inpam, sizeof(inpam));

    computeSliceGeometry(cmdline, inpam, !!cmdline.verbose,
                         &nHorizSlice, &sliceWidth, &nVertSlice, &sliceHeight);

    inputRow = pnm_allocpamrow(&inpam);

    rowsRead = 0;  /* initial value */

    for (horizSlice = 0; horizSlice < nHorizSlice; ++horizSlice) {
        unsigned int row;
        unsigned int thisSliceHeight = min(sliceHeight, inpam.height-rowsRead);

        openOutStreams(inpam, outpam, horizSlice, nVertSlice, 
                       thisSliceHeight, sliceWidth,
                       cmdline.outstem);

        for (row = 0; row < thisSliceHeight; ++row) {
            pnm_readpamrow(&inpam, inputRow);
            ++rowsRead;
            sliceRow(inputRow, outpam, nVertSlice);
        }
        closeOutFiles(outpam, nVertSlice);
    }

    pnm_freepamrow(inputRow);
    pm_close(ifP);
    exit(0);
}
