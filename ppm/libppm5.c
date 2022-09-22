/* libppm5.c - ppm utility library part 5
**
** This library module contains the ppmdraw routines.
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
** 
** The character drawing routines are by John Walker
** Copyright (C) 1994 by John Walker, kelvin@fourmilab.ch
*/

#include "ppm.h"
#include "ppmdraw.h"

extern void *realloc2(void *, int, int);

#define DDA_SCALE 8192

#if __STDC__
void
ppmd_point_drawproc( pixel** pixels, int cols, int rows, pixval maxval, int x, int y, char* clientdata )
#else /*__STDC__*/
void
ppmd_point_drawproc( pixels, cols, rows, maxval, x, y, clientdata )
    pixel** pixels;
    int cols, rows, x, y;
    pixval maxval;
    char* clientdata;
#endif /*__STDC__*/
    {
    if ( x >= 0 && x < cols && y >= 0 && y < rows )
	pixels[y][x] = *( (pixel*) clientdata );
    }


/* Simple fill routine. */

#if __STDC__
void
ppmd_filledrectangle( pixel** pixels, int cols, int rows, pixval maxval, int x, int y, int width, int height, void (*drawprocP)(pixel**, int, int, pixval, int, int, char*), char* clientdata )
#else /*__STDC__*/
void
ppmd_filledrectangle( pixels, cols, rows, maxval, x, y, width, height, drawprocP, clientdata )
    pixel** pixels;
    int cols, rows, x, y, width, height;
    pixval maxval;
    void (*drawprocP)();
    char* clientdata;
#endif /*__STDC__*/
    {
    register int cx, cy, cwidth, cheight, col, row;

    /* Clip. */
    cx = x;
    cy = y;
    cwidth = width;
    cheight = height;
    if ( cx < 0 )
	{
	cx = 0;
	cwidth += x;
	}
    if ( cy < 0 )
	{
	cy = 0;
	cheight += y;
	}
    if ( cx + cwidth > cols )
	cwidth = cols - cx;
    if ( cy + cheight > rows )
	cheight = rows - cy;

    /* Draw. */
    for ( row = cy; row < cy + cheight; ++row )
	for ( col = cx; col < cx + cwidth; ++col )
	    if ( drawprocP == PPMD_NULLDRAWPROC )
		pixels[row][col] = *( (pixel*) clientdata );
	    else
		(*drawprocP)(
		    pixels, cols, rows, maxval, col, row, clientdata );
    }


/* Outline drawing stuff. */

static int ppmd_linetype = PPMD_LINETYPE_NORMAL;

int
ppmd_setlinetype( type )
    int type;
    {
    int old;

    old = ppmd_linetype;
    ppmd_linetype = type;
    return old;
    }

static int ppmd_lineclip = 1;

int
ppmd_setlineclip( clip )
    int clip;
    {
    int old;

    old = ppmd_lineclip;
    ppmd_lineclip = clip;
    return old;
    }

#if __STDC__
void
ppmd_line( pixel** pixels, int cols, int rows, pixval maxval, int x0, int y0, int x1, int y1, void (*drawprocP)(pixel**, int, int, pixval, int, int, char*), char* clientdata )
#else /*__STDC__*/
void
ppmd_line( pixels, cols, rows, maxval, x0, y0, x1, y1, drawprocP, clientdata )
    pixel** pixels;
    int cols, rows, x0, y0, x1, y1;
    pixval maxval;
    void (*drawprocP)();
    char* clientdata;
#endif /*__STDC__*/
    {
    register int cx0, cy0, cx1, cy1;

    /* Special case zero-length lines. */
    if ( x0 == x1 && y0 == y1 )
	{
	if ( ( ! ppmd_lineclip ) ||
	     ( x0 >= 0 && x0 < cols && y0 >= 0 && y0 < rows ) ) {
	    if ( drawprocP == PPMD_NULLDRAWPROC )
            ppmd_point_drawproc(pixels, cols, rows, maxval, x0, y0, 
                                clientdata );
	    else
            (*drawprocP)( pixels, cols, rows, maxval, x0, y0, clientdata );
    }
	return;
	}

    /* Clip. */
    cx0 = x0;
    cy0 = y0;
    cx1 = x1;
    cy1 = y1;
    if ( ppmd_lineclip )
	{
	if ( cx0 < 0 )
	    {
	    if ( cx1 < 0 ) return;
	    cy0 = cy0 + ( cy1 - cy0 ) * ( -cx0 ) / ( cx1 - cx0 );
	    cx0 = 0;
	    }
	else if ( cx0 >= cols )
	    {
	    if ( cx1 >= cols ) return;
	    cy0 = cy0 + ( cy1 - cy0 ) * ( cols - 1 - cx0 ) / ( cx1 - cx0 );
	    cx0 = cols - 1;
	    }
	if ( cy0 < 0 )
	    {
	    if ( cy1 < 0 ) return;
	    cx0 = cx0 + ( cx1 - cx0 ) * ( -cy0 ) / ( cy1 - cy0 );
	    cy0 = 0;
	    }
	else if ( cy0 >= rows )
	    {
	    if ( cy1 >= rows ) return;
	    cx0 = cx0 + ( cx1 - cx0 ) * ( rows - 1 - cy0 ) / ( cy1 - cy0 );
	    cy0 = rows - 1;
	    }
	if ( cx1 < 0 )
	    {
	    cy1 = cy1 + ( cy0 - cy1 ) * ( -cx1 ) / ( cx0 - cx1 );
	    cx1 = 0;
	    }
	else if ( cx1 >= cols )
	    {
	    cy1 = cy1 + ( cy0 - cy1 ) * ( cols - 1 - cx1 ) / ( cx0 - cx1 );
	    cx1 = cols - 1;
	    }
	if ( cy1 < 0 )
	    {
	    cx1 = cx1 + ( cx0 - cx1 ) * ( -cy1 ) / ( cy0 - cy1 );
	    cy1 = 0;
	    }
	else if ( cy1 >= rows )
	    {
	    cx1 = cx1 + ( cx0 - cx1 ) * ( rows - 1 - cy1 ) / ( cy0 - cy1 );
	    cy1 = rows - 1;
	    }

	/* Check again for zero-length lines. */
	if ( cx0 == cx1 && cy0 == cy1 )
	    {
	    if ( drawprocP == PPMD_NULLDRAWPROC )
		ppmd_point_drawproc(
		    pixels, cols, rows, maxval, cx0, cy0, clientdata );
	    else
		(*drawprocP)(
		    pixels, cols, rows, maxval, cx0, cy0, clientdata );
	    return;
	    }
	}

    /* Draw, using a simple DDA. */
    if ( abs( cx1 - cx0 ) > abs( cy1 - cy0 ) )
	{ /* Loop over X domain. */
	register long dy, srow;
	register int dx, col, row, prevrow;

	if ( cx1 > cx0 )
	    dx = 1;
	else
	    dx = -1;
	dy = ( cy1 - cy0 ) * DDA_SCALE / abs( cx1 - cx0 );
	prevrow = row = cy0;
	srow = row * DDA_SCALE + DDA_SCALE / 2;
	col = cx0;
	for ( ; ; )
	    {
	    if ( ppmd_linetype == PPMD_LINETYPE_NODIAGS && row != prevrow )
		{
		if ( drawprocP == PPMD_NULLDRAWPROC )
		    pixels[prevrow][col] = *( (pixel*) clientdata );
		else
		    (*drawprocP)(
		        pixels, cols, rows, maxval, col, prevrow, clientdata );
		prevrow = row;
		}
	    if ( drawprocP == PPMD_NULLDRAWPROC )
		pixels[row][col] = *( (pixel*) clientdata );
	    else
		(*drawprocP)(
		    pixels, cols, rows, maxval, col, row, clientdata );
	    if ( col == cx1 )
		break;
	    srow += dy;
	    row = srow / DDA_SCALE;
	    col += dx;
	    }
	}
    else
	{ /* Loop over Y domain. */
	register long dx, scol;
	register int dy, col, row, prevcol;

	if ( cy1 > cy0 )
	    dy = 1;
	else
	    dy = -1;
	dx = ( cx1 - cx0 ) * DDA_SCALE / abs( cy1 - cy0 );
	row = cy0;
	prevcol = col = cx0;
	scol = col * DDA_SCALE + DDA_SCALE / 2;
	for ( ; ; )
	    {
	    if ( ppmd_linetype == PPMD_LINETYPE_NODIAGS && col != prevcol )
		{
		if ( drawprocP == PPMD_NULLDRAWPROC )
		    pixels[row][prevcol] = *( (pixel*) clientdata );
		else
		    (*drawprocP)(
			pixels, cols, rows, maxval, prevcol, row, clientdata );
		prevcol = col;
		}
	    if ( drawprocP == PPMD_NULLDRAWPROC )
		pixels[row][col] = *( (pixel*) clientdata );
	    else
		(*drawprocP)(
		    pixels, cols, rows, maxval, col, row, clientdata );
	    if ( row == cy1 )
		break;
	    row += dy;
	    scol += dx;
	    col = scol / DDA_SCALE;
	    }
	}
    }

