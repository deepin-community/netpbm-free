/**************************************************************************
                                  libpm.c
***************************************************************************
  This is the most fundamental Netpbm library.  It contains routines
  not specific to any particular Netpbm format.

  Some of the subroutines in this library are intended and documented
  for use by Netpbm users, but most of them are just used by other
  Netpbm library subroutines.

  Before May 2001, this function was served by the libpbm library
  (in addition to being the library for handling the PBM format).

**************************************************************************/

#include <stdio.h>
#include <limits.h>
#include "version.h"
#include "compile.h"
#include "shhopt.h"
#include "pm.h"

#include <stdarg.h>
#include <string.h>
#include <errno.h>

/* The following are set by pm_init(), then used by subsequent calls to other
   pm_xxx() functions.
   */
static const char* pm_progname;
static bool pm_showmessages;  
    /* Programs should display informational messages (because the user didn't
       specify the --quiet option).
    */

void
pm_usage( const char usage[] ) {
    fprintf( stderr, "usage:  %s %s\n", pm_progname, usage );
    exit( 1 );
    }

void
pm_perror(const char reason[] ) {
    const char* e;

#ifdef A_STRERROR
    e = strerror(errno);
#else /* A_STRERROR */
    e = sys_errlist[errno];
#endif /* A_STRERROR */

    if ( reason != 0 && reason[0] != '\0' )
        pm_error( "%s - %s", reason, e );
    else
        pm_error( "%s", e );
    }

void GNU_PRINTF_ATTR
pm_message(const char format[], ... ) {

    va_list args;

    va_start( args, format );

    if ( pm_showmessages )
        {
        fprintf( stderr, "%s: ", pm_progname );
        (void) vfprintf( stderr, format, args );
        fputc( '\n', stderr );
        }
    va_end( args );
    }

void GNU_PRINTF_ATTR
pm_error(const char format[], ... ) {
    va_list args;

    va_start( args, format );

    fprintf( stderr, "%s: ", pm_progname );
    (void) vfprintf( stderr, format, args );
    fputc( '\n', stderr );
    va_end( args );
    exit( 1 );
    }


/* Variable-sized arrays. */

char*
pm_allocrow(int const cols, int const size) {
    register char* itrow;

    itrow = (char*) malloc2( cols , size );
    if ( itrow == (char*) 0 )
        pm_error( "out of memory allocating a row" );
    return itrow;
    }

void
pm_freerow(char * const itrow) {
    free( itrow );
}


#ifndef A_FRAGARRAY
char**
pm_allocarray(int const cols, int const rows, int const size )  {
    char** its;
    int i;

    its = (char**) malloc2( rows, sizeof(char*) );
    if ( its == (char**) 0 )
        pm_error( "out of memory allocating an array" );
    its[0] = (char*) malloc3( rows, cols, size );
    if ( its[0] == (char*) 0 )
        pm_error( "out of memory allocating an array" );
    for ( i = 1; i < rows; ++i )
        its[i] = &(its[0][i * cols * size]);
    return its;
    }

void
pm_freearray(char ** const its, int const rows) {
    free( its[0] );
    free( its );
}
#else /* A_FRAGARRAY */
char**
pm_allocarray(int const cols, int const rows, int const size) {
    char** its;
    int i;

    overflow_add(rows, 1);
    its = (char**) malloc2( (rows + 1),  sizeof(char*) );
    if ( its == (char**) 0 )
        pm_error( "out of memory allocating an array" );
    its[rows] = its[0] = (char*) malloc3( rows. cols, size );
    if ( its[0] != (char*) 0 )
        for ( i = 1; i < rows; ++i )
            its[i] = &(its[0][i * cols * size]);
    else
        for( i = 0; i < rows; ++i )
            its[i] = pm_allocrow( cols, size );
    return its;
}

void
pm_freearray(char ** const its, int const rows) {
    int i;
    if( its[rows] != (char*) 0 )
        free( its[rows] );
    else
        for( i = 0; i < rows; ++i )
            pm_freerow( its[i] );
    free( its );
}
#endif /* A_FRAGARRAY */


/* Case-insensitive keyword matcher. */

