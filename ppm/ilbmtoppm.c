/* ilbmtoppm.c - read an IFF ILBM file and produce a portable pixmap
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
** Modified by Mark Thompson on 10/4/90 to accomodate 24-bit IFF files
** as used by ASDG, NewTek, etc.
**
** Modified by Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
**  20/Jun/93:
**  - row-by-row operation
**  - better de-interleave algorithm
**  - colormap files
**  - direct color
**  04/Oct/93:
**  - multipalette support (PCHG chunk)
**  - options -ignore, -isham, -isehb and -adjustcolors
**  22/May/94:
**  - minor change: check first for 24 planes, then for HAM
**  21/Sep/94:
**  - write mask plane to a file if -maskfile option used
**  - write colormap file
**  - added sliced HAM/dynamic HAM/dynamic Hires multipalette formats (SHAM, CTBL chunk)
**  - added color lookup tables (CLUT chunk)
**  - major rework of colormap/multipalette handling
**  - now uses numeric IFF IDs
**  24/Oct/94:
**  - transparentColor support
**  - added RGBN/RGB8 image types
**  - 24-bit & direct color modified to n-bit deep ILBM
**  22/Feb/95:
**  - direct color (DCOL) reimplemented
**  29/Mar/95
**  - added IFF-PBM format
*/

#include <string.h>

#include "ppm.h"
#include "ppmcmap.h"
#include "ilbm.h"

/*#define DEBUG*/

typedef struct {
    int reg;            /* color register to change */
    pixval r, g, b;     /* new colors for register */
} PaletteChange;
typedef struct {
    pixel *color;
    int    ncolors;
    /* lookup tables */
    unsigned char *redlut;
    unsigned char *greenlut;
    unsigned char *bluelut;
    unsigned char *monolut;
    /* multipalette stuff */
    PaletteChange *mp_init;
    PaletteChange **mp_change;
    int mp_rows;                /* # of rows in change array */
    int mp_type;                /* see below, higher types preferred */
    int mp_flags;
    IFF_ID  mp_id;
} ColorMap;
#define HAS_COLORMAP(cmap)      ((cmap) && (cmap)->color)
#define HAS_COLORLUT(cmap)      ((cmap) && ((cmap)->redlut || (cmap)->greenlut || (cmap)->bluelut))
#define HAS_MONOLUT(cmap)       ((cmap) && (cmap)->monolut)
#define HAS_MULTIPALETTE(cmap)  (HAS_COLORMAP(cmap) && (cmap)->mp_type)
#define MP_TYPE_SHAM        1
#define MP_TYPE_CTBL        2
#define MP_TYPE_PCHG        3
#define MP_REG_IGNORE       -1
#define MP_REG_END          -2
#define MP_FLAGS_SKIPLACED   (1<<0)


/* prototypes */
static unsigned char get_byte ARGS((FILE* f, IFF_ID iffid, long *counter));
static long get_big_long ARGS((FILE *f, IFF_ID iffid, long *counter));
static short get_big_short ARGS((FILE *f, IFF_ID iffid, long *counter));
static void read_bytes ARGS((FILE *ifp, int bytes, unsigned char *buffer, IFF_ID iffid, long *counter));
static char *ID2string ARGS((IFF_ID iffid));

static void chunk_end ARGS((FILE *ifp, IFF_ID iffid, long chunksize));
static void skip_chunk ARGS((FILE *f, IFF_ID iffid, long chunksize));
static void display_chunk ARGS((FILE *ifp, IFF_ID iffid, long chunksize));
static BitMapHeader * read_bmhd ARGS((FILE *f, IFF_ID iffid, long chunksize));
static ColorMap * read_cmap ARGS((FILE *f, IFF_ID iffid, long chunksize, ColorMap *cmap));
static ColorMap * read_cmyk ARGS((FILE *f, IFF_ID iffid, long chunksize, ColorMap *cmap));
static ColorMap * read_clut ARGS((FILE *f, IFF_ID iffid, long chunksize, ColorMap *cmap));
static ColorMap * read_4bit_mp ARGS((FILE *ifp, IFF_ID iffid, long chunksize, ColorMap *cmap));
static ColorMap * read_pchg ARGS((FILE *ifp, IFF_ID iffid, long chunksize, ColorMap *cmap));

static void ham_to_ppm ARGS((FILE *ifp, long chunksize, BitMapHeader *bmhd, ColorMap *colormap, long viewportmodes));
static void deep_to_ppm ARGS((FILE *ifp, long chunksize, BitMapHeader *bmhd, ColorMap *cmap));
static void cmap_to_ppm ARGS((ColorMap *colormap));
static void std_to_ppm ARGS((FILE *ifp, long chunksize, BitMapHeader *bmhd, ColorMap *colormap, long viewportmodes));
static void rgbn_to_ppm ARGS((FILE *ifp, long chunksize, BitMapHeader *bmhd, ColorMap *cmap));
static void dcol_to_ppm ARGS((FILE *ifp, long chunksize, BitMapHeader *bmhd, ColorMap *cmap, DirectColor *dcol));
static void ipbm_to_ppm ARGS((FILE *ifp, long chunksize, BitMapHeader *bmhd, ColorMap *cmap, long viewportmodes));

static ColorMap* ehbcmap ARGS((ColorMap *colormap));
static void read_ilbm_plane ARGS((FILE *ifp, long *chunksizeP, int bytes, int compression));
static void decode_row ARGS((FILE *ifp, long *chunksizeP, rawtype *chunkyrow, int planes, BitMapHeader *bmhd));
static void decode_mask ARGS((FILE *ifp, long *chunksizeP, rawtype *chunkyrow, BitMapHeader *bmhd));

static void get_color ARGS((ColorMap *cmap, int idx, pixval *r, pixval *g, pixval *b));
static pixval lookup_red ARGS((ColorMap *cmap, int oldval));
static pixval lookup_green ARGS((ColorMap *cmap, int oldval));
static pixval lookup_blue ARGS((ColorMap *cmap, int oldval));
static pixval lookup_mono ARGS((ColorMap *cmap, int oldval));
static pixval lut_maxval ARGS((ColorMap *cmap, pixval maxval));

static void multi_init ARGS((ColorMap *cmap, long viewportmodes));
static void multi_update ARGS((ColorMap *cmap, int row));
static void multi_free ARGS((ColorMap *cmap));

static rawtype * alloc_rawrow ARGS((int cols));
static ColorMap * alloc_cmap ARGS((void));
static void check_cmap ARGS((BitMapHeader *bmhd, ColorMap *cmap));
static void * xmalloc ARGS((int bytes));
static void * xmalloc2 ARGS((int x, int y));
#define MALLOC(n, type)     (type *)xmalloc2((n), sizeof(type))

#define FACTOR_4BIT     17      /* scale factor maxval 15 -> maxval 255 */

static short verbose = 0;
static short adjustcolors = 0;
static unsigned char *ilbmrow;
static pixel *pixelrow;
static FILE *maskfile = NULL;
static bit *maskrow = NULL;
static short wrotemask = 0;
static IFF_ID typeid;       /* ID_ILBM, ID_RGBN, ID_RGB8 */

static pixel *transpColor = NULL;       /* transparent color */
static short transpIndex = -1;
static char *transpName = NULL;

