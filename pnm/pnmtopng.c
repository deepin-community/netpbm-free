/*
** pnmtopng.c -
** read a portable anymap and produce a Portable Network Graphics file
**
** derived from pnmtorast.c (c) 1990,1991 by Jef Poskanzer and some
** parts derived from ppmtogif.c by Marcel Wijkstra <wijkstra@fwi.uva.nl>
** thanks to Greg Roelofs <newt@pobox.com> for contributions and bug-fixes
**
** Copyright (C) 1995-1998 by Alexander Lehmann <alex@hal.rhein-main.de>
**                        and Willem van Schaik <willem@schaik.com>
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

/* A performance note: This program reads one row at a time because
   the whole image won't fit in memory always.  When you realize that
   in a Netpbm xel array a one bit pixel can take 96 bits of memory,
   it's easy to see that an ordinary fax could deplete your virtual
   memory and even if it didn't, it might deplete your real memory and
   iterating through the array would cause thrashing.  This program
   iterates through the image multiple times.  

   So instead, we read the image into memory one row at a time, into a
   single row buffer.  We use Netpbm's pm_openr_seekable() facility to
   access the file.  That facility copies the file into a temporary
   file if it isn't seekable, so we always end up with a file that we
   can rewind and reread multiple times.

   This shouldn't cause I/O delays because the entire image ought to fit
   in the system's I/O cache (remember that the file is a lot smaller than
   the xel array you'd get by doing a pnm_pnmread() of it).

   However, it does introduce some delay because of all the system calls 
   required to read the file.  A future enhancement might read the entire
   file into an xel array in some cases, and read one row at a time in 
   others, depending on the needs of the particular use.

   We do still read the entire alpha mask (if there is one) into a
   'gray' array, rather than access it one row at a time.  

   Before May 2001, we did in fact read the whole image into an xel array,
   and we got complaints.  Before April 2000, it wasn't as big a problem
   because xels were only 8 bits.  Now they're 32.
*/
   
/* GRR 20000930:  fixed bug reported by Steven Grady <grady@xcf.berkeley.edu>:
 *  if -transparent option given but exact color does not exist (e.g., when
 *  doing batch conversion of a web site), pnmtopng would pick an approximate
 *  color instead of ignoring the transparency.  Also added 2 (== warning)
 *  return code for such cases.  (1 already used by pm_error().) */

/* GRR 20000315:  ifdef'd out a never-reached (for non-PGM_BIGGRAYS) case
 *  that causes a gcc warning. */

/* GRR 19991203:  incorporated fix by Rafal Rzeczkowski <rzeczkrg@mcmaster.ca>:
 *  gray images with exactly 16 shades were being to 8-bit grayscale rather
 *  than 4-bit palette due to misuse of the pm_maxvaltobits() function.  Also
 *  moved VERSION to new version.h header file. */

/* GRR 19990713:  fixed redundant freeing of png_ptr and info_ptr in setjmp()
 *  blocks and added png_destroy_write_struct() and pm_close(ifp) in each
 *  pm_error() block.  */

/* GRR 19990308:  declared "clobberable" automatic variables in convertpnm()
 *  static to fix Solaris/gcc stack-corruption bug.  Also installed custom
 *  error-handler to avoid jmp_buf size-related problems (i.e., jmp_buf
 *  compiled with one size in libpng and another size here).  */

/* GRR 19980621:  moved some if-tests out of full-image loops; added fix for
 *  following problem discovered by Magnus Holmgren and debugged by Glenn:
 *
 *    The pnm has three colors:  0 0 0, 204 204 204, and 255 255 255.
 *    These were apparently scaled to 0 0 0, 12 12 12, and 15 15 15.
 *    That would have been OK if the file had been written as color-type
 *    0 (grayscale), but it was written as an indexed-color file, in
 *    which the colors from the pnm file should have been used rather
 *    than the scaled colors.  What appears in the PLTE chunk is
 *    0 0 0, 12 12 12, and 15 15 15, which of course results in a
 *    very dark image.
 *
 *  (temporarily ifdef'd for easier inspection and before/after testing)
 */
#define GRR_GRAY_PALETTE_FIX

#ifndef PNMTOPNG_WARNING_LEVEL
#  define PNMTOPNG_WARNING_LEVEL 0   /* use 0 for backward compatibility, */
#endif                               /*  2 for warnings (1 == error) */

#include <string.h>	/* strcat() */
#include <limits.h>
#include <png.h>	/* includes zlib.h and setjmp.h */
#define VERSION "2.37.5 (24 October 2000) +netpbm"
#include "pnm.h"

#include "ppmcmap.h"

typedef struct cahitem {
    xel color;
    gray alpha;
    int value;
    struct cahitem * next;
} cahitem;

typedef cahitem ** coloralphahash_table;

typedef struct _jmpbuf_wrapper {
  jmp_buf jmpbuf;
} jmpbuf_wrapper;

/* GRR 19991205:  this is used as a test for pre-1999 versions of netpbm and
 *   pbmplus vs. 1999 or later (in which pm_close was split into two) 
 */
#ifdef PBMPLUS_RAWBITS
#  define pm_closer pm_close
#  define pm_closew pm_close
#endif

#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif
#ifndef NONE
#  define NONE 0
#endif
#define MAXCOLORS 256
#define MAXCOMMENTS 256

/* PALETTEMAXVAL is the maxval used in a PNG palette */
#define PALETTEMAXVAL 255

#define PALETTEOPAQUE 255
#define PALETTETRANSPARENT 0

#define SQR(a) ((a)*(a))

/* function prototypes */
#ifdef __STDC__
static void read_text (png_info *info_ptr, FILE *tfp);
static void pnmtopng_error_handler (png_structp png_ptr, png_const_charp msg);
static int convertpnm (FILE *ifp, FILE *afp, FILE *tfp);
int main (int argc, char *argv[]);
#endif

static int verbose = FALSE;
static int interlace = FALSE;
static int downscale = FALSE;
static int transparent = -1;
static char *transstring;
static int alpha = FALSE;
static char *alpha_file;
static int background = -1;
static char *backstring;
static float gamma = -1.0;
static int hist = FALSE;
static float chroma_wx = -1.0;
static float chroma_wy = -1.0;
static float chroma_rx = -1.0;
static float chroma_ry = -1.0;
static float chroma_gx = -1.0;
static float chroma_gy = -1.0;
static float chroma_bx = -1.0;
static float chroma_by = -1.0;
static int phys_x = -1.0;
static int phys_y = -1.0;
static int phys_unit = -1.0;
static int text = FALSE;
static int ztxt = FALSE;
static char *text_file;
static int mtime = FALSE;
static char *date_string;
static char *time_string;
static struct tm time_struct;
static int filter = -1;
static int compression = -1;
static int force = FALSE;
static jmpbuf_wrapper pnmtopng_jmpbuf_struct;
static int errorlevel;



static png_color_16
xelToPngColor_16(xel const input, 
                 xelval const maxval, 
                 xelval const pngMaxval) {
    png_color_16 retval;

    xel scaled;

    PPM_DEPTH(scaled, input, maxval, pngMaxval);

    retval.red   = PPM_GETR(scaled);
    retval.green = PPM_GETG(scaled);
    retval.blue  = PPM_GETB(scaled);
    retval.gray  = PNM_GET1(scaled);

    return retval;
}


static void
closestColorInPalette(pixel          const targetColor, 
                      pixel                palette_pnm[],
                      unsigned int   const paletteSize,
                      unsigned int * const bestIndexP,
                      unsigned int * const bestMatchP) {
    
    unsigned int paletteIndex;
    unsigned int bestIndex = 0;
    unsigned int bestMatch;

    bestMatch = UINT_MAX;
    for (paletteIndex = 0; paletteIndex < paletteSize; ++paletteIndex) {
        pixel const thisColor = palette_pnm[paletteIndex];

        unsigned int match = 
            SQR(PPM_GETR(thisColor) - PPM_GETR(targetColor)) +
            SQR(PPM_GETG(thisColor) - PPM_GETG(targetColor)) +
            SQR(PPM_GETB(thisColor) - PPM_GETB(targetColor));

        if (match < bestMatch) {
            bestMatch = match;
            bestIndex = paletteIndex;
        }
    }
    if (bestIndexP != NULL)
        *bestIndexP = bestIndex;
    if (bestMatchP != NULL)
        *bestMatchP = bestMatch;
}

