/*----------------------------------------------------------------------------
                                 libopt
------------------------------------------------------------------------------
  This is a program to convert link library filepaths to linker options
  that select them.  E.g. ../pbm/libpbm.so becomes -L../pbm -lpbm   .

  Each argument is a library filepath.  The option string to identify
  all of those library filepaths goes to Standard Output.

  If an argument doesn't make sense as a library filespec, it is
  copied verbatim, blank delimited, to the output string.

  The "lib" part of the library name, which we call the prefix, may be
  other than "lib".  The list of recognized values is compiled in as
  the macro SHLIBPREFIXLIST (see below).
  
  There is no newline or null character or anything after the output.
------------------------------------------------------------------------------
  Why would you want to use this?  

  On some systems, the -L/-l output of this program has exactly the
  same effect as the filepath input when used in the arguments to a
  link command.  A GNU/Linux system, for example.  On others (Solaris,
  for example), if you include /tmp/lib/libpbm.so in the link as a
  link object, the executable gets built in such a way that the system
  accesses the shared library /tmp/lib/libpbm.so at run time.  On the
  other hand, if you instead put the options -L/tmp/lib -lpbm on the
  link command, the executable gets built so that the system accesses
  libpbm.so in its actual installed directory at runtime (that
  location might be determined by a --rpath linker option or a
  LD_LIBRARY_PATH environment variable at run time).

  In a make file, it is nice to use the same variable as the
  dependency of rule that builds an executable and as the thing that
  the rule's command uses to identify its input.  Here is an example
  of using libopt for that:

     PBMLIB=../pbm/libpbm.so
     ...
     pbmmake: pbmmake.o $(PBMLIB)
             ld -o pbmmake pbmmake.o `libopt $(PBMLIB)` --rpath=/lib/netpbm

  Caveat: "-L../pbm -lpbm" is NOT exactly the same as
  "../pbm/libpbm.so" on any system.  All of the -l libraries are
  searched for in all of the -L directories.  So you just might get a
  different library with the -L/-l version than if you specify the
  library file explicitly.

-----------------------------------------------------------------------------*/
#define _BSD_SOURCE 1
    /* Make sure strdup() is in stdio.h */
#define MAX_PREFIXES 10

/* Here's how to use SHLIBPREFIXLIST:  Use a -D compile option to pass in
   a value appropriate for the platform on which you are linking libraries.

   It's a blank-delimited list of prefixes that library names might
   have.  "lib" is traditionally the only prefix, as in libc or
   libpbm.  However, on Windows there is a convention of using
   different prefixes to distinguish different co-existent versions of
   the same library (kind of like a major number in some unices). 
   E.g. the value "cyg lib" is appropriate for a Cygwin system.
*/
#ifndef SHLIBPREFIXLIST
#  define SHLIBPREFIXLIST "lib"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef unsigned char bool;
#define true (1)
#define false (0)

static void
parse_prefixlist(const char * const prefixlist, 
                 char * parsed_prefixes[MAX_PREFIXES+1],
                 bool * const errorP) {
/*----------------------------------------------------------------------------
   Given a space-separated list of tokens (suffixes), return an
   argv-style array of pointers, each to a newly malloc'ed storage
   area the prefix as a null-terminated string.  Return a null string
   for the unused entries at the end of the array

   We never return more than MAX_PREFIXES prefixes from the input, so
   there is guaranteed always to be one null string at the end of the
   array.

   In case of error, return *errorP == true and don't allocate any
   storage.  Otherwise, return *errorP = false.
-----------------------------------------------------------------------------*/
    char * prlist;

    prlist = strdup(prefixlist);
    if (prlist == NULL)
        *errorP = true;
    else {
        if (strlen(prlist) <= 0) 
            *errorP = true;
        else {
            /* NOTE: Mac OS X, at least, does not have strtok_r().
               2001.09.24
            */
            char * token;
            int num_tokens;
            int i;
            
            for (i=0; i < MAX_PREFIXES + 1; i++) {
                parsed_prefixes[i] = NULL;
            }
            num_tokens = 0;
            token = strtok(prlist, " ");
            *errorP = false;  /* initial value */
            while (token != NULL && num_tokens < MAX_PREFIXES && !*errorP) {
                parsed_prefixes[num_tokens] = strdup (token);
                if (parsed_prefixes[num_tokens] == NULL) 
                    *errorP = true;
                num_tokens++;
                token = strtok(NULL, " ");
            }
            for (i = num_tokens; i < MAX_PREFIXES + 1 && !*errorP;  i++) {
                parsed_prefixes[i] = strdup("");
                if (parsed_prefixes[i] == NULL) 
                    *errorP = true;
            }
        }
        if (*errorP) {
            /* Deallocate any array entries we successfully did */
            int i;
            for (i = 0; i < MAX_PREFIXES + 1; i++)
                if (parsed_prefixes[i])
                    free(parsed_prefixes[i]);
        }
        free(prlist);
    }
}



