/* pbmtext.c - render text into a bitmap
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

#include <string.h>

#include "pbm.h"
#include "pbmfont.h"

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    char *text;    /* text from command line or NULL if none */
    char *font;    /* -font option value or NULL if none */
    char *builtin; /* -builtin option value or NULL if none */
    int dump;   /* undocumented dump flag for installing a new built-in font */
    float space;   /* -space option value or default */
    unsigned int width;     /* future -width option value or zero */
    int lspace;    /* lspace option value or default */
};




static void
parse_command_line(int argc, char ** argv,
                   struct cmdline_info *cmdline_p) {
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
    OPTENTRY(0,   "font",     OPT_STRING,   &cmdline_p->font,         0);
    OPTENTRY(0,   "builtin",  OPT_STRING,   &cmdline_p->builtin,      0);
    OPTENTRY(0,   "dump",     OPT_UINT,     &cmdline_p->dump,         0);
    OPTENTRY(0,   "space",    OPT_FLOAT,    &cmdline_p->space,        0);
    OPTENTRY(0,   "width",    OPT_UINT,     &cmdline_p->width,        0);
    OPTENTRY(0,   "lspace",   OPT_INT,      &cmdline_p->lspace,       0);

    /* Set the defaults */
    cmdline_p->font = NULL;
    cmdline_p->builtin = NULL;
    cmdline_p->dump = FALSE;
    cmdline_p->space = 0.0;
    cmdline_p->width = 0;
    cmdline_p->lspace = 0;

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    pm_optParseOptions2(&argc, argv, opt, 0);
        /* Uses and sets argc, argv, and some of *cmdline_p and others. */

    if (argc-1 == 0)
        cmdline_p->text = NULL;
    else {
        int i;
        int totaltextsize;

        totaltextsize = 1;  /* initial value */
        
        cmdline_p->text = malloc(totaltextsize);
        cmdline_p->text[0] = '\0';
        
        for (i = 1; i < argc; i++) {
            if (i > 1) {
	        overflow_add(totaltextsize, 1);
                totaltextsize += 1;
                cmdline_p->text = realloc(cmdline_p->text, totaltextsize);
                if (cmdline_p->text == NULL)
                    pm_error("out of memory");
                strcat(cmdline_p->text, " ");
            } 
	    overflow_add(totaltextsize, strlen(argv[i]));
            totaltextsize += strlen(argv[i]);
            cmdline_p->text = realloc(cmdline_p->text, totaltextsize);
            if (cmdline_p->text == NULL)
                pm_error("out of memory");
            strcat(cmdline_p->text, argv[i]);
        }
    }
}



static void
fix_control_chars(char * const buf, struct font * const fn) {

    int i;

    /* chop off terminating newline */
    if (strlen(buf) >= 1 && buf[strlen(buf)-1] == '\n')
        buf[strlen(buf)-1] = '\0';
    
    for ( i = 0; buf[i] != '\0'; ++i ) {
        if ( buf[i] == '\t' ) { 
            /* Turn tabs into the right number of spaces. */
            int j, n, l;
            n = ( i + 8 ) / 8 * 8;
            l = strlen( buf );
            for ( j = l; j > i; --j )
                buf[j + n - i - 1] = buf[j];
            for ( ; i < n; ++i )
                buf[i] = ' ';
            --i;
	    }
        else if ( !fn->glyph[(unsigned char)buf[i]] )
            /* Turn unknown chars into a single space. */
            buf[i] = ' ';
	}
}



static void
fill_rect(bit** const bits, 
          int   const row0, 
          int   const col0, 
          int   const height, 
          int   const width, 
          bit   const color) {

    int row;

    for (row = row0; row < row0 + height; ++row) {
        int col;
        for (col = col0; col < col0 + width; ++col)
            bits[row][col] = color;
    }
}