int
main(argc, argv)
    int argc;
    char *argv[];
{
    FILE *ifp;
    int argn;
    short body = 0, cmaponly = 0, isdeep = 0, endchunk = 0;
    long formsize, bytesread, chunksize, viewportmodes = 0, fakeviewport = 0, viewportmask;
    IFF_ID iffid;
    BitMapHeader *bmhd = NULL;
    ColorMap *cmap = NULL;
    DirectColor *dcol = NULL;
#define MAX_IGNORE  16
    IFF_ID ignorelist[MAX_IGNORE];
    int ignorecount = 0;
    char *maskname;
    char *usage =
"[-verbose] [-ignore <chunkID> [-ignore <chunkID>] ...]\
 [-isham|-isehb|-isdeep|-isnotham|-isnotehb|-isnotdeep]\
 [-cmaponly] [-adjustcolors]\
 [-transparent <color>] [-maskfile <filename>] [ilbmfile]";

    ppm_init(&argc, argv);

    viewportmask = 0xffffffff;

    argn = 1;
    while( argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0' ) {
        if( pm_keymatch(argv[argn], "-verbose", 2) )
            verbose = 1;
        else
        if( pm_keymatch(argv[argn], "-noverbose", 4) )
            verbose = 0;
        else
        if( pm_keymatch(argv[argn], "-isham", 4) )
            fakeviewport |= vmHAM;
        else
        if( pm_keymatch(argv[argn], "-isehb", 4) )
            fakeviewport |= vmEXTRA_HALFBRITE;
        else
        if( pm_keymatch(argv[argn], "-isdeep", 4) )
            isdeep = 1;
        else
        if( pm_keymatch(argv[argn], "-isnotham", 7) )
            viewportmask &= ~(vmHAM);
        else
        if( pm_keymatch(argv[argn], "-isnotehb", 7) )
            viewportmask &= ~(vmEXTRA_HALFBRITE);
        else
        if( pm_keymatch(argv[argn], "-isnotdeep", 7) )
            isdeep = -1;
        else
        if( pm_keymatch(argv[argn], "-cmaponly", 2) )
            cmaponly = 1;
        else
        if( pm_keymatch(argv[argn], "-adjustcolors", 2) )
            adjustcolors = 1;
        else
        if( pm_keymatch(argv[argn], "-noadjustcolors", 4) )
            adjustcolors = 0;
        else
        if( pm_keymatch(argv[argn], "-transparent", 2) ) {
            if( ++argn >= argc )
                pm_usage(usage);
            transpName = argv[argn];
        }
        else
        if( pm_keymatch(argv[argn], "-maskfile", 2) ) {
            if( ++argn >= argc )
                pm_usage(usage);
            maskname = argv[argn];
            maskfile = pm_openw(maskname);
        }
        else
        if( pm_keymatch(argv[argn], "-ignore", 2) ) {
            if( ++argn >= argc )
                pm_usage(usage);
            if( strlen(argv[argn]) != 4 )
                pm_error("\"-ignore\" option needs a 4 byte chunk ID string as argument");
            if( ignorecount >= MAX_IGNORE )
                pm_error("max %d chunk IDs to ignore", MAX_IGNORE);
            ignorelist[ignorecount++] = MAKE_ID(argv[argn][0], argv[argn][1], argv[argn][2], argv[argn][3]);
        }
        else
            pm_usage(usage);
        ++argn;
    }

    if( argn < argc ) {
        ifp = pm_openr( argv[argn] );
        argn++;
    }
    else
        ifp = stdin;

    if( argn != argc )
        pm_usage(usage);

    /* Read in the ILBM file. */
    iffid = get_big_long(ifp, ID_FORM, NULL);
    if( iffid != ID_FORM )
        pm_error("input is not a FORM type IFF file");
    formsize = get_big_long(ifp, ID_FORM, NULL);
    typeid = get_big_long(ifp, ID_FORM, NULL);
    if( typeid != ID_ILBM && typeid != ID_RGBN && typeid != ID_RGB8 && typeid != ID_PBM )
        pm_error( "input is not an ILBM, RGBN, RGB8 or PBM type FORM IFF file" );
    bytesread = 4;  /* FORM and formsize do not count */

    /* Main loop, parsing the IFF FORM. */
    while( !endchunk && formsize-bytesread >= 8 ) {
        short i, ignore = 0;

        iffid = get_big_long(ifp, ID_FORM, NULL);
        chunksize = get_big_long(ifp, iffid, NULL);
        bytesread += 8;

        for( i = 0; i < ignorecount && !ignore; i++ ) {
            if( iffid == ignorelist[i] )
                ignore = 1;
        }

#ifdef DEBUG
            pm_message("reading %s chunk: %ld bytes", ID2string(iffid), chunksize);
#endif
        if( ignore ) {
            ignore = 0;
            pm_message("ignoring %s chunk", ID2string(iffid));
            skip_chunk(ifp, iffid, chunksize);
        }
        else
        if( iffid == ID_END ) {
            /* END chunks are not officially supported in IFF, but
                suggested as a future expansion for stream-writing,
                see Amiga RKM Devices, 3rd Ed, page 376 */
            if( chunksize != 0 ) {
                pm_message("warning - non-0 %s chunk", ID2string(iffid));
                skip_chunk(ifp, iffid, chunksize);
            }
            if( formsize != 0xffffffff )
                pm_message("warning - %s chunk with FORM size 0x%08lx "
                           "(should be 0x%08x)",
                            ID2string(iffid), formsize, 0xffffffff);
            endchunk = 1;
        }
        else
        if( body != 0 ) {
            pm_message("%s chunk found after %s chunk - skipping", ID2string(iffid), ID2string(ID_BODY));
            skip_chunk(ifp, iffid, chunksize);
        }
        else
        switch( iffid ) {
            case ID_BMHD:
                bmhd = read_bmhd(ifp, iffid, chunksize);
                if( bmhd->nPlanes == 0 )
                    cmaponly = 1;
                break;
            case ID_CMAP:
                cmap = read_cmap(ifp, iffid, chunksize, cmap);
                break;
            case ID_CMYK:
                cmap = read_cmyk(ifp, iffid, chunksize, cmap);
                break;
            case ID_CLUT:
                cmap = read_clut(ifp, iffid, chunksize, cmap);
                break;
            case ID_CAMG:
                if( chunksize != CAMGChunkSize )
                    pm_error("%s chunk size mismatch", ID2string(iffid));
                viewportmodes = get_big_long(ifp, ID_CAMG, NULL);
                viewportmodes &= viewportmask;      /* -isnotham/-isnotehb */
                break;
            case ID_PCHG:
                cmap = read_pchg(ifp, iffid, chunksize, cmap);
                break;
            case ID_CTBL:
            case ID_SHAM:
                cmap = read_4bit_mp(ifp, iffid, chunksize, cmap);
                break;
            case ID_DCOL:
                if( chunksize != DirectColorSize )
                    pm_error("%s chunk size mismatch", ID2string(iffid));
                dcol = MALLOC(1, DirectColor);
                dcol->r = get_byte(ifp, iffid, NULL);
                dcol->g = get_byte(ifp, iffid, NULL);
                dcol->b = get_byte(ifp, iffid, NULL);
                (void)get_byte(ifp, iffid, NULL);       /* skip pad byte */
                break;
            case ID_BODY:
                body = 1;

                if( bmhd == NULL )
                    pm_error("%s chunk without %s chunk", ID2string(ID_BODY), ID2string(ID_BMHD));

                if( cmaponly ) {
                    skip_chunk(ifp, iffid,  chunksize);
                    break;
                }

                check_cmap(bmhd, cmap);

                pixelrow = ppm_allocrow(bmhd->w);
                if( maskfile ) {
                    maskrow = pbm_allocrow(bmhd->w);
                    pbm_writepbminit(maskfile, bmhd->w, bmhd->h, 0);
                }

                if( typeid == ID_ILBM ) {
                    overflow_add(bmhd->w, 15);
                    ilbmrow = MALLOC(RowBytes(bmhd->w), unsigned char);
                    viewportmodes |= fakeviewport;      /* -isham/-isehb */

                    if( isdeep > 0 && (bmhd->nPlanes % 3 != 0) ) {
                        pm_message("cannot interpret %d-plane image as \'deep\' (# of planes must be divisible by 3)", bmhd->nPlanes);
                        isdeep = 0;
                    }

                    if( isdeep > 0 )
                        deep_to_ppm(ifp, chunksize, bmhd, cmap);
                    else
                    if( dcol )
                        dcol_to_ppm(ifp, chunksize, bmhd, cmap, dcol);
                    else
                    if( bmhd->nPlanes > 8 ) {
                        if( bmhd->nPlanes <= 16 && HAS_COLORMAP(cmap) )
                            std_to_ppm(ifp, chunksize, bmhd, cmap, viewportmodes);
                        else
                        if( isdeep >= 0 && (bmhd->nPlanes % 3 == 0) )
                            deep_to_ppm(ifp, chunksize, bmhd, cmap);
                        else
                        if( bmhd->nPlanes <= 16 )   /* will be interpreted as grayscale */
                            std_to_ppm(ifp, chunksize, bmhd, cmap, viewportmodes);
                        else
                            pm_error("don\'t know how to interpret %d-plane image", bmhd->nPlanes);
                    }
                    else
                        std_to_ppm(ifp, chunksize, bmhd, cmap, viewportmodes);
                }
                else
                if( typeid == ID_PBM )
                    ipbm_to_ppm(ifp, chunksize, bmhd, cmap, viewportmodes);
                else   /* RGBN or RGB8 */
                    rgbn_to_ppm(ifp, chunksize, bmhd, cmap);
                break;
            case ID_GRAB:   case ID_DEST:   case ID_SPRT:   case ID_CRNG:
            case ID_CCRT:   case ID_DYCP:   case ID_DPPV:   case ID_DRNG:
            case ID_EPSF:   case ID_JUNK:   case ID_CNAM:   case ID_PRVW:
                skip_chunk(ifp, iffid, chunksize);
                break;
            case ID_copy:   case ID_AUTH:   case ID_NAME:   case ID_ANNO:
            case ID_TEXT:   case ID_FVER:
                if( verbose )
                    display_chunk(ifp, iffid, chunksize);
                else
                    skip_chunk(ifp, iffid, chunksize);
                break;
            case ID_DPI:
                {
                    int x, y;

                    x = get_big_short(ifp, ID_DPI, NULL);
                    y = get_big_short(ifp, ID_DPI, NULL);
                    if( verbose )
                        pm_message("%s chunk:  dpi_x = %d    dpi_y = %d", ID2string(ID_DPI), x, y);
                }
                break;
            default:
                pm_message("unknown chunk type %s - skipping", ID2string(iffid));
                skip_chunk(ifp, iffid, chunksize);
                break;
        }

        bytesread += chunksize;
        if( odd(chunksize) ) {
            (void) get_byte(ifp, iffid, NULL);
            ++bytesread;
        }
    }

    if( maskfile ) {
        pm_close(maskfile);
        if( !wrotemask )
            remove(maskname);
        pbm_freerow(maskrow);
    }

    if( cmaponly ) {
        if( HAS_COLORMAP(cmap) ) {
            check_cmap(bmhd, cmap);
            cmap_to_ppm(cmap);
        }
        else
            pm_error("no colormap");
    }
    else
    if( body == 0 ) {
        if( HAS_COLORMAP(cmap) ) {
            pm_message("input is a colormap file");
            check_cmap(bmhd, cmap);
            cmap_to_ppm(cmap);
        }
        else
            pm_error("no %s or %s chunk found", ID2string(ID_BODY), ID2string(ID_CMAP));
    }

    for(chunksize = 0; fgetc(ifp) != EOF; chunksize++ )
        bytesread++;
    pm_close(ifp);

    if( chunksize )
        pm_message("skipped %ld extraneous byte%s after last chunk",
                    chunksize, (chunksize == 1 ? "" : "s"));

    if( !endchunk && bytesread != formsize ) {
        pm_message("warning - file length/FORM size field mismatch (%ld != %ld+8)",
                    bytesread+8 /* FORM+size */, formsize);
    }

    exit(0);
}


/****************************************************************************
 Chunk reader
 ****************************************************************************/

static void
chunk_end(ifp, iffid, chunksize)
    FILE *ifp;
    IFF_ID iffid;
    long chunksize;
{
    if( chunksize ) {
        pm_message("warning - %ld extraneous byte%s in %s chunk",
                    chunksize, (chunksize == 1 ? "" : "s"), ID2string(iffid));
        while( chunksize )
            (void) get_byte(ifp, iffid, &chunksize);
    }
}


static void
skip_chunk(ifp, iffid, chunksize)
    FILE *ifp;
    IFF_ID iffid;
    long chunksize;
{
    while( chunksize )
        (void) get_byte(ifp, iffid, &chunksize);
}


