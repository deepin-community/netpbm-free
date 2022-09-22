/* pnmflip.c - perform one or more flip operations on a portable anymap
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "pnm.h"

struct xformMatrix {
    int a;
    int b;
    int c;
    int d;
    int e;
    int f;
};

static void
leftright( int * const aP, int * const bP, int * const cP, 
           int * const dP, int * const eP, int * const fP ) {
    *aP = - *aP;
    *cP = - *cP;
    *eP = - *eP + 1;
}



static void
topbottom( int * const aP, int * const bP, int * const cP, 
           int * const dP, int * const eP, int * const fP ) {
    *bP = - *bP;
    *dP = - *dP;
    *fP = - *fP + 1;
}



static void
transpose( int * const aP, int * const bP, int * const cP, 
           int * const dP, int * const eP, int * const fP ) {

    register int t;
    
    t = *aP;
    *aP = *bP;
    *bP = t;
    t = *cP;
    *cP = *dP;
    *dP = t;
    t = *eP;
    *eP = *fP;
    *fP = t;
}



static void
parseCommandLine( int argc, char * argv[], const char ** const inputFilespec,
                  struct xformMatrix * const xformP ) {

    const char* const usage = 
        "[-leftright|-lr] [-topbottom|-tb] [-transpose|-xy]\n"
        "            [-rotate90|-r90|-ccw] [-rotate270|r270|-cw]\n"
        "            [-rotate180|-r180] [pnmfile]";

    int a, b, c, d, e, f;

    int argn;

    argn = 1;
    
    /* Just check the validity of arguments here. */
    while ( argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0' ) 	{
        if ( pm_keymatch( argv[argn], "-lr", 2 ) ||
             pm_keymatch( argv[argn], "-leftright", 2 ) )
	    { }
        else if ( pm_keymatch( argv[argn], "-tb", 3 ) ||
                  pm_keymatch( argv[argn], "-topbottom", 3 ) )
	    { }
        else if ( pm_keymatch( argv[argn], "-xy", 2 ) ||
                  pm_keymatch( argv[argn], "-transpose", 3 ) )
	    { }
        else if ( pm_keymatch( argv[argn], "-r90", 3 ) ||
                  pm_keymatch( argv[argn], "-rotate90", 8 ) ||
                  pm_keymatch( argv[argn], "-ccw", 3 ) )
	    { }
        else if ( pm_keymatch( argv[argn], "-r270", 3 ) ||
                  pm_keymatch( argv[argn], "-rotate270", 8 ) ||
                  pm_keymatch( argv[argn], "-cw", 3 ) )
	    { }
        else if ( pm_keymatch( argv[argn], "-r180", 3 ) ||
                  pm_keymatch( argv[argn], "-rotate180", 8 ) )
	    { }
        else
            pm_usage( usage );
        ++argn;
	}

    if ( argn != argc ) 
        *inputFilespec = argv[argn++];
    else
        *inputFilespec = "-";

    if ( argn != argc )
        pm_usage( usage );

    /* Now go through the flags again, this time accumulating transforms. */
    a = 1; b = 0;
    c = 0; d = 1;
    e = 0; f = 0;
    argn = 1;
    while ( argn < argc && argv[argn][0] == '-' ) {
        if ( pm_keymatch( argv[argn], "-lr", 2 ) ||
             pm_keymatch( argv[argn], "-leftright", 2 ) )
            leftright( &a, &b, &c, &d, &e, &f );
        else if ( pm_keymatch( argv[argn], "-tb", 3 ) ||
                  pm_keymatch( argv[argn], "-topbottom", 3 ) )
            topbottom( &a, &b, &c, &d, &e, &f );
        else if ( pm_keymatch( argv[argn], "-xy", 2 ) ||
                  pm_keymatch( argv[argn], "-transpose", 3 ) )
            transpose( &a, &b, &c, &d, &e, &f );
        else if ( pm_keymatch( argv[argn], "-r90", 3 ) ||
                  pm_keymatch( argv[argn], "-rotate90", 8 ) ||
                  pm_keymatch( argv[argn], "-ccw", 3 ) ) {
            transpose( &a, &b, &c, &d, &e, &f );
            topbottom( &a, &b, &c, &d, &e, &f );
	    } else if ( pm_keymatch( argv[argn], "-r270", 3 ) ||
                    pm_keymatch( argv[argn], "-rotate270", 8 ) ||
                    pm_keymatch( argv[argn], "-cw", 3 ) ) {
            transpose( &a, &b, &c, &d, &e, &f );
            leftright( &a, &b, &c, &d, &e, &f );
	    } else if ( pm_keymatch( argv[argn], "-r180", 3 ) ||
                    pm_keymatch( argv[argn], "-rotate180", 8 ) ) {
            leftright( &a, &b, &c, &d, &e, &f );
            topbottom( &a, &b, &c, &d, &e, &f );
	    } else
            pm_error( "shouldn't happen!" );
        ++argn;
	}

    xformP->a = a;
    xformP->b = b;
    xformP->c = c;
    xformP->d = d;
    xformP->e = e;
    xformP->f = f;
}