int
pm_keymatch(char * const strarg, char * const keywordarg, int const minchars) {
    register int len;
    char *str, *keyword;
    
    str = strarg;
    keyword = keywordarg;

    len = strlen( str );
    if ( len < minchars )
        return 0;
    while ( --len >= 0 )
        {
        register char c1, c2;

        c1 = *str++;
        c2 = *keyword++;
        if ( c2 == '\0' )
            return 0;
        if ( isupper( c1 ) )
            c1 = tolower( c1 );
        if ( isupper( c2 ) )
            c2 = tolower( c2 );
        if ( c1 != c2 )
            return 0;
        }
    return 1;
}


/* Log base two hacks. */

int
pm_maxvaltobits(int const maxval) {
    if ( maxval <= 1 )
        return 1;
    else if ( maxval <= 3 )
        return 2;
    else if ( maxval <= 7 )
        return 3;
    else if ( maxval <= 15 )
        return 4;
    else if ( maxval <= 31 )
        return 5;
    else if ( maxval <= 63 )
        return 6;
    else if ( maxval <= 127 )
        return 7;
    else if ( maxval <= 255 )
        return 8;
    else if ( maxval <= 511 )
        return 9;
    else if ( maxval <= 1023 )
        return 10;
    else if ( maxval <= 2047 )
        return 11;
    else if ( maxval <= 4095 )
        return 12;
    else if ( maxval <= 8191 )
        return 13;
    else if ( maxval <= 16383 )
        return 14;
    else if ( maxval <= 32767 )
        return 15;
    else if ( (long) maxval <= 65535L )
        return 16;
    else
        pm_error( "maxval of %d is too large!", maxval );
        return -1;  /* Should never come here */
}

int
pm_bitstomaxval(int const bits) {
    return ( 1 << bits ) - 1;
}


unsigned int 
pm_lcm(const unsigned int x, 
       const unsigned int y, 
       const unsigned int z, 
       const unsigned int limit) {
/*----------------------------------------------------------------------------
  Compute the least common multiple of x, y, and z.  If it's bigger than
  'limit', though, just return 'limit'.
-----------------------------------------------------------------------------*/
    unsigned int biggest;
    unsigned int candidate;

    if (x == 0 || y == 0 || z == 0)
        pm_error("pm_lcm(): Least common multiple of zero taken.");

    biggest = max(x, max(y,z));

    candidate = biggest;
    while (((candidate % x) != 0 ||       /* not a multiple of x */
            (candidate % y) != 0 ||       /* not a multiple of y */
            (candidate % z) != 0 ) &&     /* not a multiple of z */
           candidate <= limit)
        candidate += biggest;

    if (candidate > limit) 
        candidate = limit;

    return candidate;
}


/* Initialization. */


#ifdef VMS
static const char *
vmsProgname(int * const argcP, char * argv[]) {   
    char **temp_argv = argv;
    int old_argc = *argcP;
    int i;
    const char * retval;
    
    getredirection( argcP, &temp_argv );
    if (*argcP > old_argc) {
        /* Number of command line arguments has increased */
        fprintf( stderr, "Sorry!! getredirection() for VMS has "
                 "changed the argument list!!!\n");
        fprintf( stderr, "This is intolerable at the present time, "
                 "so we must stop!!!\n");
        exit(1);
    }
    for (i=0; i<*argcP; i++)
        argv[i] = temp_argv[i];
    retval = strrchr( argv[0], '/');
    if ( retval == NULL ) retval = rindex( argv[0], ']');
    if ( retval == NULL ) retval = rindex( argv[0], '>');

    return retval;
}
#endif



void
pm_init(const char * const progname, unsigned int const flags) {
/*----------------------------------------------------------------------------
   Initialize static variables that Netpbm library routines use.

   Any user of Netpbm library routines is expected to call this at the
   beginning of this program, before any other Netpbm library routines.

   A program may call this via pm_proginit() instead, though.
-----------------------------------------------------------------------------*/
    pm_setMessage(FALSE, NULL);

    pm_progname = progname;
    
#ifdef O_BINARY
#ifdef HAVE_SETMODE
    /* Set the stdin and stdout mode to binary.  This means nothing on Unix,
       but matters on Windows.
       
       Note that stdin and stdout aren't necessarily image files.  In
       particular, stdout is sometimes text for human consumption,
       typically printed on the terminal.  Binary mode isn't really
       appropriate for that case.  We do this setting here without
       any knowledge of how stdin and stdout are being used because it is
       easy.  But we do make an exception for the case that we know the
       file is a terminal, to get a little closer to doing the right
       thing.  
    */
    if (!isatty(0)) setmode(0,O_BINARY);  /* Standard Input */
    if (!isatty(1)) setmode(1,O_BINARY);  /* Standard Output */
#endif /* HAVE_SETMODE */
#endif /* O_BINARY */

}



