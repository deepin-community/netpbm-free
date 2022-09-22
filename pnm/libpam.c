/*----------------------------------------------------------------------------
                                  libpam.c
------------------------------------------------------------------------------
   These are the library functions, which belong in the libpnm library,
   that deal with the PAM (Portable Arbitrary Format) image format.
-----------------------------------------------------------------------------*/

#include <string.h>
#include "pam.h"
#include "ppm.h"
#include "libpbm.h"
#include "libpgm.h"
#include "libppm.h"


char
stripeq(const char * const comparand, const char * const comparator) {
/*----------------------------------------------------------------------------
  Compare two strings, ignoring leading and trailing white space.

  Return true (1) if the strings are identical, false (0) otherwise.
-----------------------------------------------------------------------------*/

  char *p, *q, *px, *qx;
  char equal;
  
  /* Make p and q point to the first non-blank character in each string.
     If there are no non-blank characters, make them point to the terminating
     NULL.
     */

  p = (char *) comparand;
  while (isspace(*p)) p++;
  q = (char *) comparator;
  while (isspace(*q)) q++;

  /* Make px and qx point to the last non-blank character in each string.
     If there are no nonblank characters (which implies the string is
     null), make them point to the terminating NULL.
     */

  if (*p == '\0') px = p;
  else {
    px = p + strlen(p) - 1;
    while (isspace(*px)) px--;
  }

  if (*q == '\0') qx = q;
  else {
    qx = q + strlen(q) - 1;
    while (isspace(*qx)) qx--;
  }

  equal = TRUE;   /* initial assumption */
  
  /* If the stripped strings aren't the same length, 
     we know they aren't equal 
     */
  if (px - p != qx - q) equal = FALSE;


  while (p <= px) {
    if (*p != *q) equal = FALSE;
    p++; q++;
  }
  return(equal);
}



int
pnm_tupleequal(const struct pam * const pamP, 
               tuple              const comparand, 
               tuple              const comparator) {

    /* TODO: For speed reasons, we should probably make this an inline */

    unsigned int plane;
    bool equal;

    equal = TRUE;  /* initial value */
    for (plane = 0; plane < pamP->depth; ++plane) 
        if (comparand[plane] != comparator[plane])
            equal = FALSE;

    return equal;
}




void
pnm_assigntuple(const struct pam * const pamP,
                tuple              const dest,
                tuple              const source) {

    /* TODO: For speed reasons, we should probably make this an inline */

    unsigned int plane;
    for (plane = 0; plane < pamP->depth; ++plane) {
        dest[plane] = source[plane];
    }
}



void
pnm_scaletuple(const struct pam * const pamP,
               tuple              const dest,
               tuple              const source, 
               sample             const newmaxval) {

    /* TODO: For speed reasons, we should probably make this an inline */

    unsigned int plane;
    for (plane = 0; plane < pamP->depth; ++plane) 
        dest[plane] = 
            (source[plane] * newmaxval + (pamP->maxval/2)) / pamP->maxval;
}


 
void
createBlackTuple(const struct pam * const pamP, tuple * const blackTupleP) {
/*----------------------------------------------------------------------------
   Create a "black" tuple.  By that we mean a tuple all of whose elements
   are zero.  If it's an RGB, grayscale, or b&w pixel, that means it's black.
-----------------------------------------------------------------------------*/
    *blackTupleP = pnm_allocpamtuple(pamP);
    if (pamP->format == PAM_FORMAT) {
        /* In this format, we don't know the meaning of "black", so we
           just punt.
           */
        int i;
        for (i = 0; i < pamP->depth; i++) 
            (*blackTupleP)[i] = 0;
    } else {
        xel black_xel;
        black_xel = pnm_blackxel(pamP->maxval, pamP->format);
        (*blackTupleP)[0] = PPM_GETR(black_xel);
        (*blackTupleP)[1] = PPM_GETG(black_xel);
        (*blackTupleP)[2] = PPM_GETB(black_xel);
    }
}



