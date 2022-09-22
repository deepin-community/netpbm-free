/* pgmslice.c - Extract one slice of pixel-values out of a portable graymap
 *
 * Copyright (C) 2000 Jos Dingjan <jos@tuatha.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define ROW     0  /* Extract a row of pixels */
#define COLUMN  1  /* Extract a column of pixels */

#include "pgm.h"

int main( int argc, char *argv[] ) {
  FILE *ifp;
  gray maxval;
  gray **grays;
  int rows, cols, line, col, colstart, collength, row, rowstart, rowlength;
  int line_type, count;
  char *usage = "-row|-col <line> [pgmfile]";

  pgm_init( &argc, argv );

  if (argc < 3 || argc > 4 )
    pm_usage( usage );

  if ( pm_keymatch( argv[1], "-row", 4) )
    line_type = ROW;
  else if ( pm_keymatch( argv[1], "-col", 4) )
    line_type = COLUMN;
  else
    pm_usage( usage );

  if (! sscanf( argv[2], "%d", &line ) )
    pm_usage( usage );

  if ( line < 0 )
    pm_error( "<line> is less than 0" );

  if (argc == 4 )
    ifp = pm_openr( argv[3] );
  else
    ifp = stdin;

  grays = pgm_readpgm( ifp, &cols, &rows, &maxval);

  if ( (line_type == COLUMN ) && (line >= cols) )
    pm_error( "<line> is too large -- the graymap has only %d columns", cols );
  if ( (line_type == ROW) && (line >= rows) )
    pm_error( "<line> is too large -- the graymap has only %d rows", rows );

  if ( line_type == COLUMN ) {
    colstart = line;
    collength = 1;
    rowstart = 0;
    rowlength = rows;
  }
  else if ( line_type == ROW ) {
    colstart = 0;
    collength = cols;
    rowstart = line;
    rowlength = 1;
  }

  count = 0;
  for ( row = rowstart; row < rowstart + rowlength; row++ ) {
    for ( col = colstart; col < colstart + collength; col++ ) {
      fprintf( stdout, "%d %u\n", count++, grays[row][col] );
    }
  }

  pm_close( ifp );
  pm_close( stdout );

  exit( 0 );
}