#define SPLINE_THRESH 3
#if __STDC__
void
ppmd_spline3( pixel** pixels, int cols, int rows, pixval maxval, int x0, int y0, int x1, int y1, int x2, int y2, void (*drawprocP)(pixel**, int, int, pixval, int, int, char*), char* clientdata )
#else /*__STDC__*/
void
ppmd_spline3( pixels, cols, rows, maxval, x0, y0, x1, y1, x2, y2, drawprocP, clientdata )
    pixel** pixels;
    int cols, rows, x0, y0, x1, y1, x2, y2;
    pixval maxval;
    void (*drawprocP)();
    char* clientdata;
#endif /*__STDC__*/
    {
    register int xa, ya, xb, yb, xc, yc, xp, yp;

    xa = ( x0 + x1 ) / 2;
    ya = ( y0 + y1 ) / 2;
    xc = ( x1 + x2 ) / 2;
    yc = ( y1 + y2 ) / 2;
    xb = ( xa + xc ) / 2;
    yb = ( ya + yc ) / 2;

    xp = ( x0 + xb ) / 2;
    yp = ( y0 + yb ) / 2;
    if ( abs( xa - xp ) + abs( ya - yp ) > SPLINE_THRESH )
	ppmd_spline3(
	    pixels, cols, rows, maxval, x0, y0, xa, ya, xb, yb, drawprocP,
	    clientdata );
    else
	ppmd_line(
	    pixels, cols, rows, maxval, x0, y0, xb, yb, drawprocP, clientdata );

    xp = ( x2 + xb ) / 2;
    yp = ( y2 + yb ) / 2;
    if ( abs( xc - xp ) + abs( yc - yp ) > SPLINE_THRESH )
	ppmd_spline3(
	    pixels, cols, rows, maxval, xb, yb, xc, yc, x2, y2, drawprocP,
	    clientdata );
    else
	ppmd_line(
	    pixels, cols, rows, maxval, xb, yb, x2, y2, drawprocP, clientdata );
    }

#if __STDC__
void
ppmd_polyspline( pixel** pixels, int cols, int rows, pixval maxval, int x0, int y0, int nc, int* xc, int* yc, int x1, int y1, void (*drawprocP)(pixel**, int, int, pixval, int, int, char*), char* clientdata )
#else /*__STDC__*/
void
ppmd_polyspline( pixels, cols, rows, maxval, x0, y0, nc, xc, yc, x1, y1, drawprocP, clientdata )
    pixel** pixels;
    int cols, rows, x0, y0, nc, x1, y1;
    int* xc;
    int* yc;
    pixval maxval;
    void (*drawprocP)();
    char* clientdata;
#endif /*__STDC__*/
    {
    register int i, x, y, xn, yn;

    x = x0;
    y = y0;
    for ( i = 0; i < nc - 1; ++i )
	{
	xn = ( xc[i] + xc[i + 1] ) / 2;
	yn = ( yc[i] + yc[i + 1] ) / 2;
	ppmd_spline3(
	    pixels, cols, rows, maxval, x, y, xc[i], yc[i], xn, yn, drawprocP,
	    clientdata );
	x = xn;
	y = yn;
	}
    ppmd_spline3(
	pixels, cols, rows, maxval, x, y, xc[nc - 1], yc[nc - 1], x1, y1,
	drawprocP, clientdata );
    }

#if __STDC__
void
ppmd_circle( pixel** pixels, int cols, int rows, pixval maxval, int cx, int cy, int radius, void (*drawprocP)(pixel**, int, int, pixval, int, int, char*), char* clientdata )
#else /*__STDC__*/
void
ppmd_circle( pixels, cols, rows, maxval, cx, cy, radius, drawprocP, clientdata )
    pixel** pixels;
    int cols, rows, cx, cy, radius;
    pixval maxval;
    void (*drawprocP)();
    char* clientdata;