static void
transformRowByRow( FILE * const ifp, int const rows, int const cols, 
                   int const newrows, int const newcols, xelval const maxval,
                   int const format, struct xformMatrix const xform ) {
    xel* xelrow;
    xel* newxelrow;
    int row;
    
    xelrow = pnm_allocrow( cols );
    newxelrow = pnm_allocrow( newcols );
    pnm_writepnminit( stdout, newcols, newrows, maxval, format, 0 );
    
    for ( row = 0; row < rows; ++row ) {
        int col;
        pnm_readpnmrow( ifp, xelrow, cols, maxval, format );
        for ( col = 0; col < cols; ++col ) {
            /* Transform a point:
            **
            **            [ a b 0 ]
            **  [ x y 1 ] [ c d 0 ] = [ x2 y2 1 ]
            **            [ e f 1 ]
            */
            int const newcol = xform.a * col + xform.c * row + 
                xform.e * ( newcols - 1 );
            newxelrow[newcol] = xelrow[col];
        }
        pnm_writepnmrow( stdout, newxelrow, newcols, maxval, format, 0 );
    }
    pnm_freerow( xelrow );
    pnm_freerow( newxelrow );
}



static void __inline__
transformPoint( int const col, int const newcols,
                int const row, int const newrows, 
                struct xformMatrix const xform, 
                int * const newcolP, int * const newrowP ) {
    /* Transform a point:
    **
    **            [ a b 0 ]
    **  [ x y 1 ] [ c d 0 ] = [ x2 y2 1 ]
    **            [ e f 1 ]
    */
    *newcolP = xform.a * col + xform.c * row + 
        xform.e * ( newcols - 1 );
    *newrowP = xform.b * col + xform.d * row + 
        xform.f * ( newrows - 1 );
}



static void
transformNonPbm( FILE * const ifp, int const rows, int const cols,
                 int const newrows, int const newcols, xelval const maxval,
                 int const format, struct xformMatrix const xform ) {

    xel* xelrow;
    xel** newxels;
    int row;
    
    xelrow = pnm_allocrow( cols );
    newxels = pnm_allocarray( newcols, newrows );
    
    for ( row = 0; row < rows; ++row ) {
        int col;
        pnm_readpnmrow( ifp, xelrow, cols, maxval, format );
        for ( col = 0; col < cols; ++col ) {
            int newcol, newrow;
            transformPoint( col, newcols, row, newrows, xform,
                            &newcol, &newrow );
            newxels[newrow][newcol] = xelrow[col];
        }
    }
    
    pnm_writepnm( stdout, newxels, newcols, newrows, maxval, format, 0 );
    
    pnm_freearray( newxels, newrows);
    pnm_freerow( xelrow );
}



static void
transformPbm( FILE * const ifp, int const rows, int const cols, 
              int const newrows, int const newcols, int const format,
              struct xformMatrix const xform ) { 
/*----------------------------------------------------------------------------
   This is the same as transformPgmOrPpm, except that it uses less 
   memory, since the PBM buffer format uses one byte per pixel instead
   of twelve.
-----------------------------------------------------------------------------*/
    bit* bitrow;
    bit** newbits;
    int row;
            
    bitrow = pbm_allocrow( cols );
    newbits = pbm_allocarray( newcols, newrows );
            
    for ( row = 0; row < rows; ++row ) {
        int col;
        pbm_readpbmrow( ifp, bitrow, cols, format );
        for ( col = 0; col < cols; ++col ) {
            int newcol, newrow;
            transformPoint( col, newcols, row, newrows, xform,
                            &newcol, &newrow );
            newbits[newrow][newcol] = bitrow[col];
        }
    }
            
    pbm_writepbm( stdout, newbits, newcols, newrows, 0 );
            
    pbm_freearray( newbits, newrows);
    pbm_freerow( bitrow );
}



int
main( int argc, char * argv[] ) {
    FILE* ifp;
    int cols, rows, format, newrows, newcols;
    xelval maxval;
    const char * inputFilespec;
    struct xformMatrix xform;

    pnm_init( &argc, argv );

    parseCommandLine( argc, argv, &inputFilespec, &xform );

    ifp = pm_openr( inputFilespec );
    
    pnm_readpnminit( ifp, &cols, &rows, &maxval, &format );

    overflow2(abs(xform.a), cols);
    overflow2(abs(xform.b), cols);
    overflow2(abs(xform.c), rows);
    overflow2(abs(xform.d), rows);
    overflow_add(abs( xform.a ) * cols, abs( xform.c ) * rows);
    overflow_add(abs( xform.b ) * cols, abs( xform.d ) * rows);

    newcols = abs( xform.a ) * cols + abs( xform.c ) * rows;
    newrows = abs( xform.b ) * cols + abs( xform.d ) * rows;
    
    if ( xform.b == 0 && xform.d == 1 && xform.f == 0 ) {
        /* In this case newrow is always equal to row, so we can do the
        ** transform line by line and avoid in-memory buffering altogether.
        */
        transformRowByRow( ifp, rows, cols, newrows, newcols, maxval, 
                           format, xform );
	} else {
        /* Generic case.  Read in the anymap a line at a time and transform
        ** it into an in-memory array.
        */
        switch ( PNM_FORMAT_TYPE(format) ) {
        case PBM_TYPE: 
            transformPbm( ifp, rows, cols, newrows, newcols, format, xform );
            break;
        default:
            transformNonPbm( ifp, rows, cols, newrows, newcols, maxval,
                             format, xform );
            break;
        }
	}
    
    pm_close( ifp );
    pm_close( stdout );
    
    exit( 0 );
}