static void
get_line_dimensions(const char line[], const struct font * const font_p, 
                    const float intercharacter_space,
                    int * const bwid_p, int * const backup_space_needed_p) {
/*----------------------------------------------------------------------------
   Determine the width in pixels of the line of text line[] in the font
   *font_p, and return it as *bwid_p.  Also determine how much of this
   width goes to the left of the nominal starting point of the line because
   the first character in the line has a "backup" distance.  Return that
   as *backup_space_needed_p.
-----------------------------------------------------------------------------*/
    int cursor;  /* cursor into the line of text */
    unsigned char lastch;  /* line[cursor] */
    float accumulated_ics;
        /* accumulated intercharacter space so far in the line we are 
           stepping through.  Because the intercharacter space might not be
           an integer, we accumulate it here and realize full pixels whenever
           we have more than one pixel.
           */

    int no_chars_yet; 
        /* logical: we haven't seen any renderable characters yet in 
           the line.
        */
    no_chars_yet = TRUE;   /* initial value */
    *bwid_p = 0;  /* initial value */
    accumulated_ics = 0.0;  /* initial value */

    for (cursor = 0; line[cursor] != '\0'; cursor++) {
        lastch = line[cursor];
        if (font_p->glyph[(unsigned char)lastch]) {
            if (no_chars_yet) {
                no_chars_yet = FALSE;
                if (font_p->glyph[lastch]->x < 0) 
                    *backup_space_needed_p = -font_p->glyph[lastch]->x;
                else {
                    *backup_space_needed_p = 0;
                    *bwid_p += font_p->glyph[lastch]->x;
                }
            } else {
                /* handle extra intercharacter space (-space option) */
                int full_pixels;  /* integer part of accumulated_ics */
                accumulated_ics += intercharacter_space;
                full_pixels = (int) accumulated_ics;
                if (full_pixels > 0) {
                    *bwid_p += full_pixels;
                    accumulated_ics -= full_pixels;
                }
            }
            *bwid_p += font_p->glyph[lastch]->xadd;
        }
    }
    if (no_chars_yet)
        /* Line has no renderable characters */
        *backup_space_needed_p = 0;
    else {
        /* Line has at least one renderable character.
           Recalculate width of last character in line so it ends
           right at the right edge of the glyph (no extra space to
           anticipate another character).
        */
        *bwid_p -= font_p->glyph[lastch]->xadd;
        *bwid_p += font_p->glyph[lastch]->width + font_p->glyph[lastch]->x;
    }
}



static void
insert_character(const struct glyph * const glyph, 
                 const int toprow, const int leftcol,
                 bit ** const bits) {
/*----------------------------------------------------------------------------
   Insert one character (whose glyph is 'glyph') into the image bits[].
   Its top left corner shall be row 'toprow', column 'leftcol'.
-----------------------------------------------------------------------------*/

    int glyph_y, glyph_x;  /* position within the glyph */

    for (glyph_y = 0; glyph_y < glyph->height; glyph_y++) {
        for (glyph_x = 0; glyph_x < glyph->width; glyph_x++) {
            if (glyph->bmap[glyph_y * glyph->width + glyph_x])
                bits[toprow+glyph_y][leftcol+glyph->x+glyph_x] = 
                    PBM_BLACK;
        }
    }
}    



