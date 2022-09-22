/* pbmtog3.c - read a portable bitmap and produce a Group 3 FAX file
**
** Copyright (C) 1989 by Paul Haeberli <paul@manray.sgi.com>.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "pbm.h"
#include "g3.h"

static void tofax ARGS(( bit* bitrow, int n ));
static void putwhitespan ARGS(( int c ));
static void putblackspan ARGS(( int c ));
static void putcode ARGS(( tableentry* te ));
static void puteol ARGS(( void ));
static void putinit ARGS(( void ));
static void putbit ARGS(( int d ));
static void flushbits ARGS(( void ));

static int reversebits;

int
main( argc, argv )
    int argc;
    char* argv[];
    {
    FILE* ifp;
    bit* bitrow;
    int argn, rows, cols, format, row, i;
    char* usage = " [-reversebits] [pbmfile]";


    pbm_init( &argc, argv );

    argn = 1;
    reversebits = 0;

    if ( argn < argc && argv[argn][0] == '-' && argv[argn][1] != '\0' )
	{
	if ( pm_keymatch( argv[argn], "-reversebits", 2 ) )
	    reversebits = 1;
	else
	    pm_usage( usage );
	++argn;
	}
	
    if ( argn == argc )
	ifp = stdin;
    else
	{
	ifp = pm_openr( argv[argn] );
	++argn;
	}
    
    if ( argn != argc )
	pm_usage( usage );

    pbm_readpbminit( ifp, &cols, &rows, &format );
    bitrow = pbm_allocrow( cols );

    /* Write out four extra rows to get things stabilized. */
    putinit();
    puteol();

    /* Write out bitmap. */
    for ( row = 0; row < rows; ++row )
	{
	pbm_readpbmrow( ifp, bitrow, cols, format );
	tofax( bitrow, cols );
	}

    /* And finish off. */
    for( i = 0; i < 6; ++i )
	puteol( );
    flushbits( );

    pm_close( ifp );

    exit( 0 );
    }

static void
tofax(bitrow,n)
    bit* bitrow;
    int n;
{
    int c;

    while(n>0) {
	c = 0;
	while(*bitrow == PBM_WHITE && n>0) {
	    ++bitrow;
	    ++c;
	    --n;
	}
	putwhitespan(c);
	c = 0;
	if(n==0)
	    break;
	while(*bitrow == PBM_BLACK && n>0) {
	    ++bitrow;
	    ++c;
	    --n;
	}
	putblackspan(c);
    }
    puteol();
}

static void
putwhitespan(c)
    int c;
{
    int tpos;
    tableentry* te;

    do {
        if(c>=64) {
            if (c < 1792) {
                tpos = (c - 64) / 64;
                te = mwtable+tpos;
                }
            else {
                tpos = (c - 1792) / 64;
                if (tpos > ((2560 - 1792) / 64))
                    tpos = (2560 - 1792) / 64;
                te = extable+tpos;
                }
	    c -= te->count;
	    putcode(te);
        }
        tpos = c;
        if (tpos >= 64)
            tpos = 63;
        te = twtable+tpos;
        c -= te->count;
        putcode(te);
        if (c)
          putblackspan(0);
    } while (c);
}

static void
putblackspan(c)
    int c;
{
    int tpos;
    tableentry* te;

    do {
        if(c>=64) {
            if (c < 1792) {
                tpos = (c - 64) / 64;
                te = mbtable+tpos;
                }
            else {
                tpos = (c - 1792) / 64;
                if (tpos > ((2560 - 1792) / 64))
                    tpos = (2560 - 1792) / 64;
                te = extable+tpos;
                }
	    c -= te->count;
	    putcode(te);
        }
        tpos = c;
        if (tpos >= 64)
            tpos = 63;
        te = tbtable+tpos;
        c -= te->count;
        putcode(te);
        if (c)
          putwhitespan(0);
    } while (c);
}

static void
putcode(te)
    tableentry* te;
{
    unsigned int mask;
    int code;

    mask = 1<<(te->length-1);
    code = te->code;
    while(mask) {
 	if(code&mask)
	    putbit(1);
	else
	    putbit(0);
	mask >>= 1;
    }

}

static void
puteol()
{
    int i;

    for(i=0; i<11; ++i)
	putbit(0);
    putbit(1);
}

static int shdata;
static int shbit;

static void
putinit()
{
    shdata = 0;
    shbit = reversebits ? 0x01 : 0x80;
}

static void
putbit(d)
int d;
{
    if(d) 
	shdata = shdata|shbit;
    if ( reversebits )
	shbit = shbit<<1;
    else
	shbit = shbit>>1;
    if((shbit&0xff) == 0) {
	putchar(shdata);
	shdata = 0;
	shbit = reversebits ? 0x01 : 0x80;
    }
}

static void
flushbits( )
{
    if ( ( reversebits && shbit != 0x01 ) ||
	 ( ! reversebits && shbit != 0x80 ) ) {
	putchar(shdata);
	shdata = 0;
	shbit = reversebits ? 0x01 : 0x80;
    }
}
