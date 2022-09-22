/******************************************************************************
                                  libpammap.c
*******************************************************************************

  These are functions that deal with tuple hashes and tuple tables.

  Both tuple hashes and tuple tables let you associate an arbitrary
  integer with a tuple value.  A tuple hash lets you look up the one
  integer value (if any) associated with a given tuple value, having
  the low memory and execution time characteristics of a hash table.
  A tuple table lets you scan all the values, being a table of elements
  that consist of an ordered pair of a tuple value and integer.

  (At least) some of these functions were inspired or derived from functions
  in ppm/linppmcmap.c (previously libppm3.c)
  (C) 1989, 1991 by Jef Poskanzer.

  Permission to use, copy, modify, and distribute this software and its
  documentation for any purpose and without fee is hereby granted, provided
  that the above copyright notice appear in all copies and that both that
  copyright notice and this permission notice appear in supporting
  documentation.  This software is provided "as is" without express or
  implied warranty.

  This copyright notice was appended by Andreas Barth
  during a license review for debian in August 2003.

******************************************************************************/

#include "pam.h"
#include "pammap.h"


#define HASH_SIZE 20023

unsigned int
pnm_hashtuple(struct pam * const pamP, tuple const tuple) {
/*----------------------------------------------------------------------------
   Return the hash value of the tuple 'tuple' -- i.e. an index into a hash
   table.
-----------------------------------------------------------------------------*/
    int i;
    unsigned int hash;
    const unsigned int hash_factor[] = {33023, 30013, 27011};

    hash = 0;  /* initial value */
    for (i = 0; i < min(pamP->depth, 3); ++i) {
        hash += tuple[i] * hash_factor[i];  /* May overflow */
    }
    hash %= HASH_SIZE;
    return hash;
}




tuplehash
pnm_createtuplehash(void) {
/*----------------------------------------------------------------------------
   Create an empty tuple hash -- i.e. a hash table of zero length hash chains.
-----------------------------------------------------------------------------*/

    tuplehash const retval = malloc(HASH_SIZE * sizeof(tupleint_list));
    unsigned int i;

    for (i = 0; i < HASH_SIZE; ++i) 
        retval[i] = NULL;

    return retval;
}



void
pnm_destroytuplehash(tuplehash const tuplehash) {


    int i;

    /* Free the chains */

    for (i = 0; i < HASH_SIZE; ++i) {
        struct tupleint_list_item * p;
        struct tupleint_list_item * next;
        
        /* Walk this chain, freeing each element */
        for (p = tuplehash[i]; p; p = next) {
            next = p->next;

            free(p);
        }            
    }

    /* Free the table of chains */
    free(tuplehash);
}




static struct tupleint_list_item * 
allocTupleIntListItem(struct pam * const pamP) {


    /* This is complicated by the fact that the last element of a 
       tupleint_list_item is of variable length, because the last element
       of _it_ is of variable length 
    */
    struct tupleint_list_item * retval;

    retval = (struct tupleint_list_item *)
        malloc(sizeof(*retval) - sizeof(retval->tupleint.tuple) 
               + pamP->depth * sizeof(sample));

    return retval;
}



void
pnm_addtotuplehash(struct pam *   const pamP,
                   tuplehash      const tuplehash, 
                   tuple          const tupletoadd,
                   int            const value,
                   int *          const fitsP) {
/*----------------------------------------------------------------------------
   Add a tuple value to the hash -- assume it isn't already there.

   Allocate new space for the tuple value and the hash chain element.

   If we can't allocate space for the new hash chain element, don't
   change anything and return *fitsP = FALSE;
-----------------------------------------------------------------------------*/
    struct tupleint_list_item * const listItemP = allocTupleIntListItem(pamP);
    if (listItemP == NULL)
        *fitsP = FALSE;
    else {
        unsigned int const hashvalue = pnm_hashtuple(pamP, tupletoadd);
    
        *fitsP = TRUE;

        pnm_assigntuple(pamP, listItemP->tupleint.tuple, tupletoadd);
        listItemP->tupleint.value = value;
        listItemP->next = tuplehash[hashvalue];
        tuplehash[hashvalue] = listItemP;
    }
}



void
pnm_lookuptuple(struct pam * const pamP, const tuplehash tuplehash, 
                const tuple searchval, 
                int * const foundP, int * const retvalP) {
    
    unsigned int const hashvalue = pnm_hashtuple(pamP, searchval);
    struct tupleint_list_item * p;
    struct tupleint_list_item * found;

    found = NULL;  /* None found yet */
    for (p = tuplehash[hashvalue]; p && !found; p = p->next)
        if (pnm_tupleequal(pamP, p->tupleint.tuple, searchval)) {
            found = p;
        }

    if (found) {
        *foundP = TRUE;
        *retvalP = found->tupleint.value;
    } else
        *foundP = FALSE;
}