static void
showVersion(void) {
    pm_message( "Using libpbm from Netpbm Version: %s", NETPBM_VERSION );
#if defined(COMPILE_TIME) && defined(COMPILED_BY)
    pm_message( "Compiled %s by user \"%s\"",
                COMPILE_TIME, COMPILED_BY );
#endif
#ifdef BSD
    pm_message( "BSD defined" );
#endif /*BSD*/
#ifdef SYSV
#ifdef VMS
    pm_message( "VMS & SYSV defined" );
#else
    pm_message( "SYSV defined" );
#endif
#endif /*SYSV*/
#ifdef MSDOS
    pm_message( "MSDOS defined" );
#endif /*MSDOS*/
#ifdef AMIGA
    pm_message( "AMIGA defined" );
#endif /* AMIGA */
    {
        const char * rgbdef;
        pm_message( "RGB_ENV='%s'", RGBENV );
        rgbdef = getenv(RGBENV);
        if( rgbdef )
            pm_message( "RGBENV= '%s' (env vbl set to '%s')", 
                        RGBENV, rgbdef );
        else
            pm_message( "RGBENV= '%s' (env vbl is unset)", RGBENV);
    }
}



static void
showNetpbmHelp(const char progname[]) {
/*----------------------------------------------------------------------------
  Tell the user where to get help for this program, assuming it is a Netpbm
  program (a program that comes with the Netpbm package, as opposed to a 
  program that just uses the Netpbm libraries).

  As we are in debian, everything is in the manpages or /usr/share/
-----------------------------------------------------------------------------*/
   pm_message("For more help see man %s or"
		      "the files in /usr/share/doc/netpbm*", progname);
}



void
pm_proginit(int * const argcP, char * argv[]) {
/*----------------------------------------------------------------------------
   Do various initialization things that all programs in the Netpbm package,
   and programs that emulate such programs, should do.

   This includes processing global options.

   This includes calling pm_init() to initialize the Netpbm libraries.
-----------------------------------------------------------------------------*/
    int argn, i;
    const char * progname;
    bool showmessages;
    bool show_version;
        /* We're supposed to just show the version information, then exit the
           program.
        */
    bool show_help;
        /* We're supposed to just tell user where to get help, then exit the
           program.
        */
    
    /* Extract program name. */
#ifdef VMS
    progname = vmsProgname(argcP, argv);
#else
    progname = strrchr( argv[0], '/');
#endif
    if (progname == NULL)
        progname = argv[0];
    else
        ++progname;

    pm_init(progname, 0);

    /* Check for any global args. */
    showmessages = TRUE;
    show_version = FALSE;
    show_help = FALSE;
    for (argn = 1; argn < *argcP; ++argn) {
        if (pm_keymatch(argv[argn], "-quiet", 6) ||
            pm_keymatch(argv[argn], "--quiet", 7)) 
            showmessages = FALSE;
        else if (pm_keymatch(argv[argn], "-version", 8) ||
                   pm_keymatch(argv[argn], "--version", 9)) 
            show_version = TRUE;
        else if (pm_keymatch(argv[argn], "-help", 5) ||
                 pm_keymatch(argv[argn], "--help", 6) ||
                 pm_keymatch(argv[argn], "-?", 2)) 
            show_help = TRUE;
        else
            continue;
        for (i = argn + 1; i <= *argcP; ++i)
            argv[i - 1] = argv[i];
        --(*argcP);
    }

    pm_setMessage((unsigned int) showmessages, NULL);

    if (show_version) {
        showVersion();
        exit( 0 );
    } else if (show_help) {
        pm_error("Use 'man %s' for help.", progname);
        /* If we can figure out a way to distinguish Netpbm programs from 
           other programs using the Netpbm libraries, we can do better here.
        */
        if (0)
            showNetpbmHelp(progname);
        exit(0);
    }
}


