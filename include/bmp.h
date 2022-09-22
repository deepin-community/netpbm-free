/*\
 * $Id: bmp.h,v 1.2 2003/08/16 20:10:09 aba-guest Exp $
 * 
 * bmp.h - routines to calculate sizes of parts of BMP files
 *
 * Some fields in BMP files contain offsets to other parts
 * of the file.  These routines allow us to calculate these
 * offsets, so that we can read and write BMP files without
 * the need to fseek().
 * 
 * Copyright (C) 1992 by David W. Sanderson.
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  This software is provided "as is"
 * without express or implied warranty.
 * 
 * $Log: bmp.h,v $
 * Revision 1.2  2003/08/16 20:10:09  aba-guest
 * source should now be 9.25 upstream
 *
 * Revision 1.1.1.1  2003/08/12 18:23:03  aba-guest
 * Latest debian release
 *
 * Revision 1.3  1992/11/24  19:39:56  dws
 * Added copyright.
 *
 * Revision 1.2  1992/11/17  02:13:37  dws
 * Adjusted a string's name.
 *
 * Revision 1.1  1992/11/16  19:54:44  dws
 * Initial revision
 *
\*/

/* There is a specification of the Windows BMP format in  (2000.06.08)
   
   <http://www.daubnet.com/formats/BMP.html>

   However, the format we have seen for Windows BMP files with 24 bit
   truecolor is quite a bit different, and zgv seems to know it too:

   The raster is 3 bytes per pixel, in the order G, B, R with each
   row padded out to a multiple of 4 bytes.  The above document says
   it is 4 bytes per pixel, in the order R, G, B, Z (zero).

   And the colormap format is also different: Each color entry is in
   the order B, G, R, Z whereas the document says R, G, B, Z.

   The ColorsImportant field is defined in that spec as "Number of
   important colors.  0 = all"  That is the entire definition.  The
   spec also says the number of entries in the color map is a function
   of the BitCount field alone.

   But Marc Moorcroft says (2000.07.23) that he found two BMP files
   some time ago that had a color map whose number of entries was not
   as specified and was in fact the value of ColorsImportant.

   And Bill Janssen actually produced some BMPs in January 2001 that
   appear to have the size of the colormap determined by ColorsUsed.
   They have 8 bits per pixel in the raster, but ColorsUsed is 4 and
   there are in fact 4 entries in the color map.  He got these from
   the Palm emulator for Windows, using the "Save Screen" menu 
   option.

   Bmptoppm had, for a few releases in 2000, code by Marc to use
   ColorsImportant as the color map size unless it was zero, in which
   case it determined color map size as specified.  The current
   thinking is that there are probably more BMPs that need to be
   interpreted per the spec than that need to be interpreted Marc's
   way.  

   But in light of Janssen's discovery, we have made the assumption
   since February 2001 that when ColorsUsed is zero, the colormap size
   is as specified, and when it is nonzero, the colormap size is given
   by ColorsUsed.  

   Now we just have to wait for reports of BMPs for which this
   assumption is not true.

*/

#ifndef _BMP_H_
#define _BMP_H_

#include	"ppm.h"  /* For pm_error() */

enum bmpClass {C_WIN=1, C_OS2=2};

static char     er_internal[] = "%s: internal error!";

static unsigned int
BMPlenfileheader(enum bmpClass const class) {

    unsigned int retval;

	switch (class) {
	case C_WIN: retval = 14; break;
	case C_OS2: retval = 14; break;
    }
    return retval;
}



static unsigned int
BMPleninfoheader(enum bmpClass const class) {

    unsigned int retval;

	switch (class) {
	case C_WIN: retval = 40; break;
	case C_OS2: retval = 12; break;
    }
    return retval;
}



static unsigned int
BMPlencolormap(enum bmpClass const class, unsigned int const bitcount, 
               int const cmapsize) {

	unsigned int lenrgb;
    unsigned int lencolormap;

	if (bitcount < 1)
		pm_error(er_internal, "BMPlencolormap");
    else if (bitcount > 8) 
        lencolormap = 0;
    else {
        switch (class) {
        case C_WIN: lenrgb = 4; break;
        case C_OS2: lenrgb = 3; break;
        }

        if (cmapsize < 0) 
            lencolormap = (1 << bitcount) * lenrgb;
        else 
            lencolormap = cmapsize * lenrgb;
    }
    return lencolormap;
}

/*
 * length, in bytes, of a line of the image
 * 
 * Each row is padded on the right as needed to make it a
 * multiple of 4 bytes int.  This appears to be true of both
 * OS/2 and Windows BMP files.
 */
static unsigned int
BMPlenline(enum bmpClass const class, unsigned int const bitcount, 
           unsigned int const x) {

	unsigned int bitsperline;
    unsigned int retval;

	bitsperline = x * bitcount;

	/*
	 * if bitsperline is not a multiple of 32, then round
	 * bitsperline up to the next multiple of 32.
	 */
	if ((bitsperline % 32) != 0)
		bitsperline += (32 - (bitsperline % 32));

	if ((bitsperline % 32) != 0) {
		pm_error(er_internal, "BMPlenline");
		retval = 0;
	} else 
        /* number of bytes per line == bitsperline/8 */
        retval = bitsperline >> 3;

    return retval;
}

/* return the number of bytes used to store the image bits */
static unsigned int
BMPlenbits(enum bmpClass const class, unsigned int const bitcount, 
           unsigned int const x, unsigned int const y) {

	return y * BMPlenline(class, bitcount, x);
}



/* return the offset to the BMP image bits */
static unsigned int
BMPoffbits(enum bmpClass const class, unsigned int const bitcount, 
           unsigned int const cmapsize) {

	return BMPlenfileheader(class)
		+ BMPleninfoheader(class)
		+ BMPlencolormap(class, bitcount, cmapsize);
}


/* return the size of the BMP file in bytes */
static unsigned int
BMPlenfile(enum bmpClass const class, unsigned int const bitcount, 
           int const cmapsize, unsigned int const x, unsigned int const y) {

	return BMPoffbits(class, bitcount, cmapsize)
		+ BMPlenbits(class, bitcount, x, y);
}

#endif /* _BMP_H_ */
