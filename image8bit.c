/// image8bit - A simple image processing module.
///
/// This module is part of a programming project
/// for the course AED, DETI / UA.PT
///
/// You may freely use and modify this code, at your own risk,
/// as long as you give proper credit to the original and subsequent authors.
///
/// João Manuel Rodrigues <jmr@ua.pt>
/// 2013, 2023

// Student authors (fill in below):
// NMec:  Name:
// 
// 
// 
// Date:
//

#include "image8bit.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "instrumentation.h"

// The data structure
//
// An image is stored in a structure containing 3 fields:
// Two integers store the image width and height.
// The other field is a pointer to an array that stores the 8-bit gray
// level of each pixel in the image.  The pixel array is one-dimensional
// and corresponds to a "raster scan" of the image from left to right,
// top to bottom.
// For example, in a 100-pixel wide image (img->width == 100),
//   pixel position (x,y) = (33,0) is stored in img->pixel[33];
//   pixel position (x,y) = (22,1) is stored in img->pixel[122].
// 
// Clients should use images only through variables of type Image,
// which are pointers to the image structure, and should not access the
// structure fields directly.

// Maximum value you can store in a pixel (maximum maxval accepted)
const uint8 PixMax = 255;

// Internal structure for storing 8-bit graymap images
struct image {
  int width;
  int height;
  int maxval;   // maximum gray value (pixels with maxval are pure WHITE)
  uint8* pixel; // pixel data (a raster scan)
};


// This module follows "design-by-contract" principles.
// Read `Design-by-Contract.md` for more details.

/// Error handling functions

// In this module, only functions dealing with memory allocation or file
// (I/O) operations use defensive techniques.
// 
// When one of these functions fails, it signals this by returning an error
// value such as NULL or 0 (see function documentation), and sets an internal
// variable (errCause) to a string indicating the failure cause.
// The errno global variable thoroughly used in the standard library is
// carefully preserved and propagated, and clients can use it together with
// the ImageErrMsg() function to produce informative error messages.
// The use of the GNU standard library error() function is recommended for
// this purpose.
//
// Additional information:  man 3 errno;  man 3 error;

// Variable to preserve errno temporarily
static int errsave = 0;

// Error cause
static char* errCause;

/// Error cause.
/// After some other module function fails (and returns an error code),
/// calling this function retrieves an appropriate message describing the
/// failure cause.  This may be used together with global variable errno
/// to produce informative error messages (using error(), for instance).
///
/// After a successful operation, the result is not garanteed (it might be
/// the previous error cause).  It is not meant to be used in that situation!
char* ImageErrMsg() { ///
  return errCause;
}


// Defensive programming aids
//
// Proper defensive programming in C, which lacks an exception mechanism,
// generally leads to possibly long chains of function calls, error checking,
// cleanup code, and return statements:
//   if ( funA(x) == errorA ) { return errorX; }
//   if ( funB(x) == errorB ) { cleanupForA(); return errorY; }
//   if ( funC(x) == errorC ) { cleanupForB(); cleanupForA(); return errorZ; }
//
// Understanding such chains is difficult, and writing them is boring, messy
// and error-prone.  Programmers tend to overlook the intricate details,
// and end up producing unsafe and sometimes incorrect programs.
//
// In this module, we try to deal with these chains using a somewhat
// unorthodox technique.  It resorts to a very simple internal function
// (check) that is used to wrap the function calls and error tests, and chain
// them into a long Boolean expression that reflects the success of the entire
// operation:
//   success = 
//   check( funA(x) != error , "MsgFailA" ) &&
//   check( funB(x) != error , "MsgFailB" ) &&
//   check( funC(x) != error , "MsgFailC" ) ;
//   if (!success) {
//     conditionalCleanupCode();
//   }
//   return success;
// 
// When a function fails, the chain is interrupted, thanks to the
// short-circuit && operator, and execution jumps to the cleanup code.
// Meanwhile, check() set errCause to an appropriate message.
// 
// This technique has some legibility issues and is not always applicable,
// but it is quite concise, and concentrates cleanup code in a single place.
// 
// See example utilization in ImageLoad and ImageSave.
//
// (You are not required to use this in your code!)


// Check a condition and set errCause to failmsg in case of failure.
// This may be used to chain a sequence of operations and verify its success.
// Propagates the condition.
// Preserves global errno!
static int check(int condition, const char* failmsg) {
  errCause = (char*)(condition ? "" : failmsg);
  return condition;
}


