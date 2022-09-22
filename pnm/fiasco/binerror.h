/*
 *  error.h
 *  
 *  Written by:		Stefan Frank
 *			Ullrich Hafner
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

#ifndef _ERROR_H
#define _ERROR_H

#define error          error_line=__LINE__,error_file=__FILE__,_error
#define warning        error_line=__LINE__,error_file=__FILE__,_warning
#define file_error(fn) error_line=__LINE__,error_file=__FILE__,_file_error(fn)

#ifdef _ERROR_C
#define _EXTERN_TYPE
#else
#define _EXTERN_TYPE	extern
#endif

_EXTERN_TYPE int   error_line;
_EXTERN_TYPE char *error_file;

void
init_error_handling (const char *name);
void
_error (const char *format, ...);
void
_warning (const char *format, ...);
void
_file_error (const char *filename);

#if HAVE_ASSERT_H
#	include <assert.h>
#else /* not HAVE_ASSERT_H */
#	define assert(exp)	{if (!(exp)) error ("Assertion `" #exp " != NULL' failed.");}
#endif /* not HAVE_ASSERT_H */

#endif /* not _ERROR_H */

