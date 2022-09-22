/* ppmdither.c - Ordered dithering of a color ppm file to a specified number
**               of primary shades.
**
** Copyright (C) 1991 by Christos Zoulas.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "ppm.h"

/* Besides having to have enough memory available, the limiting factor
   in the dithering matrix power is the size of the dithering value.
   We need 2*dith_power bits in an unsigned int.  We also reserve
   one bit to give headroom to do calculations with these numbers.
*/
#define MAX_DITH_POWER ((sizeof(unsigned int)*8 - 1) / 2)

typedef unsigned char ubyte;

static unsigned int dith_power;     /* base 2 log of dither matrix dimension */
static unsigned int dith_dim;      	/* dimension of the dither matrix	*/
static unsigned int dith_dm2;      	/* dith_dim squared				*/
static unsigned int **dith_mat; 	/* the dithering matrix			*/
static int debug;

/* COLOR():
 *	returns the index in the color table for the
 *      r, g, b values specified.
 */
#define COLOR(r,g,b) (((r) * dith_ng + (g)) * dith_nb + (b))

/* LEVELS():
 * The number of shades we can represent by dithering of a primary whose
 * individual pixels can express (s) shades of that primary.
 */
#define LEVELS(s)     (dith_dm2 * ((s) - 1) + 1)

/* DITHER():
 *	Returns the dithered color for a single primary.
 *      p = the primary value for the input pixel
 *      m = the maxval on which 'p' is based
 *      d = entry in the dither matrix
 *      s = the number of shades of this primary we can use in an 
 *          individual output pixel
 */
#define DITHER(p,m,d,s) ((ubyte) ((LEVELS(s) * (p) / (m) + (d)) / dith_dm2))



/* 
 *	Return the value of a dither matrix which is 2**dith_power elements
 *  square at Row x, Column y.
 *	[graphics gems, p. 714]
 */
static unsigned int
dith_value(unsigned int y, unsigned int x, const unsigned int dith_power) { 

    unsigned int d;

    /*
     * Think of d as the density. At every iteration, d is shifted
     * left one and a new bit is put in the low bit based on x and y.
     * If x is odd and y is even, or visa versa, then a bit is shifted in.
     * This generates the checkerboard pattern seen in dithering.
     * This quantity is shifted again and the low bit of y is added in.
     * This whole thing interleaves a checkerboard pattern and y's bits
     * which is what you want.
     */
    int i;
    for (i = 0, d = 0; i < dith_power; i++, x >>= 1, y >>= 1)
        d = (d << 2) | (((x & 1) ^ (y & 1)) << 1) | (y & 1);
    return(d);
} /* end dith_value */


/*
 *	Form the dithering matrix for the dimension specified
 *
 *  Note that we assume 'dith_dim' is small enough that the dith_mat_sz
 *  computed within fits in an int.  Otherwise, results are undefined.
 */
static void
dith_matrix(void)
{
    unsigned int x, y, *dat;

    {
        const unsigned int dith_mat_sz = 
            (dith_dim * sizeof(int *)) + /* pointers */
            (dith_dim * dith_dim * sizeof(int)); /* data */

        overflow2(dith_dim, sizeof(int *));
        overflow3(dith_dim, dith_dim, sizeof(int));
        overflow_add(dith_dim * sizeof(int *), dith_dim * dith_dim * sizeof(int));
        dith_mat = (unsigned int **) malloc(dith_mat_sz);

        if (dith_mat == NULL) 
            pm_error("Out of memory.  "
                     "Cannot allocate %d bytes for dithering matrix.",
                     dith_mat_sz);
    }
    dat = (unsigned int *) &dith_mat[dith_dim];
    for (y = 0; y < dith_dim; y++)
        dith_mat[y] = &dat[y * dith_dim];

    for (y = 0; y < dith_dim; y++) {
        for (x = 0; x < dith_dim; x++) {
            dith_mat[y][x] = dith_value(y, x, dith_power);
            if (debug)
                (void) fprintf(stderr, "%4d ", dith_mat[y][x]);
        }
        if (debug)
            (void) fprintf(stderr, "\n");
    }
} /* end dith_matrix */

    
/*        
 *	Setup the dithering parameters, lookup table and dithering matrix
 *
 *  Malloc the lookup table and return its address as *ptab.

 */