void
pm_setMessage(int const newState, int * const oldStateP) {
    
    if (oldStateP)
        *oldStateP = pm_showmessages;

    pm_showmessages = !!newState;
}


char *
pm_arg0toprogname(const char arg0[]) {
/*----------------------------------------------------------------------------
   Given a value for argv[0] (a command name or file name passed to a 
   program in the standard C calling sequence), return the name of the
   Netpbm program to which is refers.

   In the most ordinary case, this is simply the argument itself.

   But if the argument contains a slash, it is the part of the argument 
   after the last slash, and if there is a .exe on it (as there is for
   DJGPP), that is removed.

   The return value is in static storage within.  It is null-terminated,
   but truncated at 64 characters.
-----------------------------------------------------------------------------*/
    static char retval[64+1];
    char *slash_pos;

    /* Chop any directories off the left end */
    slash_pos = strrchr(arg0, '/');

    if (slash_pos == NULL) {
        strncpy(retval, arg0, sizeof(retval));
        retval[sizeof(retval)-1] = '\0';
    } else {
        strncpy(retval, slash_pos +1, sizeof(retval));
        retval[sizeof(retval)-1] = '\0';
    }

    /* Chop any .exe off the right end */
    if (strlen(retval) >= 4 && strcmp(retval+strlen(retval)-4, ".exe") == 0)
        retval[strlen(retval)-4] = 0;

    return(retval);
}



/* File open/close that handles "-" as stdin/stdout and checks errors. */

FILE*
pm_openr(const char * const name) {
    FILE* f;

    if (strcmp(name, "-") == 0)
        f = stdin;
    else {
#ifndef VMS
        f = fopen(name, "rb");
#else
        f = fopen (name, "r", "ctx=stm");
#endif
        if (f == NULL) {
            pm_perror(name);
            exit(1);
        }
    }
    return f;
}

FILE*
pm_openw(const char * const name) {
    FILE* f;

    if (strcmp(name, "-") == 0)
        f = stdout;
    else {
#ifndef VMS
        f = fopen(name, "wb");
#else
        f = fopen (name, "w", "mbc=32", "mbf=2");  /* set buffer factors */
#endif
        if (f == NULL) {
            pm_perror(name);
            exit(1);
        }
    }
    return f;
}



FILE *
pm_openr_seekable(const char name[]) {
/*----------------------------------------------------------------------------
  Open the file named by name[] such that it is seekable (i.e. it can be
  rewound and read in multiple passes with fseek()).

  If the file is actually seekable, this reduces to the same as
  pm_openr().  If not, we copy the named file to a temporary file
  and return that file's stream descriptor.

  We use a file that the operating system recognizes as temporary, so
  it picks the filename and deletes the file when we close it.
-----------------------------------------------------------------------------*/
    int stat_rc;
    int seekable;  /* logical: file is seekable */
    struct stat statbuf;
    FILE * original_file;
    FILE * seekable_file;

    original_file = pm_openr((char *) name);

    /* I would use fseek() to determine if the file is seekable and 
       be a little more general than checking the type of file, but I
       don't have reliable information on how to do that.  I have seen
       streams be partially seekable -- you can, for example seek to
       0 if the file is positioned at 0 but you can't actually back up
       to 0.  I have seen documentation that says the errno for an
       unseekable stream is EBADF and in practice seen ESPIPE.

       On the other hand, regular files are always seekable and even if
       some other file is, it doesn't hurt much to assume it isn't.
    */

    stat_rc = fstat(fileno(original_file), &statbuf);
    if (stat_rc == 0 && S_ISREG(statbuf.st_mode))
        seekable = TRUE;
    else 
        seekable = FALSE;

    if (seekable) {
        seekable_file = original_file;
    } else {
        seekable_file = tmpfile();

        /* Copy the input into the temporary seekable file */
        while (!feof(original_file) && !ferror(original_file) 
               && !ferror(seekable_file)) {
            char buffer[4096];
            int bytes_read;
            bytes_read = fread(buffer, 1, sizeof(buffer), original_file);
            fwrite(buffer, 1, bytes_read, seekable_file);
        }
        if (ferror(original_file))
            pm_error("Error reading input file into temporary file.  "
                     "Errno = %s (%d)", strerror(errno), errno);
        if (ferror(seekable_file))
            pm_error("Error writing input into temporary file.  "
                     "Errno = %s (%d)", strerror(errno), errno);
        pm_close(original_file);
        {
            int seek_rc;
            seek_rc = fseek(seekable_file, 0, SEEK_SET);
            if (seek_rc != 0)
                pm_error("fseek() failed to rewind temporary file.  "
                         "Errno = %s (%d)", strerror(errno), errno);
        }
    }
    return seekable_file;
}