/* We really ought to make this hash function actually depend upon
   the "a" argument; we just don't know a decent prime number off-hand.
*/
#define HASH_SIZE 20023
#define hashpixelalpha(p,a) ((((long) PPM_GETR(p) * 33023 + \
                               (long) PPM_GETG(p) * 30013 + \
                               (long) PPM_GETB(p) * 27011 ) \
                              & 0x7fffffff ) % HASH_SIZE )

static coloralphahash_table
alloccoloralphahash(void)  {
    coloralphahash_table caht;
    int i;

    caht = (coloralphahash_table) malloc(HASH_SIZE * sizeof(cahitem *));
    if (caht == 0)
        pm_error( "out of memory allocating hash table" );

    for (i = 0; i < HASH_SIZE; ++i)
        caht[i] = NULL;

    return caht;
}


static void
freecoloralphahash(coloralphahash_table const caht) {
    int i;

    for (i = 0; i < HASH_SIZE; ++i) {
        cahitem * p;
        cahitem * next;
        for (p = caht[i]; p; p = next) {
            next = p->next;
            free(p);
        }
    }
    free(caht);
}



static void
addtocoloralphahash(coloralphahash_table const caht,
                    pixel *              const colorP,
                    gray *               const alphaP,
                    int                  const value) {

    int hash;
    cahitem * itemP;

    itemP = (cahitem *) malloc(sizeof(cahitem));
    if (itemP == NULL)
        pm_error("Out of memory building hash table");
    hash = hashpixelalpha(*colorP, *alphaP);
    itemP->color = *colorP;
    itemP->alpha = *alphaP;
    itemP->value = value;
    itemP->next = caht[hash];
    caht[hash] = itemP;
}



static int
lookupColorAlpha(coloralphahash_table const caht,
                 pixel const * const colorP,
                 gray * const alphaP) {

    int hash;
    cahitem * p;

    hash = hashpixelalpha(*colorP, *alphaP);
    for (p = caht[hash]; p; p = p->next)
        if (PPM_EQUAL(p->color, *colorP) && p->alpha == *alphaP)
            return p->value;

    return -1;
}



#ifdef __STDC__
static void read_text (png_info *info_ptr, FILE *tfp)
#else
static void read_text (info_ptr, tfp)
png_info *info_ptr;
FILE *tfp;
#endif
{
  char textline[1024];
  int textpos;
  int i, j;
  int c;
  char *cp;

  overflow2 (MAXCOMMENTS, (int)sizeof(png_text));
  info_ptr->text = (png_text *)malloc2 (MAXCOMMENTS, sizeof (png_text));
  if(info_ptr->text == NULL)
  	pm_error("out of memory");
  j = 0;
  textpos = 0;
  do {
    /* any line >= 1024 bytes long: truncate and treat as EOF */
    c = (textpos < 1024)? getc(tfp) : EOF;
    if (c != '\n' && c != EOF) {
      overflow_add(textpos, 1);
      textline[textpos++] = c;
    } else {
      if (textpos == 0)
        continue;
      overflow_add(textpos, 1);
      textline[textpos++] = '\0';
      if ((textline[0] != ' ') && (textline[0] != '\t')) {
        /* the following is a not that accurate check on Author or Title */
        if ((!ztxt) || (textline[0] == 'A') || (textline[0] == 'T'))
          info_ptr->text[j].compression = -1;
        else
          info_ptr->text[j].compression = 0;
        cp = malloc (textpos);
        info_ptr->text[j].key = cp;
        i = 0;
        if (textline[0] == '"') {
          i++;
          while (textline[i] != '"' && textline[i] != '\n')
            *(cp++) = textline[i++];
          i++;
        } else {
          while (textline[i] != ' ' && textline[i] != '\t' && textline[i] != '\n')
            *(cp++) = textline[i++];
        }
        *(cp++) = '\0';
        cp = malloc (textpos);
        info_ptr->text[j].text = cp;
        while (textline[i] == ' ' || textline[i] == '\t')
          i++;
        strcpy (cp, &textline[i]);
        info_ptr->text[j].text_length = strlen (cp);
        j++;
      } else {
        j--;
        if (info_ptr->text[j].text_length + textpos <= 0) {
          /* malloc() would overflow:  terminate now; lose comment */
          fprintf(stderr, "Invalid text line, aborting\n");
          fflush(stderr);
          c = EOF;
          break;
        }
        overflow_add(info_ptr->text[j].text_length, textpos);
        cp = malloc (info_ptr->text[j].text_length + textpos);
        strcpy (cp, info_ptr->text[j].text);
        cp[ info_ptr->text[j].text_length ] = '\n';
        i = 0;
        while (textline[i] == ' ' || textline[i] == '\t')
          i++;
        strcpy (cp + info_ptr->text[j].text_length + 1, &textline[i]);
        free (info_ptr->text[j].text); /* FIXME: see realloc() comment above */
        info_ptr->text[j].text = cp;
        info_ptr->text[j].text_length = strlen (cp);
        j++;
      }
      textpos = 0;
    }
  } while (c != EOF);
  info_ptr->num_text = j;
}

#ifdef __STDC__
static void pnmtopng_error_handler (png_structp png_ptr, png_const_charp msg)
#else
static void pnmtopng_error_handler (png_ptr, msg)
png_structp png_ptr;
png_const_charp msg;
#endif
{
  jmpbuf_wrapper  *jmpbuf_ptr;

  /* this function, aside from the extra step of retrieving the "error
   * pointer" (below) and the fact that it exists within the application
   * rather than within libpng, is essentially identical to libpng's
   * default error handler.  The second point is critical:  since both
   * setjmp() and longjmp() are called from the same code, they are
   * guaranteed to have compatible notions of how big a jmp_buf is,
   * regardless of whether _BSD_SOURCE or anything else has (or has not)
   * been defined. */

  fprintf(stderr, "pnmtopng:  fatal libpng error: %s\n", msg);
  fflush(stderr);

  jmpbuf_ptr = png_get_error_ptr(png_ptr);
  if (jmpbuf_ptr == NULL) {         /* we are completely hosed now */
    fprintf(stderr,
      "pnmtopng:  EXTREMELY fatal error: jmpbuf unrecoverable; terminating.\n");
    fflush(stderr);
    exit(99);
  }

  longjmp(jmpbuf_ptr->jmpbuf, 1);
}


static void
meaningful_bits_pgm(FILE *         const ifp, 
                    int            const imagepos, 
                    int            const cols,
                    int            const rows,
                    xelval         const maxval,
                    int            const format,
                    unsigned int * const retvalP) {
/*----------------------------------------------------------------------------
   In the PGM raster with maxval 'maxval' at file offset 'imagepos'
   in file 'ifp', the samples may be composed of groups of 1, 2, 4, or 8
   bits repeated.  This would be the case if the image were converted
   at some point from a 2 bits-per-pixel image to an 8-bits-per-pixel
   image, for example.

   If this is the case, we find out and find out how small these repeated
   groups of bits are and return the number of bits.
-----------------------------------------------------------------------------*/
    int mayscale;
    int x, y;
    xel * xelrow;

    xelrow = pnm_allocrow(cols);

    *retvalP = pm_maxvaltobits(maxval);

    if (maxval == 65535) {
        mayscale = TRUE;   /* initial assumption */
        pm_seek(ifp, imagepos);
        for (y = 0 ; y < rows && mayscale ; y++) {
            pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
            for (x = 0 ; x < cols && mayscale ; x++) {
                xel const p = xelrow[x];
                if ( (PNM_GET1 (p)&0xff)*0x101 != PNM_GET1 (p) )
                    mayscale = FALSE;
            }
        }
        if (mayscale)
            *retvalP = 8;
    }
    if (maxval == 255 || *retvalP == 8) {
        mayscale = TRUE; /* initial assumption */
        pm_seek(ifp, imagepos);
        for (y = 0 ; y < rows && mayscale ; y++) {
            pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
            for (x = 0 ; x < cols && mayscale ; x++) {
                if ((PNM_GET1 (xelrow[x]) & 0xf) * 0x11 
                    != PNM_GET1 (xelrow[x]))
                    mayscale = FALSE;
            }
        }
        if (mayscale)
            *retvalP = 4;
    }

    if (maxval == 15 || *retvalP == 4) {
        mayscale = TRUE; /* initial assumption */
        pm_seek(ifp, imagepos);
        for (y = 0 ; y < rows && mayscale ; y++) {
            pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
            for (x = 0 ; x < cols && mayscale ; x++) {
                if ((PNM_GET1 (xelrow[x])&3) * 0x5 != PNM_GET1 (xelrow[x]))
                    mayscale = FALSE;
            }
        }
        if (mayscale) {
            *retvalP = 2;
        }
    }

    if (maxval == 3 || *retvalP == 2) {
        mayscale = TRUE; /* initial assumption */
        pm_seek(ifp, imagepos);
        for (y = 0 ; y < rows && mayscale ; y++) {
            pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
            for (x = 0 ; x < cols && mayscale ; x++) {
                if ((PNM_GET1 (xelrow[x])&1) * 0x3 != PNM_GET1 (xelrow[x]))
                    mayscale = FALSE;
            }
        }
        if (mayscale) {
            *retvalP = 1;
        }
    }
    pnm_freerow(xelrow);
}


