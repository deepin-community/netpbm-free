/* pnmtops.c - read a portable anymap and produce a PostScript file
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
**
** -nocenter option added November 1993 by Wolfgang Stuerzlinger,
**  wrzl@gup.uni-linz.ac.at.
**
*/

#include <string.h>
#include "pam.h"
#include "shhopt.h"

#define STRSEQ(a,b) (strncmp((a), (b), sizeof(a)) == 0)

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filespec;  /* Filespecs of input file */
    float scale;
    unsigned int dpiX;     /* horiz component of DPI option */
    unsigned int dpiY;     /* vert component of DPI option */
    unsigned int width;              /* in 1/72 inch */
    unsigned int height;             /* in 1/72 inch */
    unsigned int mustturn;
    bool         canturn;
    unsigned int rle;
    bool         center;
    unsigned int imagewidth;         /* in 1/72 inch; zero if unspec */
    unsigned int imageheight;        /* in 1/72 inch; zero if unspec */
    unsigned int equalpixels;
    unsigned int setpage;
};



static void putxelval ARGS(( xelval xv ));
static void putrest ARGS(( void ));
static void rleputbuffer ARGS(( void ));
static void rleputitem ARGS(( void ));
static void rleputxelval ARGS(( xelval xv ));
static void rleflush ARGS(( void ));
static void rleputrest ARGS(( void ));


static void
parse_dpi(char * const dpiOpt, 
          unsigned int * const dpiXP, unsigned int * const dpiYP) {

    char *dpistr2;
    unsigned int dpiX, dpiY;

    dpiX = strtol(dpiOpt, &dpistr2, 10);
    if (dpistr2 == dpiOpt) 
        pm_error("Invalid value for -dpi: '%s'.  Must be either number "
                 "or NxN ", dpiOpt);
    else {
        if (*dpistr2 == '\0') {
            *dpiXP = dpiX;
            *dpiYP = dpiX;
        } else if (*dpistr2 == 'x') {
            char * dpistr3;

            dpistr2++;  /* Move past 'x' */
            dpiY = strtol(dpistr2, &dpistr3, 10);        
            if (dpistr3 != dpistr2 && *dpistr3 == '\0') {
                *dpiXP = dpiX;
                *dpiYP = dpiY;
            } else {
                pm_error("Invalid value for -dpi: '%s'.  Must be either "
                         "number or NxN", dpiOpt);
            }
        }
    }
}



static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdlineP) {

    unsigned int imagewidth_spec, imageheight_spec;
    float imagewidth, imageheight;
    unsigned int center, nocenter;
    unsigned int nosetpage;
    float width, height;
    unsigned int noturn;

    optStruct3 opt;
    unsigned int option_def_index = 0;
    optEntry *option_def = malloc(100*sizeof(optEntry));

    char *dpiOpt = "300";

    OPTENT3(0, "scale",       OPT_FLOAT, &cmdlineP->scale, NULL,         0);
    OPTENT3(0, "dpi",         OPT_STRING, &dpiOpt,         NULL,         0);
    OPTENT3(0, "width",       OPT_FLOAT, &width,           NULL,         0);
    OPTENT3(0, "height",      OPT_FLOAT, &height,          NULL,         0);
    OPTENT3(0, "turn",        OPT_FLAG,  NULL, &cmdlineP->mustturn,      0);
    OPTENT3(0, "noturn",      OPT_FLAG,  NULL, &noturn,                  0);
    OPTENT3(0, "rle",         OPT_FLAG,  NULL, &cmdlineP->rle,           0);
    OPTENT3(0, "runlength",   OPT_FLAG,  NULL, &cmdlineP->rle,           0);
    OPTENT3(0, "center",      OPT_FLAG,  NULL, &center,                  0);
    OPTENT3(0, "nocenter",    OPT_FLAG,  NULL, &nocenter,                0);
    OPTENT3(0, "equalpixels", OPT_FLAG,  NULL, &cmdlineP->equalpixels,   0);
    OPTENT3(0, "imagewidth",  OPT_FLOAT, &imagewidth,  &imagewidth_spec, 0);
    OPTENT3(0, "imageheight", OPT_FLOAT, &imageheight, &imageheight_spec,0);
    OPTENT3(0, "nosetpage",   OPT_FLAG,  NULL, &nosetpage,               0);
    OPTENT3(0, "setpage",     OPT_FLAG,  NULL, &cmdlineP->setpage,       0);
    
    /* DEFAULTS */
    cmdlineP->scale = 1.0;
    width = 8.5;
    height = 11.0;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;
    opt.allowNegNum = FALSE;

    pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);

    if (cmdlineP->mustturn && noturn)
        pm_error("You cannot specify both -turn and -noturn");
    if (center && nocenter)
        pm_error("You cannot specify both -center and -nocenter");

    if (cmdlineP->setpage && nosetpage)
        pm_error("You cannot specify both -setpage and -nosetpage");
    /* No need to do anything else on nosetpage, as this is the default */

    parse_dpi(dpiOpt, &cmdlineP->dpiX, &cmdlineP->dpiY);

    cmdlineP->center  = !nocenter;
    cmdlineP->canturn = !noturn;
    
    overflow2(width, 72);
    overflow2(height, 72);

    cmdlineP->width  = width * 72;
    cmdlineP->height = height * 72;

    if (imagewidth_spec)
    {
	overflow2(imagewidth, 72);
        cmdlineP->imagewidth = imagewidth * 72;
    }
    else
        cmdlineP->imagewidth = 0;
    if (imageheight_spec)
    {
        overflow2(imageheight, 72);
        cmdlineP->imageheight = imageheight * 72;
    }
    else
        cmdlineP->imageheight = 0;

    if (argc-1 == 0) 
        cmdlineP->input_filespec = "-";
    else if (argc-1 != 1)
        pm_error("Program takes zero or one argument (filename).  You "
                 "specified %d", argc-1);
    else
        cmdlineP->input_filespec = argv[1];

}



