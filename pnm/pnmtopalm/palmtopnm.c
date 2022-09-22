/* palmtopnm.c - read a pilot Tbmp file and write a Portable aNyMap
 *
 * Program by Ian Goldberg <iang@cs.berkeley.edu>, and 
 * Bill Janssen <bill@janssen.org>
 */

#include <string.h>

#include "pnm.h"

#include "palm.h"

static xelval *
figure_graymap (unsigned int ncolors, unsigned int minval, unsigned int maxval)
{
    int i;
    xelval *map;

    map = (xelval *) malloc2(sizeof(xelval), ncolors);
    for (i = 0;  i < ncolors;  i++)
    {
        map[i] = maxval - (i * (maxval - minval)) / (ncolors - 1);
    }
    return map;
}

static void skipbytes (FILE *f, unsigned int nbytes)
{
    unsigned char buf[256];
    unsigned int n = nbytes;
    while (n > 0) {
        if (n > sizeof(buf)) {
            fread(buf, sizeof(buf), 1, f);
            n -= sizeof(buf);
        } else {
            fread(buf, n, 1, f);
            n = 0;
        }
    }    
}

int
main( int argc, char **argv )
{
    FILE* ifp;
    short rows, cols, color, bytes_per_row, flags, nextDepthOffset, pad;
    int i,j;
    unsigned char *inbyte;
    unsigned char inbit;
    int format, version, pixelSize, transparentIndex, compressionType;
    xel *xelrow;
    unsigned char *Tbmprow, *lastrow;
    int argsGood = 1, verbose = 0, transparent = 0, forceplain = 0;
    int showhist = 0;
    int rle_compression = 0, scanline_compression = 0;
    int packbits_compression = 0;
    xelval *graymap;
    xelval mapped_color;
    int cc;
    unsigned int mask, ncolors, maxval, incount, inval;
    unsigned int redbits, greenbits, bluebits;
    unsigned int *seen = 0;
    unsigned int rendition = 1, rendition_counter, unread_header;
    Colormap colormap = 0;
    Color_s temp_color;
    Color actual_color;

    /* Parse default params */
    pnm_init( &argc, argv );

    for (cc = 1; (cc < argc) && (argv[cc][0] == '-'); cc++) {
        if (strcmp(argv[cc], "-verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[cc], "-forceplain") == 0) {
            forceplain = 1;
        } else if (strcmp(argv[cc], "-showhist") == 0) {
            showhist = 1;
        } else if ((strcmp(argv[cc], "-rendition") == 0) 
                   && ((cc + 1) < argc)) {
            cc++;
            rendition = atoi(argv[cc]);
            if (rendition < 1)
                pm_error("version specifier must be greater than 0");
        } else if (strcmp(argv[cc], "-transparent") == 0) {
            transparent = 1;
        } else {
            argsGood = 0;
        }
    }
    if (!argsGood) {
        pm_usage( " [-verbose] [-rendition N] "
                  "( [-showhist] [-forceplain] | -transparent ) [Tbmpfile]" );
    }

    if ( cc < argc )
        ifp = pm_openr( argv[cc] );
    else
        ifp = stdin;

    unread_header = 1;
    rendition_counter = 1;
    while (unread_header) {
        /* Read the bitmap header */
        pm_readbigshort( ifp, &cols );
        pm_readbigshort( ifp, &rows );
        pm_readbigshort( ifp, &bytes_per_row );
        pm_readbigshort( ifp, &flags );
        pixelSize = fgetc(ifp);
        version = fgetc(ifp);
        pm_readbigshort( ifp, &nextDepthOffset );
        if (rendition_counter < rendition) {
            if (nextDepthOffset == 0)
                pm_error("Not enough renditions in the input pixmap "
                         "to extract the %dth", rendition);
            rendition_counter += 1;
            skipbytes(ifp, nextDepthOffset);
            continue;
        } else {
            unread_header = 0;
        }
        transparentIndex = fgetc(ifp);
        compressionType = fgetc(ifp);
        pm_readbigshort(ifp, &pad);	/* reserved by Palm as of 8/9/00 */
    }

    if (verbose) {
        char *ctype;
        if (compressionType == PALM_COMPRESSION_RLE)
            ctype = "rle (Palm OS 3.5)";
        else if (compressionType == PALM_COMPRESSION_SCANLINE)
            ctype = "scanline (Palm OS 2.0)";
        else if (compressionType == PALM_COMPRESSION_PACKBITS)
            ctype = "packbits (Palm OS 4.0)";
        else if (compressionType == PALM_COMPRESSION_NONE)
            ctype = "none";
        else
            ctype = "unknown compression type";
        pm_message("cols %u, rows %u, bytes-per-row %u, "
                   "flags 0x%04x, %d bits-per-pixel, version %d,"
                   " nextDepthOffset 0x%x, transparentIndex %d, "
                   "compressionType %d (%s)",
                   (unsigned) cols, (unsigned) rows, 
                   (unsigned) bytes_per_row, (unsigned) flags,
                   (int) pixelSize, (int) version, (unsigned) nextDepthOffset,
                   (int) transparentIndex,
                   (int) compressionType, ctype);
    }

    if (flags & PALM_IS_COMPRESSED_FLAG) {
        if (compressionType == PALM_COMPRESSION_RLE)
            rle_compression = 1;
        else if (compressionType == PALM_COMPRESSION_SCANLINE)
            scanline_compression = 1;
        else if (compressionType == PALM_COMPRESSION_PACKBITS) {
            packbits_compression = 1;
            pm_message ("This program cannot yet decompress 'packbits' "
                        "compressed Palm bitmaps.");
            return 1;
        }
        else {
            pm_message ("The input is compressed with an unknown "
                        "compression type %d", compressionType);
            return 1;
        }
    } else if (compressionType != PALM_COMPRESSION_NONE) {
        pm_error("Bad compression type 0x%x; "
                 "the compression flag bit is not set", compressionType);
    }

    /* figure the maxval, colormap, and pnm format */
    if (flags & PALM_DIRECT_COLOR) {
        colormap = 0;
        format = PPM_TYPE;
        maxval = 255;
    } else if (flags & PALM_HAS_COLORMAP_FLAG) {
        colormap = palmcolor_read_colormap (ifp);
        format = PPM_TYPE;
        maxval = 255;
    } else if (pixelSize == 1) {
        colormap = 0;
        format = PBM_TYPE;
        maxval = 1;
    } else if (pixelSize >= 8) {
        colormap = palmcolor_build_default_8bit_colormap();
        qsort (colormap->color_entries, colormap->ncolors, 
               sizeof(Color_s), palmcolor_compare_indices);
        format = PPM_TYPE;
        maxval = (1 << pixelSize) - 1;
    } else {
        colormap = 0;
        format = PGM_TYPE;
        maxval = (1 << pixelSize) - 1;
    }

    if (flags & PALM_DIRECT_COLOR) {
        redbits = fgetc(ifp);
        greenbits = fgetc(ifp);
        bluebits = fgetc(ifp);
        pad = fgetc(ifp);
        pad = fgetc(ifp);
        temp_color = (fgetc(ifp) << 16) | (fgetc(ifp) << 8) | fgetc(ifp);
    }

    if (transparent) {
        if (flags & PALM_HAS_TRANSPARENCY_FLAG) {
            if (colormap != 0) {
                temp_color = transparentIndex << 24;
                actual_color = (bsearch (&temp_color,
                                         colormap->color_entries, 
                                         colormap->ncolors,
                                         sizeof(Color_s), 
                                         palmcolor_compare_indices));
                printf("#%02x%02x%02x\n", 
                       (unsigned int) ((*actual_color >> 16) & 0xFF),
                       (unsigned int) ((*actual_color >> 8) & 0xFF), 
                       (unsigned int) (*actual_color & 0xFF));
            } else if (flags & PALM_DIRECT_COLOR) {
                printf("#%02x%02x%02x\n", 
                       (unsigned int)((temp_color >> 16) & 0xFF), 
                       (unsigned int)((temp_color >> 8) & 0xFF), 
                       (unsigned int)(temp_color & 0xFF));
            } else {
                unsigned int grayval = 
                    ((maxval - transparentIndex) * 256) / maxval;
                printf("#%02x%02x%02x\n", grayval, grayval, grayval);
            }
        }
        return 0;
    }

    /* Write the pnm header */
    pnm_writepnminit(stdout, cols, rows, maxval, format, forceplain);
    xelrow = pnm_allocrow(cols);

    /* Read the picture data, one row at a time */
    Tbmprow = (unsigned char *)malloc(bytes_per_row);
    if (scanline_compression)
        lastrow = (unsigned char *)malloc(bytes_per_row);
    if (!Tbmprow) {
        pm_error("Unable to allocate %d bytes for one row of the Tbmp image", 
                 bytes_per_row);
    }
    ncolors = (1 << pixelSize);
    mask = (1 << pixelSize) - 1;
    if (colormap == 0)
        graymap = figure_graymap(ncolors, 0, maxval);
    if (showhist)
    {
        seen = (unsigned int *) malloc2(sizeof(unsigned int), ncolors);
        if (!seen)
            pm_error("Can't allocate %d bytes for keeping track of "
                     "which pixels were seen.", 
                     sizeof(unsigned int) * ncolors);
        memset(seen, 0, sizeof(unsigned int) * ncolors);
    }

    for (i=0;i<rows;++i) {
        if (rle_compression) {
            for (j = 0;  j < bytes_per_row;  ) {
                incount = fgetc(stdin);
                inval = fgetc(stdin);
                memset(Tbmprow + j, inval, incount);
                j += incount;
            }
        } else if (scanline_compression) {
            for (j = 0;  j < bytes_per_row;  j += 8) {
                incount = fgetc(stdin);
                inval = ((bytes_per_row - j) < 8) ? (bytes_per_row - j) : 8;
                for (inbit = 0;  inbit < inval;  inbit += 1) {
                    if (incount & (1 << (7 - inbit)))
                        Tbmprow[j + inbit] = fgetc(stdin);
                    else
                        Tbmprow[j + inbit] = lastrow[j + inbit];
                }
            }
            memcpy (lastrow, Tbmprow, bytes_per_row);
            if (verbose) {
                fprintf(stderr, "%4d:  ", i);
                for (j = 0;  j < bytes_per_row;  j++)
                    fprintf(stderr, "%02x ", Tbmprow[j]);
                fprintf(stderr, "\n");
                fflush(stderr);
            }
        } else {
            if (fread( Tbmprow, 1, bytes_per_row, ifp ) != bytes_per_row) {
                pm_error("Error reading Tbmp file");
            }
        }
        if (flags & PALM_DIRECT_COLOR) {

            /* There's a problem with this.  Take the Palm 16-bit
               direct color.  That's 5 bits for the red, 6 for the
               green, and 5 for the blue.  So what should the MAXVAL
               be?  I decided to use 255 (8 bits) for everything,
               since that's the theoretical max of the number of bits
               in any one color, according to Palm.  So the Palm color
               0xFFFF (white) would be red=0x1F, green=0x3F, and
               blue=0x1F.  How do we promote those colors?  Simple
               shift would give us R=248,G=252,B=248; which is
               slightly green.  Hardly seems right.

               So I've perverted the math a bit.  Each color value is
               multiplied by 255, then divided by either 31 (red or
               blue) or 63 (green).  That's the right way to do it
               anyway.  
            */

            if (pixelSize == 16) {
                for (inbyte = Tbmprow, j = 0;  j < cols;  ++j) {
                    inval = *inbyte++ << 8;
                    inval |= *inbyte++;
                    if (showhist)
                        seen[inval] += 1;

                    PPM_ASSIGN(xelrow[j], 
                               (((inval >> 11) & 0x1F) * maxval) / 31 , 
                               (((inval >> 5) & 0x3F) * maxval) / 63, 
                               ((inval & 0x1F) * maxval) / 31);
                }
            } else {
                pm_error ("Can convert only 16-bit direct color "
                          "Palm bitmaps.");
                return 1;
            }
        } else {
            inbit = 8 - pixelSize;
            inbyte = Tbmprow;
            for (j=0; j<cols; ++j) {
                color = ((*inbyte) & (mask << inbit)) >> inbit;
                if (showhist)
                    seen[color] += 1;
                if (colormap) {
                    temp_color = (color << 24);
                    actual_color = (bsearch (&temp_color,
                                             colormap->color_entries, 
                                             colormap->ncolors,
                                             sizeof(Color_s), 
                                             palmcolor_compare_indices));
                    PPM_ASSIGN(xelrow[j], 
                               (*actual_color >> 16) & 0xFF, 
                               (*actual_color >> 8) & 0xFF, 
                               (*actual_color & 0xFF));
                } else {
                    mapped_color = graymap[color];
                    PNM_ASSIGN1(xelrow[j], mapped_color);
                }
                if (!inbit) {
                    ++inbyte;
                    inbit = 8 - pixelSize;
                } else {
                    inbit -= pixelSize;
                }
            }
        }
        pnm_writepnmrow(stdout, xelrow, cols, maxval, format, forceplain);
    }

    pm_close( ifp );

    if (showhist)
    {
        int i;
        for (i = 0;  i < ncolors;  i++)
        {
            if (colormap == 0) {
                pm_message("%.3d -> %.3d:  %d", i, graymap[i], seen[i]);
            } else {
                temp_color = (i << 24);
                actual_color = (bsearch (&temp_color,
                                         colormap->color_entries, 
                                         colormap->ncolors,
                                         sizeof(Color_s), 
                                         palmcolor_compare_indices));
                if (actual_color != 0) {
                    pm_message("%.3d -> %ld,%ld,%ld:  %d", i,
                               (*actual_color >> 16) & 0xFF,
                               (*actual_color >> 8) & 0xFF,
                               (*actual_color & 0xFF), seen[i]);
                }
            }
        }
    }

    exit( 0 );
}
