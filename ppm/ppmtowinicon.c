/* ppmtowinicon.c - read portable pixmap file(s) and write a MS Windows .ico
**
** Copyright (C) 2000 by Lee Benfield - lee@benf.org
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

/*
 * Changelog
 *
 * 2002/01/25 - Use pbm instead of ppm for alpha maps and fix alpha
 *      sense. (black = transparent, white = opaque).
 *      Option renamed to -andpbms; -andppms retained
 *      for compatibility.
 *      ppmtowinicon should now be the true reverse of winicontoppm.
 *      Filled some "ignored" fields in ICO output for maximum
 *      compatibility.  MVB.
 *
 * 2000/05/24 - using colorhash_table instead of a straight array search
 *              Though it's a little slower for files due to the low # of
 *              colors, its neater.  LAB.
 */

#include <string.h>
#include <math.h>
#include <string.h>

#include "winico.h"
#include "ppm.h"
#include "ppmcmap.h"

extern void *realloc2(void *, int, int);

#define MAJVERSION 0
#define MINVERSION 3

#define MAXCOLORS 256


static int      verbose = 0;
static int      readAndMaps = 0;
static int      file_offset = 0;    /* not actually used, but useful for debug. */
static int      readFromStdin = 0;
static char     er_write[] = "%s: write error";
static const char *   infname  = "";
static const char *   outfname = "-";
static FILE *   ofp;
static FILE *   ifp;

static void 
PutByte(int const v) {
   
   if (putc(v, ofp) == EOF)
     {
    pm_error(er_write, outfname);
     }
}
   
static void 
PutShort(short const v) {
   
   if (pm_writelittleshort(ofp, v) == -1)
     {
    pm_error(er_write, outfname);
     }
   
}
   
static void 
PutLong(long const v) {
   
   if (pm_writelittlelong(ofp, v) == -1)
     {
    pm_error(er_write, outfname);
     }
   
}
   
/*
 * These have no purpose but to wrapper the Byte, Short & Long 
 * functions.
 */
static void 
writeU1 (u1 const v) {
   file_offset++;
   PutByte(v);
}

static  void 
writeU2 (u2 const v) {
   file_offset +=2;
   PutShort(v);
}

static void 
writeU4 (u4 const v) {
   file_offset += 4;
   PutLong(v);
}

static MS_Ico 
createIconFile (void) {
   MS_Ico MSIconData = malloc( sizeof (* MSIconData) );

   if (MSIconData == NULL)
       pm_error("out of memory");

   MSIconData->reserved     = 0;
   MSIconData->type         = 1;
   MSIconData->count        = 0;
   MSIconData->entries      = NULL;
   return MSIconData;
}


/* create andBitmap from pgm */

static ICON_bmp 
createAndBitmap (gray ** const ba, int const cols, int const rows,
                 gray const maxval) {
   /*
    * How wide should the u1 string for each row be?
    * each byte is 8 pixels, but must be a multiple of 4 bytes.
    */
   ICON_bmp icBitmap = malloc ( sizeof (* icBitmap) );
   int xBytes,y,x;
   int wt = cols;
   u1 ** rowData;
   if (icBitmap == NULL)
        pm_error("out of memory");
   wt >>= 3;
   if (wt & 3) {
      wt = (wt & ~3) + 4;
   }
   xBytes = wt;
   rowData = malloc2 ( rows , sizeof (char *));
   if (rowData == NULL)
        pm_error("out of memory");
   icBitmap->xBytes = xBytes;
   icBitmap->data   = rowData;
   overflow2(xBytes, rows);
   icBitmap->size   = xBytes * rows;
   for (y=0;y<rows;y++) {
      u1 * row = malloc2 ( xBytes, sizeof (u1));
      int byteOn = 0;
      int bitOn = 128;
      if (row == NULL)
          pm_error("out of memory");
      memset (row, 0, xBytes);
      rowData[rows-y-1] = row;
      /* 
       * Check there's a bit array, otherwise we're just faking this...
       */
      if (ba) {
         for (x=0;x<cols;x++) {
            /* Black (bit clear) is transparent in PGM alpha maps,
             * in ICO bit *set* is transparent.
             */
            if (ba[y][x] <= maxval/2) row[byteOn] |= bitOn;

            if (bitOn == 1) {
               byteOn++;
               bitOn = 128;
            } else {
               bitOn >>= 1;
            }
         }
      }
   }
   return icBitmap;
}


