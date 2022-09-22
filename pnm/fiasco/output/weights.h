/*
 *  weights.h
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

#ifndef _WEIGHTS_H
#define _WEIGHTS_H

#include "wfa.h"
#include "bit-io.h"

void
write_weights (unsigned total, const wfa_t *wfa, bitfile_t *output);

#endif /* not _WEIGHTS_H */

