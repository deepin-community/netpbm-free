/*

	       Include text labels in a portable pixmap

	       by John Walker  --  kelvin@fourmilab.ch
	       WWW home page: http://www.fourmilab.ch/
			      June 1995
*/

#include <string.h>
#include <math.h>

#include "ppm.h"
#include "ppmdraw.h"

#define TRUE	1
#define FALSE	0

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#define dtr(x)	(((x) * M_PI) / 180.0)

static int argn, rows, cols, x, y, size, angle, transparent;
static pixel **pixels;
static pixval maxval;
static pixel rgbcolour, backcolour;

/*  DRAWTEXT  --  Draw text at current location and advance to
		  start of next line.  */

static void drawtext(text)
  char *text;
{
    if (!transparent && strlen(text) > 0) {
	char *handle = ppmd_fill_init();
	int left, top, right, bottom,
	    lx, ly,
	    p1x, p1y, p2x, p2y, p3x, p3y, p4x, p4y;
	double sina, cosa;

	ppmd_text_box(size, 0, text,
	    &left, &top, &right, &bottom);

	/* Displacement vector */ 

	lx = right;
	ly = -(top - bottom);

	/* Sine and cosine */

	sina = sin(dtr(angle));
	cosa = cos(dtr(angle));

	/* Rotated extent box corners */

	p1x = (int) ((x + left * cosa + bottom * sina) + 0.5);
	p1y = (int) ((y + bottom * cosa + -left * sina) + 0.5);

#define WERF	ppmd_fill_drawproc, handle

	p2x = (int) (p1x - sina * ly + 0.5);
	p2y = (int) ((p1y - cosa * ly) + 0.5);

	p3x = (int) (p1x + cosa * lx + -sina * ly + 0.5);
	p3y = (int) ((p1y - (cosa * ly + sina * lx)) + 0.5);

	p4x = (int) (p1x + cosa * lx + 0.5);
	p4y = (int) ((p1y - sina * lx) + 0.5);

	ppmd_line(pixels, cols, rows, maxval,
	  p1x, p1y, p2x, p2y,
	  WERF);
	ppmd_line(pixels, cols, rows, maxval,
	  p2x, p2y, p3x, p3y,
	  WERF);
	ppmd_line(pixels, cols, rows, maxval,
	  p3x, p3y, p4x, p4y,
	  WERF);
	ppmd_line(pixels, cols, rows, maxval,
	  p4x, p4y, p1x, p1y,
	  WERF);


	ppmd_fill(pixels, cols, rows, maxval,
		  handle, PPMD_NULLDRAWPROC, (char *) &backcolour);
    }
    ppmd_text(pixels, cols, rows, maxval,
	x, y, size, angle, text,
	PPMD_NULLDRAWPROC, (char *) &rgbcolour);

    /* For convenience, simulate a carriage return to the next line.
       This allows multiple "-text" specifications or multiple lines
       in a -file input to write consecutive lines of text in a
       generally reasonable fashion. */

    x += (int) ((cos(dtr(angle + 270)) * size * 1.75) + 0.5);
    y -= (int) ((sin(dtr(angle + 270)) * size * 1.75) + 0.5);
}

/*  Main program.  */

int main(argc, argv)
  int argc;
  char *argv[];
{
    FILE *ifp;
    char *usage = "[-x <x>] [-y <y>] [-size <size>] [-angle <degrees>]\n\
        [-colo[u]r <colourspec>] [-background transparent|<colourspec>]\n\
        [-text \"<text string>\"] [-file <filename>]\n\
        [ppmfile]\n";

    /* Process standard command line arguments */

    ppm_init(&argc, argv);

    argn = 1;

    /* Check for explicit input file specification,  Note that
       we count on the fact that every command line switch
       takes a single argument.  If this becomes untrue due
       to a change in the future, you'll have to make this
       test smarter. */

    if ((argn != argc) && (argc == 2 || argv[argc - 2][0] != '-')) {
	ifp = pm_openr(argv[argc - 1]);
	argc--;
    } else {
	ifp = stdin;
    }

    /* Load input image */

    pixels = ppm_readppm(ifp, &cols, &rows, &maxval);
    pm_close(ifp);

    /* Set initial defaults */

    x = 0;
    y = rows / 2;
    size = 12;
    angle = 0;
    PPM_ASSIGN(rgbcolour, maxval, maxval, maxval);
    PPM_ASSIGN(backcolour, 0, 0, 0);
    transparent = TRUE;

    while (argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0') {

        if (pm_keymatch(argv[argn], "-angle", 1)) {
	    argn++;
            if ((argn == argc) || (sscanf(argv[argn], "%d", &angle) != 1))
		pm_usage(usage);

        } else if (pm_keymatch(argv[argn], "-background", 1)) {
	    argn++;
            if (strcmp(argv[argn], "transparent") == 0) {
		transparent = TRUE;
	    } else {
		transparent = FALSE;
		backcolour = ppm_parsecolor(argv[argn], maxval);
	    }

        } else if (pm_keymatch(argv[argn], "-color", 1)
                   || pm_keymatch(argv[argn], "-colour", 1)) {
	    argn++;
	    rgbcolour = ppm_parsecolor(argv[argn], maxval);

        } else if (pm_keymatch(argv[argn], "-file", 1)) {
	    char s[512];

	    argn++;
	    ifp = pm_openr(argv[argn]);
	    while (fgets(s, sizeof s, ifp) != NULL) {
                while (s[0] != 0 && s[strlen(s) - 1] < ' ') {
		    s[strlen(s) - 1] = 0;
		}
		drawtext(s);
	    }
	    pm_close(ifp);

        } else if (pm_keymatch(argv[argn], "-size", 1)) {
	    argn++;
            if ((argn == argc) || (sscanf(argv[argn], "%d", &size) != 1))
		pm_usage(usage);
        } else if (pm_keymatch(argv[argn], "-text", 1)) {
	    argn++;
	    drawtext(argv[argn]);

        } else if (pm_keymatch(argv[argn], "-u", 1)) {
		pm_usage(usage);

        } else if (pm_keymatch(argv[argn], "-x", 1)) {
	    argn++;
            if ((argn == argc) || (sscanf(argv[argn], "%d", &x) != 1))
		pm_usage(usage);

        } else if (pm_keymatch(argv[argn], "-y", 1)) {
	    argn++;
            if ((argn == argc) || (sscanf(argv[argn], "%d", &y) != 1))
		pm_usage(usage);

	} else {
	    pm_usage(usage);
	}
	argn++;
    }

    if (argn != argc) { 	      /* Extra bogus arguments ? */
	pm_usage(usage);
    }

    /* Output annotated image */

    ppm_writeppm(stdout, pixels, cols, rows, maxval, 0);

    ppm_freearray(pixels, rows);

    return 0;
}