#endif /*__STDC__*/
    {
    register int x0, y0, x, y, prevx, prevy, nopointsyet;
    register long sx, sy, e;

    x0 = x = radius;
    y0 = y = 0;
    sx = x * DDA_SCALE + DDA_SCALE / 2;
    sy = y * DDA_SCALE + DDA_SCALE / 2;
    e = DDA_SCALE / radius;
    if ( drawprocP == PPMD_NULLDRAWPROC )
	pixels[y + cy][x + cx] = *( (pixel*) clientdata );
    else
	(*drawprocP)( pixels, cols, rows, maxval, x + cx, y + cy, clientdata );
    nopointsyet = 1;
    do
	{
	prevx = x;
	prevy = y;
	sx += e * sy / DDA_SCALE;
	sy -= e * sx / DDA_SCALE;
	x = sx / DDA_SCALE;
	y = sy / DDA_SCALE;
	if ( x != prevx || y != prevy )
	    {
	    nopointsyet = 0;
	    if ( drawprocP == PPMD_NULLDRAWPROC )
		pixels[y + cy][x + cx] = *( (pixel*) clientdata );
	    else
		(*drawprocP)(
		    pixels, cols, rows, maxval, x + cx, y + cy, clientdata );
	    }
	}
    while ( nopointsyet || x != x0 || y != y0 );
    }


/* Arbitrary fill stuff. */

typedef struct
    {
    short x;
    short y;
    short edge;
    } coord;
typedef struct
    {
    int n;
    int size;
    int curedge;
    int segstart;
    int ydir;
    int startydir;
    coord* coords;
    } fillobj;

#define SOME 1000

static int oldclip;

char*
ppmd_fill_init( )
    {
    fillobj* fh;

    fh = (fillobj*) malloc( sizeof(fillobj) );
    if ( fh == 0 )
	pm_error( "out of memory allocating a fillhandle" );
    fh->n = 0;
    fh->coords = (coord*) malloc2( SOME , sizeof(coord) );
    if ( fh->coords == 0 )
	pm_error( "out of memory allocating a fillhandle" );
    fh->size = SOME;
    fh->curedge = 0;

    /* Turn off line clipping. */
    oldclip = ppmd_setlineclip( 0 );
    
    return (char*) fh;
    }

#if __STDC__
void
ppmd_fill_drawproc( pixel** pixels, int cols, int rows, pixval maxval, int x, int y, char* clientdata )
#else /*__STDC__*/
void
ppmd_fill_drawproc( pixels, cols, rows, maxval, x, y, clientdata )
    pixel** pixels;
    int cols, rows, x, y;
    pixval maxval;
    char* clientdata;
#endif /*__STDC__*/
    {
    register fillobj* fh;
    register coord* cp;
    register coord* ocp;

    fh = (fillobj*) clientdata;

    if ( fh->n > 0 )
	{
	/* If these are the same coords we saved last time, don't bother. */
	ocp = &(fh->coords[fh->n - 1]);
	if ( x == ocp->x && y == ocp->y )
	    return;
	}

    /* Ok, these are new; check if there's room for two more coords. */
    if ( fh->n + 1 >= fh->size )
	{
	overflow_add(fh->size, SOME);
	fh->size += SOME;
	fh->coords = (coord*) realloc2(
	    (char*) fh->coords, fh->size,  sizeof(coord) );
	if ( fh->coords == 0 )
	    pm_error( "out of memory enlarging a fillhandle" );
	}

    /* Check for extremum and set the edge number. */
    if ( fh->n == 0 )
	{ /* Start first segment. */
	fh->segstart = fh->n;
	fh->ydir = 0;
	fh->startydir = 0;
	}
    else
	{
	register int dx, dy;

	dx = x - ocp->x;
	dy = y - ocp->y;
	if ( dx < -1 || dx > 1 || dy < -1 || dy > 1 )
	    { /* Segment break.  Close off old one. */
	    if ( fh->startydir != 0 && fh->ydir != 0 )
		if ( fh->startydir == fh->ydir )
		    { /* Oops, first edge and last edge are the same.
		      ** Renumber the first edge in the old segment. */
		    register coord* fcp;
		    int oldedge;

		    fcp = &(fh->coords[fh->segstart]);
		    oldedge = fcp->edge;
		    for ( ; fcp->edge == oldedge; ++fcp )
			fcp->edge = ocp->edge;
		    }
	    /* And start new segment. */
	    ++(fh->curedge);
	    fh->segstart = fh->n;
	    fh->ydir = 0;
	    fh->startydir = 0;
	    }
	else
	    { /* Segment continues. */
	    if ( dy != 0 )
		{
		if ( fh->ydir != 0 && fh->ydir != dy )
		    { /* Direction changed.  Insert a fake coord, old
		      ** position but new edge number. */
		    ++(fh->curedge);
		    cp = &(fh->coords[fh->n]);
		    cp->x = ocp->x;
		    cp->y = ocp->y;
		    cp->edge = fh->curedge;
		    ++(fh->n);
		    }
		fh->ydir = dy;
		if ( fh->startydir == 0 )
		    fh->startydir = dy;
		}
	    }
	}

    /* Save this coord. */
    cp = &(fh->coords[fh->n]);
    cp->x = x;
    cp->y = y;
    cp->edge = fh->curedge;
    ++(fh->n);
    }

static int yx_compare ARGS((const void* c1, const void* c2));
static int
yx_compare( const void *c1, const void *c2 )
    {
    if ( ((coord*)c1)->y > ((coord*)c2)->y )
	return 1;
    if ( ((coord*)c1)->y < ((coord*)c2)->y )
	return -1;
    if ( ((coord*)c1)->x > ((coord*)c2)->x )
	return 1;
    if ( ((coord*)c1)->x < ((coord*)c2)->x )
	return -1;
    return 0;
    }

#if __STDC__
void
ppmd_fill( pixel** pixels, int cols, int rows, pixval maxval, char* fillhandle, void (*drawprocP)(pixel**, int, int, pixval, int, int, char*), char* clientdata )
#else /*__STDC__*/
void
ppmd_fill( pixels, cols, rows, maxval, fillhandle, drawprocP, clientdata )
    pixel** pixels;
    int cols, rows;
    pixval maxval;
    char* fillhandle;
    void (*drawprocP)();
    char* clientdata;
