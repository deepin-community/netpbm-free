/* libpgm1.c - pgm utility library part 1
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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "pgm.h"
#include "libpgm.h"
#include "pbm.h"
#include "libpbm.h"

void 
pgm_init(int * const argcP, char ** const argv) {
    pbm_init( argcP, argv );
}



void
pgm_nextimage(FILE * const file, int * const eofP) {
    pm_nextimage(file, eofP);
}


void
pgm_readpgminitrest(FILE * const file, 
                    int * const colsP, int * const rowsP, 
                    gray * const maxvalP) {

    int maxval;

    /* Read size. */
    *colsP = pbm_getint( file );
    *rowsP = pbm_getint( file );

    /* Read maxval. */
    maxval = pbm_getint( file );
    if ( maxval > PGM_OVERALLMAXVAL )
    pm_error( "maxval is too large.  The largest we can handle is %d.", 
              PGM_OVERALLMAXVAL );
    *maxvalP = maxval;
}



void
pgm_readpgminit(FILE * const file, int * const colsP, 
                int * const rowsP, gray * const maxvalP, int * const formatP) {

    /* Check magic number. */
    *formatP = pbm_readmagicnumber( file );
    switch ( PGM_FORMAT_TYPE(*formatP) ) {
        case PGM_TYPE:
            pgm_readpgminitrest( file, colsP, rowsP, maxvalP );
    break;
    
    case PBM_TYPE:
        pbm_readpbminitrest( file, colsP, rowsP );
        
        /* Mathematically, it makes the most sense for the maxval of a PBM
           file seen as a PGM to be 1.  But we tried this for a while and
           found that it causes unexpected results and frequent need for a
           Pnmdepth stage to convert the maxval to 255.  You see, when you
           transform a PGM file in a way that causes interpolated gray shades,
           there's no in-between value to use when maxval is 1.  It's really
           hard even to discover that your lack of Pnmdepth is your problem.
           So we pick 255, which is the most common PGM maxval, and the highest
           resolution you can get without increasing the size of the PGM 
           image.

           So this means some programs that are capable of exploiting the
           bi-level nature of a PBM file must be PNM programs instead of PGM
           programs.
        */
        
        *maxvalP = PGM_MAXMAXVAL;
        break;

    default:
        pm_error( "bad magic number - not a pgm or pbm file" );
    }
}



gray
pgm_getrawsample(FILE * const file, gray const maxval) {

    if (maxval < 256) {
        /* The sample is just one byte.  Read it. */
        return(pbm_getrawbyte(file));
    } else {
        /* The sample is two bytes.  Read both. */
        unsigned char byte_pair[2];
        size_t pairs_read;

        pairs_read = fread(&byte_pair, 2, 1, file);
        if (pairs_read == 0) 
            pm_error("EOF /read error while reading a long sample");
        /* This could be a few instructions faster if exploited the internal
           format (i.e. endianness) of a pixval.  Then we might be able to
           skip the shifting and oring.
           */
        return((byte_pair[0]<<8) | byte_pair[1]);
    }
}



void
pgm_readpgmrow(FILE* const file, gray* const grayrow, 
               int const cols, gray const maxval, int const format) {

    switch (format) {
    case PGM_FORMAT: {
        int col;
        for (col = 0; col < cols; ++col) {
            grayrow[col] = pbm_getint(file);
#ifdef DEBUG
            if (grayrow[col] > maxval)
                pm_error( "value out of bounds (%u > %u)", 
                          grayrow[col], maxval );
#endif /*DEBUG*/
        }
    }
    break;
        
    case RPGM_FORMAT: {
        int col;
        for (col = 0; col < cols; ++col) {
            grayrow[col] = pgm_getrawsample( file, maxval );
#ifdef DEBUG
            if ( grayrow[col] > maxval )
                pm_error( "value out of bounds (%u > %u)", 
                          grayrow[col], maxval );
#endif /*DEBUG*/
        }
    }
    break;
    
    case PBM_FORMAT:
    case RPBM_FORMAT:
    {
        bit * bitrow;
        int col;

        bitrow = pbm_allocrow(cols);
        pbm_readpbmrow( file, bitrow, cols, format );
        for (col = 0; col < cols; ++col)
            grayrow[col] = (bitrow[col] == PBM_WHITE ) ? maxval : 0;
        pbm_freerow(bitrow);
    }
    break;
        
    default:
        pm_error( "can't happen" );
    }
}



gray**
pgm_readpgm(FILE * const file, int * const colsP, int * const rowsP, 
            gray * const maxvalP) {

    gray** grays;
    int row;
    int format;

    pgm_readpgminit( file, colsP, rowsP, maxvalP, &format );
    
    grays = pgm_allocarray( *colsP, *rowsP );
    
    for ( row = 0; row < *rowsP; ++row )
        pgm_readpgmrow( file, grays[row], *colsP, *maxvalP, format );
    
    return grays;
}



void
pgm_check(FILE * file, const enum pm_check_type check_type, 
          const int format, const int cols, const int rows, const int maxval,
          enum pm_check_code * const retval_p) {

    if (check_type != PM_CHECK_BASIC) {
        if (retval_p) *retval_p = PM_CHECK_UNKNOWN_TYPE;
    } else if (PGM_FORMAT_TYPE(format) == PBM_TYPE) {
        pbm_check(file, check_type, format, cols, rows, retval_p);
    } else if (format != RPGM_FORMAT) {
        if (retval_p) *retval_p = PM_CHECK_UNCHECKABLE;
    } else {        
        const unsigned int bytes_per_row =
            cols * (maxval > 255 ? 2 : 1);
        const unsigned int need_raster_size = rows * bytes_per_row;
        
        pm_check(file, check_type, need_raster_size, retval_p);
        
    }
}