tuple *
pnm_allocpamrow(struct pam * const pamP) {
    const int bytes_per_tuple = pamP->depth * sizeof(sample);
    tuple * tuplerow;

    /* The tuple row data structure starts with 'width' pointers to
        the tuples, immediately followed by the 'width' tuples
        themselves.  Each tuple consists of 'depth' samples.  
    */

    overflow_add(sizeof(tuple *), bytes_per_tuple);
    tuplerow = malloc2(pamP->width, sizeof(tuple *) + bytes_per_tuple);
    if (tuplerow == NULL)
        pm_error("Out of memory allocating space for a tuple row of\n"
                 "%d tuples by %d samples per tuple by %d bytes per sample.",
                 pamP->width, pamP->depth, sizeof(sample));

    {
        /* Now we initialize the pointers to the individual tuples to make this
           a regulation C two dimensional array.
        */
        
        char *p;
        int i;
        
        p = (char*) (tuplerow + pamP->width);  /* location of Tuple 0 */
        for (i = 0; i < pamP->width; i++) {
            tuplerow[i] = (tuple) p;
            p += bytes_per_tuple;
        }
    }
    return(tuplerow);
}



tuple **
pnm_allocpamarray(struct pam * const pamP) {
    
    tuple **tuplearray;
    int row;

    /* If the speed of this is ever an issue, it might be sped up a little
       by allocating one large chunk.
       */
    
    tuplearray = malloc2(pamP->height, sizeof(tuple*));
    if (tuplearray == NULL) 
        pm_error("Out of memory allocating the row pointer section of "
                 "a %u row array", pamP->height);

    for (row = 0; row < pamP->height; row++) {
        tuplearray[row] = pnm_allocpamrow(pamP);
    }
    return(tuplearray);
}



void
pnm_freepamarray(tuple ** const tuplearray, struct pam * const pamP) {

    int row;
    for (row = 0; row < pamP->height; row++)
        pnm_freepamrow(tuplearray[row]);

    free(tuplearray);
}


void
pnm_setpamrow(struct pam const pam, 
              tuple *    const tuplerow, 
              sample     const value) {

    int col;
    for (col = 0; col < pam.width; ++col) {
        int plane;
        for (plane = 0; plane < pam.depth; ++plane) 
            tuplerow[col][plane] = value;
    }
}




#define MAX_LABEL_LENGTH 8
#define MAX_VALUE_LENGTH 255

static void
parse_header_line(const char buffer[], char label[MAX_LABEL_LENGTH+1], 
                  char value[MAX_VALUE_LENGTH+1]) {
    
    int buffer_curs;

    buffer_curs = 0;
    /* Skip initial white space */
    while (isspace(buffer[buffer_curs])) buffer_curs++;

    {
        /* Read off label, put as much as will fit into label[] */
        int label_curs;
        label_curs = 0;
        while (!isspace(buffer[buffer_curs]) && buffer[buffer_curs] != '\0') {
            if (label_curs < MAX_LABEL_LENGTH) 
                label[label_curs++] = buffer[buffer_curs];
            buffer_curs++;
        }
        label[label_curs] = '\0';  /* null terminate it */
    }    

    /* Skip white space between label and value */
    while (isspace(buffer[buffer_curs])) buffer_curs++;

    /* copy value into value[] */
    strncpy(value, buffer+buffer_curs, MAX_VALUE_LENGTH+1);

    {
        /* Remove trailing white space from value[] */
        int value_curs;
        value_curs = strlen(value)-1;
        while (value_curs >= 0 && isspace(value[value_curs])) 
            value[value_curs--] = '\0';
    }
}