static unsigned int itemsinline;
    /* The number of items in the line we are currently building */
static unsigned int bitsinitem;
    /* The number of bits filled so far in the item we are currently
       building 
    */
static unsigned int rlebitsinitem;
    /* The number of bits filled so far in the item we are currently
       building 
    */
static unsigned int bitspersample;
static unsigned int item, bitshift, items;
static unsigned int rleitem, rlebitshift;
static unsigned int repeat, itembuf[128], count, repeatitem, repeatcount;



static void
init_putitem(bool const rle, unsigned int const bitspersample) {

    itemsinline = 0;
    items = 0;

    if (rle) {
        rleitem = 0;
        rlebitsinitem = 0;
        rlebitshift = 8 - bitspersample;
        repeat = 1;
        count = 0;
    } else {
        item = 0;
        bitsinitem = 0;
        bitshift = 8 - bitspersample;
    }

}

static void
putitem()
    {
    char* hexits = "0123456789abcdef";

    if ( itemsinline == 30 )
    {
    putchar( '\n' );
    itemsinline = 0;
    }
    putchar( hexits[item >> 4] );
    putchar( hexits[item & 15] );
    ++itemsinline;
    ++items;
    item = 0;
    bitsinitem = 0;
    bitshift = 8 - bitspersample;
    }

#if __STDC__
static void putxelval( xelval xv )
#else /*__STDC__*/
static void
putxelval( xv )
    xelval xv;
#endif /*__STDC__*/
    {
    if ( bitsinitem == 8 )
    putitem();
    item += xv << bitshift;
    bitsinitem += bitspersample;
    bitshift -= bitspersample;
    }

static void
putrest()
    {
    if ( bitsinitem > 0 )
    putitem();
    printf( "\n" );
    printf( "grestore\n" );
    printf( "showpage\n" );
    printf( "%%%%Trailer\n" );
    }

static void
rleputbuffer()
    {
    int i;

    if ( repeat )
    {
    item = 256 - count;
    putitem();
    item = repeatitem;
    putitem();
    }
    else
    {
    item = count - 1;
    putitem();
    for ( i = 0; i < count; ++i )
        {
        item = itembuf[i];
        putitem();
        }
    }
    repeat = 1;
    count = 0;
    }