static void
meaningful_bits_ppm(FILE *         const ifp, 
                    int            const imagepos, 
                    int            const cols,
                    int            const rows,
                    xelval         const maxval,
                    int            const format,
                    unsigned int * const retvalP) {
/*----------------------------------------------------------------------------
   In the PPM raster with maxval 'maxval' at file offset 'imagepos'
   in file 'ifp', the samples may be composed of groups of 8
   bits repeated twice.  This would be the case if the image were converted
   at some point from a 8 bits-per-pixel image to an 16-bits-per-pixel
   image, for example.

   We return the smallest number of bits we can take from the right of
   a sample without losing information (8 or all).
-----------------------------------------------------------------------------*/
    int mayscale;
    int x, y;
    xel * xelrow;

    xelrow = pnm_allocrow(cols);

    *retvalP = pm_maxvaltobits(maxval);

    if (maxval == 65535) {
        mayscale = TRUE;   /* initial assumption */
        pm_seek(ifp, imagepos);
        for (y = 0 ; y < rows && mayscale ; y++) {
            pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
            for (x = 0 ; x < cols && mayscale ; x++) {
                xel const p = xelrow[x];
                if ( (PPM_GETR (p)&0xff)*0x101 != PPM_GETR (p) ||
                     (PPM_GETG (p)&0xff)*0x101 != PPM_GETG (p) ||
                     (PPM_GETB (p)&0xff)*0x101 != PPM_GETB (p) )
                    mayscale = FALSE;
            }
        }
        if (mayscale)
            *retvalP = 8;
    }
    pnm_freerow(xelrow);
}



static void
alpha_trans(FILE *  const ifp, 
            int     const imagepos, 
            int     const cols, 
            int     const rows, 
            xelval  const maxval, int const format, 
            gray ** const alpha_mask, gray alpha_maxval,
            bool *  const alpha_can_be_transparency_indexP, 
            pixel*  const alpha_transcolorP) {
/*----------------------------------------------------------------------------
  Check if the alpha mask can be represented by a single transparency
  value (i.e. all colors fully opaque except one fully transparent;
  the transparent color may not also occur as fully opaque.
  we have to do this before any scaling occurs, since alpha is only
  possible with 8 and 16-bit.
-----------------------------------------------------------------------------*/
    xel * xelrow;
    bool retval;
        /* Our eventual return value -- alpha mask can be represented by
           a simple transparency index.
        */
    bool found_transparent_pixel;
        /* We found a pixel in the image where the alpha mask says it is
           transparent.
        */
    pixel transcolor;
        /* Color of the transparent pixel mentioned above. */
    int const pnm_type = PNM_FORMAT_TYPE(format);
    
    xelrow = pnm_allocrow(cols);

    {
        int row;
        /* Find a candidate transparent color -- the color of any pixel in the
           image that the alpha mask says should be transparent.
        */
        found_transparent_pixel = FALSE;  /* initial assumption */
        pm_seek(ifp, imagepos);
        for (row = 0 ; row < rows && !found_transparent_pixel ; ++row) {
            int col;
            pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
            for (col = 0 ; col < cols && !found_transparent_pixel; ++col) {
                if (alpha_mask[row][col] == 0) {
                    found_transparent_pixel = TRUE;
                    transcolor = xeltopixel(xelrow[col]);
                }
            }
        }
    }

    if (found_transparent_pixel) {
        int row;
        pm_seek(ifp, imagepos);
        retval = TRUE;  /* initial assumption */
        
        for (row = 0 ; row < rows && retval == TRUE; ++row) {
            int col;
            pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
            for (col = 0 ; col < cols && retval == TRUE; ++col) {
                if (alpha_mask[row][col] == 0) { /* transparent */
                    /* If we have a second transparent color, we're
                       disqualified
                    */
                    if (pnm_type == PPM_TYPE) {
                        if (!PPM_EQUAL (xelrow[col], transcolor))
                            retval = FALSE;
                    } else {
                        if (PNM_GET1 (xelrow[col]) != PNM_GET1 (transcolor))
                            retval = FALSE;
                    }
                } else if (alpha_mask[row][col] != alpha_maxval) {
                    /* Here's an area of the mask that is translucent.  That
                       disqualified us.
                    */
                    retval = FALSE;
                } else 
                    /* Here's an area of the mask that is opaque.  If it's
                       the same color as our candidate transparent color,
                       that disqualifies us.
                    */
                    if (pnm_type == PPM_TYPE) {
                        if (PPM_EQUAL (xelrow[col], transcolor))
                            retval = FALSE;
                    } else {
                        if (PNM_GET1 (xelrow[col]) == PNM_GET1 (transcolor))
                            retval = FALSE;
                    }
            }
        }  
    }
    pnm_freerow(xelrow);

    *alpha_can_be_transparency_indexP = retval;
    *alpha_transcolorP = transcolor;
}



static void
compute_nonalpha_palette(colorhist_vector const chv,
                         int              const colors,
                         pixval           const maxval,
                         pixel                  palette_pnm[],
                         unsigned int *   const paletteSizeP,
                         gray                   trans_pnm[],
                         unsigned int *   const transSizeP) {
/*----------------------------------------------------------------------------
   Compute the palette corresponding to the color set 'chv'
   (consisting of 'colors' distinct colors) assuming a pure-color (no
   transparency) palette.
-----------------------------------------------------------------------------*/
    unsigned int colorIndex;

    for (colorIndex = 0; colorIndex < colors ; ++colorIndex) {
        palette_pnm[colorIndex] = chv[colorIndex].color;
    }
    *paletteSizeP = colors;
    *transSizeP = 0;
}