static void
insert_characters(bit ** const bits, const char ** const lp, const int lines,
                  struct font *font_p, 
                  const int topmargin, const int leftmargin,
                  const float intercharacter_space, const int lspace) {
/*----------------------------------------------------------------------------
   Put the lines of text 'lp' (array of 'lines' strings) into the image 'bits'
   using font *font_p and putting 'intercharacter_space' pixels between
   characters and 'lspace' pixels between the lines.
-----------------------------------------------------------------------------*/
    int line;  /* Line number in input text */

    for ( line = 0; line < lines; ++line ) {
        int row;  /* row in image of top of current typeline */
        int leftcol;  /* Column in image of left edge of current glyph */
        int cursor;  /* cursor into a line of input text */
        float accumulated_ics;
            /* accumulated intercharacter space so far in the line we
               are building.  Because the intercharacter space might
               not be an integer, we accumulate it here and realize
               full pixels whenever we have more than one pixel. 
            */

        row = topmargin + line * (font_p->maxheight + lspace);
        leftcol = leftmargin;
        accumulated_ics = 0.0;  /* initial value */
    
        for ( cursor = 0; lp[line][cursor] != '\0'; ++cursor ) {
            struct glyph* glyph;   /* the glyph for this character */

            glyph = font_p->glyph[(unsigned char)lp[line][cursor]];
            if (glyph != NULL) {
                const int toprow = row + font_p->maxheight + font_p->y 
                    - glyph->height - glyph->y;
                    /* row number in image of top row in glyph */
                
                insert_character(glyph, toprow, leftcol, bits);

                leftcol += glyph->xadd;
                {
                    /* handle extra intercharacter space (-space option) */
                    int full_pixels;  /* integer part of accumulated_ics */
                    accumulated_ics += intercharacter_space;
                    full_pixels = (int) accumulated_ics;
                    if (full_pixels > 0) {
                        leftcol += full_pixels;
                        accumulated_ics -= full_pixels;
                    }
                }
            }
        }
    }
}


static void
flow_text(char **lp[], const char input_text[], int *linesP,
          const int width, 
          struct font * const font_p, const float intercharacter_space) {
/* under construction */
    *lp[0] = "not implemented";
    *linesP = 1;
}


static void
truncate_text(char **lp[], const char *input_text[], const int lines,
              const int width, 
              struct font * const font_p, const float intercharacter_space) {
/* under construction */

    int line;  /* Line number on which we are currently working */

    *lp[0] = "not implemented";

    for (line = 0; line < lines; line++){
            
        int cursor;  /* cursor into the line of text */
        unsigned char lastch;  /* line[cursor] */
        int width_so_far;
            /* How long the line we've built, in pixels, is so far */
        float accumulated_ics;
        /* accumulated intercharacter space so far in the line we are 
           stepping through.  Because the intercharacter space might not be
           an integer, we accumulate it here and realize full pixels whenever
           we have more than one pixel.
           */

        int no_chars_yet; 
        /* logical: we haven't seen any renderable characters yet in 
           the line.
        */
        no_chars_yet = TRUE;   /* initial value */
        width_so_far = 0;  /* initial value */
        accumulated_ics = 0.0;  /* initial value */

        for (cursor = 0; 
             input_text[line][cursor] != '\0' && width_so_far < width; 
             cursor++) {
            lastch = input_text[line][cursor];
            if (font_p->glyph[(unsigned char)lastch]) {
                if (no_chars_yet) {
                    no_chars_yet = FALSE;
                    if (font_p->glyph[lastch]->x > 0) 
                        width_so_far += font_p->glyph[lastch]->x;
                } else {
                    /* handle extra intercharacter space (-space option) */
                    int full_pixels;  /* integer part of accumulated_ics */
                    accumulated_ics += intercharacter_space;
                    full_pixels = (int) intercharacter_space;
                    if (full_pixels > 0) {
                        width_so_far += full_pixels;
                        accumulated_ics -= full_pixels;
                    }
                }
                width_so_far += font_p->glyph[lastch]->xadd;
            }
            if (width_so_far < width) {
/* not implemented yet
                lp[line][cursor] = input_text[line][cursor];
                lp[line][cursor+1] = '\0';
*/
            }
        }
    }
}



