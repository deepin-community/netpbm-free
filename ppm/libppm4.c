/* libppm4.c - ppm utility library part 4
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
#include "ppm.h"

static void canonstr ARGS((char* str));
static void
canonstr( str )
    char* str;
{
    while ( *str != '\0' )
        {
	/* Modification by Arjen Bax, 2001-06-21: delete all whitespace,
	 * including carriage returns originating from MSDOS line ends.
	 */
	if ( isspace(*str) )
            {
            (void) strcpy( str, &(str[1]) );
            continue;
            }
        if ( isupper( *str ) )
            *str = tolower( *str );
        ++str;
        }
}


static FILE *
open_colorname_file(const int must_open) {
/*----------------------------------------------------------------------------
   Open the colorname database file.  Its pathname is the value of the
   environment variable whose name is RGB_ENV (e.g. "RGBDEF").  Except
   if that environment variable is not set, it is RGB_DB1, RGB_DB2,
   or RGB_DB3 (e.g. "/usr/lib/X11/rgb.txt"), whichever exists.
   
   'must_open' is a logical: we must get the file open or die.  If
   'must_open' is true and we can't open the file (e.g. it doesn't
   exist), exit the program with an error message.  If 'must_open' is
   false and we can't open the file, just return a null pointer.
-----------------------------------------------------------------------------*/
    const char *rgbdef;
    FILE *f;

    if ((rgbdef = getenv(RGBENV))==NULL) {
        /* The environment variable isn't set, so try the hardcoded
           default color names database locations.
        */
        if ((f = fopen(RGB_DB1, "r")) == NULL &&
            (f = fopen(RGB_DB2, "r")) == NULL &&
            (f = fopen(RGB_DB3, "r")) == NULL && must_open) {
            pm_error("can't open color names database file named "
                     "%s, %s, or %s "
                     "and Environment variable %s not set.  Set %s to "
                     "the pathname of your rgb.txt file or don't use "
                     "color names.", 
                     RGB_DB1, RGB_DB2, RGB_DB3, RGBENV, RGBENV);
        }
    } else {            
        /* The environment variable is set */
        if ((f = fopen(rgbdef, "r")) == NULL && must_open)
            pm_error("Can't open the color names database file named %s, "
                     "per the %s environment variable.",
                     rgbdef, RGBENV);
    }
    return(f);
}


FILE *
pm_openColornameFile(const int must_open) {
    return open_colorname_file(must_open);
}


struct colorfile_entry {
    long r, g, b;
    char * colorname;
};


static int
colorget(struct colorfile_entry * const ceP, FILE * const f) {
/*----------------------------------------------------------------------------
   Get next color entry from the color name database file 'f'.

   If eof or error, return FALSE;  Else, return TRUE.

   return color name in static storage within.
-----------------------------------------------------------------------------*/
    char buf[200];
    static char colorname[200];
    int got_one;
    
    got_one = FALSE;  /* initial value */
    while (!got_one && fgets(buf, sizeof(buf), f ) != NULL) {
        if (buf[0] != '\0' && buf[0] != '!' && buf[0] != '#') {
            if (sscanf(buf, "%ld %ld %ld %[^\n]", 
                       &ceP->r, &ceP->g, &ceP->b, colorname) 
                != 4 ) 
                pm_message("can't parse color names database line - \"%s\"", 
                           buf);
            else got_one = TRUE;
        }
    }
    ceP->colorname = colorname;
    return (got_one);
}

struct colorfile_entry
pm_colorget(FILE * const f) {
    struct colorfile_entry retval;
    if (!colorget(&retval, f))
	    retval.colorname=NULL;
    return retval;
}

long
pm_rgbnorm(const long rgb, const long lmaxval, const int n, 
        const char * const colorname) {
/*----------------------------------------------------------------------------
   Normalize the color (r, g, or b) value 'rgb', which was specified with
   'n' digits, to a maxval of 'lmaxval'.  If the number of digits isn't
   valid, issue an error message and identify the complete color 
   color specification in error as 'colorname'.

   For example, if the user says 0ff/000/000 and the maxval is 100,
   then rgb is 0xff, n is 3, and our result is 
   0xff / (16**3-1) * 100 = 6.

-----------------------------------------------------------------------------*/
    long retval;

    switch (n) {
    case 1:
        retval = (long)((double) rgb * lmaxval / 15 + 0.5);
        break;
    case 2:
        retval = (long) ((double) rgb * lmaxval / 255 + 0.5);
        break;
    case 3:
        retval = (long) ((double) rgb * lmaxval / 4095 + 0.5);
        break;
    case 4:
        retval = (long) ((double) rgb * lmaxval / 65535L + 0.5);
        break;
    default:
        pm_error( "invalid color specifier - \"%s\"", colorname );
    }
    return retval;
}