static void
dith_setup(const unsigned int dith_power, 
           const unsigned int dith_nr, 
           const unsigned int dith_ng, 
           const unsigned int dith_nb, 
           const pixval output_maxval,
           pixel ** const ptab_p)
{
    unsigned int r, g, b;

    if (dith_nr < 2) 
	pm_error("too few shades for red, minimum of 2");
    if (dith_ng < 2) 
	pm_error("too few shades for green, minimum of 2");
    if (dith_nb < 2) 
	pm_error("too few shades for blue, minimum of 2");

    overflow2(dith_nr, dith_ng);
    *ptab_p = malloc3(dith_nr * dith_ng, dith_nb, sizeof(pixel));

    if (*ptab_p == NULL) 
        pm_error("Unable to allocate space for the color lookup table "
                 "(%d by %d by %d pixels).", dith_nr, dith_ng, dith_nb);
    
    for (r = 0; r < dith_nr; r++) 
        for (g = 0; g < dith_ng; g++) 
            for (b = 0; b < dith_nb; b++) {
                PPM_ASSIGN((*ptab_p)[COLOR(r,g,b)], 
                           (r * output_maxval / (dith_nr - 1)),
                           (g * output_maxval / (dith_ng - 1)),
                           (b * output_maxval / (dith_nb - 1)));
            }
    
    if (dith_power > MAX_DITH_POWER) {
        pm_error("Dithering matrix power %d is too large.  Must be <= %d",
                 dith_power, MAX_DITH_POWER);
    } else {
        dith_dim = (1 << dith_power);
        dith_dm2 = dith_dim * dith_dim;
    }

    dith_matrix();
} /* end dith_setup */


/* 
 *  Dither whole image
 */
static void
dith_dither(const unsigned int width, const unsigned int height, 
            const pixval maxval,
            const pixel * const ptab, 
            pixel ** const input, pixel ** const output,
            const unsigned int dith_nr,
            const unsigned int dith_ng,
            const unsigned int dith_nb, 
            const pixval output_maxval
            ) {

    const unsigned int dm = (dith_dim - 1);  /* A mask */
    unsigned int row, col; 

    for (row = 0; row < height; row++)
        for (col = 0; col < width; col++) {
            const unsigned int d = dith_mat[row & dm][(width-col-1) & dm];
            const pixel input_pixel = input[row][col];
            output[row][col] = 
                ptab[COLOR(DITHER(PPM_GETR(input_pixel), maxval, d, dith_nr), 
                           DITHER(PPM_GETG(input_pixel), maxval, d, dith_ng), 
                           DITHER(PPM_GETB(input_pixel), maxval, d, dith_nb))];

        }
}


int
main( argc, argv )
    int argc;
    char* argv[];
    {
    FILE* ifp;
    pixel *ptab;    /* malloc'd */
    pixel **ipixels;        /* Input image */
    pixel **opixels;        /* Output image */
    int cols, rows;
    pixval maxval;  /* Maxval of the input image */
    pixval output_maxval;  /* Maxval in the dithered output image */
    unsigned int argn;
    char* usage = 
	"[-dim <num>] [-red <num>] [-green <num>] [-blue <num>] [ppmfile]";
    unsigned int dith_nr; /* number of red shades in output */
    unsigned int dith_ng; /* number of green shades	in output */
    unsigned int dith_nb; /* number of blue shades in output */


    ppm_init( &argc, argv );

    dith_nr = 5;  /* default */
    dith_ng = 9;  /* default */
    dith_nb = 5;  /* default */

    dith_power = 4;  /* default */
    debug = 0; /* default */
    argn = 1;

    while ( argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0' )
	{
	if ( pm_keymatch( argv[argn], "-dim", 1) &&  argn + 1 < argc ) {
	    argn++;
	    if (sscanf(argv[argn], "%u", &dith_power) != 1)
		pm_usage( usage );
	}
	else if ( pm_keymatch( argv[argn], "-red", 1 ) && argn + 1 < argc ) {
	    argn++;
	    if (sscanf(argv[argn], "%u", &dith_nr) != 1)
		pm_usage( usage );
	}
	else if ( pm_keymatch( argv[argn], "-green", 1 ) && argn + 1 < argc ) {
	    argn++;
	    if (sscanf(argv[argn], "%u", &dith_ng) != 1)
		pm_usage( usage );
	}
	else if ( pm_keymatch( argv[argn], "-blue", 1 ) && argn + 1 < argc ) {
	    argn++;
	    if (sscanf(argv[argn], "%u", &dith_nb) != 1)
		pm_usage( usage );
	}
	else if ( pm_keymatch( argv[argn], "-debug", 6 )) {
        debug = 1;
	}
	else
	    pm_usage( usage );
	++argn;
	}

    if ( argn != argc )
	{
	ifp = pm_openr( argv[argn] );
	++argn;
	}
    else
	ifp = stdin;

    if ( argn != argc )
	pm_usage( usage );

    ipixels = ppm_readppm( ifp, &cols, &rows, &maxval );
    pm_close( ifp );
    opixels = ppm_allocarray(cols, rows);
    output_maxval = pm_lcm(dith_nr-1, dith_ng-1, dith_nb-1, PPM_MAXMAXVAL);
    dith_setup(dith_power, dith_nr, dith_ng, dith_nb, output_maxval, &ptab);
    dith_dither(cols, rows, maxval, ptab, ipixels, opixels,
                dith_nr, dith_ng, dith_nb, output_maxval);
    ppm_writeppm(stdout, opixels, cols, rows, output_maxval, 0);
    pm_close(stdout);
    exit(0);
}
