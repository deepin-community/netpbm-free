/*
 * pbmtextps.c -  render text into a bitmap using a postscript interpreter
 *
 * Copyright (C) 2002 by James McCann.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  This software is provided "as is" without express or
 * implied warranty.
 *
 * PostScript is a registered trademark of Adobe Systems International.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pbm.h"

#define FILENAME_LEN 200
static FILE *psfile = 0;
char psfname[FILENAME_LEN];
static FILE *pbmfile = 0;
char pbmfname[FILENAME_LEN];

#define BUFFER_SIZE 2048

const char *gs_exe_path = 
#ifdef GHOSTSCRIPT_EXECUTABLE_PATH
GHOSTSCRIPT_EXECUTABLE_PATH;
#else
0;
#endif

const char *pnmcrop_exe_path = 
#ifdef PNMCROP_EXECUTABLE_PATH
PNMCROP_EXECUTABLE_PATH;
#else
0;
#endif

static struct cmdlineInfo {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
  int res;         /* resolution, DPI */
  int fontsize;    /* Size of font in points */
  char *font;      /* Name of postscript font */
  float stroke;    /* Width of stroke in points (only for outline font) */
  unsigned int verbose;
  char *text;
} cmdline;



static void
parseCommandLine(int argc, char ** argv,
                 struct cmdlineInfo *cmdlineP) 
{
  /*---------------------------------------------------------------------------
    Note that the file spec array we return is stored in the storage that
    was passed to us as the argv array.
    -------------------------------------------------------------------------*/
  optEntry *option_def = malloc(100*sizeof(optStruct));
  /* Instructions to OptParseOptions2 on how to parse our options.
   */
  optStruct3 opt;

  unsigned int option_def_index;
  int i;
  int totaltextsize = 0;

  option_def_index = 0;   /* incremented by OPTENTRY */
  OPTENT3(0,   "resolution", OPT_INT,    &cmdlineP->res,            NULL,  0);
  OPTENT3(0,   "font",       OPT_STRING, &cmdlineP->font,           NULL,  0);
  OPTENT3(0,   "fontsize",   OPT_INT,    &cmdlineP->fontsize,       NULL,  0);
  OPTENT3(0,   "stroke",     OPT_FLOAT,  &cmdlineP->stroke,         NULL,  0);
  OPTENT3(0,   "verbose",    OPT_FLAG,   NULL, &cmdlineP->verbose,         0);

  /* Set the defaults */
  cmdlineP->res = 150;
  cmdlineP->fontsize = 24;
  cmdlineP->font = "Times-Roman";
  cmdlineP->stroke = -1;
  cmdlineP->text = NULL;

  opt.opt_table = option_def;
  opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
  opt.allowNegNum = FALSE;

  pm_optParseOptions3(&argc, argv, opt, sizeof(opt), 0);
  for (i = 1; i < argc; i++) {
    if (i > 1) {
      totaltextsize += 1;
      cmdlineP->text = realloc(cmdlineP->text, totaltextsize);
      if (cmdlineP->text == NULL)
	pm_error("out of memory");
      strcat(cmdlineP->text, " ");
    } 
    totaltextsize += strlen(argv[i]);
    cmdlineP->text = realloc(cmdlineP->text, totaltextsize);
    if (cmdlineP->text == NULL)
      pm_error("out of memory");
    strcat(cmdlineP->text, argv[i]);
  }
}



static char *
construct_postscript(struct  cmdlineInfo *cmdl)
{
  int size = 100;
  char *buffer = malloc (size);
  int nchars;
  const char *template;

  if(cmdl->stroke <= 0) {
    template ="/%s findfont\n%d scalefont\nsetfont\n12 36 moveto\n"
      "(%s) show\nshowpage\n";
  }    
  else {
    template ="/%s findfont\n%d scalefont\nsetfont\n12 36 moveto\n"
      "%f setlinewidth\n0 setgray\n"
      "(%s) true charpath\nstroke\nshowpage\n";
  }    
  nchars = snprintf (buffer, size, template, cmdl->font, 
		     cmdl->fontsize, cmdl->text);
  if (nchars >= size) {
    if(!(buffer = realloc (buffer, nchars + 1)))
      pm_error("Can't allocate buffer for postscript string");
      
  }
  if(cmdl->stroke < 0)
    snprintf (buffer, nchars, template, cmdl->font, cmdl->fontsize, 
	      cmdl->text);
  else
    snprintf (buffer, nchars, template, cmdl->font, cmdl->fontsize, 
	      cmdl->stroke, cmdl->text);

  return buffer;
}