/// Init Image library.  (Call once!)
/// Currently, simply calibrate instrumentation and set names of counters.
void ImageInit(void) { ///
  InstrCalibrate();
  InstrName[0] = "pixmem";  // InstrCount[0] will count pixel array acesses
  // Name other counters here...
  InstrName[1] = "pixmemwr";  // InstrName[1] will count pixel array writes
  InstrName[2] = "pixmemre";  // InstrName[2] will count pixel array reads
  InstrName[3] = "pixcomp";  // InstrName[3] will count pixel comparisons
}

// Macros to simplify accessing instrumentation counters:
#define PIXMEM InstrCount[0]
// Add more macros here...
#define PIXMEMWR InstrCount[1]
#define PIXMEMRE InstrCount[2]
#define PIXCOMP InstrCount[3]

// TIP: Search for PIXMEM or InstrCount to see where it is incremented!


/// Image management functions

/// Create a new black image.
///   width, height : the dimensions of the new image.
///   maxval: the maximum gray level (corresponding to white).
/// Requires: width and height must be non-negative, maxval > 0.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageCreate(int width, int height, uint8 maxval) { ///
  assert (width >= 0);
  assert (height >= 0);
  assert (0 < maxval && maxval <= PixMax);

  const Image image = (Image)malloc(sizeof(struct image));
  if (check(image == NULL, "Cannot allocate memory for image")) {
    return NULL;
  }

  *image = (struct image){
      .width = width,
      .height = height,
      .maxval = maxval,
      .pixel = (uint8*)calloc(width * height, sizeof(uint8)),
  };

  if (check(image->pixel == NULL, "Cannot allocate memory for pixel data")) {
    free(image);
    return NULL;
  }
  // calloc initializes allocated array to 0, so count all the writes
  PIXMEM += (unsigned long) (width * height);
  PIXMEMWR += (unsigned long) (width * height);

  return image;
}

/// Destroy the image pointed to by (*imgp).
///   imgp : address of an Image variable.
/// If (*imgp)==NULL, no operation is performed.
/// Ensures: (*imgp)==NULL.
/// Should never fail, and should preserve global errno/errCause.
void ImageDestroy(Image* imgp) { ///
  assert (imgp != NULL);

  const Image image = *imgp;
  if (image != NULL) {
    // Pixel data pointer will never be NULL on a valid image
    free(image->pixel);
    free(image);
    *imgp = NULL;
  }

  assert (*imgp == NULL);
}

/// PGM file operations

// See also:
// PGM format specification: http://netpbm.sourceforge.net/doc/pgm.html

// Match and skip 0 or more comment lines in file f.
// Comments start with a # and continue until the end-of-line, inclusive.
// Returns the number of comments skipped.
static int skipComments(FILE* f) {
  char c;
  int i = 0;
  while (fscanf(f, "#%*[^\n]%c", &c) == 1 && c == '\n') {
    i++;
  }
  return i;
}

/// Load a raw PGM file.
/// Only 8 bit PGM files are accepted.
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageLoad(const char* filename) { ///
  int w, h;
  int maxval;
  char c;
  FILE* f = NULL;
  Image img = NULL;

  int success = 
  check( (f = fopen(filename, "rb")) != NULL, "Open failed" ) &&
  // Parse PGM header
  check( fscanf(f, "P%c ", &c) == 1 && c == '5' , "Invalid file format" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d ", &w) == 1 && w >= 0 , "Invalid width" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d ", &h) == 1 && h >= 0 , "Invalid height" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d", &maxval) == 1 && 0 < maxval && maxval <= (int)PixMax , "Invalid maxval" ) &&
  check( fscanf(f, "%c", &c) == 1 && isspace(c) , "Whitespace expected" ) &&
  // Allocate image
  (img = ImageCreate(w, h, (uint8)maxval)) != NULL &&
  // Read pixels
  check( fread(img->pixel, sizeof(uint8), w*h, f) == w*h , "Reading pixels" );
  PIXMEM += (unsigned long)(w*h);  // count pixel memory accesses

  // Cleanup
  if (!success) {
    errsave = errno;
    ImageDestroy(&img);
    errno = errsave;
  }
  if (f != NULL) fclose(f);
  return img;
}

/// Save image to PGM file.
/// On success, returns nonzero.
/// On failure, returns 0, errno/errCause are set appropriately, and
/// a partial and invalid file may be left in the system.
int ImageSave(Image img, const char* filename) { ///
  assert (img != NULL);
  int w = img->width;
  int h = img->height;
  uint8 maxval = img->maxval;
  FILE* f = NULL;

  int success =
  check( (f = fopen(filename, "wb")) != NULL, "Open failed" ) &&
  check( fprintf(f, "P5\n%d %d\n%u\n", w, h, maxval) > 0, "Writing header failed" ) &&
  check( fwrite(img->pixel, sizeof(uint8), w*h, f) == w*h, "Writing pixels failed" ); 
  PIXMEM += (unsigned long)(w*h);  // count pixel memory accesses

  // Cleanup
  if (f != NULL) fclose(f);
  return success;
}


