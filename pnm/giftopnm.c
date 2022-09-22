/* +-------------------------------------------------------------------+ */
/* | Copyright 1990, 1991, 1993, David Koblas.  (koblas@netcom.com)    | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

/* There is a copy of the GIF89 specification, as defined by its
   inventor, Compuserve, in 1989, at http://members.aol.com/royalef/gif89a.txt
*/

#include <string.h>
#include "pnm.h"

#define GIFMAXVAL 255
#define MAXCOLORMAPSIZE 256

#define CM_RED 0
#define CM_GRN 1
#define CM_BLU 2

#define MAX_LZW_BITS  12

#define INTERLACE      0x40
#define LOCALCOLORMAP  0x80
#define BitSet(byte, bit)      (((byte) & (bit)) == (bit))

#define        ReadOK(file,buffer,len) (fread(buffer, len, 1, file) != 0)

#define LM_to_uint(a,b)                        (((b)<<8)|(a))

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filespec;  /* Filespecs of input files */
    int verbose;    /* -verbose option */
    int comments;   /* -comments option */
    int image_no;   /* image number he wants from input */
    char *alpha_filename;
    int alpha_stdout;
};


static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdlineP) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions2 on how to parse our options.
         */
    optStruct2 opt;

    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY(0,   "verbose",     OPT_FLAG,   &cmdlineP->verbose,         0);
    OPTENTRY(0,   "comments",    OPT_FLAG,   &cmdlineP->comments,        0);
    OPTENTRY(0,   "image",       OPT_UINT,   &cmdlineP->image_no,        0);
    OPTENTRY(0,   "alphaout",    OPT_STRING, &cmdlineP->alpha_filename, 0);

    /* Set the defaults */
    cmdlineP->verbose = FALSE;
    cmdlineP->comments = FALSE;
    cmdlineP->image_no = 1;
    cmdlineP->alpha_filename = NULL;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions2(&argc, argv, opt, 0);
        /* Uses and sets argc, argv, and some of *cmdlineP and others. */

    if (cmdlineP->image_no < 1)
        pm_error("Invalid -image option: %d.  Must be at least 1.",
                 cmdlineP->image_no);
    
    if (argc-1 == 0) 
        cmdlineP->input_filespec = "-";
    else if (argc-1 != 1)
        pm_error("Program takes zero or one argument (filename).  You "
                 "specified %d", argc-1);
    else
        cmdlineP->input_filespec = argv[1];

    if (cmdlineP->alpha_filename && 
        strcmp(cmdlineP->alpha_filename, "-") == 0 )
        cmdlineP->alpha_stdout = 1;
    else 
        cmdlineP->alpha_stdout = 0;
}


typedef unsigned char gifColorMap[3][MAXCOLORMAPSIZE];

struct gifScreen {
    unsigned int    Width;
    unsigned int    Height;
    gifColorMap     ColorMap;
    unsigned int    ColorMapSize;
    unsigned int    ColorResolution;
    unsigned int    Background;
    unsigned int    AspectRatio;
        /* Aspect ratio of each pixel, times 64, minus 15.  (i.e. 1 => 1:4).
           But Zero means 1:1.
        */
    int      hasGray;  
        /* Boolean: global colormap has at least one gray color
           (not counting black and white) 
        */
    int      hasColor;
        /* Boolean: global colormap has at least one non-gray,
           non-black, non-white color 
        */
};

struct gif89 {
       int     transparent;
       int     delayTime;
       int     inputFlag;
       int     disposal;
};

static void
initGif89(struct gif89 * const gif89P) {
    gif89P->transparent = -1;
    gif89P->delayTime = -1;
    gif89P->inputFlag = -1;
    gif89P->disposal = -1;
}       


static int verbose;
int    showComment;



