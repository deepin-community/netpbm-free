/*
 *  dither.h
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

#ifndef _DITHER_H
#define _DITHER_H

typedef struct renderer_private
{
   int 	       	*Cr_r_tab, *Cr_g_tab, *Cb_g_tab, *Cb_b_tab;
   unsigned int *r_table, *g_table, *b_table, *y_table;
   bool_t	double_resolution;
} renderer_private_t;

#endif /* _DITHER_H */
