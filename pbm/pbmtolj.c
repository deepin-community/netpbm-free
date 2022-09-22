/* pbmtolj.c - read a portable bitmap and produce a LaserJet bitmap file
**  
**  based on pbmtops.c
**
**  Michael Haberler HP Vienna mah@hpuviea.uucp
**                 mcvax!tuvie!mah
**  misfeatures: 
**      no positioning
**
**      Bug fix Dec 12, 1988 :
**              lines in putbit() reshuffled 
**              now runs OK on HP-UX 6.0 with X10R4 and HP Laserjet II
**      Bo Thide', Swedish Institute of Space Physics, Uppsala <bt@irfu.se>
**
**  Flags added December, 1993:
**      -noreset to suppress printer reset code
**      -float to suppress positioning code (such as it is)
**  Wim Lewis, Seattle <wiml@netcom.com>
**
** Copyright (C) 1988 by Jef Poskanzer and Michael Haberler.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "pbm.h"
#include <string.h>
#include <assert.h>
#include <string.h>

static int dpi = 75;
static int floating = 0;  /* suppress the ``ESC & l 0 E'' ? */
static int pack = 0; /* use TIFF packbits compression */
static int delta = 0; /* use row-delta compression */
static int resets = 3;    /* bit mask for when to emit printer reset seq */
static int copies = 1;    /* number of copies */
static char *rowBuffer, *prevRowBuffer, *packBuffer, *deltaBuffer;
static int rowBufferSize, rowBufferIndex;
static int packBufferSize, packBufferIndex;
static int deltaBufferSize, deltaBufferIndex;

static void putinit ARGS(( void ));
static void putbit ARGS(( bit b ));
static void putflush ARGS(( void ));
static void putrest ARGS(( void ));
static void putitem ARGS(( void ));
static void packbits ARGS(( void ));
static void deltarow ARGS(( void ));