static long
rgbnorm(const long rgb, const long lmaxval, const int n, 
        const char * const colorname) {
    return pm_rgbnorm(rgb, lmaxval, n, colorname);
}


pixel
ppm_parsecolor( char* colorname, pixval maxval )
    {
    int hexit[256], i;
    pixel p;
    long lmaxval, r, g, b;
    char* inval = "invalid color specifier - \"%s\"";

    for ( i = 0; i < 256; ++i )
        hexit[i] = 1234567890;
    hexit['0'] = 0;
    hexit['1'] = 1;
    hexit['2'] = 2;
    hexit['3'] = 3;
    hexit['4'] = 4;
    hexit['5'] = 5;
    hexit['6'] = 6;
    hexit['7'] = 7;
    hexit['8'] = 8;
    hexit['9'] = 9;
    hexit['a'] = hexit['A'] = 10;
    hexit['b'] = hexit['B'] = 11;
    hexit['c'] = hexit['C'] = 12;
    hexit['d'] = hexit['D'] = 13;
    hexit['e'] = hexit['E'] = 14;
    hexit['f'] = hexit['F'] = 15;

    lmaxval = maxval;
    if ( strncmp( colorname, "rgb:", 4 ) == 0 )
        {
        /* It's a new-X11-style hexadecimal rgb specifier. */
        char* cp;

        cp = colorname + 4;
        r = g = b = 0;
        for ( i = 0; *cp != '/'; ++i, ++cp )
            r = r * 16 + hexit[(int)*cp];
        r = rgbnorm( r, lmaxval, i, colorname );
        for ( i = 0, ++cp; *cp != '/'; ++i, ++cp )
            g = g * 16 + hexit[(int)*cp];
        g = rgbnorm( g, lmaxval, i, colorname );
        for ( i = 0, ++cp; *cp != '\0'; ++i, ++cp )
            b = b * 16 + hexit[(int)*cp];
        b = rgbnorm( b, lmaxval, i, colorname );
        if ( r < 0 || r > lmaxval || g < 0 || g > lmaxval 
             || b < 0 || b > lmaxval )
            pm_error( inval, colorname );
        }
    else if ( strncmp( colorname, "rgbi:", 5 ) == 0 )
        {
        /* It's a new-X11-style decimal/float rgb specifier. */
        float fr, fg, fb;

        if ( sscanf( colorname, "rgbi:%f/%f/%f", &fr, &fg, &fb ) != 3 )
            pm_error( inval, colorname );
        if ( fr < 0.0 || fr > 1.0 || fg < 0.0 || fg > 1.0 
             || fb < 0.0 || fb > 1.0 )
            pm_error( "invalid color specifier - \"%s\" - "
                      "values must be between 0.0 and 1.0", colorname );
        r = fr * lmaxval;
        g = fg * lmaxval;
        b = fb * lmaxval;
        }
    else if ( colorname[0] == '#' )
        {
        /* It's an old-X11-style hexadecimal rgb specifier. */
        switch ( strlen( colorname ) - 1 /* (Number of hex digits) */ )
            {
            case 3:
            r = hexit[(int)colorname[1]];
            g = hexit[(int)colorname[2]];
            b = hexit[(int)colorname[3]];
            r = rgbnorm( r, lmaxval, 1, colorname );
            g = rgbnorm( g, lmaxval, 1, colorname );
            b = rgbnorm( b, lmaxval, 1, colorname );
            break;

            case 6:
            r = ( hexit[(int)colorname[1]] << 4 ) + hexit[(int)colorname[2]];
            g = ( hexit[(int)colorname[3]] << 4 ) + hexit[(int)colorname[4]];
            b = ( hexit[(int)colorname[5]] << 4 ) + hexit[(int)colorname[6]];
            r = rgbnorm( r, lmaxval, 2, colorname );
            g = rgbnorm( g, lmaxval, 2, colorname );
            b = rgbnorm( b, lmaxval, 2, colorname );
            break;

            case 9:
            r = ( hexit[(int)colorname[1]] << 8 ) +
              ( hexit[(int)colorname[2]] << 4 ) +
                hexit[(int)colorname[3]];
            g = ( hexit[(int)colorname[4]] << 8 ) + 
              ( hexit[(int)colorname[5]] << 4 ) +
                hexit[(int)colorname[6]];
            b = ( hexit[(int)colorname[7]] << 8 ) + 
              ( hexit[(int)colorname[8]] << 4 ) +
                hexit[(int)colorname[9]];
            r = rgbnorm( r, lmaxval, 3, colorname );
            g = rgbnorm( g, lmaxval, 3, colorname );
            b = rgbnorm( b, lmaxval, 3, colorname );
            break;

            case 12:
            r = ( hexit[(int)colorname[1]] << 12 ) + 
              ( hexit[(int)colorname[2]] << 8 ) +
                ( hexit[(int)colorname[3]] << 4 ) + hexit[(int)colorname[4]];
            g = ( hexit[(int)colorname[5]] << 12 ) + 
              ( hexit[(int)colorname[6]] << 8 ) +
                ( hexit[(int)colorname[7]] << 4 ) + hexit[(int)colorname[8]];
            b = ( hexit[(int)colorname[9]] << 12 ) + 
              ( hexit[(int)colorname[10]] << 8 ) +
                ( hexit[(int)colorname[11]] << 4 ) + hexit[(int)colorname[12]];
            r = rgbnorm( r, lmaxval, 4, colorname );
            g = rgbnorm( g, lmaxval, 4, colorname );
            b = rgbnorm( b, lmaxval, 4, colorname );
            break;

            default:
            pm_error( inval, colorname );
            }
        if ( r < 0 || r > lmaxval || g < 0 || g > lmaxval 
             || b < 0 || b > lmaxval )
            pm_error( inval, colorname );
        }
    else if ( ( colorname[0] >= '0' && colorname[0] <= '9' ) ||
              colorname[0] == '.' )
        {
        /* It's an old-style decimal/float rgb specifier. */
        float fr, fg, fb;

        if ( sscanf( colorname, "%f,%f,%f", &fr, &fg, &fb ) != 3 )
            pm_error( inval, colorname );
        if ( fr < 0.0 || fr > 1.0 || fg < 0.0 || fg > 1.0 
             || fb < 0.0 || fb > 1.0 )
            pm_error( "invalid color specifier - \"%s\" - "
                      "values must be between 0.0 and 1.0", colorname );
        r = fr * lmaxval;
        g = fg * lmaxval;
        b = fb * lmaxval;
        }
    else
        {
        /* Must be a name from the X-style rgb file. */
        FILE* f;
        struct colorfile_entry colorfile_entry;
        int gotit;

        f = open_colorname_file(TRUE);  /* exits if error */
        canonstr( colorname );
        gotit = FALSE;
        while ( !gotit && colorget( &colorfile_entry, f ) ) {
            canonstr( colorfile_entry.colorname );
            if ( strcmp( colorname, colorfile_entry.colorname ) == 0 )
                gotit = TRUE;
        }
        fclose( f );

        if (!gotit)
            pm_error( "unknown color - \"%s\"", colorname );

        /* Rescale from [0..255] if necessary. */
        if ( lmaxval != 255 ) {
            r = colorfile_entry.r * lmaxval / 255;
            g = colorfile_entry.g * lmaxval / 255;
            b = colorfile_entry.b * lmaxval / 255;
        } else {
            r = colorfile_entry.r;
            g = colorfile_entry.g;
            b = colorfile_entry.b;
        }
        }
    
    PPM_ASSIGN( p, r, g, b );
    return p;
    }



