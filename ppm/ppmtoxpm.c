/* ppmtoxpm.c - read a portable pixmap and produce a (version 3) X11 pixmap
**
** Copyright (C) 1990 by Mark W. Snitily
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
** This tool was developed for Schlumberger Technologies, ATE Division, and
** with their permission is being made available to the public with the above
** copyright notice and permission notice.
**
** Upgraded to XPM2 by
**   Paul Breslaw, Mecasoft SA, Zurich, Switzerland (paul@mecazh.uu.ch)
**   Thu Nov  8 16:01:17 1990
**
** Upgraded to XPM version 3 by
**   Arnaud Le Hors (lehors@mirsa.inria.fr)
**   Tue Apr 9 1991
**
** Rainer Sinkwitz sinkwitz@ifi.unizh.ch - 21 Nov 91:
**  - Bug fix, should should malloc space for rgbn[j].name+1 in line 441
**    caused segmentation faults
**    
**  - lowercase conversion of RGB names def'ed out,
**    considered harmful.
**
** Michael Pall (pall@rz.uni-karlsruhe.de) - 29 Nov 93:
**  - Use the algorithm from xpm-lib for pixel encoding
**    (base 93 not base 28 -> saves a lot of space for colorful xpms)
*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "ppm.h"
#include "ppmcmap.h"

/* Max number of entries we will put in the XPM color map 
   Don't forget the one entry for transparency.

   We don't use this anymore.  Ppmtoxpm now has no arbitrary limit on
   the number of colors.

   We're assuming this isn't in fact an XPM format requirement, because
   we've seen it work with 257, and 257 seems to be common, because it's
   the classic 256 colors, plus transparency.  The value was 256 for
   ages before we added transparency capability to this program in May
   2001.  At that time, we started failing with 256 color images.
   Some limit was also necessary before then because ppm_computecolorhash()
   required us to specify a maximum number of colors.  It doesn't anymore.

   If we find out that some XPM processing programs choke on more than
   256 colors, we'll have to readdress this issue.  - Bryan.  2001.05.13.
*/
#define MAXCOLORS    256

/* Max number of rgb mnemonics allowed in rgb text file. */
#define MAX_RGBNAMES 1024

#define MAXPRINTABLE 92         /* number of printable ascii chars
                     * minus \ and " for string compat
                     * and ? to avoid ANSI trigraphs. */

static char *printable =
" .XoO+@#$%&*=-;:>,<1234567890qwertyuipasdfghjklzxcvbnmMNBVCZ\
ASDFGHJKLPIUYTREWQ!~^/()_`'][{}|";


#define max(a,b) ((a) > (b) ? (a) : (b))

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filespec;  /* Filespecs of input files */
    char *name;
    char *rgb;
    char *alpha_filename;
    int verbose;
};

static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions2 on how to parse our options.
         */
    optStruct2 opt;

    unsigned int option_def_index;
    char *nameOpt;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY(0,   "alphamask",   OPT_STRING, &cmdline_p->alpha_filename, 0);
    OPTENTRY(0,   "name",        OPT_STRING, &nameOpt,                   0);
    OPTENTRY(0,   "rgb",         OPT_STRING, &cmdline_p->rgb,            0);

    /* Set the defaults */
    cmdline_p->alpha_filename = NULL;  /* no transparency */
    nameOpt = NULL;      /* no name */
    cmdline_p->rgb = NULL;      /* no rgb file specified */

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We may have parms that are negative numbers */

    pm_optParseOptions2(&argc, argv, opt, 0);
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (argc-1 == 0) 
        cmdline_p->input_filespec = "-";
    else if (argc-1 != 1)
        pm_error("Program takes zero or one argument (filename).  You "
                 "specified %d", argc-1);
    else
        cmdline_p->input_filespec = argv[1];

    /* If output filename not specified, use input filename as default. */
    if (nameOpt)
        cmdline_p->name = nameOpt;
    else if (strcmp(cmdline_p->input_filespec, "-") == 0)
        cmdline_p->name = "noname";
    else {
        static char name[80+1];
        char *cp;

        strncpy(name, cmdline_p->input_filespec, sizeof(name));
        name[sizeof(name)] = '\0';
        cp = strchr(name, '.');
        if (cp)
            *cp = '\0';     /* remove extension */
        cmdline_p->name = name;
    }
}