/*
 * Depending on if the image is stored as 1bpp, 4bpp or 8bpp, the 
 * encoding mechanism is different.
 * 
 * I didn't re-use the code from ppmtobmp since I need to keep the
 * bitmaps in memory till I've loaded all ppms.
 * 
 * 8bpp => 1 byte/palette index.
 * 4bpp => High Nibble, Low Nibble
 * 1bpp => 1 palette value per bit, high bit 1st.
 */
static ICON_bmp 
create1Bitmap (pixel ** const pa, int const cols, int const rows, 
               colorhash_table const cht) {
   /*
    * How wide should the u1 string for each row be?
    * each byte is 8 pixels, but must be a multiple of 4 bytes.
    */
   ICON_bmp icBitmap = malloc ( sizeof (* icBitmap) );
   int xBytes,y,x;
   int wt = cols;
   u1 ** rowData;
   if (icBitmap == NULL)
        pm_error("out of memory");
   wt >>= 3;
   if (wt & 3) {
      wt = (wt & ~3) + 4;
   }
   xBytes = wt;
   rowData = malloc2 ( rows,  sizeof (char *));
   if (rowData == NULL)
        pm_error("out of memory");
   icBitmap->xBytes = xBytes;
   icBitmap->data   = rowData;
   overflow2(xBytes, rows);
   icBitmap->size   = xBytes * rows;
   for (y=0;y<rows;y++) {
      u1 * row = malloc2 ( xBytes , sizeof (u1));
      int byteOn = 0;
      int bitOn = 128;
      int value;
      if (row == NULL)
         pm_error("out of memory");
      memset (row, 0, xBytes);
      rowData[rows-y-1] = row;
      /* 
       * Check there's a pixel array, otherwise we're just faking this...
       */
      if (pa) {
         for (x=0;x<cols;x++) {
            /*
             * So we've got a colorhash_table with two colors in it.
             * Which is black?!
             * 
             * Unless the hashing function changes, 0's black.
             */
            value = ppm_lookupcolor(cht, &pa[y][x]);
            if (!value) {
               /* leave black. */
            } else {
               row[byteOn] |= bitOn;
            }
            if (bitOn == 1) {
               byteOn++;
               bitOn = 128;
            } else {
               bitOn >>= 1;
            }
         }
      }
   }
   return icBitmap;
}


static ICON_bmp 
create4Bitmap (pixel ** const pa, int const cols, int const rows,
               colorhash_table const cht) {
   /*
    * How wide should the u1 string for each row be?
    * each byte is 8 pixels, but must be a multiple of 4 bytes.
    */
   ICON_bmp icBitmap = malloc ( sizeof (* icBitmap) );
   int xBytes,y,x;
   int wt = cols;
   u1 ** rowData;
   if (icBitmap == NULL)
        pm_error("out of memory");
   wt >>= 1;
   if (wt & 3) {
      wt = (wt & ~3) + 4;
   }
   xBytes = wt;
   rowData = malloc2 ( rows , sizeof (char *));
   if (rowData == NULL)
        pm_error("out of memory");
   icBitmap->xBytes = xBytes;
   icBitmap->data   = rowData;
   overflow2(xBytes, rows);
   icBitmap->size   = xBytes * rows;

   for (y=0;y<rows;y++) {
      u1 * row = malloc2 ( xBytes, sizeof (u1));
      int byteOn = 0;
      int nibble = 1;   /* high nibble = 1, low nibble = 0; */
      int value;
      if (row == NULL)
         pm_error("out of memory");
      memset (row, 0, xBytes);
      rowData[rows-y-1] = row;
      /* 
       * Check there's a pixel array, otherwise we're just faking this...
       */
      if (pa) {
         for (x=0;x<cols;x++) {
            value = ppm_lookupcolor(cht, &pa[y][x]);
            /*
             * Shift it, if we're putting it in the high nibble.
             */
            if (nibble) {
               value <<= 4;
            }
            row[byteOn] |= value;
            if (nibble) {
               nibble = 0;
            } else {
               nibble = 1;
               byteOn++;
            }
         }
      }
   }
   return icBitmap;
}