static void
get_text(const char cmdline_text[], struct font * const fn, 
         char *** const input_textP, int * linesP) {

    int maxlines;  
        /* Maximum number of lines for which we presently have space
           in the text array 
           */

    maxlines = 50;  /* initial value */
    *input_textP = (char**) malloc2(maxlines, sizeof(char*));
    if (*input_textP == NULL)
        pm_error("out of memory");

    if (cmdline_text) {
        overflow_add(strlen(cmdline_text), 1);
        (*input_textP)[0] = malloc(strlen(cmdline_text)+1);
        if ((*input_textP)[0] == NULL)
            pm_error("Out of memory.");
        strcpy((*input_textP)[0], cmdline_text);
        fix_control_chars((*input_textP)[0], fn);
        *linesP = 1;
    } else {
        /* Read text from stdin. */
        char buf[5000];

        *linesP = 0;  /* initial value */
        while (fgets(buf, sizeof(buf), stdin) != NULL) {
            fix_control_chars(buf, fn);
            if (*linesP >= maxlines) {
	        overflow2(maxlines, 2);
                maxlines *= 2;
		overflow2(maxlines, sizeof(char *));
                *input_textP = (char**) realloc((char*) *input_textP, 
                                                maxlines * sizeof(char*));
                if(*input_textP == NULL)
                    pm_error( "out of memory" );
            }
            (*input_textP)[*linesP] = (char*) malloc(strlen(buf) + 1);
            if ((*input_textP)[*linesP] == NULL)
                pm_error( "out of memory" );
            strcpy((*input_textP)[*linesP], buf);
            ++(*linesP);
	    }
	}
}



static void
compute_image_width(char ** const lp, const int lines,
                    const struct font * const fn,
                    const float intercharacter_space,
                    int * const maxwidthP, int * maxleftbP) {
    int line;
    
    *maxwidthP = 0;  /* initial value */
    *maxleftbP = 0;  /* initial value */
    for (line = 0; line < lines; ++line) {
        int bwid, backup_space_needed;
        
        get_line_dimensions(lp[line], fn, intercharacter_space,
                            &bwid, &backup_space_needed);
        
        *maxwidthP = max(*maxwidthP, bwid);
        *maxleftbP = max(*maxleftbP, backup_space_needed);
    }
}



int
main(int argc, char *argv[]) {

    struct cmdline_info cmdline;
    bit** bits;
    int rows, cols;
    struct font* fn;
    int vmargin, hmargin;
    char** input_text;
    char** lp;
    int lines;
    int maxwidth;
        /* width in pixels of the longest line of text in the image */
    int maxleftb;

    pbm_init( &argc, argv );

    parse_command_line(argc, argv, &cmdline);
    
    if (cmdline.font)
	    fn = pbm_loadfont(cmdline.font);
    else {
        if (cmdline.builtin)
            fn = pbm_defaultfont(cmdline.builtin);
        else
            fn = pbm_defaultfont("bdf");
    }

    if (cmdline.dump) {
        pbm_dumpfont(fn);
        exit(0);
	}

    get_text(cmdline.text, fn, &input_text, &lines);
    
    if (lines == 1) {
        vmargin = fn->maxheight / 2;
        hmargin = fn->maxwidth;
	} else {
        vmargin = fn->maxheight;
        overflow2(2, fn->maxwidth);
        hmargin = 2 * fn->maxwidth;
	}

    if (cmdline.width > 0) {
        /* Flow or truncate lines to meet user's width request */
        /* Not actually implemented yet. */
        if (lines == 1) 
            flow_text(&lp, input_text[0], &lines, cmdline.width, 
                      fn, cmdline.space);
        else 
            truncate_text(&lp, (const char **) input_text, lines, 
                          cmdline.width, fn, cmdline.space);
    } else
        lp = input_text;
    
    overflow2(2, vmargin);
    overflow2(lines, fn->maxheight);
    overflow_add(vmargin * 2, lines * fn->maxheight);
    rows = 2 * vmargin + lines * fn->maxheight + (lines-1) * cmdline.lspace;

    compute_image_width(lp, lines, fn, cmdline.space, &maxwidth, &maxleftb);

    overflow2(2, hmargin);
    overflow_add(2*hmargin, maxwidth);
    cols = 2 * hmargin + maxwidth;
    bits = pbm_allocarray(cols, rows);

    /* Fill background with white */
    fill_rect(bits, 0, 0, rows, cols, PBM_WHITE);

    /* Put the text in  */
    insert_characters(bits, (const char **) lp, lines, fn, 
               vmargin, hmargin + maxleftb, cmdline.space, cmdline.lspace);

    /* All done. */
    pbm_writepbm(stdout, bits, cols, rows, 0);
    pm_close(stdout);

    exit(0);
}



