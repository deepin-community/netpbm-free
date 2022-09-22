/*
 * st4topgm.c
 * Justin Pryzby <justinpryzby@users.sf.net>
 * Mon Dec 15 17:51:29 EST 2003
 *
 * Convert an image from an SBIG ST-4 astronomical CCD camera to pgm.
 * See [http://www.sbig.com/].  Use sbigtopgm for all other SBIG
 * cameras.
 *
 * Copyright (C) 2003 by Justin Pryzby <justinpryzby@users.sf.net>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  This software is provided "as is" without express or
 * implied warranty.
 */

#include <string.h>
#include "pgm.h"

/* 192x166, a 192*165 image plus a "line" of metadata. */
#define	FSIZE	31872
#define	ROWS	165
#define	COLS	192
#define	MAXVAL	255

int main(int argc, char **argv);
void read_foot(FILE *fp);
void trim_comment0(char *comment);
void trim_comment1(char *comment);

int main(int argc, char **argv)
{
	FILE *fp;
	int row,col;
	gray *grayrow;

	pgm_init(&argc, argv);

	if (argc>2) pm_usage("[st4file]");
	else if (argc==1) fp=pm_openr_seekable("-");
	else fp=pm_openr_seekable(argv[1]);

	fseek(fp, 0, SEEK_END);
	if (ftell(fp)!=FSIZE) pm_error("Incorrect file size.");

	pgm_writepgminit(stdout, COLS, ROWS, MAXVAL, 0);
	grayrow = pgm_allocrow(COLS);

	fseek(fp, 31680, SEEK_SET);
	read_foot(fp);

	/*
	 * Okay, the file is of the correct size, and read_foot has
	 * confirmed the constant-position, constant value byte.  We
	 * accept the file as valid.
	 */
	fseek(fp, 0, SEEK_SET);

	/* Data. */
	for (row=0; row<ROWS; row++) {
		for (col=0; col<COLS; col++) {
			grayrow[col]=fgetc(fp);
		}

		pgm_writepgmrow(stdout, grayrow, COLS, MAXVAL, 0);
	}

	pgm_freerow(grayrow);
	
	pm_close(fp);
	pm_close(stdout);

	return 0;
}


/* 
 * Read the footer, which is stored in the following form.
 * 1:       'v'.
 * 2-79:    Freeform comment.
 * 80-89:   Exposure time in 1/100s of a second.
 * 90-99:   Focal length in inches.
 * 100-109: Aperture area in square inches.
 * 110-119: Calibration factor.
 * 120-192: Reserved.
 */
void read_foot(FILE *fp)
{
	char buf[192];

	fread(buf, 192, 1, fp);

	/*
	 * The only constant byte at a known constant offset, and thus
	 * serves as one of our two checks of the file type.  (The other
	 * check being the file size).
	 */
	if (buf[0]!='v') pm_error("Invalid file format.");

	memmove(buf, buf+1, 78);
	buf[78]=0;
	trim_comment0(buf);
	fprintf(stderr, "Comment:                 %80s\n", buf);

	memcpy(buf, buf+79, 10);
	buf[10]=0;
	trim_comment1(buf);
	fprintf(stderr, "Exposure time (1/100 s): %80s\n", buf);

	memcpy(buf, buf+89, 10);
	trim_comment1(buf);
	fprintf(stderr, "Focal length (in):       %80s\n", buf);

	memcpy(buf, buf+99, 10);
	trim_comment1(buf);
	fprintf(stderr, "Aperture area (sq in):   %80s\n", buf);

	memcpy(buf, buf+109, 10);
	trim_comment1(buf);
	fprintf(stderr, "Calibration factor:      %80s\n", buf);
}

/*
 * Trim up to 79 trailing spaces from the given string.
 */
void trim_comment0(char *comment)
{
	for ( ; comment[strlen(comment)-1]==' '; comment[strlen(comment)-1]=0);
}

/*
 * Trim up to 79 leading spaces from the given string.
 */
void trim_comment1(char *comment)
{
	for ( ; 32==*comment; memmove(comment, comment+1, strlen(comment)));
}
