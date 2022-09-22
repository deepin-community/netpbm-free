#include "pnm.h"

#include "palm.h"

int palmcolor_compare_indices (const void *p1, const void *p2)
{
  if ((*((Color) p1) & 0xFF000000) < (*((Color) p2) & 0xFF000000))
    return -1;
  else if ((*((Color) p1) & 0xFF000000) > (*((Color) p2) & 0xFF000000))
    return 1;
  else
    return 0;
}

int palmcolor_compare_colors (const void *p1, const void *p2)
{
  register unsigned long val1 = *((const unsigned long *) p1) & 0xFFFFFF;
  register unsigned long val2 = *((const unsigned long *) p2) & 0xFFFFFF;

  if (val1 < val2)
    return -1;
  else if (val1 > val2)
    return 1;
  else
    return 0;
}

/***********************************************************************
 ***********************************************************************
 ***********************************************************************
 ******* colortables from pilrc-2.6/bitmap.c ***************************
 ***********************************************************************
 ***********************************************************************
 ***********************************************************************/

#if 0

/*
 * The 1bit-2 color system palette for Palm Computing Devices.
 */
static int PalmPalette1bpp[2][3] = 
{
  { 255, 255, 255}, {   0,   0,   0 }
};

/*
 * The 2bit-4 color system palette for Palm Computing Devices.
 */
static int PalmPalette2bpp[4][3] = 
{
  { 255, 255, 255}, { 192, 192, 192}, { 128, 128, 128 }, {   0,   0,   0 }
};

/*
 * The 4bit-16 color system palette for Palm Computing Devices.
 */
static int PalmPalette4bpp[16][3] = 
{
  { 255, 255, 255}, { 238, 238, 238 }, { 221, 221, 221 }, { 204, 204, 204 },
  { 187, 187, 187}, { 170, 170, 170 }, { 153, 153, 153 }, { 136, 136, 136 },
  { 119, 119, 119}, { 102, 102, 102 }, {  85,  85,  85 }, {  68,  68,  68 },
  {  51,  51,  51}, {  34,  34,  34 }, {  17,  17,  17 }, {   0,   0,   0 }
};

/*
 * The 4bit-16 color system palette for Palm Computing Devices.
 */
static int PalmPalette4bppColor[16][3] = 
{
  { 255, 255, 255}, { 128, 128, 128 }, { 128,   0,   0 }, { 128, 128,   0 },
  {   0, 128,   0}, {   0, 128, 128 }, {   0,   0, 128 }, { 128,   0, 128 },
  { 255,   0, 255}, { 192, 192, 192 }, { 255,   0,   0 }, { 255, 255,   0 },
  {   0, 255,   0}, {   0, 255, 255 }, {   0,   0, 255 }, {   0,   0,   0 }
};

#endif  /* 0 */

/*
 * The 8bit-256 color system palette for Palm Computing Devices.
 *
 * NOTE:  only the first 231, plus the last one, are valid.
 */