static ICON_bmp 
create8Bitmap (pixel ** const pa, int const cols, int const rows,
               colorhash_table const cht) {
   /*
    * How wide should the u1 string for each row be?
    * each byte is 8 pixels, but must be a multiple of 4 bytes.
    */
   ICON_bmp icBitmap = malloc ( sizeof (* icBitmap) );
   int xBytes,y,x;
   int wt = cols;
   u1 ** rowData;
   if (icBitmap == NULL)
        pm_error("out of memory");
   if (wt & 3) {
      wt = (wt & ~3) + 4;
   }
   xBytes = wt;
   rowData = malloc2 ( rows,  sizeof (char *));
   if (rowData == NULL)
        pm_error("out of memory");
   icBitmap->xBytes = xBytes;
   icBitmap->data   = rowData;
   overflow2(xBytes, rows);
   icBitmap->size   = xBytes * rows;

   for (y=0;y<rows;y++) {
      u1 * row = malloc2 ( xBytes, sizeof (u1));
      if (row == NULL)
         pm_error("out of memory");
      memset (row, 0, xBytes);
      rowData[rows-y-1] = row;
      /* 
       * Check there's a pixel array, otherwise we're just faking this...
       */
      if (pa) {
         for (x=0;x<cols;x++) {
            row[x] = ppm_lookupcolor(cht, &pa[y][x]);
         }
      }
   }
   return icBitmap;
}



static IC_InfoHeader 
createInfoHeader(IC_Entry const entry, ICON_bmp const xbmp,
                 ICON_bmp const abmp) {
   IC_InfoHeader ih  = malloc ( sizeof (* ih) );
   if (ih == NULL)
       pm_error("out of memory");

   ih->size          = 40;
   ih->width         = entry->width;
   ih->height        = entry->height * 2;  
   ih->planes        = 1;  
   ih->bitcount      = entry->bitcount;
   ih->compression   = 0;
   ih->imagesize     = entry->width * entry->height * 8 / entry->bitcount;
   ih->x_pixels_per_m= 0;
   ih->y_pixels_per_m= 0;
   ih->colors_used   = 1 << entry->bitcount;
   ih->colors_important = 0;
   return ih;
}



static IC_Palette 
createCleanPalette(void) {
   IC_Palette palette = malloc ( sizeof (* palette) );
   int x;
   if (palette == NULL)
        pm_error("out of memory");
   palette->colors = malloc2 (MAXCOLORS, sizeof(IC_Color *));
   if (palette->colors == NULL)
        pm_error("out of memory");

   for (x=0;x<MAXCOLORS;x++ ){
      palette->colors[x] = NULL;
   }
   return palette;
}



static void 
addColorToPalette(IC_Palette const palette, int const i,
                  int const r, int const g, int const b) {
   palette->colors[i] = malloc ( sizeof (* palette->colors[i]) );
    if (palette->colors[i] == NULL)
        pm_error("out of memory");

    palette->colors[i]->red      = r;
    palette->colors[i]->green    = g;
    palette->colors[i]->blue     = b;
    palette->colors[i]->reserved = 0;
}



