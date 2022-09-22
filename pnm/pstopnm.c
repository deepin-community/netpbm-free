/*----------------------------------------------------------------------------
                                 pstopnm
------------------------------------------------------------------------------
  Use Ghostscript to convert a Postscript file into a PBM, PGM, or PNM
  file.

-----------------------------------------------------------------------------*/

#define _BSD_SOURCE 1   /* Make sure strdup() is in string.h */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>  
#include <sys/stat.h>

#include "pnm.h"
#include "shhopt.h"

enum orientation {PORTRAIT, LANDSCAPE, UNSPECIFIED};
struct box {
    /* Description of a rectangle within an image; all coordinates 
       measured in pixels with lower left corner of page being the 
       origin.
       */
    int llx;  /* lower left X coord */
        /* -1 for llx means whole box is undefined. */
    int lly;  /* lower left Y coord */
    int urx;  /* upper right X coord */
    int ury;  /* upper right Y coord */
};

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *input_filespec;  /* Filespecs of input files */
    unsigned int forceplain;
    struct box extract_box;
    unsigned int nocrop;
    unsigned int format_type;
    unsigned int verbose;
    float xborder;
    int xmax;
    int xsize;
    float yborder;
    int ymax;
    int ysize;
    enum orientation orientation;
    unsigned int goto_stdout;
} cmdline;


static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optEntry *option_def = malloc( 100*sizeof( optEntry ) );
        /* Instructions to optParseOptions3 on how to parse our options.
         */
    optStruct3 opt;

    unsigned int option_def_index;

    unsigned int pbm_opt, pgm_opt, ppm_opt;
    unsigned int portrait_opt, landscape_opt;
    float llx, lly, urx, ury;
    unsigned int llxSpec, llySpec, urxSpec, urySpec;
    
    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENT3(0, "forceplain", OPT_FLAG,  NULL, &cmdline_p->forceplain,     0);
    OPTENT3(0, "llx",        OPT_FLOAT, &llx, &llxSpec,                   0);
    OPTENT3(0, "lly",        OPT_FLOAT, &lly, &llySpec,                   0);
    OPTENT3(0, "urx",        OPT_FLOAT, &urx, &urxSpec,                   0);
    OPTENT3(0, "ury",        OPT_FLOAT, &ury, &urySpec,                   0);
    OPTENT3(0, "nocrop",     OPT_FLAG,  NULL, &cmdline_p->nocrop,         0);
    OPTENT3(0, "pbm",        OPT_FLAG,  NULL, &pbm_opt,                   0);
    OPTENT3(0, "pgm",        OPT_FLAG,  NULL, &pgm_opt,                   0);
    OPTENT3(0, "ppm",        OPT_FLAG,  NULL, &ppm_opt,                   0);
    OPTENT3(0, "verbose",    OPT_FLAG,  NULL, &cmdline_p->verbose,        0);
    OPTENT3(0, "xborder",    OPT_FLOAT, &cmdline_p->xborder, NULL,        0);
    OPTENT3(0, "xmax",       OPT_UINT,  &cmdline_p->xmax, NULL,           0);
    OPTENT3(0, "xsize",      OPT_UINT,  &cmdline_p->xsize, NULL,          0);
    OPTENT3(0, "yborder",    OPT_FLOAT, &cmdline_p->yborder, NULL,        0);
    OPTENT3(0, "ymax",       OPT_UINT,  &cmdline_p->ymax, NULL,           0);
    OPTENT3(0, "ysize",      OPT_UINT,  &cmdline_p->ysize, NULL,          0);
    OPTENT3(0, "portrait",   OPT_FLAG,  NULL, &portrait_opt,              0);
    OPTENT3(0, "landscape",  OPT_FLAG,  NULL, &landscape_opt,             0);
    OPTENT3(0, "stdout",     OPT_FLAG,  NULL, &cmdline_p->goto_stdout,    0);

    /* Set the defaults */
    llx = lly = urx = ury = -1;
    cmdline_p->xmax = 612;
    cmdline_p->ymax = 792;
    cmdline_p->xsize = cmdline_p->ysize = -1;
    cmdline_p->xborder = cmdline_p->yborder = 0.1;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (argc-1 == 0)
        cmdline_p->input_filespec = "-";  /* stdin */
    else if (argc-1 == 1)
        cmdline_p->input_filespec = argv[1];
    else 
        pm_error("Too many arguments (%d).  "
                 "Only need one: the Postscript filespec", argc-1);

    if (portrait_opt & !landscape_opt)
        cmdline_p->orientation = PORTRAIT;
    else if (!portrait_opt & landscape_opt)
        cmdline_p->orientation = LANDSCAPE;
    else if (!portrait_opt & !landscape_opt)
        cmdline_p->orientation = UNSPECIFIED;
    else
        pm_error("Cannot specify both -portrait and -landscape options");

    if (pbm_opt)
        cmdline_p->format_type = PBM_TYPE;
    else if (pgm_opt)
        cmdline_p->format_type = PGM_TYPE;
    else if (ppm_opt)
        cmdline_p->format_type = PPM_TYPE;
    else
        cmdline_p->format_type = PPM_TYPE;

    /* If any one of the 4 bounding box coordinates is given on the
       command line, we default any of the 4 that aren't.  
    */
    if (llxSpec || llySpec || urxSpec || urySpec) {
        if (!llxSpec) cmdline_p->extract_box.llx = 72;
        else cmdline_p->extract_box.llx = llx * 72;
        if (!llySpec) cmdline_p->extract_box.lly = 72;
        else cmdline_p->extract_box.lly = lly * 72;
        if (!urxSpec) cmdline_p->extract_box.urx = 540;
        else cmdline_p->extract_box.urx = urx * 72;
        if (!urySpec) cmdline_p->extract_box.ury = 720;
        else cmdline_p->extract_box.ury = ury * 72;
    } else {
        cmdline_p->extract_box.llx = -1;
    }

}



