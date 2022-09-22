/* ximtoppm.c - read an Xim file and produce a portable pixmap
**
** Copyright (C) 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#define _BSD_SOURCE 1
    /* This makes sure strdup() is in string.h */

#include <string.h>
#include "ppm.h"
#include "xim.h"
#include "shhopt.h"

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filename;
    char *alpha_filename;
    int alpha_stdout;
} cmdline;


static void
parse_command_line(int argc, char ** argv, struct cmdline_info *cmdline_p);
static int ReadXim ARGS(( FILE *in_fp, XimImage *xim ));
static int ReadXimHeader ARGS(( FILE *in_fp, XimImage *header ));
static int ReadXimImage ARGS(( FILE *in_fp, XimImage *xim ));
static int ReadImageChannel ARGS(( FILE *infp, byte *buf, unsigned int *bufsize, int encoded ));

int
main( argc, argv )
    int argc;
    char *argv[];
    {
    FILE *ifp, *imageout_file, *alpha_file;
    XimImage xim;
    pixel *pixelrow, colormap[256];
    gray *alpharow;
       /* The alpha channel of the row we're currently converting, in pgm fmt*/
    register pixel *pP;
    gray *aP;
    int rows, cols, row, mapped;
    register int col;
    pixval maxval;


    ppm_init( &argc, argv );

    parse_command_line(argc, argv, &cmdline);

    if ( cmdline.input_filename != NULL ) 
        ifp = pm_openr( cmdline.input_filename );
    else
        ifp = stdin;

    if (cmdline.alpha_stdout)
        alpha_file = stdout;
    else if (cmdline.alpha_filename == NULL) 
        alpha_file = NULL;
    else {
        alpha_file = pm_openw(cmdline.alpha_filename);
    }

    if (cmdline.alpha_stdout) 
        imageout_file = NULL;
    else
        imageout_file = stdout;


    if ( ! ReadXim( ifp, &xim ) )
	pm_error( "can't read Xim file" );
    rows = xim.height;
    cols = xim.width;
    if ( xim.nchannels == 1 && xim.bits_channel == 8 )
	{
	int i;

	mapped = 1;
	maxval = 255;
	for ( i = 0; i < xim.ncolors; ++i )
	    {
	    PPM_ASSIGN(
		colormap[i], xim.colors[i].red,
		xim.colors[i].grn, xim.colors[i].blu );
	    /* Should be colormap[xim.colors[i].pixel], but Xim is broken. */
	    }
	}
    else if ( xim.nchannels == 3 || xim.nchannels == 4 )
	{
	mapped = 0;
	maxval = pm_bitstomaxval( xim.bits_channel );
	}
    else
	pm_error(
	    "unknown Xim file type, nchannels == %d, bits_channel == %d",
	    xim.nchannels, xim.bits_channel );

    if (imageout_file) 
        ppm_writeppminit( imageout_file, cols, rows, maxval, 0 );
    if (alpha_file)
        pgm_writepgminit( alpha_file, cols, rows, maxval, 0 );

    pixelrow = ppm_allocrow( cols );
    alpharow = pgm_allocrow( cols );

    for ( row = 0; row < rows; ++row )
	{
	if ( mapped )
	    {
	    register byte *xbp;

	    for ( col = 0, pP = pixelrow,
		      xbp = xim.data + row * xim.bytes_per_line;
              col < cols;
              ++col, ++pP, ++xbp )
            *pP = colormap[*xbp];
            *aP = 0;
	    }
	else
	    {
	    register byte *xrbp, *xgbp, *xbbp;

	    for ( col = 0, pP = pixelrow, aP = alpharow,
		      xrbp = xim.data + row * xim.bytes_per_line,
		      xgbp = xim.grn_data + row * xim.bytes_per_line,
		      xbbp = xim.blu_data + row * xim.bytes_per_line;
              col < cols;
              ++col, ++pP, ++xrbp, ++xgbp, ++xbbp ) {
            PPM_ASSIGN( *pP, *xrbp, *xgbp, *xbbp );
            if (xim.nchannels > 3)
                *aP = *(xim.other + row * xim.bytes_per_line + col);
            else 
                *aP = 0;
        }
	    }
    if (imageout_file) 
        ppm_writeppmrow( imageout_file, pixelrow, cols, maxval, 0 );
    if (alpha_file)
        pgm_writepgmrow( alpha_file, alpharow, cols, maxval, 0 );
	}
    pm_close( ifp );
    if (imageout_file)
        pm_close( imageout_file );
    if (alpha_file)
        pm_close( alpha_file );
    exit( 0 );
    }

/* The rest is excerpted and slightly modified from the X.V11R4 version
** of xim_io.c.
*/