static void
display_chunk(ifp, iffid, chunksize)
    FILE *ifp;
    IFF_ID iffid;
    long chunksize;
{
    int byte;

    pm_message("contents of %s chunk:", ID2string(iffid));

    while( chunksize ) {
        byte = get_byte(ifp, iffid, &chunksize);
        if( fputc(byte, stderr) == EOF )
            pm_error("write error");
    }
    if( byte != '\n' )
        if( fputc('\n', stderr) == EOF )
            pm_error("write error");
}


static ColorMap *
read_cmap(ifp, iffid, chunksize, cmap)
    FILE *ifp;
    IFF_ID iffid;
    long chunksize;
    ColorMap *cmap;
{
    int i, r, g, b;
    long colors;

    colors = chunksize / 3;
    if( colors == 0 ) {
        pm_error("warning - empty %s colormap", ID2string(iffid));
        skip_chunk(ifp, iffid, chunksize);
        return cmap;
    }

    if( !cmap )
        cmap = alloc_cmap();
    else
    if( cmap->color )               /* prefer CMAP-chunk over CMYK-chunk */
        ppm_freerow(cmap->color);
    cmap->color = ppm_allocrow(colors);
    cmap->ncolors = colors;

    for( i = 0; i < colors; i++ ) {
        r = get_byte(ifp, iffid, &chunksize);
        g = get_byte(ifp, iffid, &chunksize);
        b = get_byte(ifp, iffid, &chunksize);
        PPM_ASSIGN(cmap->color[i], r, g, b);
    }
    chunk_end(ifp, iffid, chunksize);
    return cmap;
}


static ColorMap *
read_cmyk(ifp, iffid, chunksize, cmap)
    FILE *ifp;
    IFF_ID iffid;
    long chunksize;
    ColorMap *cmap;
{
    long colors;
    int i;

    if( HAS_COLORMAP(cmap) ) {      /* prefer RGB color map */
        skip_chunk(ifp, iffid, chunksize);
        return cmap;
    }

    colors = chunksize/4;
    if( colors == 0 ) {
        pm_error("warning - empty %s colormap", ID2string(iffid));
        skip_chunk(ifp, iffid, chunksize);
        return cmap;
    }
    if( !cmap )
        cmap = alloc_cmap();
    cmap->color = ppm_allocrow(colors);
    cmap->ncolors = colors;

    for( i = 0; i <colors; i++ ) {
        int c, m, y, k;
        c = get_byte(ifp, iffid, &chunksize);
        m = get_byte(ifp, iffid, &chunksize);
        y = get_byte(ifp, iffid, &chunksize);
        k = get_byte(ifp, iffid, &chunksize);

        {
            pixval const red = 
                MAXCOLVAL - min(MAXCOLVAL, c*(MAXCOLVAL-k)/MAXCOLVAL+k); 
            pixval const green = 
                MAXCOLVAL - min(MAXCOLVAL, m*(MAXCOLVAL-k)/MAXCOLVAL+k);
            pixval const blue = 
                MAXCOLVAL - min(MAXCOLVAL, y*(MAXCOLVAL-k)/MAXCOLVAL+k);

            PPM_ASSIGN(cmap->color[i], red, green, blue);
        }
    }
    chunk_end(ifp, iffid, chunksize);
    return cmap;
}


static ColorMap *
read_clut(ifp, iffid, chunksize, cmap)
    FILE *ifp;
    IFF_ID iffid;
    long chunksize;
    ColorMap *cmap;
{
    long type;
    unsigned char *lut;
    int i;

    if( chunksize != CLUTSize ) {
        pm_message("invalid size for %s chunk - skipping it", ID2string(iffid));
        skip_chunk(ifp, iffid, chunksize);
        return cmap;
    }

    if( !cmap )
        cmap = alloc_cmap();

    type = get_big_long(ifp, iffid, &chunksize);
    (void) get_big_long(ifp, iffid, &chunksize);    /* skip reserved field */

    lut = MALLOC(256, unsigned char);
    for( i = 0; i < 256; i++ )
        lut[i] = get_byte(ifp, iffid, &chunksize);

    switch( type ) {
        case CLUT_MONO:
            cmap->monolut = lut;
            break;
        case CLUT_RED:
            cmap->redlut = lut;
            break;
        case CLUT_GREEN:
            cmap->greenlut = lut;
            break;
        case CLUT_BLUE:
            cmap->bluelut = lut;
            break;
        default:
            pm_message("warning - %s type %ld not supported", ID2string(iffid), type);
            free(lut);
    }
    return cmap;
}


static BitMapHeader *
read_bmhd(ifp, iffid, chunksize)
    FILE *ifp;
    IFF_ID iffid;
    long chunksize;
{
    BitMapHeader *bmhd;

    if( chunksize != BitMapHeaderSize ) {
        pm_message("invalid size for %s chunk - skipping it", ID2string(iffid));
        skip_chunk(ifp, iffid, chunksize);
        return NULL;
    }

    bmhd = MALLOC(1, BitMapHeader);

    bmhd->w = get_big_short(ifp, iffid, &chunksize);        /* cols */
    bmhd->h = get_big_short(ifp, iffid, &chunksize);        /* rows */
    bmhd->x = get_big_short(ifp, iffid, &chunksize);        /* ignored */
    bmhd->y = get_big_short(ifp, iffid, &chunksize);        /* ignored */
    bmhd->nPlanes = get_byte(ifp, iffid, &chunksize);
    bmhd->masking = get_byte(ifp, iffid, &chunksize);
    bmhd->compression = get_byte(ifp, iffid, &chunksize);
    bmhd->flags = get_byte(ifp, iffid, &chunksize);
    bmhd->transparentColor = get_big_short(ifp, iffid, &chunksize);
    bmhd->xAspect = get_byte(ifp, iffid, &chunksize);
    bmhd->yAspect = get_byte(ifp, iffid, &chunksize);
    bmhd->pageWidth  = get_big_short(ifp, iffid, &chunksize);   /* ignored */
    bmhd->pageHeight = get_big_short(ifp, iffid, &chunksize);   /* ignored */


    if( verbose ) {
        if( typeid == ID_ILBM )
            pm_message("dimensions: %dx%d, %d planes", bmhd->w, bmhd->h, bmhd->nPlanes);
        else
            pm_message("dimensions: %dx%d", bmhd->w, bmhd->h);

        if( typeid == ID_ILBM || typeid == ID_PBM ) {
            pm_message("compression: %s", bmhd->compression <= cmpMAXKNOWN ?
                        cmpNAME[bmhd->compression] : "unknown");

            switch( bmhd->masking ) {
                case mskNone:
                    break;
                case mskHasMask:
                case mskHasTransparentColor:
                    if( !maskfile )
                        pm_message("use \"-maskfile <filename>\" to generate a PBM mask file from %s", mskNAME[bmhd->masking]);
                    break;
                case mskLasso:
                    pm_message("warning - masking type \"%s\" not supported", mskNAME[bmhd->masking]);
                    break;
                default:
                    pm_error("unknown masking type %d", bmhd->masking);
            }
        }
        else    /* RGBN/RGB8 */
        if( !maskfile )
            pm_message("use \"-maskfile <filename>\" to generate a PBM mask file from genlock bits");
    }

    /* fix aspect ratio */
    if( bmhd->xAspect == 0 || bmhd->yAspect == 0 ) {
        pm_message("warning - illegal aspect ratio %d:%d, using 1:1", bmhd->xAspect, bmhd->yAspect);
        bmhd->xAspect = bmhd->yAspect = 1;
    }

    if( bmhd->xAspect != bmhd->yAspect ) {
        pm_message("warning - non-square pixels; to fix do a 'pnmscale -%cscale %g'",
            bmhd->xAspect > bmhd->yAspect ? 'x' : 'y',
            bmhd->xAspect > bmhd->yAspect ? (float)(bmhd->xAspect)/bmhd->yAspect : (float)(bmhd->yAspect)/bmhd->xAspect);
    }

    return bmhd;
}


/****************************************************************************
 Conversion functions
 ****************************************************************************/

static void
ham_to_ppm(ifp, chunksize, bmhd, cmap, viewportmodes)
    FILE *ifp;
    long chunksize;
    BitMapHeader *bmhd;
    ColorMap *cmap;
    long viewportmodes;
{
    int cols, rows, hambits, hammask, hamshift, hammask2, col, row;
    pixval maxval;
    rawtype *rawrow;
    unsigned char hamlut[256];

    cols = bmhd->w;
    rows = bmhd->h;
    hambits = bmhd->nPlanes - 2;
    hammask = (1 << hambits) - 1;
    hamshift = 8 - hambits;
    hammask2 = (1 << hamshift) - 1;

    if( hambits > 8 || hambits < 1 ) {
        pm_message("%d-plane HAM?? - interpreting image as a normal ILBM", bmhd->nPlanes);
        viewportmodes &= ~(vmHAM);
        std_to_ppm(ifp, chunksize, bmhd, cmap, viewportmodes);
        return;
    }

    pm_message("input is a %sHAM%d file", HAS_MULTIPALETTE(cmap) ? "multipalette " : "", bmhd->nPlanes);

    if( HAS_COLORLUT(cmap) || HAS_MONOLUT(cmap) ) {
        pm_message("warning - color lookup tables ignored in HAM");
        if( cmap->redlut )      free(cmap->redlut);
        if( cmap->greenlut )    free(cmap->greenlut);
        if( cmap->bluelut )     free(cmap->bluelut);
        if( cmap->monolut )     free(cmap->monolut);
        cmap->redlut = cmap->greenlut = cmap->bluelut = cmap->monolut = NULL;
    }
    if( !HAS_COLORMAP(cmap) ) {
        pm_message("no colormap - interpreting values as grayscale");
        maxval = pm_bitstomaxval(hambits);
        for( col = 0; col <= maxval; col++ )
            hamlut[col] = col * MAXCOLVAL / maxval;
        cmap->monolut = hamlut;
    }

    if( transpName ) {
        transpColor = MALLOC(1, pixel);
        *transpColor = ppm_parsecolor(transpName, MAXCOLVAL);
    }

    if( HAS_MULTIPALETTE(cmap) )
        multi_init(cmap, viewportmodes);

    rawrow = alloc_rawrow(cols);

    ppm_writeppminit(stdout, cols, rows, MAXCOLVAL, 0);
    for( row = 0; row < rows; row++ ) {
        pixval r, g, b;

        if( HAS_MULTIPALETTE(cmap) )
            multi_update(cmap, row);

        decode_row(ifp, &chunksize, rawrow, bmhd->nPlanes, bmhd);
        decode_mask(ifp, &chunksize, rawrow, bmhd);

        r = g = b = 0;
        for( col = 0; col < cols; col++ ) {
            int idx = rawrow[col] & hammask;

            if( transpColor && maskrow && maskrow[col] == PBM_WHITE )
                pixelrow[col] = *transpColor;
            else {
                switch((rawrow[col] >> hambits) & 0x03) {
                    case HAMCODE_CMAP:
                        get_color(cmap, idx, &r, &g, &b);
                        break;
                    case HAMCODE_BLUE:
                        b = ((b & hammask2) | (idx << hamshift));
                        /*b = hamlut[idx];*/
                        break;
                    case HAMCODE_RED:
                        r = ((r & hammask2) | (idx << hamshift));
                        /*r = hamlut[idx];*/
                        break;
                    case HAMCODE_GREEN:
                        g = ((g & hammask2) | (idx << hamshift));
                        /*g = hamlut[idx];*/
                        break;
                    default:
                        pm_error("ham_to_ppm(): impossible HAM code - can\'t happen");
                }
                PPM_ASSIGN(pixelrow[col], r, g, b);
            }
        }
        ppm_writeppmrow(stdout, pixelrow, cols, MAXCOLVAL, 0);
    }
    chunk_end(ifp, ID_BODY, chunksize);
}