/// Information queries

/// These functions do not modify the image and never fail.

/// Get image width
int ImageWidth(Image img) { ///
  assert (img != NULL);
  return img->width;
}

/// Get image height
int ImageHeight(Image img) { ///
  assert (img != NULL);
  return img->height;
}

/// Get image maximum gray level
int ImageMaxval(Image img) { ///
  assert (img != NULL);
  return img->maxval;
}

/// Pixel stats
/// Find the minimum and maximum gray levels in image.
///   img: image to 
/// Requires: img, min and max must not be NULL,
///           values pointed to by min and max must be initialized.
/// Ensures: The image is not modified
/// 
/// On return,
/// *min is set to the minimum gray level in the image,
/// *max is set to the maximum.
void ImageStats(Image img, uint8* min, uint8* max) { ///
  assert (img != NULL);
  assert (min != NULL && max != NULL);

  for (int y = 0; y < img->height; ++y) {
    for (int x = 0; x < img->width; ++x) {
      const uint8 pixelValue = ImageGetPixel(img, x, y);
      if (pixelValue < *min) {
        *min = pixelValue;
      }
      if (pixelValue > *max) {
        *max = pixelValue;
      }
    }
  }
}

/// Check if pixel position (x,y) is inside img.
int ImageValidPos(Image img, int x, int y) { ///
  assert (img != NULL);
  return (0 <= x && x < img->width) && (0 <= y && y < img->height);
}

/// Check if rectangular area (x,y,w,h) is completely inside img.
/// Requires: img must not be NULL.
///           x, y, width and height must be non-negative.
int ImageValidRect(Image img, int x, int y, int w, int h) {
  assert (img != NULL);
  assert (x >= 0);
  assert (y >= 0);
  assert (w >= 0);
  assert (h >= 0);

  return ImageValidPos(img, x, y) && x + w <= img->width
         && y + h <= img->height;
}

/// Pixel get & set operations

/// These are the primitive operations to access and modify a single pixel
/// in the image.
/// These are very simple, but fundamental operations, which may be used to 
/// implement more complex operations.

// Transform (x, y) coords into linear pixel index.
// This internal function is used in ImageGetPixel / ImageSetPixel. 
// The returned index must satisfy (0 <= index < img->width*img->height)
static inline int G(Image img, int x, int y) {
  int index = y * img->width + x;

  assert (0 <= index && index < img->width*img->height);
  return index;
}

/// Get the pixel (level) at position (x,y).
uint8 ImageGetPixel(Image img, int x, int y) { ///
  assert (img != NULL);
  assert (ImageValidPos(img, x, y));
  PIXMEM += 1;  // count one pixel access (read)
  PIXMEMRE += 1;  // count one pixel read
  return img->pixel[G(img, x, y)];
} 

/// Set the pixel at position (x,y) to new level.
void ImageSetPixel(Image img, int x, int y, uint8 level) { ///
  assert (img != NULL);
  assert (ImageValidPos(img, x, y));
  PIXMEM += 1;  // count one pixel access (store)
  PIXMEMWR += 1;  // count one pixel store
  img->pixel[G(img, x, y)] = level;
} 


/// Pixel transformations

/// These functions modify the pixel levels in an image, but do not change
/// pixel positions or image geometry in any way.
/// All of these functions modify the image in-place: no allocation involved.
/// They never fail.


/// Transform image to negative image.
///   img : the image to modify.
/// Requires: img must not be NULL.
/// 
/// This transforms dark pixels to light pixels and vice-versa,
/// resulting in a "photographic negative" effect.
void ImageNegative(Image img) { ///
  assert (img != NULL);

  for (int y = 0; y < img->height; ++y) {
    for (int x = 0; x < img->width; ++x) {
      ImageSetPixel(img, x, y, img->maxval - ImageGetPixel(img, x, y));
    }
  }
}

/// Apply threshold to image.
///   img : the image to modify.
///   thr : the threshold to check to the image's pixels against.
/// Requires: img must not be NULL.
/// 
/// Transform all pixels with level<thr to black (0) and
/// all pixels with level>=thr to white (maxval).
void ImageThreshold(Image img, uint8 thr) { ///
  assert (img != NULL);

  for (int y = 0; y < img->height; ++y) {
    for (int x = 0; x < img->width; ++x) {
      ImageSetPixel(
          img, x, y, ImageGetPixel(img, x, y) < thr ? 0 : img->maxval);
    }
  }
}