static void
process_header_line(const char buffer[], struct pam * const pamP,
                    int * const endofheaderP) {            
/*----------------------------------------------------------------------------
   Process a line from the PAM header.  The line is buffer[], and it is not
   a comment or blank.

   Put the value that the line defines in *pamP (unless it's ENDHDR).
   set *endofheader true if it's an ENDHDR line, false otherwise.
-----------------------------------------------------------------------------*/
    char label[MAX_LABEL_LENGTH+1];
    char value[MAX_VALUE_LENGTH+1];

    parse_header_line(buffer, label, value);

    if (strcmp(label, "ENDHDR") == 0)
        *endofheaderP = TRUE;
    else {
        *endofheaderP = FALSE;

        if (strcmp(label, "WIDTH") == 0 ||
            strcmp(label, "HEIGHT") == 0 ||
            strcmp(label, "DEPTH") == 0 ||
            strcmp(label, "MAXVAL") == 0) {

            if (strlen(value) == 0)
                pm_error("Missing value for %s in PAM file header.",
                         label);
            else {
                char *endptr;
                long numeric_value;
                numeric_value = strtol(value, &endptr, 10);
                if (*endptr != '\0') 
                    pm_error("Non-numeric value for %s in "
                             "PAM file header: '%s'", label, value);
                else if (numeric_value < 0) 
                    pm_error("Negative value for %s in "
                             "PAM file header: '%s'", label, value);
            }
        }
    
        if (strcmp(label, "WIDTH") == 0) 
            pamP->width = atoi(value);
        else if (strcmp(label, "HEIGHT") == 0) 
            pamP->height = atoi(value);
        else if (strcmp(label, "DEPTH") == 0) 
            pamP->depth = atoi(value);
        else if (strcmp(label, "MAXVAL") == 0) 
            pamP->maxval = atoi(value);
        else if (strcmp(label, "TUPLTYPE") == 0) {
            int len = strlen(pamP->tuple_type);
            if (len + strlen(value) + 1 > sizeof(pamP->tuple_type)-1)
                pm_error("TUPLTYPE value too long in PAM header");
            if (len == 0)
                strcpy(pamP->tuple_type, value);
            else {
                strcat(pamP->tuple_type, "\n");
                strcat(pamP->tuple_type, value);
            }
            pamP->tuple_type[sizeof(pamP->tuple_type)-1] = '\0';
        } else 
            pm_error("Unrecognized header line: '%s'.  "
                     "Possible missing ENDHDR line?", label);
    }
}



static void
pnm_readpaminitrest(struct pam * const pamP) {
/*----------------------------------------------------------------------------
   Read the rest of the PAM header (after the first line -- the magic
   number line).  Fill in all the information in *pamP.
-----------------------------------------------------------------------------*/
    char buffer[256];
    char *rc;
    int through_header;

    pamP->width = 0;
    pamP->height = 0;
    pamP->depth = 0;
    pamP->maxval = 0;
    pamP->tuple_type[0] = '\0';

    { int c;
    /* Read off rest of 1st line -- probably just the newline after the 
       magic number 
       */
    while ((c = getc(pamP->file)) != -1 && c != '\n');
    }    

    through_header = FALSE;
    while (!through_header) {
        rc = fgets(buffer, sizeof(buffer), pamP->file);
        if (rc == NULL)
            pm_error("EOF or error reading file while trying to read the "
                     "PAM header");
        else {
            if (buffer[0] == '#');
                /* Ignore it; it's a comment */
            else if (stripeq(buffer, ""));
                /* Ignore it; it's a blank line */
            else 
                process_header_line(buffer, pamP, &through_header);
        }
    }
    if (pamP->height == 0) 
        pm_error("HEIGHT value is zero or unspecified in PAM header");
    if (pamP->width == 0) 
        pm_error("WIDTH value is zero or unspecified in PAM header");
    if (pamP->depth == 0) 
        pm_error("DEPTH value is zero or unspecified in PAM header");
    if (pamP->maxval == 0) 
        pm_error("MAXVAL value is zero or unspecified in PAM header");
}