static void
deep_to_ppm(ifp, chunksize, bmhd, cmap)
    FILE *ifp;
    long chunksize;
    BitMapHeader *bmhd;
    ColorMap *cmap;     /* for CLUTs */
{
    int cols, rows, col, row;
    rawtype *Rrow, *Grow, *Brow;
    int planespercolor = bmhd->nPlanes / 3;
    pixval maxval;

    cols = bmhd->w;
    rows = bmhd->h;

    pm_message("input is a deep (%d-bit) ILBM", bmhd->nPlanes);
    if( planespercolor > MAXPLANES )
        pm_error("too many planes (max %d)", MAXPLANES * 3);

    if( bmhd->masking == mskHasTransparentColor || bmhd->masking == mskLasso ) {
        pm_message("masking type \"%s\" in a deep ILBM?? - ignoring it", mskNAME[bmhd->masking]);
        bmhd->masking = mskNone;
    }

    maxval = lut_maxval(cmap, pm_bitstomaxval(planespercolor));
    if( maxval > PPM_OVERALLMAXVAL )
        pm_error("nPlanes is too large");

    if( transpName ) {
        transpColor = MALLOC(1, pixel);
        *transpColor = ppm_parsecolor(transpName, maxval);
    }

    Rrow = alloc_rawrow(cols);
    Grow = alloc_rawrow(cols);
    Brow = alloc_rawrow(cols);

    ppm_writeppminit(stdout, cols, rows, maxval, 0);
    for( row = 0; row < rows; row++ ) {
        decode_row(ifp, &chunksize, Rrow, planespercolor, bmhd);
        decode_row(ifp, &chunksize, Grow, planespercolor, bmhd);
        decode_row(ifp, &chunksize, Brow, planespercolor, bmhd);
        decode_mask(ifp, &chunksize, NULL, bmhd);

        for( col = 0; col < cols; col++ ) {
            if( transpColor && maskrow && maskrow[col] == PBM_WHITE )
                pixelrow[col] = *transpColor;
            else
                PPM_ASSIGN(pixelrow[col],   lookup_red(cmap, Rrow[col]),
                                            lookup_green(cmap, Grow[col]),
                                            lookup_blue(cmap, Brow[col]) );
        }
        ppm_writeppmrow(stdout, pixelrow, cols, maxval, 0);
    }
    chunk_end(ifp, ID_BODY, chunksize);
}


static void
dcol_to_ppm(ifp, chunksize, bmhd, cmap, dcol)
    FILE *ifp;
    long chunksize;
    BitMapHeader *bmhd;
    ColorMap *cmap;     /* for CLUTs */
    DirectColor *dcol;
{
    int cols, rows, col, row, redplanes, greenplanes, blueplanes, i;
    rawtype *Rrow, *Grow, *Brow;
    pixval maxval, redmaxval, greenmaxval, bluemaxval;
    pixval *redtable, *greentable, *bluetable;

    cols = bmhd->w;
    rows = bmhd->h;

    redplanes = dcol->r; greenplanes = dcol->g; blueplanes = dcol->b;

    pm_message("input is a %d:%d:%d direct color ILBM",
                redplanes, greenplanes, blueplanes);

    if( redplanes > MAXPLANES || blueplanes > MAXPLANES || greenplanes > MAXPLANES )
        pm_error("too many planes (max %d per color component)", MAXPLANES);

    if( bmhd->nPlanes != (redplanes + greenplanes + blueplanes) )
        pm_error("%s/%s plane number mismatch", ID2string(ID_BMHD), ID2string(ID_DCOL));

    if( bmhd->masking == mskHasTransparentColor || bmhd->masking == mskLasso ) {
        pm_message("masking type \"%s\" in a direct color ILBM?? - ignoring it", mskNAME[bmhd->masking]);
        bmhd->masking = mskNone;
    }

    if( HAS_COLORLUT(cmap) ) {      /* TODO */
        pm_error("%s chunk not supported in direct color", ID2string(ID_CLUT));
        cmap->redlut = cmap->greenlut = cmap->bluelut = NULL;
    }

    redmaxval   = pm_bitstomaxval(redplanes);
    greenmaxval = pm_bitstomaxval(greenplanes);
    bluemaxval  = pm_bitstomaxval(blueplanes);
    maxval = max(redmaxval, max(greenmaxval, bluemaxval));

    if( maxval > PPM_OVERALLMAXVAL )
        pm_error("too many planes");

    if( redmaxval != maxval || greenmaxval != maxval || bluemaxval != maxval )
        pm_message("scaling colors to %d bits", pm_maxvaltobits(maxval));

    overflow_add(redmaxval, 1);
    overflow_add(greenmaxval, 1);
    overflow_add(bluemaxval, 1);
    redtable   = MALLOC(redmaxval  +1, pixval);
    greentable = MALLOC(greenmaxval+1, pixval);
    bluetable  = MALLOC(bluemaxval +1, pixval);

    for( i = 0; i <= redmaxval; i++ )
        redtable[i] = (i * maxval + redmaxval/2)/redmaxval;
    for( i = 0; i <= greenmaxval; i++ )
        greentable[i] = (i * maxval + greenmaxval/2)/greenmaxval;
    for( i = 0; i <= bluemaxval; i++ )
        bluetable[i] = (i * maxval + bluemaxval/2)/bluemaxval;

    if( transpName ) {
        transpColor = MALLOC(1, pixel);
        *transpColor = ppm_parsecolor(transpName, maxval);
    }

    Rrow = alloc_rawrow(cols);
    Grow = alloc_rawrow(cols);
    Brow = alloc_rawrow(cols);

    ppm_writeppminit(stdout, cols, rows, maxval, 0);
    for( row = 0; row < rows; row++ ) {
        decode_row(ifp, &chunksize, Rrow, redplanes, bmhd);
        decode_row(ifp, &chunksize, Grow, greenplanes, bmhd);
        decode_row(ifp, &chunksize, Brow, blueplanes, bmhd);
        decode_mask(ifp, &chunksize, NULL, bmhd);

        for( col = 0; col < cols; col++ ) {
            if( transpColor && maskrow && maskrow[col] == PBM_WHITE )
                pixelrow[col] = *transpColor;
            else
                PPM_ASSIGN( pixelrow[col],  redtable[Rrow[col]],
                                            greentable[Grow[col]],
                                            bluetable[Brow[col]]   );
        }
        ppm_writeppmrow(stdout, pixelrow, cols, maxval, 0);
    }
    chunk_end(ifp, ID_BODY, chunksize);
}


static void
cmap_to_ppm(cmap)
    ColorMap *cmap;
{
    ppm_colorrowtomapfile(stdout, cmap->color, cmap->ncolors, MAXCOLVAL);
#if 0
    int i;
    ppm_writeppminit(stdout, cmap->ncolors, 1, MAXCOLVAL, 1);
    for( i = 0; i < cmap->ncolors; i++ )
        ppm_writeppmrow(stdout, &(cmap->color[i]), 1, MAXCOLVAL, 1);
#endif
}


static void
std_to_ppm(ifp, chunksize, bmhd, cmap, viewportmodes)
    FILE *ifp;
    long chunksize;
    BitMapHeader *bmhd;
    ColorMap *cmap;
    long viewportmodes;
{
    rawtype *rawrow;
    int row, rows, col, cols;
    pixval maxval;

    if( viewportmodes & vmHAM ) {
        ham_to_ppm(ifp, chunksize, bmhd, cmap, viewportmodes);
        return;
    }

    cols = bmhd->w;
    rows = bmhd->h;

    pm_message("input is a %d-plane %s%sILBM", bmhd->nPlanes,
                HAS_MULTIPALETTE(cmap) ? "multipalette " : "",
                viewportmodes & vmEXTRA_HALFBRITE ? "EHB " : ""
              );

    if( bmhd->nPlanes > MAXPLANES )
        pm_error("too many planes (max %d)", MAXPLANES);

    if( HAS_COLORMAP(cmap) ) {
        if( viewportmodes & vmEXTRA_HALFBRITE )
            cmap = ehbcmap(cmap);
        maxval = MAXCOLVAL;
    }
    else {
        pm_message("no colormap - interpreting values as grayscale");
        maxval = lut_maxval(cmap, pm_bitstomaxval(bmhd->nPlanes));
        if( maxval > PPM_OVERALLMAXVAL )
            pm_error("nPlanes is too large");
    }

    if( transpName ) {
        transpColor = MALLOC(1, pixel);
        *transpColor = ppm_parsecolor(transpName, maxval);
    }

    rawrow = alloc_rawrow(cols);

    if( HAS_MULTIPALETTE(cmap) )
        multi_init(cmap, viewportmodes);

    ppm_writeppminit( stdout, cols, rows, maxval, 0 );
    for( row = 0; row < rows; row++ ) {

        if( HAS_MULTIPALETTE(cmap) )
            multi_update(cmap, row);

        decode_row(ifp, &chunksize, rawrow, bmhd->nPlanes, bmhd);
        decode_mask(ifp, &chunksize, rawrow, bmhd);

        for( col = 0; col < cols; col++ ) {
            pixval r, g, b;
            if( transpColor && maskrow && maskrow[col] == PBM_WHITE )
                pixelrow[col] = *transpColor;
            else {
                get_color(cmap, rawrow[col], &r, &g, &b);
                PPM_ASSIGN(pixelrow[col], r, g, b);
            }
        }
        ppm_writeppmrow(stdout, pixelrow, cols, maxval, 0);
    }
    chunk_end(ifp, ID_BODY, chunksize);
}