#endif /*__STDC__*/
    {
    register fillobj* fh;
    int pedge, eq;
    register int i, leftside, edge, lx, rx, py;
    register coord* cp;

    fh = (fillobj*) fillhandle;

    /* Close off final segment. */
    if ( fh->n > 0 && fh->startydir != 0 && fh->ydir != 0 )
	if ( fh->startydir == fh->ydir )
	    { /* Oops, first edge and last edge are the same. */
	    register coord* fcp;
	    int lastedge, oldedge;

	    lastedge = fh->coords[fh->n - 1].edge;
	    fcp = &(fh->coords[fh->segstart]);
	    oldedge = fcp->edge;
	    for ( ; fcp->edge == oldedge; ++fcp )
		fcp->edge = lastedge;
	    }

    /* Restore clipping now. */
    (void) ppmd_setlineclip( oldclip );

    /* Sort the coords by Y, secondarily by X. */
    qsort( (char*) fh->coords, fh->n, sizeof(coord), yx_compare );

    /* Find equal coords with different edge numbers, and swap if necessary. */
    edge = -1;
    for ( i = 0; i < fh->n; ++i )
	{
	cp = &(fh->coords[i]);
	if ( i > 1 && eq && cp->edge != edge && cp->edge == pedge )
	    { /* Swap .-1 and .-2. */
	    coord t;

	    t = fh->coords[i-1];
	    fh->coords[i-1] = fh->coords[i-2];
	    fh->coords[i-2] = t;
	    }
	if ( i > 0 )
	    {
	    if ( cp->x == lx && cp->y == py )
		{
		eq = 1;
		if ( cp->edge != edge && cp->edge == pedge )
		    { /* Swap . and .-1. */
		    coord t;

		    t = *cp;
		    *cp = fh->coords[i-1];
		    fh->coords[i-1] = t;
		    }
		}
	    else
		eq = 0;
	    }
	lx = cp->x;
	py = cp->y;
	pedge = edge;
	edge = cp->edge;
	}

    /* Ok, now run through the coords filling spans. */
    for ( i = 0; i < fh->n; ++i )
	{
	cp = &(fh->coords[i]);
	if ( i == 0 )
	    {
	    lx = rx = cp->x;
	    py = cp->y;
	    edge = cp->edge;
	    leftside = 1;
	    }
	else
	    {
	    if ( cp->y != py )
		{ /* Row changed.  Emit old span and start a new one. */
		ppmd_filledrectangle(
		    pixels, cols, rows, maxval, lx, py, rx - lx + 1, 1,
		    drawprocP, clientdata);
		lx = rx = cp->x;
		py = cp->y;
		edge = cp->edge;
		leftside = 1;
		}
	    else
		{
		if ( cp->edge == edge )
		    { /* Continuation of side. */
		    rx = cp->x;
		    }
		else
		    { /* Edge changed.  Is it a span? */
		    if ( leftside )
			{
			rx = cp->x;
			leftside = 0;
			}
		    else
			{ /* Got a span to fill. */
			ppmd_filledrectangle(
			    pixels, cols, rows, maxval, lx, py, rx - lx + 1,
			    1, drawprocP, clientdata);
			lx = rx = cp->x;
			leftside = 1;
			}
		    edge = cp->edge;
		    }
		}
	    }
	}

    /* All done.  Free up the fillhandle and leave. */
    free( fh->coords );
    free( fh );
    }



/*		  Stroke character definitions

   The	following  character  definitions are derived from the (public
   domain) Hershey plotter  font  database,  using  the  single-stroke
   Roman font.

   Each  character  definition	begins	with 3 bytes which specify the
   number of X, Y plot pairs which follow, the negative  of  the  skip
   before  starting  to  draw  the  characters, and the skip after the
   character.  The first plot pair moves the pen to that location  and
   subsequent  pairs  draw  to	the  location given.  A pair of 192, 0
   raises the pen, moves to the location given by the following  pair,
   and resumes drawing with the pair after that.

   The  values  in  the  definition  tables are 8-bit two's complement
   signed numbers.  We  declare  the  table  as  "unsigned  char"  and
   manually  sign-extend  the  values because C compilers differ as to
   whether the type "char" is signed or unsigned, and  some  compilers
   don't  accept the qualifier "signed" which we would like to use for
   these items.  We specify negative numbers as their  unsigned  two's
   complements  to  avoid  complaints  from compilers which don't like
   initialising unsigned data with signed values.  Ahhh,  portability.
*/

static unsigned char
char32[] = { 0, 0, 21 },
char33[] = { 8, 251, 5, 0, 244, 0, 2, 192, 0, 0, 7, 255, 8, 0, 9, 1, 8, 0, 7 },
char34[] = { 17, 253, 15, 2, 244, 1, 245, 0, 244, 1, 243, 2, 244, 2, 246, 1,
	     248, 0, 249, 192, 0, 10, 244, 9, 245, 8, 244, 9, 243, 10, 244,
	     10, 246, 9, 248, 8, 249, },
char35[] = { 11, 246, 11, 1, 240, 250, 16, 192, 0, 7, 240, 0, 16, 192, 0, 250,
	     253, 8, 253, 192, 0, 249, 3, 7, 3 },
char36[] = { 26, 246, 10, 254, 240, 254, 13, 192, 0, 2, 240, 2, 13, 192, 0, 7,
	     247, 5, 245, 2, 244, 254, 244, 251, 245, 249, 247, 249, 249, 250,
	     251, 251, 252, 253, 253, 3, 255, 5, 0, 6, 1, 7, 3, 7, 6, 5, 8, 2,
	     9, 254, 9, 251, 8, 249, 6 },
char37[] = { 31, 244, 12, 9, 244, 247, 9, 192, 0, 252, 244, 254, 246, 254,
	     248, 253, 250, 251, 251, 249, 251, 247, 249, 247, 247, 248, 245,
	     250, 244, 252, 244, 254, 245, 1, 246, 4, 246, 7, 245, 9, 244,
	     192, 0, 5, 2, 3, 3, 2, 5, 2, 7, 4, 9, 6, 9, 8, 8, 9, 6, 9, 4, 7,
	     2, 5, 2 },
char38[] = { 34, 243, 13, 10, 253, 10, 252, 9, 251, 8, 251, 7, 252, 6, 254, 4,
	     3, 2, 6, 0, 8, 254, 9, 250, 9, 248, 8, 247, 7, 246, 5, 246, 3,
	     247, 1, 248, 0, 255, 252, 0, 251, 1, 249, 1, 247, 0, 245, 254,
	     244, 252, 245, 251, 247, 251, 249, 252, 252, 254, 255, 3, 6, 5,
	     8, 7, 9, 9, 9, 10, 8, 10, 7 },
char39[] = { 7, 251, 5, 0, 246, 255, 245, 0, 244, 1, 245, 1, 247, 0, 249, 255,
	     250 },
char40[] = { 10, 249, 7, 4, 240, 2, 242, 0, 245, 254, 249, 253, 254, 253, 2,
	     254, 7, 0, 11, 2, 14, 4, 16 },
char41[] = { 10, 249, 7, 252, 240, 254, 242, 0, 245, 2, 249, 3, 254, 3, 2, 2,
	     7, 0, 11, 254, 14, 252, 16 },
char42[] = { 8, 248, 8, 0, 250, 0, 6, 192, 0, 251, 253, 5, 3, 192, 0, 5, 253,
	     251, 3 },
char43[] = { 5, 243, 13, 0, 247, 0, 9, 192, 0, 247, 0, 9, 0 },
char44[] = { 8, 251, 5, 1, 8, 0, 9, 255, 8, 0, 7, 1, 8, 1, 10, 0, 12, 255, 13
	   },