/// Return the minimum integer between x and y.
static int minInt(int x, int y) {
  if (x < y) {
    return x;
  }
  return y;
}

/// Clamp x between min and max.
/// If x < min, returns min.
/// If x > max, returns max.
/// Otherwise returns x.
static int clampInt(int x, int min, int max) {
  if (x < min) {
    return min;
  }
  if (x > max) {
    return max;
  }
  return x;
}

/// Clamp x between min and max.
/// If x < min, returns min.
/// If x > max, returns max.
/// Otherwise returns x.
static double clampDouble(double x, double min, double max) {
  if (x > max) {
    return max;
  }
  if (x < min) {
    return min;
  }
  return x;
}

/// Brighten image by a factor.
///   img : the image to brighten.
/// Requires: img must not be NULL.
/// 
/// Multiply each pixel level by a factor, but saturate at maxval.
/// This will brighten the image if factor>1.0 and
/// darken the image if factor<1.0.
void ImageBrighten(Image img, double factor) { ///
  assert (img != NULL);
  // ? assert (factor >= 0.0);

  for (int y = 0; y < img->height; ++y) {
    for (int x = 0; x < img->width; ++x) {
      const double brightenedValue = factor * (double)ImageGetPixel(img, x, y);

      // Since `round` from `math.h` is inaccessible, we resort to adding 0.5.
      // This does not work for negative numbers, but `clampDouble` makes sure
      // we never have a negative number.
      ImageSetPixel(img, x, y, (uint8)clampDouble(brightenedValue + 0.5, 0.0, (double)img->maxval));
    }
  }
}

/// Geometric transformations

/// These functions apply geometric transformations to an image,
/// returning a new image as a result.
/// 
/// Success and failure are treated as in ImageCreate:
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.

// Implementation hint: 
// Call ImageCreate whenever you need a new image!

/// Rotate an image.
/// Returns a rotated version of the image.
/// The rotation is 90 degrees anti-clockwise.
///   img : the image to rotate.
/// Requires: img must not be NULL.
/// Ensures: The original img is not modified.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageRotate(Image img) { ///
  assert (img != NULL);

  const Image rotated = ImageCreate(img->height, img->width, img->maxval);
  if (check(rotated == NULL, "Cannot allocate memory for rotated image")) {
    return NULL;
  }

  for (int y = 0; y < img->height; ++y) {
    for (int x = 0; x < img->width; ++x) {
      ImageSetPixel(rotated, y, rotated->height - 1 - x, ImageGetPixel(img, x, y));
    }
  }

  return rotated;
}

/// Mirror an image = flip left-right.
///   img : the image to mirror.
/// Returns a mirrored version of the image.
/// Requires: img must not be NULL.
/// Ensures: The original img is not modified.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageMirror(Image img) { ///
  assert (img != NULL);

  const Image mirrored = ImageCreate(img->width, img->height, img->maxval);
  if (check(mirrored == NULL, "Cannot allocate memory for mirrored image")) {
    return NULL;
  }

  for (int y = 0; y < img->height; ++y) {
    for (int x = 0; x < img->width; ++x) {
      ImageSetPixel(mirrored, mirrored->width - 1 - x, y, ImageGetPixel(img, x, y));
    }
  }

  return mirrored;
}

/// Crop a rectangular subimage from img.
/// The rectangle is specified by the top left corner coords (x, y) and
/// width w and height h.
/// Requires:
///   The rectangle must be inside the original image.
/// Ensures:
///   The original img is not modified.
///   The returned image has width w and height h.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageCrop(Image img, int x, int y, int w, int h) { ///
  assert (img != NULL);
  assert (ImageValidRect(img, x, y, w, h));

  const Image cropped = ImageCreate(w, h, img->maxval);
  if (check(cropped == NULL, "Cannot allocate memory for cropped image")) {
    return NULL;
  }

  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      // TODO: Should this map pixel values based on the images' max values?
      // Tests don't scream at us, but they don't test images with different
      // max values.
      ImageSetPixel(cropped, i, j, ImageGetPixel(img, x + i, y + j));
    }
  }

  assert (cropped->width == w && cropped->height == h);
  return cropped;
}

/// Operations on two images