int
main( argc, argv )
    int argc;
    char* argv[];
    {
    FILE* ifp;
    bit* bitrow;
    register bit* bP;
    int argn, rows, cols, format, rucols, padright, row, mode, blankRows;
    int prevRowBufferIndex;
    int savedRowBufferIndex;
    register int nzcol, col;
    char* usage = "[-noreset|-float|-packbits|-delta|-compress|-resolution N|-copies N] [pbmfile]\n\tresolution = [75|100|150|300|600] (dpi)";

    pbm_init( &argc, argv );

    argn = 1;

    /* Check for flags. */
    while ( argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0' )
    {
    if ( pm_keymatch( argv[argn], "-resolution", 2 ) )
        {
        ++argn;
        if ( argn == argc || sscanf( argv[argn], "%d", &dpi ) != 1 )
        pm_usage( usage );
        }
    else if ( pm_keymatch( argv[argn], "-copies", 2 ) )
        {
        ++argn;
        if ( argn == argc || sscanf( argv[argn], "%d", &copies ) != 1 )
        pm_usage( usage );
        }
    else if ( pm_keymatch( argv[argn], "-float", 2 ) )
        {
        floating = 1;
        }
    else if ( pm_keymatch( argv[argn], "-noreset", 2 ) )
        {
        resets = 0;
        }
    else if ( pm_keymatch( argv[argn], "-packbits", 2 ) )
        {
        pack = 1;
        }
    else if ( pm_keymatch( argv[argn], "-delta", 2 ) )
        {
        delta = 1;
        }
    else if ( pm_keymatch( argv[argn], "-compress", 2 ) )
        {
        pack = 1;
        delta = 1;
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

    pbm_readpbminit( ifp, &cols, &rows, &format );
    bitrow = pbm_allocrow( cols );

    overflow_add(cols, 8);
    rowBufferSize = (cols + 7) / 8;
    overflow_add(rowBufferSize, 128);
    overflow_add(rowBufferSize, rowBufferSize+128);
    overflow_add(rowBufferSize+10, rowBufferSize/8);
    packBufferSize = rowBufferSize + (rowBufferSize + 127) / 128 + 1;
    deltaBufferSize = rowBufferSize + rowBufferSize / 8 + 10;

    prevRowBuffer = (char *) malloc(rowBufferSize);
    rowBuffer = (char *) malloc(rowBufferSize);
    packBuffer = (char *) malloc(packBufferSize);
    deltaBuffer = (char *) malloc(deltaBufferSize);

    if (!rowBuffer || !prevRowBuffer || !packBuffer || !deltaBuffer) {
      fprintf(stderr, "Can't allocate row buffer\n");
      return 1;
    }

    putinit( );

    blankRows = 0;
    prevRowBufferIndex = 0;
    memset(prevRowBuffer, 0, rowBufferSize);

    mode = -1;

    for ( row = 0; row < rows; ++row )
      {
    pbm_readpbmrow( ifp, bitrow, cols, format );

    /* Find rightmost black pixel. */
    for ( nzcol = cols - 1; nzcol >= 0 && bitrow[nzcol] == PBM_WHITE; --nzcol )
      continue;

    if (nzcol < 0) {
      blankRows ++;
      continue;
    }

    if (blankRows > 0) {
        int x;
        /* The code used to be this, but Charles Howes reports that this
           escape sequence does not exist on his HP Laserjet IIP plus,
           so we use the following less elegant code instead.

           printf("\033*b%dY", blankRows);
           */
        for (x=0;x<blankRows;x++) 
            printf("\033*b0W");

        blankRows = 0;
        memset(prevRowBuffer, 0, rowBufferSize);
    }

    memset(rowBuffer, 0, rowBufferSize);

    /* Round up to the nearest multiple of 8. */
    rucols = ( nzcol + 8 ) / 8;
    rucols = rucols * 8;
    padright = rucols - (nzcol + 1);

    rowBufferIndex = 0;

    /*
      Generate the unpacked data
    */

        for ( col = 0, bP = bitrow; col <= nzcol; ++col, ++bP )
      putbit( *bP );
    for ( col = 0; col < padright; ++col )
      putbit( 0 );
    putflush( );

    /*
        Try optional compression algorithms
    */

    if (pack) {
      packbits();
    } else {
      packBufferIndex = rowBufferIndex + 999;
    }

    if (delta) {

      /*
         May need to temporarily bump the row buffer index up to
         whatever the previous line's was - if this line is shorter 
         than the previous would otherwise leave dangling cruft.
      */

      savedRowBufferIndex = rowBufferIndex;

      if (rowBufferIndex < prevRowBufferIndex) {
        rowBufferIndex = prevRowBufferIndex;
      }

      deltarow();

      rowBufferIndex = savedRowBufferIndex;

    } else {
      deltaBufferIndex = packBufferIndex + 999;
    }

    if (deltaBufferIndex < packBufferIndex &&
        deltaBufferIndex < rowBufferIndex) {
      assert(deltaBufferIndex <= deltaBufferSize);
      /*
        It's smallest when delta'ed
      */
      if (mode != 3) {
        printf("\033*b3M");
        mode = 3;
      }
      printf("\033*b%dW", deltaBufferIndex);
      fwrite(deltaBuffer, 1, deltaBufferIndex, stdout);
    } else if (rowBufferIndex <= packBufferIndex) {
      assert (rowBufferIndex <= rowBufferSize);
      /*
        It didn't pack - send it unpacked
      */
      if (mode != 0) {
        printf("\033*b0M");
        mode = 0;
      }
      printf("\033*b%dW", rowBufferIndex);
      fwrite(rowBuffer, 1, rowBufferIndex, stdout);
    } else {
      assert (packBufferIndex <= packBufferSize);
      /*
        It's smaller when packed
      */
      if (mode != 2) {
        printf("\033*b2M");
        mode = 2;
      }
      printf("\033*b%dW", packBufferIndex);
      fwrite(packBuffer, 1, packBufferIndex, stdout);
        }
    memcpy(prevRowBuffer, rowBuffer, rowBufferSize);
    prevRowBufferIndex = rowBufferIndex;
      }

    pm_close( ifp );

    putrest( );

    exit( 0 );
    }

static int item, bitsperitem, bitshift, itemsperline, firstitem;

static void
putinit( )
    {
    if(resets & 1)
    {
    /* Printer reset. */
    printf("\033E");
    }

    if(copies > 1)
        {
        /* number of copies */
        printf("\033&l%dX", copies);
    }
    if(!floating)
    {
    /* Ensure top margin is zero */
    printf("\033&l0E");
    }

    /* Set raster graphics resolution */
    printf("\033*t%dR",dpi);

    /* Start raster graphics, relative adressing */
    printf("\033*r1A");

    itemsperline = 0;
    bitsperitem = 1;
    item = 0;
    bitshift = 7;
    firstitem = 1;
    }

#if __STDC__
static void
putbit( bit b )
#else /*__STDC__*/
static void
putbit( b )
bit b;
#endif /*__STDC__*/
    {
    if ( b == PBM_BLACK )
    item += 1 << bitshift;
    bitshift--;
    if ( bitsperitem == 8 ) {
    putitem( );
        bitshift = 7;
    }
    bitsperitem++;
    }

static void
putflush(  )
    {
    if ( bitsperitem > 1 )
    putitem( );
    }

static void
putrest( )
    {
    /* end raster graphics */
    printf( "\033*rB" );

    if(resets & 2)
    {
    /* Printer reset. */
    printf("\033E");
    }
    }

static void
putitem( )
    {
    assert (rowBufferIndex < rowBufferSize);
    rowBuffer[rowBufferIndex++] = item;
    bitsperitem = 0;
    item = 0;
    }

static void
packbits()
  {
    int ptr, litStart, runStart, thisByte, startByte, chew, spit;
    packBufferIndex = 0;
    ptr = 0;
    while (ptr < rowBufferIndex) {
      litStart = ptr;
      runStart = ptr;
      startByte = rowBuffer[ptr];
      ptr++;
      while (ptr < rowBufferIndex) {
    thisByte = rowBuffer[ptr];
    if (thisByte != startByte) {
      if (ptr - runStart > 3) { /* found literal after nontrivial run */
        break;
      }
      startByte = thisByte;
      runStart = ptr;
    }
    ptr ++;
      }
      /*
    We drop out here after having found a [possibly empty]
    literal, followed by a [possibly degenerate] run of repeated
    bytes.  Degenerate runs can occur at the end of the scan line...
    there may be a "repeat" of 1 byte (which can't actually be 
    represented as a repeat) so we simply fold it into the previous
    literal.
      */
      if (runStart == rowBufferIndex - 1) {
    runStart = rowBufferIndex;
      }
      /*
    Spit out the leading literal if it isn't empty
      */
      chew = runStart - litStart;
      while (chew > 0) {
    spit = (chew > 127) ? 127 : chew;
    packBuffer[packBufferIndex++] = (char) (spit - 1);
    memcpy(packBuffer+packBufferIndex, rowBuffer+litStart, spit);
    packBufferIndex += spit;
    litStart += spit;
    chew -= spit;
      }
      /*
    Spit out the repeat, if it isn't empty
      */
      chew = ptr - runStart;
      while (chew > 0) {
    spit = (chew > 128) ? 128 : chew;
    if (chew == spit + 1) {
      spit--; /* don't leave a degenerate run at the end */
    }
    if (spit == 1) {
      fprintf(stderr, "packbits created a degenerate run!\n");
    }
    packBuffer[packBufferIndex++] = (char) -(spit - 1);
    packBuffer[packBufferIndex++] = startByte;
    chew -= spit;
      }
    }
  }

static void
deltarow()
  {
    int burstStart, burstEnd, burstCode, mustBurst, ptr, skip, skipped, code;
    deltaBufferIndex = 0;
    if (memcmp(rowBuffer, prevRowBuffer, rowBufferIndex) == 0) {
      return; /* exact match, no deltas required */
    }
    ptr = 0;
    skipped = 0;
    burstStart = -1;
    burstEnd = -1;
    mustBurst = 0;
    while (ptr < rowBufferIndex) {
      skip = 0;
      if (ptr == 0 || skipped == 30 || rowBuffer[ptr] != prevRowBuffer[ptr] ||
      (burstStart != -1 && ptr == rowBufferIndex - 1)) {
    /* we want to output this byte... */
    if (burstStart == -1) {
      burstStart = ptr;
    }
    if (ptr - burstStart == 7 || ptr == rowBufferIndex - 1) {
      /* we have to output it now... */
      burstEnd = ptr;
      mustBurst = 1;
    }
      } else {
    /* duplicate byte, we can skip it */
    if (burstStart != -1) {
      burstEnd = ptr - 1;
      mustBurst = 1;
    }
    skip = 1;
      }
      if (mustBurst) {
    burstCode = burstEnd - burstStart; /* 0-7, means 1-8 bytes follow */
    code = (burstCode << 5) | skipped;
    deltaBuffer[deltaBufferIndex++] = (char) code;
    memcpy(deltaBuffer+deltaBufferIndex, rowBuffer+burstStart, burstCode + 1);
    deltaBufferIndex += burstCode + 1;
    burstStart = -1;
    burstEnd = -1;
    mustBurst = 0;
    skipped = 0;
      }
      if (skip) {
    skipped ++;
      }
      ptr ++;
    }
  }
