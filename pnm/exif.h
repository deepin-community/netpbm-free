typedef unsigned char uchar;

#define MAX_COMMENT 1000

/*--------------------------------------------------------------------------
  This structure stores Exif header image elements in a simple manner
  Used to store camera data as extracted from the various ways that it can be
  stored in an exif header
--------------------------------------------------------------------------*/
typedef struct {
    char  CameraMake   [32];
    char  CameraModel  [40];
    char  DateTime     [20];
    int   Height, Width;
    int   IsColor;
    int   FlashUsed;
    float FocalLength;
    float ExposureTime;
    float ApertureFNumber;
    float Distance;
    float CCDWidth;
    float ExposureBias;
    int   Whitebalance;
    int   MeteringMode;
    int   ExposureProgram;
    int   ISOequivalent;
    int   CompressionLevel;
    char  Comments[MAX_COMMENT];

    unsigned char * ThumbnailPointer;  /* Pointer at the thumbnail */
    unsigned ThumbnailSize;     /* Size of thumbnail. */

}ImageInfo_t;


/* Prototypes for exif.c functions. */

void 
process_EXIF (unsigned char * CharBuf, unsigned int length,
              ImageInfo_t * ImageInfoP, int const ShowTags);

void 
ShowImageInfo(ImageInfo_t * const ImageInfoP);