static void
compute_alpha_palette(FILE *         const ifP, 
                      int            const cols,
                      int            const rows,
                      xelval         const maxval,
                      int            const format,
                      int            const imagepos,
                      int            const colors, 
                      colorhist_vector const chv,
                      gray **        const alpha_mask,
                      pixel                palette_pnm[],
                      gray                 trans_pnm[],
                      unsigned int * const paletteSizeP,
                      unsigned int * const transSizeP,
                      bool *         const tooBigP) {
/*----------------------------------------------------------------------------
   Return the palette of color/alpha pairs for the image indicated by
   'ifP', 'cols', 'rows', 'maxval', 'format', and 'imagepos'.  There
   are 'colors' distinct colors in the image and 'chv' indexes them.
   alpha_mask[] is the Netpbm-style alpha mask for the image.

   Return the palette as the arrays palette_pnm[] and trans_pnm[].
   The ith entry in the palette is the combination of palette[i],
   which defines the color, and trans[i], which defines the
   transparency.

   Return the number of entries in the palette as *paletteSizeP.

   The palette is sorted so that the opaque entries are last, and we return
   *transSizeP as the number of non-opaque entries.

   palette[] and trans[] are allocated by the caller to at least MAXCOLORS
   elements.

   If there are more than MAXCOLORS color/alpha pairs in the image, don't
   return any palette information -- just return *tooBigP == TRUE.
-----------------------------------------------------------------------------*/
    colorhash_table const cht = ppm_colorhisttocolorhash (chv, colors);
    int color_index;
    int row;
    xel * xelrow;

    /* The following data structures describe the alpha/color palette.
       The alpha/color palette is the set of all ordered pairs of
       (color,alpha) in the PNG, including the background color.  The
       actual palette is an array with up to MAXCOLORS elements.  Each
       array element contains a color index from the color palette and
       an alpha value.  All the elements with the same color index are
       contiguous.  alphas_first_index[x] is the index in the
       alpha/color palette of the first element that has color index x.
       alphas_of_color_cnt[x] is the number of elements that have color
       index x.  alphas_of_color[x][y] is the yth alpha value that
       appears with color index x (in order of appearance).
       alpha_color_pair_count is the total number of elements, i.e. the
       total number of combinations color and alpha.
    */
    gray *alphas_of_color[MAXCOLORS];
    unsigned int alphas_first_index[MAXCOLORS+1];
    unsigned int alphas_of_color_cnt[MAXCOLORS+1];

    for (color_index = 0 ; color_index < colors + 1 ; ++color_index) {
        alphas_of_color[color_index] = 
            (gray *)malloc (MAXCOLORS * sizeof (int));
        if (alphas_of_color[color_index] == NULL)
            pm_error ("out of memory allocating alpha/palette entries");
        alphas_of_color_cnt[color_index] = 0;
    }
 
    pm_seek(ifP, imagepos);

    xelrow = pnm_allocrow(cols);

    for (row = 0 ; row < rows ; ++row) {
        int col;
        pnm_readpnmrow(ifP, xelrow, cols, maxval, format);
        pnm_promoteformatrow(xelrow, cols, maxval, format, maxval, PPM_TYPE);
        for (col = 0 ; col < cols ; ++col) {
            int i;
            int const color = ppm_lookupcolor(cht, &xelrow[col]);
            for (i = 0 ; i < alphas_of_color_cnt[color] ; i++) {
                if (alpha_mask[row][col] == alphas_of_color[color][i])
                    break;
            }
            if (i == alphas_of_color_cnt[color]) {
                alphas_of_color[color][i] = alpha_mask[row][col];
                alphas_of_color_cnt[color]++;
            }
        }
    }
    pnm_freerow(xelrow);

    {
        int i;
        alphas_first_index[0] = 0;
        for (i = 1 ; i < colors ; i++)
            alphas_first_index[i] = alphas_first_index[i-1] +
                alphas_of_color_cnt[i-1];
    }
    *paletteSizeP = 
        alphas_first_index[colors-1] + alphas_of_color_cnt[colors-1];
    if (*paletteSizeP > MAXCOLORS) {
        *tooBigP = TRUE;
    } else {
        int mapping[MAXCOLORS];
            /* Sorting of the alpha/color palette.  mapping[x] is the
               index into the sorted PNG palette of the alpha/color
               pair whose index is x in the unsorted PNG palette.
               This mapping sorts the palette so that opaque entries
               are last.  
            */
        unsigned int bot_idx;
        unsigned int top_idx;
        unsigned int colorIndex;

        *tooBigP = FALSE;

        bot_idx = 0;
        top_idx = alphas_first_index[colors-1] + alphas_of_color_cnt[colors-1] - 1;

        /* remap palette indices so opaque entries are last */
        for (colorIndex = 0;  colorIndex < colors;  ++colorIndex) {
            unsigned int j;
            for (j = 0; j < alphas_of_color_cnt[colorIndex]; ++j) {
                unsigned int const paletteIndex = 
                    alphas_first_index[colorIndex] + j;
                if (alphas_of_color[colorIndex][j] == PALETTEOPAQUE)
                    mapping[paletteIndex] = top_idx--;
                else
                    mapping[paletteIndex] = bot_idx++;
            }
        }
        /* indices should have just crossed paths */
        if (bot_idx != top_idx + 1) {
            pm_error ("internal inconsistency: "
                      "remapped bot_idx = %d, top_idx = %d",
                      bot_idx, top_idx);
        }
        for (colorIndex = 0; colorIndex < colors; ++colorIndex) {
            unsigned int j;
            for (j = 0; j < alphas_of_color_cnt[colorIndex]; ++j) {
                unsigned int const paletteIndex = 
                    alphas_first_index[colorIndex] + j;
                palette_pnm[mapping[paletteIndex]] = chv[colorIndex].color;
                trans_pnm[mapping[paletteIndex]] = 
                    alphas_of_color[colorIndex][j];
            }
        }
        *transSizeP = bot_idx;
    }
    { 
        unsigned int color_index;
        for (color_index = 0; color_index < colors + 1; ++color_index)
            free(alphas_of_color[color_index]);
    }
    ppm_freecolorhash (cht);
} 




static void
makeOneColorTransparentInPalette(xel            const transColor, 
                                 bool           const exact,
                                 pixel                palette_pnm[],
                                 unsigned int   const paletteSize,
                                 gray                 trans_pnm[],
                                 unsigned int * const transSizeP) {
/*----------------------------------------------------------------------------
   Find the color 'transColor' in the color/alpha palette defined by
   palette_pnm[], paletteSize, trans_pnm[] and *transSizeP.  If it's
   not there and exact == TRUE, return without changing anything, but
   issue a warning message.  If it's not there and exact == FALSE,
   just find the closest color.

   Make that entry fully transparent.

   Rearrange the palette so that that entry is first.  (The PNG compressor
   can do a better job when the opaque entries are all last in the 
   color/alpha palette).

   We assume every entry in the palette is opaque upon entry.
-----------------------------------------------------------------------------*/
    unsigned int transparentIndex;
    unsigned int distance;
    
    if (*transSizeP != 0)
        pm_error("Internal error: trying to make a color in the palette "
                 "transparent where there already is one.");

    closestColorInPalette(transColor, palette_pnm, paletteSize, 
                          &transparentIndex, &distance);

    if (distance != 0 && exact) {
        pm_message ("specified transparent color not present in palette; "
                    "ignoring -transparent");
        errorlevel = PNMTOPNG_WARNING_LEVEL;
    } else {        
        /* Swap this with the first entry in the palette */
        pixel tmp;
    
        tmp = palette_pnm[transparentIndex];
        palette_pnm[transparentIndex] = palette_pnm[0];
        palette_pnm[0] = tmp;
        
        /* Make it transparent */
        trans_pnm[0] = PGM_TRANSPARENT;
        *transSizeP = 1;
        if (verbose) {
            pixel const p = palette_pnm[0];
            pm_message("Requested transparent color (%u, %u, %u)." ,
                       PPM_GETR(transColor), PPM_GETG(transColor), PPM_GETB(transColor));
            pm_message("Making all occurences of color (%u, %u, %u) "
                       "transparent.",
                       PPM_GETR(p), PPM_GETG(p), PPM_GETB(p));
        }
    }
}



static void
findOrAddBackgroundInPalette(pixel          const backColor, 
                             pixel                palette_pnm[], 
                             unsigned int * const paletteSizeP,
                             unsigned int   const transSize,
                             unsigned int * const backgroundIndexP) {
/*----------------------------------------------------------------------------
  Add the background color to the palette, unless there's no room for
  it, in which case choose a background color that's already in the
  palette.
-----------------------------------------------------------------------------*/
    int backgroundIndex;  /* negative means not found */
    unsigned int paletteIndex;

    backgroundIndex = -1;
    for (paletteIndex = transSize; 
         paletteIndex < *paletteSizeP; 
         ++paletteIndex) 
        if (PPM_EQUAL(palette_pnm[paletteIndex], backColor))
            backgroundIndex = paletteIndex;

    if (backgroundIndex >= 0) {
        /* The background color, opaque, is already in the palette. */
        *backgroundIndexP = backgroundIndex;
        if (verbose) {
            pixel const p = palette_pnm[*backgroundIndexP];
            pm_message ("background color (%u, %u, %u) appears in image.",
                        PPM_GETR(p), PPM_GETG(p), PPM_GETB(p));
        }
    } else {
        /* Try to add the background color, opaque, to the palette. */
        if (*paletteSizeP < MAXCOLORS) {
            /* There's room, so just add it to the end of the palette */

            *backgroundIndexP = (*paletteSizeP)++;
            palette_pnm[*backgroundIndexP] = backColor;
            if (verbose) {
                pixel const p = palette_pnm[*backgroundIndexP];
                pm_message ("added background color (%u, %u, %u) to palette.",
                            PPM_GETR(p), PPM_GETG(p), PPM_GETB(p));
            }
        } else {
            closestColorInPalette(backColor, palette_pnm, *paletteSizeP,
                                  backgroundIndexP, NULL);
            errorlevel = PNMTOPNG_WARNING_LEVEL;
            if (verbose) {
                pixel const p = palette_pnm[*backgroundIndexP];
                pm_message ("no room in palette for background color; "
                            "using closest match (%u, %u, %u) instead",
                            PPM_GETR(p), PPM_GETG(p), PPM_GETB(p));
            }
        }
    }
}