static void
rleputitem()
    {
    int i;

    if ( count == 128 )
    rleputbuffer();

    if ( repeat && count == 0 )
    { /* Still initializing a repeat buf. */
    itembuf[count] = repeatitem = rleitem;
    ++count;
    }
    else if ( repeat )
    { /* Repeating - watch for end of run. */
    if ( rleitem == repeatitem )
        { /* Run continues. */
        itembuf[count] = rleitem;
        ++count;
        }
    else
        { /* Run ended - is it long enough to dump? */
        if ( count > 2 )
        { /* Yes, dump a repeat-mode buffer and start a new one. */
        rleputbuffer();
        itembuf[count] = repeatitem = rleitem;
        ++count;
        }
        else
        { /* Not long enough - convert to non-repeat mode. */
        repeat = 0;
        itembuf[count] = repeatitem = rleitem;
        ++count;
        repeatcount = 1;
        }
        }
    }
    else
    { /* Not repeating - watch for a run worth repeating. */
    if ( rleitem == repeatitem )
        { /* Possible run continues. */
        ++repeatcount;
        if ( repeatcount > 3 )
        { /* Long enough - dump non-repeat part and start repeat. */
        count = count - ( repeatcount - 1 );
        rleputbuffer();
        count = repeatcount;
        for ( i = 0; i < count; ++i )
            itembuf[i] = rleitem;
        }
        else
        { /* Not long enough yet - continue as non-repeat buf. */
        itembuf[count] = rleitem;
        ++count;
        }
        }
    else
        { /* Broken run. */
        itembuf[count] = repeatitem = rleitem;
        ++count;
        repeatcount = 1;
        }
    }

    rleitem = 0;
    rlebitsinitem = 0;
    rlebitshift = 8 - bitspersample;
    }

#if __STDC__
static void rleputxelval( xelval xv )
#else /*__STDC__*/
static void
rleputxelval( xv )
    xelval xv;
#endif /*__STDC__*/
    {
    if ( rlebitsinitem == 8 )
    rleputitem();
    rleitem += xv << rlebitshift;
    rlebitsinitem += bitspersample;
    rlebitshift -= bitspersample;
    }

static void
rleflush()
    {
    if ( rlebitsinitem > 0 )
    rleputitem();
    if ( count > 0 )
    rleputbuffer();
    }

static void
rleputrest()
    {
    rleflush();
    printf( "\n" );
    printf( "grestore\n" );
    printf( "showpage\n" );
    printf( "%%%%Trailer\n" );
    }