static void
add_ps_to_filespec(const char orig_filespec[], char ** const new_filespec_p,
                   const int verbose) {
/*----------------------------------------------------------------------------
   If orig_filespec[] names an existing file, but the same name with ".ps"
   added to the end does, return the name with the .ps attached.  Otherwise,
   just return orig_filespec[].

   Return the name in newly malloc'ed storage, pointed to by
   *new_filespec_p.
-----------------------------------------------------------------------------*/
    struct stat statbuf;
    int stat_rc;

    stat_rc = lstat(orig_filespec, &statbuf);
    
    if (stat_rc == 0)
        *new_filespec_p = strdup(orig_filespec);
    else {
        char *filespec_plus_ps;

        overflow_add(strlen(orig_filespec), 4);
        filespec_plus_ps = malloc(strlen(orig_filespec) + 4);
        strcpy(filespec_plus_ps, orig_filespec);
        strcat(filespec_plus_ps, ".ps");

        stat_rc = lstat(filespec_plus_ps, &statbuf);
        if (stat_rc == 0)
            *new_filespec_p = strdup(filespec_plus_ps);
        else
            *new_filespec_p = strdup(orig_filespec);
    }
    if (verbose)
        pm_message("Input file is %s", *new_filespec_p);
}



static void
compute_size_res(const struct cmdline_info cmdline, 
                 const enum orientation orientation, 
                 const struct box bordered_box,
                 int * const xsize_p, int * const ysize_p,
                 int * const xres_p, int * const yres_p) {

    int sx, sy;

    if (orientation == LANDSCAPE) {
        sx = bordered_box.ury - bordered_box.lly;
        sy = bordered_box.urx - bordered_box.llx;
    } else {
        sx = bordered_box.urx - bordered_box.llx;
        sy = bordered_box.ury - bordered_box.lly;
    }
    if (cmdline.ysize == -1 && cmdline.xsize == -1) {
        /* User didn't specify either x or y size, so we compute them
           from xmax and ymax
           */
        *yres_p = cmdline.ymax * 72 / sy;
        *xres_p = *yres_p = 
            min(cmdline.xmax * 72 / sx, cmdline.ymax * 72 / sy);
        
        if (cmdline.nocrop) {
            *xsize_p = cmdline.xmax;
            *ysize_p = cmdline.ymax;
        } else {
            *xsize_p = (int) (sx * *xres_p / 72 + 0.5);
            *ysize_p = (int) (sy * *yres_p / 72 + 0.5);
        }
    } else {
        /* Compute the x and y size from the specified x and/or y sizes */
        if (cmdline.xsize != -1) {
            *xsize_p = cmdline.xsize;
            *xres_p = cmdline.xsize * 72 / sx;
            if (cmdline.ysize == -1) {
                *yres_p = *xres_p;
                *ysize_p = (int) (sy * *yres_p/72 + 0.5);
            }
        }

        if (cmdline.ysize != -1) {
            *ysize_p = cmdline.ysize;
            *yres_p = cmdline.ysize * 72 / sy;
            if (cmdline.xsize == -1) {
                *xres_p = *yres_p;
                *xsize_p = (int) (sx * *xres_p/72 + 0.5);
            }
        }
    }
}