static void
parse_prefix(const char * const filename, 
             bool * const prefix_good_p, unsigned int * const prefix_length_p,
             bool * const error_p) {
/*----------------------------------------------------------------------------
   Find the library name prefix (e.g. "lib") in the library filename
   'filename'.
   
   Return the length of the prefix, in characters, as *prefix_length_p.
   (The prefix always starts at the beginning of the filename).

   Iff we don't find a valid library name prefix, return *prefix_good_p
   == false.  

   The list of valid prefixes is compiled in as the blank-delimited
   string which is the value of the SHLIBPREFIXLIST macro.
-----------------------------------------------------------------------------*/
    char * shlibprefixlist[MAX_PREFIXES+1];
        /* An argv-style array of prefix strings in the first entries, 
           null strings in the later entries.  At most MAX_PREFIXES prefixes,
           so at least one null string.
        */
    char * prefix;
        /* The prefix that the filename actually
           uses.  e.g. if shlibprefixlist = { "lib", "cyg", "", ... } and the
           filename is "path/to/cyg<something>.<extension>", then 
           prefix = "cyg".  String is in the same storage as pointed to by
           shlibprefixlist (shlibprefixlist[1] in this example).
        */
    bool prefix_good;
        /* The first part of the filename matched one of the prefixes
           in shlibprefixlist[].
        */
    int prefix_length;
    int i;

    parse_prefixlist(SHLIBPREFIXLIST , shlibprefixlist, error_p);
    if (!*error_p) {
        if (strcmp(shlibprefixlist[0], "") == 0) {
            fprintf(stderr, "libopt was compiled with an invalid value "
                    "of the SHLIBPREFIX macro.  It seems to have no "
                    "tokens.  SHLIBPREFIX = '%s'", 
                    SHLIBPREFIXLIST);
            exit(100);
        }

        i = 0;  /* start with the first entry in shlibprefixlist[] */
        prefix_length = 0;  /* initial value */
        prefix = shlibprefixlist[i];
        prefix_good = false;  /* initial value */
        while ( (*prefix != '\0' ) && !prefix_good ) {
            /* stop condition: shlibprefixlist has MAX_PREFIXES+1 entries.
             * we only ever put tokens in the 0..MAX_PREFIXES-1 positions.
             * Then, we fill DOWN from the MAX_PREFIXES position with '\0'
             * so we insure that the shlibprefixlist array contains at 
             * least one final '\0' string, but probably many '\0' 
             * strings (depending on how many tokens there were).               
             */
            prefix_length = strlen(prefix);
            if (strncmp(filename, prefix, prefix_length) == 0) {
                prefix_good = true;
                /* at this point, prefix is pointing to the correct
                 * entry, and prefix_length has the correct value.
                 * When we bail out of the while loop because of the
                 * !prefix_good clause, we can then use these 
                 * vars (prefix, prefix_length) 
                 */
            } else {
                prefix = shlibprefixlist[++i];
            }
        }
        *prefix_length_p = prefix_length;
        *prefix_good_p = prefix_good;
        { 
            int i;
            for (i=0; i < MAX_PREFIXES + 1; i++) 
                free (shlibprefixlist[i]);
        }
    }
}