static tuplehash
computetuplefreqhash(struct pam *   const pamP,
                     tuple **       const tupleArray, 
                     unsigned int   const maxsize, 
                     unsigned int * const sizeP) {
/*----------------------------------------------------------------------------
  Compute a tuple frequency hash from a PAM.  This is a hash that gives
  you the number of times a given tuple value occurs in the PAM.  You can
  supply the input PAM in one of two ways:

  1) a two-dimensional array of tuples tupleArray[][];  In this case,
     'tupleArray' is non-NULL.

  2) an open PAM file, positioned to the raster.  In this case,
     'tupleArray' is NULL.  *pamP contains the file descriptor.
  
     We return with the file still open and its position undefined.  

  In either case, *pamP contains parameters of the tuple array.

  Return the number of unique tuple values found as *sizeP.

  However, if the number of unique tuple values is greater than 'maxsize', 
  return a null return value and *sizeP undefined.

-----------------------------------------------------------------------------*/
    tuplehash tuplefreqhash;
    int row;
    tuple * rowbuffer;  /* malloc'ed */
        /* Buffer for a row read from the input file; undefined (but still
           allocated) if input is not from a file.
        */
    
    tuplefreqhash = pnm_createtuplehash();
    *sizeP = 0;   /* initial value */

    rowbuffer = pnm_allocpamrow(pamP);

    /* Go through the entire raster, building a hash table of tuple values. */
    for (row = 0; row < pamP->height; ++row) {
        int col;
        const tuple * tuplerow;  /* The row of tuples we are processing */

        if (tupleArray)
            tuplerow = tupleArray[row];
        else {
            pnm_readpamrow(pamP, rowbuffer);
            tuplerow = rowbuffer;
        }
        for (col = 0; col < pamP->width; ++col) {
            tuple const tuple = tuplerow[col];
            unsigned int const hashvalue = pnm_hashtuple(pamP, tuple);
            struct tupleint_list_item *p;
            
            for (p = tuplefreqhash[hashvalue]; 
                 p && !pnm_tupleequal(pamP, p->tupleint.tuple, tuple);
                 p = p->next);

            if (p)
                p->tupleint.value++;
            else {
                /* It's not in the hash yet, so add it (if allowed) */
                ++(*sizeP);
                if (maxsize > 0 && *sizeP > maxsize) {
                    pnm_destroytuplehash(tuplefreqhash);
                    return NULL;
                } else {
                    p = allocTupleIntListItem(pamP);
                    if (p == NULL)
                        pm_error("out of memory computing hash table");
                    pnm_assigntuple(pamP, p->tupleint.tuple, tuple);
                    p->tupleint.value = 1;
                    p->next = tuplefreqhash[hashvalue];
                    tuplefreqhash[hashvalue] = p;
                }
            }
        }
    }
    ppm_freerow(rowbuffer);
    return tuplefreqhash;
}



tuplehash
pnm_computetuplefreqhash(struct pam *   const pamP,
                         tuple **       const tupleArray,
                         unsigned int   const maxsize,
                         unsigned int * const sizeP) {
/*----------------------------------------------------------------------------
   Compute the tuple frequency hash for the tuple array tupleArray[][].
-----------------------------------------------------------------------------*/
    return computetuplefreqhash(pamP, tupleArray, maxsize, sizeP);
}




tupletable
pnm_alloctupletable(const struct pam * const pamP, unsigned int const size) {
    
    unsigned int const tupleIntSize = 
        sizeof(struct tupleint) - sizeof(sample) 
        + pamP->depth * sizeof(sample);
    unsigned int const mainTableSize = size * sizeof(struct tupleint *);
    void * pool;
    tupletable retval;
    unsigned int i;
    
    /* To save the enormous amount of time it could take to allocate
       each individual tuple, we do a trick here and allocate everything
       as a single malloc block and suballocate internally.
    */
    
    pool = malloc(mainTableSize + size * tupleIntSize);
    
    retval = (tupletable) pool;

    for (i = 0; i < size; ++i)
        retval[i] = (struct tupleint *)
            ((char*)pool + mainTableSize + i * tupleIntSize);
    
    return retval;
}



void
pnm_freetupletable(struct pam * const pamP, tupletable const tupletable) {

    /* Note that the address 'tupletable' is, to the operating system, 
       the address of a larger block of memory that contains not only 
       tupletable, but all the samples to which it points (e.g.
       tupletable[0].tuple[0])
    */

    free(tupletable);
}