static ICON_bmp 
createBitmap (int const bpp, pixel ** const pa, 
              int const cols, int const rows, 
              colorhash_table const cht) {
    
    ICON_bmp retval;
    const int assumed_bpp = (pa == NULL) ? 1 : bpp;

    switch (assumed_bpp) {
    case 1:
        retval = create1Bitmap (pa,cols,rows,cht);
        break;
    case 4:
        retval = create4Bitmap (pa,cols,rows,cht);
        break;
    case 8:
    default:
        retval = create8Bitmap (pa,cols,rows,cht);
        break;
    }
    return retval;
}



static void 
addEntryToIcon (MS_Ico const MSIconData, const char * const xorPPM,
                const char * const andPGM) {

   IC_Entry entry = malloc ( sizeof (* entry) );
   FILE * xorfile;
   FILE * andfile;
   pixel ** xorPPMarray;
   gray ** andPGMarray;
   ICON_bmp xorBitmap;
   ICON_bmp andBitmap;
   int xorRows, xorCols, andRows, andCols;
   int bpp, colors, i;
   int entry_cols;
   IC_Palette palette = createCleanPalette();
   colorhist_vector xorChv;
   colorhash_table  xorCht;
   colorhash_table  andCht; 
   
   pixval xorMaxval;
   gray andMaxval;
   if (entry == NULL)
       pm_error("out of memory");

   /*
    * Read the xor PPM.
    */
   xorfile = pm_openr(xorPPM);
   xorPPMarray = ppm_readppm( xorfile, &xorCols, &xorRows, &xorMaxval );
   pm_close(xorfile);
   /*
    * Since the entry uses 1 byte to hold the width and height of the icon, the
    * image can't be more than 256 x 256.
    */
   if (xorRows > 255 || xorCols > 255) {
      pm_error("Max size for a icon is 255 x 255 (1 byte fields)\n%s is "
               "%d x %d", xorPPM, xorCols, xorRows);
   }
   
   if (verbose) pm_message("read PPM: maxval = %d", xorMaxval);

   /*
    * Figure out the colormap and turn it into the appropriate GIF
    * colormap - this code's pretty much straight from ppmtobpm
    */
   if (verbose) pm_message("computing colormap...");
   xorChv = ppm_computecolorhist(xorPPMarray, xorCols, xorRows, MAXCOLORS, 
                                 &colors);
   if (xorChv == (colorhist_vector) 0)
     pm_error("%s has too many colors - try doing a 'pnmquant %d'"
          , xorPPM, MAXCOLORS);
   if (verbose) pm_message("%d colors found", colors);

   if (verbose && (xorMaxval > 255))
       pm_message("maxval is not 255 - automatically rescaling colors");
   for (i = 0; i < colors; ++i)
     {
    if (xorMaxval == 255)
      {
         addColorToPalette(palette,i,
                   PPM_GETR(xorChv[i].color),
                   PPM_GETG(xorChv[i].color),
                   PPM_GETB(xorChv[i].color));
      }
    else
      {
         addColorToPalette(palette,i,
                   PPM_GETR(xorChv[i].color) * 255 / xorMaxval,
                   PPM_GETG(xorChv[i].color) * 255 / xorMaxval,
                   PPM_GETB(xorChv[i].color) * 255 / xorMaxval);
      }
     }
   
   /* And make a hash table for fast lookup. */
   xorCht = ppm_colorhisttocolorhash(xorChv, colors);
   ppm_freecolorhist(xorChv);
   
   /*
    * All the icons I found seemed to pad the palette to the max entries
    * for that bitdepth.
    * 
    * The spec indicates this isn't neccessary, but I'll follow this behaviour
    * just in case.
    */
   if (colors < 3) {
      bpp = 1;
      entry_cols = 2;
   } else if (colors < 17) {
      bpp = 4;
      entry_cols = 16;
   } else {
      bpp = 8;
      entry_cols = 256;
   }
   if (!strlen (andPGM)) {
      /* He's not supplying a bitmap for 'and'.  Fake the bitmap. */
      andPGMarray = NULL;
      andCols = xorCols;
      andRows = xorRows;
      andMaxval = 1;
      andCht  = NULL;

   } else {
      andfile = pm_openr(andPGM);
      andPGMarray = pgm_readpgm( andfile, &andCols, &andRows, &andMaxval );
      pm_close(andfile);

      if ((andCols != xorCols) || (andRows != xorRows)) {
          pm_error("%s and %s have different dimensions.\nAborting.\n",
                   xorPPM, andPGM);
      }
   }
   if (verbose) pm_message ("Dimensions of ppms %d, %d\n",xorCols,xorRows);
   xorBitmap = createBitmap(bpp,xorPPMarray,xorCols,xorRows,xorCht);
   andBitmap = createAndBitmap(andPGMarray, andCols, andRows, andMaxval);
   /*
    * Fill in the entry data fields.
    */
   entry->width         = xorCols;
   entry->height        = xorRows;
   entry->color_count   = entry_cols;
   entry->reserved      = 0;
   entry->planes        = 1;
   /* 
    * all the icons I looked at ignored this value...
    */
   entry->bitcount      = bpp;
   entry->ih            = createInfoHeader(entry,xorBitmap,andBitmap);
   entry->colors        = palette->colors;
   overflow2(4, entry->color_count);
   overflow_add(xorBitmap->size, andBitmap->size);
   overflow_add(xorBitmap->size + andBitmap->size, 40);
   overflow_add(xorBitmap->size + andBitmap->size + 40, 4 * entry->color_count);
   entry->size_in_bytes = 
       xorBitmap->size + andBitmap->size + 40 + (4 * entry->color_count);
   if (verbose) 
       pm_message ("entry->size_in_bytes = %d + %d + %d = %d\n",
                   xorBitmap->size ,andBitmap->size, 
                   40, entry->size_in_bytes );
   /*
    * We don't know the offset ATM, set to 0 for now.
    * Have to calculate this at the end.
    */
   entry->file_offset   = 0;
   entry->xorBitmapOut  = xorBitmap->data;
   entry->andBitmapOut  = andBitmap->data;
   entry->xBytesXor     = xorBitmap->xBytes;
   entry->xBytesAnd     = andBitmap->xBytes;  
   /*
    * Add the entry to the entries array.
    */
   overflow_add(MSIconData->count,1);
   MSIconData->count++;
   /* 
    * Perhaps I should use something that allocs a decent amount at start...
    */
   MSIconData->entries = 
       realloc2(MSIconData->entries, MSIconData->count, sizeof(IC_Entry *));
   MSIconData->entries[MSIconData->count-1] = entry;
}