static void 
buildColorLookup(pixel                   palette_pnm[], 
                 unsigned int      const paletteSize,
                 colorhash_table * const chtP) {
/*----------------------------------------------------------------------------
   Create a colorhash_table out of the palette palette_pnm[] (which has
   'paletteSize' entries) so one can look up the palette index of a given
   color.  

   Assume no color appears in palette_pnm[] twice.
-----------------------------------------------------------------------------*/
    colorhash_table const cht = ppm_alloccolorhash();
    unsigned int paletteIndex;

    for (paletteIndex = 0; paletteIndex < paletteSize; ++paletteIndex) 
        ppm_addtocolorhash(cht, &palette_pnm[paletteIndex], paletteIndex);

    *chtP = cht;
}


static void 
buildColorAlphaLookup(pixel              palette_pnm[], 
                      unsigned int const paletteSize,
                      gray               trans_pnm[], 
                      unsigned int const transSize,
                      gray         const alphaMaxval,
                      coloralphahash_table * const cahtP) {
    
    coloralphahash_table const caht = alloccoloralphahash();

    unsigned int paletteIndex;

    for (paletteIndex = 0; paletteIndex < paletteSize; ++paletteIndex) {
        gray paletteTrans;

        if (paletteIndex < transSize)
            paletteTrans = alphaMaxval;
        else
            paletteTrans = trans_pnm[paletteIndex];


        addtocoloralphahash(caht, &palette_pnm[paletteIndex],
                            &trans_pnm[paletteIndex], paletteIndex);
    }
    *cahtP = caht;
}



static void
createPngPalette(pixel              palette_pnm[], 
                 unsigned int const paletteSize, 
                 pixval       const maxval,
                 gray               trans_pnm[],
                 unsigned int const transSize,
                 gray               alpha_maxval,
                 png_color          palette[],
                 png_byte           trans[]) {
/*----------------------------------------------------------------------------
   Create the data structure to be passed to the PNG compressor to represent
   the palette -- the whole palette, color + transparency.

   This is basically just a maxval conversion from the Netpbm-format
   equivalents we get as input.
-----------------------------------------------------------------------------*/
    unsigned int i;

    for (i = 0; i < paletteSize; ++i) {
        pixel p;
        PPM_DEPTH(p, palette_pnm[i], maxval, PALETTEMAXVAL);
        palette[i].red   = PPM_GETR(p);
        palette[i].green = PPM_GETG(p);
        palette[i].blue  = PPM_GETB(p);
#ifdef DEBUG
        pm_message("palette[%u] = (%d, %d, %d)",
                   i, palette[i].red, palette[i].green, palette[i].blue);
#endif
    }

    for (i = 0; i < transSize; ++i) {
        unsigned int const newmv = PALETTEMAXVAL;
        unsigned int const oldmv = alpha_maxval;
        trans[i] = (trans_pnm[i] * newmv + (oldmv/2)) / oldmv;
#ifdef DEBUG
        pm_message("trans[%u] = %d", i, trans[i]);
#endif
    }
}