static void
ipbm_to_ppm(ifp, chunksize, bmhd, cmap, viewportmodes)
    FILE *ifp;
    long chunksize;
    BitMapHeader *bmhd;
    ColorMap *cmap;
    long viewportmodes;
{
    int col, cols, row, rows;
    pixval maxval;

    cols = bmhd->w;
    rows = bmhd->h;

    pm_message("input is a %sPBM ", HAS_MULTIPALETTE(cmap) ? "multipalette " : "");

    if( bmhd->nPlanes != 8 )
        pm_error("invalid number of planes for IFF-PBM: %d (must be 8)", bmhd->nPlanes);

    if( bmhd->masking == mskHasMask )
        pm_error("maskplane not supported in IFF-PBM");

    if( HAS_COLORMAP(cmap) )
        maxval = MAXCOLVAL;
    else {
        pm_message("no colormap - interpreting values as grayscale");
        maxval = lut_maxval(cmap, pm_bitstomaxval(bmhd->nPlanes));
    }

    if( transpName ) {
        transpColor = MALLOC(1, pixel);
        *transpColor = ppm_parsecolor(transpName, maxval);
    }

    if( HAS_MULTIPALETTE(cmap) )
        multi_init(cmap, viewportmodes);

    ilbmrow = MALLOC(cols, unsigned char);

    ppm_writeppminit(stdout, cols, rows, maxval, 0);
    for( row = 0; row < rows; row++ ) {
        if( HAS_MULTIPALETTE(cmap) )
            multi_update(cmap, row);

        read_ilbm_plane(ifp, &chunksize, cols, bmhd->compression);

        for( col = 0; col < cols; col++ ) {
            pixval r, g, b;
            if( transpColor && maskrow && maskrow[col] == PBM_WHITE )
                pixelrow[col] = *transpColor;
            else {
                get_color(cmap, ilbmrow[col], &r, &g, &b);
                PPM_ASSIGN(pixelrow[col], r, g, b);
            }
        }
        ppm_writeppmrow(stdout, pixelrow, cols, maxval, 0);
    }
    chunk_end(ifp, ID_BODY, chunksize);
}


static void
rgbn_to_ppm(ifp, chunksize, bmhd, cmap)
    FILE *ifp;
    long chunksize;
    BitMapHeader *bmhd;
    ColorMap *cmap;     /* for CLUTs */
{
    int row, rows, col, cols, count, genlock, tries;
    pixval r, g, b, maxval;

    pm_message("input is a %d-bit RGB image", (typeid == ID_RGB8 ? 8 : 4));

    rows = bmhd->h;
    cols = bmhd->w;
    if( bmhd->compression != 4 )
        pm_error("invalid compression mode for %s: %d (must be 4)", ID2string(typeid), bmhd->compression);

    switch( typeid ) {
        case ID_RGBN:
            if( bmhd->nPlanes != 13 )
                pm_error("invalid number of planes for %s: %d (must be 13)", ID2string(typeid), bmhd->nPlanes);
            maxval = lut_maxval(cmap, 15);
            break;
        case ID_RGB8:
            if( bmhd->nPlanes != 25 )
                pm_error("invalid number of planes for %s: %d (must be 25)", ID2string(typeid), bmhd->nPlanes);
            maxval = 255;
            break;
        default:
            pm_error("rgbn_to_ppm(): invalid IFF ID %s - can\'t happen", ID2string(typeid));
    }

    if( transpName ) {
        transpColor = MALLOC(1, pixel);
        *transpColor = ppm_parsecolor(transpName, maxval);
    }

    ppm_writeppminit(stdout, cols, rows, maxval, 0);
    count = 0;
    for( row = 0; row < rows; row++ ) {
        for( col = 0; col < cols; col++ ) {
            tries = 0;
            while( !count ) {
                if( typeid == ID_RGB8 ) {
                    r = lookup_red(cmap,   get_byte(ifp, ID_BODY, &chunksize));
                    g = lookup_green(cmap, get_byte(ifp, ID_BODY, &chunksize));
                    b = lookup_blue(cmap,  get_byte(ifp, ID_BODY, &chunksize));
                    count = get_byte(ifp, ID_BODY, &chunksize);
                    genlock = count & 0x80;
                    count &= 0x7f;
                }
                else {
                    int word;
                    word = get_big_short(ifp, ID_BODY, &chunksize);
                    r = lookup_red(cmap, (word & 0xf000) >> 12);
                    g = lookup_green(cmap, (word & 0x0f00) >> 8);
                    b = lookup_blue(cmap, (word & 0x00f0) >> 4);
                    genlock = word & 0x0008;
                    count = word & 0x0007;
                }
                if( !count ) {
                    count = get_byte(ifp, ID_BODY, &chunksize);
                    if( !count )
                        count = get_big_short(ifp, ID_BODY, &chunksize);
                        if( !count )
                            ++tries;
                }
            }
            if( tries ) {
                pm_message("warning - repeat count 0 at col %d row %d: skipped %d RGB entr%s",
                            col, row, tries, (tries == 1 ? "y" : "ies"));
            }
            if( maskfile ) {
                /* genlock bit set -> transparent */
                if( genlock )
                    maskrow[col] = PBM_WHITE;
                else
                    maskrow[col] = PBM_BLACK;
            }
            if( transpColor && maskrow && maskrow[col] == PBM_WHITE )
                pixelrow[col] = *transpColor;
            else
                PPM_ASSIGN(pixelrow[col], r, g, b);
            --count;
        }
        ppm_writeppmrow(stdout, pixelrow, cols, maxval, 0);
        if( maskfile ) {
            pbm_writepbmrow(maskfile, maskrow, cols, 0);
            wrotemask = 1;
        }
    }
    chunk_end(ifp, ID_BODY, chunksize);
}

/****************************************************************************
 ILBM functions
 ****************************************************************************/


static void
read_ilbm_plane(ifp, chunksizeP, bytes, compression)
    FILE *ifp;
    long *chunksizeP;
    int bytes, compression;
{
    unsigned char *ubp;
    int j, byte;

    switch(compression) {
        case cmpNone:
            read_bytes(ifp, bytes, ilbmrow, ID_BODY, chunksizeP);
            break;
        case cmpByteRun1:
            ubp = ilbmrow;
            do {
                byte = (int)get_byte(ifp, ID_BODY, chunksizeP);
                if( byte <= 127 ) {
                    j = byte;
                    bytes -= (j+1);
                    if( bytes < 0 )
                        pm_error("error doing ByteRun1 decompression");
                    for( ; j >= 0; j-- )
                        *ubp++ = get_byte(ifp, ID_BODY, chunksizeP);
                }
                else
                if ( byte != 128 ) {
                    j = 256 - byte;
                    bytes -= (j+1);
                    if( bytes < 0 )
                        pm_error("error doing ByteRun1 decompression");
                    byte = (int)get_byte(ifp, ID_BODY, chunksizeP);
                    for( ; j >= 0; j-- )
                        *ubp++ = (unsigned char)byte;
                }
                /* 128 is a NOP */
            }
            while( bytes > 0 );
            break;
        default:
            pm_error("unknown compression type %d", compression);
    }
}


static const unsigned char bit_mask[] = {1, 2, 4, 8, 16, 32, 64, 128};

static void
decode_row(ifp, chunksizeP, chunkyrow, nPlanes, bmhd)
    FILE *ifp;
    long *chunksizeP;
    rawtype *chunkyrow;
    int nPlanes;
    BitMapHeader *bmhd;
{
    int plane, col, cols, cbit, bytes;
    unsigned char *ilp;
    rawtype *chp;

    cols = bmhd->w;
    overflow_add(cols, 15);
    bytes = RowBytes(cols);
    for( plane = 0; plane < nPlanes; plane++ ) {
        int mask;

        mask = 1 << plane;
        read_ilbm_plane(ifp, chunksizeP, bytes, bmhd->compression);

        ilp = ilbmrow;
        chp = chunkyrow;

        cbit = 7;
        for( col = 0; col < cols; col++, cbit--, chp++ ) {
            if( cbit < 0 ) {
                cbit = 7;
                ilp++;
            }
            if( *ilp & bit_mask[cbit] )
                *chp |= mask;
            else
                *chp &= ~mask;
        }
    }
}