static void
compute_image_position(int const dpiX, int const dpiY, 
                       int const icols, int const irows,
                       bool const mustturn, bool const canturn,
                       bool const center,
                       int const pagewid, int const pagehgt, 
                       float const requested_scale,
                       float const imagewidth, float const imageheight,
                       bool const equalpixels,
                       float * const scolsP, float * const srowsP,
                       float * const llxP, float * const llyP,
                       bool * const turnedP ) {
/*----------------------------------------------------------------------------
   Determine where on the page the image is to go.  This means position,
   dimensions, and orientation.

   icols/irows are the dimensions of the PNM input in xels.

   'mustturn' means we are required to rotate the image.

   'canturn' means we may rotate the image if it fits better, but don't
   have to.

   *scolsP,*srowsP are the dimensions of the image in 1/72 inch.

   *llxP,*llyP are the coordinates, in 1/72 inch, of the lower left
   corner of the image on the page.

   *turnedP is true iff the image is to be rotated 90 degrees on the page.

   imagewidth/imageheight are the requested dimensions of the image on
   the page, in 1/72 inch.  Image will be as large as possible within
   those dimensions.  Zero means unspecified, so 'scale', 'pagewid',
   'pagehgt', 'irows', and 'icols' determine image size.

   'equalpixels' means the user wants one printed pixel per input pixel.
   It is inconsistent with imagewidth or imageheight != 0

   'requested_scale' is meaningful only when imageheight/imagewidth == 0
   and equalpixels == FALSE.  It tells how many inches the user wants
   72 pixels of input to occupy, if it fits on the page.
-----------------------------------------------------------------------------*/
    int cols, rows;
        /* Number of columns, rows of input xels in the output, as
           rotated if applicable
        */
    bool shouldturn;  /* The image fits the page better if we turn it */
    
    if (icols > irows && pagehgt > pagewid)
        shouldturn = TRUE;
    else if (irows > icols && pagewid > pagehgt)
        shouldturn = TRUE;
    else
        shouldturn = FALSE;

    if (mustturn || (canturn && shouldturn)) {
        *turnedP = TRUE;
        cols = irows;
        rows = icols;
    } else {
        *turnedP = FALSE;
        cols = icols;
        rows = irows;
    }
    if (equalpixels) {
        *scolsP = (72.0/dpiX)*cols;
        *srowsP = (72.0/dpiY)*rows;
    } else if (imagewidth > 0 || imageheight > 0) {
        float scale;

        if (imagewidth == 0)
            scale = (float) imageheight/rows;
        else if (imageheight == 0)
            scale = (float) imagewidth/cols;
        else
            scale = min((float)imagewidth/cols, (float)imageheight/rows);
        
        *scolsP = cols*scale;
        *srowsP = rows*scale;
    } else {
        /* He didn't give us a bounding box for the image so figure
           out output image size from other inputs.
        */
        const int devpixX = dpiX / 72.0 + 0.5;        
        const int devpixY = dpiY / 72.0 + 0.5;        
            /* How many device pixels make up 1/72 inch, rounded to
               nearest integer */
        const float pixfacX = 72.0 / dpiX * devpixX;  /* 1, approx. */
        const float pixfacY = 72.0 / dpiY * devpixY;  /* 1, approx. */
        float scale;

        scale = min(requested_scale, 
                    min((float)pagewid/cols, (float)pagehgt/rows));

        *scolsP = scale * cols * pixfacX;
        *srowsP = scale * rows * pixfacY;
        
        if (scale != requested_scale)
            pm_message("warning, image too large for page, rescaling to %g", 
                       scale );

        /* Before May 2001, Pnmtops enforced a 5% margin around the page.
           If the image would be too big to leave a 5% margin, Pnmtops would
           scale it down.  But people have images that are exacty the size
           of a page, e.g. because they created them with Sane's 'scanimage'
           program from a full page of input.  So we removed the gratuitous
           5% margin.  -Bryan.
        */
    }
    *llxP = (center) ? ( pagewid - *scolsP ) / 2 : 0;
    *llyP = (center) ? ( pagehgt - *srowsP ) / 2 : 0;
}