void
pm_close(FILE * const f) {
    fflush( f );
    if ( ferror( f ) )
        pm_message( "a file read or write error occurred at some point" );
    if ( f != stdin )
        if ( fclose( f ) != 0 )
            pm_perror( "fclose" );
}



/* The pnmtopng package uses pm_closer() and pm_closew() instead of 
   pm_close(), apparently because the 1999 Pbmplus package has them.
   I don't know what the difference is supposed to be.
*/

void
pm_closer(FILE * const f) {
    pm_close(f);
}



void
pm_closew(FILE * const f) {
    pm_close(f);
}



/* Endian I/O.
*/

int
pm_readbigshort(FILE * const in, short * const sP) {
    int c;

    if ( (c = getc( in )) == EOF )
        return -1;
    *sP = ( c & 0xff ) << 8;
    if ( (c = getc( in )) == EOF )
        return -1;
    *sP |= c & 0xff;
    return 0;
}

int
pm_writebigshort(FILE* const out, short const s) {
    (void) putc( ( s >> 8 ) & 0xff, out );
    (void) putc( s & 0xff, out );
    return 0;
}

int
pm_readbiglong(FILE * const in, long * const lP) {
    int c;

    if ( (c = getc( in )) == EOF )
        return -1;
    *lP = ( c & 0xff ) << 24;
    if ( (c = getc( in )) == EOF )
        return -1;
    *lP |= ( c & 0xff ) << 16;
    if ( (c = getc( in )) == EOF )
        return -1;
    *lP |= ( c & 0xff ) << 8;
    if ( (c = getc( in )) == EOF )
        return -1;
    *lP |= c & 0xff;
    return 0;
}

int
pm_writebiglong(FILE * const out, long const l) {
    (void) putc( ( l >> 24 ) & 0xff, out );
    (void) putc( ( l >> 16 ) & 0xff, out );
    (void) putc( ( l >> 8 ) & 0xff, out );
    (void) putc( l & 0xff, out );
    return 0;
}

int
pm_readlittleshort(FILE * const in, short * const sP) {
    int c;

    if ( (c = getc( in )) == EOF )
        return -1;
    *sP = c & 0xff;
    if ( (c = getc( in )) == EOF )
        return -1;
    *sP |= ( c & 0xff ) << 8;
    return 0;
}

int
pm_writelittleshort(FILE* const out, short const s) {
    (void) putc( s & 0xff, out );
    (void) putc( ( s >> 8 ) & 0xff, out );
    return 0;
}

int
pm_readlittlelong(FILE * const in, long * const lP) {
    int c;

    if ( (c = getc( in )) == EOF )
        return -1;
    *lP = c & 0xff;
    if ( (c = getc( in )) == EOF )
        return -1;
    *lP |= ( c & 0xff ) << 8;
    if ( (c = getc( in )) == EOF )
        return -1;
    *lP |= ( c & 0xff ) << 16;
    if ( (c = getc( in )) == EOF )
        return -1;
    *lP |= ( c & 0xff ) << 24;
    return 0;
}

int
pm_writelittlelong(FILE * const out, long const l) {
    (void) putc( l & 0xff, out );
    (void) putc( ( l >> 8 ) & 0xff, out );
    (void) putc( ( l >> 16 ) & 0xff, out );
    (void) putc( ( l >> 24 ) & 0xff, out );
    return 0;
}


/* Read a file of unknown size to a buffer. Return the number of bytes
   read. Allocate more memory as we need it. The calling routine has
   to free() the buffer.

   Oliver Trepte, oliver@fysik4.kth.se, 930613 */

#define PM_BUF_SIZE 16384      /* First try this size of the buffer, then
                                   double this until we reach PM_MAX_BUF_INC */
#define PM_MAX_BUF_INC 65536   /* Don't allocate more memory in larger blocks
                                   than this. */