/***********************************************************************
*  File:   xlib.c
*  Author: Philip Thompson
*  $Date: 2003/08/12 18:23:03 $
*  $Revision: 1.1.1.1 $
*  Purpose: General xim libray of utililities
*  Copyright (c) 1988  Philip R. Thompson
*                Computer Resource Laboratory (CRL)
*                Dept. of Architecture and Planning
*                M.I.T., Rm 9-526
*                Cambridge, MA  02139
*   This  software and its documentation may be used, copied, modified,
*   and distributed for any purpose without fee, provided:
*       --  The above copyright notice appears in all copies.
*       --  This disclaimer appears in all source code copies.
*       --  The names of M.I.T. and the CRL are not used in advertising
*           or publicity pertaining to distribution of the software
*           without prior specific written permission from me or CRL.
*   I provide this software freely as a public service.  It is NOT a
*   commercial product, and therefore is not subject to an an implied
*   warranty of merchantability or fitness for a particular purpose.  I
*   provide it as is, without warranty.
*   This software is furnished  only on the basis that any party who
*   receives it indemnifies and holds harmless the parties who furnish
*   it against any claims, demands, or liabilities connected with using
*   it, furnishing it to others, or providing it to a third party.
*
*   Philip R. Thompson (phils@athena.mit.edu)
***********************************************************************/

static int
ReadXim(in_fp, xim)
    FILE *in_fp;
    XimImage *xim;
{
    if (!ReadXimHeader(in_fp, xim)) {
        pm_message("can't read xim header" );
	return(0);
    }
    if (!ReadXimImage(in_fp, xim)) {
        pm_message("can't read xim data" );
	return(0);
    }
    return(1);
}

static int
ReadXimHeader(in_fp, header)
    FILE *in_fp;
    XimImage  *header;
{
    int  i;
    char *cp;
    XimAsciiHeader  a_head;

    cp = (char *) header;
    for (i = 0; i < sizeof(XimImage); ++i )
	*cp++ = 0;
    /* Read header and verify image file formats */
    if (fread((char *)&a_head, sizeof(ImageHeader), 1, in_fp) != 1) {
        pm_message("ReadXimHeader: unable to read file header" );
        return(0);
    }
    if (atoi(a_head.header_size) != sizeof(ImageHeader)) {
        pm_message("ReadXimHeader: header size mismatch" );
        return(0);
    }
    if (atoi(a_head.file_version) != IMAGE_VERSION) {
        pm_message("ReadXimHeader: incorrect Image_file version" );
        return(0);
    }
    header->width = atoi(a_head.image_width);
    header->height = atoi(a_head.image_height);
    header->ncolors = atoi(a_head.num_colors);
    header->nchannels = atoi(a_head.num_channels);
    header->bytes_per_line = atoi(a_head.bytes_per_line);
/*    header->npics = atoi(a_head.num_pictures);
*/
    header->bits_channel = atoi(a_head.bits_per_channel);
    header->alpha_flag = atoi(a_head.alpha_channel);
    if (strlen(a_head.author)) {
    	overflow_add(strlen(a_head.author),1);
        if (!(header->author = calloc((unsigned int)strlen(a_head.author)+1,
                1))) {
            pm_message("ReadXimHeader: can't calloc author string" );
            return(0);
        }
    header->width = atoi(a_head.image_width);
        strncpy(header->author, a_head.author, strlen(a_head.author));
    }
    if (strlen(a_head.date)) {
        overflow_add(strlen(a_head.date),1);
        if (!(header->date =calloc((unsigned int)strlen(a_head.date)+1,1))){
            pm_message("ReadXimHeader: can't calloc date string" );
            return(0);
        }
    header->width = atoi(a_head.image_width);
        strncpy(header->date, a_head.date, strlen(a_head.date));
    }
    if (strlen(a_head.program)) {
        overflow_add(strlen(a_head.program),1);
        if (!(header->program = calloc(
                    (unsigned int)strlen(a_head.program) + 1, 1))) {
            pm_message("ReadXimHeader: can't calloc program string" );
            return(0);
        }
    header->width = atoi(a_head.image_width);
        strncpy(header->program, a_head.program,strlen(a_head.program));
    }
    /* Do double checking for bakwards compatibility */
    if (header->npics == 0)
        header->npics = 1;
    if (header->bits_channel == 0)
        header->bits_channel = 8;
    else if (header->bits_channel == 24) {
        header->nchannels = 3;
        header->bits_channel = 8;
    }
    if ((int)header->bytes_per_line == 0)
        header->bytes_per_line = 
            (header->bits_channel == 1 && header->nchannels == 1) ?
                (header->width + 7) / 8 :
                header->width;
    header->datasize =(unsigned int)header->bytes_per_line * header->height;
    if (header->nchannels == 3 && header->bits_channel == 8)
        header->ncolors = 0;
    else if (header->nchannels == 1 && header->bits_channel == 8) {
	overflow2(header->ncolors, sizeof(Color));
        header->colors = (Color *)calloc((unsigned int)header->ncolors,
                sizeof(Color));
        if (header->colors == NULL) {
            pm_message("ReadXimHeader: can't calloc colors" );
            return(0);
        }
        for (i=0; i < header->ncolors; i++) {
            header->colors[i].red = a_head.c_map[i][0];
            header->colors[i].grn = a_head.c_map[i][1];
            header->colors[i].blu = a_head.c_map[i][2];
        }
    }
    return(1);
}