void 
pnm_readpaminit(FILE *file, struct pam * const pamP, const int size) {
    if (size < (char*)&pamP->tuple_type - (char*)pamP) 
        pm_error("pam object passed to pnm_readpaminit() is too small.  "
                 "It must be large\n"
                 "enough to hold at least up to the "
                 "'tuple_type' member, but according\n"
                 "to the 'size' argument, it is only %d bytes long.", 
                 size);

    pamP->size = size;
    pamP->file = file;
    pamP->len = min(pamP->size, sizeof(struct pam));

    /* Get magic number. */
    pamP->format = pbm_readmagicnumber(file);

    pamP->plainformat =
        (pamP->format == PBM_FORMAT || 
         pamP->format == PGM_FORMAT ||
         pamP->format == PPM_FORMAT);

    switch (PAM_FORMAT_TYPE(pamP->format)) {
    case PAM_TYPE: 
        pnm_readpaminitrest(pamP);
        break;
    case PPM_TYPE: {
        pixval maxval;
        ppm_readppminitrest(pamP->file, &pamP->width, &pamP->height, &maxval);
        pamP->maxval = (sample) maxval;
        pamP->depth = 3;
        strcpy(pamP->tuple_type, PAM_PPM_TUPLETYPE);
    }
        break;

    case PGM_TYPE: {
        gray maxval;
        pgm_readpgminitrest(pamP->file, &pamP->width, &pamP->height, &maxval );
        pamP->maxval = (sample) maxval;
        pamP->depth = 1;
        strcpy(pamP->tuple_type, PAM_PGM_TUPLETYPE);
    }
    break;

    case PBM_TYPE:
        pbm_readpbminitrest(pamP->file, &pamP->width,&pamP->height);
        pamP->maxval = (sample) 1;
        pamP->depth = 1;
        strcpy(pamP->tuple_type, PAM_PBM_TUPLETYPE);
        break;
        
    default:
    pm_error("bad magic number - not a PAM, PPM, PGM, or PBM file");
    }
    
    if (pamP->maxval >> 8 == 0)       pamP->bytes_per_sample = 1;
    else if (pamP->maxval >> 16 == 0) pamP->bytes_per_sample = 2;
    else if (pamP->maxval >> 24 == 0) pamP->bytes_per_sample = 3;
    else                              pamP->bytes_per_sample = 4;
}



static sample
get_sample(FILE *file, const int bytes) {

    sample value;  /* our return value */

    if (bytes == 1) {
        /* Here's a speedup for the common 1-byte sample case: */
        value = getc(file);
        if (value == EOF)
            pm_error("EOF/error reading 1 byte sample to file.");
    } else {
        /* This code works for bytes == 1..4 */
        /* We could speed this up by exploiting knowledge of the format of
           an unsigned integer (i.e. endianness).  Then we could just cast
           the value as an array of characters instead of shifting and
           masking.
           */
        int shift;
        unsigned char inval[4];
        int cursor;
        int n_read;

        n_read = fread(inval, bytes, 1, file);
        if (n_read < 1) 
            pm_error("EOF/error reading %d byte sample from file.", bytes);
        value = 0;  /* initial value */
        cursor = 0;
        for (shift = (bytes-1)*8; shift >= 0; shift-=8) 
            value += inval[cursor++] << shift;
    }
    return(value);
}



void 
pnm_readpamrow(struct pam * const pamP, tuple* const tuplerow) {

    /* Need a special case for raw PBM because it has multiple tuples (8)
       packed into one byte.
       */
    if (PAM_FORMAT_TYPE(pamP->format) == PBM_TYPE) {
        int col;
        bit *bitrow;
        if (pamP->depth != 1)
            pm_error("Invalid pam structure passed to pnm_readpamrow().  "
                     "It says PBM format, but 'depth' member is not 1.");
        bitrow = pbm_allocrow(pamP->width);
        pbm_readpbmrow(pamP->file, bitrow, pamP->width, pamP->format);
        for (col = 0; col < pamP->width; col++)
            tuplerow[col][0] = 
                bitrow[col] == PBM_BLACK ? PAM_PBM_BLACK : PAM_PBM_WHITE;
        pbm_freerow(bitrow);
    } else {
        int col;
        for (col = 0; col < pamP->width; col++) {
            int samp;
            for (samp = 0; samp < pamP->depth; samp++) {
                if (pamP->plainformat)
                    tuplerow[col][samp] = pbm_getint(pamP->file);
                else 
                    tuplerow[col][samp] = 
                        get_sample(pamP->file, pamP->bytes_per_sample);
            }
        }
    }
}



tuple** 
pnm_readpam(FILE *file, struct pam * const pamP, const int size) {

    tuple **tuplearray;
    int row;

    pnm_readpaminit(file, pamP, size);
    
    tuplearray = pnm_allocpamarray(pamP);
    
    for (row = 0; row < pamP->height; row++) 
        pnm_readpamrow(pamP, tuplearray[row]);

    return(tuplearray);
}