char *pm_read_unknown_size(FILE * const file, long * const nread) {
    long nalloc;
    register int val;
    char* buf;

    *nread = 0;
    if ((buf=malloc(PM_BUF_SIZE)) == NULL)
        pm_error("Cannot allocate memory");
    nalloc = PM_BUF_SIZE;

    while(1) {
        if (*nread >= nalloc) { /* We need a larger buffer */
            if (nalloc > PM_MAX_BUF_INC)
                nalloc += PM_MAX_BUF_INC;
            else
                nalloc += nalloc;
            if ((buf=realloc(buf, nalloc)) == NULL)
                pm_error("Cannot allocate %ld bytes of memory", nalloc);
        }

        if ((val = getc(file)) == EOF)
            return (buf);

        buf[(*nread)++] = val;
    }
}


unsigned int
pm_tell(FILE * const fileP) {
/*----------------------------------------------------------------------------
   Return the current file position.  Abort the program if error,
   including if *fileP isn't a file that has a position.
-----------------------------------------------------------------------------*/
    long const rc = ftell(fileP);

    if (rc < 0)
        pm_error("ftell() to get current file position failed.  "
                 "Errno = %s (%d)\n", strerror(errno), errno);

    return rc;
}


void
pm_seek(FILE * const fileP, unsigned long filepos) {
/*----------------------------------------------------------------------------
   Position file *fileP to position 'filepos'.  Abort if error, including
   if *fileP isn't a seekable file.
-----------------------------------------------------------------------------*/
    fseek(fileP, filepos, SEEK_SET);
}



void
pm_nextimage(FILE * const file, int * const eofP) {

    int c;

    c = getc(file);
    if (c == EOF) {
        if (feof(file))
            *eofP = TRUE;
        else
            pm_error("File error on getc() to position to image");
    } else {
        int rc;
        *eofP = FALSE;
        rc = ungetc(c, file);
        if (rc == EOF) 
            pm_error("File error doing ungetc() to position to image.");
    }
}



void
pm_check(FILE * const file, const enum pm_check_type check_type, 
         const unsigned int need_raster_size,
         enum pm_check_code * const retval_p) {

    struct stat statbuf;
    int curpos;  /* Current position of file; -1 if none */
    int rc;

    curpos = ftell(file);
    if (curpos >= 0) {
        /* This type of file has a current position */
            
        rc = fstat(fileno(file), &statbuf);
        if (rc != 0) 
            pm_error("fstat() failed to get size of file, though ftell() "
                     "successfully identified\n"
                     "the current position.  Errno=%s (%d)",
                     strerror(errno), errno);
        else if (!S_ISREG(statbuf.st_mode)) {
            /* Not a regular file; we can't know its size */
            if (retval_p) *retval_p = PM_CHECK_UNCHECKABLE;
        } else {
            const unsigned int have_raster_size = statbuf.st_size - curpos;
                
            if (have_raster_size < need_raster_size)
                pm_error("File has invalid format.  The raster should "
                         "contain %d bytes, but\n"
                         "the file ends after only %d bytes.",
                         need_raster_size, have_raster_size);
            else if (have_raster_size > need_raster_size) {
                if (retval_p) *retval_p = PM_CHECK_TOO_LONG;
            } else {
                if (retval_p) *retval_p = PM_CHECK_OK;
            }
        }
    } else
        if (retval_p) *retval_p = PM_CHECK_UNCHECKABLE;
}


/*
 *	Maths wrapping
 */

void overflow2(int a, int b)
{
	if(a < 0 || b < 0)
		pm_error("object too large");
	if(b == 0)
		return;
	if(a > INT_MAX / b)
		pm_error("object too large");
}

void overflow3(int a, int b, int c)
{
	overflow2(a,b);
	overflow2(a*b, c);
}

void overflow_add(int a, int b)
{
	if( a > INT_MAX - b)
		pm_error("object too large");
}

void *malloc2(int a, int b)
{
	overflow2(a, b);
	if(a*b == 0)
		pm_error("Zero byte allocation");
	return malloc(a*b);
}

void *malloc3(int a, int b, int c)
{
	overflow3(a, b, c);
	if(a*b*c == 0)
		pm_error("Zero byte allocation");
	return malloc(a*b*c);
}

void *realloc2(void * a, int b, int c)
{
	overflow2(b, c);
	if(b*c == 0)
		pm_error("Zero byte allocation");
	return realloc(a, b*c);
}