static void
readColorMap(FILE *ifP, const int colormapsize, 
             unsigned char colormap[3][MAXCOLORMAPSIZE],
             int *hasGrayP, int * const hasColorP) {

    int             i;
    unsigned char   rgb[3];

    *hasGrayP = FALSE;  /* initial assumption */
    *hasColorP = FALSE;  /* initial assumption */

    for (i = 0; i < colormapsize; ++i) {
        if (! ReadOK(ifP, rgb, sizeof(rgb)))
            pm_error("Unable to read Color %d from colormap", i);

        colormap[CM_RED][i] = rgb[0] ;
        colormap[CM_GRN][i] = rgb[1] ;
        colormap[CM_BLU][i] = rgb[2] ;

        if (rgb[0] == rgb[1] && rgb[1] == rgb[2]) {
            if (rgb[0] != 0 && rgb[0] != GIFMAXVAL)
                *hasGrayP = TRUE;
        } else
            *hasColorP = TRUE;
    }
}



static bool zeroDataBlock = FALSE;
    /* the most recently read DataBlock was an EOD marker, i.e. had
       zero length */

static void
getDataBlock(FILE *          const ifP, 
             unsigned char * const buf, 
             bool *          const eofP,
             unsigned int *  const lengthP) {
/*----------------------------------------------------------------------------
   Read a DataBlock from file 'ifP', return it at 'buf'.

   The first byte of the datablock is the length, in pure binary, of the
   rest of the datablock.  We return the data portion (not the length byte)
   of the datablock at 'buf', and its length as *lengthP.

   Except that if we hit EOF or have an I/O error reading the first
   byte (size field) of the DataBlock, we return *eofP == TRUE and
   *lengthP == 0.

   We return *eofP == FALSE if we don't hit EOF or have an I/O error.

   If we hit EOF or have an I/O error reading the data portion of the
   DataBlock, we exit the program with pm_error().
-----------------------------------------------------------------------------*/
    unsigned char count;
    bool successfulRead;
    
    successfulRead = ReadOK(ifP, &count, 1);
    if (!successfulRead) {
        pm_message("EOF or error in reading DataBlock size from file" );
        *eofP = TRUE;
        *lengthP = 0;
    } else {
        *eofP = FALSE;
        *lengthP = count;

        if (count == 0) 
            zeroDataBlock = TRUE;
        else {
            bool successfulRead;

            zeroDataBlock = FALSE;
            successfulRead = ReadOK(ifP, buf, count); 
            
            if (!successfulRead) 
                pm_error("EOF or error reading data portion of %d byte "
                         "DataBlock from file", count);
        }
    }
}



static void
readThroughEod(FILE * const ifP) {
    unsigned char buf[260];
    bool eod;

    eod = FALSE;  /* initial value */
    while (!eod) {
        bool eof;
        unsigned int count;

        getDataBlock(ifP, buf, &eof, &count);
        if (eof)
            pm_message("EOF encountered before EOD marker.  The GIF "
                       "file is malformed, but we are proceeding "
                       "anyway as if an EOD marker were at the end "
                       "of the file.");
        if (eof || count == 0)
            eod = TRUE;
    }
}



static void
doCommentExtension(FILE * const ifP) {
/*----------------------------------------------------------------------------
   Read the rest of a comment extension from the input file 'ifP' and handle
   it.
   
   We ought to deal with the possibility that the comment is not text.  I.e.
   it could have nonprintable characters or embedded nulls.  I don't know if
   the GIF spec requires regular text or not.
-----------------------------------------------------------------------------*/
    char buf[255+1];
    unsigned int blocklen;  
    bool done;

    done = FALSE;
    while (!done) {
        bool eof;
        getDataBlock(ifP, (unsigned char*) buf, &eof, &blocklen); 
        if (blocklen == 0 || eof)
            done = TRUE;
        else {
            buf[blocklen] = '\0';
            if (showComment) {
                pm_message("gif comment: %s", buf);
            }
        }
    }
}



static void 
doGraphicControlExtension(FILE *         const ifP,
                          struct gif89 * const gif89P) {

    bool eof;
    unsigned int length;
    static unsigned char buf[256];

    getDataBlock(ifP, buf, &eof, &length);
    if (eof)
        pm_error("EOF/error encountered reading "
                 "1st DataBlock of Graphic Control Extension.");
    else if (length < 4) 
        pm_error("graphic control extension 1st DataBlock too short.  "
                 "It must be at least 4 bytes; it is %d bytes.",
                 length);
    else {
        gif89P->disposal = (buf[0] >> 2) & 0x7;
        gif89P->inputFlag = (buf[0] >> 1) & 0x1;
        gif89P->delayTime = LM_to_uint(buf[1],buf[2]);
        if ((buf[0] & 0x1) != 0)
            gif89P->transparent = buf[3];
        readThroughEod(ifP);
    }
}



