/*
 * This is derived from the file of the same name dated June 5, 1995,
 * copied from the Army High Performance Computing Research Center's
 * media-tools.tar.gz package, received from 
 * http://www.arc.umn.edu/gvl-software/media-tools.tar.gz on 2000.04.13.
 *
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is
 * preserved on all copies.
 *
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 *
 *  2002-12-19: Fix maths wrapping bugs. Alan Cox <alan@redhat.com>
 */
/*
 * rletopnm - A conversion program to convert from Utah's "rle" image format
 *            to pbmplus ppm or pgm image formats.
 *
 * Author:      Wes Barris (wes@msc.edu)
 *              AHPCRC
 *              Minnesota Supercomputer Center, Inc.
 * Date:        March 30, 1994
 * Copyright (c) Minnesota Supercomputer Center 1994
 * 
 * 2000.04.13 adapted for Netpbm by Bryan Henderson.  Quieted compiler 
 *            warnings.  Added --alpha option.  Accept input on stdin
 *
 */

#define _BSD_SOURCE 1
    /* Make sure strdup() is in string.h */

/*-----------------------------------------------------------------------------
 * System includes.
 */
#include <string.h>
#include <stdio.h>
#define NO_DECLARE_MALLOC
#include <rle.h>
#include "pnm.h"
#include "shhopt.h"

#define VPRINTF if (cmdline.verbose || cmdline.headerdump) fprintf
#define GRAYSCALE   001	/* 8 bits, no colormap */
#define PSEUDOCOLOR 010	/* 8 bits, colormap */
#define TRUECOLOR   011	/* 24 bits, colormap */
#define DIRECTCOLOR 100	/* 24 bits, no colormap */
#define RLE_MAXVAL 255
/*
 * Utah type declarations.
 */
rle_hdr		hdr;
rle_map		*colormap;


static struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filename;
    int headerdump;
    int verbose;
    int plain;
    char *alpha_filename;
    int alpha_stdout;
} cmdline;


static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p);


/*
 * Other declarations.
 */
int		visual, maplen;
int		width, height;
/*-----------------------------------------------------------------------------
 *                                                         Read the rle header.
 */
static void 
read_rle_header(FILE *ifp)
{
   int	i;
   hdr.rle_file = ifp;
   rle_get_setup(&hdr);
   width = hdr.xmax - hdr.xmin + 1;
   height = hdr.ymax - hdr.ymin + 1;
   VPRINTF(stderr, "Image size: %dx%d\n", width, height);
   if (hdr.ncolors == 1 && hdr.ncmap == 3) {
      visual = PSEUDOCOLOR;
      colormap = hdr.cmap;
      maplen = (1 << hdr.cmaplen);
      VPRINTF(stderr, "Mapped color image with a map of length %d.\n", maplen);
      }
   else if (hdr.ncolors == 3 && hdr.ncmap == 0) {
      visual = DIRECTCOLOR;
      VPRINTF(stderr, "24 bit color image, no colormap.\n");
      }
   else if (hdr.ncolors == 3 && hdr.ncmap == 3) {
      visual = TRUECOLOR;
      colormap = hdr.cmap;
      maplen = (1 << hdr.cmaplen);
      VPRINTF(stderr, 
              "24 bit color image with color map of length %d\n" ,maplen);
      }
   else if (hdr.ncolors == 1 && hdr.ncmap == 0) {
      visual = GRAYSCALE;
      VPRINTF(stderr, "Grayscale image.\n");
      }
   else {
      fprintf(stderr,
              "ncolors = %d, ncmap = %d, I don't know how to handle this!\n",
              hdr.ncolors, hdr.ncmap);
      exit(-1);
      }
   if (hdr.alpha == 0) {
      VPRINTF(stderr, "No alpha channel.\n");
   } else if (hdr.alpha == 1) {
      VPRINTF(stderr, "Alpha channel exists!\n");
   } else {
      fprintf(stderr, "alpha = %d, I don't know how to handle this!\n",
              hdr.alpha);
      exit(-1);
      }
   switch (hdr.background) {
      case 0:
         VPRINTF(stderr, "Use all pixels, ignore background color.");
         break;
      case 1:
         VPRINTF(stderr,
                  "Use only non-background pixels, ignore background color.");
         break;
      case 2:
         VPRINTF(stderr,
        "Use only non-background pixels, clear to background color (default).");
         break;
      default:
         VPRINTF(stderr, "Unknown background flag!\n");
         break;
      }
   if (hdr.background == 2)
      for (i = 0; i < hdr.ncolors; i++)
         VPRINTF(stderr, " %d", hdr.bg_color[i]);
   if (hdr.ncolors == 1 && hdr.ncmap == 3) {
      VPRINTF(stderr, " (%d %d %d)\n",
              hdr.cmap[hdr.bg_color[0]]>>8,
              hdr.cmap[hdr.bg_color[0]+256]>>8,
              hdr.cmap[hdr.bg_color[0]+512]>>8);
      }
   else if (hdr.ncolors == 3 && hdr.ncmap == 3) {
      VPRINTF(stderr, " (%d %d %d)\n",
              hdr.cmap[hdr.bg_color[0]]>>8,
              hdr.cmap[hdr.bg_color[1]+256]>>8,
              hdr.cmap[hdr.bg_color[2]+512]>>8);
      }
   else
      VPRINTF(stderr, "\n");
   if (hdr.comments)
      for (i = 0; hdr.comments[i] != NULL; i++)
         VPRINTF(stderr, "%s\n", hdr.comments[i]);
}
/*-----------------------------------------------------------------------------
 *                                                    Write the ppm image data.
 */
