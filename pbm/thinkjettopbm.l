%pointer
/*
 * $Id: thinkjettopbm.l,v 1.2 2003/08/14 19:38:51 aba-guest Exp $
 *
 * Simple FLEX scanner to convert HP ThinkJet graphics image
 * to PBM format.
 *
 * Implements a small subset of ThinkJet commands.
 *
 * Copyright (C) 2001 by W. Eric Norum
 *
 * Department of Electrical Engineering
 * University of Saskatchewan
 * Saskatoon, Saskatchewan, CANADA
 * eric.norum@usask.ca
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and
 *  that both that copyright notice and this permission notice appear in
 *  supporting documentation.  This software is provided "as is" without
 *  express or implied warranty.
 *
 *  Modified 2001.04.05 by Bryan Henderson for inclusion in the Netpbm
 *  package.  Now uses Netpbm libraries and, for consistency with other
 *  Netpbm programs, does not have PGM output option.
 */

%{

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "pbm.h"

static int yylex(void);
static int yywrap(void);

struct RowInfo {
    int     length;    /* length, in bytes */
    char    *bits;     /* Bitmap */
};

static int maxRowLength;
static int rowCount;
static int rowCapacity;
static struct RowInfo *rows;

static int column;

const char *progname;
int debugFlag;
static void debug (const char *format, ...);

%}

DIG             [0-9]

%x RASTERMODE ROWMODE

%%

<ROWMODE>[\0-\377]      {
                        rows[rowCount].bits[column++] = yytext[0]; 
                        if (column >= rows[rowCount].length) {
                            rowCount++;
                            debug ("Done %d-byte row %d.\n", column, rowCount);
                            BEGIN (RASTERMODE);
                        }
                        }

<RASTERMODE>\033\*b{DIG}+W  {
                            int l;
                            if (rowCount >= rowCapacity) {
				overflow_add(rowCapacity, 100);
                                rowCapacity += 100;
				overflow2(rowCapacity, sizeof *rows);
                                rows = realloc (rows, rowCapacity * sizeof *rows);
                                if (rows == NULL)
                                    pm_error ("Out of memory.");
                            }
                            l = atoi (yytext+3);
                            rows[rowCount].length = l;
                            rows[rowCount].bits = malloc (l);
                            if (rows[rowCount].bits == NULL)
                                pm_error ("Out of memory.");
                            if (l > maxRowLength)
                                maxRowLength = l;
                            debug ("Start %d-byte row.\n", l);
                            column = 0;
                            BEGIN (ROWMODE);
                            }

<RASTERMODE>\033\*rB   {
                       debug ("Match <esc>*rB\n");
                       BEGIN (0);
                       }

<RASTERMODE>[.\0\n]    { pm_error ("Unexpected character (%#x) in raster mode.\n", yytext[0]); }

\033\&l{DIG}+.         { debug ("Match <esc>&l\n"); }
\033\*r{DIG}+S         { debug ("Match <esc>*r#S\n"); }
\033\*b{DIG}+W         { debug ("Match <esc>*r#w\n"); }
\033\*rA               {
                       debug ("Match <esc>*rA\n");
                       BEGIN (RASTERMODE);
                       }

[\0-\377]               { /* Silently consume all other characters */ }

%%

/*
 * Application entry point
 */
int
main (int argc, char **argv)
{
    int c;
    char *cp;
    int bad = 0;
    extern int optind;

    pbm_init( &argc, argv );

    if ((cp = strrchr (argv[0], '/')) != NULL)
        progname = cp + 1;
    else
        progname = argv[0];
    while ((c = getopt (argc, argv, "d")) != EOF) {
        switch (c) {
        case 'd':
            debugFlag++;
            break;

        default:
            bad = 1;
            break;
        }
    }
    if (optind == (argc - 1)) {
        if (freopen (argv[optind], "rb", stdin) == NULL) {
            pm_error ("Can't open `%s'.  Errno = %d (%s).\n", 
                      argv[optind], errno, strerror (errno));
        }
    }
    else if (optind != argc)
        bad = 1;
    if (bad) {
        pm_error ("Usage: %s [-d] [thinkjet_file]\n", progname);
    }
    yylex ();
    return 0;
}

/*
 * Finish at end of file
 */
static int 
yywrap (void)
{
    int row;
    unsigned char * packed_bitrow;

    debug ("Got %d rows, %d columns\n", rowCount, maxRowLength);

    /*
     * Quite simple since ThinkJet bit arrangement matches PBM
     */

    overflow2(maxRowLength, 8);
    pbm_writepbminit(stdout, maxRowLength*8, rowCount, 0);

    packed_bitrow = malloc(maxRowLength);
    if (packed_bitrow == NULL) pm_error("Out of memory");

    for (row = 0 ; row < rowCount ; row++) {
        int col;
        for (col = 0 ; col < rows[row].length ; col++) 
            packed_bitrow[col] = rows[row].bits[col];
        for (        ; col < maxRowLength;      col++)
            packed_bitrow[col] = 0;
        pbm_writepbmrow_packed(stdout, packed_bitrow, maxRowLength*8, 0);
    }
    free(packed_bitrow);
    return 1;
}

/*
 * Print debugging message
 */
static void
debug (const char *format, ...)
{
    va_list args;

    if (debugFlag) {
        fprintf (stderr, "%s: ", progname);
        va_start (args, format);
        vfprintf (stderr, format, args);
        va_end (args);
    }
}