static void
parse_filename(const char * const filename,
               char ** const libname_p,
               bool * const valid_library_p,
               bool * const error_p) {
/*----------------------------------------------------------------------------
   Extract the library name root component of the filename 'filename'.  This
   is just a filename, not a whole pathname.

   Return it in newly malloc'ed storage pointed to by '*libname_p'.
   
   E.g. for "libxyz.so", return "xyz".

   return *valid_library == true iff 'filename' validly names a library
   that can be expressed in a -l linker option.

   return *error_p == true iff some error such as out of memory prevents
   parsing.

   Do not allocate any memory if *error_p == true or *valid_library == false.
-----------------------------------------------------------------------------*/
    char *lastdot;  
    /* Pointer to last period in 'filename'.  Null if none */
    
    /* We accept any period-delimited suffix as a library type suffix.
       It's probably .so or .a, but is could be .kalamazoo for all we
       care. (HOWEVER, the double-suffixed import lib used on 
       cygwin (.dll.a) is NOT understood). 
    */

    lastdot = strrchr(filename, '.');
    if (lastdot == NULL) {
        /* This filename doesn't have any suffix, so we don't understand
           it as a library filename.
        */
        *valid_library_p = false;
        *error_p = false;
    } else {
        unsigned int prefix_length;
        bool prefix_good;
        parse_prefix(filename, &prefix_good, &prefix_length, error_p);
        if (!*error_p) {
            if (!prefix_good) {
                *valid_library_p = false;
            } else {
                /* Extract everything between <prefix> and "." as 
                   the library name root. 
                */
                *libname_p = strdup(filename + prefix_length);
                if (*libname_p == NULL)
                    *error_p = true;
                else {
                    (*libname_p)[lastdot - filename - prefix_length] = '\0';
                
                    if (strlen(*libname_p) == 0) {
                        *valid_library_p = false;
                        free(*libname_p);
                    } else
                        *valid_library_p = true;
                }
            }
        }
    }
}   

static void
parse_filepath(const char * const filepath,
               char ** const directory_p, char ** const filename_p,
               bool * const error_p) {
/*----------------------------------------------------------------------------
   Extract the directory and filename components of the filepath 
   'filepath' and return them in newly malloc'ed storage pointed to by
   '*directory_p' and '*filename_p'.
-----------------------------------------------------------------------------*/
    char *lastslash; /* Pointer to last slash in 'filepath', or null if none */

    lastslash = strrchr(filepath, '/');

    if (lastslash == NULL) {
        /* There's no directory component; the filename starts at the
           beginning of the filepath 
        */
        *filename_p = strdup(filepath);
        if (*filename_p == NULL)
            *error_p = true;
        else {
            *directory_p = strdup("");
            if (*directory_p == NULL) {
                *error_p = true;
                free(*filename_p);
            } else
                *error_p = false;
        }
    } else {
        /* Split the string at the slash we just found, into filename and 
           directory 
           */
        *filename_p = strdup(lastslash+1);
        if (*filename_p == NULL)
            *error_p = true;
        else {
            *directory_p = strdup(filepath);
            if (*directory_p == NULL) {
                *error_p = true;
                free(*filename_p);
            } else {
                *error_p = false;
                (*directory_p)[lastslash - filepath] = '\0';
            }
        }
    }
}



int
main(int argc, char **argv) {

    bool error;
    int retval;
    unsigned int arg;  /* Index into argv[] of argument we're processing */

    error = false;  /* no error yet */
    for (arg = 1; arg < argc && !error; arg++) {
        const char *filepath;  /* The argument we're processing */
        bool valid_library;  
          /* Our argument is a valid library filepath that can be converted to
             -l/-L notation.
          */
        char *directory = NULL;
          /* Directory component of 'filepath' */
        char *filename = NULL;
          /* Filename component of 'filepath' */
        char *libname = NULL;
          /* Library name component of 'filename'.  e.g. xyz in libxyz.so */

        filepath = argv[arg];

        parse_filepath(filepath, &directory, &filename, &error);
        if (!error) {
            parse_filename(filename, &libname, &valid_library, &error);

            if (!error) {
                if (valid_library) {
                    if (strlen(directory) == 0)
                        fprintf(stdout, "-L. -l%s ", libname);
                    else
                        fprintf(stdout, "-L%s -l%s ", directory, libname);
                    free(libname);
                } else
                    fprintf(stdout, "%s ", filepath);
            }
            free(directory); 
            free(filename);
        }
    }
    if (error) {
        fprintf(stderr, "serious libopt error prevented parsing library "
                "names.  Invalid input to libopt is NOT the problem.\n");
        retval = 10;
    } else 
        retval = 0;

    return retval;
}