typedef struct {            /* rgb values and ascii names (from
                     * rgb text file) */
    int r, g, b;            /* rgb values, range of 0 -> 65535 */
    char *name;             /* color mnemonic of rgb value */
}      rgb_names;

typedef struct {            
    /* Information for an XPM color map entry */
    char *cixel;    
        /* XPM color number, as might appear in the XPM raster */
    char *rgbname;  
       /* color name, e.g. "blue" or "#01FF23" */
} cixel_map;



/*---------------------------------------------------------------------------*/
/* This routine reads a rgb text file.  It stores the rgb values (0->65535)
   and the rgb mnemonics (malloc'ed) into the "rgbn" array.  Returns the
   number of entries stored in "rgbn_max". */
static void
read_rgb_names(rgb_fname, rgbn, rgbn_max)
    char *rgb_fname;
    rgb_names rgbn[MAX_RGBNAMES];
    int *rgbn_max;

{
    FILE *rgbf;
    int i, items, red, green, blue;
    char line[512], name[512], *rgbname;

    /* Open the rgb text file.  Abort if error. */
    if ((rgbf = fopen(rgb_fname, "r")) == NULL)
    pm_error("error opening rgb text file \"%s\"", rgb_fname);

    /* Loop reading each line in the file. */
    for (i = 0; fgets(line, sizeof(line), rgbf); i++) {

    /* Quit if rgb text file is too large. */
    if (i == MAX_RGBNAMES) {
        fprintf(stderr,
        "Too many entries in rgb text file, truncated to %d entries.\n",
            MAX_RGBNAMES);
        fflush(stderr);
        break;
    }
    /* Read the line.  Skip if bad. */
    items = sscanf(line, "%d %d %d %[^\n]\n", &red, &green, &blue, name);
    if (items != 4) {
        fprintf(stderr, "rgb text file syntax error on line %d\n", i + 1);
        fflush(stderr);
        i--;
        continue;
    }
    /* Make sure rgb values are within 0->255 range.  Skip if bad. */
    if (red < 0 || red > 0xFF ||
        green < 0 || green > 0xFF ||
        blue < 0 || blue > 0xFF) {
        fprintf(stderr, "rgb value for \"%s\" out of range, ignoring it\n",
            name);
        fflush(stderr);
        i--;
        continue;
    }
    /* Allocate memory for ascii name.  Abort if error. */
    overflow_add(strlen(name), 1);
    if (!(rgbname = (char *) malloc(strlen(name) + 1)))
        pm_error("out of memory allocating rgb name");
        
#ifdef NAMESLOWCASE
    /* Copy string to ascii name and lowercase it. */
    for (n = name, m = rgbname; *n; n++)
        *m++ = isupper(*n) ? tolower(*n) : *n;
    *m = '\0';
#else
    strcpy(rgbname, name);
#endif

    /* Save the rgb values and ascii name in the array. */
    rgbn[i].r = red << 8;
    rgbn[i].g = green << 8;
    rgbn[i].b = blue << 8;
    rgbn[i].name = rgbname;
    }

    /* Return the max number of rgb names. */
    *rgbn_max = i - 1;

    fclose(rgbf);

}                   /* read_rgb_names */



