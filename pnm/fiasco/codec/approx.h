/*
 *  approx.h
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

#ifndef _APPROX_H
#define _APPROX_H

#include "types.h"
#include "cwfa.h"
#include "domain-pool.h"

real_t 
approximate_range (real_t max_costs, real_t price, int max_edges,
		   int y_state, range_t *range, domain_pool_t *domain_pool,
		   coeff_t *coeff, const wfa_t *wfa, const coding_t *c);

#endif /* not _APPROX_H */