static void
decode_mask(ifp, chunksizeP, chunkyrow, bmhd)
    FILE *ifp;
    long *chunksizeP;
    rawtype *chunkyrow;
    BitMapHeader *bmhd;
{
    int col, cols, cbit;
    unsigned char *ilp;

    cols = bmhd->w;
    switch( bmhd->masking ) {
        case mskNone:
            break;
        case mskHasMask:        /* mask plane */
            overflow_add(cols, 15);
            read_ilbm_plane(ifp, chunksizeP, RowBytes(cols), bmhd->compression);
            if( maskfile ) {
                ilp = ilbmrow;
                cbit = 7;
                for( col = 0; col < cols; col++, cbit-- ) {
                    if( cbit < 0 ) {
                        cbit = 7;
                        ilp++;
                    }
                    if( *ilp & bit_mask[cbit] )
                        maskrow[col] = PBM_BLACK;
                    else
                        maskrow[col] = PBM_WHITE;
                }
                pbm_writepbmrow(maskfile, maskrow, cols, 0);
                wrotemask = 1;
            }
            break;
        case mskHasTransparentColor:
            if( !chunkyrow )
                pm_error("decode_mask(): chunkyrow == NULL - can\'t happen");

            if( maskfile ) {
                for( col = 0; col < cols; col++ ) {
                    if( chunkyrow[col] == bmhd->transparentColor )
                        maskrow[col] = PBM_WHITE;
                    else
                        maskrow[col] = PBM_BLACK;
                }
                pbm_writepbmrow(maskfile, maskrow, cols, 0);
                wrotemask = 1;
            }
            break;
        case mskLasso:
            /* not supported yet */
            break;
        default:
            pm_error("decode_mask(): unknown masking type %d - can\'t happen", bmhd->masking);
    }
}


static rawtype *
alloc_rawrow(cols)
    int cols;
{
    rawtype *r;
    int i;

    r = MALLOC(cols, rawtype);

    for( i = 0; i < cols; i++ )
        r[i] = 0;

    return r;
}


static void *
xmalloc(bytes)
    int bytes;
{
    void *mem;

    if( bytes == 0 )
        return NULL;

    mem = malloc(bytes);
    if( mem == NULL )
        pm_error("out of memory allocating %d bytes", bytes);
    return mem;
}

static void *
xmalloc2(x, y)
    int x;
    int y;
{
    void *mem;

    overflow2(x,y);
    if( x * y == 0 )
        return NULL;

    mem = malloc2(x,y);
    if( mem == NULL )
        pm_error("out of memory allocating %d bytes", x * y);
    return mem;
}


static char *
ID2string(id)
    IFF_ID id;
{
    static char str[] = "abcd";

    str[0] = (char)(id >> 24 & 0xff);
    str[1] = (char)(id >> 16 & 0xff);
    str[2] = (char)(id >>  8 & 0xff);
    str[3] = (char)(id >>  0 & 0xff);

    return str;
}


/****************************************************************************
 Multipalette chunk reader

    Currently there are three multipalette formats:
        SHAM - sliced HAM (obselete)
        CTBL - dynamic HAM/Hires (obselete)
        PCHG - palette change
    There is no official documentation available for SHAM and CTBL, so
   this is mostly guesswork from other sources and hexdumps of pictures...

    CTBL format (dynamic HAM/Hires):
        16 bigendian words per row (16 bit: 0000rrrrggggbbbb)
    This is simply an entire 4-bit colormap per row.
    I have no idea what the DYCP chunk is for.

    SHAM format (sliced HAM):
        1 word  - version info (?) - always 0
        16 bigendian words per row (16 bit: 0000rrrrggggbbbb)
    If the picture is laced, then there are only rows/2 changes, don't change
    palette on odd lines.

    PCHG format:
    A detailed description of this format is available from AmiNet as PCHGLib12.lha.

 ****************************************************************************/

/* local functions */
static void PCHG_Decompress ARGS((PCHGHeader *PCHG, PCHGCompHeader *CompHdr, unsigned char *compdata, unsigned long compsize, unsigned char *comptree, unsigned char *data));
static void PCHG_DecompHuff ARGS((unsigned char *src, unsigned char *dest, short *tree, unsigned long origsize));
static void PCHG_ConvertSmall ARGS((PCHGHeader *PCHG, ColorMap *cmap, unsigned char *data, unsigned long datasize));
static void PCHG_ConvertBig   ARGS((PCHGHeader *PCHG, ColorMap *cmap, unsigned char *data, unsigned long datasize));
static void multi_adjust ARGS((ColorMap *cmap, int row, PaletteChange *palchange));
/* Turn big-endian 4-byte long and 2-byte short stored at x (unsigned char *)
 * into the native format of the CPU
 */
#define BIG_LONG(x) (   ((unsigned long)((x)[0]) << 24) + \
                        ((unsigned long)((x)[1]) << 16) + \
                        ((unsigned long)((x)[2]) <<  8) + \
                        ((unsigned long)((x)[3]) <<  0) )
#define BIG_WORD(x) (   ((unsigned short)((x)[0]) << 8) + \
                        ((unsigned short)((x)[1]) << 0) )



static ColorMap *
read_4bit_mp(ifp, iffid, chunksize, cmap)
    FILE *ifp;
    IFF_ID iffid;
    long chunksize;
    ColorMap *cmap;
{
    int row, rows, i, type;
    short data;

    if( !cmap )
        cmap = alloc_cmap();

    type = (iffid == ID_SHAM ? MP_TYPE_SHAM : MP_TYPE_CTBL);

    if( cmap->mp_type >= type ) {
        skip_chunk(ifp, iffid, chunksize);
        return cmap;
    }
    if( cmap->mp_type )
        multi_free(cmap);
    cmap->mp_type = type;

    if( type == MP_TYPE_SHAM ) {
        cmap->mp_flags = MP_FLAGS_SKIPLACED;
        (void)get_big_short(ifp, iffid, &chunksize);   /* skip first word */
    }

    cmap->mp_rows = rows = chunksize/32;    /* sizeof(word) * 16 */
    cmap->mp_change = MALLOC(rows, PaletteChange *);

    for( row = 0; row < rows; row++ ) {
        cmap->mp_change[row] = MALLOC(17, PaletteChange);   /* 16 + sentinel */
        for( i = 0; i < 16; i++ ) {
            data = get_big_short(ifp, iffid, &chunksize);
            cmap->mp_change[row][i].reg = i;
            cmap->mp_change[row][i].r = ((data & 0x0f00) >> 8) * FACTOR_4BIT;
            cmap->mp_change[row][i].g = ((data & 0x00f0) >> 4) * FACTOR_4BIT;
            cmap->mp_change[row][i].b = ((data & 0x000f) >> 0) * FACTOR_4BIT;
        }
        cmap->mp_change[row][16].reg = MP_REG_END;   /* sentinel */
    }
    chunk_end(ifp, iffid, chunksize);
    return cmap;
}


static ColorMap *
read_pchg(ifp, iffid, chunksize, cmap)
    FILE *ifp;
    IFF_ID iffid;
    long chunksize;
    ColorMap *cmap;
{
    PCHGHeader      PCHG;
    unsigned char   *data;
    unsigned long   datasize;
    int i;

    if( !cmap )
        cmap = alloc_cmap();

    if( cmap->mp_type >= MP_TYPE_PCHG ) {
        skip_chunk(ifp, iffid, chunksize);
        return cmap;
    }
    if( cmap->mp_type )
        multi_free(cmap);
    cmap->mp_type = MP_TYPE_PCHG;

    PCHG.Compression = get_big_short(ifp, iffid, &chunksize);
    PCHG.Flags       = get_big_short(ifp, iffid, &chunksize);
    PCHG.StartLine   = get_big_short(ifp, iffid, &chunksize);
    PCHG.LineCount   = get_big_short(ifp, iffid, &chunksize);
    PCHG.ChangedLines= get_big_short(ifp, iffid, &chunksize);
    PCHG.MinReg      = get_big_short(ifp, iffid, &chunksize);
    PCHG.MaxReg      = get_big_short(ifp, iffid, &chunksize);
    PCHG.MaxChanges  = get_big_short(ifp, iffid, &chunksize);
    PCHG.TotalChanges= get_big_long(ifp, iffid, &chunksize);

#ifdef DEBUG
    pm_message("PCHG StartLine   : %d", PCHG.StartLine);
    pm_message("PCHG LineCount   : %d", PCHG.LineCount);
    pm_message("PCHG ChangedLines: %d", PCHG.ChangedLines);
    pm_message("PCHG TotalChanges: %d", PCHG.TotalChanges);
#endif

    if( PCHG.Compression != PCHG_COMP_NONE ) {
        PCHGCompHeader  CompHdr;
        unsigned char *compdata, *comptree;
        long treesize, compsize;

        CompHdr.CompInfoSize     = get_big_long(ifp, iffid, &chunksize);
        CompHdr.OriginalDataSize = get_big_long(ifp, iffid, &chunksize);

        treesize = CompHdr.CompInfoSize;
        comptree = MALLOC(treesize, unsigned char);
        read_bytes(ifp, treesize, comptree, iffid, &chunksize);

        compsize = chunksize;
        compdata= MALLOC(chunksize, unsigned char);
        read_bytes(ifp, compsize, compdata, iffid, &chunksize);

        datasize = CompHdr.OriginalDataSize;
        data    = MALLOC(datasize, unsigned char);
        PCHG_Decompress(&PCHG, &CompHdr, compdata, compsize, comptree, data);

        free(comptree);
        free(compdata);
    }
    else {
#ifdef DEBUG
        pm_message("uncompressed PCHG");
#endif
        datasize = chunksize;
        data = MALLOC(datasize, unsigned char);
        read_bytes(ifp, datasize, data, iffid, &chunksize);
    }

    if( PCHG.Flags & PCHGF_USE_ALPHA )
        pm_message("warning - PCHG alpha channel not supported");

    cmap->mp_rows = PCHG.StartLine + PCHG.LineCount;
    cmap->mp_change = MALLOC(cmap->mp_rows, PaletteChange *);
    for( i = 0; i < cmap->mp_rows; i++ )
        cmap->mp_change[i] = NULL;
    if( PCHG.StartLine < 0 ) {
        int nch;
        if(PCHG.MaxReg < PCHG.MinReg)
        	pm_error("assert: MinReg > MaxReg");
        overflow_add(PCHG.MaxReg-PCHG.MinReg, 2);
        nch = PCHG.MaxReg - PCHG.MinReg +1;
        cmap->mp_init = MALLOC(nch + 1, PaletteChange);
        for( i = 0; i < nch; i++ )
            cmap->mp_init[i].reg = MP_REG_IGNORE;
        cmap->mp_init[nch].reg = MP_REG_END;
    }

    if( PCHG.Flags & PCHGF_12BIT ) {
#ifdef DEBUG
        pm_message("SmallLineChanges");
#endif
        PCHG_ConvertSmall(&PCHG, cmap, data, datasize);
    }
    else
    if( PCHG.Flags & PCHGF_32BIT ) {
#ifdef DEBUG
        pm_message("BigLineChanges");
#endif
        PCHG_ConvertBig(&PCHG, cmap, data, datasize);
    }
    else
        pm_error("unknown palette changes structure format in %s chunk", ID2string(iffid));

    free(data);
    chunk_end(ifp, iffid, chunksize);
    return cmap;
}