static int 
convertpnm (FILE *ifp, FILE *afp, FILE *tfp) {
  xel p;
  int rows, cols, format;
  xelval maxval;
      /* The maxval of the input image */
  xelval png_maxval;
      /* The maxval of the samples in the PNG output 
         (must be 1, 3, 7, 15, 255, or 65535)
      */
  pixel transcolor;
      /* The color that is to be transparent, with maxval equal to that
         of the input image.
      */
  int transexact;  
    /* boolean: the user wants only the exact color he specified to be
       transparent; not just something close to it.
    */
  xelval pnm_meaningful_bits;
  pixel backcolor;
      /* The background color, with maxval equal to that of the input
         image.
      */
  png_struct *png_ptr;
  png_info *info_ptr;

  bool colorMapped;
  pixel palette_pnm[MAXCOLORS];
  png_color palette[MAXCOLORS];
      /* The color part of the color/alpha palette passed to the PNG
         compressor 
      */
  unsigned int palette_size = MAXCOLORS;

  gray trans_pnm[MAXCOLORS];
  png_byte  trans[MAXCOLORS];
      /* The alpha part of the color/alpha palette passed to the PNG
         compressor 
      */
  unsigned int trans_size;

  colorhash_table cht;
  coloralphahash_table caht;

  unsigned int background_index;
      /* Index into palette[] of the background color. */

  png_uint_16 histogram[MAXCOLORS];
  png_byte *line;
  png_byte *pp;
  int pass;
  gray alpha_maxval;
  int alpha_rows;
  int alpha_cols;
  int alpha_can_be_transparency_index;

  int colors;
  int fulldepth;
      /* The total number of bits per pixel, including all channels */
  int x, y;
  int i;
  int imagepos;  
      /* file position in input image file of start of image (i.e. after
         the header)
      */
  xel *xelrow;    /* malloc'ed */
      /* The row of the input image currently being processed */

  /* these variables are declared static because gcc wasn't kidding
   * about "variable XXX might be clobbered by `longjmp' or `vfork'"
   * (stack corruption observed on Solaris 2.6 with gcc 2.8.1, even
   * in the absence of any other error condition) */
  static int pnm_type;
  static xelval maxmaxval;
  static gray **alpha_mask;
  static int depth;
  

  /* these guys are initialized to quiet compiler warnings: */
  maxmaxval = 255;
  alpha_mask = NULL;
  depth = 0;
  errorlevel = 0;

  png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING,
    &pnmtopng_jmpbuf_struct, pnmtopng_error_handler, NULL);
  if (png_ptr == NULL) {
    pm_closer (ifp);
    pm_error ("cannot allocate LIBPNG structure");
  }

  info_ptr = png_create_info_struct (png_ptr);
  if (info_ptr == NULL) {
    png_destroy_write_struct (&png_ptr, (png_infopp)NULL);
    pm_closer (ifp);
    pm_error ("cannot allocate LIBPNG structures");
  }

  if (setjmp (pnmtopng_jmpbuf_struct.jmpbuf)) {
    png_destroy_write_struct (&png_ptr, &info_ptr);
    pm_closer (ifp);
    pm_error ("setjmp returns error condition (1)");
  }

  pnm_readpnminit (ifp, &cols, &rows, &maxval, &format);
  imagepos = pm_tell(ifp);
  pnm_type = PNM_FORMAT_TYPE (format);

  xelrow = pnm_allocrow(cols);

  if (verbose) {
    if (pnm_type == PBM_TYPE)    
      pm_message ("reading a PBM file (maxval=%d)", maxval);
    else if (pnm_type == PGM_TYPE)    
      pm_message ("reading a PGM file (maxval=%d)", maxval);
    else if (pnm_type == PPM_TYPE)    
      pm_message ("reading a PPM file (maxval=%d)", maxval);
  }

  if (pnm_type == PGM_TYPE)
    maxmaxval = PGM_OVERALLMAXVAL;
  else if (pnm_type == PPM_TYPE)
    maxmaxval = PPM_OVERALLMAXVAL;

  if (transparent > 0) {  /* -1 or 1 are the only possibilities so far */
    char * transstring2;  
      /* Same as transstring, but with possible leading '=' removed */
    if (transstring[0] == '=') {
      transexact = 1;
      transstring2 = transstring+1;
    } else {
      transexact = 0;
      transstring2 = transstring;
    }  
    /* We do this funny PPM_DEPTH thing instead of just passing 'maxval'
       to ppm_parsecolor() because ppm_parsecolor() does a cheap maxval
       scaling, and this is more precise.
    */
    PPM_DEPTH (transcolor, ppm_parsecolor(transstring2, maxmaxval),
               maxmaxval, maxval);
  }
  if (alpha) {
    pixel alpha_transcolor;

    alpha_mask = pgm_readpgm (afp, &alpha_cols, &alpha_rows, &alpha_maxval);
    if (alpha_cols != cols || alpha_rows != rows) {
      png_destroy_write_struct (&png_ptr, &info_ptr);
      pm_closer (ifp);
      pm_error ("dimensions for image and alpha mask do not agree");
    }
    alpha_trans(ifp, imagepos, cols, rows, maxval, format, 
                alpha_mask, alpha_maxval,
                &alpha_can_be_transparency_index, &alpha_transcolor);

    if (alpha_can_be_transparency_index && !force) {
      if (verbose)
        pm_message ("converting alpha mask to transparency index");
      alpha = FALSE;
      transparent = 2;
      transcolor = alpha_transcolor;
    } else {
      transparent = -1;
    }
  } else
      /* Though there's no alpha_mask, we still need an alpha_maxval for
         use with trans[], which can have stuff in it if the user specified
         a transparent color.
      */
      alpha_maxval = 255;


  /* gcc 2.7.0 -fomit-frame-pointer causes stack corruption here */
  if (background > -1) 
      PPM_DEPTH(backcolor, ppm_parsecolor (backstring, maxmaxval), 
                maxmaxval, maxval);;

  /* first of all, check if we have a grayscale image written as PPM */

  if (pnm_type == PPM_TYPE && !force) {
    int isgray = TRUE;

    pm_seek(ifp, imagepos);
    for (y = 0 ; y < rows && isgray ; y++) {
      pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
      for (x = 0 ; x < cols && isgray ; x++) {
        p = xelrow[x];
        if (PPM_GETR (p) != PPM_GETG (p) || PPM_GETG (p) != PPM_GETB (p))
          isgray = FALSE;
      }
    }
    if (isgray)
      pnm_type = PGM_TYPE;
  }

  /* handle `odd' maxvalues */

    if (maxval > 65535 && !downscale) {
      png_destroy_write_struct (&png_ptr, &info_ptr);
      pm_closer (ifp);
      pm_error ("can only handle files up to 16-bit (use -downscale to override");
    }

  /* Find out if we can mask off redundant (repeated) bits in sample values */

  if (!alpha && pnm_type == PGM_TYPE && !force) 
      meaningful_bits_pgm(ifp, imagepos, cols, rows, maxval, format,
                          &pnm_meaningful_bits);
  else if (pnm_type == PPM_TYPE && !force)
      meaningful_bits_ppm(ifp, imagepos, cols, rows, maxval, format,
                          &pnm_meaningful_bits);
  else 
      pnm_meaningful_bits = pm_maxvaltobits(maxval);

  if (verbose && pnm_meaningful_bits != pm_maxvaltobits(maxval))
      pm_message("Using only %d rightmost bits of input samples.  The "
                 "rest are just copies of those.", pnm_meaningful_bits);
  
  /* -----------------------------------------------------------------
     Determine whether to do a colormapped or truecolor PNG (variable
     'colorMapped'), and if colormapped, compute the full PNG palette
     -- both color and transparency -- as the arrays palette_pnm[]
     and trans_pnm[], with sizes palette_size and trans_size.
  -------------------------------------------------------------------- */

  if (force)
      colorMapped = FALSE;
  else if (pnm_type == PBM_TYPE)
      colorMapped = FALSE;
  else if (maxval > PALETTEMAXVAL)
      colorMapped = FALSE;
  else {
      colorhist_vector chv;

      if (verbose) 
          pm_message ("Finding colors for possible colormap...");

      pm_seek(ifp, imagepos);
      chv = ppm_computecolorhist2 (ifp, cols, rows, maxval, format, 
                                   MAXCOLORS, &colors);
      if (chv == NULL) {
          colorMapped = FALSE;
          if (verbose)
              pm_message("More than %d colors found -- too many for a "
                         "colormapped PNG", MAXCOLORS);
      } else {
          pm_message ("%d colors found", colors);
          if ((pnm_type == PGM_TYPE && pm_maxvaltobits(colors-1) > 
               (pm_maxvaltobits(maxval) / 2)) ||
              (pnm_type == PPM_TYPE && maxval > 255)) {
              
              colorMapped = FALSE;
              if (verbose) 
                  pm_message ("palette index would be larger than the "
                              "indexed value, so not doing colormap");
          } else {
              if (alpha) {
                  bool tooBig;
                  compute_alpha_palette(ifp, cols, rows, maxval, format, 
                                        imagepos,  colors, chv, alpha_mask,
                                        palette_pnm, trans_pnm, 
                                        &palette_size, &trans_size,
                                        &tooBig);
                  if (tooBig) {
                      colorMapped = FALSE;
                      if (verbose)
                          pm_message("too many color/transparency pairs, "
                                     "writing a non-mapped file");
                  } else
                      colorMapped = TRUE;
              } else {
                  colorMapped = TRUE;
                  compute_nonalpha_palette(chv, colors, maxval, 
                                           palette_pnm, &palette_size, 
                                           trans_pnm, &trans_size);

                  if (transparent != -1)
                      makeOneColorTransparentInPalette(
                          transcolor, transexact, 
                          palette_pnm, palette_size, trans_pnm, &trans_size);
              }
              
              if (background > -1) {
                  findOrAddBackgroundInPalette(
                      backcolor, palette_pnm, &palette_size, trans_size, 
                      &background_index);
              }
          }
          ppm_freecolorhist(chv);
      }
  }

  if (colorMapped) {
      /* Compute a lookup table for the palette index.  If there's no alpha
         mask, this is just a standard Netpbm colorhash_table.  If there's
         an alpha mask, it is the slower Pnmtopng-specific 
         coloralphahash_table.
      */
      if (alpha) {
          buildColorAlphaLookup(palette_pnm, palette_size, 
                                trans_pnm, trans_size, alpha_maxval, &caht);
          cht = NULL;
      } else { 
          buildColorLookup(palette_pnm, palette_size, &cht);
          caht = NULL;
      }
      if (verbose)
          pm_message("PNG palette has %u entries, %u of them non-opaque",
                     palette_size, trans_size);
  } else {
      cht = NULL;
      caht = NULL;
  }
  
  