char45[] = { 2, 243, 13, 247, 0, 9, 0 },
char46[] = { 5, 251, 5, 0, 7, 255, 8, 0, 9, 1, 8, 0, 7 },
char47[] = { 2, 245, 11, 9, 240, 247, 16 },
char48[] = { 17, 246, 10, 255, 244, 252, 245, 250, 248, 249, 253, 249, 0, 250,
	     5, 252, 8, 255, 9, 1, 9, 4, 8, 6, 5, 7, 0, 7, 253, 6, 248, 4,
	     245, 1, 244, 255, 244 },
char49[] = { 4, 246, 10, 252, 248, 254, 247, 1, 244, 1, 9 },
char50[] = { 14, 246, 10, 250, 249, 250, 248, 251, 246, 252, 245, 254, 244, 2,
	     244, 4, 245, 5, 246, 6, 248, 6, 250, 5, 252, 3, 255, 249, 9, 7, 9
	   },
char51[] = { 15, 246, 10, 251, 244, 6, 244, 0, 252, 3, 252, 5, 253, 6, 254, 7,
	     1, 7, 3, 6, 6, 4, 8, 1, 9, 254, 9, 251, 8, 250, 7, 249, 5 },
char52[] = { 6, 246, 10, 3, 244, 249, 2, 8, 2, 192, 0, 3, 244, 3, 9 },
char53[] = { 17, 246, 10, 5, 244, 251, 244, 250, 253, 251, 252, 254, 251, 1,
	     251, 4, 252, 6, 254, 7, 1, 7, 3, 6, 6, 4, 8, 1, 9, 254, 9, 251,
	     8, 250, 7, 249, 5 },
char54[] = { 23, 246, 10, 6, 247, 5, 245, 2, 244, 0, 244, 253, 245, 251, 248,
	     250, 253, 250, 2, 251, 6, 253, 8, 0, 9, 1, 9, 4, 8, 6, 6, 7, 3,
	     7, 2, 6, 255, 4, 253, 1, 252, 0, 252, 253, 253, 251, 255, 250, 2
	     },
char55[] = { 5, 246, 10, 7, 244, 253, 9, 192, 0, 249, 244, 7, 244 },
char56[] = { 29, 246, 10, 254, 244, 251, 245, 250, 247, 250, 249, 251, 251,
	     253, 252, 1, 253, 4, 254, 6, 0, 7, 2, 7, 5, 6, 7, 5, 8, 2, 9,
	     254, 9, 251, 8, 250, 7, 249, 5, 249, 2, 250, 0, 252, 254, 255,
	     253, 3, 252, 5, 251, 6, 249, 6, 247, 5, 245, 2, 244, 254, 244 },
char57[] = { 23, 246, 10, 6, 251, 5, 254, 3, 0, 0, 1, 255, 1, 252, 0, 250,
	     254, 249, 251, 249, 250, 250, 247, 252, 245, 255, 244, 0, 244, 3,
	     245, 5, 247, 6, 251, 6, 0, 5, 5, 3, 8, 0, 9, 254, 9, 251, 8, 250,
	     6 },
char58[] = { 11, 251, 5, 0, 251, 255, 252, 0, 253, 1, 252, 0, 251, 192, 0, 0,
	     7, 255, 8, 0, 9, 1, 8, 0, 7 },
char59[] = { 14, 251, 5, 0, 251, 255, 252, 0, 253, 1, 252, 0, 251, 192, 0, 1,
	     8, 0, 9, 255, 8, 0, 7, 1, 8, 1, 10, 0, 12, 255, 13 },
char60[] = { 3, 244, 12, 8, 247, 248, 0, 8, 9 },
char61[] = { 5, 243, 13, 247, 253, 9, 253, 192, 0, 247, 3, 9, 3 },
char62[] = { 3, 244, 12, 248, 247, 8, 0, 248, 9 },
char63[] = { 20, 247, 9, 250, 249, 250, 248, 251, 246, 252, 245, 254, 244, 2,
	     244, 4, 245, 5, 246, 6, 248, 6, 250, 5, 252, 4, 253, 0, 255, 0,
	     2, 192, 0, 0, 7, 255, 8, 0, 9, 1, 8, 0, 7 },
char64[] = { 55, 243, 14, 5, 252, 4, 250, 2, 249, 255, 249, 253, 250, 252,
	     251, 251, 254, 251, 1, 252, 3, 254, 4, 1, 4, 3, 3, 4, 1, 192, 0,
	     255, 249, 253, 251, 252, 254, 252, 1, 253, 3, 254, 4, 192, 0, 5,
	     249, 4, 1, 4, 3, 6, 4, 8, 4, 10, 2, 11, 255, 11, 253, 10, 250, 9,
	     248, 7, 246, 5, 245, 2, 244, 255, 244, 252, 245, 250, 246, 248,
	     248, 247, 250, 246, 253, 246, 0, 247, 3, 248, 5, 250, 7, 252, 8,
	     255, 9, 2, 9, 5, 8, 7, 7, 8, 6, 192, 0, 6, 249, 5, 1, 5, 3, 6, 4
	     },
char65[] = { 8, 247, 9, 0, 244, 248, 9, 192, 0, 0, 244, 8, 9, 192, 0, 251, 2,
	     5, 2 },
char66[] = { 23, 245, 10, 249, 244, 249, 9, 192, 0, 249, 244, 2, 244, 5, 245,
	     6, 246, 7, 248, 7, 250, 6, 252, 5, 253, 2, 254, 192, 0, 249, 254,
	     2, 254, 5, 255, 6, 0, 7, 2, 7, 5, 6, 7, 5, 8, 2, 9, 249, 9 },
char67[] = { 18, 246, 11, 8, 249, 7, 247, 5, 245, 3, 244, 255, 244, 253, 245,
	     251, 247, 250, 249, 249, 252, 249, 1, 250, 4, 251, 6, 253, 8,
	     255, 9, 3, 9, 5, 8, 7, 6, 8, 4 },
char68[] = { 15, 245, 10, 249, 244, 249, 9, 192, 0, 249, 244, 0, 244, 3, 245,
	     5, 247, 6, 249, 7, 252, 7, 1, 6, 4, 5, 6, 3, 8, 0, 9, 249, 9 },
char69[] = { 11, 246, 9, 250, 244, 250, 9, 192, 0, 250, 244, 7, 244, 192, 0,
	     250, 254, 2, 254, 192, 0, 250, 9, 7, 9 },
char70[] = { 8, 246, 8, 250, 244, 250, 9, 192, 0, 250, 244, 7, 244, 192, 0,
	     250, 254, 2, 254 },
char71[] = { 22, 246, 11, 8, 249, 7, 247, 5, 245, 3, 244, 255, 244, 253, 245,
	     251, 247, 250, 249, 249, 252, 249, 1, 250, 4, 251, 6, 253, 8,
	     255, 9, 3, 9, 5, 8, 7, 6, 8, 4, 8, 1, 192, 0, 3, 1, 8, 1 },
