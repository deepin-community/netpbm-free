/******************************************************************************
                               pnmsplit
*******************************************************************************
  Split a Netpbm format input file into multiple Netpbm format output files
  with one image per output file.

  By Bryan Henderson June 2000
  Contributed to the public domain.
******************************************************************************/

#define _BSD_SOURCE 2
   /* This ensures that strdup() is in string.h */

#include <string.h>
#include <stdio.h>
#include "pnm.h"


struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_file;
    char *output_file_pattern;
    int debug;
} cmdline;



static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that the pointers we place into *cmdline_p are sometimes to storage
   in the argv array.
-----------------------------------------------------------------------------*/
    optStruct *option_def = malloc(100*sizeof(optStruct));
        /* Instructions to OptParseOptions2 on how to parse our options.
         */
    optStruct2 opt;

    unsigned int option_def_index;

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENTRY(0,   "debug", OPT_FLAG,   &cmdline_p->debug,     0);

    /* Set the defaults */
    cmdline_p->debug = FALSE;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions2(&argc, argv, opt, 0);
        /* Uses and sets argc, argv, and all of *cmdline_p. */

    if (argc - 1 < 1) 
        cmdline_p->input_file = "-";
    else
        cmdline_p->input_file = argv[1];
    if (argc -1 < 2)
        cmdline_p->output_file_pattern = "image%d";
    else
        cmdline_p->output_file_pattern = argv[2];

    if (!strstr(cmdline_p->output_file_pattern, "%d"))
        pm_error("output file spec pattern parameter must include the "
                 "string '%%d',\n"
                 "to stand for the image sequence number.\n"
                 "You specified '%s'.", cmdline_p->output_file_pattern);
}



static void
extract_one_image(FILE *infile, const char outputfilename[]) {

    FILE *outfile;
    xelval maxval;
    int rows, cols, format;
    enum pm_check_code check_retval;
    
    int row;
    xel *xelrow;

    pnm_readpnminit(infile, &cols, &rows, &maxval, &format);

    pnm_check(infile, PM_CHECK_BASIC, format, cols, rows, maxval, 
              &check_retval);

    outfile = pm_openw(outputfilename);
    pnm_writepnminit(outfile, cols, rows, maxval, format, 0);

    xelrow = pnm_allocrow(cols);
    for (row = 0; row < rows; row++) {
        pnm_readpnmrow(infile, xelrow, cols, maxval, format);
        pnm_writepnmrow(outfile, xelrow, cols, maxval, format, 0);
    }
    pnm_freerow(xelrow);
    pm_close(outfile);
}



static void
compute_output_name(const char output_file_pattern[], 
                    const unsigned int image_seq,
                    char ** const output_name_p) {
/*----------------------------------------------------------------------------
   Compute the name of an output file given the pattern output_file_pattern[]
   and the image sequence number 'image_seq'.  output_file_pattern[] 
   contains at least one instance of the string "%d" and we substitute the
   ASCII decimal representation of image_seq for it to generate the output
   file name.
-----------------------------------------------------------------------------*/

    char *before_sub, *after_sub;
    int size;

    before_sub = strdup(output_file_pattern);
    *(strstr(before_sub, "%d")) = '\0';
    after_sub = strstr(output_file_pattern, "%d") + 2;

    overflow_add(strlen(output_file_pattern), 9);
    size = strlen(output_file_pattern) - 2 + 10 + 1;
    *output_name_p = malloc(size);

    sprintf(*output_name_p, "%s%d%s", before_sub, image_seq, after_sub);

    free(before_sub);
}



int
main(int argc, char *argv[]) {

    FILE* ifp;
    int eof;  /* No more images in input */
    unsigned int image_seq;  
        /* Sequence of current image in input file.  First = 0 */

    pnm_init( &argc, argv );

    parse_command_line(argc, argv, &cmdline);

    ifp = pm_openr(cmdline.input_file);

    eof = FALSE;
    for (image_seq = 0; !eof; image_seq++) {
        char *output_file_name;  /* malloc'ed */
        compute_output_name(cmdline.output_file_pattern, image_seq,
                            &output_file_name);
        pm_message("WRITING %s\n", output_file_name);
        extract_one_image(ifp, output_file_name);
        free(output_file_name);
        pnm_nextimage(ifp, &eof);
    }

    pm_close(ifp);
    
    exit( 0 );
}