static void 
write_ppm_data(FILE *imageout_file, FILE *alpha_file)
{
   rle_pixel ***scanlines, **scanline;
   pixval r, g, b;
   pixel *pixelrow;
   gray *alpharow;
   
   int scan, x, y;
/*
 *  Allocate some stuff.
 */
   pixelrow = ppm_allocrow(width);
   alpharow = pgm_allocrow(width);

   scanlines = (rle_pixel ***)malloc2( height,  sizeof(rle_pixel **) );
   RLE_CHECK_ALLOC( hdr.cmd, scanlines, "scanline pointers" );

   for ( scan = 0; scan < height; scan++ )
      RLE_CHECK_ALLOC( hdr.cmd, (rle_row_alloc(&hdr, &scanlines[scan]) >= 0),
		       "pixel memory" );
/*
 * Loop through those scan lines.
 */
   for (scan = 0; scan < height; scan++)
      y = rle_getrow(&hdr, scanlines[height - scan - 1]);
   for (scan = 0; scan < height; scan++) {
      scanline = scanlines[scan];
      switch (visual) {
         case GRAYSCALE:	/* 8 bits without colormap */
	    for (x = 0; x < width; x++) {
	       r = scanline[0][x];
	       g = scanline[0][x];
	       b = scanline[0][x];
	       PPM_ASSIGN(pixelrow[x], r, g, b);
           if (hdr.alpha)
               alpharow[x] = scanline[-1][x];
           else 
               alpharow[x] = 0;
	    }
	    break;
         case TRUECOLOR:	/* 24 bits with colormap */
	    for (x = 0; x < width; x++) {
	       r = colormap[scanline[0][x]]>>8;
	       g = colormap[scanline[1][x]+256]>>8;
	       b = colormap[scanline[2][x]+512]>>8;
	       PPM_ASSIGN(pixelrow[x], r, g, b);
           if (hdr.alpha) 
               alpharow[x] = colormap[scanline[-1][x]];
           else
               alpharow[x] = 0;
	    }
	    break;
         case DIRECTCOLOR:	/* 24 bits without colormap */
	    for (x = 0; x < width; x++) {
	       r = scanline[0][x];
	       g = scanline[1][x];
	       b = scanline[2][x];
	       PPM_ASSIGN(pixelrow[x], r, g, b);
           if (hdr.alpha)
               alpharow[x] = scanline[-1][x];
           else
               alpharow[x] = 0;
	    }
	    break;
         case PSEUDOCOLOR:	/* 8 bits with colormap */
	    for (x = 0; x < width; x++) {
	       r = colormap[scanline[0][x]]>>8;
	       g = colormap[scanline[0][x]+256]>>8;
	       b = colormap[scanline[0][x]+512]>>8;
	       PPM_ASSIGN(pixelrow[x], r, g, b);
           if (hdr.alpha) 
               alpharow[x] = colormap[scanline[-1][x]];
           else
               alpharow[x] = 0;
	    }
	    break;
         default:
	    break;
      }
      /*
       * Write the scan line.
       */
    if (imageout_file ) 
        ppm_writeppmrow(imageout_file, pixelrow, width, RLE_MAXVAL, 
                        cmdline.plain);
    if (alpha_file)
        pgm_writepgmrow(alpha_file, alpharow, width, RLE_MAXVAL, 
                        cmdline.plain);
   }				/* end of for scan = 0 to height */

   /* Free scanline memory. */
   for ( scan = 0; scan < height; scan++ )
      rle_row_free( &hdr, scanlines[scan] );
   free( scanlines );
   ppm_freerow(pixelrow);
   pgm_freerow(alpharow);
}
/*-----------------------------------------------------------------------------
 *                                                    Write the pgm image data.
 */