#if __STDC__
char*
ppm_colorname( pixel* colorP, pixval maxval, int hexok )
#else /*__STDC__*/
char*
ppm_colorname( colorP, maxval, hexok )
    pixel* colorP;
    pixval maxval;
    int hexok;
#endif /*__STDC__*/
    {
    int r, g, b;
    FILE* f;
    static char colorname[200];

    if ( maxval == 255 )
        {
        r = PPM_GETR( *colorP );
        g = PPM_GETG( *colorP );
        b = PPM_GETB( *colorP );
        }
    else
        {
        r = (int) PPM_GETR( *colorP ) * 255 / (int) maxval;
        g = (int) PPM_GETG( *colorP ) * 255 / (int) maxval;
        b = (int) PPM_GETB( *colorP ) * 255 / (int) maxval;
        }

    f = open_colorname_file(!hexok);
    if (f != NULL) {
        int best_diff, this_diff;
        struct colorfile_entry ce;

        best_diff = 32767;
        while (colorget(&ce, f)) 
        {
            this_diff = 
                abs( r - ce.r ) + abs( g - ce.g ) + abs( b - ce.b );
            if ( this_diff < best_diff )
            {
                best_diff = this_diff;
                (void) strcpy( colorname, ce.colorname );
            }
        }
        (void) fclose( f );
        if ( best_diff != 32767 && ( best_diff == 0 || ! hexok ) )
            return colorname;
    }

    /* Color lookup failed, but caller is willing to take an X11-style
       hex specifier, so return that.
    */
    sprintf( colorname, "#%02x%02x%02x", r, g, b );
    return colorname;
    }