char72[] = { 8, 245, 11, 249, 244, 249, 9, 192, 0, 7, 244, 7, 9, 192, 0, 249,
	     254, 7, 254 },
char73[] = { 2, 252, 4, 0, 244, 0, 9 },
char74[] = { 10, 248, 8, 4, 244, 4, 4, 3, 7, 2, 8, 0, 9, 254, 9, 252, 8, 251,
	     7, 250, 4, 250, 2 },
char75[] = { 8, 245, 10, 249, 244, 249, 9, 192, 0, 7, 244, 249, 2, 192, 0,
	     254, 253, 7, 9 },
char76[] = { 5, 246, 7, 250, 244, 250, 9, 192, 0, 250, 9, 6, 9 },
char77[] = { 11, 244, 12, 248, 244, 248, 9, 192, 0, 248, 244, 0, 9, 192, 0, 8,
	     244, 0, 9, 192, 0, 8, 244, 8, 9 },
char78[] = { 8, 245, 11, 249, 244, 249, 9, 192, 0, 249, 244, 7, 9, 192, 0, 7,
	     244, 7, 9 },
char79[] = { 21, 245, 11, 254, 244, 252, 245, 250, 247, 249, 249, 248, 252,
	     248, 1, 249, 4, 250, 6, 252, 8, 254, 9, 2, 9, 4, 8, 6, 6, 7, 4,
	     8, 1, 8, 252, 7, 249, 6, 247, 4, 245, 2, 244, 254, 244 },
char80[] = { 13, 245, 10, 249, 244, 249, 9, 192, 0, 249, 244, 2, 244, 5, 245,
	     6, 246, 7, 248, 7, 251, 6, 253, 5, 254, 2, 255, 249, 255 },
char81[] = { 24, 245, 11, 254, 244, 252, 245, 250, 247, 249, 249, 248, 252,
	     248, 1, 249, 4, 250, 6, 252, 8, 254, 9, 2, 9, 4, 8, 6, 6, 7, 4,
	     8, 1, 8, 252, 7, 249, 6, 247, 4, 245, 2, 244, 254, 244, 192, 0,
	     1, 5, 7, 11 },
char82[] = { 16, 245, 10, 249, 244, 249, 9, 192, 0, 249, 244, 2, 244, 5, 245,
	     6, 246, 7, 248, 7, 250, 6, 252, 5, 253, 2, 254, 249, 254, 192, 0,
	     0, 254, 7, 9 },
char83[] = { 20, 246, 10, 7, 247, 5, 245, 2, 244, 254, 244, 251, 245, 249,
	     247, 249, 249, 250, 251, 251, 252, 253, 253, 3, 255, 5, 0, 6, 1,
	     7, 3, 7, 6, 5, 8, 2, 9, 254, 9, 251, 8, 249, 6 },
char84[] = { 5, 248, 8, 0, 244, 0, 9, 192, 0, 249, 244, 7, 244 },
char85[] = { 10, 245, 11, 249, 244, 249, 3, 250, 6, 252, 8, 255, 9, 1, 9, 4,
	     8, 6, 6, 7, 3, 7, 244 },
char86[] = { 5, 247, 9, 248, 244, 0, 9, 192, 0, 8, 244, 0, 9 },
char87[] = { 11, 244, 12, 246, 244, 251, 9, 192, 0, 0, 244, 251, 9, 192, 0, 0,
	     244, 5, 9, 192, 0, 10, 244, 5, 9 },
char88[] = { 5, 246, 10, 249, 244, 7, 9, 192, 0, 7, 244, 249, 9 },
char89[] = { 6, 247, 9, 248, 244, 0, 254, 0, 9, 192, 0, 8, 244, 0, 254 },
char90[] = { 8, 246, 10, 7, 244, 249, 9, 192, 0, 249, 244, 7, 244, 192, 0,
	    249, 9, 7, 9 },
char91[] = { 11, 249, 7, 253, 240, 253, 16, 192, 0, 254, 240, 254, 16, 192, 0,
	     253, 240, 4, 240, 192, 0, 253, 16, 4, 16 },
char92[] = { 2, 245, 11, 9, 16, 247, 240 },
char93[] = { 11, 249, 7, 2, 240, 2, 16, 192, 0, 3, 240, 3, 16, 192, 0, 252,
	     240, 3, 240, 192, 0, 252, 16, 3, 16 },
char94[] = { 7, 245, 11, 248, 2, 0, 253, 8, 2, 192, 0, 248, 2, 0, 254, 8, 2 },
char95[] = { 2, 253, 22, 0, 9, 20, 9 },
char96[] = { 7, 251, 5, 1, 244, 0, 245, 255, 247, 255, 249, 0, 250, 1, 249, 0,
	     248 },
char97[] = { 17, 247, 10, 6, 251, 6, 9, 192, 0, 6, 254, 4, 252, 2, 251, 255,
	     251, 253, 252, 251, 254, 250, 1, 250, 3, 251, 6, 253, 8, 255, 9,
	     2, 9, 4, 8, 6, 6 },
char98[] = { 17, 246, 9, 250, 244, 250, 9, 192, 0, 250, 254, 252, 252, 254,
	     251, 1, 251, 3, 252, 5, 254, 6, 1, 6, 3, 5, 6, 3, 8, 1, 9, 254,
	     9, 252, 8, 250, 6 },
char99[] = { 14, 247, 9, 6, 254, 4, 252, 2, 251, 255, 251, 253, 252, 251, 254,
	     250, 1, 250, 3, 251, 6, 253, 8, 255, 9, 2, 9, 4, 8, 6, 6 },
char100[] = { 17, 247, 10, 6, 244, 6, 9, 192, 0, 6, 254, 4, 252, 2, 251, 255,
	      251, 253, 252, 251, 254, 250, 1, 250, 3, 251, 6, 253, 8, 255, 9,
	      2, 9, 4, 8, 6, 6 },
char101[] = { 17, 247, 9, 250, 1, 6, 1, 6, 255, 5, 253, 4, 252, 2, 251, 255,
	      251, 253, 252, 251, 254, 250, 1, 250, 3, 251, 6, 253, 8, 255, 9,
	      2, 9, 4, 8, 6, 6 },
char102[] = { 8, 251, 7, 5, 244, 3, 244, 1, 245, 0, 248, 0, 9, 192, 0, 253,
	      251, 4, 251 },
char103[] = { 22, 247, 10, 6, 251, 6, 11, 5, 14, 4, 15, 2, 16, 255, 16, 253,
	      15, 192, 0, 6, 254, 4, 252, 2, 251, 255, 251, 253, 252, 251,
	      254, 250, 1, 250, 3, 251, 6, 253, 8, 255, 9, 2, 9, 4, 8, 6, 6 },
char104[] = { 10, 247, 10, 251, 244, 251, 9, 192, 0, 251, 255, 254, 252, 0,
	      251, 3, 251, 5, 252, 6, 255, 6, 9 },
