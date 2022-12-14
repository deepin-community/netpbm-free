#ifndef _PM_CONFIG_H
#define _PM_CONFIG_H
/**************************************************************************
                               NETPBM
                              config.h
***************************************************************************
  This file provides platform-dependent definitions for all Netpbm
  libraries and the programs that use them.

  Though it is hardly ever necessary, it is intended that people modify
  this file to adapt the Netpbm distribution to a particular platform.
  In the future, Netpbm's 'configure' program may generate this file
  automatically.

  Wherever possible, Netpbm handles customization via the make files
  instead of via this file.  However, Netpbm's make file philosophy
  discourages lining up a bunch of -D options on every compile, so a 
  #define here would be preferable to a -D compile option.

**************************************************************************/

/* uint32n is a 32 bit unsigned integer.  There are a bunch of other
   conventional names for this data type, but we don't know any that 
   are available in all compilation environments, or absent in all
   compilation environments, so we have to make up our own that we hope
   no one else defines.

   We don't know today of any environment where "unsigned int" is not
   32 bits, but we also don't know that there aren't any, so we have 
   infrastructure here for the eventuality that we find one where 
   uint32n has to be something else.  If you understand the standards
   and conventions for 32 bit vs 64 bit, please enlighten the Netpbm
   maintainer.  2002.01.16.
*/
typedef unsigned int uint32n;
typedef signed int int32n;

#if defined(USG) || defined(SVR4) || defined(VMS) || defined(__SVR4)
#define SYSV
#endif
#if ! ( defined(BSD) || defined(SYSV) || defined(MSDOS) || defined(AMIGA) )
/* CONFIGURE: If your system is >= 4.2BSD, set the BSD option; if you're a
** System V site, set the SYSV option; if you're IBM-compatible, set MSDOS;
** and if you run on an Amiga, set AMIGA. If your compiler is ANSI C, you're
** probably better off setting SYSV - all it affects is string handling.
*/
#define BSD
/* #define SYSV */
/* #define MSDOS */
/* #define AMIGA */
#endif

/* Switch macros like _POSIX_SOURCE are supposed to add features from
   the indicated standard to the C library.  A source file defines one
   of these macros to declare that it uses features of that standard
   as opposed to conflicting features of other standards (e.g. the
   POSIX foo() subroutine might do something different from the X/Open
   foo() subroutine).  Plus, this forces the coder to understand upon
   what feature sets his program relies.

   But some C library developers have misunderstood this and think of these
   macros like the old __ansi__ macro, which tells the C library, "Don't 
   have any features that aren't in the ANSI standard."  I.e. it's just
   the opposite -- the macro subtracts features instead of adding them.

   This means that on some platforms, Netpbm programs must define
   _POSIX_SOURCE, and on others, it must not.  Netpbm's POSIX_IS_IMPLIED 
   macro indicates that we're on a platform where we need not define
   _POSIX_SOURCE (and probably must not).
*/
#if defined(__OpenBSD__) || defined (__NetBSD__) || defined(__bsdi__) || defined(__APPLE__)
#define POSIX_IS_IMPLIED
#endif


/* NOTE: do not use "bool" as a type in an external interface.  It could
   have different definitions on either side of the interface.  Even if both
   sides include this interface header file, the conditional compilation
   here means one side may use the typedef below and the other side may
   use some other definition.  For an external interface, be safe and just
   use "int".
*/

/* We used to assume that if TRUE was defined, then bool was too.
   However, we had a report on 2001.09.21 of a Tru64 system that had
   TRUE but not bool and on 2002.03.21 of an AIX 4.3 system that was
   likewise.  So now we define bool all the time, unless the macro
   HAVE_BOOL is defined.  If someone is using the Netpbm libraries and
   also another library that defines bool, he can either make the
   other library define/respect HAVE_BOOL or just define HAVE_BOOL in
   the file that includes pm_config.h or with a compiler option.  Note
   that C++ always has bool.  
*/
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
/* C++ has a "bool" type built in. */
#ifndef __cplusplus
#ifndef HAVE_BOOL
#define HAVE_BOOL 1
typedef int bool;
#endif
#endif

/* CONFIGURE: If you have an X11-style rgb color names file, define its
** path here.  This is used by PPM to parse color names into rgb values.
** If you don't have such a file, comment this out and use the alternative
** hex and decimal forms to specify colors (see ppm/pgmtoppm.1 for details).  */
/* There was some evidence before Netpbm 9.1 that the rgb database macros
   might be already set right now.  I couldn't figure out how, so I changed
   their meanings and they are now set unconditionally.  -Bryan 00.05.03.
*/
#ifdef VMS
#define RGB_DB1 "PBMplus_Dir:RGB.TXT"
#define RGB_DB2 "PBMplus_Dir:RGB.TXT"
#define RGB_DB3 "PBMplus_Dir:RGB.TXT"
#else
#define RGB_DB1 "/usr/lib/X11/rgb.txt"
#define RGB_DB2 "/etc/X11/rgb.txt"
#define RGB_DB3 "/usr/X11R6/lib/X11/rgb.txt"
#endif

/* CONFIGURE: This is the name of an environment variable that tells
** where the color names database is.  If the environment variable isn't
** set, Netpbm tries the hardcoded defaults set above.
*/
#define RGBENV "RGBDEF"    /* name of env-var */

/* CONFIGURE: Normally, PPM handles a pixel as a struct of three grays.
** If grays are represented as chars, that's 24 bits per color pixel; if
** grays are represented as ints, that's usually 96 bits per color pixel.  PPM
** can be modified to pack the three samples into a single longword,
** 10 bits each, for 32 bits per pixel.
**
** If you don't need more than 10 bits for each color component, AND
** you care more about memory use than speed, then this option might
** be a win.  Under these circumstances it will make some of the
** programs use 3 times less space, but all of the programs will run
** slower.  In one test, it was 1.4 times slower.
** 
*/
/* #define PPM_PACKCOLORS */