static char *
gen_numstr(int const input, int const digits) {
/*---------------------------------------------------------------------------
   Given a number and a base (MAXPRINTABLE), this routine prints the
   number into a malloc'ed string and returns it.  The length of the
   string is specified by "digits".  The ascii characters of the
   printed number range from printable[0] to printable[MAXPRINTABLE].
   The string is printable[0] filled, (e.g. if printable[0]==0,
   printable[1]==1, MAXPRINTABLE==2, digits==5, i=3, routine would
   return the malloc'ed string "00011").
---------------------------------------------------------------------------*/
    char *str, *p;
    int d;
    int i;

    /* Allocate memory for printed number.  Abort if error. */
    overflow_add(digits, 1);
    if (!(str = (char *) malloc(digits + 1)))
        pm_error("out of memory");

    i = input;
    /* Generate characters starting with least significant digit. */
    p = str + digits;
    *p-- = '\0';            /* nul terminate string */
    while (p >= str) {
        d = i % MAXPRINTABLE;
        i /= MAXPRINTABLE;
        *p-- = printable[d];
    }

    if (i != 0)
        pm_error("Overflow converting %d to %d digits in base %d",
                 input, digits, MAXPRINTABLE);

    return str;
}



static void
gen_cmap(colorhist_vector const chv, int const ncolors, 
         pixval const maxval, int map_rgb_names, 
         rgb_names rgbn[MAX_RGBNAMES], int const rgbn_max,
         cixel_map ** const cmapP, int const charsPerPixel) {
/*----------------------------------------------------------------------------
   Generate the XPM color map in cixel_map format (which is just a step
   away from the actual text that needs to be written the XPM file).

   Output is in newly malloc'ed storage, with its address returned as
   *cmapP.

   This map includes an entry for transparency, whether the raster uses
   it or not.  Its index is ncolors (so cmap's dimension is ncolors +1).
-----------------------------------------------------------------------------*/
    int i, j, mval, red, green, blue, r, g, b, matched;
    char *str;
    cixel_map * cmap;  /* malloc'ed */

    overflow_add(ncolors,1);
    cmap = malloc2(sizeof(cixel_map), (ncolors+1));
    if (cmapP == NULL)
        pm_error("Out of memory allocating %d bytes for a color map.",
                 sizeof(cixel_map) * (ncolors+1));
    /*
     * Determine how many hex digits we'll be normalizing to if the rgb
     * value doesn't match a color mnemonic. 
     */
    mval = (int) maxval;
    if (mval <= 0x000F)
    mval = 0x000F;
    else if (mval <= 0x00FF)
    mval = 0x00FF;
    else if (mval <= 0x0FFF)
    mval = 0x0FFF;
    else
    mval = 0xFFFF;

    /*
     * Generate the character-pixel string and the rgb name for each
     * colormap entry. 
     */
    for (i = 0; i < ncolors; i++) {

    /*
     * The character-pixel string is simply a printed number in base
     * MAXPRINTABLE where the digits of the number range from
     * printable[0] .. printable[MAXPRINTABLE-1] and the printed length
     * of the number is 'charsPerPixel'. 
     */
    cmap[i].cixel = gen_numstr(i, charsPerPixel);

    /* Fetch the rgb value of the current colormap entry. */
    red = PPM_GETR(chv[i].color);
    green = PPM_GETG(chv[i].color);
    blue = PPM_GETB(chv[i].color);

    /*
     * If the ppm color components are not relative to 15, 255, 4095,
     * 65535, normalize the color components here. 
     */
    if (mval != (int) maxval) {
        red = (red * mval) / (int) maxval;
        green = (green * mval) / (int) maxval;
        blue = (blue * mval) / (int) maxval;
    }

    /*
     * If the "-rgb <rgbfile>" option was specified, attempt to map the
     * rgb value to a color mnemonic. 
     */
    if (map_rgb_names) {

        /*
         * The rgb values of the color mnemonics are normalized relative
         * to 255 << 8, (i.e. 0xFF00).  [That's how the original MIT
         * code did it, really should have been "v * 65535 / 255"
         * instead of "v << 8", but have to use the same scheme here or
         * else colors won't match...]  So, if our rgb values aren't
         * already 16-bit values, need to shift left. 
         */
        if (mval == 0x000F) {
        r = red << 12;
        g = green << 12;
        b = blue << 12;
        /* Special case hack for "white". */
        if (0xF000 == r && r == g && g == b)
            r = g = b = 0xFF00;
        } else if (mval == 0x00FF) {
        r = red << 8;
        g = green << 8;
        b = blue << 8;
        } else if (mval == 0x0FFF) {
        r = red << 4;
        g = green << 4;
        b = blue << 4;
        } else {
        r = red;
        g = green;
        b = blue;
        }

        /*
         * Just perform a dumb linear search over the rgb values of the
         * color mnemonics.  One could speed things up by sorting the
         * rgb values and using a binary search, or building a hash
         * table, etc... 
         */
        for (matched = 0, j = 0; j <= rgbn_max; j++)
        if (r == rgbn[j].r && g == rgbn[j].g && b == rgbn[j].b) {

            /* Matched.  Allocate string, copy mnemonic, and exit. */
            overflow_add(strlen(rgbn[j].name), 1);
            if (!(str = (char *) malloc(strlen(rgbn[j].name) + 1)))
            pm_error("out of memory");
            strcpy(str, rgbn[j].name);
            cmap[i].rgbname = str;
            matched = 1;
            break;
        }
        if (matched)
        continue;
    }

    /*
     * Either not mapping to color mnemonics, or didn't find a match.
     * Generate an absolute #RGB value string instead. 
     */
    if (!(str = (char *) malloc(mval == 0x000F ? 5 :
                    mval == 0x00FF ? 8 :
                    mval == 0x0FFF ? 11 :
                    14)))
        pm_error("out of memory");

    sprintf(str, mval == 0x000F ? "#%X%X%X" :
        mval == 0x00FF ? "#%02X%02X%02X" :
        mval == 0x0FFF ? "#%03X%03X%03X" :
        "#%04X%04X%04X", red, green, blue);
    cmap[i].rgbname = str;
    }
    /* Add the special transparency entry to the colormap */
    cmap[ncolors].rgbname = strdup("None");
    cmap[ncolors].cixel = gen_numstr(ncolors, charsPerPixel);

    *cmapP = cmap;
}