static int
ReadXimImage(in_fp, xim)
    FILE *in_fp;
    XimImage *xim;
{
    if (xim->data) {
        free((char *)xim->data);
        xim->data = (byte *)0;
    }
    if (xim->grn_data) {
        free((char *)xim->grn_data);
        xim->grn_data = (byte *)0;
    }
    if (xim->blu_data) {
        free((char *)xim->blu_data);
        xim->blu_data = (byte *)0;
    }
    if (xim->other) {
        free((char *)xim->other);
        xim->other = (byte *)0;
    }
    xim->npics = 0;
    if (!(xim->data = (byte *)calloc(xim->datasize, 1))) {
        pm_message("ReadXimImage: can't malloc pixmap data" );
        return(0);
    }
    if (!ReadImageChannel(in_fp, xim->data, &xim->datasize, 0)) {
        pm_message("ReadXimImage: end of the images" );
        return(0);
    }
    if (xim->nchannels == 3) {
        xim->grn_data = (byte *)malloc(xim->datasize);
        xim->blu_data = (byte *)malloc(xim->datasize);
        if (xim->grn_data == NULL || xim->blu_data == NULL) {
            pm_message("ReadXimImage: can't malloc rgb channel data" );
            free((char *)xim->data);
            if (xim->grn_data)  free((char *)xim->grn_data);
            if (xim->blu_data)  free((char *)xim->blu_data);
            xim->data = xim->grn_data = xim->blu_data = (byte*)0;
            return(0);
        }
        if (!ReadImageChannel(in_fp, xim->grn_data, &xim->datasize, 0))
            return(0);
        if (!ReadImageChannel(in_fp, xim->blu_data, &xim->datasize, 0))
            return(0);
    }
    if (xim->nchannels > 3) {
        /* In theory, this can be any fourth channel, but the only one we
           know about is an Alpha channel, so we'll call it that, even
           though we process it generically.
           */
        if ((xim->other = (byte *)malloc(xim->datasize)) == NULL) {
            pm_message("ReadXimImage: can't malloc alpha data" );
            return(0);
        }
        if (!ReadImageChannel(in_fp, xim->other, &xim->datasize, 0))
            return(0);
    }
    xim->npics = 1;
    return(1);
}

static int
ReadImageChannel(infp, buf, bufsize, encoded)
FILE *infp;
    byte  *buf;
    unsigned int  *bufsize;
    int  encoded;
{
    register int  i, runlen, nbytes;
    register unsigned int  j;
    register byte *line;
    long  marker;

    if (!encoded)
        j = fread((char *)buf, 1, (int)*bufsize, infp);
    else {
        if ((line=(byte *)malloc((unsigned int)BUFSIZ)) == NULL) {
            pm_message("ReadImageChannel: can't malloc() fread string" );
            return(0);
        }
        /* Unrunlength encode data */
        marker = ftell(infp);
        j = 0;
        while (((nbytes=fread((char *)line, 1, BUFSIZ, infp)) > 0) &&
            (j < *bufsize)) {
            for (i=0; (i < nbytes) && (j < *bufsize); i++) {
                runlen = (int)line[i]+1;
                i++;
                while (runlen--)
                    buf[j++] = line[i];
            }
            marker += i;
        }
        /* return to the begining of the next image's bufffer */
        if (fseek(infp, marker, 0) == -1) {
            pm_message("ReadImageChannel: can't fseek to location in image buffer" );
            return(0);
        }
        free((char *)line);
    }
    if (j != *bufsize) {
        pm_message("unable to complete channel: %u / %u (%d%%)",
            j, *bufsize, (int)(j*100.0 / *bufsize) );
        *bufsize = j;
    }
    return(1);
}



static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that many of the strings that this function returns in the
   *cmdline_p structure are actually in the supplied argv array.  And
   sometimes, one of these strings is actually just a suffix of an entry
   in argv!
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions on how to parse our options.
         */
    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY(0,   "alphaout",   OPT_STRING, &cmdline_p->alpha_filename, 0);

    /* Set the defaults */
    cmdline_p->alpha_filename = NULL;

    pm_optParseOptions(&argc, argv, option_def, 0);
        /* Uses and sets argc, argv, and all of *cmdline_p. */

    if (argc - 1 == 0)
        cmdline_p->input_filename = NULL;  /* he wants stdin */
    else if (argc - 1 == 1)
        cmdline_p->input_filename = strdup(argv[1]);
    else 
        pm_error("Too many arguments.  The only argument accepted\n"
                 "is the input file specificaton");

    if (cmdline_p->alpha_filename && 
        strcmp(cmdline_p->alpha_filename, "-") == 0 )
        cmdline_p->alpha_stdout = 1;
    else 
        cmdline_p->alpha_stdout = 0;
}
