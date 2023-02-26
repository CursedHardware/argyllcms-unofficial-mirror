#ifndef ICC_H
#define ICC_H

/* 
 * International Color Consortium Format Library (icclib)
 *
 * Author:  Graeme W. Gill
 * Date:    1999/11/29
 * Version: 3.0.0
 *
 * Copyright 1997 - 2022 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 */

/* We can get some subtle errors if certain headers aren't included */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <sys/types.h>

#ifdef __cplusplus
	extern "C" {
#endif

/* Version of icclib release */

#define ICCLIB_VERSION 0x030000			/* Format is MaMiBf */  
#define ICCLIB_VERSION_STR "3.0.0"

/*
 *  Note XYZ scaling to 1.0, not 100.0
 */

#undef ICC_DEBUG_MALLOC		/* Turns on partial support for filename and linenumber capture */

/* Make allowance for shared library use */
#ifdef ICCLIB_SHARED		/* Compiling or Using shared library version */
# ifdef ICCLIB_EXPORTS		/* Compiling shared library */
#  ifdef NT
#   define ICCLIB_API __declspec(dllexport)
#  endif /* NT */
# else						/* Using shared library */
#  ifdef NT
#   define ICCLIB_API __declspec(dllimport)
#   ifdef ICCLIB_DEBUG
#    pragma comment (lib, "icclibd.lib")
#   else
#    pragma comment (lib, "icclib.lib")
#   endif	/* DEBUG */
#  endif /* NT */
# endif
#else						/* Using static library */
# define ICCLIB_API	/* empty */
#endif

#if UINT_MAX < 0xffffffff
# error "icclib: integer size is too small, must be at least 32 bit"
#endif

/* =========================================================== */
/* Platform specific primitive defines. */
/* This needs checking for each different platform. */
/* Using C99 and MSC covers a lot of cases, */
/* and the fallback default is pretty reliable with modern compilers and machines. */
/* Note that MSWin is LLP64 == 32 bit long, while OS X/Linux is LP64 == 64 bit long. */
/* so long shouldn't be used in any code outside these #defines.... */

/* Use __P64__ as cross platform 64 bit pointer #define */
#if defined(__LP64__) || defined(__ILP64__) || defined(__LLP64__) || defined(_WIN64)
# define __P64__ 1
#endif

#ifndef ORD32			/* If not defined elsewhere */

#if (__STDC_VERSION__ >= 199901L)	/* C99 */		\
 || defined(_STDINT_H_) || defined(_STDINT_H)		\
 || defined(_SYS_TYPES_H)

#include <stdint.h> 

#define INR8   int8_t		/* 8 bit signed */
#define INR16  int16_t		/* 16 bit signed */
#define INR32  int32_t		/* 32 bit signed */
#define INR64  int64_t		/* 64 bit signed - not used in icclib */
#define ORD8   uint8_t		/* 8 bit unsigned */
#define ORD16  uint16_t		/* 16 bit unsigned */
#define ORD32  uint32_t		/* 32 bit unsigned */
#define ORD64  uint64_t		/* 64 bit unsigned - not used in icclib */

#define PNTR intptr_t

#define PF64PREC "ll"		/* printf format precision specifier */
#define CF64PREC "LL"		/* Constant precision specifier */

#ifndef ATTRIBUTE_NORETURN
# ifdef _MSC_VER
#  define ATTRIBUTE_NORETURN __declspec(noreturn)
# else
#  define ATTRIBUTE_NORETURN __attribute__((noreturn))
# endif
#endif

#else  /* !__STDC_VERSION__ */

#ifdef _MSC_VER

#define INR8   __int8				/* 8 bit signed */
#define INR16  __int16				/* 16 bit signed */
#define INR32  __int32				/* 32 bit signed */
#define INR64  __int64				/* 64 bit signed - not used in icclib */
#define ORD8   unsigned __int8		/* 8 bit unsigned */
#define ORD16  unsigned __int16		/* 16 bit unsigned */
#define ORD32  unsigned __int32		/* 32 bit unsigned */
#define ORD64  unsigned __int64		/* 64 bit unsigned - not used in icclib */

#define PNTR UINT_PTR

#define vsnprintf _vsnprintf
#define snprintf _snprintf

#define PF64PREC "I64"				/* printf format precision specifier */
#define CF64PREC "LL"				/* Constant precision specifier */

#ifndef ATTRIBUTE_NORETURN
# define ATTRIBUTE_NORETURN __declspec(noreturn)
#endif

#else  /* !_MSC_VER */

/* The following works on a lot of modern systems, including */
/* LLP64 and LP64 models, but won't work with ILP64 which needs int32 */

#define INR8   signed char		/* 8 bit signed */
#define INR16  signed short		/* 16 bit signed */
#define INR32  signed int		/* 32 bit signed */
#define ORD8   unsigned char	/* 8 bit unsigned */
#define ORD16  unsigned short	/* 16 bit unsigned */
#define ORD32  unsigned int		/* 32 bit unsigned */

#ifdef __GNUC__
# ifdef __LP64__	/* long long could be 128 bit ? */
#  define INR64  long				/* 64 bit signed - not used in icclib */
#  define ORD64  unsigned long		/* 64 bit unsigned - not used in icclib */
#  define PF64PREC "l"				/* printf format precision specifier */
#  define CF64PREC "L"				/* Constant precision specifier */
# else
#  define INR64  long long			/* 64 bit signed - not used in icclib */
#  define ORD64  unsigned long long	/* 64 bit unsigned - not used in icclib */
#  define PF64PREC "ll"				/* printf format precision specifier */
#  define CF64PREC "LL"				/* Constant precision specifier */
# endif /* !__LP64__ */
#endif /* __GNUC__ */

#define PNTR unsigned long 

#ifndef ATTRIBUTE_NORETURN
# define ATTRIBUTE_NORETURN __attribute__((noreturn))
#endif

#endif /* !_MSC_VER */
#endif /* !__STDC_VERSION__ */
#endif /* !ORD32 */
 
typedef ORD32	icmUTF32;
typedef ORD16	icmUTF16;
typedef ORD8	icmUTF8;

/* =========================================================== */
#include "iccV44.h"	/* ICC Version 4.4 definitions. */ 

/* The prefix icm is used for the native Machine */
/* equivalents of the ICC binary file structures, and other icclib */
/* specific definitions. */

/* =================================================== */

struct _icc;		/* Forward declaration */

/* =================================================== */
/* An error reporting context. */

/* Perhaps this could be expanded to save any error arguments and */
/* their type too, to allow multi-localisation. */
/* Make this fixed size to allow creaton on stack, struct copy etc. */

#define ICM_ERRM_SIZE 2000		/* Try and keep in sync with CGATS_ERRM_LENGTH */

typedef struct {
	int  c;            		/* Error code */
	char m[ICM_ERRM_SIZE];	/* Error message */
} icmErr;

/* Set the error code and error message in an icmErr context. */
/* Ignored if e == NULL */
/* Return err */
int icm_err_e(icmErr *e, int err, const char *format, ...);

/* Same as above using a va_list */
int icm_verr_e(icmErr *e, int err, const char *format, va_list vp);

/* Clear any error and message */
/* Ignored if e == NULL */
void icm_err_clear_e(icmErr *e);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* icclib Error codes */

/* General codes */
#define ICM_ERR_OK				0		/* No error */
#define ICM_ERR_MALLOC        	1		/* A malloc failed. (errm could be NULL) */

/* ICC low level codes */
#define ICM_ERR_FILE_OPEN		        0x101	/* Open failed */
#define ICM_ERR_FILE_SEEK		        0x102	/* Seek failed */
#define ICM_ERR_FILE_READ		        0x103	/* Read failed */
#define ICM_ERR_FILE_WRITE		        0x104	/* Write failed */
#define ICM_ERR_BUFFER_BOUND	        0x105	/* Attemp to access file outside buffer */
#define ICM_ERR_ENCODING		        0x106	/* Can't encode or decode value */
#define ICM_ERR_NOT_FOUND   	        0x107	/* A "not found" error */
#define ICM_ERR_DUPLICATE   	        0x108	/* Tag added is duplicate */
#define ICM_ERR_INTERNAL		        0x109	/* Internal inconsistency */
#define ICM_ERR_BADCS     		        0x10a	/* Unsupported colorspace */
#define ICM_ERR_BADCURVE   		        0x10b	/* Bad curve parameters */
#define ICM_ERR_CLASSMISMATCH	        0x10c	/* Signature classes don't match */

/* Format errors/warnings - plus sub-codes below */
#define ICM_ERR_RD_FORMAT    	        0x200	/* Read ICC file format error */
#define ICM_ERR_WR_FORMAT   	        0x300	/* Write ICC file format error */

/* Format/Transform sub-codes */
#define ICM_FMT_MASK					0xff	/* Format sub code mask */

#define ICM_FMT_SIG2TYPE			    0x03	/* Type unexpected for Tag */

#define ICM_FMT_MAJV	    	        0x05	/* Header Major version error */
#define ICM_FMT_MINV	    	        0x06	/* Header Minor version error */
#define ICM_FMT_BFV		    	        0x07	/* Header Bugfix version error */
#define ICM_FMT_SCREEN	    	        0x08	/* Screen encodings error */
#define ICM_FMT_DEVICE	    	        0x09	/* Device attributes encodings error */
#define ICM_FMT_PROFFLGS	   	        0x0a	/* Profile Flags encodings error */
#define ICM_FMT_ASCBIN		   	        0x0b	/* Ascii/Binary encoding flag error */
#define ICM_FMT_PHCOL		   	        0x0c	/* Phosphor or Colorant sets encoding error */
#define ICM_FMT_GAMMA		   	        0x0d	/* Video Card Gamma Format encoding error */
#define ICM_FMT_TECH		   	        0x0e	/* Technology Signature error */
#define ICM_FMT_COLSP		   	        0x0f	/* Colorspace Signature error */
#define ICM_FMT_PROF		   	        0x10	/* Profile Class Signature error */
#define ICM_FMT_PLAT		   	        0x11	/* Platform Signature error */
#define ICM_FMT_RMGAM		   	        0x12	/* Reference Medium Gamut Signature error */
#define ICM_FMT_MESGEOM		   	        0x13	/* Measurement Geometry encoding error */
#define ICM_FMT_INTENT		   	        0x14	/* Rendering intent sifnature error */
#define ICM_FMT_SPSHAPE		   	        0x15	/* Spot Shape encoding error */
#define ICM_FMT_STOBS		   	        0x16	/* Standard Observer encoding error */
#define ICM_FMT_PRILL		   	        0x17	/* Predefined Illuminant encoding error */
#define ICM_FMT_LANG		   	        0x18	/* Language code error */
#define ICM_FMT_REGION		   	        0x19	/* Region code error */
#define ICM_FMT_MSDEVID		   	        0x1a	/* Microsoft platform device settings ID error */
#define ICM_FMT_MSMETYP		   	        0x1b	/* Microsoft platform media type error */
#define ICM_FMT_MSHALFTN				0x1c	/* Microsoft platform Halgtone encoding error */
#define ICM_FMT_RESCVUNITS				0x1d	/* ResponseCurve Measurement units Sig error */
#define ICM_FMT_COLPHOS					0x1e	/* Colorant and Phosphor Encoding error */
#define ICM_FMT_PARCVTYP				0x1f	/* Parametric curve type Encoding error */
#define ICM_FMT_DATETIME    	        0x20	/* Date & time endoding error */
#define ICM_FMT_FZ8STRING    	        0x21	/* Fixed length, null terminated 8 bit string */
#define ICM_FMT_VZ8STRING    	        0x22	/* Variable length, null terminated 8 bit string */
#define ICM_FMT_PARTIALEL  	            0x23	/* Tag has partial array element */
#define ICM_FMT_SHORTTAG  	            0x24	/* Tag is short of space allocated to it */ 

#define ICM_FMT_CHRMCHAN    	        0x30	/* Chromaticity channels mismatch */ 
#define ICM_FMT_CHRMENC    	            0x31	/* Chromaticity encoding mismatch */ 
#define ICM_FMT_CHRMVALS   	            0x33	/* Chromaticity encoding values error */ 

#define ICM_FMT_CORDCHAN    	        0x34	/* Colorant Order channels mismatch */ 
#define ICM_FMT_CORDVALS   	            0x35	/* Colorant Order values error */ 

#define ICM_FMT_DSSIZE   	            0x36	/* Device Settings setting size error */

#define ICM_FMT_LUICHAN   	            0x37	/* Lut input channels don't match colorspace */
#define ICM_FMT_LUOCHAN   	            0x38	/* Lut output channels don't match colorspace */

#define ICM_FMT_DATA_FLAG  	            0x39	/* Data type has unknown flag value */
#define ICM_FMT_DATA_TERM  	            0x3a	/* ASCII data is not null terminated */
#define ICM_FMT_TEXT_ANOTTERM           0x3b	/* Text ASCII string is not nul terminate */
#define ICM_FMT_TEXT_ASHORT             0x3c	/* Text ASCII string shorter than space */
#define ICM_FMT_TEXT_UERR           	0x3d	/* Text unicode string translation error */
#define ICM_FMT_TEXT_SCRTERM            0x3f	/* Text scriptcode string not terminated */

#define ICM_FMT_MATRIX_SCALE            0x40	/* Matrix profile values have wrong scale */

#define ICM_FMT_REQUIRED_TAG            0x41	/* Missing a required tag for this profile class */
#define ICM_FMT_UNEXPECTED_TAG          0x42	/* A tag is unexpected for this profile class */
#define ICM_FMT_UNKNOWN_CLASS           0x43	/* Profile class is unknown */

#define ICM_TR_GAMUT_INTENT             0x80	/* Intent is unexpected for Gamut table */

/* Version errors/warnings - plus sub-codes below */
#define ICM_ERR_RD_VERSION    	        0x400	/* Read ICC file format error */
#define ICM_ERR_WR_VERSION   	        0x500	/* Write ICC file format error */

/* version sub-codes */
#define ICM_VER_SIGVERS				    0x01	/* Tag Sig not valid in profile version */
#define ICM_VER_TYPEVERS				0x02	/* TagType not valid in profile version */
#define ICM_VER_SIG2TYPEVERS		    0x03	/* Tag & TagType not valid comb. in prof. version */

/* Higher level error codes */
#define ICM_ERR_MAGIC_NUMBER	        0x601	/* ICC profile has bad magic number */
#define ICM_ERR_HEADER_VERSION	        0x602	/* Header has invalid version number */
#define ICM_ERR_HEADER_LENGTH	        0x603	/* Internal: Header is wrong legth */
#define ICM_ERR_UNKNOWN_VERSION	        0x604	/* ICC Version is not known */
#define ICM_ERR_UNKNOWN_COLORANT	    0x605	/* Unknown colorant */


/* =========================================================== */
/* System Heap interface object. The defaults can be replaced */
/* for adaption to different system environments. */
/* Note that we guarantee certain semantics, such as */
/* malloc(0) != NULL, realloc(NULL, size) and free(NULL) working, */
/* was well as providing recalloc() */

#ifndef ICC_DEBUG_MALLOC

/* Heap allocator class interface definition */
#define ICM_ALLOC_BASE																		\
	/* Public: */																			\
																							\
	void *(*malloc)  (struct _icmAlloc *p, size_t size);									\
	void *(*realloc) (struct _icmAlloc *p, void *ptr, size_t size);							\
	void *(*calloc)  (struct _icmAlloc *p, size_t num, size_t size);						\
	void *(*recalloc)(struct _icmAlloc *p, void *ptr, size_t cnum, size_t csize,			\
	                                        size_t nnum, size_t nsize);						\
	void  (*free)    (struct _icmAlloc *p, void *ptr);										\
																							\
	/* Take a reference counted copy of the allocator */									\
	struct _icmAlloc *(*reference)(struct _icmAlloc *p);									\
																							\
	/* we're done with the allocator object */												\
	void (*del)(struct _icmAlloc *p);														\
																							\
	/* Private: */																			\
	int refcount;																			\

#else /* !ICC_DEBUG_MALLOC */

#define ICM_ALLOC_BASE																		\
	/* Public: */																			\
																							\
	void *(*dmalloc)  (struct _icmAlloc *p, size_t size, char *file, int line);				\
	void *(*drealloc) (struct _icmAlloc *p, void *ptr, size_t size, char *file, int line);	\
	void *(*dcalloc)  (struct _icmAlloc *p, size_t num, size_t size, char *file, int line);	\
	void *(*drecalloc)(struct _icmAlloc *p, void *ptr, size_t cnum, size_t csize,			\
	                                        size_t nnum, size_t nsize, char *file, int line); \
	void  (*dfree)    (struct _icmAlloc *p, void *ptr, char *file, int line);				\
																							\
	/* Take a reference counted copy of the allocator */									\
	struct _icmAlloc *(*reference)(struct _icmAlloc *p);									\
																							\
	/* we're done with the allocator object */												\
	void (*del)(struct _icmAlloc *p);														\
																							\
	/* Private: */																			\
	int refcount;																			\

#ifdef _ICC_IMP_	/* only inside implementation */
#define malloc( p, size )	    dmalloc( p, size, __FILE__, __LINE__ )
#define calloc( p, num, size )	dcalloc( p, num, size, __FILE__, __LINE__ )
#define realloc( p, ptr, size )	drealloc( p, ptr, size, __FILE__, __LINE__ )
#define recalloc( p, ptr, cnum, csize, nnum, size ) drecalloc( p, ptr, cnum, csize, nnum, size, __FILE__, __LINE__ )
#define free( p, ptr )	        dfree( p, ptr , __FILE__, __LINE__ )
#endif /* _ICC_IMP */

#endif /* ICC_DEBUG_MALLOC */

/* Common heap allocator interface class */
struct _icmAlloc {
	ICM_ALLOC_BASE
}; typedef struct _icmAlloc icmAlloc;

/* - - - - - - - - - - - - - - - - - - - - -  */

/* Implementation of heap class based on standard system malloc */
struct _icmAllocStd {
	ICM_ALLOC_BASE
}; typedef struct _icmAllocStd icmAllocStd;

/* Create a standard alloc object */
/* Note that this will fail if e has an error already set */
icmAlloc *new_icmAllocStd(icmErr *e);

/* =========================================================== */
/* System File interface object. The defaults can be replaced */
/* for adaption to different system environments. */

#define ICM_FILE_BASE																		\
	/* Public: */																			\
																							\
	/* Get the size of the file (Only valid for reading file). */							\
	size_t (*get_size) (struct _icmFile *p);												\
																							\
	/* Set current position to offset. Return 0 on success, nz on failure. */				\
	int    (*seek) (struct _icmFile *p, unsigned int offset);								\
																							\
	/* Read count items of size length. Return number of items successfully read. */ 		\
	size_t (*read) (struct _icmFile *p, void *buffer, size_t size, size_t count);			\
																							\
	/* write count items of size length. Return number of items successfully written. */ 	\
	size_t (*write)(struct _icmFile *p, void *buffer, size_t size, size_t count);			\
																							\
	/* printf to the file */																\
	int (*printf)(struct _icmFile *p, const char *format, ...);								\
																							\
	/* flush all write data out to secondary storage. Return nz on failure. */				\
	int (*flush)(struct _icmFile *p);														\
																							\
	/* Return the memory buffer. Error if not icmFileMem */									\
	int (*get_buf)(struct _icmFile *p, unsigned char **buf, size_t *len);					\
																							\
	/* Take a reference counted copy of the allocator */									\
	struct _icmFile *(*reference)(struct _icmFile *p);										\
																							\
	/* we're done with the file object, return nz on failure */								\
	int (*del)(struct _icmFile *p);															\
																							\
	/* Private: */																			\
	int refcount;																			\

/* Common file interface class */
struct _icmFile {
	ICM_FILE_BASE
}; typedef struct _icmFile icmFile;


/* - - - - - - - - - - - - - - - - - - - - -  */

/* These are avalailable if SEPARATE_STD is not defined: */

/* Implementation of file access class based on standard file I/O */
struct _icmFileStd {
	ICM_FILE_BASE

	/* Private: */
	icmAlloc *al;		/* Heap allocator reference */
	FILE     *fp;
	int   doclose;		/* nz if free should close fp */

	/* Private: */
	size_t size;    	/* Size of the file (For read) */

}; typedef struct _icmFileStd icmFileStd;

/* Create given a file name */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_name(icmErr *e, char *name, char *mode);

/* Create given a (binary) FILE* */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_fp(icmErr *e, FILE *fp);

/* Create given a file name and take allocator reference */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_name_a(icmErr *e, char *name, char *mode, icmAlloc *al);

/* Create given a (binary) FILE* and take eallocator reference */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_fp_a(icmErr *e, FILE *fp, icmAlloc *al);


/* - - - - - - - - - - - - - - - - - - - - -  */
/* Implementation of file access class based on a memory image */
/* The buffer is assumed to be allocated with the given heap allocator */
/* Pass base = NULL, length = 0 for no initial buffer */

struct _icmFileMem {
	ICM_FILE_BASE

	/* Private: */
	icmAlloc *al;		/* Heap allocator reference */
	int      del_buf;   /* NZ if memory file buffer should be deleted */
	unsigned char *start, *cur, *end, *aend;

}; typedef struct _icmFileMem icmFileMem;

/* Create a memory image file access class and take allocator reference */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileMem_a(icmErr *e, void *base, size_t length, icmAlloc *al);

/* Create a memory image file access class and take allocator reference */
/* and delete buffer when icmFile is deleted. */
icmFile *new_icmFileMem_ad(icmErr *e, void *base, size_t length, icmAlloc *al);

/* This is avalailable if SEPARATE_STD is not defined: */

/* Create a memory image file access class */
icmFile *new_icmFileMem(icmErr *e, void *base, size_t length);

/* Create a memory image file access class */
/* and delete buffer when icmFile is deleted. */
icmFile *new_icmFileMem_d(icmErr *e, void *base, size_t length);

/* ================================= */
/* Assumed constants                 */

#define MAX_CHAN 15		/* Maximum number of color channels */

/* ================================= */
/* tag and other compound structures */

typedef ORD32 icmSig;	/* Otherwise un-enumerated 4 byte signature */

/* ArgyllCMS Private tagSignature: */
/* Absolute to Media Relative Transformation Space matrix. */
/* Uses the icSigS15Fixed16ArrayType */
/* (Unit matrix for default ICC, Bradford matrix for Argll default) */
#define icmSigAbsToRelTransSpace ((icTagSignature) icmMakeTag('a','r','t','s'))

/* ArgyllCMS ICCLIB extensions */

/* Non-standard Color Space Signatures - will be incompatible outside icclib! */

/* A non-specific monochrome space */
#define icSig1colorData ((icColorSpaceSignature) icmMakeTag('1','C','L','R'))

/* A non-specific monochrome colorsync space */
#define icSigMch1Data ((icColorSpaceSignature) icmMakeTag('M','C','H','1'))

/* Y'u'v' perceptual CIE 1976 UCS diagram Yu'v' */
#define icmSigYuvData ((icColorSpaceSignature) icmMakeTag('Y','u','v',' '))

/* A monochrome CIE L* space */
#define icmSigLData ((icColorSpaceSignature) icmMakeTag('L',' ',' ',' '))

/* A monochrome CIE Y space */
#define icmSigYData ((icColorSpaceSignature) icmMakeTag('Y',' ',' ',' '))

/* A modern take on Lab */
#define icmSigLptData ((icColorSpaceSignature) icmMakeTag('L','p','t',' '))


/* Pseudo Color Space Signatures - just used within icclib, not used in profiles. */

/* Pseudo PCS colospace of profile */
#define icmSigPCSData ((icColorSpaceSignature) icmMakeTag('P','C','S',' '))

/* Pseudo PCS colospace to signal 8 bit Lab */
#define icmSigLab8Data ((icColorSpaceSignature) icmMakeTag('L','a','b','8'))

/* Pseudo PCS colospace to signal V2 16 bit Lab */
#define icmSigLabV2Data ((icColorSpaceSignature) icmMakeTag('L','a','b','2'))

/* Pseudo PCS colospace to signal V4 16 bit Lab */
#define icmSigLabV4Data ((icColorSpaceSignature) icmMakeTag('L','a','b','4'))

/* Pseudo PCS colospace to signal 8 bit L */
#define icmSigL8Data ((icColorSpaceSignature) icmMakeTag('L',' ',' ','8'))

/* Pseudo PCS colospace to signal V2 16 bit L */
#define icmSigLV2Data ((icColorSpaceSignature) icmMakeTag('L',' ',' ','2'))

/* Pseudo PCS colospace to signal V4 16 bit L */
#define icmSigLV4Data ((icColorSpaceSignature) icmMakeTag('L',' ',' ','4'))


/* Alias for icSigColorantTableType found in LOGO profiles (Byte swapped clrt !) */ 
#define icmSigAltColorantTableType ((icTagTypeSignature)icmMakeTag('t','r','l','c'))

/* Non-standard Platform Signature for Linux and similiar */
#define icmSig_nix ((icPlatformSignature) icmMakeTag('*','n','i','x'))

/* Tag Type signature, used to handle any tag that */
/* isn't handled with a specific type object. */
/* Also used in manufacturer & model header fields */
/* Old: #define icmSigUnknownType ((icTagTypeSignature)icmMakeTag('?','?','?','?')) */
#define icmSigUnknownType ((icTagTypeSignature) 0)


typedef struct {
	ORD32 l;			/* High and low components of signed 64 bit */
	INR32 h;
} icmInt64;

typedef struct {
	ORD32 l,h;			/* High and low components of unsigned 64 bit */
} icmUInt64;

/* XYZ Number */
#ifdef NEVER
/* Old version */
typedef struct {
    double  X;
    double  Y;
    double  Z;
} icmXYZNumber;
#endif
#ifndef NEVER
/* Compatible new version (depends on anonomous unions) */
typedef struct {
	union {
		double XYZ[3];			/* New version */
		struct {
			double X, Y, Z;		/* For backwards compatibility */
		};
	};
} icmXYZNumber;
#endif
#ifdef NEVER
/* Incompatible new version */
typedef struct {
	double XYZ[3];
} icmXYZNumber;
#endif

/* CIE xy coordinate value */
typedef struct {
	double xy[2];
} icmxyCoordinate;

/* icmPositionNumber */
typedef struct {
	unsigned int	offset;	/* Offset to the data in bytes */
	unsigned int	size;	/* Size ofthe data in bytes */
} icmPositionNumber;

/* Response 16 number */
typedef struct {
	double	       deviceValue;	/* The device value in range 0.0 - 1.0 */
	double	       measurement;	/* The reading value */
} icmResponse16Number;

/* Date & time number */
struct _icmDateTimeNumber {
    unsigned int      year;
    unsigned int      month;
    unsigned int      day;
    unsigned int      hours;
    unsigned int      minutes;
    unsigned int      seconds;
}; typedef struct _icmDateTimeNumber icmDateTimeNumber;

/* Set the DateTime structure to the current date and time */
void icmDateTimeNumber_setcur(icmDateTimeNumber *p);

/* Convert a DateTimeNumber from UTC to local time */
void icmDateTimeNumber_tolocal(icmDateTimeNumber *d, icmDateTimeNumber *s);

/* ICC version version number */
typedef struct { 
	int majv, minv, bfv;		/* Version - major, minor, bug fix */
} icmVers;

/* =========================================================== */
/* Overflow protected unsigned int arithmatic */
/* These functions saturate rather than wrapping around */

unsigned int sat_add(unsigned int a, unsigned int b);	/* a + b */
unsigned int sat_sub(unsigned int a, unsigned int b);	/* a - b */
unsigned int sat_mul(unsigned int a, unsigned int b);	/* a * b */
unsigned int sat_pow(unsigned int a, unsigned int b);	/* a ^ b */

/* Version that set the indicator flag if there was saturation, */
/* else indicator is unchanged */
unsigned int sati_add(int *ind, unsigned int a, unsigned int b);	/* a + b */
unsigned int sati_sub(int *ind, unsigned int a, unsigned int b);	/* a - b */
unsigned int sati_mul(int *ind, unsigned int a, unsigned int b);	/* a * b */
unsigned int sati_pow(int *ind, unsigned int a, unsigned int b);	/* a ^ b */


/* Float comparisons to certain precision */
int diff_u16f16(double a, double b);		/* u16f16 precision */

/* =========================================================== */
/* Parameters to icmGetNormFunc(): */

typedef enum {			/* Direction */
    icmGNtoCS     = 0,	/* Return conversion from Norm to Colorspace */
    icmGNtoNorm   = 1,	/* Return conversion from Colorspace to Norm */
} icmGNDir;

typedef enum {			/* Lab version encoding */
    icmGNV2 = 0,		/* ICC V2 */
    icmGNV4 = 1,		/* ICC V4 */
} icmGNVers;

typedef enum {			/* Lab version representation */
    icmGN8bit  = 0,		/* 8 bit unsigned int */
    icmGN16bit = 1,		/* 16 bir unsigned int */
} icmGNRep;

/* Find appropriate conversion functions from the normalised */
/* Lut data range 0.0 - 1.0 to/from a given colorspace value, */
/* given the color space and Lut type. */
/* If the signature specifies a specific Lab encoding, dir, ver & rep are ignored */
/* Return 0 on success, 1 on match failure. */
int icmGetNormFunc(
	void (**nfunc)(double *out, double *in),	/* return function pointers */
	void (**nfuncn)(double *out, double *in, unsigned int ch),
	icColorSpaceSignature csig, 		/* Colorspace to convert to/from */
	icmGNDir              dir,			/* Direction */
	icmGNVers             ver,			/* Lab version */
	icmGNRep              rep			/* Representation */
);

/* =========================================================== */

/* Errors and warnings:

	Errors are "sticky" and ultimately fatal for any particular
	operation. They can be cleared after that operation has returned.
	The first error is the one that is returned.

	Non-fatal Format errors are treated as fatal unless set to Warn.

	Non-fatal Version errors are treated as fatal unless set to Warn.

	Quirks are errors that would normally be fatal, but if the Quirk flag
	is set are repaired and treated as warnings.
*/

/* Flags that control the format & version checking behaviour. */
/* By default checking will be strict. */
typedef unsigned int icmCompatFlags;

#define icmCFlagRdFormatWarn ((icmCompatFlags)0x0001)
					/* Warn if read format is not strictly complient rather than error  */
#define icmCFlagWrFormatWarn ((icmCompatFlags)0x0002)
					/* Warn if write format is not strictly complient rather than error  */

#define icmCFlagRdVersionWarn ((icmCompatFlags)0x0004)
					/* Warn if read version has incompatibility problem rather than error  */

#define icmCFlagWrVersionWarn ((icmCompatFlags)0x0008)
					/* Warn if write version has incompatibility problem rather than error  */

#define icmCFlagAllowUnknown ((icmCompatFlags)0x0010)
					/* Allow unknown tags and tagtypes without warning or error. */
					/* Will warn if known tag uses unknown tagtype */

#define icmCFlagAllowExtensions ((icmCompatFlags)0x0020)
					/* Allow ICCLIB extensions without warning or error. */

#define icmCFlagAllowQuirks ((icmCompatFlags)0x0040)
					/* Permit non-fatal deviations from spec., and allow for profile quirks */
					/* and emit warnings. */

#define icmCFlagAllowWrVersion ((icmCompatFlags)0x0080)
					/* Allow out of version Tags & TagTypes on write without warnings */
					/* over tag & tagtype version range defined by vcrange */
					/* (Use icp->set_vcrange() to set/unset) */

#define icmCFlagNoRequiredCheck ((icmCompatFlags)0x0100)
					/* Don't check for required tags on write */
					/* (This is intended only for testing) */

/* Flags that indicate that a warning has occured */
#define icmCFlagRdWarning ((icmCompatFlags)0x1000)
					/* A read warning was issued */
#define icmCFlagWrWarning ((icmCompatFlags)0x2000)
					/* A write warning was issued */

/* =================================================== */
/* Declaration of NEW serialisation support functions. */

/* Operation that a serialisation routine should perform. */
/* Note that icmSnOp & icmSnResize will be true for Resize or Read */
typedef enum {
	/* Component flags - test by anding with icmSnOp */
	icmSnDumyBuf   = 1,	/* A dummy buffer is used */
	icmSnSerialise = 2,	/* Full serialisation is needed, not just arrays allocation/free */
	icmSnAlloc     = 4,	/* Need to alloc/realloc storage */

	/* Individual states */
    icmSnResize   = icmSnAlloc + icmSnDumyBuf,		/* 5 = Resize variable sized elements */
    icmSnFree     = icmSnDumyBuf, 					/* 1 = Free variable sized elements */
    icmSnSize     = icmSnSerialise + icmSnDumyBuf,	/* 3 = Return file size the element would use */
    icmSnWrite    = icmSnSerialise,					/* 2 = Write elements */
    icmSnRead     = icmSnAlloc + icmSnSerialise		/* 6 = Read elements */
} icmSnOp;


/* File buffer class */
/* The serialisation functionaly uses this as a source or target. */
struct _icmFBuf {
	struct _icc *icp;	/* icc we're part of */

	icmSnOp op;			/* Operation being performed */
	unsigned int size;	/* size of tag */ 
	icmFile *fp;		/* Associated file */
	unsigned int of;	/* File offset of buffer */
	ORD8 *buf;			/* Pointer to buffer base */
	ORD8 *bp;			/* Pointer to next location to read/write */
	ORD8 *ep;			/* Pointer to location one past end of buffer */

	/* Offset the current buffer location by a relative amount */
	void (* roff)(struct _icmFBuf *p, INR32 off);

	/* Set the current location to an absolute location within the buffer */
	void (* aoff)(struct _icmFBuf *p, ORD32 off);

	/* Return the current absolute offset within the buffer */
	ORD32 (* get_off)(struct _icmFBuf *p);

	/* Return the currently available space  within the buffer */
	ORD32 (* get_space)(struct _icmFBuf *p);

	/* Finalise (ie. Write), free object and buffer, return error and size used */
	ORD32 (* done)(struct _icmFBuf *p);
}; typedef struct _icmFBuf icmFBuf;

/* Create a new file buffer. */
/* If op & icmSnDumyBuf, then f, of and size are ignored. */
/* of is the offset of the buffer in the file */ 
/* e may be NULL */
/* Returns NULL on error, & fills in icp->e */
icmFBuf *new_icmFBuf(struct _icc *icp, icmSnOp op, icmFile *fp, unsigned int of, ORD32 size);


#ifdef NEVER		// Not used

/* There is a serialisation base class that can be inhereted, */
/* that provides the necessary serialisation support functionality. */
/* (This need not be a base class for every serialisable object.) */

#define ICM_SN																	\
	int  (*serialise)(struct _icmSn *p, icmFBuf *fb);							\
	int  (*resize)(struct _icmSn *p);											\

/* Base serialisation class */
struct _icmSn {
	ICM_SN
}; typedef struct _icmSn icmSn;

#endif /* NEVER */

/* Utility function: */
/* Deal with a possibly non-fatal formatting error. */
/* Return the current error code. This will be not ICM_ERROR_OK if fatal. */
int icmFormatWarning(struct _icc *icp, int sub, const char *format, ...);

/* Serialisation building blocks */
void icmSn_pad(icmFBuf *b, ORD32 size);				/* Pad with size bytes */
void icmSn_align(icmFBuf *b, ORD32 alignment);		/* Pad to give alignment */
void icmSn_uc_UInt8(icmFBuf *b, unsigned char *p);
void icmSn_us_UInt8(icmFBuf *b, unsigned short *p);
void icmSn_ui_UInt8(icmFBuf *b, unsigned int *p);
void icmSn_us_UInt16(icmFBuf *b, unsigned short *p);
void icmSn_ui_UInt16(icmFBuf *b, unsigned int *p);
void icmSn_ui_UInt32(icmFBuf *b, unsigned int *p);
void icmSn_uii_UInt64(icmFBuf *b, icmUInt64 *p);
void icmSn_d_U8Fix8(icmFBuf *b, double *p);
void icmSn_d_U16Fix16(icmFBuf *b, double *p);
void icmSn_c_SInt8(icmFBuf *b, signed char *p);
void icmSn_s_SInt8(icmFBuf *b, short *p);
void icmSn_i_SInt8(icmFBuf *b, int *p);
void icmSn_s_SInt16(icmFBuf *b, short *p);
void icmSn_i_SInt16(icmFBuf *b, int *p);
void icmSn_i_SInt32(icmFBuf *b, int *p);
void icmSn_ii_SInt64(icmFBuf *b, icmInt64 *p);
void icmSn_d_S8Fix8(icmFBuf *b, double *p);
void icmSn_d_S15Fix16(icmFBuf *b, double *p);
void icmSn_d_NFix8(icmFBuf *b, double *p);
void icmSn_d_NFix16(icmFBuf *b, double *p);
void icmSn_d_NFix32(icmFBuf *b, double *p);

/* - - - - - - - - - - - - - - - */
/* Independent Check functions */

int icmCheckVariableNullASCII(icmFBuf *b, char *p, int length);

/* - - - - - - - - - - - - - - - */
/* Encoding checked serialisation functions */

void icmSn_Screening32(icmFBuf *b, icScreening *p);
void icmSn_DeviceAttributes64(icmFBuf *b, icmUInt64 *p);
void icmSn_ProfileFlags32(icmFBuf *b, icProfileFlags *p);
void icmSn_AsciiOrBinary32(icmFBuf *b, icAsciiOrBinary *p);
void icmSn_VideoCardGammaFormat32(icmFBuf *b, icVideoCardGammaFormat *p);
void icmSn_TagSig32(icmFBuf *b, icTagSignature *p);
void icmSn_TagTypeSig32(icmFBuf *b, icTagTypeSignature *p);
void icmSn_ColorSpaceSig32(icmFBuf *b, icColorSpaceSignature *p);
void icmSn_ProfileClassSig32(icmFBuf *b, icProfileClassSignature *p);
void icmSn_PlatformSig32(icmFBuf *b, icPlatformSignature *p);
void icmSn_DeviceManufacturerSig32(icmFBuf *b, icDeviceManufacturerSignature *p);
void icmSn_DeviceModelSig32(icmFBuf *b, icDeviceModelSignature *p);
void icmSn_CMMSig32(icmFBuf *b, icCMMSignature *p);
void icmSn_ReferenceMediumGamutSig32(icmFBuf *b, icReferenceMediumGamutSignature *p);
void icmSn_TechnologySig32(icmFBuf *b, icTechnologySignature *p);
void icmSn_MeasurementGeometry32(icmFBuf *b, icMeasurementGeometry *p);
void icmSn_MeasurementFlare32(icmFBuf *b, icMeasurementFlare *p);
void icmSn_RenderingIntent32(icmFBuf *b, icRenderingIntent *p);
void icmSn_SpotShape32(icmFBuf *b, icSpotShape *p);
void icmSn_StandardObserver32(icmFBuf *b, icStandardObserver *p);
void icmSn_PredefinedIlluminant32(icmFBuf *b, icIlluminant *p);
void icmSn_LanguageCode16(icmFBuf *b, icEnumLanguageCode *p);
void icmSn_RegionCode16(icmFBuf *b, icEnumRegionCode *p);
void icmSn_DevSetMsftIDSig32(icmFBuf *b, icDevSetMsftIDSignature *p);
void icmSn_DevSetMsftMedia32(icmFBuf *b, icDevSetMsftMedia *p);
void icmSn_DevSetMsftDither32(icmFBuf *b, icDevSetMsftDither *p);
void icmSn_MeasUnitsSig32(icmFBuf *b, icMeasUnitsSig *p);
void icmSn_PhColEncoding(icmFBuf *b, icPhColEncoding *p);
void icmSn_ParametricCurveFunctionType16(icmFBuf *b, icParametricCurveFunctionType *p);

/* - - - - - - - - - - - - - - - */
/* Compound serialisation blocks */

void icmSn_xyCoordinate8b(icmFBuf *b, icmxyCoordinate *p);
void icmSn_DateTimeNumber12b(icmFBuf *b, icmDateTimeNumber *p);
void icmSn_VersionNumber32(icmFBuf *b, icmVers *p);
void icmSn_FixedNullASCII(icmFBuf *b, char *p, unsigned int length);
void icmSn_VariableNullASCII(icmFBuf *b, char *p, unsigned int length);

void icmSn_XYZNumber12b(icmFBuf *b, icmXYZNumber *p);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Colorspace value serialiser class */
/* Instances are assumed to be statically allocated */
struct _icmCSSn {
	/* Private: */
	struct _icc *icp; 							/* icc - for allocator, error reporting etc. */
	unsigned int n;								/* Number of elements */
	void (*rdnfunc)(double *out, double *in);	/* Read normalising function */
	void (*wrnfunc)(double *out, double *in);	/* Write normalising function */
	void (*rdnfuncn)(double *out, double *in, unsigned int ch);
												/* Read per-channel normalising function */
	void (*wrnfuncn)(double *out, double *in, unsigned int ch);
												/* Write per-channel normalising function */
	void (*snfuncn)(icmFBuf *b, double *p);		/* per-channel Serialisation function */

	/* Public: */
	void (*sn)(struct _icmCSSn *p, icmFBuf *b, double *vp);
	void (*snf)(struct _icmCSSn *p, icmFBuf *b, float *vp);
	void (*snn)(struct _icmCSSn *p, icmFBuf *b, double *vp, unsigned int ch);
}; typedef struct _icmCSSn icmCSSn;

/* Macro to set ver parameter from icc object */
#define PCSVER( mp ) ((mp)->header->vers.majv >= 4 ? icmGNV4 : icmGNV2)

/* Initialise an icmCSSn instance for a particular colorspace and encoding */
int init_icmCSSn(					/* Return error code */
	icmCSSn              *p,		/* Object to initialise */
	struct _icc          *icp, 		/* icc - for allocator, error reporting etc. */
	icColorSpaceSignature csig, 	/* Colorspace to serialise */
	icmGNVers             ver,		/* Lab version */
	icmGNRep              rep		/* Representation */
);

/* Create an icmCSSn instance for a PCS colorspace and encoding */
int init_icmPCSSn(					/* Return error code */
	icmCSSn              *p,		/* Object to initialise */
	struct _icc          *icp, 		/* icc - for allocator, error reporting etc. */
	icColorSpaceSignature csig, 	/* Colorspace to serialise - must be XYZ or Lab! */
	icmGNVers             ver,		/* Lab version */
	icmGNRep              rep		/* Representation */
);

/* ==================================================== */
/* Convenience functions and macro's, to help implement */
/* tag types. */ 


/* Seting up the right kind of file buffer for each method type */
/* The buffer contains the type of processing flags. */

#define ICM_TAG_NEW_RESIZE_BUFFER											\
	icmFBuf *b;																\
	if ((b = new_icmFBuf(p->icp, icmSnResize, NULL, 0, 0)) == NULL)			\
		return p->icp->e.c;;												\

#define ICM_TAG_NEW_FREE_BUFFER												\
	icmFBuf *b;																\
	if ((b = new_icmFBuf(p->icp, icmSnFree, NULL, 0, 0)) == NULL)			\
		return;																\

#define ICM_TAG_NEW_SIZE_BUFFER												\
	icmFBuf *b;																\
	if ((b = new_icmFBuf(p->icp, icmSnSize, NULL, 0, 0)) == NULL)			\
		return 0;															\

#define ICM_TAG_NEW_READ_BUFFER												\
	icmFBuf *b;																\
	if ((b = new_icmFBuf(p->icp, icmSnRead,									\
	                         p->icp->rfp, of, size)) == NULL)				\
		return p->icp->e.c;

#define ICM_TAG_NEW_WRITE_BUFFER											\
	icmFBuf *b;																\
	if ((b = new_icmFBuf(p->icp, icmSnWrite,								\
	            p->icp->wfp, of, size)) == NULL)							\
		return p->icp->e.c;													\


/* Implementation of array resizing utility function,         */
/* used to deal with the serialization actions on an array.   */

/* Array allocation count type */
typedef enum {
    icmAResizeByCount     = 0,  /* count is explicit */
    icmAResizeBySize      = 1   /* count set by available size */ 
} icmAResizeCountType;

/* Return zero on sucess, nz if there is an error */
int icmArrayRdAllocResize(
	icmFBuf *b,					/* Buffer we're dealing with */
	icmAResizeCountType ct,		/* explicit count or count from buffer size ? */

	unsigned int *p_count,		/* Pointer to current count of items */
	unsigned int *pcount,		/* Pointer to new count of items */
	void **pdata,				/* Pointer to items memory allocation */
	unsigned int *p_msize,		/* Pointer to previous memory size of each item (may be NULL) */
	unsigned int msize,			/* Current memory size of each item */

	unsigned int maxsize,		/* Maximum size permitted for buffer (use UINT_MAX if not needed) */
	unsigned int fixedsize,		/* Fixed size to be allowed for in file buffer. */
	                            /* Buffer space allowed is min(maxsize, buf->space - fixedsize) */
	unsigned int bsize,			/* File buffer size of each item */
	char *tagdesc				/* Stringification of tag type used for warning/error reporting */
);

/* On read, check the tag has been fully consumed */
#define ICMRDCHECKCONSUMED(TAGTYPE)														\
	if (b->op == icmSnRead) {															\
		unsigned int tavail;															\
		tavail = b->get_space(b);														\
		if (tavail != 0) {																\
			icmFormatWarning(b->icp, ICM_FMT_SHORTTAG,								\
			         #TAGTYPE " tag array doesn't occupy all of tag (%u bytes short)",	\
			         tavail);															\
		}																				\
	}																					\

/* Implement serialise() free functionality */
/* BASE  is the icmBase TagType object for reference to DATA and COUNT */
/* COUNT is the variable holding the size of the array */
/* DATA is the variable holding the array */
#define ICMFREEARRAY(BASE, COUNT, DATA)													\
	if (b->op == icmSnFree) {															\
		BASE->icp->al->free(BASE->icp->al, BASE->DATA);									\
		BASE->_##COUNT = 0;																\
	}																					\

/* --------------------------------------------------- */

/*
 * LEGACY method: serialise == NULL and the tagtype instance has to
 * implement the get_size, read, write, del & allocate methods.
 * (In this case the write(size) argument can be ignored.)
 *
 * NEW method: serialise is implemented and get_size, read, write, del & allocate methods
 * are implemented by the icmGeneric* functions, which use serialise to
 * do all the work. 
 *
 *  Read and write method error codes:
 *  0 = sucess
 *  1 = file format/logistical error
 *  2 = system error
 */

#define ICM_BASE_MEMBERS(TTYPE)		/* This tag type */										\
	/* Private: */																			\
	icTagTypeSignature ttype;		/* The tag type signature of the object */				\
	struct _icc    *icp;			/* Pointer to ICC we're a part of */					\
	icTagSignature creatorsig;		/* tag that created this tagtype */						\
									/* (shared tagtypes will have been linked to others) */	\
	int	           touched;			/* Flag for write bookeeping */							\
    int            refcount;		/* Reference count for sharing tag instances */			\
																							\
	void           (*serialise)(TTYPE *p, icmFBuf *b);										\
									/* Usual Read/Write/Del/Allocate implementation, */		\
									/* NULL if not implemented */							\
	unsigned int   (*get_size)(TTYPE *p);													\
									/* Return number of bytes needed to write this tag */	\
	int            (*read)(TTYPE *p, unsigned int size, unsigned int of);					\
									/* size is expected size to read */						\
									/* Offset is offset into file */						\
	int            (*write)(TTYPE *p, unsigned int size, unsigned int of);					\
									/* size is expected size to write */					\
									/* Offset is offset into file */						\
	void           (*del)(TTYPE *p);														\
									/* Free this object */									\
																							\
	/* Public: */																			\
	/* ?? should add get_ttype() so we can check ttype without looking at private ?? */		\
																							\
	void           (*dump)(TTYPE *p, icmFile *op, int verb);								\
									/* Dump a human readable description of this */			\
									/* object to the given output file. */					\
									/* 0 = silent, 1 = minimal, 2 = moderate, 3 = all */	\
	int            (*allocate)(TTYPE *p);													\
									/* Re-allocate space needed acording to sizes */		\
									/* in the object. Return an error code */				\
	int            (*check)(TTYPE *p, icTagSignature sig);									\
									/* Perform check of tag values before write/after */	\
	                                /* read for validity above that of the elements */		\
	                                /* it is composed of. May be NULL. Ret E.C. */			\

/* Base tag element data object */
struct _icmBase {
	ICM_BASE_MEMBERS(struct _icmBase)
}; typedef struct _icmBase icmBase;

/* Default implementation of the tag type methods, that all use */
/* serialise as their implementation. */
unsigned int icmGeneric_get_size(icmBase *p);
int icmGeneric_read(icmBase *p, unsigned int size, unsigned int of);
int icmGeneric_write(icmBase *p, unsigned int size, unsigned int of);
void icmGeneric_del(icmBase *p);
int icmGeneric_allocate(icmBase *p);

/* TagType class allocation and initialisation. */
/* It's assumed that the parent class provides all methods. */
/* By default we initialise to the generic implementation, */
/* that assumes the presense of only the TTYPE##_serialise(), TTYPE##_dump() */
/* and (optional) TTYPE##_check() methods. */
/* p == NULL on error */
#define ICM_TTYPE_ALLOCINIT(TTYPE, _sigttype)											\
	TTYPE *p = NULL;																	\
	if (icp->e.c == ICM_ERR_OK) {														\
		if ((p = (TTYPE *)icp->al->calloc(icp->al, 1, sizeof(TTYPE))) == NULL) {		\
			icm_err(icp, ICM_ERR_MALLOC, "Allocating tag %s failed",#TTYPE);			\
		} else {																		\
			ICM_TTYPE_INIT(TTYPE, _sigttype)											\
		}																				\
	}																					\

#define ICM_TTYPE_INIT(TTYPE, _sigttype)												\
	p->icp       = icp;																	\
	p->ttype     = _sigttype;															\
	p->refcount  = 1;																	\
	p->serialise = TTYPE##_serialise;													\
	p->get_size  = (unsigned int (*)(TTYPE *p))	icmGeneric_get_size;					\
	p->read      = (int (*)(TTYPE *p, unsigned int size, unsigned int of))				\
	               icmGeneric_read;														\
	p->write     = (int (*)(TTYPE *p, unsigned int size, unsigned int of))				\
	               icmGeneric_write;													\
	p->del       = (void (*)(TTYPE *p)) icmGeneric_delete;								\
	p->dump      = TTYPE##_dump;														\
	p->allocate  = (int (*)(TTYPE *p)) icmGeneric_allocate;								\
	p->check     = TTYPE##_check;														\

/* The LEGACY init is assumed to have no serialize() or check() */						\
#define ICM_TTYPE_ALLOCINIT_LEGACY(TTYPE, _sigttype)									\
	TTYPE *p = NULL;																	\
	if (icp->e.c == ICM_ERR_OK) {														\
		if ((p = (TTYPE *)icp->al->calloc(icp->al, 1, sizeof(TTYPE))) == NULL) {		\
			icm_err(icp, ICM_ERR_MALLOC, "Allocating tag %s failed",#TTYPE);			\
		} else {																		\
			ICM_TTYPE_INIT_LEGACY(TTYPE, _sigttype)										\
		}																				\
	}																					\

#define ICM_TTYPE_INIT_LEGACY(TTYPE, _sigttype)											\
	p->icp       = icp;																	\
	p->ttype     = _sigttype;															\
	p->refcount  = 1;																	\
	p->serialise = NULL;																\
	p->get_size  = TTYPE##_get_size;													\
	p->read      = TTYPE##_read;														\
	p->write     = TTYPE##_write;														\
	p->del       = TTYPE##_delete;														\
	p->dump      = TTYPE##_dump;														\
	p->allocate  = TTYPE##_allocate;													\
	p->check     = NULL;																\

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Tag type to hold an unknown tag type, */
/* so that it can be read and copied. */
struct _icmUnknown {
	ICM_BASE_MEMBERS(struct _icmUnknown)

	/* Private: */
	unsigned int     _count;	/* Count of data currently allocated */

	/* Public: */
	icTagTypeSignature uttype;	/* The unknown tag type actual signature */
	unsigned int	  count;	/* Allocated and used count of the array */
    unsigned char	  *data;  	/* tag data */
}; typedef struct _icmUnknown icmUnknown;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* UInt8 Array */
struct _icmUInt8Array {
	ICM_BASE_MEMBERS(struct _icmUInt8Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used size of the array */
    unsigned int   *data;		/* Pointer to array of data */ 
}; typedef struct _icmUInt8Array icmUInt8Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* uInt16 Array */
struct _icmUInt16Array {
	ICM_BASE_MEMBERS(struct _icmUInt16Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    unsigned int	*data;		/* Pointer to array of data */ 
}; typedef struct _icmUInt16Array icmUInt16Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* uInt32 Array */
struct _icmUInt32Array {
	ICM_BASE_MEMBERS(struct _icmUInt32Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    unsigned int	*data;		/* Pointer to array of data */ 
}; typedef struct _icmUInt32Array icmUInt32Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* UInt64 Array */
struct _icmUInt64Array {
	ICM_BASE_MEMBERS(struct _icmUInt64Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    icmUInt64		*data;		/* Pointer to array of hight data */ 
}; typedef struct _icmUInt64Array icmUInt64Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* u16Fixed16 Array */
struct _icmU16Fixed16Array {
	ICM_BASE_MEMBERS(struct _icmU16Fixed16Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    double			*data;		/* Pointer to array of hight data */ 
}; typedef struct _icmU16Fixed16Array icmU16Fixed16Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* s15Fixed16 Array */
struct _icmS15Fixed16Array {
	ICM_BASE_MEMBERS(struct _icmS15Fixed16Array)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    double			*data;		/* Pointer to array of hight data */ 
}; typedef struct _icmS15Fixed16Array icmS15Fixed16Array;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* XYZ Array */
struct _icmXYZArray {
	ICM_BASE_MEMBERS(struct _icmXYZArray)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	count;		/* Allocated and used count of the array */
    icmXYZNumber	*data;		/* Pointer to array of data */ 
}; typedef struct _icmXYZArray icmXYZArray;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Curve */
typedef enum {
    icmCurveUndef           = -1, /* Undefined curve */
    icmCurveLin             = 0,  /* Linear transfer curve */
    icmCurveGamma           = 1,  /* Gamma power transfer curve */
    icmCurveSpec            = 2   /* Specified curve */
} icmCurveStyle;

/* Curve reverse lookup information */
typedef struct {
	int inited;				/* Flag */
	double rmin, rmax;		/* Range of reverse grid */
	double qscale;			/* Quantising scale factor */
	int rsize;				/* Number of reverse lists */
	unsigned int **rlists;	/* Array of list of fwd values that may contain output value */
							/* Offset 0 = allocated size */
							/* Offset 1 = next free index */
							/* Offset 2 = first fwd index */
	unsigned int count;		/* Copy of forward table size */
	double       *data;		/* Copy of forward table data */
} icmRevTable;

struct _icmCurve {
	ICM_BASE_MEMBERS(struct _icmCurve)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */
	icmRevTable  rt;			/* Reverse table information */

	/* Public: */
    icmCurveStyle   flag;		/* Style of curve */
	unsigned int	count;		/* Allocated and used size of the array */
    double         *data;  		/* Curve data scaled to range 0.0 - 1.0 */
								/* or data[0] = gamma value */
	/* Translate a value through the curve, return warning flags */
	int (*lookup_fwd) (struct _icmCurve *p, double *out, double *in);	/* Forwards */
	int (*lookup_bwd) (struct _icmCurve *p, double *out, double *in);	/* Backwards */

}; typedef struct _icmCurve icmCurve;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Data */
typedef enum {
    icmDataUndef           = -1, /* Undefined data */
    icmDataASCII           = 0,  /* ASCII data */
    icmDataBin             = 1   /* Binary data */
} icmDataStyle;

struct _icmData {
	ICM_BASE_MEMBERS(struct _icmData)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
    icmDataStyle	flag;		/* Style of data */
	unsigned int	count;		/* Allocated and used size of the array (inc ascii null) */
    unsigned char	*data;  	/* data or string, NULL if size == 0 */
}; typedef struct _icmData icmData;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* text */
struct _icmText {
	ICM_BASE_MEMBERS(struct _icmText)

	/* Private: */
	unsigned int   _count;		/* Size currently allocated */

	/* Public: */
	unsigned int	 count;		/* Allocated and used size of data, inc null */
	char             *data;		/* ascii string (null terminated), NULL if size==0 */

}; typedef struct _icmText icmText;


/* - - - - - - - - - - - - - - - - - - - - -  */
/* DateTime */
struct _icmDateTime {
	ICM_BASE_MEMBERS(struct _icmDateTime)

	icmDateTimeNumber date;		/* Date/Time */

}; typedef struct _icmDateTime icmDateTime;

#ifdef NEW
/* - - - - - - - - - - - - - - - - - - - - -  */
/* DeviceSettings */

/*
   I think this all works like this:

Valid setting = (   (platform == platform1 and platform1.valid)
                 or (platform == platform2 and platform2.valid)
                 or ...
                )

where
	platformN.valid = (   platformN.combination1.valid
	                   or platformN.combination2.valid
	                   or ...
	                  )

where
	platformN.combinationM.valid = (    platformN.combinationM.settingstruct1.valid
	                                and platformN.combinationM.settingstruct2.valid
	                                and ...
	                               )

where
	platformN.combinationM.settingstructP.valid = (   platformN.combinationM.settingstructP.setting1.valid
	                                               or platformN.combinationM.settingstructP.setting2.valid
	                                               or ...
	                                              )

 */

/* The Settings Structure holds an array of settings of a particular type */
struct _icmSettingStruct {
	ICM_BASE_MEMBERS(struct _icmSettingStruct)

	/* Private: */
	unsigned int   _num;				/* Size currently allocated */

	/* Public: */
	icSettingsSig       settingSig;		/* Setting identification */
	unsigned int        numSettings; 	/* number of setting values */
	union {								/* Setting values - type depends on Sig */
		icUInt64Number      *resolution;
		icDeviceMedia       *media;
		icDeviceDither      *halftone;
	}
}; typedef struct _icmSettingStruct icmSettingStruct;

/* A Setting Combination holds all arrays of different setting types */
struct _icmSettingComb {
	/* Private: */
	unsigned int   _num;			/* number currently allocated */

	/* Public: */
	unsigned int        numStructs;   /* num of setting structures */
	icmSettingStruct    *data;
}; typedef struct _icmSettingComb icmSettingComb;

/* A Platform Entry holds all setting combinations */
struct _icmPlatformEntry {
	/* Private: */
	unsigned int   _num;			/* number currently allocated */

	/* Public: */
	icPlatformSignature platform;
	unsigned int        numCombinations;    /* num of settings and allocated array size */
	icmSettingComb      *data; 
}; typedef struct _icmPlatformEntry icmPlatformEntry;

/* The Device Settings holds all platform settings */
struct _icmDeviceSettings {
	/* Private: */
	unsigned int   _num;			/* number currently allocated */

	/* Public: */
	unsigned int        numPlatforms;	/* num of platforms and allocated array size */
	icmPlatformEntry    *data;			/* Array of pointers to platform entry data */
}; typedef struct _icmDeviceSettings icmDeviceSettings;

#endif /* NEW */

/* - - - - - - - - - - - - - - - - - - - - -  */
/* optimised simplex Lut lookup axis flipping structure */
typedef struct {
	double fth;		/* Flip threshold */
	char bthff;		/* Below threshold flip flag value */
	char athff;		/* Above threshold flip flag value */
} sx_flip_info;
 
/* Set method flags */
#define ICM_CLUT_SET_EXACT 0x0000	/* Set clut node values exactly from callback */
#define ICM_CLUT_SET_APXLS 0x0001	/* Set clut node values to aproximate least squares fit */

#define ICM_CLUT_SET_FILTER 0x0002	/* Post filter values (icmSetMultiLutTables() only) */


/* lut */
struct _icmLut {
	ICM_BASE_MEMBERS(struct _icmLut)

	/* Private: */
	/* Cache appropriate normalization routines */
	int dinc[MAX_CHAN];				/* Dimensional increment through clut (in doubles) */
	int dcube[1 << MAX_CHAN];		/* Hyper cube offsets (in doubles) */
	icmRevTable rit[MAX_CHAN];		/* Reverse input table information */
	icmRevTable rot[MAX_CHAN];		/* Reverse output table information */
	sx_flip_info finfo[MAX_CHAN];	/* Optimised simplex flip information */

	unsigned int inputTable_size;	/* size allocated to input table */
	unsigned int clutTable_size;	/* size allocated to clut table */
	unsigned int outputTable_size;	/* size allocated to output table */

	/* Optimised simplex orientation information. oso_ffa is NZ if valid. */
	/* Only valid if inputChan > 1 && clutPoints > 1 */
									/* oso_ff[01] will be NULL if not valid */
	unsigned short *oso_ffa;		/* Flip flags for inputChan-1 dimensions, organised */
									/* [inputChan 0, 0..cp-2]..[inputChan ic-2, 0..cp-2] */
	unsigned short *oso_ffb;		/* Flip flags for dimemension inputChan-1, organised */
									/* [0..cp-2] */
	int odinc[MAX_CHAN];			/* Dimensional increment through oso_ffa */
	
	/* return the minimum and maximum values of the given channel in the clut */
	void (*min_max) (struct _icmLut *pp, double *minv, double *maxv, int chan);

	/* Translate color values through 3x3 matrix, input tables only, multi-dimensional lut, */
	/* or output tables, */
	int (*lookup_matrix)  (struct _icmLut *pp, double *out, double *in);
	int (*lookup_input)   (struct _icmLut *pp, double *out, double *in);
	int (*lookup_clut_nl) (struct _icmLut *pp, double *out, double *in);
	int (*lookup_clut_sx) (struct _icmLut *pp, double *out, double *in);
	int (*lookup_output)  (struct _icmLut *pp, double *out, double *in);

	/* Public: */

	/* return non zero if matrix is non-unity */
	int (*nu_matrix) (struct _icmLut *pp);

    unsigned int	inputChan;      /* Num of input channels */
    unsigned int	outputChan;     /* Num of output channels */
    unsigned int	clutPoints;     /* Num of grid points */
    unsigned int	inputEnt;       /* Num of in-table entries (must be 256 for Lut8) */
    unsigned int	outputEnt;      /* Num of out-table entries (must be 256 for Lut8) */
    double			e[3][3];		/* 3 * 3 array */
	double	        *inputTable;	/* The in-table: [inputChan * inputEnt] */
	double	        *clutTable;		/* The clut: [(clutPoints ^ inputChan) * outputChan] */
	double	        *outputTable;	/* The out-table: [outputChan * outputEnt] */
	/* inputTable  is organized [inputChan 0..ic-1][inputEnt 0..ie-1] */
	/* clutTable   is organized [inputChan 0, 0..cp-1]..[inputChan ic-1, 0..cp-1]
	                                                                [outputChan 0..oc-1] */
	/* outputTable is organized [outputChan 0..oc-1][outputEnt 0..oe-1] */

	/* Helper function to setup a Lut tables contents */
	int (*set_tables) (
		struct _icmLut *p,						/* Pointer to Lut object */
		int     flags,							/* Setting flags */
		void   *cbctx,							/* Opaque callback context pointer value */
		icColorSpaceSignature insig, 			/* Input color space */
		icColorSpaceSignature outsig, 			/* Output color space */
		void (*infunc)(void *cbctx, double *out, double *in),
								/* Input transfer function, inspace->inspace' (NULL = default) */
		double *inmin, double *inmax,			/* Maximum range of inspace' values */
												/* (NULL = default) */
		void (*clutfunc)(void *cbntx, double *out, double *in),
								/* inspace' -> outspace' transfer function */
		double *clutmin, double *clutmax,		/* Maximum range of outspace' values */
												/* (NULL = default) */
		void (*outfunc)(void *cbntx, double *out, double *in),
								/* Output transfer function, outspace'->outspace (NULL = deflt) */
		int *apxls_gmin, int *apxls_gmax	/* If not NULL, the grid indexes not to be affected */
										/* by ICM_CLUT_SET_APXLS, defaulting to 0..>clutPoints-1 */
	);

	/* Helper function to fine tune a single value interpolation */
	/* Return 0 on success, 1 if input clipping occured, 2 if output clipping occured */
	/* To guarantee the optimal function, an icmLuLut needs to be create. */
	/* The default will be to assume simplex interpolation will be used. */
	int (*tune_value) (
		struct _icmLut *p,				/* Pointer to Lut object */
		double *out,					/* Target value */
		double *in);					/* Input value */
		
}; typedef struct _icmLut icmLut;

/* Helper function to set multiple Lut tables simultaneously. */
/* Note that these tables all have to be compatible in */
/* having the same configuration and resolutions, and the */
/* same per channel input and output curves. */
/* Set errc and return error number in underlying icc */
/* Note that clutfunc in[] value has "index under". */
/* If ICM_CLUT_SET_FILTER is set, the per grid node filtering radius */
/* is returned in clutfunc out[-1], out[-2] etc for each table */
int icmSetMultiLutTables(
	int ntables,						/* Number of tables to be set, 1..n */
	struct _icmLut **p,					/* Pointer to Lut object */
	int     flags,						/* Setting flags */
	void   *cbctx,						/* Opaque callback context pointer value */
	icColorSpaceSignature insig, 		/* Input color space */
	icColorSpaceSignature outsig, 		/* Output color space */
	void (*infunc)(void *cbctx, double *out, double *in),
							/* Input transfer function, inspace->inspace' (NULL = default) */
							/* Will be called ntables times each input grid value */
	double *inmin, double *inmax,		/* Maximum range of inspace' values */
										/* (NULL = default) */
	void (*clutfunc)(void *cbntx, double *out, double *in),
							/* inspace' -> outspace[ntables]' transfer function */
							/* will be called once for each input' grid value, and */
							/* ntables output values should be written consecutively */
							/* to out[]. */
	double *clutmin, double *clutmax,	/* Maximum range of outspace' values */
										/* (NULL = default) */
	void (*outfunc)(void *cbntx, double *out, double *in),
								/* Output transfer function, outspace'->outspace (NULL = deflt) */
								/* Will be called ntables times on each output value */
	int *apxls_gmin, int *apxls_gmax	/* If not NULL, the grid indexes not to be affected */
										/* by ICM_CLUT_SET_APXLS, defaulting to 0..>clutPoints-1 */
);
		
/* - - - - - - - - - - - - - - - - - - - - -  */
/* Measurement Data */
struct _icmMeasurement {
	ICM_BASE_MEMBERS(struct _icmMeasurement)

	/* Public: */
    icStandardObserver           observer;       /* Standard observer */
    icmXYZNumber                 backing;        /* XYZ for backing */
    icMeasurementGeometry        geometry;       /* Meas. geometry */
    double                       flare;          /* Measurement flare */
    icIlluminant                 illuminant;     /* Illuminant */
}; typedef struct _icmMeasurement icmMeasurement;

/* - - - - - - - - - - - - - - - - - - - - -  */

/* Pseudo Type that maps to icSigTextDescriptionType on V2 profiles and */
/* icSigMultiLocalizedUnicodeType on V4 profiles. */
#define icmMultiLocalizedTextDescriptionType ((icTagTypeSignature)icmMakeTag('m','l','t','d'))

/* Base class common to icmTextDescription and icmMultiLocalizedUnicode classes */ 
#define ICM_MLTD_MEMBERS(TTYPE)		/* This tag type */										\
	ICM_BASE_MEMBERS(TTYPE)																	\
																							\
	/* Private: */																			\
	unsigned int     _count;		/* Count currently allocated */							\
																							\
	/* Public: */																			\
	unsigned int 	  count;		/* Allocated and used size of desc, inc null */			\
	char              *desc;		/* ascii or utf-8 string (null terminated) */			\

struct _icmMultiLocalizedTextDescription {
	ICM_MLTD_MEMBERS(struct _icmMultiLocalizedTextDescription)
}; typedef struct _icmMultiLocalizedTextDescription icmMultiLocalizedTextDescription;


/* - - - - - - - - - - - - - - - - - - - - -  */
/* Named color */

/* Structure that holds each named color data */
typedef struct {
	struct _icc      *icp;				/* Pointer to ICC we're a part of */
	char              root[32];			/* Root name for color */
	double            pcsCoords[3];		/* icmNC2: PCS coords of color */
	double            deviceCoords[MAX_CHAN];	/* Dev coords of color */
} icmNamedColorVal;

struct _icmNamedColor {
	ICM_BASE_MEMBERS(struct _icmNamedColor)

	/* Private: */
	unsigned int      _count;			/* Count currently allocated */

	/* Public: */
    unsigned int      vendorFlag;		/* Bottom 16 bits for IC use */
    unsigned int      count;			/* Count of named colors */
    unsigned int      nDeviceCoords;	/* Num of device coordinates */
    char              prefix[32];		/* Prefix for each color name (null terminated) */
    char              suffix[32];		/* Suffix for each color name (null terminated) */
    icmNamedColorVal  *data;			/* Array of [count] color values */
}; typedef struct _icmNamedColor icmNamedColor;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Colorant table */
/* (Contribution from Piet Vandenborre, derived from NamedColor) */

/* Structure that holds colorant table data */
typedef struct {
	struct _icc         *icp;			/* Pointer to ICC we're a part of */
	char                 name[32];		/* Name for colorant */
	double               pcsCoords[3];	/* PCS coords of colorant */
} icmColorantTableVal;

struct _icmColorantTable {
	ICM_BASE_MEMBERS(struct _icmColorantTable)

	/* Private: */
	unsigned int         _count;		/* Count currently allocated */

	/* Public: */
    unsigned int         count;			/* Count of colorants */
    icmColorantTableVal *data;			/* Array of [count] colorants */
}; typedef struct _icmColorantTable icmColorantTable;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* textDescription */
struct _icmTextDescription {
	ICM_MLTD_MEMBERS(struct _icmTextDescription)

	/* Private: */
	unsigned int   uc16Count;		/* uc UTF-16 file character count */
	unsigned int   uc_count;		/* uc Count currently allocated */
	int            (*core_read)(struct _icmTextDescription *p, char **bpp, char *end);
	int            (*core_write)(struct _icmTextDescription *p, char **bpp);

	/* Public: */

	/* ascii count and *desc are defined in ICM_MLTD_MEMBERS */
	/* unsigned int      _count;	   Count currently allocated */
	/* unsigned int 	  count;	   Allocated and used size of desc, inc null */
	/* char              *desc;		   ascii string (null terminated) */

	unsigned int      ucLangCode;	/* UniCode language code */
	unsigned int 	  ucCount;		/* Allocated and used size of ucDesc in chars, inc null */
	icmUTF8           *uc8Desc;		/* The utf-8 description (nul terminated) */

	/* See <http://unicode.org/Public/MAPPINGS/VENDORS/APPLE/ReadMe.txt> */
	ORD16             scCode;		/* ScriptCode code */
	unsigned int 	  scCount;		/* Used size of scDesc in bytes, inc null */
	ORD8              scDesc[67];	/* ScriptCode Description (null terminated, max 67) */
}; typedef struct _icmTextDescription icmTextDescription;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Profile sequence structure */
struct _icmDescStruct {
	/* Private: */
	struct _icc      *icp;				/* Pointer to ICC we're a part of */

	/* Public: */
	int             (*allocate)(struct _icmDescStruct *p);	/* Allocate method */
    icmSig            deviceMfg;		/* Dev Manufacturer */
    unsigned int      deviceModel;		/* Dev Model */
    icmUInt64         attributes;		/* Dev attributes */
    icTechnologySignature technology;	/* Technology sig */
	icmTextDescription device;			/* Manufacturer text (sub structure) */
	icmTextDescription model;			/* Model text (sub structure) */
}; typedef struct _icmDescStruct icmDescStruct;

/* Profile sequence description */
struct _icmProfileSequenceDesc {
	ICM_BASE_MEMBERS(struct _icmProfileSequenceDesc)

	/* Private: */
	unsigned int	 _count;			/* number currently allocated */

	/* Public: */
    unsigned int      count;			/* Number of descriptions */
	icmDescStruct     *data;			/* array of [count] descriptions */
}; typedef struct _icmProfileSequenceDesc icmProfileSequenceDesc;

#ifdef NEW
/* - - - - - - - - - - - - - - - - - - - - -  */
/* ResponseCurveSet16 */

struct _icmResponseCurveSet16 {
	ICM_BASE_MEMBERS(struct _icmResponseCurveSet16)

	/* Private: */
	~~~999

	/* Public: */
	~~~999

}; typedef struct _icmResponseCurveSet16 icmResponseCurveSet16;

#endif /* NEW */

/* - - - - - - - - - - - - - - - - - - - - -  */
/* signature (only ever used for technology ??) */
struct _icmSignature {
	ICM_BASE_MEMBERS(struct _icmSignature)

	/* Public: */
    icTechnologySignature sig;	/* Signature */
}; typedef struct _icmSignature icmSignature;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Per channel Screening Data */
typedef struct {
	/* Public: */
    double            frequency;		/* Frequency */
    double            angle;			/* Screen angle */
    icSpotShape       spotShape;		/* Spot Shape encodings below */
} icmScreeningData;

struct _icmScreening {
	ICM_BASE_MEMBERS(struct _icmScreening)

	/* Private: */
	unsigned int   _channels;			/* number currently allocated */

	/* Public: */
    unsigned int      screeningFlag;	/* Screening flag */
    unsigned int      channels;			/* Number of channels */
    icmScreeningData  *data;			/* Array of screening data */
}; typedef struct _icmScreening icmScreening;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Under color removal, black generation */
struct _icmUcrBg {
	ICM_BASE_MEMBERS(struct _icmUcrBg)

	/* Private: */
    unsigned int      UCR_count;		/* Currently allocated UCR count */
    unsigned int      BG_count;			/* Currently allocated BG count */
	unsigned int 	  _count;			/* Currently allocated string size */

	/* Public: */
    unsigned int      UCRcount;			/* Undercolor Removal Curve length */
    double           *UCRcurve;		    /* The array of UCR curve values, 0.0 - 1.0 */
										/* or 0.0 - 100 % if count = 1 */
    unsigned int      BGcount;			/* Black generation Curve length */
    double           *BGcurve;			/* The array of BG curve values, 0.0 - 1.0 */
										/* or 0.0 - 100 % if count = 1 */
	unsigned int 	  count;			/* Allocated and used size of desc, inc null */
	char              *string;			/* UcrBg description (null terminated) */
}; typedef struct _icmUcrBg icmUcrBg;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* viewingConditionsType */
struct _icmViewingConditions {
	ICM_BASE_MEMBERS(struct _icmViewingConditions)

	/* Public: */
    icmXYZNumber    illuminant;		/* In candelas per sq. meter */
    icmXYZNumber    surround;		/* In candelas per sq. meter */
    icIlluminant    stdIlluminant;	/* See icIlluminant defines */
}; typedef struct _icmViewingConditions icmViewingConditions;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Postscript Color Rendering Dictionary names type */
struct _icmCrdInfo {
	ICM_BASE_MEMBERS(struct _icmCrdInfo)
	/* Private: */
    unsigned int     _ppsize;		/* Currently allocated size */
	unsigned int     _crdsize[4];	/* Currently allocated sizes */

	/* Public: */
    unsigned int     ppsize;		/* Postscript product name size (including null) */
    char            *ppname;		/* Postscript product name (null terminated) */
	unsigned int     crdsize[4];	/* Rendering intent 0-3 CRD names sizes (icluding null) */
	char            *crdname[4];	/* Rendering intent 0-3 CRD names (null terminated) */
}; typedef struct _icmCrdInfo icmCrdInfo;

/* - - - - - - - - - - - - - - - - - - - - -  */
/* Apple ColorSync 2.5 video card gamma type (vcgt) */
struct _icmVideoCardGammaTable {
	unsigned short   channels;		/* No of gamma channels (1 or 3) */
	unsigned short   entryCount; 	/* Number of entries per channel */
	unsigned short   entrySize;		/* Size in bytes of each entry */
	void            *data;			/* Variable size data */
}; typedef struct _icmVideoCardGammaTable icmVideoCardGammaTable;

struct _icmVideoCardGammaFormula {
	unsigned short   channels;		/* No of gamma channels (always 3) */
	double           redGamma;		/* must be > 0.0 */
	double           redMin;		/* must be > 0.0 and < 1.0 */
	double           redMax;		/* must be > 0.0 and < 1.0 */
	double           greenGamma;   	/* must be > 0.0 */
	double           greenMin;		/* must be > 0.0 and < 1.0 */
	double           greenMax;		/* must be > 0.0 and < 1.0 */
	double           blueGamma;		/* must be > 0.0 */
	double           blueMin;		/* must be > 0.0 and < 1.0 */
	double           blueMax;		/* must be > 0.0 and < 1.0 */
}; typedef struct _icmVideoCardGammaFormula icmVideoCardGammaFormula;

typedef enum {
	icmVideoCardGammaTableType = 0,
	icmVideoCardGammaFormulaType = 1
} icmVideoCardGammaTagType;

struct _icmVideoCardGamma {
	ICM_BASE_MEMBERS(struct _icmVideoCardGamma)
	icmVideoCardGammaTagType tagType;	/* eg. table or formula, use above enum */
	union {
		icmVideoCardGammaTable   table;
		icmVideoCardGammaFormula formula;
	} u;

	double (*lookup)(struct _icmVideoCardGamma *p, int chan, double iv); /* Read a value */

}; typedef struct _icmVideoCardGamma icmVideoCardGamma;

/* ------------------------------------------------- */
/* The Profile header */
/* NOTE that the deviceClass should be set up before calling chromAdaptMatrix()! */ 
struct _icmHeader {

#ifdef USE_LEGACY_HEADERFUNC

  /* Private: */
	unsigned int           (*get_size)(struct _icmHeader *p);
	int                    (*read)(struct _icmHeader *p, unsigned int len, unsigned int of);
	int                    (*write)(struct _icmHeader *p, unsigned int of, int doid);
	void                   (*del)(struct _icmHeader *p);
	struct _icc            *icp;			/* Pointer to ICC we're a part of */
    unsigned int            size;			/* Profile size in bytes */

  /* public: */
	void                   (*dump)(struct _icmHeader *p, icmFile *op, int verb);

#else /* !USE_LEGACY_HEADERFUNC */

	// NEW:
	ICM_BASE_MEMBERS(struct _icmHeader)

	unsigned int hsize;		/* Header size */
	unsigned int phsize;	/* Padded header size */
	unsigned int size;		/* Nominated size of the profile  */

	int doid;				/* Flag, nz = writing to compute ID */

#endif /* !USE_LEGACY_HEADERFUNC */

	/* Values that must be set before writing */
    icProfileClassSignature deviceClass;	/* Type of profile */
    icColorSpaceSignature   colorSpace;		/* Clr space of data (Cannonical input space) */
    icColorSpaceSignature   pcs;			/* PCS: XYZ or Lab (Cannonical output space) */
    icRenderingIntent       renderingIntent;/* Rendering intent (lower 16 bits) */

	/* Values that should be set before writing */
    icmSig                  manufacturer;	/* Dev manufacturer */
    icmSig		            model;			/* Dev model */
    icmUInt64               attributes;		/* Device attributes */
    icProfileFlags			flags;			/* Various bits */

	/* Values that may optionally be set before writing */
    /* icmUInt64            attributes;		   Device attributes.h (see above) */
    icmSig                  creator;		/* Profile creator */

	/* Values that are not normally set, since they have defaults */
    icmSig                  cmmId;			/* CMM for profile */
	icmVers                 vers;			/* Format version */
    icmDateTimeNumber       date;			/* Creation Date */
    icPlatformSignature     platform;		/* Primary Platform */
    icmXYZNumber            illuminant;		/* Profile illuminant */
    icRenderingIntent       extraRenderingIntent;	/* top 16 bits are extra non-ICC part of RI */

	/* Values that are created automatically */
    unsigned char           id[16];			/* MD5 fingerprint value, lsB to msB  <V4.0+> */

}; typedef struct _icmHeader icmHeader;

/* ---------------------------------------------------------- */
/* Objects for accessing lookup functions. */
/* Note that the "effective" PCS colorspace is the one specified by the */
/* PCS override, and visible in the overal profile conversion. */
/* The "native" PCS colorspace is the one that the underlying tags actually */
/* represent, and the PCS that the individual stages within each profile type handle. */
/* The conversion between the native and effective PCS is done in the to/from_abs() */
/* conversions. */

/* Public: Parameter to get_luobj function */
typedef enum {
    icmFwd           = 0,  /* Device to PCS, or Device 1 to Last Device */
    icmBwd           = 1,  /* PCS to Device, or Last Device to Device */
    icmGamut         = 2,  /* PCS Gamut check */
    icmPreview       = 3   /* PCS to PCS preview */
} icmLookupFunc;

/* Public: Parameter to get_luobj function */
typedef enum {
    icmLuOrdNorm     = 0,  /* Normal profile preference: Lut, matrix, monochrome */
    icmLuOrdRev      = 1   /* Reverse profile preference: monochrome, matrix, monochrome */
} icmLookupOrder;

/* Public: Lookup algorithm object type */
typedef enum {
    icmMonoFwdType       = 0,	/* Monochrome, Forward */
    icmMonoBwdType       = 1,	/* Monochrome, Backward */
    icmMatrixFwdType     = 2,	/* Matrix, Forward */
    icmMatrixBwdType     = 3,	/* Matrix, Backward */
    icmLutType           = 4,	/* Multi-dimensional Lookup Table */
    icmNamedType         = 5	/* Named color data */
} icmLuAlgType;

/* Lookup class members common to named and non-named color types */
#define LU_ICM_BASE_MEMBERS(TTYPE)															\
	/* Private: */																			\
	icmLuAlgType   ttype;		    	/* The object tag */								\
	struct _icc    *icp;				/* Pointer to ICC we're a part of */				\
	icRenderingIntent intent;			/* Effective (externaly visible) intent */			\
	icmLookupFunc function;				/* Functionality being used */						\
	icmLookupOrder order;        		/* Conversion representation search Order */ 		\
	icmXYZNumber pcswht, whitePoint, blackPoint;	/* White and black point info (absolute XYZ) */	\
	int blackisassumed;					/* nz if black point tag is missing from profile */ \
	double toAbs[3][3];					/* Matrix to convert from relative to absolute */ 	\
	double fromAbs[3][3];				/* Matrix to convert from absolute to relative */ 	\
    icColorSpaceSignature inSpace;		/* Native Clr space of input */						\
    icColorSpaceSignature outSpace;		/* Native Clr space of output */					\
	icColorSpaceSignature pcs;			/* Native PCS */									\
    icColorSpaceSignature e_inSpace;	/* Effective Clr space of input */					\
    icColorSpaceSignature e_outSpace;	/* Effective Clr space of output */					\
	icColorSpaceSignature e_pcs;		/* Effective PCS */									\
																							\
	/* Public: */																			\
	void           (*del)(TTYPE *p);											\
																							\
	               /* Internal native colorspaces: */	     								\
	void           (*lutspaces) (TTYPE *p, icColorSpaceSignature *ins, int *inn,	\
	                                                   icColorSpaceSignature *outs, int *outn,	\
	                                                   icColorSpaceSignature *pcs);			\
	                                                                                        \
	               /* External effecive colorspaces */										\
	void           (*spaces) (TTYPE *p, icColorSpaceSignature *ins, int *inn,	\
	                                 icColorSpaceSignature *outs, int *outn,				\
	                                 icmLuAlgType *alg, icRenderingIntent *intt, 			\
	                                 icmLookupFunc *fnc, icColorSpaceSignature *pcs,		\
	                                 icmLookupOrder *ord);							 		\
																							\
	               /* Relative to Absolute for this WP in XYZ */   							\
	void           (*XYZ_Rel2Abs)(TTYPE *p, double *xyzout, double *xyzin);		\
																							\
	               /* Absolute to Relative for this WP in XYZ */   							\
	void           (*XYZ_Abs2Rel)(TTYPE *p, double *xyzout, double *xyzin);		\


/* Non-algorithm specific lookup class. Used as base class of algorithm specific class. */
#define LU_ICM_NN_BASE_MEMBERS(TTYPE)														\
    LU_ICM_BASE_MEMBERS(TTYPE)                                                              \
																							\
	/* Public: */																			\
																							\
	/* Get the native input space and output space ranges */								\
	/* This is an accurate number of what the underlying profile can hold. */				\
	void (*get_lutranges) (TTYPE *p,														\
		double *inmin, double *inmax,		/* Range of inspace values */					\
		double *outmin, double *outmax);	/* Range of outspace values */					\
																							\
	/* Get the effective input space and output space ranges */								\
	/* This may not be accurate when the effective type is different to the native type */	\
	void (*get_ranges) (TTYPE *p,															\
		double *inmin, double *inmax,		/* Range of inspace values */					\
		double *outmin, double *outmax);	/* Range of outspace values */					\
																							\
	/* Initialise the white and black points from the ICC tags */							\
	/* and the corresponding absolute<->relative conversion matrices */						\
	/* Return nz on error */																\
	int (*init_wh_bk)(TTYPE *p);															\
																							\
	/* Get the LU white and black points in absolute XYZ space. */							\
	/* Return nz if the black point is being assumed to be 0,0,0 rather */					\
	/* than being from the tag. */															\
	int (*wh_bk_points)(TTYPE *p, double *wht, double *blk);								\
																							\
	/* Get the LU white and black points in LU PCS space, converted to XYZ. */				\
	/* (ie. white and black will be relative if LU is relative intent etc.) */				\
	/* Return nz if the black point is being assumed to be 0,0,0 rather */					\
	/* than being from the tag. */															\
	int (*lu_wh_bk_points)(TTYPE *p, double *wht, double *blk);								\
																							\
	/* Translate color values through profile in effective in and out colorspaces, */		\
	/* return values: */																	\
	/* 0 = success, 1 = warning: clipping occured, 2 = fatal: other error */				\
	/* Note that clipping is not a reliable means of detecting out of gamut */				\
	/* in the lookup(bwd) call for clut based profiles. */									\
	int (*lookup) (TTYPE *p, double *out, double *in);										\
																							\
																							\
	/* Alternate to above, splits color conversion into three steps. */						\
	/* Colorspace of _in and _out and _core are the effective in and out */					\
	/* colorspaces. Note that setting colorspace overrides will probably. */				\
	/* force _in and/or _out to be dumy (unity) transforms. */								\
																							\
	/* Per channel input lookup (may be unity): */											\
	int (*lookup_in) (TTYPE *p, double *out, double *in);									\
																							\
	/* Intra channel conversion: */															\
	int (*lookup_core) (TTYPE *p, double *out, double *in);									\
																							\
	/* Per channel output lookup (may be unity): */											\
	int (*lookup_out) (TTYPE *p, double *out, double *in);									\
																							\
	/* Inverse versions of above - partially implemented */									\
	/* Inverse per channel input lookup (may be unity): */									\
	int (*lookup_inv_in) (TTYPE *p, double *out, double *in);								\
																							\


/* Base lookup object */
struct _icmLuBase {
	LU_ICM_NN_BASE_MEMBERS(struct _icmLuBase)
}; typedef struct _icmLuBase icmLuBase;


/* Algorithm specific lookup classes */

/* Monochrome  Fwd & Bwd type object */
struct _icmLuMono {
	LU_ICM_NN_BASE_MEMBERS(struct _icmLuMono)
	icmCurve    *grayCurve;

	/* Overall lookups */
	int (*fwd_lookup) (struct _icmLuMono *p, double *out, double *in);
	int (*bwd_lookup) (struct _icmLuMono *p, double *out, double *in);

	/* Components of lookup */
	int (*fwd_curve) (struct _icmLuMono *p, double *out, double *in);
	int (*fwd_map)   (struct _icmLuMono *p, double *out, double *in);
	int (*fwd_abs)   (struct _icmLuMono *p, double *out, double *in);
	int (*bwd_abs)   (struct _icmLuMono *p, double *out, double *in);
	int (*bwd_map)   (struct _icmLuMono *p, double *out, double *in);
	int (*bwd_curve) (struct _icmLuMono *p, double *out, double *in);

}; typedef struct _icmLuMono icmLuMono;

/* 3D Matrix Fwd & Bwd type object */
struct _icmLuMatrix {
	LU_ICM_NN_BASE_MEMBERS(struct _icmLuMatrix)
	icmCurve    *redCurve, *greenCurve, *blueCurve;
	icmXYZArray *redColrnt, *greenColrnt, *blueColrnt;
    double		mx[3][3];	/* 3 * 3 conversion matrix */
    double		bmx[3][3];	/* 3 * 3 backwards conversion matrix */

	/* Overall lookups */
	int (*fwd_lookup) (struct _icmLuMatrix *p, double *out, double *in);
	int (*bwd_lookup) (struct _icmLuMatrix *p, double *out, double *in);

	/* Components of lookup */
	int (*fwd_curve)  (struct _icmLuMatrix *p, double *out, double *in);
	int (*fwd_matrix) (struct _icmLuMatrix *p, double *out, double *in);
	int (*fwd_abs)    (struct _icmLuMatrix *p, double *out, double *in);
	int (*bwd_abs)    (struct _icmLuMatrix *p, double *out, double *in);
	int (*bwd_matrix) (struct _icmLuMatrix *p, double *out, double *in);
	int (*bwd_curve)  (struct _icmLuMatrix *p, double *out, double *in);

}; typedef struct _icmLuMatrix icmLuMatrix;

/* Multi-D. Lut type object */
struct _icmLuLut {
	LU_ICM_NN_BASE_MEMBERS(struct _icmLuLut)

	/* private: */
	icmLut *lut;								/* Lut to use */
	int    usematrix;							/* non-zero if matrix should be used */
    double imx[3][3];							/* 3 * 3 inverse conversion matrix */
	int    imx_valid;							/* Inverse matrix is valid */
	void (*in_normf)(double *out, double *in);	/* Lut input data normalizing function */
	void (*in_denormf)(double *out, double *in);/* Lut input data de-normalizing function */
	void (*out_normf)(double *out, double *in);	/* Lut output data normalizing function */
	void (*out_denormf)(double *out, double *in);/* Lut output de-normalizing function */
	void (*e_in_denormf)(double *out, double *in);/* Effective input de-normalizing function */
	void (*e_out_denormf)(double *out, double *in);/* Effecive output de-normalizing function */
	/* function chosen out of lut->lookup_clut_sx and lut->lookup_clut_nl to imp. clut() */
	int (*lookup_clut) (struct _icmLut *pp, double *out, double *in);	/* clut function */

	/* public: */

	/* Components of lookup */
	int (*in_abs)  (struct _icmLuLut *p, double *out, double *in);	/* Should be in icmLut ? */
	int (*matrix)  (struct _icmLuLut *p, double *out, double *in);
	int (*input)   (struct _icmLuLut *p, double *out, double *in);
	int (*clut)    (struct _icmLuLut *p, double *out, double *in);
	int (*output)  (struct _icmLuLut *p, double *out, double *in);
	int (*out_abs) (struct _icmLuLut *p, double *out, double *in);	/* Should be in icmLut ? */

	/* Some inverse components, in reverse order */
	/* Should be in icmLut ??? */
	int (*inv_out_abs) (struct _icmLuLut *p, double *out, double *in);
	int (*inv_output)  (struct _icmLuLut *p, double *out, double *in);
	/* inv_clut is beyond scope of icclib. See argyll for solution! */
	int (*inv_input)   (struct _icmLuLut *p, double *out, double *in);
	int (*inv_matrix)  (struct _icmLuLut *p, double *out, double *in);
	int (*inv_in_abs)  (struct _icmLuLut *p, double *out, double *in);

	/* Get various types of information about the LuLut */
	/* (Note that there's no reason why white/black point info */
	/*  isn't being made available for other Lu types) */
	void (*get_info) (struct _icmLuLut *p, icmLut **lutp,
	                 icmXYZNumber *pcswhtp, icmXYZNumber *whitep,
	                 icmXYZNumber *blackp);

	/* Get the matrix contents */
	void (*get_matrix) (struct _icmLuLut *p, double m[3][3]);

}; typedef struct _icmLuLut icmLuLut;

/* Named colors lookup object */
struct _icmLuNamed {
	LU_ICM_BASE_MEMBERS(struct _icmLuNamed)

  /* Private: */

  /* Public: */

	/* Get various types of information about the Named lookup */
	void (*get_info) (struct _icmLuLut *p, 
	                 icmXYZNumber *pcswhtp, icmXYZNumber *whitep,
	                 icmXYZNumber *blackp,
	                 int *maxnamesize,
	                 int *num_colors, char *prefix, char *suffix);


	/* Lookup a name that includes prefix and suffix */
	int (*fullname_lookup) (struct _icmLuNamed *p, double *pcs, double *dev, char *in);

	/* Lookup a name that doesn't include prefix and suffix */
	int (*name_lookup) (struct _icmLuNamed *p, double *pcs, double *dev, char *in);

	/* Fill in a list with the nout closest named colors to the pcs target */
	int (*pcs_lookup) (struct _icmLuNamed *p, char **out, int nout, double *in);

	/* Fill in a list with the nout closest named colors to the device target */
	int (*dev_lookup) (struct _icmLuNamed *p, char **out, int nout, double *in);

}; typedef struct _icmLuNamed icmLuNamed;

/* ========================================================== */

/* A tag record in the header */
typedef struct {
    icTagSignature      sig;			/* The tag signature */
	icTagTypeSignature  ttype;			/* The tag type actual signature */
    unsigned int        offset;			/* File offset relative to profile */
    unsigned int        size;			/* Size in bytes (not including padding) */
#ifdef USE_LEGACY_COREFUNC
	unsigned int        pad;			/* LEGACY: Padding in bytes */
#else
	unsigned int        psize;			/* NEW: Padded size */
#endif
	icmBase            *objp;			/* In memory data structure, NULL if never read. */
} icmTagRec;


/* =================================================== */
/* Tag version managament support */

/* Return a string that shows the icc's version */
char *icmICCvers2str(struct _icc *p);

/* A version management version number */
typedef int icmTV; 	/* Version - major * 10000 + minor * 100 + bug fix */

/* Return a string that shows the given version */
/* This can only be called once before reusing returned static buffer */
char *icmTVers2str(icmTV tv);

/* Range of versions, inclusive */
typedef struct { 
	icmTV min, max;
} icmTVRange;

/* Return a string that shows the given version range */
/* This can only be called once before reusing returned static buffer */
char *icmTVersRange2str(icmTVRange *tvr);

/* True if a given version is within the rang. */
#define ICMTV_IN_RANGE(vers, vrangep) ((vrangep)->min <= (vers) && (vers) <= (vrangep)->max)

/* Pre-defined version extremes */
#define ICMTV_MIN 0
#define ICMTV_MAX 999999

#define ICMTV_20 20000
#define ICMTV_21 20100		/* (N-Color) */
#define ICMTV_22 20200
#define ICMTV_23 20300		/* (Chromaticity Tag) */
#define ICMTV_24 20400		/* (Display etc. have intents) */
#define ICMTV_40 40000		/* (LutAToB etc., ColorantOrder etc.) */
#define ICMTV_41 40100
#define ICMTV_42 40200
#define ICMTV_43 40300		/* (BToD0 etc.) */
#define ICMTV_44 40400		/* (Cicp, Metadata) */

#define ICMTV_DEFAULT ICMTV_22		/* To use default creation ICC version */

#define ICMTVRANGE_ALL { ICMTV_MIN, ICMTV_MAX }
#define ICMTVRANGE_NONE { ICMTV_MAX, ICMTV_MIN }

#define ICMTVRANGE_20_PLUS { ICMTV_20, ICMTV_MAX }
#define ICMTVRANGE_21_PLUS { ICMTV_21, ICMTV_MAX }
#define ICMTVRANGE_22_PLUS { ICMTV_22, ICMTV_MAX }
#define ICMTVRANGE_23_PLUS { ICMTV_23, ICMTV_MAX }
#define ICMTVRANGE_24_PLUS { ICMTV_24, ICMTV_MAX }
#define ICMTVRANGE_40_PLUS { ICMTV_40, ICMTV_MAX }
#define ICMTVRANGE_41_PLUS { ICMTV_41, ICMTV_MAX }
#define ICMTVRANGE_42_PLUS { ICMTV_42, ICMTV_MAX }
#define ICMTVRANGE_43_PLUS { ICMTV_43, ICMTV_MAX }
#define ICMTVRANGE_44_PLUS { ICMTV_44, ICMTV_MAX }

extern const icmTVRange icmtvrange_all;
extern const icmTVRange icmtvrange_none;

/* icmTV from a icmVers */
#define ICMVERS2TV(vers)  (((vers).majv * 100 + (vers).minv) * 100 + (vers).bfv)

/* Return true if icc version is within range */
int icmVersInRange(struct _icc *icp, icmTVRange *tvr); 

/* Return true if the two version ranges have an overlap */
int icmVersOverlap(icmTVRange *tvr1, icmTVRange *tvr2); 

/* Return a string that shows the icc version */
char *icmProfileVers2str(struct _icc *icp);

/* [Table] A record of tag type, valid version range and constructor. */
typedef	struct {
	icTagTypeSignature  ttype;			/* The tag type signature */
	icmTVRange        vrange;			/* File versions it is valid to use it in (inclusive) */
	icmBase *(*new_obj)(struct _icc *icp);
} icmTagTypeVersConstrRec;


/* transform class - used to add semantics to lut tags */
typedef enum {
	icmTCNone    = 0,		/* Not Applicable */

	/* Lut classes - determines in/out colorspace */
	icmTCLutFwd     = 1,	/* AtoBn:    Device to PCS */
	icmTCLutBwd     = 2,	/* BtoAn:    PCS to Device */
	icmTCLutGamut   = 3,	/* Gamut:    PCS to Gray */
	icmTCLutPreview = 4 	/* Preview:	 PCS to PCS */
} icmLutClass;

/* A record of tag type, valid version range and constructor. */
typedef	struct {
	icTagTypeSignature  ttype;			/* The tag type signature */
	icmTVRange          vrange;			/* File versions it is valid to use it in (inclusive) */
} icmTagTypeAndVersRec;

/* [Table] A record of tag types permitted for each tag sig */
typedef struct {
	icTagSignature       sig;
	icmTVRange           vrange;		/* Versions it is valid over (inclusive) */
	icmLutClass          cclass;		/* Compatibility class enum */
	icmTagTypeAndVersRec ttypes[5];		/* ttypes and file versions ttype can be used with sig */
										/* Compatibility is intersection of TT vers & this vers. */
										/* Last has ttype icMaxEnumType */
} icmTagSigVersTypesRec;


/* A record of a tag sig and file versions it is valid to use it in */
typedef	struct {
	icTagSignature      sig;			/* The tag signature */
	icmTVRange          vrange;			/* File versions it is valid to use it in (inclusive) */
} icmTagSigVersRec;

typedef enum {
    icmCSMF_ANY = 1, 	/* Any colorspace */
    icmCSMF_XYZ = 2, 	/* icSigXYZData is a match */
    icmCSMF_Lab = 3, 	/* icSigLabData is a match */
    icmCSMF_PCS = 4, 	/* icSigXYZData or icSigLabData is a match */
    icmCSMF_DEV = 5, 	/* Device colorspace */
    icmCSMF_NCOL = 6, 	/* Device N-color space */
    icmCSMF_NOT_NCOL = 7, /* Not a device N-color space */
    icmCSMF_DEV_NOT_NCOL = 8 /* Device colorspace but not device N-color space */
} icmCSMF;

/* Colorspace matching requirements - true if matches all. */
typedef struct {
	icmCSMF mf;				/* Match flags */ 
	int min, max;			/* Device min and max channel count, inclusive, */
							/* Ignored if either is 0 */
} icmCSMR;

#define ICMCSMR_ANY 			{ icmCSMF_ANY,          0, 0 } 
#define ICMCSMR_XYZ 			{ icmCSMF_XYZ,          0, 0 } 
#define ICMCSMR_LAB 			{ icmCSMF_LAB,          0, 0 } 
#define ICMCSMR_PCS 			{ icmCSMF_PCS,          0, 0 } 
#define ICMCSMR_NCOL 			{ icmCSMF_NCOL,         0, 0 } 
#define ICMCSMR_NOT_NCOL		{ icmCSMF_NOT_NCOL,     0, 0 } 
#define ICMCSMR_DEV_NOT_NCOL	{ icmCSMF_DEV_NOT_NCOL, 0, 0 } 
#define ICMCSMR_DEVN			{ icmCSMF_DEV,          1, 15 } 
#define ICMCSMR_DEV(NN)			{ icmCSMF_DEV,         NN, NN } 

/* [Table] Class signature instance tag requirements */
typedef struct {
	icProfileClassSignature clsig;		/* Profile class signature (i.e. input/output/link etc.) */
	icmCSMR                 colsig;		/* Data Color space signature */
	icmCSMR                 pcssig;		/* PCS Color space signature */
	icmTVRange          	vrange;		/* File versions it is valid to use it in (inclusive) */
	icmTagSigVersRec        tags[16];	/* Required Tags, terminated by icMaxEnumTag */
										/* Compatibility is intersection of T vers & this vers. */
										/* Last has sig icMaxEnumType */
	icmTagSigVersRec        otags[16];	/* Optional Tags, terminated by icMaxEnumTag */
} icmRequiredTagType;


/* - - - - - - - - - - - - - - - - - - - */
/* Pseudo enumerations valid as parameter to get_luobj(): */

/* Special purpose Perceptual intent */
#define icmAbsolutePerceptual ((icRenderingIntent)97)

/* Special purpose Saturation intent */
#define icmAbsoluteSaturation ((icRenderingIntent)98)

/* To be specified where an intent is not appropriate */
#define icmDefaultIntent ((icRenderingIntent)99)


/* Pseudo PCS colospace used to indicate the native PCS */
#define icmSigDefaultData ((icColorSpaceSignature) 0x0)

/* Pseudo colospace used to indicate a named colorspace */
#define icmSigNamedData ((icColorSpaceSignature) 0x1)

/* The ICC object */
struct _icc {
  /* Public: */
	icmFile     *(*get_rfp)(struct _icc *p);			/* Return the current read fp (if any) */
	int          (*set_version)(struct _icc *p, icmTV ver); /* For creation, use ICC V4 etc. */

	void         (*set_cflag)(struct _icc *p, icmCompatFlags flags); /* Set compatibility flags */
	void         (*unset_cflag)(struct _icc *p, icmCompatFlags flags); /* Unset compat. flags */
	icmCompatFlags (*get_cflags)(struct _icc *p);					   /* Return compat. flags */
	void         (*set_vcrange)(struct _icc *p, const icmTVRange *tvr);
															/* Set icmCFlagAllowWrVersion */ 
															/* Use ICMTVRANGE_NONE to disable */
	void		 (*clear_err)(struct _icc *p);					       /* Clear any errors */
	unsigned int (*get_size)(struct _icc *p);				/* Return total size needed, 0 = err. */
															/* Will add auto tags (i.e. 'arts') */
	int          (*read)(struct _icc *p, icmFile *fp, unsigned int of);	/* Returns error code */
	int          (*write)(struct _icc *p, icmFile *fp, unsigned int of);/* Returns error code */
															/* Will add auto tags (i.e. 'arts') */
	void         (*dump)(struct _icc *p, icmFile *op, int verb);	/* Dump whole icc */
	void         (*del)(struct _icc *p);						/* Free whole icc */
	int          (*find_tag)(struct _icc *p, icTagSignature sig);
							/* Returns 0 if found, 1 if found but not readable, 2 of not found */
	icmBase *    (*read_tag)(struct _icc *p, icTagSignature sig); /* Returns pointer to object */
																/* Returns NULL if not found */
	icmBase *    (*read_tag_any)(struct _icc *p, icTagSignature sig);
								/* Returns pointer to object, but may be icmSigUnknownType! */
	icmBase *    (*add_tag)(struct _icc *p, icTagSignature sig, icTagTypeSignature ttype);
															/* Returns pointer to object */
	int          (*rename_tag)(struct _icc *p, icTagSignature sig, icTagSignature sigNew);
															/* Rename and existing tag */
	icmBase *    (*link_tag)(struct _icc *p, icTagSignature sig, icTagSignature ex_sig);
															/* Returns pointer to object */
	int          (*unread_tag)(struct _icc *p, icTagSignature sig);
														/* Unread a tag (free on refcount == 0 */
	int          (*read_all_tags)(struct _icc *p); 		/* Read all the tags, non-zero on error. */

	int          (*delete_tag)(struct _icc *p, icTagSignature sig);	/* Delet a tag. Error if not */
															/* Returns 0 if deleted OK */
	int          (*delete_tag_quiet)(struct _icc *p, icTagSignature sig); /* Delet a tag quietly */
										/* No error if tag doesn't exist. Returns 0 if deleted OK */

	int          (*check_id)(struct _icc *p, ORD8 *id);	/* Check profiles MD5 id */
												/* Returns 0 if ID is OK, 1 if not present etc. */
	int          (*write_check)(struct _icc *p);	/* Check profile is legal to write. */
													/* Return error code. */
	double       (*get_tac)(struct _icc *p, double *chmax,
	                        void (*calfunc)(void *cntx, double *out, double *in), void *cntx);
 	                           /* Returns total ink limit and channel maximums */
	                           /* calfunc is optional. */
	icmLutClass  (*get_tag_lut_class)(struct _icc *p, icTagSignature sig);
									/* Return the lut class value for a tag signature. */
									/* Return icmTCUnknown if there is not sigtypetable, */
									/* or the signature is not recognised. */
	void         (*set_illum)(struct _icc *p, double ill_wp[3]);
								/* Clear any existing 'chad' matrix, and if Output type profile */
	                            /* and ARGYLL_CREATE_OUTPUT_PROFILE_WITH_CHAD set and */
								/* ill_wp != NULL, create a 'chad' matrix. */
	void         (*chromAdaptMatrix)(struct _icc *p, int flags, double imat[3][3],
	                                 double mat[3][3], icmXYZNumber d_wp, icmXYZNumber s_wp);
	                            /* Return an overall Chromatic Adaptation Matrix */
	                            /* for the given source and destination white points. */
	                            /* This will depened on the icc profiles current setup */
	                            /* for Abs->Rel conversion (wpchtmx[][] set to wrong Von */
	                            /* Kries or not, whether 'arts' tag has been read), and */
	                            /* whether an Output profile 'chad' tag has bean read */
	                            /* or will be created. (i.e. on writing assumes */
	                            /* icc->set_illum() called or not). */
	                            /* Set header->deviceClass before calling this! */
	                            /* ICM_CAM_NONE or ICM_CAM_MULMATRIX to mult by mat. */
	                            /* Return invers of matrix if imat != NULL. */
	                            /* Use icmMulBy3x3(dst, mat, src). */
	                            /* NOTE that to transform primaries they */
	                            /* must be mat[XYZ][RGB] format! */

	/* Get a particular color conversion function */
	icmLuBase *  (*get_luobj) (struct _icc *p,
                               icmLookupFunc func,			/* Functionality */
	                           icRenderingIntent intent,	/* Intent */
	                           icColorSpaceSignature pcsor,	/* PCS override (0 = def) */
	                           icmLookupOrder order);		/* Search Order */
	                           /* Return appropriate lookup object */
	                           /* NULL on error, check errc+err for reason */

	/* Low level - load specific cLUT conversion */
	icmLuBase *  (*new_clutluobj)(struct _icc *p,
	                            icTagSignature        ttag,		  /* Target Lut tag */
                                icColorSpaceSignature inSpace,	  /* Native Input color space */
                                icColorSpaceSignature outSpace,	  /* Native Output color space */
                                icColorSpaceSignature pcs,		  /* Native PCS (from header) */
                                icColorSpaceSignature e_inSpace,  /* Override Inpt color space */
                                icColorSpaceSignature e_outSpace, /* Override Outpt color space */
                                icColorSpaceSignature e_pcs,	  /* Override PCS */
	                            icRenderingIntent     intent,	  /* Rendering intent (For abs.) */
	                            icmLookupFunc         func);	  /* For icmLuSpaces() */
	                           /* Return appropriate lookup object */
	                           /* NULL on error, check errc+err for reason */
	
	/* - - - - - - - - tweaks - - - - - - - */

	int              allowclutPoints256; /* Non standard - allow 256 res cLUT */

	int              useLinWpchtmx;		/* Force Wrong Von Kries for output class (default false) */
										/* Could be set by code, and is set set by */
										/* ARGYLL_CREATE_WRONG_VON_KRIES_OUTPUT_CLASS_REL_WP env. */
    icProfileClassSignature wpchtmx_class;	/* Class of profile wpchtmx was set for. */
										/* Set wpchtmx automatically on ->chromAdaptMatrix() */
										/* or ->get_size() or ->write() if the profile class */
										/* doesn't match wpchtmx_class. */
	double           wpchtmx[3][3];		/* Absolute to Media Relative Transformation Space (i.e. */
										/* Cone Space transformation) matrix. */
	double           iwpchtmx[3][3];	/* Inverse of wpchtmx[][] */
										/* (Default is Bradford matrix) */
	int				 useArts;			/* Save ArgyllCMS private 'arts' tag (default yes). */
										/* This creates 'arts' tag on writing. */
										/* This is cleared if 'arts' tag is not found on reading. */

	double           illwp[3];			/* Output type profile illuminant white point, */
										/* used to create 'chad' tag if */
										/* ARGYLL_CREATE_OUTPUT_PROFILE_WITH_CHAD is set. */
	int              illwpValid;		/* illwp[] has been set. */

	int              naturalChad;		/* nz if 'chad' tag is naturally present in the profile, */
										/* because it was read or added. wrD/OChad will be */
										/* ignored if this is the case. */

	int              chadmxValid;		/* nz if 'chad' tag has been read, or created */
										/* using ->set_illum() and chadmx is valid. */
	double			 chadmx[3][3];		/* 'chad' tag matrix if read or created. */
										/* (This is used to restore full absolute intent for */
	                                    /* Display and Output type profiles.) */

	int				 wrDChad;			/* Create 'chad' tag for Display profiles (default no). */
										/* Override with ARGYLL_CREATE_DISPLAY_PROFILE_WITH_CHAD. */
										/* On writing Display profiles this sets the media white */
										/* point tag to D50 and creates 'chad' tag from the white */
										/* point to D50. Ignored if naturalChad. */
		
	int				 wrOChad;			/* Create 'chad' tag for Output profiles (default no). */
										/* Override with ARGYLL_CREATE_DISPLAY_PROFILE_WITH_CHAD. */
										/* On writing an Output profile, this  Creates a 'chad' */
										/* tag frp, illwp[] to D50, and sets the white point */
										/* tag to be the real value transformed by the */
										/* 'chad' matrix. Ignored if naturalChad. */

	int              tempChad;			/* nz while temporary chad tag is in place and */
										/* white point has been modified, i.e. inside */
										/* ->get_size() or ->write() calls when wrD/OChad active. */
	icmXYZNumber	 tempWP;			/* Save real wp tag value here inside ->get_size() */
										/* or ->write() calls when wrD/OChad active. */
		
  /* - - - - - - - - - -*/


	void         (*warning)(struct _icc *p, int err, const char *format, va_list vp);
									/* Installable warning callback function, */
									/* which is NULL by default. */

	int              warnc;			/* Clip warning code */
									/* Set by icmLut_set_tables(), icmSetMultiLutTables() */

  /* Private: ? */
	icmErr         e;				/* Common error reporting */
	icmAlloc      *al;				/* Heap allocator reference */
	icmFile       *fp;				/* LEGACY: File associated with object */
	icmFile       *rfp;				/* Read File associated with object reference */
	icmFile       *wfp;				/* Write File associated with object reference */
	unsigned int of;				/* Offset of the profile within the file */

	unsigned int  align;			/* ~8 Alignment for tag table and tags */
    icmHeader      *header;			/* The header */
    unsigned int   count;			/* Num tags in the profile */
    icmTagRec     *data;    		/* ~8 The tagTable and tagData */
	unsigned int  pttsize;			/* ~8 Padded tag table size */

	unsigned int  mino;				/* Minimum offset of tags */
	unsigned int  maxs;				/* Maximum size available for tagtable & tags */

	icmCompatFlags cflags;          /* Compatibility flags */
	icmTVRange     vcrange;			/* icmCFlagAllowWrVersion range */
	icmSnOp        op;				/* tag->check() icmSnRead or icmSnWrite */

	icmTagTypeVersConstrRec *tagtypetable;	/* Table of Tag Types valid version range and */
											/* constructor. Last has ttype icMaxEnumType */

	icmTagSigVersTypesRec *tagsigtable;		/* Table of Tags valid version range and possible */
											/* Tag Types.  Last has sig icMaxEnumTag */

	icmRequiredTagType *classtagtable;		/* Table of Class Signature instance requirements */
											/* and optional. Last has sig icMaxEnumClass */

	icmTagSigVersRec *transtagtable;		/* Table of transform tags subject to classtagtable */
											/* requirements. */

}; typedef struct _icc icc;


/* ========================================================== */
/* Utility structures and declarations */

/* Type of encoding for argument to icm2str() */
typedef enum {
    icmScreenEncodings,
    icmDeviceAttributes,
	icmProfileHeaderFlags,
	icmAsciiOrBinaryData,
	icmVideoCardGammaFormat,
	icmTagSignature,
	icmTypeSignature,
	icmColorSpaceSignature,
	icmProfileClassSignature,
	icmPlatformSignature,
	icmDeviceManufacturerSignature,
	icmDeviceModelSignature,
	icmCMMSignature,
	icmReferenceMediumGamutSignature,
	icmTechnologySignature,
	icmMeasurementGeometry,
	icmMeasurementFlare,
	icmRenderingIntent,
	icmSpotShape,
	icmStandardObserver,
	icmIlluminant,
	icmLanguageCode,
	icmRegionCode,
	icmDevSetMsftID,
	icmDevSetMsftMedia,
	icmDevSetMsftDither,
	icmMeasUnitsSignature,
	icmPhColEncoding,
	icmParametricCurveFunction,
	icmTransformLookupFunc,
	icmTransformLookupOrder,
	icmTransformLookupAlgorithm
} icmEnumType;

/* Return a string description of the given enumeration value */
extern ICCLIB_API const char *icm2str(icmEnumType etype, int enumval);


/* Return a string that shows the XYZ number value */
/* Returned buffer is static */
char *icmXYZNumber2str(icmXYZNumber *p);

/* Return a string that shows the XYZ number value, */
/* and the Lab D50 number in paren. */
/* Returned string is static */
char *icmXYZNumber_and_Lab2str(icmXYZNumber *p);

/* Return a string that shows the given date and time */
/* Returned buffer is static */
char *icmDateTimeNumber2str(icmDateTimeNumber *p);

/* ----------------------------------------------- */

/* Structure to hold pseudo-hilbert counter info */
struct _psh {
	int      di;	/* Dimensionality */
	unsigned int res;	/* Resolution per coordinate */
	unsigned int bits;	/* Bits per coordinate */
	unsigned int ix;	/* Current binary index */
	unsigned int tmask;	/* Total 2^n count mask */
	unsigned int count;	/* Usable count */
}; typedef struct _psh psh;


/* ============================================== */
/* MD5 checksum support */
 
/* A helper object that computes MD5 checksums */
struct _icmMD5 {
  /* Private: */
	int refcount;			/* Reference count for this object */
	icmAlloc *al;			/* Heap allocator reference */
	int fin;				/* Flag, nz if final has been called */
	ORD32 sum[4];			/* Current/final checksum */
	unsigned int tlen;		/* Total length added in bytes */
	ORD8 buf[64];			/* Partial buffer */

  /* Public: */
	void (*reset)(struct _icmMD5 *p);	/* Reset the checksum */
	void (*add)(struct _icmMD5 *p, ORD8 *buf, unsigned int len);	/* Add some bytes */
	void (*get)(struct _icmMD5 *p, ORD8 chsum[16]);		/* Finalise and get the checksum */
	struct _icmMD5 *(*reference)(struct _icmMD5 *p);	/* Take a reference */
	void (*del)(struct _icmMD5 *p);		/* We're done with the object */

}; typedef struct _icmMD5 icmMD5;

/* Create a new MD5 checksumming object, with a reset checksum value */
/* Return it or NULL if there is an error */
/* Note that this will fail if e has an error already set */
icmMD5 *new_icmMD5_a(icmErr *e, icmAlloc *al);

/* If SEPARATE_STD not defined: */
/* Note that this will fail if e has an error already set */
extern ICCLIB_API icmMD5 *new_icmMD5(icmErr *e);


/* Implementation of file access class to compute an MD5 checksum */
struct _icmFileMD5 {
	ICM_FILE_BASE

	/* Private: */
	icmErr e;			/* Error record */
	icmAlloc *al;		/* Heap allocator */
	icmMD5 *md5;		/* MD5 object */
	unsigned int of;	/* Current offset */

	/* Private: */
	size_t size;    	/* Size of the file (For read) */

}; typedef struct _icmFileMD5 icmFileMD5;

/* Create a dumy file access class with allocator */
icmFile *new_icmFileMD5_a(icmMD5 *md5, icmAlloc *al);

/* ========================================================== */
/* Public function declarations */
/* Create an empty object. Return null on error */
/* If e not NULL, return error code and message */
/* Note that this will fail if e has an error already set */
extern ICCLIB_API icc *new_icc_a(icmErr *e, icmAlloc *al);		/* With allocator class */

/* If SEPARATE_STD not defined: */
/* Note that this will fail if e has an error already set */
extern ICCLIB_API icc *new_icc(icmErr *e);				/* Default allocator */

/* - - - - - - - - - - - - - */
/* Some useful utilities: */

/* Default ICC profile file extension string */

#if defined (UNIX) || defined(__APPLE__)
#define ICC_FILE_EXT ".icc"
#define ICC_FILE_EXT_ND "icc"
#else
#define ICC_FILE_EXT ".icm"
#define ICC_FILE_EXT_ND "icm"
#endif

/* Return a string that represents a tag */
extern ICCLIB_API char *icmtag2str(int tag);

/* Return a tag created from a string */
extern ICCLIB_API unsigned int icmstr2tag(const char *str);

/* Utility to define a non-standard tag, arguments are 4 character constants */
#define icmMakeTag(A,B,C,D)	\
	(   (((ORD32)(ORD8)(A)) << 24)	\
	  | (((ORD32)(ORD8)(B)) << 16)	\
	  | (((ORD32)(ORD8)(C)) << 8)	\
	  |  ((ORD32)(ORD8)(D)) )

/* Return the number of channels for the given color space. Return 0 if unknown. */
extern ICCLIB_API unsigned int icmCSSig2nchan(icColorSpaceSignature sig);

#define CSSigType_PCS   0x0001		/* icSigXYZData or icSigLabData */
#define CSSigType_NDEV  0x0002		/* Not a device space */
#define CSSigType_DEV   0x0004		/* A device space (AKA N-component) */
#define CSSigType_NCOL  0x0008		/* An N-Color space */
#define CSSigType_EXT   0x0010		/* Extension */

/* Return a mask classifying the color space */
extern ICCLIB_API unsigned int icmCSSig2type(icColorSpaceSignature sig);

#ifdef __cplusplus
	}
#endif

#include "icc_util.h"

#endif /* ICC_H */