static const char *
gs_executable_name()
{
  static char buffer[BUFFER_SIZE];
  if(! gs_exe_path) {
    char *which = "which gs";
    FILE *f;
    memset(buffer, 0, BUFFER_SIZE);
    if(!(f = popen(which, "r")))
      pm_error("Can't find ghostscript");
    fread(buffer, 1, BUFFER_SIZE, f);
    if(buffer[strlen(buffer) - 1] == '\n')
      buffer[strlen(buffer) - 1] = 0;
    pclose(f);
    if(buffer[0] != '/' && buffer[0] != '.')
      pm_error("Can't find ghostscript");
  }
  else
    strcpy(buffer, gs_exe_path);

  return buffer;
}



static const char *
crop_executable_name()
{
  static char buffer[BUFFER_SIZE];
  if(! pnmcrop_exe_path) {
    char *which = "which pnmcrop";
    FILE *f;
    memset(buffer, 0, BUFFER_SIZE);
    if(!(f = popen(which, "r"))) {
      return 0;
    }
    
    fread(buffer, 1, BUFFER_SIZE, f);
    if(buffer[strlen(buffer) - 1] == '\n')
      buffer[strlen(buffer) - 1] = 0;
    pclose(f);
    if(buffer[0] != '/' && buffer[0] != '.') {
      buffer[0] = 0;
      pm_message("Can't find pnmcrop");
    }
  }
  else
    strcpy(buffer, pnmcrop_exe_path);

  return buffer;
}



static char *
gs_command(struct cmdlineInfo *cmdl)
{
  static char com[BUFFER_SIZE];
  int x = cmdl->res * 11;
  int y = cmdl->res * (cmdl->fontsize * 2 + 72)  / 72.;
  snprintf(com, BUFFER_SIZE, "%s -g%dx%d -r%d -sDEVICE=pbm "
            "-sOutputFile=%s -q -dBATCH -dNOPAUSE %s </dev/null >/dev/null", 
            gs_executable_name(), x, y, cmdl->res, pbmfname, psfname);
  return com;
}



static char *
crop_command(void)
{
  static char com[BUFFER_SIZE];
  if(crop_executable_name())
    snprintf(com, BUFFER_SIZE, "%s -top -right %s", crop_executable_name(), 
	     pbmfname);
  else
    return 0;
  return com;
}



static void
cleanup(void)
{
  if(psfile) {
    fclose(psfile);
    unlink(psfname);
  }
  if(pbmfile) {
    pm_close(pbmfile);
    unlink(pbmfname);
  }
}



static void
create_outputfile(struct cmdlineInfo *cmdl)
{
  char *ps;
  char *com;
  const char *template = "./pstextpbm.%d.tmp.%s";
  FILE *pnmcrop;

  sprintf(psfname, template, getpid(), "ps");
  if(! (psfile = fopen(psfname, "w")))
    pm_error("Can't open temp file");

  sprintf(pbmfname, template, getpid(), "pbm");

  if(! (com = strdup(gs_command(cmdl))))
    pm_error("Can't allocate ghostscript command");

  ps = construct_postscript(cmdl);

  if (cmdl->verbose)
    pm_message("Postscript program = '%s'", ps);

  if(fwrite(ps, 1, strlen(ps), psfile) != strlen(ps))
    pm_error("Can't write postscript to temp file");
  fclose(psfile);

  if (cmdl->verbose)
    pm_message("Running Postscript interpreter '%s'", com);

  if(system(com))
    pm_error("Can't run ghostscript process");

  free(com);

  pbmfile = pm_openw(pbmfname);

  com = crop_command();
  if(com) {
    if (cmdl->verbose)
      pm_message("Running crop command '%s'", com);
    if(!(pnmcrop= popen(com, "r"))) 
      pm_error("Can't run pnmcrop process");
    else {
      char buf[2048];
      int chars;

      while( (chars = fread(buf, 1, 2048, pnmcrop) ) == 2048) {
	if(fwrite(buf, 1, chars, stdout) != chars)
	  pm_error("Can't write to stdout");
      }
      if(chars < 0)
	pm_error("Can't read from pbm file");
      if(chars > 0)
	if(fwrite(buf, 1, chars, stdout) != chars)
	  pm_error("Can't write to stdout");
      fclose(pnmcrop);
    }
  }
  else  /* If the crop command can't be found warn and keep going */
    pm_message("Can't find pnmcrop command, image will be large");
}



int 
main(int argc, char *argv[])
{
  pbm_init( &argc, argv );

  parseCommandLine(argc, argv, &cmdline);

  atexit(cleanup);

  create_outputfile(&cmdline);

  exit(0);
}