static void
PCHG_Decompress(PCHG, CompHdr, compdata, compsize, comptree, data)
    PCHGHeader *PCHG;
    PCHGCompHeader *CompHdr;
    unsigned char *compdata;
    unsigned long compsize;
    unsigned char *comptree;
    unsigned char *data;
{
    short *hufftree;
    unsigned long huffsize, i;
    unsigned long treesize = CompHdr->CompInfoSize;

    switch( PCHG->Compression ) {
        case PCHG_COMP_HUFFMAN:

#ifdef DEBUG
            pm_message("PCHG Huffman compression");
#endif
            /* turn big-endian 2-byte shorts into native format */
            huffsize = treesize/2;
            hufftree = MALLOC(huffsize, short);
            for( i = 0; i < huffsize; i++ ) {
                hufftree[i] = (short)BIG_WORD(comptree);
                comptree += 2;
            }

            /* decompress the change structure data */
            PCHG_DecompHuff(compdata, data, &hufftree[huffsize-1], CompHdr->OriginalDataSize);

            free(hufftree);
            break;
        default:
            pm_error("unknown PCHG compression type %d", PCHG->Compression);
    }
}


static void
PCHG_ConvertSmall(PCHG, cmap, mask, datasize)
    PCHGHeader *PCHG;
    ColorMap *cmap;
    unsigned char *mask;
    unsigned long datasize;
{
    unsigned char *data;
    unsigned char thismask;
    int bits, row, i, changes, masklen, reg;
    unsigned char ChangeCount16, ChangeCount32;
    unsigned short SmallChange;
    unsigned long totalchanges = 0;
    int changedlines = PCHG->ChangedLines;

    masklen = 4 * MaskLongWords(PCHG->LineCount);
    data = mask + masklen; datasize -= masklen;

    bits = 0;
    for( row = PCHG->StartLine; changedlines && row < 0; row++ ) {
        if( bits == 0 ) {
            if( masklen == 0 ) goto fail2;
            thismask = *mask++;
            --masklen;
            bits = 8;
        }
        if( thismask & (1<<7) ) {
            if( datasize < 2 ) goto fail;
            ChangeCount16 = *data++;
            ChangeCount32 = *data++;
            datasize -= 2;

            changes = ChangeCount16 + ChangeCount32;
            for( i = 0; i < changes; i++ ) {
                if( totalchanges >= PCHG->TotalChanges ) goto fail;
                if( datasize < 2 ) goto fail;
                SmallChange = BIG_WORD(data); data += 2; datasize -= 2;
                reg = ((SmallChange & 0xf000) >> 12) + (i >= ChangeCount16 ? 16 : 0);
                cmap->mp_init[reg - PCHG->MinReg].reg = reg;
                cmap->mp_init[reg - PCHG->MinReg].r = ((SmallChange & 0x0f00) >> 8) * FACTOR_4BIT;
                cmap->mp_init[reg - PCHG->MinReg].g = ((SmallChange & 0x00f0) >> 4) * FACTOR_4BIT;
                cmap->mp_init[reg - PCHG->MinReg].b = ((SmallChange & 0x000f) >> 0) * FACTOR_4BIT;
                ++totalchanges;
            }
            --changedlines;
        }
        thismask <<= 1;
        bits--;
    }

    for( row = PCHG->StartLine; changedlines && row < cmap->mp_rows; row++ ) {
        if( bits == 0 ) {
            if( masklen == 0 ) goto fail2;
            thismask = *mask++;
            --masklen;
            bits = 8;
        }
        if( thismask & (1<<7) ) {
            if( datasize < 2 ) goto fail;
            ChangeCount16 = *data++;
            ChangeCount32 = *data++;
            datasize -= 2;

	    overflow_add(ChangeCount16, ChangeCount32);
            changes = ChangeCount16 + ChangeCount32;
            overflow_add(changes, 1);
            cmap->mp_change[row] = MALLOC(changes + 1, PaletteChange);
            for( i = 0; i < changes; i++ ) {
                if( totalchanges >= PCHG->TotalChanges ) goto fail;
                if( datasize < 2 ) goto fail;
                SmallChange = BIG_WORD(data); data += 2; datasize -= 2;
                reg = ((SmallChange & 0xf000) >> 12) + (i >= ChangeCount16 ? 16 : 0);
                cmap->mp_change[row][i].reg = reg;
                cmap->mp_change[row][i].r = ((SmallChange & 0x0f00) >> 8) * FACTOR_4BIT;
                cmap->mp_change[row][i].g = ((SmallChange & 0x00f0) >> 4) * FACTOR_4BIT;
                cmap->mp_change[row][i].b = ((SmallChange & 0x000f) >> 0) * FACTOR_4BIT;
                ++totalchanges;
            }
            cmap->mp_change[row][changes].reg = MP_REG_END;
            --changedlines;
        }
        thismask <<= 1;
        bits--;
    }
    if( totalchanges != PCHG->TotalChanges )
        pm_message("warning - got %ld change structures, chunk header reports %ld", totalchanges, PCHG->TotalChanges);
    return;
fail:
    pm_error("insufficient data in SmallLineChanges structures");
fail2:
    pm_error("insufficient data in line mask");
}


static void
PCHG_ConvertBig(PCHG, cmap, mask, datasize)
    PCHGHeader *PCHG;
    ColorMap *cmap;
    unsigned char *mask;
    unsigned long datasize;
{
    unsigned char *data;
    unsigned char thismask;
    int bits, row, i, changes, masklen, reg;
    unsigned long totalchanges = 0;
    int changedlines = PCHG->ChangedLines;

    masklen = 4 * MaskLongWords(PCHG->LineCount);
    data = mask + masklen; datasize -= masklen;

    bits = 0;
    for( row = PCHG->StartLine; changedlines && row < 0; row++ ) {
        if( bits == 0 ) {
            if( masklen == 0 ) goto fail2;
            thismask = *mask++;
            --masklen;
            bits = 8;
        }
        if( thismask & (1<<7) ) {
            if( datasize < 2 ) goto fail;
            changes = BIG_WORD(data); data += 2; datasize -= 2;

            for( i = 0; i < changes; i++ ) {
                if( totalchanges >= PCHG->TotalChanges ) goto fail;
                if( datasize < 6 ) goto fail;
                reg = BIG_WORD(data); data += 2;
                cmap->mp_init[reg - PCHG->MinReg].reg = reg;
                ++data; /* skip alpha */
                cmap->mp_init[reg - PCHG->MinReg].r = *data++;
                cmap->mp_init[reg - PCHG->MinReg].b = *data++;    /* yes, RBG */
                cmap->mp_init[reg - PCHG->MinReg].g = *data++;
                datasize -= 6;
                ++totalchanges;
            }
            --changedlines;
        }
        thismask <<= 1;
        bits--;
    }

    for( row = PCHG->StartLine; changedlines && row < cmap->mp_rows; row++ ) {
        if( bits == 0 ) {
            if( masklen == 0 ) goto fail2;
            thismask = *mask++;
            --masklen;
            bits = 8;
        }
        if( thismask & (1<<7) ) {
            if( datasize < 2 ) goto fail;
            changes = BIG_WORD(data); data += 2; datasize -= 2;

	    overflow_add(changes, 1);
            cmap->mp_change[row] = MALLOC(changes + 1, PaletteChange);
            for( i = 0; i < changes; i++ ) {
                if( totalchanges >= PCHG->TotalChanges ) goto fail;
                if( datasize < 6 ) goto fail;
                reg = BIG_WORD(data); data += 2;
                cmap->mp_change[row][i].reg = reg;
                ++data; /* skip alpha */
                cmap->mp_change[row][i].r = *data++;
                cmap->mp_change[row][i].b = *data++;    /* yes, RBG */
                cmap->mp_change[row][i].g = *data++;
                datasize -= 6;
                ++totalchanges;
            }
            cmap->mp_change[row][changes].reg = MP_REG_END;
            --changedlines;
        }
        thismask <<= 1;
        bits--;
    }
    if( totalchanges != PCHG->TotalChanges )
        pm_message("warning - got %ld change structures, chunk header reports %ld", totalchanges, PCHG->TotalChanges);
    return;
fail:
    pm_error("insufficient data in BigLineChanges structures");
fail2:
    pm_error("insufficient data in line mask");
}


static void
PCHG_DecompHuff(src, dest, tree, origsize)
    unsigned char *src, *dest;
    short *tree;
    unsigned long origsize;
{
    unsigned long i = 0, bits = 0;
    unsigned char thisbyte;
    short *p;

    p = tree;
    while( i < origsize ) {
        if( bits == 0 ) {
            thisbyte = *src++;
            bits = 8;
        }
        if( thisbyte & (1 << 7) ) {
            if( *p >= 0 ) {
                *dest++ = (unsigned char)*p;
                i++;
                p = tree;
            }
            else
                p += (*p / 2);
        }
        else {
            p--;
            if( *p > 0 && (*p & 0x100) ) {
                *dest++ = (unsigned char )*p;
                i++;
                p = tree;
            }
        }
        thisbyte <<= 1;
        bits--;
    }
}


/****************************************************************************
 Multipalette handling
 ****************************************************************************/


static void
multi_adjust(cmap, row, palchange)
    ColorMap *cmap;
    int row;
    PaletteChange *palchange;
{
    int i, reg;

    for( i = 0; palchange[i].reg != MP_REG_END; i++ ) {
        reg = palchange[i].reg;
        if( reg >= cmap->ncolors ) {
            pm_message("warning - palette change register out of range");
            pm_message("    row %d  change structure %d  reg=%d (max %d)", row, i, reg, cmap->ncolors-1);
            pm_message("    ignoring it... colors might get messed up from here");
        }
        else
        if( reg != MP_REG_IGNORE ) {
            PPM_ASSIGN(cmap->color[reg], palchange[i].r, palchange[i].g, palchange[i].b);
        }
    }
}