char105[] = { 8, 252, 4, 255, 244, 0, 245, 1, 244, 0, 243, 255, 244, 192, 0,
	      0, 251, 0, 9 },
char106[] = { 11, 251, 5, 0, 244, 1, 245, 2, 244, 1, 243, 0, 244, 192, 0, 1,
	      251, 1, 12, 0, 15, 254, 16, 252, 16 },
char107[] = { 8, 247, 8, 251, 244, 251, 9, 192, 0, 5, 251, 251, 5, 192, 0,
	      255, 1, 6, 9 },
char108[] = { 2, 252, 4, 0, 244, 0, 9 },
char109[] = { 18, 241, 15, 245, 251, 245, 9, 192, 0, 245, 255, 248, 252, 250,
	      251, 253, 251, 255, 252, 0, 255, 0, 9, 192, 0, 0, 255, 3, 252,
	      5, 251, 8, 251, 10, 252, 11, 255, 11, 9 },
char110[] = { 10, 247, 10, 251, 251, 251, 9, 192, 0, 251, 255, 254, 252, 0,
	      251, 3, 251, 5, 252, 6, 255, 6, 9 },
char111[] = { 17, 247, 10, 255, 251, 253, 252, 251, 254, 250, 1, 250, 3, 251,
	      6, 253, 8, 255, 9, 2, 9, 4, 8, 6, 6, 7, 3, 7, 1, 6, 254, 4, 252,
	      2, 251, 255, 251 },
char112[] = { 17, 246, 9, 250, 251, 250, 16, 192, 0, 250, 254, 252, 252, 254,
	      251, 1, 251, 3, 252, 5, 254, 6, 1, 6, 3, 5, 6, 3, 8, 1, 9, 254,
	      9, 252, 8, 250, 6 },
char113[] = { 17, 247, 10, 6, 251, 6, 16, 192, 0, 6, 254, 4, 252, 2, 251, 255,
	      251, 253, 252, 251, 254, 250, 1, 250, 3, 251, 6, 253, 8, 255, 9,
	      2, 9, 4, 8, 6, 6 },
char114[] = { 8, 249, 6, 253, 251, 253, 9, 192, 0, 253, 1, 254, 254, 0, 252,
	      2, 251, 5, 251 },
char115[] = { 17, 248, 9, 6, 254, 5, 252, 2, 251, 255, 251, 252, 252, 251,
	      254, 252, 0, 254, 1, 3, 2, 5, 3, 6, 5, 6, 6, 5, 8, 2, 9, 255, 9,
	      252, 8, 251, 6 },
char116[] = { 8, 251, 7, 0, 244, 0, 5, 1, 8, 3, 9, 5, 9, 192, 0, 253, 251, 4,
	      251 },
char117[] = { 10, 247, 10, 251, 251, 251, 5, 252, 8, 254, 9, 1, 9, 3, 8, 6, 5,
	      192, 0, 6, 251, 6, 9 },
char118[] = { 5, 248, 8, 250, 251, 0, 9, 192, 0, 6, 251, 0, 9 },
char119[] = { 11, 245, 11, 248, 251, 252, 9, 192, 0, 0, 251, 252, 9, 192, 0,
	      0, 251, 4, 9, 192, 0, 8, 251, 4, 9 },
char120[] = { 5, 248, 9, 251, 251, 6, 9, 192, 0, 6, 251, 251, 9 },
char121[] = { 9, 248, 8, 250, 251, 0, 9, 192, 0, 6, 251, 0, 9, 254, 13, 252,
	      15, 250, 16, 249, 16 },
char122[] = { 8, 248, 9, 6, 251, 251, 9, 192, 0, 251, 251, 6, 251, 192, 0,
	      251, 9, 6, 9 },
char123[] = { 39, 249, 7, 2, 240, 0, 241, 255, 242, 254, 244, 254, 246, 255,
	      248, 0, 249, 1, 251, 1, 253, 255, 255, 192, 0, 0, 241, 255, 243,
	      255, 245, 0, 247, 1, 248, 2, 250, 2, 252, 1, 254, 253, 0, 1, 2,
	      2, 4, 2, 6, 1, 8, 0, 9, 255, 11, 255, 13, 0, 15, 192, 0, 255, 1,
	      1, 3, 1, 5, 0, 7, 255, 8, 254, 10, 254, 12, 255, 14, 0, 15, 2,
	      16 },
char124[] = { 2, 252, 4, 0, 240, 0, 16 },
char125[] = { 39, 249, 7, 254, 240, 0, 241, 1, 242, 2, 244, 2, 246, 1, 248, 0,
	      249, 255, 251, 255, 253, 1, 255, 192, 0, 0, 241, 1, 243, 1, 245,
	      0, 247, 255, 248, 254, 250, 254, 252, 255, 254, 3, 0, 255, 2,
	      254, 4, 254, 6, 255, 8, 0, 9, 1, 11, 1, 13, 0, 15, 192, 0, 1, 1,
	      255, 3, 255, 5, 0, 7, 1, 8, 2, 10, 2, 12, 1, 14, 0, 15, 254, 16,
	      },
char126[] = { 23, 255, 21, 2, 1, 0, 255, 1, 253, 3, 251, 5, 251, 7, 252,
	      11, 255, 13, 0, 15, 0, 17, 255, 18, 254, 192, 0, 2, 0, 1,
	      254, 3, 253, 5, 253, 7, 254, 11, 1, 13, 2, 15, 2, 17, 1, 18,
	      255, 18, 252 };

/* Pointers to character definition tables. */

static unsigned char *ctab[] = {
    char32, char33, char34, char35, char36, char37, char38, char39, char40,
    char41, char42, char43, char44, char45, char46, char47, char48, char49,
    char50, char51, char52, char53, char54, char55, char56, char57, char58,
    char59, char60, char61, char62, char63, char64, char65, char66, char67,
    char68, char69, char70, char71, char72, char73, char74, char75, char76,
    char77, char78, char79, char80, char81, char82, char83, char84, char85,
    char86, char87, char88, char89, char90, char91, char92, char93, char94,
    char95, char96, char97, char98, char99, char100, char101, char102,
    char103, char104, char105, char106, char107, char108, char109, char110,
    char111, char112, char113, char114, char115, char116, char117, char118,
    char119, char120, char121, char122, char123, char124, char125, char126
};

/* Table used to look up sine of angles from 0 through 90 degrees.
   The value returned is the sine * 65536.  Symmetry is used to
   obtain sine and cosine for arbitrary angles using this table. */