static int PalmPalette8bpp[256][3] = 
{
  { 255, 255, 255 }, { 255, 204, 255 }, { 255, 153, 255 }, { 255, 102, 255 }, 
  { 255,  51, 255 }, { 255,   0, 255 }, { 255, 255, 204 }, { 255, 204, 204 }, 
  { 255, 153, 204 }, { 255, 102, 204 }, { 255,  51, 204 }, { 255,   0, 204 }, 
  { 255, 255, 153 }, { 255, 204, 153 }, { 255, 153, 153 }, { 255, 102, 153 }, 
  { 255,  51, 153 }, { 255,   0, 153 }, { 204, 255, 255 }, { 204, 204, 255 },
  { 204, 153, 255 }, { 204, 102, 255 }, { 204,  51, 255 }, { 204,   0, 255 },
  { 204, 255, 204 }, { 204, 204, 204 }, { 204, 153, 204 }, { 204, 102, 204 },
  { 204,  51, 204 }, { 204,   0, 204 }, { 204, 255, 153 }, { 204, 204, 153 },
  { 204, 153, 153 }, { 204, 102, 153 }, { 204,  51, 153 }, { 204,   0, 153 },
  { 153, 255, 255 }, { 153, 204, 255 }, { 153, 153, 255 }, { 153, 102, 255 },
  { 153,  51, 255 }, { 153,   0, 255 }, { 153, 255, 204 }, { 153, 204, 204 },
  { 153, 153, 204 }, { 153, 102, 204 }, { 153,  51, 204 }, { 153,   0, 204 },
  { 153, 255, 153 }, { 153, 204, 153 }, { 153, 153, 153 }, { 153, 102, 153 },
  { 153,  51, 153 }, { 153,   0, 153 }, { 102, 255, 255 }, { 102, 204, 255 },
  { 102, 153, 255 }, { 102, 102, 255 }, { 102,  51, 255 }, { 102,   0, 255 },
  { 102, 255, 204 }, { 102, 204, 204 }, { 102, 153, 204 }, { 102, 102, 204 },
  { 102,  51, 204 }, { 102,   0, 204 }, { 102, 255, 153 }, { 102, 204, 153 },
  { 102, 153, 153 }, { 102, 102, 153 }, { 102,  51, 153 }, { 102,   0, 153 },
  {  51, 255, 255 }, {  51, 204, 255 }, {  51, 153, 255 }, {  51, 102, 255 },
  {  51,  51, 255 }, {  51,   0, 255 }, {  51, 255, 204 }, {  51, 204, 204 },
  {  51, 153, 204 }, {  51, 102, 204 }, {  51,  51, 204 }, {  51,   0, 204 },
  {  51, 255, 153 }, {  51, 204, 153 }, {  51, 153, 153 }, {  51, 102, 153 },
  {  51,  51, 153 }, {  51,   0, 153 }, {   0, 255, 255 }, {   0, 204, 255 },
  {   0, 153, 255 }, {   0, 102, 255 }, {   0,  51, 255 }, {   0,   0, 255 },
  {   0, 255, 204 }, {   0, 204, 204 }, {   0, 153, 204 }, {   0, 102, 204 },
  {   0,  51, 204 }, {   0,   0, 204 }, {   0, 255, 153 }, {   0, 204, 153 },
  {   0, 153, 153 }, {   0, 102, 153 }, {   0,  51, 153 }, {   0,   0, 153 },
  { 255, 255, 102 }, { 255, 204, 102 }, { 255, 153, 102 }, { 255, 102, 102 },
  { 255,  51, 102 }, { 255,   0, 102 }, { 255, 255,  51 }, { 255, 204,  51 },
  { 255, 153,  51 }, { 255, 102,  51 }, { 255,  51,  51 }, { 255,   0,  51 },
  { 255, 255,   0 }, { 255, 204,   0 }, { 255, 153,   0 }, { 255, 102,   0 },
  { 255,  51,   0 }, { 255,   0,   0 }, { 204, 255, 102 }, { 204, 204, 102 },
  { 204, 153, 102 }, { 204, 102, 102 }, { 204,  51, 102 }, { 204,   0, 102 },
  { 204, 255,  51 }, { 204, 204,  51 }, { 204, 153,  51 }, { 204, 102,  51 },
  { 204,  51,  51 }, { 204,   0,  51 }, { 204, 255,   0 }, { 204, 204,   0 },
  { 204, 153,   0 }, { 204, 102,   0 }, { 204,  51,   0 }, { 204,   0,   0 },
  { 153, 255, 102 }, { 153, 204, 102 }, { 153, 153, 102 }, { 153, 102, 102 },
  { 153,  51, 102 }, { 153,   0, 102 }, { 153, 255,  51 }, { 153, 204,  51 },
  { 153, 153,  51 }, { 153, 102,  51 }, { 153,  51,  51 }, { 153,   0,  51 },
  { 153, 255,   0 }, { 153, 204,   0 }, { 153, 153,   0 }, { 153, 102,   0 },
  { 153,  51,   0 }, { 153,   0,   0 }, { 102, 255, 102 }, { 102, 204, 102 },
  { 102, 153, 102 }, { 102, 102, 102 }, { 102,  51, 102 }, { 102,   0, 102 },
  { 102, 255,  51 }, { 102, 204,  51 }, { 102, 153,  51 }, { 102, 102,  51 },
  { 102,  51,  51 }, { 102,   0,  51 }, { 102, 255,   0 }, { 102, 204,   0 },
  { 102, 153,   0 }, { 102, 102,   0 }, { 102,  51,   0 }, { 102,   0,   0 },
  {  51, 255, 102 }, {  51, 204, 102 }, {  51, 153, 102 }, {  51, 102, 102 },
  {  51,  51, 102 }, {  51,   0, 102 }, {  51, 255,  51 }, {  51, 204,  51 },
  {  51, 153,  51 }, {  51, 102,  51 }, {  51,  51,  51 }, {  51,   0,  51 },
  {  51, 255,   0 }, {  51, 204,   0 }, {  51, 153,   0 }, {  51, 102,   0 },
  {  51,  51,   0 }, {  51,   0,   0 }, {   0, 255, 102 }, {   0, 204, 102 },
  {   0, 153, 102 }, {   0, 102, 102 }, {   0,  51, 102 }, {   0,   0, 102 },
  {   0, 255,  51 }, {   0, 204,  51 }, {   0, 153,  51 }, {   0, 102,  51 },
  {   0,  51,  51 }, {   0,   0,  51 }, {   0, 255,   0 }, {   0, 204,   0 },
  {   0, 153,   0 }, {   0, 102,   0 }, {   0,  51,   0 }, {  17,  17,  17 },
  {  34,  34,  34 }, {  68,  68,  68 }, {  85,  85,  85 }, { 119, 119, 119 },
  { 136, 136, 136 }, { 170, 170, 170 }, { 187, 187, 187 }, { 221, 221, 221 },
  { 238, 238, 238 }, { 192, 192, 192 }, { 128,   0,   0 }, { 128,   0, 128 },
  {   0, 128,   0 }, {   0, 128, 128 }, {   0,   0,   0 }, {   0,   0,   0 },
  {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 },
  {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 },
  {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 },
  {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 },
  {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 },
  {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 }
};

