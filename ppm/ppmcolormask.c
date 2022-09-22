/* ppmcolormask.c - produce PBM mask of areas containing certain color
 * written by Bryan Henderson in 2000
 * Contributed to the public domain.
*/

#define _BSD_SOURCE
    /* This makes sure strdup() is in string.h */

#include <string.h>
#include "ppm.h"
#include "pbm.h"

static struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filename;
    pixel mask_color;
    int verbose;

} cmdline;

static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p);



int
main(int argc, char *argv[]) {

    FILE* ifp;

    /* Parameters of input image: */
    int rows, cols;
    pixval maxval;
    int format;

    ppm_init(&argc, argv);

    parse_command_line(argc, argv, &cmdline);

    if (cmdline.input_filename != NULL) 
        ifp = pm_openr(cmdline.input_filename);
    else
        ifp = stdin;

    ppm_readppminit(ifp, &cols, &rows, &maxval, &format);
    pbm_writepbminit(stdout, cols, rows, 0);
    {
        pixel* const input_row = ppm_allocrow(cols);
        bit* const mask_row = pbm_allocrow(cols);
        {
            int row;
            for (row = 0; row < rows; ++row) {
                int col;
                ppm_readppmrow(ifp, input_row, cols, maxval, format);
                for (col = 0; col < cols; ++col) {
                    if (PPM_EQUAL(input_row[col], cmdline.mask_color)) 
                        mask_row[col] = PBM_BLACK;
                    else 
                        mask_row[col] = PBM_WHITE;
                }
                pbm_writepbmrow(stdout, mask_row, cols, 0);
            }
        }
        pbm_freerow(mask_row);
        ppm_freerow(input_row);
    }

    pm_close(ifp);

    exit(0);
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
    OPTENTRY('v', "verbose",    OPT_FLAG,   &cmdline_p->verbose,        0);

    /* Set the defaults */
    cmdline_p->verbose = 0;

    pm_optParseOptions(&argc, argv, option_def, 0);
        /* Uses and sets argc, argv, and all of *cmdline_p. */

    if (argc - 1 == 0)
        pm_error("You must specify the color to mask as an argument.");
    cmdline_p->mask_color = ppm_parsecolor(argv[1], PPM_MAXMAXVAL);

    if (argc - 1 == 1)
        cmdline_p->input_filename = NULL;  /* he wants stdin */
    else if (argc - 1 == 2) {
        if (strcmp(argv[2], "-") == 0)
            cmdline_p->input_filename = NULL;  /* he wants stdin */
        else 
            cmdline_p->input_filename = strdup(argv[2]);
    } else 
        pm_error("Too many arguments.  The only arguments accepted\n"
                 "are the mask color and optional input file specificaton");

}