static void 
write_pgm_data(FILE *imageout_file, FILE *alpha_file)
{
   rle_pixel		***scanlines, **scanline;
   gray *pixelrow;
   gray *alpharow;
   int		scan, x, y;
/*
 *  Allocate some stuff.
 */
   pixelrow = pgm_allocrow(width);
   alpharow = pgm_allocrow(width);

   scanlines = (rle_pixel ***)malloc2( height, sizeof(rle_pixel **) );
   RLE_CHECK_ALLOC( hdr.cmd, scanlines, "scanline pointers" );

   for ( scan = 0; scan < height; scan++ )
      RLE_CHECK_ALLOC( hdr.cmd, (rle_row_alloc(&hdr, &scanlines[scan]) >= 0),
		       "pixel memory" );
/*
 * Loop through those scan lines.
 */
   for (scan = 0; scan < height; scan++)
      y = rle_getrow(&hdr, scanlines[height - scan - 1]);
   for (scan = 0; scan < height; scan++) {
      scanline = scanlines[scan];
      for (x = 0; x < width; x++) {
         pixelrow[x] = scanline[0][x];
         if (hdr.alpha) 
             alpharow[x] = scanline[1][x];
         else
             alpharow[x] = 0;
      }
    if (imageout_file) 
        pgm_writepgmrow(imageout_file, pixelrow, width, RLE_MAXVAL, 
                        cmdline.plain);
    if (alpha_file)
        pgm_writepgmrow(alpha_file, alpharow, width, RLE_MAXVAL, 
                        cmdline.plain);
   }				/* end of for scan = 0 to height */

   /* Free scanline memory. */
   for ( scan = 0; scan < height; scan++ )
      rle_row_free( &hdr, scanlines[scan] );
   free( scanlines );
   pgm_freerow(pixelrow);
   pgm_freerow(alpharow);
}
/*-----------------------------------------------------------------------------
 *                               Convert a Utah rle file to a pbmplus pnm file.
 */
int
main(argc, argv)
int argc;
char **argv;
{
    FILE		*ifp;
    FILE *imageout_file, *alpha_file;
    char *fname = NULL;

    pnm_init( &argc, argv );


    parse_command_line(argc, argv, &cmdline);

    if ( cmdline.input_filename != NULL ) 
        ifp = pm_openr( cmdline.input_filename );
    else
        ifp = stdin;

    if (cmdline.alpha_stdout)
        alpha_file = stdout;
    else if (cmdline.alpha_filename == NULL) 
        alpha_file = NULL;
    else {
        alpha_file = pm_openw(cmdline.alpha_filename);
    }

    if (cmdline.alpha_stdout) 
        imageout_file = NULL;
    else
        imageout_file = stdout;


/*
 * Open the file.
 */
   /* Initialize header. */
   hdr = *rle_hdr_init( (rle_hdr *)NULL );
   rle_names( &hdr, cmd_name( argv ), fname, 0 );

/*
 * Read the rle file header.
 */
   read_rle_header(ifp);
   if (cmdline.headerdump)
      exit(0);

   /* 
    * Write the alpha file header
    */
   if (alpha_file)
       pgm_writepgminit(alpha_file, width, height, RLE_MAXVAL, cmdline.plain);

   /*
    * Write the pnm file header.
    */
   switch (visual) {
      case GRAYSCALE:	/* 8 bits without colormap -> pgm */
         VPRINTF(stderr, "Writing pgm file.\n");
         if (imageout_file)
             pgm_writepgminit(imageout_file, width, height, RLE_MAXVAL, 
                              cmdline.plain);
         write_pgm_data(imageout_file, alpha_file);
         break;
      default:		/* anything else -> ppm */
         VPRINTF(stderr, "Writing ppm file.\n");
         if (imageout_file)
             ppm_writeppminit(imageout_file, width, height, RLE_MAXVAL, 
                              cmdline.plain);
         write_ppm_data(imageout_file, alpha_file);
	    break;
      }
   
   pm_close(ifp);
   if (imageout_file) 
       pm_close( imageout_file );
   if (alpha_file)
       pm_close( alpha_file );

   return 0;
}



static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that many of the strings that this function returns in the
   *cmdline_p structure are actually in the supplied argv array.  And
   sometimes, one of these strings is actually just a suffix of an entry
   in argv!
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions on how to parse our options.
         */
    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY('h', "headerdump", OPT_FLAG,   &cmdline_p->headerdump,     0);
    OPTENTRY('v', "verbose",    OPT_FLAG,   &cmdline_p->verbose,        0);
    OPTENTRY('p', "plain",      OPT_FLAG,   &cmdline_p->plain,          0);
    OPTENTRY(0,   "alphaout",   OPT_STRING, &cmdline_p->alpha_filename, 0);

    /* Set the defaults */
    cmdline_p->headerdump = 0;
    cmdline_p->verbose = 0;
    cmdline_p->plain = 0;
    cmdline_p->alpha_filename = NULL;

    pm_optParseOptions(&argc, argv, option_def, 0);
        /* Uses and sets argc, argv, and all of *cmdline_p. */

    if (argc - 1 == 0)
        cmdline_p->input_filename = NULL;  /* he wants stdin */
    else if (argc - 1 == 1) {
        if (strcmp(argv[1], "-") == 0)
            cmdline_p->input_filename = NULL;  /* he wants stdin */
        else 
            cmdline_p->input_filename = strdup(argv[1]);
    } else 
        pm_error("Too many arguments.  The only argument accepted\n"
                 "is the input file specificaton");

    if (cmdline_p->alpha_filename && 
        strcmp(cmdline_p->alpha_filename, "-") == 0 )
        cmdline_p->alpha_stdout = 1;
    else 
        cmdline_p->alpha_stdout = 0;
}