static long sintab[] = {
    0, 1143, 2287, 3429, 4571, 5711, 6850, 7986, 9120, 10252, 11380,
    12504, 13625, 14742, 15854, 16961, 18064, 19160, 20251, 21336,
    22414, 23486, 24550, 25606, 26655, 27696, 28729, 29752, 30767,
    31772, 32768, 33753, 34728, 35693, 36647, 37589, 38521, 39440,
    40347, 41243, 42125, 42995, 43852, 44695, 45525, 46340, 47142,
    47929, 48702, 49460, 50203, 50931, 51643, 52339, 53019, 53683,
    54331, 54963, 55577, 56175, 56755, 57319, 57864, 58393, 58903,
    59395, 59870, 60326, 60763, 61183, 61583, 61965, 62328, 62672,
    62997, 63302, 63589, 63856, 64103, 64331, 64540, 64729, 64898,
    65047, 65176, 65286, 65376, 65446, 65496, 65526, 65536
};

static int extleft, exttop, extright, extbottom;  /* To accumulate extents */

/* LINTLIBRARY */

/*  ISIN  --  Return sine of an angle in integral degrees.  The
	      value returned is 65536 times the sine.  */

#if __STDC__
static long isin(int deg)
#else
static long isin(deg)
  int deg;
#endif
{
    /* Domain reduce to 0 to 360 degrees. */

    if (deg < 0) {
	deg = (360 - ((- deg) % 360)) % 360;
    } else if (deg >= 360) {
	deg = deg % 360;
    }

    /* Now look up from table according to quadrant. */

    if (deg <= 90) {
	return sintab[deg];
    } else if (deg <= 180) {
	return sintab[180 - deg];
    } else if (deg <= 270) {
	return -sintab[deg - 180];
    }
    return -sintab[360 - deg];
}

/*  ICOS  --  Return cosine of an angle in integral degrees.  The
	      value returned is 65536 times the cosine.  */

#if __STDC__
static long icos(int deg)
#else
static long icos(deg)
  int deg;
#endif
{
    return isin(deg + 90);
}  

#define Schar(x) (u = (x), (((u) & 0x80) ? ((u) | (-1 ^ 0xFF)) : (u)))

#define Scalef 21		/* Font design size */
#define Descend 9		/* Descender offset */

/* PPMD_TEXT  --  Draw the zero-terminated  string  s,	with  its  baseline
		  starting  at	point  (x, y), inclined by angle degrees to
		  the X axis, with letters height pixels  high	(descenders
		  will	extend below the baseline).  The supplied drawprocP
		  and cliendata are passed to ppmd_line which performs	the
		  actual drawing. */

#if __STDC__
void
ppmd_text(pixel** pixels, int cols, int rows, pixval maxval, int x, int y, int height, int angle, char *s, void (*drawprocP)(pixel**, int, int, pixval, int, int, char*), char* clientdata)
#else /*__STDC__*/
void
ppmd_text(pixels, cols, rows, maxval, x, y, height, angle, s, drawprocP, clientdata)
    pixel** pixels;
    int cols, rows, x, y, height, angle;
    char *s;
    pixval maxval;
    void (*drawprocP)();
    char* clientdata;
#endif /*__STDC__*/
{
    int xpos = x, ypos = y;
    long rotsin, rotcos;

    x = y = 0;
    rotsin = isin(-angle);
    rotcos = icos(-angle);
    while (*s) {
	unsigned char ch = *s++;
	int pen = 1;
	int u;

        if (ch >= ' ' && ch < 127) {
            ch -= ' ';
	    if (ctab[ch] != NULL) {
		int cn = *ctab[ch];
		unsigned char *cst = ctab[ch] + 3;
		int lx, ly;

		x -= Schar(*(ctab[ch] + 1));
		lx = x + Schar(*cst++);
		ly = y + Schar(*cst++);
		while (--cn > 0) {
		    if (*cst == 192) {
			pen = 0;
			cst += 2;
		    } else {
			int nx = x + Schar(*cst++);
			int ny = y + Schar(*cst++);
			if (pen) {
			    int mx1, my1, mx2, my2;
			    int tx1, ty1, tx2, ty2;

                            /* Note that up until this  moment  we've  been
			       working	in  an	arbitrary model co-ordinate
			       system with  fixed  size  and  no  rotation.
			       Before  drawing	the  stroke,  transform  to
			       viewing co-ordinates to	honour	the  height
			       and angle specifications. */

			    mx1 = (lx * height) / Scalef;
			    my1 = ((ly - Descend) * height) / Scalef;
			    mx2 = (nx * height) / Scalef;
			    my2 = ((ny - Descend) * height) / Scalef;
			    tx1 = xpos + (mx1 * rotcos - my1 * rotsin) / 65536;
			    ty1 = ypos + (mx1 * rotsin + my1 * rotcos) / 65536;
			    tx2 = xpos + (mx2 * rotcos - my2 * rotsin) / 65536;
			    ty2 = ypos + (mx2 * rotsin + my2 * rotcos) / 65536;
			    ppmd_line(pixels, cols, rows, maxval,
				tx1, ty1, tx2, ty2,
				drawprocP, clientdata);
			}
			lx = nx;
			ly = ny;
			pen = 1;
		    }
		}
		x += *(ctab[ch] + 2); 
	    }
        } else if (ch == '\n') {
	    y += Scalef + Descend;
	    x = 0;
	}
    }
}

/* EXTENTS_DRAWPROC  --  Drawproc which just accumulates the extents
			 rectangle bounding the text. */

/* ARGSUSED */
#if __STDC__
static void extents_drawproc (pixel** pixels, int cols, int rows,
			      pixval maxval, int x, int y, char* clientdata)
#else
static void extents_drawproc (pixels, cols, rows,
			      maxval, x, y, clientdata)
pixel** pixels;
int cols, rows, x, y;
pixval maxval;
char* clientdata;
#endif
{
    extleft = min(extleft, x);
    exttop = min(exttop, y);
    extright = max(extright, x);
    extbottom = max(extbottom, y);
}


/* PPMD_TEXT_BOX  --  Calculate  extents  rectangle for a given piece of
		      text.  For most  applications  where  extents  are
		      needed,	angle  should  be  zero  to  obtain  the
		      unrotated extents.  If you need  the  extents  box
		      for post-rotation text, however, you can set angle
		      nonzero and it will be calculated correctly. */

#if __STDC__
void
ppmd_text_box(int height, int angle, char *s, int *left, int *top, int *right, int *bottom)
#else /*__STDC__*/
void
ppmd_text_box(height, angle, s, left, top, right, bottom)
    int height, angle;
    char *s;
    int *left, *top, *right, *bottom;
#endif /*__STDC__*/
{
    extleft = 32767;
    exttop = 32767;
    extright = -32767;
    extbottom = -32767;
    ppmd_text((pixel **) NULL, 32767, 32767, 255, 1000, 1000, height, angle, s, extents_drawproc, (char *) NULL);
    *left = extleft - 1000; 
    *top = exttop - 1000;
    *right = extright - 1000;
    *bottom = extbottom - 1000;
}