static void
putinit(const char name[], int const icols, int const irows, 
        float const scols, float const srows,
        float const llx, float const lly,
        int const padright, int const bps,
        int const pagewid, int const pagehgt,
        bool const color, bool const turned, int const rle,
        bool const setpage) {
/*----------------------------------------------------------------------------
   Write out to Standard Output the headers stuff for the Postscript
   file (everything up to the raster).
-----------------------------------------------------------------------------*/
    printf( "%%!PS-Adobe-2.0 EPSF-2.0\n" );
    printf( "%%%%Creator: pnmtops\n" );
    printf( "%%%%Title: %s.ps\n", name );
    printf( "%%%%Pages: 1\n" );
    printf(
        "%%%%BoundingBox: %d %d %d %d\n",
        (int) llx, (int) lly,
        (int) ( llx + scols + 0.5 ), (int) ( lly + srows + 0.5 ) );
    printf( "%%%%EndComments\n" );
    if (rle) {
        printf( "/rlestr1 1 string def\n" );
        printf( "/readrlestring {\n" );             /* s -- nr */
        printf( "  /rlestr exch def\n" );           /* - */
        printf( "  currentfile rlestr1 readhexstring pop\n" );  /* s1 */
        printf( "  0 get\n" );                  /* c */
        printf( "  dup 127 le {\n" );               /* c */
        printf( "    currentfile rlestr 0\n" );         /* c f s 0 */
        printf( "    4 3 roll\n" );             /* f s 0 c */
        printf( "    1 add  getinterval\n" );           /* f s */
        printf( "    readhexstring pop\n" );            /* s */
        printf( "    length\n" );               /* nr */
        printf( "  } {\n" );                    /* c */
        printf( "    256 exch sub dup\n" );         /* n n */
        printf( "    currentfile rlestr1 readhexstring pop\n" );/* n n s1 */
        printf( "    0 get\n" );                /* n n c */
        printf( "    exch 0 exch 1 exch 1 sub {\n" );       /* n c 0 1 n-1*/
        printf( "      rlestr exch 2 index put\n" );
        printf( "    } for\n" );                /* n c */
        printf( "    pop\n" );                  /* nr */
        printf( "  } ifelse\n" );               /* nr */
        printf( "} bind def\n" );
        printf( "/readstring {\n" );                /* s -- s */
        printf( "  dup length 0 {\n" );             /* s l 0 */
        printf( "    3 copy exch\n" );              /* s l n s n l*/
        printf( "    1 index sub\n" );              /* s l n s n r*/
        printf( "    getinterval\n" );              /* s l n ss */
        printf( "    readrlestring\n" );            /* s l n nr */
        printf( "    add\n" );                  /* s l n */
        printf( "    2 copy le { exit } if\n" );        /* s l n */
        printf( "  } loop\n" );                 /* s l l */
        printf( "  pop pop\n" );                /* s */
        printf( "} bind def\n" );
    } else {
        printf( "/readstring {\n" );                /* s -- s */
        printf( "  currentfile exch readhexstring pop\n" );
        printf( "} bind def\n" );
    }
    if (color) {
        printf( "/rpicstr %d string def\n", ( icols + padright ) * bps / 8 );
        printf( "/gpicstr %d string def\n", ( icols + padright ) * bps / 8 );
        printf( "/bpicstr %d string def\n", ( icols + padright ) * bps / 8 );
    } else
        printf( "/picstr %d string def\n", ( icols + padright ) * bps / 8 );
    printf( "%%%%EndProlog\n" );
    printf( "%%%%Page: 1 1\n" );
    if (setpage)
        printf( "<< /PageSize [ %d %d ] /ImagingBBox null >> setpagedevice\n",
                pagewid, pagehgt);
    printf( "gsave\n" );
    printf( "%g %g translate\n", llx, lly );
    printf( "%g %g scale\n", scols, srows );
    if ( turned )
        printf( "0.5 0.5 translate  90 rotate  -0.5 -0.5 translate\n" );
    printf( "%d %d %d\n", icols, irows, bps );
    printf( "[ %d 0 0 -%d 0 %d ]\n", icols, irows, irows );
    if (color) {
        printf( "{ rpicstr readstring }\n" );
        printf( "{ gpicstr readstring }\n" );
        printf( "{ bpicstr readstring }\n" );
        printf( "true 3\n" );
        printf( "colorimage\n" );
        pm_message( "writing color PostScript..." );
    } else {
        printf( "{ picstr readstring }\n" );
        printf( "image\n" );
    }
}



static void
compute_depth(xelval const input_maxval, unsigned int * const bitspersampleP,
              unsigned int * const ps_maxvalP) {
/*----------------------------------------------------------------------------
   Figure out how many bits will represent each sample in the Postscript
   output, and the maxval of the Postscript output samples.  The maxval
   is just the maximum value allowable in the number of bits.
-----------------------------------------------------------------------------*/
    unsigned int const bitsRequiredByMaxval = pm_maxvaltobits(input_maxval);

    if (bitsRequiredByMaxval <= 2)
        *bitspersampleP = bitsRequiredByMaxval;
    else if (bitsRequiredByMaxval > 2 && bitsRequiredByMaxval <= 4)
        *bitspersampleP = 4;
    else 
        *bitspersampleP = 8;

    if (*bitspersampleP < bitsRequiredByMaxval)
        pm_message("Maxval of input requires %u bit samples for full "
                   "resolution, but we are using the Postscript maximum "
                   "of %u", bitsRequiredByMaxval, *bitspersampleP);

    *ps_maxvalP = pm_bitstomaxval(*bitspersampleP);
}    



static void
convert_row(struct pam * const pamP, tuple * const tuplerow, 
            unsigned int const ps_maxval, const bool rle, 
            unsigned int const padright) {

    unsigned int plane;

    for (plane = 0; plane < pamP->depth; ++plane) {
        unsigned int col;
        for (col= 0; col < pamP->width; ++col) 
            if ( rle )
                rleputxelval(tuplerow[col][plane]);
            else
                putxelval(tuplerow[col][plane]);
        for (col = 0; col < padright; ++col)
            if (rle)
                rleputxelval(0);
        else
            putxelval(0);
        if (rle)
            rleflush();
    }
}


