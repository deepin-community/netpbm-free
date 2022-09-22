/* $Id: example.c,v 1.1.1.1 2003/08/12 18:23:03 aba-guest Exp $ */
/*------------------------------------------------------------------------
 |  FILE            example.c
 |
 |  DESCRIPTION     Sample source code using the shhopt-library to parse
 |                  command line options.
 |
 |  WRITTEN BY      Sverre H. Huseby <sverrehu@online.no>
 +----------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>  /* needed in strToDouble only */

#include "shhopt.h"

/*-----------------------------------------------------------------------+
|  PRIVATE DATA                                                          |
+-----------------------------------------------------------------------*/

/* called by -N or --number-func */
static void
numberFunc(int i)
{
    printf("integer function called, i=%d\n", i);
}

/* called by -S or --string-func */
static void
stringFunc(char *s)
{
    printf("string function called, s=\"%s\"\n", s);
}

/* called by -F or --flag-func */
static void
flagFunc(void)
{
    printf("flag function called\n");
}

/* shhopt does not support double arguments, so we write our own
 * handler for this. The trick is to create a function taking a string
 * argument, and convert this string to a double. The following two
 * functions handles this. */

/* convert a string to a double value, with error checking */
static double
strToDouble(char *s)
{
    double d;
    char *e;

    d = strtod(s, &e);
    if (*e) {
	fprintf(stderr, "invalid number `%s'\n", s);
	exit(1);
    }
    if (errno == ERANGE) {
	fprintf(stderr, "number `%s' out of range\n", s);
	exit(1);
    }
    return d;
}

/* called by -D or --double-func */
static void
doubleFunc(char *s)
{
    double d;

    d = strToDouble(s);
    printf("double function called, d=\"%g\"\n", d);
}

static void
usage(void)
{
    printf(
      "usage: example [option ...] [other-arguments ...]\n"
      "\n"
      "  -n, --number=NUMBER       set value of variable `number'\n"
      "  -N, --number-func=NUMBER  call function with given integer argument\n"
      "  -D, --double-func=NUMBER  call function with given double argument\n"
      "  -s, --string=STRING       set value of variable `string'\n"
      "  -S, --string-func=STRING  call function with given string argument\n"
      "  -f, --flag                set value of variable `flag' to 1\n"
      "  -F, --flag-func           call a function with no argument\n"
      "\n"
      "A double-dash (`--') indicates that the rest of the line is not to\n"
      "be scanned for options.\n"
    );
    exit(1);
}

/*-----------------------------------------------------------------------+
|  PUBLIC FUNCTIONS                                                      |
+-----------------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
    int  q;                 /* counter */
    int  number = 0;        /* set to anything by -n or --number */
    int  flag = 0;          /* set to 1 by -f or --flag */
    char *string = "none";  /* set to anything by -s or --string */
    optStruct opt[] = {
      /* short long              type        var/func       special       */
        { 'h', "help",           OPT_FLAG,   usage,         OPT_CALLFUNC },
        { 'n', "number",         OPT_INT,    &number,       0            },
        { 'N', "number-func",    OPT_INT,    numberFunc,    OPT_CALLFUNC },
        { 'D', "double-func",    OPT_STRING, doubleFunc,    OPT_CALLFUNC },
        { 's', "string",         OPT_STRING, &string,       0            },
        { 'S', "string-func",    OPT_STRING, stringFunc,    OPT_CALLFUNC },
        { 'f', "flag",           OPT_FLAG,   &flag,         0            },
        { 'F', "flag-func",      OPT_FLAG,   flagFunc,      OPT_CALLFUNC },
        { 0, 0, OPT_END, 0, 0 }  /* no more options */
    };

    /* if you link with the shhmsg-library, you may wish to set the
     * the error handling function by calling
     * optSetFatalFunc(msgFatal); */

    /* parse all options */
    optParseOptions(&argc, argv, opt, 0);

    /* display result */
    printf("number=%d, flag=%d, string=\"%s\"\n", number, flag, string);

    /* what's left in argv, are any non-optinons. */
    printf("remaining arguments: ");
    for (q = 1; q < argc; q++)
        printf("\"%s\" ", argv[q]);
    printf("\n");

    return 0;
}