Colormap
  palmcolor_build_default_8bit_colormap ()
{
  unsigned int i;

  Colormap cm = malloc(sizeof(Colormap_s));
  cm->color_entries = malloc(sizeof(Color_s) * 232);
  cm->nentries = 232;
  cm->ncolors = 232;
  /* now fill in the colors */
  for (i = 0; i < 231; i++) {
    cm->color_entries[i] = ((i << 24) |
			    (PalmPalette8bpp[i][0] << 16) |
			    (PalmPalette8bpp[i][1] << 8) |
			    (PalmPalette8bpp[i][2]));
  }
  cm->color_entries[231] = 0xFF000000;

  /* now sort the table */
  qsort (cm->color_entries, cm->ncolors, sizeof(Color_s), 
         palmcolor_compare_colors);
  return cm;
}

Colormap
palmcolor_build_custom_8bit_colormap (unsigned int rows, unsigned int cols,
                                      pixel **pixels)
{
  unsigned int row;
  register unsigned int col;
  register pixel *pP;
  Color_s temp;
  Color found;
  Colormap colormap;

  colormap = malloc(sizeof(Colormap_s));
  colormap->color_entries = malloc(sizeof(Color_s) * 256);
  colormap->nentries = 256;
  colormap->ncolors = 0;

  for ( row = 0; row < rows; ++row ) {
    for ( col = 0, pP = pixels[row]; col < cols; ++col, ++pP ) {
      temp = ((PPM_GETR(*pP) << 16) |
	      (PPM_GETG(*pP) << 8) |
	      PPM_GETB(*pP));
      found = (bsearch (&temp,
			colormap->color_entries, colormap->ncolors,
			sizeof(Color_s), palmcolor_compare_colors));
      if (!found) {
	/* add the new color, and re-sort */
	if (colormap->nentries <= colormap->ncolors)
	  pm_error("Too many colors for custom colormap (max 256).  "
		   "Try using ppmquant to reduce the number of colors.");
	else {
	  temp |= ((colormap->ncolors) << 24);
	  colormap->color_entries[colormap->ncolors] = temp;
	  colormap->ncolors += 1;
	  qsort (colormap->color_entries, colormap->ncolors, sizeof(Color_s), 
             palmcolor_compare_colors);
	  if (colormap->ncolors == 256)
	    return colormap;
	}
      }
    }
  }

  return colormap;
}

Colormap
  palmcolor_read_colormap (FILE *ifp)
{
  unsigned int i;
  long colorentry;
  unsigned short ncolors;
  Colormap colormap;

  if (pm_readbigshort(ifp, (short *) &ncolors) != 0)
    return 0;

  colormap = malloc(sizeof(Colormap_s));
  colormap->color_entries = malloc2(sizeof(Color_s), ncolors);
  colormap->nentries = ncolors;
  colormap->ncolors = ncolors;

  for (i = 0;  i < ncolors;  i++) {
    if (pm_readbiglong(ifp, &colorentry) != 0) {
      free (colormap->color_entries);
      free (colormap);
      return 0;
    }
    colormap->color_entries[i] = (colorentry & 0xFFFFFFFF);
  }

  return colormap;
}