/*----------------------------------------------------------------------------
   Compute the number of bits per raster sample and per raster pixel:
   'depth' and 'fulldepth'.  Note that a raster element may be a
   palette index, or a gray value or color with or without alpha mask.
-----------------------------------------------------------------------------*/
  if (colorMapped) {
      if (palette_size <= 2)
          depth = 1;
      else if (palette_size <= 4)
          depth = 2;
      else if (palette_size <= 16)
          depth = 4;
      else
      depth = 8;
      fulldepth = depth;
      if (verbose)
          pm_message("Writing %d-bit color indexes", depth);
   } else {
      if (pnm_type == PPM_TYPE || alpha) {
          /* PNG allows only depths of 8 and 16 for a truecolor image 
             and for a grayscale image with an alpha channel.
          */
          if (pnm_meaningful_bits > 8)
              depth = 16;
          else 
              depth = 8;
      } else {
          /* A grayscale, non-colormapped, no-alpha PNG may have any 
             bit depth from 1 to 16
          */
          if (pnm_meaningful_bits > 8)
              depth = 16;
          else if (pnm_meaningful_bits > 4)
              depth = 8;
          else if (pnm_meaningful_bits > 2)
              depth = 4;
          else if (pnm_meaningful_bits > 1)
              depth = 2;
          else
              depth = 1;
      }
    if (alpha) {
      if (pnm_type == PPM_TYPE)
      {
        overflow2(depth, 4);
        fulldepth = 4 * depth;
      }
      else
      {
        overflow2(depth, 2);
        fulldepth = 2 * depth;
      }
    } else {
      if (pnm_type == PPM_TYPE)
      {
        overflow2(depth, 3);
        fulldepth = 3 * depth;
      }
      else
        fulldepth = depth;
    }
    if (verbose)
      pm_message("Writing %d bits per component per pixel", depth);
  }

  if (verbose)
    pm_message ("writing a%s %d-bit %s%s file%s",
                fulldepth == 8 ? "n" : "", fulldepth,
                colorMapped ? "palette": 
                (pnm_type == PPM_TYPE ? "RGB" : "gray"),
                alpha ? (colorMapped ? "+transparency" : "+alpha") : "",
                interlace? " (interlaced)" : "");

  /* now write the file */

  png_maxval = pm_bitstomaxval(depth);

  if (setjmp (pnmtopng_jmpbuf_struct.jmpbuf)) {
    png_destroy_write_struct (&png_ptr, &info_ptr);
    pm_closer (ifp);
    pm_error ("setjmp returns error condition (2)");
  }

  png_init_io (png_ptr, stdout);
  info_ptr->width = cols;
  info_ptr->height = rows;
  info_ptr->bit_depth = depth;

  if (colorMapped)
    info_ptr->color_type = PNG_COLOR_TYPE_PALETTE;
  else if (pnm_type == PPM_TYPE)
    info_ptr->color_type = PNG_COLOR_TYPE_RGB;
  else
    info_ptr->color_type = PNG_COLOR_TYPE_GRAY;

  if (alpha && info_ptr->color_type != PNG_COLOR_TYPE_PALETTE)
    info_ptr->color_type |= PNG_COLOR_MASK_ALPHA;

  info_ptr->interlace_type = interlace;

  /* gAMA chunk */
  if (gamma != -1.0) {
    info_ptr->valid |= PNG_INFO_gAMA;
    info_ptr->gamma = gamma;
  }

  /* cHRM chunk */
  if (chroma_wx != -1.0) {
    info_ptr->valid |= PNG_INFO_cHRM;
    info_ptr->x_white = chroma_wx;
    info_ptr->y_white = chroma_wy;
    info_ptr->x_red = chroma_rx;
    info_ptr->y_red = chroma_ry;
    info_ptr->x_green = chroma_gx;
    info_ptr->y_green = chroma_gy;
    info_ptr->x_blue = chroma_bx;
    info_ptr->y_blue = chroma_by;
  }

  /* pHYS chunk */
  if (phys_unit != -1.0) {
    info_ptr->valid |= PNG_INFO_pHYs;
    info_ptr->x_pixels_per_unit = phys_x;
    info_ptr->y_pixels_per_unit = phys_y;
    info_ptr->phys_unit_type = phys_unit;
  }

  if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE) {
    createPngPalette(palette_pnm, palette_size, maxval,
                     trans_pnm, trans_size, alpha_maxval, 
                     palette, trans);
    info_ptr->valid |= PNG_INFO_PLTE;
    info_ptr->palette = palette;
    info_ptr->num_palette = palette_size;
    if (trans_size > 0) {
        info_ptr->valid |= PNG_INFO_tRNS;
        info_ptr->trans = trans;
        info_ptr->num_trans = trans_size;   /* omit opaque values */
    }

    /* creating hIST chunk */
    if (hist) {
        colorhist_vector chv;
        /* We need to build this from the current palette, which is 
           complicated by the facts of transparency and background
           colors.  And there may not be a palette.
        */
        pm_error("histogram feature not implemented.");
            
        for (i = 0 ; i < MAXCOLORS ; i++)
            histogram[i] = chv[i].value;
        info_ptr->valid |= PNG_INFO_hIST;
        info_ptr->hist = histogram;
        if (verbose)
            pm_message ("histogram created");
    }
  } else { /* color_type != PNG_COLOR_TYPE_PALETTE */
    if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY ||
        info_ptr->color_type == PNG_COLOR_TYPE_RGB) {
        if (transparent > 0) {
           info_ptr->valid |= PNG_INFO_tRNS;
            info_ptr->trans_values = 
                xelToPngColor_16(transcolor, maxval, png_maxval);
        }

    } else {
        /* This is PNG_COLOR_MASK_ALPHA.  Transparency will be handled
           by the alpha channel, not a transparency color.
        */
    }
    if (verbose) {
        if (info_ptr->valid && PNG_INFO_tRNS) 
            pm_message("Transparent color {gray, red, green, blue} = "
                       "{%d, %d, %d, %d}",
                       info_ptr->trans_values.gray,
                       info_ptr->trans_values.red,
                       info_ptr->trans_values.green,
                       info_ptr->trans_values.blue);
        else
            pm_message("No transparent color");
    }
  }
  /* bKGD chunk */
  if (background > -1) {
      info_ptr->valid |= PNG_INFO_bKGD;
      if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE) {
          info_ptr->background.index = background_index;
      } else {
          info_ptr->background = 
              xelToPngColor_16(backcolor, maxval, png_maxval);
          if (verbose)
              pm_message("Writing bKGD chunk with background color "
                         " {gray, red, green, blue} = {%d, %d, %d, %d}",
                         info_ptr->background.gray, 
                         info_ptr->background.red, 
                         info_ptr->background.green, 
                         info_ptr->background.blue ); 
      }
  }

  if (!colorMapped && (png_maxval != maxval || 
                       (alpha && png_maxval != alpha_maxval))) {
    /* We're writing in a bit depth that doesn't match the maxval of the
       input image and the alpha mask.  So we write an sBIT chunk to tell
       what the original image's maxval was.  The sBit chunk doesn't let
       us specify any maxval -- only powers of two minus one.  So we pick
       the power of two minus one which is greater than or equal to the
       actual input maxval.
    */
    /* sBIT chunk */
    int sbitval;

    info_ptr->valid |= PNG_INFO_sBIT;

    sbitval = pm_maxvaltobits(maxval);

    if (info_ptr->color_type & PNG_COLOR_MASK_COLOR) {
      info_ptr->sig_bit.red   = sbitval;
      info_ptr->sig_bit.green = sbitval;
      info_ptr->sig_bit.blue  = sbitval;
    } else {
      info_ptr->sig_bit.gray = sbitval;
    }
    if (verbose)
        pm_message("Writing sBIT chunk with sbitval = %d", sbitval);

    if (info_ptr->color_type & PNG_COLOR_MASK_ALPHA) {
      info_ptr->sig_bit.alpha = pm_maxvaltobits(alpha_maxval);
    }
  }

  /* tEXT and zTXT chunks */
  if ((text) || (ztxt)) {
    read_text (info_ptr, tfp);
  }

  /* tIME chunk */
  if (mtime) {
    info_ptr->valid |= PNG_INFO_tIME;
    sscanf (date_string, "%d-%d-%d", &time_struct.tm_year,
                                     &time_struct.tm_mon,
                                     &time_struct.tm_mday);
    if (time_struct.tm_year > 1900)
      time_struct.tm_year -= 1900;
    time_struct.tm_mon--; /* tm has monthes 0..11 */
    sscanf (time_string, "%d:%d:%d", &time_struct.tm_hour,
                                     &time_struct.tm_min,
                                     &time_struct.tm_sec);
    png_convert_from_struct_tm (&info_ptr->mod_time, &time_struct);
  }

  /* explicit filter-type (or none) required */
  if ((filter >= 0) && (filter <= 4))
  {
    png_set_filter (png_ptr, 0, filter);
  }

  /* zlib compression-level (or none) required */
  if ((compression >= 0) && (compression <= 9))
  {
    png_set_compression_level (png_ptr, compression);
  }

  /* write the png-info struct */
  png_write_info (png_ptr, info_ptr);

  if ((text) || (ztxt))
    /* prevent from being written twice with png_write_end */
    info_ptr->num_text = 0;

  if (mtime)
    /* prevent from being written twice with png_write_end */
    info_ptr->valid &= ~PNG_INFO_tIME;

  /* let libpng take care of, e.g., bit-depth conversions */
  png_set_packing (png_ptr);

  /* max: 3 color channels, one alpha channel, 16-bit */
  if ((line = (png_byte *) malloc2(cols, 8)) == NULL)
  {
    png_destroy_write_struct (&png_ptr, &info_ptr);
    pm_closer (ifp);
    pm_error ("out of memory allocating PNG row buffer");
  }

  for (pass = 0 ; pass < png_set_interlace_handling (png_ptr) ; pass++) {
      pm_seek(ifp, imagepos);
      for (y = 0 ; y < rows ; y++) {
          pnm_readpnmrow(ifp, xelrow, cols, maxval, format);
          pnm_promoteformatrow(xelrow, cols, maxval, format, maxval, PPM_TYPE);
          pp = line;
          for (x = 0 ; x < cols ; x++) {
              xel p_png;
              xel const p = xelrow[x];
              PPM_DEPTH(p_png, p, maxval, png_maxval);
              if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY ||
                  info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                  if (depth == 16)
                      *pp++ = PNM_GET1 (p_png) >> 8;
                  *pp++ = PNM_GET1 (p_png)&0xff;
              } else if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE) {
                  unsigned int paletteIndex;
                  if (alpha)
                      paletteIndex = lookupColorAlpha(caht, &p, 
                                                      &alpha_mask[y][x]);
                  else
                      paletteIndex = ppm_lookupcolor(cht, &p);
                  *pp++ = paletteIndex;
              } else if (info_ptr->color_type == PNG_COLOR_TYPE_RGB ||
                         info_ptr->color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
                  if (depth == 16)
                      *pp++ = PPM_GETR (p_png) >> 8;
                  *pp++ = PPM_GETR (p_png)&0xff;
                  if (depth == 16)
                      *pp++ = PPM_GETG (p_png) >> 8;
                  *pp++ = PPM_GETG (p_png)&0xff;
                  if (depth == 16)
                      *pp++ = PPM_GETB (p_png) >> 8;
                  *pp++ = PPM_GETB (p_png)&0xff;
              } else {
                  png_destroy_write_struct (&png_ptr, &info_ptr);
                  pm_closer (ifp);
                  pm_error (" (can't happen) undefined color_type");
              }
              if (info_ptr->color_type & PNG_COLOR_MASK_ALPHA) {
                  int const png_alphaval = (int)
                      alpha_mask[y][x] * (float) png_maxval / maxval +0.5;
                  if (depth == 16)
                      *pp++ = png_alphaval >> 8;
                  *pp++ = png_alphaval & 0xff;
              }
          }
          png_write_row (png_ptr, line);
      }
  }
  png_write_end (png_ptr, info_ptr);


