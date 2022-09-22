
/***************************************************************************

    PBMTOMDA: Convert portable bitmap to Microdesign area
    Copyright (C) 1999 John Elliott <jce@seasip.demon.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

#include "pbm.h"
#include <stdio.h>
#include <string.h>

/* I'm being somewhat conservative in the PBM -> MDA translation. I output 
 * only the MD2 format and don't allow RLE over the ends of lines.
 */

typedef unsigned char mdbyte;

static FILE *infile;
static mdbyte header[128];
static bit **data;
static mdbyte *mdrow;
static int bInvert = 0;
static int bScale  = 0;
static int nInRows, nInCols, nOutCols, nOutRows;

/* Encode 8 pixels as a byte */

static mdbyte encode(int row, int col)
{
	int n, mask = 0x80;
	mdbyte b = 0;

	for (n = 0; n < 8; n++)
	{
		if (data[row][col+n] == PBM_BLACK) b |= mask;
		mask = mask >> 1;
	}
	return bInvert ? b : ~b;
}

/* Translate a pbm to MD2 format, one row at a time */

static void do_translation()
{
	int x, x1, row, step;
	mdbyte b;

	step = bScale ? 2 : 1;

	for (row = 0; row < nOutRows; row+=step)
	{
		/* Encode image into non-compressed bitmap */
		for (x = 0; x < nOutCols; x++)
		{
			mdrow[x] = encode(row, x*8);
		}
		/* Encoded. Now RLE it */
		for (x = 0; x < nOutCols; )
		{
			b = mdrow[x];

			if (b != 0xFF && b != 0) /* Normal byte */
			{
				putchar(b);
				++x;
			}
			else	/* RLE a run of 0s or 0xFFs */
			{
				for (x1 = x; x1 < nOutCols; x1++)
				{
					if (mdrow[x1] != b) break;
					if (x1 - x > 256) break;
				}
				x1 -= x;	/* x1 = no. of repeats */
				if (x1 == 256) x1 = 0;
				putchar(b);
				putchar(x1);
				x += x1;		
			}	
		}
	}
}


static void usage(char *s)
{		 
	printf("pbmtomda v1.00, Copyright (C) 1999 John Elliott <jce@seasip.demon.co.uk>\n"
		 "This program is redistributable under the terms of the GNU General Public\n"
                 "License, version 2 or later.\n\n"
                 "Usage: %s [ -d ] [ -i ] [ -- ] [ infile ]\n\n"
                 "-d: Halve height (to compensate for the PCW aspect ratio)\n"
                 "-i: Invert colours\n"
                 "--: No more options (use if filename begins with a dash)\n",
		s);

	exit(0);
}

int main(int argc, char **argv)
{
	int n, optstop = 0;
	char *fname = NULL;

	pbm_init(&argc, argv);

	/* Output v2-format MDA images. Simulate MDA header... */                  
	strcpy((char*) header, ".MDA pbmtomda Unixv1.00\r\n  GPL  \r\n");

	for (n = 1; n < argc; n++)
	{
		if (argv[n][0] == '-' && !optstop)
		{	
			if (argv[n][1] == 'd' || argv[n][1] == 'D') bScale = 1;
			if (argv[n][1] == 'i' || argv[n][1] == 'I') bInvert = 1;
			if (argv[n][1] == 'h' || argv[n][1] == 'H') usage(argv[0]);
			if (argv[n][1] == '-' && argv[n][2] == 0 && !fname) 	/* "--" */
			{
				optstop = 1;
			}
			if (argv[n][1] == '-' && (argv[n][2] == 'h' || argv[n][2] == 'H')) usage(argv[0]);
		}
		else if (argv[n][0] && !fname)	/* Filename */
		{
			fname = argv[n];
		}
	}

	if (fname) infile = pm_openr(fname);
	else       infile = stdin;

	data = pbm_readpbm(infile, &nInCols, &nInRows);

	if (!data)
	{
		fprintf(stderr,"%s: Cannot read input file.\n", argv[0]);
		exit(1);
	}

	if (bScale)	nOutRows = nInRows / 2;
	else		nOutRows = nInRows;

	overflow_add(nOutRows, 3);
	nOutRows = ((nOutRows + 3) / 4) * 4;
					 /* MDA wants rows a multiple of 4 */	
	nOutCols = nInCols / 8;

	if (fwrite(header, 1, 128, stdout) < 128)
	{
		perror(argv[0]);
		exit(2);
	}

	pm_writelittleshort(stdout, nOutRows);
	pm_writelittleshort(stdout, nOutCols);

	mdrow = malloc(nOutCols);

	if (!mdrow)
	{	
		if (data) pbm_freearray(data, nInRows);
		pm_error("Not enough memory for conversion.\n");
	}

	do_translation();

	if (infile != stdin) pm_close(infile);
	fflush(stdout);
	pbm_freearray(data, nInRows);
	free(mdrow);

	return 0;
}