static void
doExtension(FILE * const ifP, int const label, struct gif89 * const gif89P) {
    char * str;
    
    switch (label) {
    case 0x01:              /* Plain Text Extension */
        str = "Plain Text";
#ifdef notdef
        GetDataBlock(ifP, (unsigned char*) buf, &eof, &length);
        
        lpos   = LM_to_uint(buf[0], buf[1]);
        tpos   = LM_to_uint(buf[2], buf[3]);
        width  = LM_to_uint(buf[4], buf[5]);
        height = LM_to_uint(buf[6], buf[7]);
        cellw  = buf[8];
        cellh  = buf[9];
        foreground = buf[10];
        background = buf[11];
        
        while (GetDataBlock(ifP, (unsigned char*) buf) != 0) {
            PPM_ASSIGN(xels[ypos][xpos],
                       cmap[CM_RED][v],
                       cmap[CM_GRN][v],
                       cmap[CM_BLU][v]);
            ++index;
        }
#else
        readThroughEod(ifP);
#endif
        break;
    case 0xff:              /* Application Extension */
        str = "Application";
        readThroughEod(ifP);
        break;
    case 0xfe:              /* Comment Extension */
        str = "Comment";
        doCommentExtension(ifP);
        break;
    case 0xf9:              /* Graphic Control Extension */
        str = "Graphic Control";
        doGraphicControlExtension(ifP, gif89P);
        break;
    default: {
        static char buf[256];
        str = buf;
        sprintf(buf, "UNKNOWN (0x%02x)", label);
        pm_message("Ignoring unrecognized extension (type 0x%02x)", label);
        readThroughEod(ifP);
        }
        break;
    }
    if (verbose)
        pm_message(" got a '%s' extension", str );
}



static int
getCode(FILE * const ifP, 
        int    const codeSize, 
        bool   const first)
{
/*----------------------------------------------------------------------------
   If 'first', initialize the code getter.

   Otherwise, read and return the next lzw code from the file *ifP.
-----------------------------------------------------------------------------*/

    static unsigned char buf[280];
    static int           curbit, lastbit, last_byte;
    static bool          done;
    int retval;

    if (first) {
        /* Fake a previous data block */
        buf[0] = 0;
        buf[1] = 0;
        last_byte  = 2;
        curbit = 16;
        lastbit = 16;

        done = FALSE;
        retval = 0;
    } else {
        if ( (curbit+codeSize) >= lastbit) {
            unsigned int count;
            unsigned int assumed_count;
            bool eof;

            if (done) {
                if (curbit >= lastbit)
                    pm_error("ran off the end of my bits" );
                return -1;
            }
            buf[0] = buf[last_byte-2];
            buf[1] = buf[last_byte-1];

            getDataBlock(ifP, &buf[2], &eof, &count);
            if (eof) {
                pm_message("EOF encountered in image "
                           "before EOD marker.  The GIF "
                           "file is malformed, but we are proceeding "
                           "anyway as if an EOD marker were at the end "
                           "of the file.");
                assumed_count = 0;
            } else
                assumed_count = count;
            if (assumed_count == 0)
                done = TRUE;

            last_byte = 2 + assumed_count;
            curbit = (curbit - lastbit) + 16;
            lastbit = (2+assumed_count)*8 ;
        }

        retval = 0;
        {
            int i, j;
            for (i = curbit, j = 0; j < codeSize; ++i, ++j)
                retval |= ((buf[ i / 8 ] & (1 << (i % 8))) != 0) << j;
        }
        curbit += codeSize;
    }
    return retval;
}



