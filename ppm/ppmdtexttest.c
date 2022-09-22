/*

                      Test pattern for ppmd_text

               by John Walker  --  kelvin@fourmilab.ch
               WWW home page: http://www.fourmilab.ch/

    This  program  exercises  the  ppmdraw  functions  ppmd_text   and
    ppmd_text_extents,  producing  a  pixmap  containing  various text
    strings illustrating the character set, alignment,  extent  boxes,
    size,  and  rotation.   This  is  not intended to be a part of ppm
    proper, but  just  a  test  driver  for  debugging  the  ppmd_text
    routines.   Note  that  you  can  change  the  image size from the
    default of 512x512 with command line arguments, with everything in
    the  image  being  automatically rescaled.  This lets you evaluate
    generation of text at various resolutions.

    Permission  to  use, copy, modify, and distribute this software and
    its documentation  for  any  purpose  and  without  fee  is  hereby
    granted,  without any conditions or restrictions.  This software is
    provided "as is" without express or implied warranty.

*/

#include "ppm.h"
#include "ppmdraw.h"

#define TRUE    1
#define FALSE   0

#define Maxval  255                   /* Maxval to use in generated pixmaps */

static pixel **pixels;                /* Pixel map */
static int pixcols, pixrows;          /* Pixel map size */
static int sxsize = 512, sysize = 512; /* X, Y size */

/*  Main program. */

int main(argc, argv)
  int argc;
  char *argv[];
{
    int argn, x, y, left, top, right, bottom;
    char *usage = "[-size <s>] [-xsize|-width <x>] [-ysize|-height <y>]";
    int widspec = FALSE, hgtspec = FALSE,
        i, ix;
    pixel rgbcolour;                  /* Pixel used to clear pixmap */
    char tascii[256];

    ppm_init(&argc, argv);
    argn = 1;

    while (argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0') {
        if (pm_keymatch(argv[argn], "-xsize", 1) ||
                   pm_keymatch(argv[argn], "-width", 1)) {
            if (widspec) {
                pm_error("already specified a size/width/xsize");
            }
            argn++;
            if ((argn == argc) || (sscanf(argv[argn], "%d", &sxsize) != 1))
                pm_usage(usage);
            widspec = TRUE;
        } else if (pm_keymatch(argv[argn], "-ysize", 1) ||
                   pm_keymatch(argv[argn], "-height", 1)) {
            if (hgtspec) {
                pm_error("already specified a size/height/ysize");
            }
            argn++;
            if ((argn == argc) || (sscanf(argv[argn], "%d", &sysize) != 1))
                pm_usage(usage);
            hgtspec = TRUE;
        } else if (pm_keymatch(argv[argn], "-size", 2)) {
            if (hgtspec || widspec) {
                pm_error("already specified a size/height/ysize");
            }
            argn++;
            if ((argn == argc) || (sscanf(argv[argn], "%d", &sysize) != 1))
                pm_usage(usage);
            sxsize = sysize;
            hgtspec = widspec = TRUE;
        } else {
            pm_usage(usage);
        }
        argn++;
    }

    if (argn != argc) {               /* Extra bogus arguments ? */
        pm_usage(usage);
    }

    /* Allocate image buffer and clear it to black. */

    pixels = ppm_allocarray(pixcols = sxsize, pixrows = sysize);
    PPM_ASSIGN(rgbcolour, 0, 0, 0);
    ppmd_filledrectangle(pixels, pixcols, pixrows, Maxval, 0, 0,
         pixcols, pixrows, PPMD_NULLDRAWPROC,
         (char *) &rgbcolour);

#define Sz(x) (((x) * min(pixcols, pixrows)) / 512)

    /* Draw baseline to verify alignment. */

    PPM_ASSIGN(rgbcolour, 0, 0, Maxval);
    ppmd_line(pixels, pixcols, pixrows, Maxval,
        0, Sz(24), pixcols - 1, Sz(24),
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);

    /* Draw title. */

    PPM_ASSIGN(rgbcolour, 0, Maxval, 0);
    ppmd_text(pixels, pixcols, pixrows, Maxval,
        Sz(36), Sz(24), Sz(24), 0,
        "ppmd_text Test Pattern",
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);

    /* Show all visible ASCII characters. */

    ix = 0;
    for (i = ' '; i <= '~'; i++) {
        tascii[ix++] = (char) i;
        if (i % 32 == 31) {
            tascii[ix++] = '\n';
        }
        tascii[ix] = 0;
    }
    PPM_ASSIGN(rgbcolour, Maxval, Maxval, Maxval);
    ppmd_text(pixels, pixcols, pixrows, Maxval,
        Sz(16), Sz(56), Sz(12), 0,
        tascii,
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);

    /* Test extents calculation. */

    ppmd_text_box(Sz(14), 0, "Extents box okay?",
        &left, &top, &right, &bottom);
    PPM_ASSIGN(rgbcolour, 0, Maxval, Maxval);
    x = Sz(16);
    y = Sz(120);
    ppmd_line(pixels, pixcols, pixrows, Maxval,
        (x + left) - 1, (y + top) - 1, (x + left) - 1, (y + bottom) + 1,
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);
    ppmd_line(pixels, pixcols, pixrows, Maxval,
        (x + left) - 1, (y + bottom) + 1, (x + right) + 1, (y + bottom) + 1,
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);
    ppmd_line(pixels, pixcols, pixrows, Maxval,
        (x + right) + 1, (y + bottom) + 1, (x + right) + 1, (y + top) - 1,
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);
    ppmd_line(pixels, pixcols, pixrows, Maxval,
        (x + right) + 1, (y + top) - 1, (x + left) - 1, (y + top) - 1,
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);
    fprintf(stderr, "Extents: left = %d top = %d right = %d bottom = %d\n",
        left, top, right, bottom);
    PPM_ASSIGN(rgbcolour, Maxval, 0, 0);
    ppmd_text(pixels, pixcols, pixrows, Maxval,
        Sz(16), Sz(120), Sz(14), 0,
        "Extents box okay?",
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);

    /* Demonstrate justification using extents. */

    PPM_ASSIGN(rgbcolour, Maxval, 0, Maxval);
    y = Sz(150);
    ppmd_text_box(Sz(12), 0, "Left", &left, &top, &right, &bottom);
    ppmd_text(pixels, pixcols, pixrows, Maxval,
        -left, y, Sz(12), 0,
        "Left",
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);
    ppmd_text_box(Sz(12), 0, "Centre", &left, &top, &right, &bottom);
    ppmd_text(pixels, pixcols, pixrows, Maxval,
        (pixcols - (right - left)) / 2, y, Sz(12), 0,
        "Centre",
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);
    ppmd_text_box(Sz(12), 0, "Right", &left, &top, &right, &bottom);
    ppmd_text(pixels, pixcols, pixrows, Maxval,
        pixcols - (right + 1), y, Sz(12), 0,
        "Right",
        PPMD_NULLDRAWPROC, (char *) &rgbcolour);

    /* Draw rotated text test. */

    PPM_ASSIGN(rgbcolour, Maxval, Maxval, 0);
    for (i = 0; i < 360; i += 15) {
        char s[40];

        sprintf(s, "    Rotated %d.", i);
        ppmd_text(pixels, pixcols, pixrows, Maxval,
            pixcols / 2, (2 * pixrows) / 3 , Sz(12), i,
            s,
            PPMD_NULLDRAWPROC, (char *) &rgbcolour);
    }

    ppm_writeppm(stdout, pixels, pixcols, pixrows, Maxval, FALSE);
    return 0;
}