enum postscript_language {COMMON_POSTSCRIPT, ENCAPSULATED_POSTSCRIPT};

static enum postscript_language
language_declaration(const char input_filespec[], int const verbose) {

    FILE *infile;
    enum postscript_language language;
    char line[80];

    infile = pm_openr(input_filespec);

    if (fgets(line, sizeof(line), infile) == NULL)
        language = COMMON_POSTSCRIPT;
    else {
        const char eps_header[] = "%!PS-Adobe EPSF-3.0";

        if (strncmp(line, eps_header, sizeof(eps_header)))
            language = ENCAPSULATED_POSTSCRIPT;
        else
            language = COMMON_POSTSCRIPT;
    }
    fclose(infile);
    if (verbose)
        pm_message("language is %s",
                   language == ENCAPSULATED_POSTSCRIPT ?
                   "encapsulated postscript" :
                   "not encapsulated postscript");
    return language;
}



static struct box
compute_box_to_extract(const struct box cmdline_extract_box,
                       const char input_filespec[],
                       const int verbose) {

    struct box retval;

    if (cmdline.extract_box.llx != -1)
        /* User told us what box to extract, so that's what we'll do */
        retval = cmdline_extract_box;
    else {
        /* Try to get the bounding box from the DSC %%BoundingBox
           statement (A Postscript comment) in the input.
        */
        struct box ps_bb;  /* Box described by %%BoundingBox stmt in input */

        if (strcmp(input_filespec, "-") == 0)
            /* Can't read stdin, because we need it positioned for the 
               Ghostscript interpreter to read it.
            */
            ps_bb.llx = -1;
        else {
            FILE *infile;
            int found_BB, eof;  /* logical */
            infile = pm_openr(input_filespec);
            
            found_BB = FALSE;
            eof = FALSE;
            while (!eof && !found_BB) {
                char line[200];
                
                if (fgets(line, sizeof(line), infile) == NULL)
                    eof = TRUE;
                else {
                    int rc;
                    rc = sscanf(line, "%%%%BoundingBox: %d %d %d %d",
                                &ps_bb.llx, &ps_bb.lly, 
                                &ps_bb.urx, &ps_bb.ury);
                    if (rc == 4) 
                        found_BB = TRUE;
                }
            }
            fclose(infile);

            if (!found_BB) {
                ps_bb.llx = -1;
                pm_message("Warning: no %%%%BoundingBox statement "
                           "in the input or command line.\n"
                           "Will use defaults");
            }
        }
        if (ps_bb.llx != -1) {
            if (verbose)
                pm_message("Using %%%%BoundingBox statement from input.");
            retval = ps_bb;
        } else { 
            /* Use the center of an 8.5" x 11" page with 1" border all around*/
            retval.llx = 72;
            retval.lly = 72;
            retval.urx = 540;
            retval.ury = 720;
        }
    }
    if (verbose)
        pm_message("Extracting the box ((%d,%d),(%d,%d))",
                   retval.llx, retval.lly, retval.urx, retval.ury);
    return retval;
}