#if 0
  /* The following code may be intended to solve some segfault problem
     that arises with png_destroy_write_struct().  The latter is the
     method recommended in the libpng documentation and this program 
     will not compile under Cygwin because the Windows DLL for libpng
     does not contain png_write_destroy() at all.  Since the author's
     comment below does not make it clear what the segfault issue is,
     we cannot consider it.  -Bryan 00.09.15
*/

  png_write_destroy (png_ptr);
  /* flush first because free(png_ptr) can segfault due to jmpbuf problems
     in png_write_destroy */
  fflush (stdout);
  free (png_ptr);
  free (info_ptr);
#else
  png_destroy_write_struct(&png_ptr, &info_ptr);
#endif

  pnm_freerow(xelrow);

  if (cht)
      ppm_freecolorhash(cht);
  if (caht)
      freecoloralphahash(caht);

  return errorlevel;
}

  int main (int argc, char *argv[])
{
  FILE *ifp, *tfp, *afp;
  int argn, errorlevel=0;

  char *usage = "[-verbose] [-downscale] [-interlace] [-alpha file] ...\n\
             ... [-transparent color] [-background color] [-gamma value] ...\n\
             ... [-hist] [-chroma wx wy rx ry gx gy bx by] [-phys x y unit] ...\n\
             ... [-text file] [-ztxt file] [-time [yy]yy-mm-dd hh:mm:ss] ...\n\
             ... [-filter 0..4] [-compression 0..9] [-force] [pnmfile]";

  pnm_init (&argc, argv);
  argn = 1;

  while (argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0') {
    if (pm_keymatch (argv[argn], "-verbose", 2)) {
      verbose = TRUE;
    } else
    if (pm_keymatch (argv[argn], "-downscale", 2)) {
      downscale = TRUE;
    } else
    if (pm_keymatch (argv[argn], "-interlace", 2)) {
      interlace = TRUE;
    } else
    if (pm_keymatch (argv[argn], "-alpha", 2)) {
      if (transparent > 0)
        pm_error ("-alpha and -transparent are mutually exclusive");
      alpha = TRUE;
      if (++argn < argc)
        alpha_file = argv[argn];
      else
        pm_usage (usage);
    } else
    if (pm_keymatch (argv[argn], "-transparent", 3)) {
      if (alpha)
        pm_error ("-alpha and -transparent are mutually exclusive");
      transparent = 1;
      if (++argn < argc)
        transstring = argv[argn];
      else
        pm_usage (usage);
    } else
    if (pm_keymatch (argv[argn], "-background", 2)) {
      background = 1;
      if (++argn < argc)
        backstring = argv[argn];
      else
        pm_usage (usage);
    } else
    if (pm_keymatch (argv[argn], "-gamma", 2)) {
      if (++argn < argc)
        sscanf (argv[argn], "%f", &gamma);
      else
        pm_usage (usage);
    } else
    if (pm_keymatch (argv[argn], "-hist", 2)) {
      hist = TRUE;
    } else
    if (pm_keymatch (argv[argn], "-chroma", 3)) {
      if (++argn < argc)
        sscanf (argv[argn], "%f", &chroma_wx);
      else
        pm_usage (usage);
      if (++argn < argc)
        sscanf (argv[argn], "%f", &chroma_wy);
      else
        pm_usage (usage);
      if (++argn < argc)
        sscanf (argv[argn], "%f", &chroma_rx);
      else
        pm_usage (usage);
      if (++argn < argc)
        sscanf (argv[argn], "%f", &chroma_ry);
      else
        pm_usage (usage);
      if (++argn < argc)
        sscanf (argv[argn], "%f", &chroma_gx);
      else
        pm_usage (usage);
      if (++argn < argc)
        sscanf (argv[argn], "%f", &chroma_gy);
      else
        pm_usage (usage);
      if (++argn < argc)
        sscanf (argv[argn], "%f", &chroma_bx);
      else
        pm_usage (usage);
      if (++argn < argc)
        sscanf (argv[argn], "%f", &chroma_by);
      else
        pm_usage (usage);
    } else
    if (pm_keymatch (argv[argn], "-phys", 3)) {
      if (++argn < argc)
        sscanf (argv[argn], "%d", &phys_x);
      else
        pm_usage (usage);
      if (++argn < argc)
        sscanf (argv[argn], "%d", &phys_y);
      else
        pm_usage (usage);
      if (++argn < argc)
        sscanf (argv[argn], "%d", &phys_unit);
      else
        pm_usage (usage);
    } else
    if (pm_keymatch (argv[argn], "-text", 3)) {
      text = TRUE;
      if (++argn < argc)
        text_file = argv[argn];
      else
        pm_usage (usage);
    } else
    if (pm_keymatch (argv[argn], "-ztxt", 2)) {
      ztxt = TRUE;
      if (++argn < argc)
        text_file = argv[argn];
      else
        pm_usage (usage);
    } else
    if (pm_keymatch (argv[argn], "-time", 3)) {
      mtime = TRUE;
      if (++argn < argc) {
        date_string = argv[argn];
        if (++argn < argc)
          time_string = argv[argn];
        else
          pm_usage (usage);
      } else {
        pm_usage (usage);
      }
    } else 
    if (pm_keymatch (argv[argn], "-filter", 3)) {
      if (++argn < argc)
      {
        sscanf (argv[argn], "%d", &filter);
        if ((filter < 0) || (filter > 4))
        {
          pm_message
            ("filter must be 0 (none), 1 (sub), 2 (up), 3 (avg) or 4 (paeth)");
          pm_usage (usage);
        }
      }
      else
        pm_usage (usage);
    } else
    if (pm_keymatch (argv[argn], "-compression", 3)) {
      if (++argn < argc)
      {
        sscanf (argv[argn], "%d", &compression);
        if ((compression < 0) || (compression > 9))
        {
          pm_message ("zlib compression must be between 0 (none) and 9 (max)");
          pm_usage (usage);
        }
      }
      else
        pm_usage (usage);
    } else
    if (pm_keymatch (argv[argn], "-force", 3)) {
      force = TRUE;
    } else {
      fprintf(stderr,"pnmtopng version %s.\n", VERSION);
      fprintf(stderr, "   Compiled with libpng %s; using libpng %s.\n",
        PNG_LIBPNG_VER_STRING, png_libpng_ver);
      fprintf(stderr, "   Compiled with zlib %s; using zlib %s.\n",
        ZLIB_VERSION, zlib_version);
      fprintf(stderr,    
        "   Compiled with %d-bit netpbm support (PPM_OVERALLMAXVAL = %d).\n",
        pm_maxvaltobits (PPM_OVERALLMAXVAL), PPM_OVERALLMAXVAL);
      fprintf(stderr, "\n");
      pm_usage (usage);
    }
    argn++;
  }

  { 
      char *input_file;
      if (argn == argc)
          input_file = "-";
      else {
          input_file = argv[argn];
          argn++;
      }
      ifp = pm_openr_seekable(input_file);
  }

  if (argn != argc)
    pm_usage (usage);

  if (alpha)
    afp = pm_openr (alpha_file);
  else
    afp = NULL;

  if ((text) || (ztxt))
    tfp = pm_openr (text_file);
  else
    tfp = NULL;

  convertpnm (ifp, afp, tfp);

  if (alpha)
    pm_closer (afp);
  if ((text) || (ztxt))
    pm_closer (tfp);

  pm_closer (ifp);
  pm_closew (stdout);

  return errorlevel;
}
