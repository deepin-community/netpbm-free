/*
 *  display.h
 *
 *  Written by:		Ullrich Hafner
 *		
 *  This file is part of FIASCO («F»ractal «I»mage «A»nd «S»equence «CO»dec)
 *  Copyright (C) 1994-2000 Ullrich Hafner <hafner@bigfoot.de>
 */

/*
 *  $Date: 2003/08/12 18:23:03 $
 *  $Author: aba-guest $
 *  $Revision: 1.1.1.1 $
 *  $State: Exp $
 */

#ifndef _DISPLAY_H
#define _DISPLAY_H

#ifndef X_DISPLAY_MISSING

#include <X11/Xlib.h>

#include "types.h"
#include "image.h"

typedef struct x11_info
{
   Display *display;
   int	    screen;			/* default screen number */
   Window   window;			
   XImage  *ximage;
   GC	    gc;
   byte_t  *pixels;
} x11_info_t;

void
display_image (unsigned x0, unsigned y0, x11_info_t *xinfo);
void
close_window (x11_info_t *xinfo);
x11_info_t *
open_window (const char *titlename, const char *iconname,
	     unsigned width, unsigned height);
void
alloc_ximage (x11_info_t *xinfo, unsigned width, unsigned height);

#endif /* X_DISPLAY_MISSING */

#endif /* not _DISPLAY_H */