static int
lzwReadByte(FILE * const ifP, bool const first, int const input_codeSize) {
/*----------------------------------------------------------------------------
  Return the next byte of the decompressed image.

  Return -1 if we hit EOF prematurely (i.e. before an "end" code.  We
  forgive the case that the "end" code is followed by EOF instead of
  an EOD marker (zero length DataBlock)).

  Return -2 if there are no more bytes in the image.
-----------------------------------------------------------------------------*/
    static int      fresh = FALSE;
    int             code, incode;
    static int      codeSize, set_codeSize;
    static int      max_code, max_codeSize;
    static int      firstcode, oldcode;
    static int      clear_code, end_code;
    static int      table[2][(1<< MAX_LZW_BITS)];
    static int      stack[(1<<(MAX_LZW_BITS))*2], *sp;

    if (first) {
        int    i;

        set_codeSize = input_codeSize;
        codeSize = set_codeSize+1;
        clear_code = 1 << set_codeSize ;
        end_code = clear_code + 1;
        max_codeSize = 2*clear_code;
        max_code = clear_code+2;

        getCode(ifP, 0, TRUE);
               
        fresh = TRUE;

        for (i = 0; i < clear_code; ++i) {
            table[0][i] = 0;
            table[1][i] = i;
        }
        for (; i < (1<<MAX_LZW_BITS); ++i)
            table[0][i] = table[1][i] = 0;

        sp = stack;

        return 0;
    } else if (fresh) {
        fresh = FALSE;
        do {
            firstcode = oldcode = getCode(ifP, codeSize, FALSE);
        } while (firstcode == clear_code);
        return firstcode;
    }

    if (sp > stack)
        return *--sp;

    while ((code = getCode(ifP, codeSize, FALSE)) >= 0) {
        if (code == clear_code) {
            int    i;
            for (i = 0; i < clear_code; ++i) {
                table[0][i] = 0;
                table[1][i] = i;
            }
            for (; i < (1<<MAX_LZW_BITS); ++i)
                table[0][i] = table[1][i] = 0;
            codeSize = set_codeSize+1;
            max_codeSize = 2*clear_code;
            max_code = clear_code+2;
            sp = stack;
            firstcode = oldcode = getCode(ifP, codeSize, FALSE);
            return firstcode;
        } else if (code == end_code) {
            if (zeroDataBlock)
                return -2;

            readThroughEod(ifP);
                       
            return -2;
        }

        incode = code;

        if (code >= max_code) {
            *sp++ = firstcode;
            code = oldcode;
        }

        while (code >= clear_code) {
            *sp++ = table[1][code];
            if (code == table[0][code])
                pm_error("circular table entry BIG ERROR");
            code = table[0][code];
        }

        *sp++ = firstcode = table[1][code];

        if ((code = max_code) <(1<<MAX_LZW_BITS)) {
            table[0][code] = oldcode;
            table[1][code] = firstcode;
            ++max_code;
            if ((max_code >= max_codeSize) &&
                (max_codeSize < (1<<MAX_LZW_BITS))) {
                max_codeSize *= 2;
                ++codeSize;
            }
        }

        oldcode = incode;

        if (sp > stack)
            return *--sp;
    }
    return code;
}