void 
pnm_writepam(struct pam *const pamP, tuple ** const tuplearray) {

    int row;

    pnm_writepaminit(pamP);
    
    for (row = 0; row < pamP->height; row++) 
        pnm_writepamrow(pamP, tuplearray[row]);
}



void 
pnm_writepaminit(struct pam *const pamP) {

    if (pamP->size < pamP->len)
        pm_error("pam object passed to pnm_writepaminit() is smaller "
                 "(%d bytes, according to its 'size' element) "
                 "than the amount of data in it "
                 "(%d bytes, according to its 'len' element).",
                 pamP->size, pamP->len);

    if (pamP->len < (char*)&pamP->tuple_type - (char*)pamP) 
        pm_error("pam object passed to pnm_writepaminit() is too small.  "
                 "It must be large\n"
                 "enough to hold at least up to the "
                 "'tuple_type' member, but according\n"
                 "to its 'len' element, it is only %d bytes long.", 
                 pamP->len);

    if (pamP->maxval >> 8 == 0)       pamP->bytes_per_sample = 1;
    else if (pamP->maxval >> 16 == 0) pamP->bytes_per_sample = 2;
    else if (pamP->maxval >> 24 == 0) pamP->bytes_per_sample = 3;
    else                              pamP->bytes_per_sample = 4;
    
    switch (PAM_FORMAT_TYPE(pamP->format)) {
    case PAM_TYPE:
        fprintf(pamP->file, "P7\n");
        fprintf(pamP->file, "WIDTH %d\n",    pamP->width);
        fprintf(pamP->file, "HEIGHT %d\n",   pamP->height);
        fprintf(pamP->file, "DEPTH %d\n",    pamP->depth);
        fprintf(pamP->file, "MAXVAL %ld\n",  pamP->maxval);
        if (!stripeq(pamP->tuple_type, ""))
            fprintf(pamP->file, "TUPLTYPE %s\n", pamP->tuple_type);
        fprintf(pamP->file, "ENDHDR\n");
        break;
	case PPM_TYPE:
        if (pamP->depth != 3) 
            pm_error("pnm_writepaminit() got PPM format, but depth = %d "
                     "instead of 3, as required for PPM.", pamP->depth);
        if (pamP->maxval > PPM_OVERALLMAXVAL) 
            pm_error("pnm_writepaminit() got PPM format, but maxval = %ld, "
                     "which exceeds the maximum allowed for PPM: %d", 
                     pamP->maxval, PPM_OVERALLMAXVAL);
        ppm_writeppminit(pamP->file, pamP->width, pamP->height, 
                         (pixval) pamP->maxval, 0);
        break;

	case PGM_TYPE:
        if (pamP->depth != 1) 
            pm_error("pnm_writepaminit() got PGM format, but depth = %d "
                     "instead of 1, as required for PGM.", pamP->depth);
        if (pamP->maxval > PGM_OVERALLMAXVAL)
            pm_error("pnm_writepaminit() got PGM format, but maxval = %ld, "
                     "which exceeds the maximum allowed for PGM: %d", 
                     pamP->maxval, PGM_OVERALLMAXVAL);
        pgm_writepgminit(pamP->file, pamP->width, pamP->height, 
                         (gray) pamP->maxval, 0);
        break;

	case PBM_TYPE:
        if (pamP->depth != 1) 
            pm_error("pnm_writepaminit() got PBM format, but depth = %d "
                     "instead of 1, as required for PBM.", pamP->depth);
        if (pamP->maxval != 1) 
            pm_error("pnm_writepaminit() got PBM format, but maxval = %ld "
                     "instead of 1, as required for PBM.", pamP->maxval);
        pbm_writepbminit(pamP->file, pamP->width, pamP->height, 0);
        break;

    default:
        pm_error("Invalid format passed to pnm_writepaminit(): %d",
                 pamP->format);
    }
}

            