static void 
writeIC_Entry (IC_Entry const entry) {
   writeU1(entry->width);
   writeU1(entry->height);
   writeU1(entry->color_count); /* chops 256->0 on its own.. */
   writeU1(entry->reserved);
   writeU2(entry->planes);
   writeU2(entry->bitcount);
   writeU4(entry->size_in_bytes);
   writeU4(entry->file_offset);
}



static void 
writeIC_InfoHeader (IC_InfoHeader const ih) {
   writeU4(ih->size);
   writeU4(ih->width);
   writeU4(ih->height);
   writeU2(ih->planes);
   writeU2(ih->bitcount);
   writeU4(ih->compression);
   writeU4(ih->imagesize);
   writeU4(ih->x_pixels_per_m);
   writeU4(ih->y_pixels_per_m);
   writeU4(ih->colors_used);
   writeU4(ih->colors_important);
}



static void 
writeIC_Color (IC_Color const col) {
   /* Since the ppm might not have as many colors in it as we'd like,
    * (2, 16, 256), stick 0 in the gaps.
    * 
    * This means that we lose palette information, but that can't be
    * helped.  
    */
   if (col == NULL) {
      writeU1(0);
      writeU1(0);
      writeU1(0);
      writeU1(0);
   } else {
      writeU1(col->blue);
      writeU1(col->green);
      writeU1(col->red);
      writeU1(col->reserved);
   }
}