static void
readImageData(FILE * const ifP, 
              xel ** const xels, 
              int    const len, 
              int    const height, 
              gifColorMap  cmap, 
              bool   const interlace) {

    unsigned char lzwMinCodeSize;      
    int v;
    int xpos, ypos, pass;

    pass = 0;
    xpos = 0;
    ypos = 0;

    
    /*
    **  Initialize the Compression routines
    */
    if (! ReadOK(ifP,&lzwMinCodeSize,1))
        pm_error("GIF stream ends (or read error) "
                 "right after an image separator; no "
                 "image data follows.");

    if (lzwMinCodeSize > MAX_LZW_BITS)
        pm_error("Invalid minimum code size value in image data: %u.  "
                 "Maximum allowable code size in GIF is %u",
                 lzwMinCodeSize, MAX_LZW_BITS);

    if (lzwReadByte(ifP, TRUE, lzwMinCodeSize) < 0)
        pm_error("GIF stream ends (or read error) right after the "
                 "minimum lzw code size field; no image data follows.");

    if (verbose)
        pm_message("reading %d by %d%s GIF image",
                   len, height, interlace ? " interlaced" : "" );

    while ((v = lzwReadByte(ifP,FALSE,lzwMinCodeSize)) >= 0 ) {
        PPM_ASSIGN(xels[ypos][xpos], 
                   cmap[CM_RED][v], cmap[CM_GRN][v], cmap[CM_BLU][v]);

        ++xpos;
        if (xpos == len) {
            xpos = 0;
            if (interlace) {
                switch (pass) {
                case 0:
                case 1:
                    ypos += 8; break;
                case 2:
                    ypos += 4; break;
                case 3:
                    ypos += 2; break;
                }
                
                if (ypos >= height) {
                    ++pass;
                    switch (pass) {
                    case 1:
                        ypos = 4; break;
                    case 2:
                        ypos = 2; break;
                    case 3:
                        ypos = 1; break;
                    default:
                        goto fini;
                    }
                }
            } else {
                ++ypos;
            }
        }
        if (ypos >= height)
            break;
    }
    
fini:
    if (lzwReadByte(ifP,FALSE,lzwMinCodeSize)>=0)
        pm_message("too much input data, ignoring extra...");

}



static void
writePnm(FILE *outfile, xel ** const xels, 
         const int cols, const int rows,
         const int hasGray, const int hasColor) {
    int format;
    const char *format_name;
           
    if (hasColor) {
        format = PPM_FORMAT;
        format_name = "PPM";
    } else if (hasGray) {
        format = PGM_FORMAT;
        format_name = "PGM";
    } else {
        format = PBM_FORMAT;
        format_name = "PBM";
    }
    if (verbose) 
        pm_message("writing a %s file", format_name);
    
    if (outfile) 
        pnm_writepnm(outfile, xels, cols, rows,
                     (xelval) GIFMAXVAL, format, FALSE);
}



static void
transparencyMessage(int const transparent_index, 
                    gifColorMap cmap) {
/*----------------------------------------------------------------------------
   If user wants verbose output, tell him that the color with index
   'transparent_index' is supposed to be a transparent background color.
   
   If transparent_index == -1, tell him there is no transparent background
   color.
-----------------------------------------------------------------------------*/
    if (verbose) {
        if (transparent_index == -1)
            pm_message("no transparency");
        else
            pm_message("transparent background color: rgb:%02x/%02x/%02x "
                       "Index %d",
                       cmap[CM_RED][transparent_index],
                       cmap[CM_GRN][transparent_index],
                       cmap[CM_BLU][transparent_index],
                       transparent_index
                );
    }
}



static void
outputAlpha(FILE *alpha_file, pixel ** const xels, 
            const int cols, const int rows, const int transparent_index,
            unsigned char cmap[3][MAXCOLORMAPSIZE]) {
/*----------------------------------------------------------------------------
   Output to file 'alpha_file' (unless it is NULL) the alpha mask for the
   image 'xels', given that the color whose index in the color map 'cmap' is
   'transparent_index' is the transparent color.  The image, and thus the
   alpha mask have dimensions 'cols' by 'rows'.

   transparent_index == -1 means there are no transparent pixels.
-----------------------------------------------------------------------------*/

    if (alpha_file) {
        bit *alpha_row;  /* malloc'ed */
        xel transparent_color;

        if (transparent_index != -1) 
            PPM_ASSIGN(transparent_color, 
                       cmap[CM_RED][transparent_index],
                       cmap[CM_GRN][transparent_index],
                       cmap[CM_BLU][transparent_index]
                );
        
        alpha_row = pbm_allocrow(cols);

        pbm_writepbminit(alpha_file, cols, rows, FALSE);

        {
            int row;
            for (row = 0; row < rows; row++) {
                int col;
                for (col = 0; col < cols; col++) {
                    if (transparent_index != -1 && 
                        PNM_EQUAL(xels[row][col], transparent_color))
                        alpha_row[col] = PBM_BLACK;
                    else 
                        alpha_row[col] = PBM_WHITE;
                }
                pbm_writepbmrow(alpha_file, alpha_row, cols, FALSE);
            }
        }
        pbm_freerow(alpha_row);
    }
}