static void
write_sample(FILE *file, const unsigned int value, const int bytes) {

    if (bytes == 1) {
        /* Here's a speedup for the common 1-byte sample case: */
        int rc;
        rc = fputc(value, file);
        if (rc == EOF)
            pm_error("Error writing 1 byte sample to file.");
    } else {
        /* This code works for bytes == 1..4 */
        /* We could speed this up by exploiting knowledge of the format of
           an unsigned integer (i.e. endianness).  Then we could just cast
           the value as an array of characters instead of shifting and
           masking.
           */
        int shift;
        unsigned char outval[4];
        int cursor;
        int n_written;

        cursor = 0;
        for (shift = (bytes-1)*8; shift >= 0; shift-=8) {
            outval[cursor++] = (value >> shift) & 0xFF;
        }
        n_written = fwrite(&outval, bytes, 1, file);
        if (n_written == 0) 
            pm_error("Error writing %d byte sample to file.", bytes);
    }
}



void 
pnm_writepamrow(struct pam *const pamP, const tuple * const tuplerow) {

    /* For speed, we don't check any of the inputs for consistency 
       here (unless it's necessary to avoid crashing).  Any consistency
       checking should have been done by a prior call to 
       pnm_writepaminit().
       */

    /* Need a special case for raw PBM because it has multiple tuples (8)
       packed into one byte.
       */
    if (PAM_FORMAT_TYPE(pamP->format) == PBM_TYPE) {
        int col;
        bit *bitrow;
        bitrow = pbm_allocrow(pamP->width);
        for (col = 0; col < pamP->width; col++)
            bitrow[col] = 
                tuplerow[col][0] == PAM_PBM_BLACK ? PBM_BLACK : PBM_WHITE;
        pbm_writepbmrow(pamP->file, bitrow, pamP->width, 0);
        pbm_freerow(bitrow);
    } else {
        int col;

        for (col = 0; col < pamP->width; col++) {
            int samp;
            for (samp = 0; samp < pamP->depth; samp++) {
                /* If we allowed writing of plain format via the pam functions,
                   we could do a putus() here to write a plain sample.  We'd
                   also need some stuff to put in the spaces and line breaks.
                   */
                write_sample(pamP->file, 
                             tuplerow[col][samp], 
                             pamP->bytes_per_sample);
            }
        }    
    }
}



void
pnm_checkpam(struct pam *const pamP, const enum pm_check_type check_type, 
             enum pm_check_code * const retval_p) {

    if (check_type != PM_CHECK_BASIC) {
        if (retval_p) *retval_p = PM_CHECK_UNKNOWN_TYPE;
    } else switch (PAM_FORMAT_TYPE(pamP->format)) {
    case PAM_TYPE: 
        pm_check(pamP->file, 
                 check_type, 
                 pamP->width * pamP->height * pamP->depth * 
                 pamP->bytes_per_sample,
                 retval_p);
        break;
    case PPM_TYPE:
        pgm_check(pamP->file, check_type, pamP->format, 
                  pamP->width, pamP->height, pamP->maxval, retval_p);
        break;
    case PGM_TYPE:
        pgm_check(pamP->file, check_type, pamP->format, 
                  pamP->width, pamP->height, pamP->maxval, retval_p);
        break;
    case PBM_TYPE:
        pbm_check(pamP->file, check_type, pamP->format, 
                  pamP->width, pamP->height, retval_p);
        break;
    default:
        if (retval_p) *retval_p = PM_CHECK_UNCHECKABLE;
    }
}




double pnm_lumin_factor[3] = {PPM_LUMINR, PPM_LUMING, PPM_LUMINB};

void
pnm_YCbCrtuple(const tuple tuple, 
               double * const YP, double * const CbP, double * const CrP) {
/*----------------------------------------------------------------------------
   Assuming that the tuple 'tuple' is of tupletype RGB, return the 
   Y/Cb/Cr representation of the color represented by the tuple.
-----------------------------------------------------------------------------*/
    const int red = tuple[PAM_RED_PLANE];
    const int grn = tuple[PAM_GRN_PLANE];
    const int blu = tuple[PAM_BLU_PLANE];
    
    *YP  = (+ PPM_LUMINR * red + PPM_LUMING * grn + PPM_LUMINB * blu);
    *CbP = (- 0.1687 * red - 0.3312 * grn + 0.5000 * blu);
    *CrP = (+ 0.5000 * red - 0.4183 * grn - 0.0816 * blu);
}