static enum orientation
compute_orientation(const struct cmdline_info cmdline, 
                    const struct box extract_box) {

    enum orientation retval;
    const int input_width = extract_box.urx - extract_box.llx;
    const int input_height = extract_box.ury - extract_box.lly;

    if (cmdline.orientation != UNSPECIFIED)
        retval = cmdline.orientation;
    else {
        if ((cmdline.xsize == -1 || cmdline.ysize == -1) &
            (cmdline.xsize != -1 || cmdline.ysize != -1)) {
            /* User specified one output dimension, but not the other,
               so we can't use output dimensions to make the decision.  So
               just use the input dimensions.
               */
            if (input_height > input_width) retval = PORTRAIT;
            else retval = LANDSCAPE;
        } else {
            int output_width, output_height;
            if (cmdline.xsize != -1) {
                /* He gave xsize and ysize, so that's the output size */
                output_width = cmdline.xsize;
                output_height = cmdline.ysize;
            } else {
                /* Well then we'll just use his (or default) xmax, ymax */
                output_width = cmdline.xmax;
                output_height = cmdline.ymax;
            }

            if (input_height > input_width && output_height > output_width)
                retval = PORTRAIT;
            else 
                if (input_height < input_width && output_height < output_width)
                    retval = PORTRAIT;
            else 
                retval = LANDSCAPE;
        }
    }
    return retval;
}



static struct box
add_borders(const struct box input_box, 
            const float xborder_scale, float yborder_scale,
            const int verbose) {

    struct box retval;

    const int left_right_border_size = 
        (int) ((input_box.urx - input_box.llx) * xborder_scale + 0.5);
    const int top_bottom_border_size = 
        (int) ((input_box.ury - input_box.lly) * yborder_scale + 0.5);

    retval.llx = input_box.llx - left_right_border_size;
    retval.lly = input_box.lly - top_bottom_border_size;
    retval.urx = input_box.urx + left_right_border_size;
    retval.ury = input_box.ury + top_bottom_border_size;

    if (verbose)
        pm_message("With borders, extracted box is ((%d,%d),(%d,%d))",
                   retval.llx, retval.lly, retval.urx, retval.ury);

    return retval;
}



static char *
compute_pstrans(const struct box box, const enum orientation orientation,
                const int xsize, const int ysize, 
                const int xres, const int yres) {

    static char retval[100]; 

    if (orientation == PORTRAIT) {
        int llx, lly;
        llx = box.llx - (xsize * 72 / xres - (box.urx - box.llx)) / 2;
        lly = box.lly - (ysize * 72 / yres - (box.ury - box.lly)) / 2;
        sprintf(retval, "%d neg %d neg translate", llx, lly);
    } else {
        int llx, ury;
        llx = box.llx - (ysize * 72 / yres - (box.urx - box.llx)) / 2;
        ury = box.ury + (xsize * 72 / xres - (box.ury - box.lly)) / 2;
        sprintf(retval, "90 rotate %d neg %d neg translate", llx, ury);
    }
    return retval;
}



static char *
compute_outfile_arg(const struct cmdline_info cmdline) {

    char *retval;  /* malloc'ed */

    overflow_add(strlen(cmdline.input_filespec), 10);
    retval = malloc(strlen(cmdline.input_filespec) + 10);

    if (cmdline.goto_stdout)
        strcpy(retval, "-");
    else if (strcmp(cmdline.input_filespec, "-") == 0)
        strcpy(retval, "-");
    else {
        strcpy(retval, cmdline.input_filespec);
        if (strlen(retval) > 3 && strcmp(retval+strlen(retval)-3, ".ps") == 0) 
            /* The input filespec ends in ".ps".  Chop it off. */
            retval[strlen(retval)-3] = '\0';

        strcat(retval, "%03d.");
        switch (cmdline.format_type) {
        case PBM_TYPE: strcat(retval, "pbm"); break;
        case PGM_TYPE: strcat(retval, "pgm"); break;
        case PPM_TYPE: strcat(retval, "ppm"); break;
        default: pm_error("Internal error: invalid value format_type");
        }
    }
    return(retval);
}



static char *
compute_gs_device(const int format_type, const int forceplain) {

    static char retval[20];

    switch (cmdline.format_type) {
    case PBM_TYPE: strcpy(retval, "pbm"); break;
    case PGM_TYPE: strcpy(retval, "pgm"); break;
    case PPM_TYPE: strcpy(retval, "ppm"); break;
    default: pm_error("Internal error: invalid value format_type");
    }
    if (!forceplain)
        strcat(retval, "raw");

    return(retval);
}