static void
readGifHeader(FILE * const gifFile, struct gifScreen * const gifScreenP) {
/*----------------------------------------------------------------------------
   Read the GIF stream header off the file gifFile, which is present
   positioned to the beginning of a GIF stream.  Return the info from it
   as *gifScreenP.
-----------------------------------------------------------------------------*/
    unsigned char   buf[16];
    char     version[4];


    if (! ReadOK(gifFile,buf,6))
        pm_error("error reading magic number" );
    
    if (strncmp((char *)buf,"GIF",3) != 0)
        pm_error("File does not contain a GIF stream.  It does not start "
                 "with 'GIF'.");
    
    strncpy(version, (char *)buf + 3, 3);
    version[3] = '\0';
    
    if (verbose)
        pm_message("GIF format version is '%s'", version);
    
    if ((strcmp(version, "87a") != 0) && (strcmp(version, "89a") != 0))
        pm_error("bad version number, not '87a' or '89a'" );
    
    if (! ReadOK(gifFile,buf,7))
        pm_error("failed to read screen descriptor" );
    
    gifScreenP->Width           = LM_to_uint(buf[0],buf[1]);
    gifScreenP->Height          = LM_to_uint(buf[2],buf[3]);
    gifScreenP->ColorMapSize    = 2<<(buf[4]&0x07);
    gifScreenP->ColorResolution = (((buf[4]&0x70)>>3)+1);
    gifScreenP->Background      = buf[5];
    gifScreenP->AspectRatio     = buf[6];

    if (verbose) {
        pm_message("GIF Width = %d GIF Height = %d "
                   "Pixel aspect ratio = %d (%f:1)",
                   gifScreenP->Width, gifScreenP->Height, 
                   gifScreenP->AspectRatio, 
                   gifScreenP->AspectRatio == 0 ? 
                   1 : (gifScreenP->AspectRatio + 15) / 64.0);
        pm_message("Colors = %d   Color Resolution = %d",
                   gifScreenP->ColorMapSize, gifScreenP->ColorResolution);
    }           
    if (BitSet(buf[4], LOCALCOLORMAP)) {    /* Global Colormap */
        readColorMap(gifFile, gifScreenP->ColorMapSize, gifScreenP->ColorMap,
                     &gifScreenP->hasGray, &gifScreenP->hasColor);
        if (verbose) {
            pm_message("Color map %s grays, %s colors", 
                       gifScreenP->hasGray ? "contains" : "doesn't contain",
                       gifScreenP->hasColor ? "contains" : "doesn't contain");
        }
    }
    
    if (gifScreenP->AspectRatio != 0 && gifScreenP->AspectRatio != 49) {
        float   r;
        r = ( (float) gifScreenP->AspectRatio + 15.0 ) / 64.0;
        pm_message("warning - input pixels are not square, "
                   "but we are rendering them as square pixels "
                   "in the output.  "
                   "To fix the output, run it through "
                   "'pnmscale -%cscale %g'",
                   r < 1.0 ? 'x' : 'y',
                   r < 1.0 ? 1.0 / r : r );
    }
}



static void
readExtensions(FILE*          const ifP, 
               struct gif89 * const gif89P,
               bool *         const eodP) {
/*----------------------------------------------------------------------------
   Read extension blocks from the GIF stream to which the file *ifP is
   positioned.  Read up through the image separator that begins the
   next image or GIF stream terminator.

   If we encounter EOD (end of GIF stream) before we find an image 
   separator, we return *eodP == TRUE.  Else *eodP == FALSE.

   If we hit end of file before an EOD marker, we abort the program with
   an error message.
-----------------------------------------------------------------------------*/
    bool imageStart;
    bool eod;

    eod = FALSE;
    imageStart = FALSE;

    /* Read the image descriptor */
    while (!imageStart && !eod) {
        unsigned char c;

        if (! ReadOK(ifP,&c,1))
            pm_error("EOF / read error on image data" );

        if (c == ';') {         /* GIF terminator */
            eod = TRUE;
        } else if (c == '!') {         /* Extension */
            if (! ReadOK(ifP,&c,1))
                pm_error("EOF / "
                         "read error on extension function code");
            doExtension(ifP, c, gif89P);
        } else if (c == ',') 
            imageStart = TRUE;
        else 
            pm_message("bogus character 0x%02x, ignoring", (int) c );
    }
    *eodP = eod;
}



