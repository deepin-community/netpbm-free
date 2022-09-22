/* pbmtoxbm.c - read a portable bitmap and produce an X11 bitmap file
**
** Copyright (C) 1988 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <string.h>
#include "pbm.h"


static void
generate_name(char filename_arg[], char * const name) {
/*----------------------------------------------------------------------------
   Generate a name for the image to put in the bitmap file.  Derive it from
   the filename argument filename_arg[] and return it as a null-terminated
   string in the space pointed to by 'name'.

   We take the part of the name after the rightmost slash
   (i.e. filename without the directory path part), stopping before
   any period.  We convert any punctuation to underscores.

   If the argument is "-", meaning standard input, we return the name
   "noname".  Also, if the argument is null or ends in a slash, we
   return "noname".
-----------------------------------------------------------------------------*/
  
  if (strcmp(filename_arg, "-") == 0) strcpy(name, "noname");
  else {
    int name_index, arg_index;
      /* indices into the input and output buffers */

    /* Start just after the rightmost slash, or at beginning if no slash */
    if (strrchr(filename_arg, '/') == 0) arg_index = 0;
    else arg_index = strrchr(filename_arg, '/') - filename_arg + 1;

    if (filename_arg[arg_index] == '\0') strcpy(name, "noname");
    else {
      name_index = 0;  /* Start at beginning of name buffer */
    
      while (filename_arg[arg_index] != '\0' 
             && filename_arg[arg_index] != '.') {
        const char filename_char = filename_arg[arg_index++];
        name[name_index++] = isalnum(filename_char) ? filename_char : '_';
      }
      name[name_index] = '\0';
    }
  }
}



int
main( argc, argv )
    int argc;
    char* argv[];
    {
    FILE* ifp;
    bit* bitrow;
    register bit* bP;
    int rows, cols, format, padright, row;
    register int col;
    char name[100];
    int itemsperline;
    register int bitsperitem;
    register int item;
    int firstitem;
    char* hexchar = "0123456789abcdef";


    pbm_init( &argc, argv );

    if ( argc > 2 )
        pm_usage( "[pbmfile]" );

    if ( argc == 2 )
        {
        ifp = pm_openr( argv[1] );
        generate_name(argv[1], name);
        }
    else
        {
        ifp = stdin;
        strcpy( name, "noname" );
        }

    pbm_readpbminit( ifp, &cols, &rows, &format );
    bitrow = pbm_allocrow( cols );
    
    /* Compute padding to round cols up to the nearest multiple of 8. */

    overflow_add(cols, 8);
    padright = ( ( cols + 7 ) / 8 ) * 8 - cols;

    printf( "#define %s_width %d\n", name, cols );
    printf( "#define %s_height %d\n", name, rows );
    printf( "static char %s_bits[] = {\n", name );

    itemsperline = 0;
    bitsperitem = 0;
    item = 0;
    firstitem = 1;

#define PUTITEM \
    { \
    if ( firstitem ) \
        firstitem = 0; \
    else \
        putchar( ',' ); \
    if ( itemsperline == 15 ) \
        { \
        putchar( '\n' ); \
        itemsperline = 0; \
        } \
    if ( itemsperline == 0 ) \
        putchar( ' ' ); \
    ++itemsperline; \
    putchar('0'); \
    putchar('x'); \
    putchar(hexchar[item >> 4]); \
    putchar(hexchar[item & 15]); \
    bitsperitem = 0; \
    item = 0; \
    }

#define PUTBIT(b) \
    { \
    if ( bitsperitem == 8 ) \
        PUTITEM; \
    if ( (b) == PBM_BLACK ) \
        item += 1 << bitsperitem; \
    ++bitsperitem; \
    }

    for ( row = 0; row < rows; ++row )
        {
        pbm_readpbmrow( ifp, bitrow, cols, format );
        for ( col = 0, bP = bitrow; col < cols; ++col, ++bP )
            PUTBIT(*bP);
        for ( col = 0; col < padright; ++col )
            PUTBIT(0);
        }

    if ( ifp != stdin )
        fclose( ifp );

    if ( bitsperitem > 0 )
        PUTITEM;
    printf( "};\n" );

    exit( 0 );
    }



/* CHANGE HISTORY

99.09.08 bryanh    In determining the image name, take only the filename,
                   not the whole filepath.  Also, convert any punctuation 
                   to underscores.  This avoids creating an invalid C
                   variable name, which can make the output unusable.


*/