tupletable
pnm_tuplehashtotable(const struct pam * const pamP,
                     tuplehash          const tuplehash,
                     unsigned int       const maxsize) {
/*----------------------------------------------------------------------------
   Create a tuple table containing the info from a tuple hash.  Allocate
   space in the table for 'maxsize' elements even if there aren't that
   many tuple values in the input hash.  That's so the caller has room
   for expansion.

   We allocate new space for all the table contents; there are no pointers
   in the table to tuples or anything else in existing space.
-----------------------------------------------------------------------------*/
    tupletable tupletable;
    int i, j;

    tupletable = pnm_alloctupletable(pamP, maxsize);

    if (tupletable == NULL)
        pm_error("out of memory generating tuple table");

    /* Loop through the hash table. */
    j = 0;
    for (i = 0; i < HASH_SIZE; ++i) {
        /* Walk this hash chain */
        struct tupleint_list_item * p;
        for (p = tuplehash[i]; p; p = p->next) {
            tupletable[j]->value = p->tupleint.value;
            pnm_assigntuple(pamP, tupletable[j]->tuple, p->tupleint.tuple);
            ++j;
        }
    }
    return tupletable;
}



tuplehash
pnm_computetupletablehash(struct pam * const pamP, 
                          tupletable   const tupletable,
                          unsigned int const tupletableSize) {
/*----------------------------------------------------------------------------
   Create a tuple hash containing indices into the tuple table
   'tupletable'.  The hash index for the hash is the value of a tuple;
   the hash value is the tuple table index for the element in the
   tuple table that contains that tuple value.

   Assume there are no duplicate tuple values in the tuple table.

   We allocate space for the main hash table and all the elements of the
   hash chains.
-----------------------------------------------------------------------------*/
    tuplehash tupletablehash;
    unsigned int i;
    
    tupletablehash = pnm_createtuplehash();

    for (i = 0; i < tupletableSize; ++i) {
        bool fits;
        
        pnm_addtotuplehash(pamP, tupletablehash, 
                           tupletable[i]->tuple, i, &fits);
        if (!fits)
            pm_error("Out of memory computing tuple hash from tuple table");
    }
    return tupletablehash;
}



tupletable
pnm_computetuplefreqtable(struct pam *   const pamP,
                          tuple **       const tupleArray,
                          unsigned int   const maxsize,
                          unsigned int * const sizeP) {
/*----------------------------------------------------------------------------
   Compute the a tuple frequency table from the array of tuples 
   tupleArray[][].  This is an array that tells how many times each tuple
   value occurs in tupleArray[][].

   Return the array in newly malloc'ed storage.  Allocate space for 'maxsize'
   entries even if there aren't that many colors in tupleArray[].  That's
   so the caller has room for expansion.

   Return the number of unique tuple values in tupleArray[][], which
   is the size of the output array, as *sizeP.

   If there are more than 'maxsize' unique tuple values in tupleArray[][],
   give up.

-----------------------------------------------------------------------------*/
    tuplehash tuplefreqhash;
    tupletable tuplefreqtable;

    tuplefreqhash = pnm_computetuplefreqhash(pamP, tupleArray, maxsize, sizeP);

    if (tuplefreqhash == NULL)
        tuplefreqtable = NULL;
    else {
        tuplefreqtable = pnm_tuplehashtotable(pamP, tuplefreqhash, maxsize);
        pnm_destroytuplehash(tuplefreqhash);
    }
    return tuplefreqtable;
}



char*
pam_colorname(struct pam *         const pamP, 
              tuple                const color, 
              enum colornameFormat const format) {

    unsigned int r, g, b;
    FILE* f;
    static char colorname[200];

    r = pnm_scalesample(color[PAM_RED_PLANE], pamP->maxval, 255);
    g = pnm_scalesample(color[PAM_GRN_PLANE], pamP->maxval, 255);
    b = pnm_scalesample(color[PAM_BLU_PLANE], pamP->maxval, 255);

    f = pm_openColornameFile(format == PAM_COLORNAME_ENGLISH);
    if (f != NULL) {
        unsigned int best_diff;
        bool done;

        best_diff = 32767;
        done = FALSE;
        while (!done) {
            struct colorfile_entry const ce = pm_colorget(f);
            if (ce.colorname) {
                unsigned int const this_diff = 
                    abs((int)r - (int)ce.r) + 
                    abs((int)g - (int)ce.g) + 
                    abs((int)b - (int)ce.b);

                if (this_diff < best_diff) {
                    best_diff = this_diff;
                    strcpy(colorname, ce.colorname);
                }
            } else
                done = TRUE;
        }
        fclose(f);
        if (best_diff != 32767 && 
            (best_diff == 0 || format == PAM_COLORNAME_ENGLISH))
            return colorname;
    }

    /* Color lookup failed, but caller is willing to take an X11-style
       hex specifier, so return that.
    */
    sprintf(colorname, "#%02x%02x%02x", r, g, b);
    return colorname;
}