static void
convertImage(FILE *           const ifP, 
             bool             const skipIt, 
             FILE *           const imageout_file, 
             FILE *           const alphafile, 
             struct gifScreen       gifScreen,
             struct gif89     const gif89) {

    unsigned char buf[16];
    bool useGlobalColormap;
    xel **xels;  /* The image raster, in libpnm format */
    int cols, rows;  /* Dimensions of the image */
    gifColorMap localColorMap;
    int localColorMapSize;

    if (! ReadOK(ifP,buf,9))
        pm_error("couldn't read left/top/width/height");

    useGlobalColormap = ! BitSet(buf[8], LOCALCOLORMAP);
        
    localColorMapSize = 1<<((buf[8]&0x07)+1);

    cols = LM_to_uint(buf[4],buf[5]);
    rows = LM_to_uint(buf[6],buf[7]);
        
    xels = pnm_allocarray(cols, rows);
    if (!xels)
        pm_error("couldn't alloc space for image" );

    if (! useGlobalColormap) {
        int hasGray, hasColor;

        readColorMap(ifP, localColorMapSize, localColorMap, 
                     &hasGray, &hasColor);
        transparencyMessage(gif89.transparent, localColorMap);
        readImageData(ifP, xels, cols, rows, localColorMap, 
                      BitSet(buf[8], INTERLACE));
        if (!skipIt) {
            writePnm(imageout_file, xels, cols, rows,
                     hasGray, hasColor);
            outputAlpha(alphafile, xels, cols, rows, 
                        gif89.transparent, localColorMap);
        }
    } else {
        transparencyMessage(gif89.transparent, gifScreen.ColorMap);
        readImageData(ifP, xels, cols, rows, gifScreen.ColorMap, 
                      BitSet(buf[8], INTERLACE));
        if (!skipIt) {
            writePnm(imageout_file, xels, cols, rows,
                     gifScreen.hasGray, gifScreen.hasColor);
            outputAlpha(alphafile, xels, cols, rows, 
                        gif89.transparent, gifScreen.ColorMap);
        }
    }
    pnm_freearray(xels, rows);
}



static void
convertImages(FILE * const ifP, 
              int    const requestedImageSeq, 
              FILE * const imageout_file, 
              FILE * const alphafile) {

    int imageSeq;
        /* Sequence within GIF stream of image we are currently processing.
           First is 0.
        */
    struct gifScreen gifScreen;
    struct gif89 gif89;
    bool eod;
        /* We've read through the GIF terminator character */

    initGif89(&gif89);

    readGifHeader(ifP, &gifScreen);

    eod = FALSE;  /* initial value */
    for (imageSeq=0; !eod; ++imageSeq) {
        readExtensions(ifP, &gif89, &eod);

        if (eod) {
            /* GIF stream ends before image with sequence imageSeq */
            if (imageSeq <= requestedImageSeq)
                pm_error("You requested Image %d, but "
                         "only %d image%s found in GIF stream",
                         requestedImageSeq+1,
                         imageSeq, imageSeq>1?"s":"" );
        } else 
            convertImage(ifP, imageSeq != requestedImageSeq, 
                         imageout_file, alphafile, gifScreen, gif89);
    }
}



int
main(int argc, char **argv) {

    struct cmdline_info cmdline;
    FILE *ifP;
    FILE *alpha_file, *imageout_file;

    pnm_init(&argc, argv);

    parse_command_line(argc, argv, &cmdline);
    verbose = cmdline.verbose;
    showComment = cmdline.comments;
   
    ifP = pm_openr(cmdline.input_filespec);

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

    convertImages(ifP, cmdline.image_no-1, imageout_file, alpha_file);

    pm_close(ifP);
    if (imageout_file != NULL) 
        pm_close( imageout_file );
    if (alpha_file != NULL)
        pm_close( alpha_file );

    exit(0);
}