static void
writeBitmap(u1 ** const bitmap, int const xBytes, int const height) {
   int y;
   for (y = 0;y<height;y++) {
      fwrite (bitmap[y],1,xBytes,ofp);
      file_offset += xBytes;
   }
}



static void 
writeMS_Ico (MS_Ico const MSIconData, char * const outFname) {
   int x,y;
   if (!strcmp(outfname,"-")) {
      ofp = stdout;
   } else if (!(ofp = fopen (outfname,"wb"))) {
      pm_error ("Failed to open file %s for writing.\n",outfname);
   }

   writeU2(MSIconData->reserved);
   writeU2(MSIconData->type);
   writeU2(MSIconData->count);
   for (x=0;x<MSIconData->count;x++) writeIC_Entry(MSIconData->entries[x]);
   for (x=0;x<MSIconData->count;x++) {
      writeIC_InfoHeader(MSIconData->entries[x]->ih);
      for (y=0;y<(MSIconData->entries[x]->color_count);y++) {
     writeIC_Color(MSIconData->entries[x]->colors[y]);
      }
      if (verbose) pm_message("writing xor bitmap\n");
      writeBitmap(MSIconData->entries[x]->xorBitmapOut,
                  MSIconData->entries[x]->xBytesXor,
                  MSIconData->entries[x]->height);
      if (verbose) pm_message("writing and bitmap\n");
      writeBitmap(MSIconData->entries[x]->andBitmapOut,
                  MSIconData->entries[x]->xBytesAnd,
                  MSIconData->entries[x]->height);
   }
   fclose (ofp);
}



int 
main(int argc, char ** argv) {
    MS_Ico MSIconData = createIconFile();
    int iconOn = 1;
    int argn;
    int offset;
   
    ppm_init ( &argc, argv);
    /*
     * Parse command line arguments.
     */
    argn = 1;
    while (argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0') {
        if (pm_keymatch(argv[argn], "-verbose", 2))
            verbose++;
        else if (pm_keymatch(argv[argn], "-andpgms", 2) 
                 || pm_keymatch(argv[argn], "-andppms", 2)) {
            readAndMaps++;
        }
        else if (pm_keymatch(argv[argn], "-output", 2)) {
            if (argc - argn > 1) {
                outfname = argv[argn+1];
                argn++;
            } else {
                pm_error ("-output must be supplied a filename");
            }
        }
        else pm_error("Invalid option '%s'", argv[argn]);
        ++argn;
    }
   
    if (argn < argc) {
        /*
         * If there's cmd line args left over, fine. 
         * Use them later.
         */
    } else {
        ifp = stdin;
        infname = "noname";
        readFromStdin++;
    }

   
    if (readFromStdin) {
        addEntryToIcon(MSIconData, "-", "");
        if (verbose) pm_message ("Added entry from stdin.\n");
    } else {
        /*
         * If we're not using fake and maps, then we skip 1 each time.
         */
        for ( iconOn = argn; 
              iconOn < argc ; 
              iconOn += (readAndMaps ? 2 : 1) ) {
            const char * xorPPM;
            const char * andPPM;
            xorPPM = argv[iconOn];
            andPPM = (readAndMaps ? argv[iconOn+1] : "");
            addEntryToIcon(MSIconData, xorPPM, andPPM);
        }
    }
    /*
     * Now we have to go through and calculate the offsets.
     * The first infoheader starts at 6 + count*16 bytes.
     */
    offset = (MSIconData->count * 16) + 6;
    for ( iconOn = 0; iconOn < MSIconData->count; iconOn++ ) {
        IC_Entry entry = MSIconData->entries[iconOn];
        entry->file_offset = offset;
        /* 
         * Increase the offset by the size of this offset & data.
         * this includes the size of the color data.
         */
        offset += entry->size_in_bytes;
    }
    /*
     * And now, we have to actually SAVE the .ico!
     */
    writeMS_Ico(MSIconData,argv[argc-1]);
    return 0;
}