static void
multi_init(cmap, viewportmodes)
    ColorMap *cmap;
    long viewportmodes;
{
    if( cmap->mp_init )
        multi_adjust(cmap, -1, cmap->mp_init);
    if( !(viewportmodes & vmLACE) )
        cmap->mp_flags &= ~(MP_FLAGS_SKIPLACED);
}

static void
multi_update(cmap, row)
    ColorMap *cmap;
    int row;
{
    if( cmap->mp_flags & MP_FLAGS_SKIPLACED ) {
        if( odd(row) )
            return;
        if( row/2 < cmap->mp_rows && cmap->mp_change[row/2] )
            multi_adjust(cmap, row, cmap->mp_change[row/2]);
    }
    else {
        if( row < cmap->mp_rows && cmap->mp_change[row] )
            multi_adjust(cmap, row, cmap->mp_change[row]);
    }
}

static void
multi_free(cmap)
    ColorMap *cmap;
{
    int i;

    if( cmap->mp_init ) {
        free(cmap->mp_init);
        cmap->mp_init = NULL;
    }
    if( cmap->mp_change ) {
        for( i = 0; i < cmap->mp_rows; i++ ) {
            if( cmap->mp_change[i] )
                free(cmap->mp_change[i]);
        }
        free(cmap->mp_change);
        cmap->mp_change = NULL;
    }
    cmap->mp_rows = 0;
    cmap->mp_type = 0;
    cmap->mp_flags = 0;
}


/****************************************************************************
 Colormap handling
 ****************************************************************************/

static ColorMap *
alloc_cmap ARGS((void))
{
    ColorMap *cmap;

    cmap = MALLOC(1, ColorMap);
    cmap->color = NULL;
    cmap->ncolors = 0;
    cmap->monolut = cmap->redlut = cmap->greenlut = cmap->bluelut = NULL;
    cmap->mp_init = NULL;
    cmap->mp_change = NULL;
    cmap->mp_rows = cmap->mp_type = cmap->mp_flags = 0;
    return cmap;
}


static void
check_cmap(bmhd, cmap)
    BitMapHeader *bmhd;
    ColorMap *cmap;
{
    pixval colmaxval = 0;
    int shifted = 1;
    int i, r, g, b;

    if( bmhd ) {
        if( bmhd->masking == mskHasTransparentColor || bmhd->masking == mskLasso ) {
            transpIndex = bmhd->transparentColor;
            if( !transpName ) {
                transpColor = MALLOC(1, pixel);
                if( transpIndex >= cmap->ncolors ) {
                    pm_message("using default transparent color (black)");
                    PPM_ASSIGN(*transpColor, 0, 0, 0);
                }
                else
                    *transpColor = cmap->color[transpIndex];
            }
        }

        if( bmhd->flags & BMHD_FLAGS_CMAPOK )
            return;
    }

    if( !HAS_COLORMAP(cmap) )
        return;

    for( i = 0; i < cmap->ncolors; i++ ) {
        r = PPM_GETR(cmap->color[i]);
        if( r > colmaxval ) colmaxval = r;
        if( r & 0x0f ) shifted = 0;

        g = PPM_GETG(cmap->color[i]);
        if( g > colmaxval ) colmaxval = g;
        if( g & 0x0f ) shifted = 0;

        b = PPM_GETB(cmap->color[i]);
        if( b > colmaxval ) colmaxval = b;
        if( b & 0x0f ) shifted = 0;
    }

#ifdef DEBUG
    pm_message("colormap maxval is %d", colmaxval);
#endif
    if( colmaxval == 0 )
        pm_message("warning - black colormap");
    else
    if( shifted || colmaxval <= 15 ) {
        if( !adjustcolors ) {
            pm_message("warning - probably %s4-bit colormap",
                        shifted ? "shifted " : "");
            pm_message("    use \"-adjustcolors\" to scale colormap to 8 bits");
        }
        else {
            pm_message("scaling colormap to 8 bits");
            for( i = 0; i < cmap->ncolors; i++ ) {
                r = PPM_GETR(cmap->color[i]);
                g = PPM_GETG(cmap->color[i]);
                b = PPM_GETB(cmap->color[i]);
                if( shifted ) {
                    r >>= 4;
                    g >>= 4;
                    b >>= 4;
                }
                r *= FACTOR_4BIT;
                g *= FACTOR_4BIT;
                b *= FACTOR_4BIT;
                PPM_ASSIGN(cmap->color[i], r, g, b);
            }
        }
    }
}


static void
get_color(cmap, idx, red, green, blue)
    ColorMap *cmap;
    int idx;
    pixval *red, *green, *blue;
{
    if( HAS_COLORMAP(cmap) ) {
        pixval r, g, b;

        if( idx >= cmap->ncolors )
            pm_error("color index out of range: %d (max %d)", idx, cmap->ncolors);
        r = PPM_GETR(cmap->color[idx]);
        g = PPM_GETG(cmap->color[idx]);
        b = PPM_GETB(cmap->color[idx]);

        *red    = lookup_red(cmap, r);
        *green  = lookup_green(cmap, g);
        *blue   = lookup_blue(cmap, b);
    }
    else {
        *red = *green = *blue = lookup_mono(cmap, idx);
    }
}


static pixval
lookup_red(cmap, oldval)
    ColorMap *cmap;
    int oldval;
{
    if( cmap && cmap->redlut && oldval < 256 )
        return cmap->redlut[oldval];
    else
        return oldval;
}

static pixval
lookup_green(cmap, oldval)
    ColorMap *cmap;
    int oldval;
{
    if( cmap && cmap->greenlut && oldval < 256 )
        return cmap->greenlut[oldval];
    else
        return oldval;
}

static pixval
lookup_blue(cmap, oldval)
    ColorMap *cmap;
    int oldval;
{
    if( cmap && cmap->bluelut && oldval < 256 )
        return cmap->bluelut[oldval];
    else
        return oldval;
}

static pixval
lookup_mono(cmap, oldval)
    ColorMap *cmap;
    int oldval;
{
    if( cmap && cmap->monolut && oldval < 256 )
        return cmap->monolut[oldval];
    else
        return oldval;
}

static ColorMap *
ehbcmap(cmap)
    ColorMap *cmap;
{
    pixel *tempcolor = NULL;
    int i, col;

    col = cmap->ncolors;

    tempcolor = ppm_allocrow(col * 2);
    for( i = 0; i < col; i++ ) {
        tempcolor[i] = cmap->color[i];
        PPM_ASSIGN(tempcolor[col + i],  PPM_GETR(cmap->color[i]) / 2,
                                        PPM_GETG(cmap->color[i]) / 2,
                                        PPM_GETB(cmap->color[i]) / 2 );
    }
    ppm_freerow(cmap->color);
    cmap->color = tempcolor;
    cmap->ncolors *= 2;

    return cmap;
}


static pixval
#if __STDC__
lut_maxval(ColorMap *cmap, pixval maxval)
#else
lut_maxval(cmap, maxval)
    ColorMap *cmap;
    pixval maxval;
#endif
{
    int i;

    if( maxval < 255 ) {
        if( HAS_COLORLUT(cmap) ) {
            pixval oldmaxval = maxval;
            for( i = 0; i < oldmaxval; i++ ) {
                if( cmap->redlut   && cmap->redlut[i]   > maxval ) maxval = cmap->redlut[i];
                if( cmap->greenlut && cmap->greenlut[i] > maxval ) maxval = cmap->greenlut[i];
                if( cmap->bluelut  && cmap->bluelut[i]  > maxval ) maxval = cmap->bluelut[i];
            }
            pm_message("warning - %d-bit index into 8-bit color lookup table, table maxval=%d", pm_maxvaltobits(oldmaxval), maxval);
            if( maxval != oldmaxval )
                maxval = 255;
            pm_message("    assuming image maxval=%d", maxval);
        }
    }
    return maxval;
}


/****************************************************************************
 Basic I/O functions
 ****************************************************************************/
static void readerr ARGS((FILE *f, IFF_ID iffid));


static void
readerr(f, iffid)
    FILE *f;
    IFF_ID iffid;
{
    if( ferror(f) )
        pm_error("read error");
    else
        pm_error("premature EOF in %s chunk", ID2string(iffid));
}


static void
read_bytes(ifp, bytes, buffer, iffid, counter)
    FILE *ifp;
    int bytes;
    unsigned char *buffer;
    IFF_ID iffid;
    long *counter;
{
    if( counter ) {
        if( *counter < bytes )
            pm_error("insufficient data in %s chunk", ID2string(iffid));
        *counter -= bytes;
    }
    if( fread(buffer, 1, bytes, ifp) != bytes )
        readerr(ifp, iffid);
}


static unsigned char
get_byte(ifp, iffid, counter)
    FILE* ifp;
    IFF_ID iffid;
    long *counter;
{
    int i;

    if( counter ) {
        if( *counter == 0 )
            pm_error("insufficient data in %s chunk", ID2string(iffid));
        --(*counter);
    }
    i = getc(ifp);
    if( i == EOF )
        readerr(ifp, iffid);

    return (unsigned char) i;
}

static long
get_big_long(ifp, iffid, counter)
    FILE *ifp;
    IFF_ID iffid;
    long *counter;
{
    long l;

    if( counter ) {
        if( *counter < 4 )
            pm_error("insufficient data in %s chunk", ID2string(iffid));
        *counter -= 4;
    }
    if( pm_readbiglong(ifp, &l) == -1 )
        readerr(ifp, iffid);

    return l;
}

static short
get_big_short(ifp, iffid, counter)
    FILE *ifp;
    IFF_ID iffid;
    long *counter;
{
    short s;

    if( counter ) {
        if( *counter < 2 )
            pm_error("insufficient data in %s chunk", ID2string(iffid));
        *counter -= 2;
    }
    if( pm_readbigshort(ifp, &s) == -1 )
        readerr(ifp, iffid);

    return s;
}