static void
convert_page(FILE *ifp, const int turnflag, const int turnokflag, 
             const int rle, const int setpage, 
             const int center, const float scale,
             const int dpiX, const int dpiY, 
             const int pagewid, const int pagehgt,
             const int imagewidth, const int imageheight, 
             const bool equalpixels,
             const char name[]) {

    struct pam inpam;
    tuple* tuplerow;
    unsigned int padright;
        /* Number of bits we must add to the right end of each Postscript
           output line in order to have an integral number of bytes of output.
           E.g. at 2 bits per sample with 10 columns, this would be 4.
        */
    int row;
    unsigned int ps_maxval;
        /* The maxval of the Postscript output */
    float scols, srows;
    float llx, lly;
    bool turned;

    pnm_readpaminit(ifp, &inpam, sizeof(inpam));
    tuplerow = pnm_allocpamrow(&inpam);
    
    if (!STRSEQ(inpam.tuple_type, PAM_PBM_TUPLETYPE) &&
        !STRSEQ(inpam.tuple_type, PAM_PGM_TUPLETYPE) &&
        !STRSEQ(inpam.tuple_type, PAM_PPM_TUPLETYPE))
        pm_error("Unrecognized tuple type %s.  This program accepts only "
                 "PBM, PGM, PPM, and equivalent PAM input images", 
                 inpam.tuple_type);

    compute_depth(inpam.maxval, &bitspersample, &ps_maxval);
    {
        unsigned int const realBitsPerLine = inpam.width * bitspersample;
        unsigned int const paddedBitsPerLine = ((realBitsPerLine + 7) / 8) * 8;
        padright = (paddedBitsPerLine - realBitsPerLine) / bitspersample;
    }
    /* In positioning/scaling the image, we treat the input image as if
       it has a density of 72 pixels per inch.
    */
    compute_image_position(dpiX, dpiY, inpam.width, inpam.height, 
                           turnflag, turnokflag, center,
                           pagewid, pagehgt, scale, imagewidth, imageheight,
                           equalpixels,
                           &scols, &srows, &llx, &lly, &turned);

    putinit(name, inpam.width, inpam.height, 
            scols, srows, llx, lly, padright, bitspersample, 
            pagewid, pagehgt,
            STRSEQ(inpam.tuple_type, PAM_PPM_TUPLETYPE), 
            turned, rle, setpage );

    init_putitem(rle, bitspersample);

    for (row = 0; row < inpam.height; ++row) {
        pnm_readpamrow(&inpam, tuplerow);
        convert_row(&inpam, tuplerow, ps_maxval, rle, padright);
    }

    if (rle)
        rleputrest();
    else
        putrest();

    pnm_freepamrow(tuplerow);
}



int
main(int argc, char * argv[]) {

    FILE* ifp;
    char *name;  /* malloc'ed */
    struct cmdline_info cmdline;

    pnm_init( &argc, argv );

    parse_command_line(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.input_filespec);

    if (strcmp(cmdline.input_filespec, "-") == 0 )
        name = strdup("noname");
    else {
        /* Set name to filename up to first period */
        char* cp;
        name = strdup(cmdline.input_filespec);
        cp = strchr(name, '.');
        if (cp)
            *cp = '\0';
    }
    {
        int eof;  /* There are no more images in the input file */
        unsigned int image_seq;

        /* I don't know if this works at all for multi-image PNM input.
           Before July 2000, it ignored everything after the first image,
           so this probably is at least as good -- it should be identical
           for a single-image file, which is the only kind which was legal
           before July 2000.

           Maybe there needs to be some per-file header and trailers stuff
           in the Postscript output, with some per-page header and trailer
           stuff inside.  I don't know Postscript.  - Bryan 2000.06.19.
        */

        eof = FALSE;  /* There is always at least one image */
        for (image_seq = 0; !eof; image_seq++) {
            convert_page(ifp, cmdline.mustturn, cmdline.canturn, 
                         cmdline.rle, cmdline.setpage, 
                         cmdline.center, cmdline.scale,
                         cmdline.dpiX, cmdline.dpiY,
                         cmdline.width, cmdline.height, 
                         cmdline.imagewidth, cmdline.imageheight, 
                         cmdline.equalpixels, name);
            pnm_nextimage(ifp, &eof);
        }
    }
    free(name);

    pm_close(ifp);
    
    exit(0);
}