/* CONFIGURE: uncomment this to enable debugging checks. */
/* #define DEBUG */

#if ( defined(SYSV) || defined(AMIGA) )

#include <string.h>
/* Before Netpbm 9.1, rand and srand were macros for random and
   srandom here.  This caused a failure on a SunOS 5.6 system, which
   is SYSV, but has both rand and random declared (with different
   return types).  The macro caused the prototype for random to be a
   second prototype for rand.  Before 9.1, Netpbm programs called
   random() and on a SVID system, that was really a call to rand().
   We assume all modern systems have rand() itself, so now Netpbm
   always calls rand() and if we find a platform that doesn't have
   rand(), we will add something here for that platform.  -Bryan 00.04.26
#define random rand
#define srandom(s) srand(s)
extern void srand();
extern int rand();
*/
/* Before Netpbm 9.15, there were macro definitions of index() and 
   rindex() here, but there are no longer any invocations of those 
   functions in Netpbm, except in the VMS-only code, so there's no
   reason for them.
*/

#ifndef __SASC
#ifndef _DCC    /* Amiga DICE Compiler */
#define bzero(dst,len) memset(dst,0,len)
#define bcopy(src,dst,len) memcpy(dst,src,len)
#define bcmp memcmp
#endif /* _DCC */
#endif /* __SASC */

#endif /*SYSV or AMIGA*/

#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
/* 
   Before Netpbm 9.0, atoi() and exit() were declared for everybody
   except MSDOS and AMIGA, and time() and write() were declared for
   everybody except MSDOS, AMIGA, and __osf__.  fcntl.h, time.h, and
   stlib.h were included for MSDOS and AMIGA, and unistd.h was included
   for everyone except VMS, MSDOS, and AMIGA.  With the netbsd patches,
   atoi(), exit(), time(), and write() were not declared for __NetBSD__.

   We're hoping that all current systems have the standard header
   files, and will reinstate some of these explicit declarations if we
   hear otherwise.  

   If it turns out to be this easy, we should just move these inclusions
   to the source files that actually need them.
   
   -Bryan 2000.04.13

extern int atoi();
extern void exit();
extern long time();
extern int write(); 
*/

/* CONFIGURE: On most BSD systems, malloc() gets declared in stdlib.h, on
** system V, it gets declared in malloc.h. On some systems, malloc.h
** doesn't declare these, so we have to do it here. On other systems,
** for example HP/UX, it declares them incompatibly.  And some systems,
** for example Dynix, don't have a malloc.h at all.  A sad situation.
** If you have compilation problems that point here, feel free to tweak
** or remove these declarations.
*/
#ifdef BSD
#include <stdlib.h>
#endif
#if (defined(SYSV) && !defined(VMS))
#include <malloc.h>
#endif
/* extern char* malloc(); */
/* extern char* realloc(); */
/* extern char* calloc(); */

/* CONFIGURE: Some systems don't have vfprintf(), which we need for the
** error-reporting routines.  If you compile and get a link error about
** this routine, uncomment the first define, which gives you a vfprintf
** that uses the theoretically non-portable but fairly common routine
** _doprnt().  If you then get a link error about _doprnt, or
** message-printing doesn't look like it's working, try the second
** define instead.
*/
/* #define NEED_VFPRINTF1 */
/* #define NEED_VFPRINTF2 */

/* CONFIGURE: Some systems don't have strstr(), which some routines need.
** If you compile and get a link error about this routine, uncomment the
** define, which gives you a strstr.
*/
/* #define NEED_STRSTR */

/* CONFIGURE: Set this option if your compiler uses strerror(errno)
** instead of sys_errlist[errno] for error messages.
*/
#define A_STRERROR

/* CONFIGURE: On small systems without VM it is possible that there is
** enough memory for a large array, but it is fragmented.  So the usual
** malloc( all-in-one-big-chunk ) fails.  With this option, if the first
** method fails, pm_allocarray() tries to allocate the array row by row.
*/
/* #define A_FRAGARRAY */

/* CONFIGURE: If your system has the setmode() function, set HAVE_SETMODE.
** If you do, and also the O_BINARY file mode, pm_init() will set the mode
** of stdin and stdout to binary for all Netpbm programs.
** You need this with Cygwin (Windows).
*/
#ifdef __CYGWIN__
#define HAVE_SETMODE
#endif

/* #define HAVE_SETMODE */

#ifdef AMIGA
#include <clib/exec_protos.h>
#define getpid(x)   ((long)FindTask(NULL))
#endif /* AMIGA */

#ifdef DJGPP
#define HAVE_SETMODE
#define lstat stat
#endif /* DJGPP */

/*  CONFIGURE: Netpbm uses __inline__ to declare functions that should
    be compiled as inline code.  GNU C recognizes the __inline__ keyword.
    If your compiler recognizes any other keyword for this, you can set
    it here.
*/
#ifndef __GNUC__
#ifndef __inline__
#ifdef __sgi
#define __inline__ __inline
#else
#define __inline__
#endif
#endif
#endif

/* CONFIGURE: Some systems seem to need more than standard program linkage
   to get a data (as opposed to function) item out of a library.

   On Windows mingw systems, it seems you have to #include <import_mingw.h>
   and #define EXTERNDATA DLL_IMPORT  .  2001.05.19
*/
#define EXTERNDATA extern

#endif
