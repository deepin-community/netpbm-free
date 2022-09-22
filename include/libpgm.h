/* libpgm.h - internal header file for libpgm portable graymap library
*/

#ifndef _LIBPGM_H_
#define _LIBPGM_H_

/* Here are some routines internal to the pgm, ppm, and pnm libraries. */

void pgm_readpgminitrest(FILE* file, int* colsP, int* rowsP, gray* maxvalP);

gray pgm_getrawsample(FILE *file, const gray maxval);

void pgm_writerawsample(FILE *file, const gray val, const gray maxval);

#endif /*_LIBPGM_H_*/