static void
findGhostscriptProg(const char ** const retvalP) {
    
    *retvalP = NULL;  /* initial assumption */
    if (getenv("GHOSTSCRIPT"))
        *retvalP = strdup(getenv("GHOSTSCRIPT"));
    if (*retvalP == NULL) {
        if (getenv("PATH") != NULL) {
            char *pathwork;  /* malloc'ed */
            const char * candidate;

            pathwork = strdup(getenv("PATH"));
            
            candidate = strtok(pathwork, ":");

            *retvalP = NULL;
            while (!*retvalP && candidate) {
                struct stat statbuf;
                const char * filename;
                int rc;

                asprintf(&filename, "%s/gs", candidate);
                rc = stat(filename, &statbuf);
                if (rc == 0) {
                    if (S_ISREG(statbuf.st_mode))
                        *retvalP = strdup(filename);
                } else if (errno != ENOENT)
                    pm_error("Error looking for Ghostscript program.  "
                             "stat(\"%s\") returns errno %d (%s)",
                             filename, errno, strerror(errno));
		free((void *) filename);

                candidate = strtok(NULL, ":");
            }
            free(pathwork);
        }
    }
    if (*retvalP == NULL)
        *retvalP = strdup("/usr/bin/gs");
}



static void
execGhostscript(int const inputPipeFd,
                const char ghostscript_device[],
                const char outfile_arg[], 
                int const xsize, int const ysize, 
                int const xres, int const yres,
                const char input_filespec[], int const verbose) {
    
    const char *arg0;
    const char *ghostscriptProg;
    const char *deviceopt;
    const char *outfileopt;
    const char *gopt;
    const char *ropt;
    int rc;

    findGhostscriptProg(&ghostscriptProg);

    /* Put the input pipe on Standard Input */
    rc = dup2(inputPipeFd, STDIN_FILENO);
    close(inputPipeFd);

    asprintf(&arg0, "gs");
    asprintf(&deviceopt, "-sDEVICE=%s", ghostscript_device);
    asprintf(&outfileopt, "-sOutputFile=%s", outfile_arg);
    asprintf(&gopt, "-g%dx%d", xsize, ysize);
    asprintf(&ropt, "-r%dx%d", xres, yres);

    if (verbose) {
        pm_message("about to message");
        pm_message("execing '%s' with args '%s' (arg 0), "
                   "'%s', '%s', '%s', '%s', '%s', '%s', '%s'",
                   ghostscriptProg, arg0,
                   deviceopt, outfileopt, gopt, ropt, "-q",
		   "-dNOPAUSE", "-dSAFER", "-");
    }

    execl(ghostscriptProg, arg0, deviceopt, outfileopt, gopt, ropt, "-q",
          "-dNOPAUSE", "-dSAFER", "-", NULL);
    
    pm_error("execl() of Ghostscript ('%s') failed, errno=%d (%s)",
             ghostscriptProg, errno, strerror(errno));
}




