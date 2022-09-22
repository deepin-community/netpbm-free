/*
 *  subdivide.h
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

#ifndef _SUBDIVIDE_H
#define _SUBDIVIDE_H

#include "types.h"
#include "cwfa.h"

real_t 
subdivide (real_t max_costs, unsigned band, int y_state, range_t *range,
	   wfa_t *wfa, coding_t *c, bool_t prediction, bool_t delta);
void
cut_to_bintree (real_t *dst, const word_t *src,
		unsigned src_width, unsigned src_height,
		unsigned x0, unsigned y0, unsigned width, unsigned height);

#endif /* not _SUBDIVIDE_H */