static void
destroy_cmap(cixel_map * const cmap, int const ncolors) {

    int i;
    /* Free the real color entries */
    for (i = 0; i < ncolors; i++) {
        free(cmap[i].rgbname);
        free(cmap[i].cixel);
    }
    free(cmap[ncolors].rgbname);  /* The transparency entry */
    free(cmap[ncolors].cixel);  /* The transparency entry */

    free(cmap);
}



static void
writeXpmFile(FILE * const outfile, pixel ** const pixels, 
             gray ** const alpha, pixval const alphamaxval,
             char name[], 
             int const cols, int const rows, int const colormapsize,
             int charsPerPixel, 
             cixel_map cmap[],
             colorhash_table const cht) {
/*----------------------------------------------------------------------------
   Write the whole XPM file to the open stream 'outfile'.

   colormapsize is the total number of entries in the colormap -- one for
   each color, plus one for transparency.
-----------------------------------------------------------------------------*/
    /* First the header */
    printf("/* XPM */\n");
    fprintf(outfile, "static char *%s[] = {\n", name);
    fprintf(outfile, "/* width height ncolors chars_per_pixel */\n");
    fprintf(outfile, "\"%d %d %d %d\",\n", cols, rows, 
            colormapsize, charsPerPixel);

    {
        int i;
        /* Write out the colormap (part of header) */
        fprintf(outfile, "/* colors */\n");
        for (i = 0; i < colormapsize; i++) { 
            fprintf(outfile, "\"%s c %s\",\n", cmap[i].cixel, cmap[i].rgbname);
        }
    }
    {
        int row;

        /* And now the raster */
        fprintf(outfile, "/* pixels */\n");
        for (row = 0; row < rows; row++) {
            int col;
            fprintf(outfile, "\"");
            for (col = 0; col < cols; col++) {
                if (alpha && alpha[row][col] <= alphamaxval/2)
                    /* It's a transparent pixel */
                    fprintf(outfile, "%s", cmap[colormapsize-1].cixel);
                else 
                    fprintf(outfile, "%s", 
                            cmap[ppm_lookupcolor(cht, 
                                                 &pixels[row][col])].cixel);
            }
            fprintf(outfile, "\"%s\n", (row == (rows - 1) ? "" : ","));
        }
    }
    /* And close up */
    fprintf(outfile, "};\n");
}