/// Paste an image into a larger image.
/// Paste img2 into position (x, y) of img1.
/// This modifies img1 in-place: no allocation involved.
/// Requires: img1 and img2 must not be NULL.
///           img2 must fit inside img1 at position (x, y).
/// Ensures: img2 is not modified.
void ImagePaste(Image img1, int x, int y, Image img2) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  assert (ImageValidRect(img1, x, y, img2->width, img2->height));

  for (int j = 0; j < img2->height; ++j) {
    for (int i = 0; i < img2->width; ++i) {
      const uint8 pixel = ImageGetPixel(img2, i, j);
      ImageSetPixel(img1, x + i, y + j, pixel);
    }
  }
}

/// Blend an image into a larger image.
/// Blend img2 into position (x, y) of img1.
/// This modifies img1 in-place: no allocation involved.
/// Requires: img1 and img must not be NULL.
///           img2 must fit inside img1 at position (x, y).
/// Ensures: img2 is not modified,
/// alpha usually is in [0.0, 1.0], but values outside that interval
/// may provide interesting effects.  Over/underflows should saturate.
void ImageBlend(Image img1, int x, int y, Image img2, double alpha) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  assert (ImageValidRect(img1, x, y, img2->width, img2->height));

  for (int j = 0; j < img2->height; ++j) {
    for (int i = 0; i < img2->width; ++i) {
      const uint8 pixel1 = ImageGetPixel(img1, x + i, y + j);
      const uint8 pixel2 = ImageGetPixel(img2, i, j);
      const double blendedPixel = (1 - alpha) * (double) pixel1 + alpha * (double) pixel2;

      // Add 0.5 to pixel value just like in ImageBrighten.
      ImageSetPixel(img1, x + i, y + j, (uint8) clampDouble(blendedPixel + 0.5, 0.0, (double)img1->maxval));
    }
  }
}

/// Compare an image to a subimage of a larger image.
/// Requires: img1 and img2 must not be NULL.
/// Ensures: The images are not modified.
/// Returns 1 (true) if img2 matches subimage of img1 at pos (x, y).
/// Returns 0, otherwise.
int ImageMatchSubImage(Image img1, int x, int y, Image img2) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  assert (ImageValidPos(img1, x, y));

  for (int j = 0; j < minInt(img1->height - y, img2->width); ++j) {
    for (int i = 0; i < minInt(img1->width - x, img2->height); ++i) {
      PIXCOMP += 1;
      if (ImageGetPixel(img1, x + i, y + j) != ImageGetPixel(img2, i, j)) {
        return 0;
      }
    }
  }

  return 1;
}

/// Locate a subimage inside another image.
/// Searches for img2 inside img1.
/// Requires: img1, img2, px and py  must not be NULL.
/// Ensures: The images are not modified.
/// 
/// If a match is found, returns 1 and matching position is set in vars (*px, *py).
/// If no match is found, returns 0 and (*px, *py) are left untouched.
int ImageLocateSubImage(Image img1, int* px, int* py, Image img2) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  assert (ImageValidRect(img1, 0, 0, img2->width, img2->height));
  assert (px != NULL);
  assert (py != NULL);

  for (int j = 0; j <= img1->height - img2->height; ++j) {
    for (int i = 0; i <= img1->width - img2->width; ++i) {
      if (ImageMatchSubImage(img1, i, j, img2)) {
        *px = i;
        *py = j;
        return 1;
      }
    }
  }

  return 0;
}


/// Filtering

/// Blur an image by a applying a (2dx+1)x(2dy+1) mean filter.
/// Each pixel is substituted by the mean of the pixels in the rectangle
/// Requires: img must not be NULL.
///           dx and dy must not be negative.
/// [x-dx, x+dx]x[y-dy, y+dy].
/// The image is changed in-place.
void ImageBlur(Image img, int dx, int dy) { ///
  assert (img != NULL);
  assert (dx >= 0);
  assert (dy >= 0);

  Image blurred = ImageCreate(img->width, img->height, img->maxval);
  for (int y = 0; y < img->height; ++y) {
    for (int x = 0; x < img->width; ++x) {
      int sum = 0;

      const int start_x = clampInt(x - dx, 0, img->width - 1);
      const int end_x = clampInt(x + dx, 0, img->width - 1);
      const int start_y = clampInt(y - dy, 0, img->height - 1);
      const int end_y = clampInt(y + dy, 0, img->height - 1);

      for (int j = start_y; j <= end_y; ++j) {
        for (int i = start_x; i <= end_x; ++i) {
          sum += ImageGetPixel(
            img,
            i,
            j
          );
        }
      }

      ImageSetPixel(blurred,
        x,
        y,
        (uint8)((double) sum / (double) ((end_x - start_x + 1) * (end_y - start_y + 1)) + 0.5)
      );
    }
  }

  ImagePaste(img, 0, 0, blurred);
  ImageDestroy(&blurred);
}

