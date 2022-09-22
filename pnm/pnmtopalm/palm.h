#define PALM_IS_COMPRESSED_FLAG		0x8000
#define PALM_HAS_COLORMAP_FLAG		0x4000
#define PALM_HAS_TRANSPARENCY_FLAG	0x2000
#define PALM_DIRECT_COLOR		0x0400
#define PALM_4_BYTE_FIELD		0x0200

#define PALM_COMPRESSION_SCANLINE	0x00
#define PALM_COMPRESSION_RLE		0x01
#define PALM_COMPRESSION_PACKBITS	0x02
#define PALM_COMPRESSION_NONE		0xFF

typedef unsigned long Color_s, *Color;

typedef struct {
  unsigned int nentries;	/* number of allocated Colors */
  unsigned int ncolors;		/* number of actually used Colors */
  Color color_entries;		/* pointer to vector of Colors */
} Colormap_s, *Colormap;

extern int palmcolor_compare_indices (const void *, const void *);
extern int palmcolor_compare_colors (const void *, const void *);

extern Colormap palmcolor_build_custom_8bit_colormap (unsigned int rows, unsigned int cols, pixel **pixels);
extern Colormap palmcolor_build_default_8bit_colormap ();
extern Colormap palmcolor_read_colormap(FILE *);