static void
execute_ghostscript(const char pstrans[], const char ghostscript_device[],
                    const char outfile_arg[], 
                    const int xsize, const int ysize, 
                    const int xres, const int yres,
                    const char input_filespec[], 
                    const enum postscript_language language,
                    const int verbose) {

    int gs_exit;  /* wait4 exit code from Ghostscript */
    FILE *gs;  /* Pipe to Ghostscript's standard input */
    FILE *infile;
    int rc;
    int eof;  /* End of file on input */
    int pipefd[2];

    if (strlen(outfile_arg) > 80)
        pm_error("output file spec too long.");
    
    rc = pipe(pipefd);
    if (rc < 0)
        pm_error("Unable to create pipe to talk to Ghostscript process.  "
                 "errno = %d (%s)", errno, strerror(errno));
    
    rc = fork();
    if (rc < 0)
        pm_error("Unable to fork a Ghostscript process.  errno=%d (%s)",
                 errno, strerror(errno));
    else if (rc == 0) {
        /* Child process */
        close(pipefd[1]);
        execGhostscript(pipefd[0], ghostscript_device, outfile_arg,
                        xsize, ysize, xres, yres, input_filespec, verbose);
    } else {
        pid_t const ghostscriptPid = rc;
        int const pipeToGhostscriptFd = pipefd[1];
        /* parent process */
        close(pipefd[0]);

        gs = fdopen(pipeToGhostscriptFd, "w");
        if (gs == NULL) 
            pm_error("Unable to open stream on pipe to Ghostscript process.");
    
        infile = pm_openr(input_filespec);
        /*
          In encapsulated Postscript, we the encapsulator are supposed to
          handle showing the page (which we do by passing a showpage
          statement to Ghostscript).  Any showpage statement in the 
          input must be defined to have no effect.
          
          See "Enscapsulated PostScript Format File Specification",
          v. 3.0, 1 May 1992, in particular Example 2, p. 21.  I found
          it at 
          http://partners.adobe.com/asn/developer/pdfs/tn/5002.EPSF_Spec.pdf
          The example given is a much fancier solution than we need
          here, I think, so I boiled it down a bit.  JM 
        */
        if (language == ENCAPSULATED_POSTSCRIPT)
            fprintf(gs, "\n/b4_Inc_state save def /showpage { } def\n");
 
        if (verbose) 
            pm_message("Postscript prefix command: '%s'", pstrans);

        fprintf(gs, "%s\n", pstrans);

        /* If our child dies, it closes the pipe and when we next write to it,
           we get a SIGPIPE.  We must survive that signal in order to report
           on the fate of the child.  So we ignore SIGPIPE:
        */
        signal(SIGPIPE, SIG_IGN);

        eof = FALSE;
        while (!eof) {
            char buffer[4096];
            int bytes_read;
            
            bytes_read = fread(buffer, 1, sizeof(buffer), infile);
            if (bytes_read == 0) 
                eof = TRUE;
            else 
                fwrite(buffer, 1, bytes_read, gs);
        }
        pm_close(infile);

        if (language == ENCAPSULATED_POSTSCRIPT)
            fprintf(gs, "\nb4_Inc_state restore showpage\n");

        fclose(gs);
        
        waitpid(ghostscriptPid, &gs_exit, 0);
        if (rc < 0)
            pm_error("Wait for Ghostscript process to terminated failed.  "
                     "errno = %d (%s)", errno, strerror(errno));

        if (gs_exit != 0) {
            if (WIFEXITED(gs_exit))
                pm_error("Ghostscript failed.  Exit code=%d\n", 
                         WEXITSTATUS(gs_exit));
            else if (WIFSIGNALED(gs_exit))
                pm_error("Ghostscript process died due to a signal %d.",
                         WTERMSIG(gs_exit));
            else 
                pm_error("Ghostscript process died with exit code %d", 
                         gs_exit);
        }
    }
}



int
main(int argc, char **argv) {

    char *input_filespec;  /* malloc'ed */
        /* The file specification of our Postscript input file */
    int xres, yres;   /* Resolution in pixels per inch */
    int xsize, ysize;  /* output image size in pixels */
    struct box extract_box;
        /* coordinates of the box within the input we are to extract; i.e.
           that will become the output. 
           */
    struct box bordered_box;
        /* Same as above, but expanded to include borders */

    enum postscript_language language;
    enum orientation orientation;
    char *ghostscript_device;
    char *outfile_arg;
    char *pstrans;

    pnm_init(&argc, argv);

    parse_command_line(argc, argv, &cmdline);

    add_ps_to_filespec(cmdline.input_filespec, &input_filespec,
                       cmdline.verbose);

    extract_box = compute_box_to_extract(cmdline.extract_box, input_filespec, 
                                         cmdline.verbose);

    language = language_declaration(input_filespec, cmdline.verbose);
    
    orientation = compute_orientation(cmdline, extract_box);

    bordered_box = add_borders(extract_box, cmdline.xborder, cmdline.yborder,
                               cmdline.verbose);

    compute_size_res(cmdline, orientation, bordered_box, 
                     &xsize, &ysize, &xres, &yres);
    
    pstrans = compute_pstrans(bordered_box, orientation,
                              xsize, ysize, xres, yres);

    outfile_arg = compute_outfile_arg(cmdline);

    ghostscript_device = 
        compute_gs_device(cmdline.format_type, cmdline.forceplain);
    
    pm_message("Writing %s file", ghostscript_device);
    
    execute_ghostscript(pstrans, ghostscript_device, outfile_arg, 
                        xsize, ysize, xres, yres, input_filespec,
                        language, cmdline.verbose);

    return 0;
}