static void
read_alpha(char filespec[], gray *** const alphaP,
           int const cols, int const rows, pixval * const alphamaxvalP) {

    FILE * alpha_file;
    int alphacols, alpharows;
        
    alpha_file = pm_openr(filespec);
    *alphaP = pgm_readpgm(alpha_file, &alphacols, &alpharows, alphamaxvalP);
    pm_close(alpha_file);
    
    if (cols != alphacols || rows != alpharows)
        pm_error("Alpha mask is not the same dimensions as the "
                 "image.  Image is %d by %d, while mask is %d x %d.",
                 cols, rows, alphacols, alpharows);
}
    

static void
compute_colormap(pixel ** const pixels, int const cols, int const rows,
                 colorhist_vector * const chvP, 
                 colorhash_table * const chtP,
                 int * const ncolorsP) {
/*----------------------------------------------------------------------------
   Compute the color map for the image 'pixels', which is 'cols' by 'rows',
   in Netpbm data structures (a colorhist_vector for index-to-color lookups
   and a colorhash_table for color-to-index lookups).
-----------------------------------------------------------------------------*/
    colorhash_table hist_cht;

    pm_message("(Computing colormap...");
    hist_cht = ppm_computecolorhash(pixels, cols, rows, 0, ncolorsP);
    /* Old versions of the library don't have the special 0 value for 
       maxcolors -- they take it literally.
    */
    if (hist_cht == NULL)
        pm_error("You need a newer version of the Netpbm library 'libppm' "
                 "to run this program.");
    pm_message("...Done.  %d colors found.)\n", *ncolorsP);

    *chvP = ppm_colorhashtocolorhist(hist_cht, *ncolorsP);
    ppm_freecolorhash(hist_cht);
    /* Despite the name, the following generates an index on cht */
    *chtP = ppm_colorhisttocolorhash(*chvP, *ncolorsP);
}



int
main(int argc, char *argv[]) {

    FILE *ifp;
    int rows, cols, ncolors;
    pixval maxval, alphamaxval;
    colorhash_table cht;
    colorhist_vector chv;

    pixel **pixels;
    gray **alpha;

    /* Used for rgb value -> rgb mnemonic mapping */
    rgb_names rgbn[MAX_RGBNAMES];
    int rgbn_max;

    /* Used for rgb value -> character-pixel string mapping */
    cixel_map *cmap;  /* malloc'ed */
    int charsPerPixel;  

    struct cmdline_info cmdline;

    ppm_init(&argc, argv);

    parse_command_line(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.input_filespec);
    pixels = ppm_readppm(ifp, &cols, &rows, &maxval);
    pm_close(ifp);

    if (cmdline.alpha_filename) 
        read_alpha(cmdline.alpha_filename, &alpha, cols, rows, &alphamaxval);
    else
        alpha = NULL;

    compute_colormap(pixels, cols, rows, &chv, &cht, &ncolors);
    /*
     * If a rgb text file was specified, read in the rgb mnemonics. Does not
     * return if fatal error occurs. 
     */
    if (cmdline.rgb)
        read_rgb_names(cmdline.rgb, rgbn, &rgbn_max);

    { 
        /* Compute characters per pixel in the XPM file */
        int j;
        for (charsPerPixel = 0, j = ncolors; j; charsPerPixel++)
            j /= MAXPRINTABLE;
    }
    /* Now generate the character-pixel colormap table. */
    gen_cmap(chv, ncolors, maxval, cmdline.rgb != NULL, rgbn, rgbn_max,
             &cmap, charsPerPixel);

    writeXpmFile(stdout, pixels, alpha, alphamaxval,
                 cmdline.name, cols, rows, ncolors + 1, 
                 charsPerPixel, cmap, cht);
    
    destroy_cmap(cmap, ncolors);
    ppm_freearray(pixels, rows);
    if (alpha) pgm_freearray(alpha, rows);

    return 0;
}

