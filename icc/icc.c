
/* 
 * International Color Consortium Format Library (icclib)
 * For ICC profile version 3.4
 *
 * Author:  Graeme W. Gill
 * Date:    2002/04/22
 * Version: 3.0.0
 *
 * Copyright 1997 - 2022 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 */

// ~~88 force 'chad' tag on V >= 4 and display profiles

/*
 * TTBD:
 *
 *      Need to fix all legacy icm_err() functions to have new error codes.
 *
 *		Should fix all write_number failure errors to indicate failed value.
 *		(Partially implemented - need to check all write_number functions)
 *
 *		Complete conversion of V2 TagTypes to new parsing code.
 *		Complete adding check() methods for TagTypes & improve existing ones.
 *
 *      NameColor Dump doesn't handle device space correctly - 
 *	    should use appropriate interpretation in case device is Lab etc.
 *
 *		Should add named color space lookup function support.
 *
 *      Add support for copying tags from one icc to another ?
 *
 *		Make write fail error messages be specific on which element failed.
 *
 *      Would be nice to add generic ability to add new tag type handling,
 *      so that the base library doesn't need to be modified (ie. VideoCardGamma) ?
 *
 */


/*  Make the default grid points of the Lab clut be symetrical about */
/* a/b 0.0, and also make L = 100.0 fall on a grid point. */
#define SYMETRICAL_DEFAULT_LAB_RANGE

#undef USE_LEGACY_GETNORMFUNC		/* use LEGACY getNormFunc for normfunc code */
								/* ~~~~8888 remove this code later */

// ~8 remove read_tag_any 	and rely on default icmCFlagAllowUnknown instead.

#define _ICC_C_				/* Turn on implimentation code */

#undef BREAK_ON_ICMERR		/* [Und] Break on an icmErr() */
#undef DEBUG_BOUNDARY_EXCEPTION /* [Und] Show details of boundary exception & Break */
#undef DEBUG_SETLUT			/* [Und] Show each value being set in setting lut contents */
#undef DEBUG_SETLUT_CLIP	/* [Und] Show clipped values when setting LUT */
#undef DEBUG_LULUT			/* [Und] Show each value being looked up from lut contents */
#undef DEBUG_LLULUT			/* [Und] Debug individual lookup steps (not fully implemented) */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#ifdef __sun
#include <unistd.h>
#endif
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif
#include "icc.h"


#if defined(_MSC_VER) && !defined(vsnprintf)
#define vsnprintf _vsnprintf
#define snprintf _snprintf
#endif

/* ========================================================== */
/* Default system interface object implementations */

#ifndef SEPARATE_STD
#define COMBINED_STD

#include "iccstd.c"

#undef COMBINED_STD
#endif /* SEPARATE_STD */

/* Forced byte alignment for tag table and tags */
// ~~~~8888 remove this, add & use icc->align
#define ALIGN_SIZE icAlignSize

/* =========================================================== */

#ifdef DEBUG_SETLUT
#undef DBGSL
#define DBGSL(xxx) printf xxx ;
#else
#undef DBGSL
#define DBGSL(xxx) 
#endif

#if defined(DEBUG_SETLUT) || defined(DEBUG_SETLUT_CLIP)
#undef DBGSLC
#define DBGSLC(xxx) printf xxx ;
#else
#undef DBGSLC
#define DBGSLC(xxx) 
#endif

#ifdef DEBUG_LULUT
#undef DBGLL
#define DBGLL(xxx) printf xxx ;
#else
#undef DBGLL
#define DBGLL(xxx) 
#endif

#ifdef DEBUG_LLULUT
#undef DBLLL
#define DBLLL(xxx) printf xxx ;
#else
#undef DBLLL
#define DBLLL(xxx) 
#endif

/* ========================================================== */
/* Set the error code and error message in an icmErr context. */
/* This makes sure that the error buffer can't overflow. */
/* Do nothing if e == NULL or there is aready an error present. */
/* Return err */
int icm_verr_e(icmErr *e, int err, const char *format, va_list vp) {
	if (e != NULL && e->c == ICM_ERR_OK) {
		int nn;
	
		e->c = err;
	
		nn = vsnprintf(e->m, ICM_ERRM_SIZE, format, vp);
	
#ifdef BREAK_ON_ICMERR
		fprintf(stderr,"Error - 0x%x, '%s'\n",e->c, e->m);
		DebugBreak();
#endif

		if (nn > -1 && nn < ICM_ERRM_SIZE)	/* Fitted in buffer */
			return err;
	
		/* We assume ICM_ERRM_SIZE isn't stupidly small.. */
		strcpy(e->m, "(Error message exceeded buffer size)");
	}
	return err;
}

/* Same as above using variable arguments */
/* Return err */
int icm_err_e(icmErr *e, int err, const char *format, ...) {
	if (e != NULL && e->c == ICM_ERR_OK) {
		va_list vp;
		va_start(vp, format);
		icm_verr_e(e, err, format, vp);
		va_end(vp);
	}
	return err;
}

/* Clear any error and message */
/* Ignore if e == NULL */
void icm_err_clear_e(icmErr *e) {
	if (e != NULL) {
		e->c = ICM_ERR_OK;
		e->m[0] = '\000';
	}
}

/* Version  of above that default to using icc icmErr */
int icm_verr(icc *icp, int err, const char *format, va_list vp) {
	icm_verr_e(&icp->e, err, format, vp);
	return err;
}

int icm_err(icc *icp, int err, const char *format, ...) {
	va_list vp;
	va_start(vp, format);
	icm_verr_e(&icp->e, err, format, vp);
	va_end(vp);
	return err;
}

void icm_err_clear(icc *icp) {
	icm_err_clear_e(&icp->e);
}


/* =========================================================== */
/* Overflow protected unsigned int arithmatic functions. */
/* These functions saturate rather than wrapping around. */
/* (Divide doesn't need protection) */
/* They return UINT_MAX if there was an overflow */

#ifndef SIZE_T_MAX
# define SIZE_T_MAX ((size_t)(-1))
#endif

/* a + b unsigned int with indicator */
unsigned int sati_add(int *ind, unsigned int a, unsigned int b) {
	if (b > (UINT_MAX - a)) {
		if (ind != NULL)
			*ind = 1;
		return UINT_MAX;
	}
	return a + b;
}

/* a + b unsigned int */
unsigned int sat_add(unsigned int a, unsigned int b) {
	if (b > (UINT_MAX - a))
		return UINT_MAX;
	return a + b;
}

/* a + b size_t */
static size_t ssat_add(size_t a, size_t b) {
	if (b > (SIZE_T_MAX - a))
		return SIZE_T_MAX;
	return a + b;
}

/* a - b unsigned int with indicator */
unsigned int sati_sub(int *ind, unsigned int a, unsigned int b) {
	if (a >= b)
		return a - b;
	if (ind != NULL)
		*ind = 1;
	return 0;
}

/* a - b unsigned int */
unsigned int sat_sub(unsigned int a, unsigned int b) {
	if (a >= b)
		return a - b;
	return 0;
}

/* a - b */
static size_t ssat_sub(size_t a, size_t b) {
	if (a >= b)
		return a - b;
	return 0;
}

/* a * b unsigned int with indicator */
unsigned int sati_mul(int *ind, unsigned int a, unsigned int b) {
	unsigned int c;

	if (a == 0 || b == 0) {
		return 0;
	}

	if (a < (UINT_MAX/b)) {
		c = a * b;
	} else {
		c = UINT_MAX;
		if (ind != NULL)
			*ind = 1;
	}
	return c;
}

/* a * b unsigned int */
unsigned int sat_mul(unsigned int a, unsigned int b) {
	unsigned int c;

	if (a == 0 || b == 0)
		return 0;

	if (a < (UINT_MAX/b))
		c = a * b;
	else
		c = UINT_MAX;
	return c;
}

/* a * b size_t */
static size_t ssat_mul(size_t a, size_t b) {
	size_t c;

	if (a == 0 || b == 0)
		return 0;

	if (a > (SIZE_T_MAX/b))
		return SIZE_T_MAX;
	else
		return a * b;
}

/* a ^ b unsigned int with indicator */
unsigned int sati_pow(int *ind, unsigned int a, unsigned int b) {
	unsigned int c = 1;
	for (; b > 0; b--) {
		c = sati_mul(ind, c, a);
		if (c == UINT_MAX)
			break;
	}
	return c;
}

/* a ^ b unsigned int */
unsigned int sat_pow(unsigned int a, unsigned int b) {
	unsigned int c = 1;
	for (; b > 0; b--) {
		c = sat_mul(c, a);
		if (c == UINT_MAX)
			break;
	}
	return c;
}

/* A + B + C */
#define sat_addadd(A, B, C) sat_add(A, sat_add(B, C))

/* A + B * C */
#define sat_addmul(A, B, C) sat_add(A, sat_mul(B, C))

/* A + B + C * D */
#define sat_addaddmul(A, B, C, D) sat_add(A, sat_add(B, sat_mul(C, D)))

/* A * B * C */
#define sat_mul3(A, B, C) sat_mul(A, sat_mul(B, C))


/* Alignment */
static unsigned int sat_align(unsigned int align_size, unsigned int a) {
	if (align_size == 0)
		return a;

	align_size--;
	
	if (align_size > (UINT_MAX - a))
		return UINT_MAX;
	
	return (a + align_size) & ~align_size;
}

/* These test functions detect whether an overflow would occur */

/* Return nz if add would overflow */
static int ovr_add(unsigned int a, unsigned int b) {

	if (b > (UINT_MAX - a))
		return 1;
	return 0;
}

/* Return nz if sub would overflow */
static int ovr_sub(unsigned int a, unsigned int b) {
	if (a < b)
		return 1;
	return 0;
}

/* Return nz if mult would overflow */
static int ovr_mul(unsigned int a, unsigned int b) {
	if (a > (UINT_MAX/b))
		return 1;
	return 0;
}

/* Float comparisons to certain precision */

/* u16f16 precision */
int diff_u16f16(double a, double b) {
	if (fabs(a - b) > 0.5/65536.0)
		return 1;
	return 0;
}

/* ------------------------------------------------- */
/* Memory image icmFile compatible class */
/* Buffer is assumed to have been allocated by the given allocator, */
/* and will be expanded on write. */

/* Get the size of the file */
static size_t icmFileMem_get_size(icmFile *pp) {
	icmFileMem *p = (icmFileMem *)pp;

	return p->end - p->start;
}

/* Set current position to offset. Return 0 on success, nz on failure. */
static int icmFileMem_seek(
icmFile *pp,
unsigned int offset
) {
	icmFileMem *p = (icmFileMem *)pp;
	unsigned char *np;

	np = p->start + offset;
	if (np < p->start || np >= p->end)
		return 1;
	p->cur = np;
	return 0;
}

/* Read count items of size length. Return number of items successfully read. */
static size_t icmFileMem_read(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileMem *p = (icmFileMem *)pp;
	size_t len;

	len = ssat_mul(size, count);
	if ((p->cur + len) >= p->end) {		/* Too much */
		if (size > 0)
			count = (p->end - p->cur)/size;
		else
			count = 0;
	}
	len = size * count;
	if (len > 0)
		memmove(buffer, p->cur, len);
	p->cur += len;
	return count;
}

/* Expand the memory buffer file to hold up to pointer ep */
/* Don't expand if realloc fails */
static void icmFileMem_filemem_resize(icmFileMem *p, unsigned char *ep) {
	size_t na, co, ce;
	unsigned char *nstart;
	
	/* No need to realloc */
	if (ep <= p->aend) {
		return;
	}

	co = p->cur - p->start;		/* Current offset */
	ce = p->end - p->start;     /* Current end */
	na = ep - p->start;			/* new allocated size */

	/* Round new allocation up */
	if (na <= 1024) {
		na += 1024;
	} else {
		na += 4096;
	}
	if ((nstart = p->al->realloc(p->al, p->start, na)) != NULL) {
		p->start = nstart;
		p->cur = nstart + co;
		p->end = nstart + ce;
		p->aend = nstart + na;
	}
}

/* write count items of size length. Return number of items successfully written. */
static size_t icmFileMem_write(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileMem *p = (icmFileMem *)pp;
	size_t len;

	len = ssat_mul(size, count);
	if (len > (size_t)(p->end - p->cur))  /* Try and expand buffer */
		icmFileMem_filemem_resize(p, p->start + len);

	if (len > (size_t)(p->end - p->cur)) {
		if (size > 0)
			count = (p->end - p->cur)/size;
		else
			count = 0;
	}
	len = size * count;
	if (len > 0)
		memmove(p->cur, buffer, len);
	p->cur += len;
	if (p->end < p->cur)
		p->end = p->cur;
	return count;
}

/* do a printf */
static int icmFileMem_printf(
icmFile *pp,
const char *format,
...
) {
	int rv;
	va_list args;
	icmFileMem *p = (icmFileMem *)pp;
	int len;

	va_start(args, format);

	rv = 1;
	len = 100;					/* Initial allocation for printf */
	icmFileMem_filemem_resize(p, p->cur + len);

	/* We have to use the available printf functions to resize the buffer if needed. */
	for (;rv != 0;) {
		/* vsnprintf() either returns -1 if it doesn't fit, or */
		/* returns the size-1 needed in order to fit. */
		len = vsnprintf(p->cur, (p->aend - p->cur), format, args);

		if (len > -1 && ((p->cur + len +1) <= p->aend))	/* Fitted in current allocation */
			break;

		if (len > -1)				/* vsnprintf returned needed size-1 */
			len = len+2;			/* (In case vsnprintf returned 1 less than it needs) */
		else
			len *= 2;				/* We just have to guess */

		/* Attempt to resize */
		icmFileMem_filemem_resize(p, p->cur + len);

		/* If resize failed */
		if ((p->aend - p->cur) < len) {
			rv = 0;
			break;			
		}
	}
	if (rv != 0) {
		/* Figure out where end of printf is */
		len = strlen(p->cur);	/* Length excluding nul */
		p->cur += len;
		if (p->cur > p->end)
			p->end = p->cur;
		rv = len;
	}
	va_end(args);
	return rv;
}

/* flush all write data out to secondary storage. Return nz on failure. */
static int icmFileMem_flush(
icmFile *pp
) {
	return 0;
}

/* Return the memory buffer. Error if not icmFileMem */
static int icmFileMem_get_buf(
icmFile *pp,
unsigned char **buf,
size_t *len
) {
	icmFileMem *p = (icmFileMem *)pp;
	if (buf != NULL)
		*buf = p->start;
	if (len != NULL)
		*len = p->end - p->start;
	return 0;
}

/* Take a reference to the icmFile */
static icmFile *icmFileMem_reference(
icmFile *pp
) {
	pp->refcount++;
	return pp;
}

/* we're done with the file object, return nz on failure */
static int icmFileMem_delete(
icmFile *pp
) {
	if (pp == NULL || --pp->refcount > 0)
		return 0;
	{
		icmFileMem *p = (icmFileMem *)pp;
		icmAlloc *al = p->al;
	
		if (p->del_buf)     /* Free the memory buffer */
		    al->free(al, p->start);
		al->free(al, p);	/* Free object */
		al->del(al);		/* Delete allocator if this is the last reference */
		return 0;
	}
}

/* Create a memory image file access class with allocator */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileMem_a(
icmErr *e,			/* Error return - may be NULL */
void *base,			/* Pointer to base of memory buffer */
size_t length,		/* Number of bytes in buffer */
icmAlloc *al		/* heap allocator to reference */
) {
	icmFileMem *p;

	if (e != NULL && e->c != ICM_ERR_OK)
		return NULL;		/* Pre-existing error */

	if ((p = (icmFileMem *) al->calloc(al, 1, sizeof(icmFileMem))) == NULL) {
		icm_err_e(e, ICM_ERR_MALLOC, "Allocating a memory image file object failed");
		return NULL;
	}
	p->refcount  = 1;
	p->al        = al->reference(al);
	p->get_size  = icmFileMem_get_size;
	p->seek      = icmFileMem_seek;
	p->read      = icmFileMem_read;
	p->write     = icmFileMem_write;
	p->printf    = icmFileMem_printf;
	p->flush     = icmFileMem_flush;
	p->get_buf   = icmFileMem_get_buf;
	p->reference = icmFileMem_reference;
	p->del       = icmFileMem_delete;

	p->start = (unsigned char *)base;
	p->cur = p->start;
	p->aend = p->end = p->start + length;

	return (icmFile *)p;
}

/* Create a memory image file access class and take allocator reference */
/* and delete buffer when icmFile is deleted. */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileMem_ad(icmErr *e, void *base, size_t length, icmAlloc *al) {
	icmFile *fp;

	if ((fp = new_icmFileMem_a(e, base, length, al)) != NULL) {	
		((icmFileMem *)fp)->del_buf = 1;
	}

	return fp;
}

/* ======================================================================== */

/* Return the number of channels for the given color space. Return 0 if unknown. */
unsigned int icmCSSig2nchan(icColorSpaceSignature sig) {
	switch ((int)sig) {
		case icSigXYZData:
			return 3;
		case icSigLabData:
			return 3;
		case icSigLuvData:
			return 3;
		case icSigYCbCrData:
			return 3;
		case icSigYxyData:
			return 3;
		case icSigRgbData:
			return 3;
		case icSigGrayData:
			return 1;
		case icSigHsvData:
			return 3;
		case icSigHlsData:
			return 3;
		case icSigCmykData:
			return 4;
		case icSigCmyData:
			return 3;
		case icSig2colorData:
			return 2;
		case icSig3colorData:
			return 3;
		case icSig4colorData:
			return 4;
		case icSig5colorData:
		case icSigMch5Data:
			return 5;
		case icSig6colorData:
		case icSigMch6Data:
			return 6;
		case icSig7colorData:
		case icSigMch7Data:
			return 7;
		case icSig8colorData:
		case icSigMch8Data:
			return 8;
		case icSig9colorData:
		case icSigMch9Data:
			return 9;
		case icSig10colorData:
		case icSigMchAData:
			return 10;
		case icSig11colorData:
		case icSigMchBData:
			return 11;
		case icSig12colorData:
		case icSigMchCData:
			return 12;
		case icSig13colorData:
		case icSigMchDData:
			return 13;
		case icSig14colorData:
		case icSigMchEData:
			return 14;
		case icSig15colorData:
		case icSigMchFData:
			return 15;

		/* Non-standard and Pseudo spaces */
		case icSig1colorData:
		case icSigMch1Data:
			return 1;
		case icmSigYuvData:
			return 3;
		case icmSigLData:
			return 1;
		case icmSigYData:
			return 1;
		case icmSigLptData:
			return 3;

		case icmSigPCSData:
			return 3;
		case icmSigL8Data:
			return 1;
		case icmSigLV2Data:
			return 1;
		case icmSigLV4Data:
			return 1;
		case icmSigLab8Data:
			return 3;
		case icmSigLabV2Data:
			return 3;
		case icmSigLabV4Data:
			return 3;

		default:
			break;
	}
	return 0;
}

/* Return a mask classifying the color space */
unsigned int icmCSSig2type(icColorSpaceSignature sig) {
	switch ((int)sig) {
		case icSigXYZData:
		case icSigLabData:
			return CSSigType_PCS | CSSigType_NDEV;

		case icSigLuvData:
		case icSigYxyData:
			return CSSigType_NDEV;

		case icSigYCbCrData:
		case icSigRgbData:
		case icSigGrayData:
		case icSigHsvData:
		case icSigHlsData:
		case icSigCmykData:
		case icSigCmyData:
			return CSSigType_DEV;


		case icSig2colorData:
		case icSig3colorData:
		case icSig4colorData:
		case icSig5colorData:
		case icSig6colorData:
		case icSig7colorData:
		case icSig8colorData:
		case icSig9colorData:
		case icSig10colorData:
		case icSig11colorData:
		case icSig12colorData:
		case icSig13colorData:
		case icSig14colorData:
		case icSig15colorData:
			return CSSigType_DEV | CSSigType_NCOL;

		case icSig1colorData:
		case icSigMch1Data:
    	case icSigMch2Data:
    	case icSigMch3Data:
    	case icSigMch4Data:
    	case icSigMch5Data:
    	case icSigMch6Data:
    	case icSigMch7Data:
    	case icSigMch8Data:
    	case icSigMch9Data:
    	case icSigMchAData:
    	case icSigMchBData:
    	case icSigMchCData:
    	case icSigMchDData:
    	case icSigMchEData:
    	case icSigMchFData:
			return CSSigType_DEV | CSSigType_NCOL | CSSigType_EXT;

		case icmSigYuvData:
		case icmSigLptData:
		case icmSigLData:
		case icmSigYData:
			return CSSigType_NDEV | CSSigType_EXT;

		default:
			break;
	}
	return 0;
}

/* ------------------------------------------------------- */
/* Flag dump functions */
/* Note - returned buffers are static, can only be used 5 */
/* times before buffers get reused. */

/* Screening Encodings */
static char *icmScreenEncodings2str(unsigned int flags) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];		/* String buffers */
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	if (flags & icPrtrDefaultScreensTrue) {
		sprintf(cp,"Default Screen");
	} else {
		sprintf(cp,"No Default Screen");
	}
	cp = cp + strlen(cp);
	if (flags & icLinesPerInch) {
		sprintf(cp,", Lines Per Inch");
	} else {
		sprintf(cp,", Lines Per cm");
	}
	cp = cp + strlen(cp);

	return bp;
}

/* Device attributes */
static char *icmDeviceAttributes2str(unsigned int flags) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];		/* String buffers */
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	if (flags & icTransparency) {
		sprintf(cp,"Transparency");
	} else {
		sprintf(cp,"Reflective");
	}
	cp = cp + strlen(cp);
	if (flags & icMatte) {
		sprintf(cp,", Matte");
	} else {
		sprintf(cp,", Glossy");
	}
	cp = cp + strlen(cp);
	if (flags & icNegative) {
		sprintf(cp,", Negative");
	} else {
		sprintf(cp,", Positive");
	}
	cp = cp + strlen(cp);
	if (flags & icBlackAndWhite) {
		sprintf(cp,", BlackAndWhite");
	} else {
		sprintf(cp,", Color");
	}
	cp = cp + strlen(cp);

	return bp;
}

/* Profile header flags */
static char *icmProfileHeaderFlags2str(unsigned int flags) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];		/* String buffers */
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	if (flags & icEmbeddedProfileTrue) {
		sprintf(cp,"Embedded Profile");
	} else {
		sprintf(cp,"Not Embedded Profile");
	}
	cp = cp + strlen(cp);
	if (flags & icUseWithEmbeddedDataOnly) {
		sprintf(cp,", Use with embedded data only");
	} else {
		sprintf(cp,", Use anywhere");
	}
	cp = cp + strlen(cp);

	return bp;
}


static char *icmAsciiOrBinaryData2str(unsigned int flags) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];		/* String buffers */
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	if (flags & icBinaryData) {
		sprintf(cp,"Binary");
	} else {
		sprintf(cp,"Ascii");
	}
	cp = cp + strlen(cp);

	return bp;
}

static char *icmVideoCardGammaFormatData2str(unsigned int flags) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];		/* String buffers */
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	if (flags & icVideoCardGammaFormula) {
		sprintf(cp,"Formula");
	} else {
		sprintf(cp,"Table");
	}
	cp = cp + strlen(cp);

	return bp;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Enumeration dump functions */
/* Note - returned buffers are static, can only be used once */
/* before buffers get reused if type is unknown. */

/* public tags and sizes */
static const char *icmTagSignature2str(icTagSignature sig) {
	switch (sig) {
		case icSigAToB0Tag:
			return "AToB0 (Perceptual) Multidimentional Transform";
		case icSigAToB1Tag:
			return "AToB1 (Colorimetric) Multidimentional Transform";
		case icSigAToB2Tag:
			return "AToB2 (Saturation) Multidimentional Transform";
		case icSigBlueMatrixColumnTag:		/* AKA icSigBlueColorantTag */
			return "Blue Matrix Column";	/* AKA "Blue Colorant" */
		case icSigBlueTRCTag:
			return "Blue Tone Reproduction Curve";
		case icSigBToA0Tag:
			return "BToA0 (Perceptual) Multidimentional Transform";
		case icSigBToA1Tag:
			return "BToA1 (Colorimetric) Multidimentional Transform";
		case icSigBToA2Tag:
			return "BToA2 (Saturation) Multidimentional Transform";
		case icSigBToD0Tag:
			return "BToD0 (Perceptual) Multidimentional Transform";
		case icSigBToD1Tag:
			return "BToD1 (Colorimetric) Multidimentional Transform";
		case icSigBToD2Tag:
			return "BToD2 (Saturation) Multidimentional Transform";
		case icSigBToD3Tag:
			return "BToD3 (Absolute Colorimetric) Multidimentional Transform";
		case icSigCalibrationDateTimeTag:
			return "Calibration Date & Time";
		case icSigCharTargetTag:
			return "Characterization Target";
		case icSigChromaticAdaptationTag:
			return "Chromatic Adaptation";
		case icSigChromaticityTag:
			return "Phosphor/Colorant Chromaticity";
		case icSigCicpTag:
			return "CICP's for Video Signal Type ID";
		case icSigColorantOrderTag:
			return "Laydown Order of Colorants";
		case icSigColorantTableTag:
			return "N-component Input Colorant Identification";
		case icSigColorantTableOutTag:
			return "N-component Output Colorant Identification";
		case icSigColorimetricIntentImageStateTag:
			return "Colorimetric Image State";
		case icSigCopyrightTag:
			return "Copyright";
		case icSigCrdInfoTag:
			return "CRD Info";
		case icSigDataTag:
			return "Data";
		case icSigDateTimeTag:
			return "Date & Time";
		case icSigDeviceMfgDescTag:
			return "Device Manufacturer Description";
		case icSigDeviceModelDescTag:
			return "Device Model Description";
		case icSigDeviceSettingsTag:
			return "Device Settings";
		case icSigDToB0Tag:
			return "DToB0 (Perceptual) Multidimentional Transform";
		case icSigDToB1Tag:
			return "DToB1 (Colorimetric) Multidimentional Transform";
		case icSigDToB2Tag:
			return "DToB2 (Saturation) Multidimentional Transform";
		case icSigDToB3Tag:
			return "DToB3 (Absolute Colorimetric) Multidimentional Transform";
		case icSigGamutTag:
			return "Gamut";
		case icSigGrayTRCTag:
			return "Gray Tone Reproduction Curve";
		case icSigGreenMatrixColumnTag:		/* AKA icSigGreenColorantTag */
			return "Green Matrix Column";	/* AKA "Green Colorant" */
		case icSigGreenTRCTag:
			return "Green Tone Reproduction Curve";
		case icSigLuminanceTag:
			return "Luminance";
		case icSigMeasurementTag:
			return "Measurement";
		case icSigMetadataTag:
			return "Metadata";
		case icSigMediaBlackPointTag:
			return "Media Black Point";
		case icSigMediaWhitePointTag:
			return "Media White Point";
		case icSigNamedColorTag:
			return "Named Color";
		case icSigNamedColor2Tag:
			return "Named Color 2";
		case icSigOutputResponseTag:
			return "Output Device Response";
		case icSigPerceptualRenderingIntentGamutTag:
			return "Colorimetric Rendering Intent Gamut";
		case icSigPreview0Tag:
			return "Preview0";
		case icSigPreview1Tag:
			return "Preview1";
		case icSigPreview2Tag:
			return "Preview2";
		case icSigProfileDescriptionTag:
			return "Profile Description";
		case icSigProfileSequenceDescTag:
			return "Profile Sequence";
		case icSigPs2CRD0Tag:
			return "PS Level 2 CRD Perceptual";
		case icSigPs2CRD1Tag:
			return "PS Level 2 CRD Colorimetric";
		case icSigPs2CRD2Tag:
			return "PS Level 2 CRD Saturation";
		case icSigPs2CRD3Tag:
			return "PS Level 2 CRD Absolute";
		case icSigPs2CSATag:
			return "PS Level 2 color space array";
		case icSigPs2RenderingIntentTag:
			return "PS Level 2 Rendering Intent";
		case icSigRedMatrixColumnTag:		/* AKA icSigRedColorantTag */
			return "Red Matrix Column";		/* AKA "Red Colorant" */
		case icSigRedTRCTag:
			return "Red Tone Reproduction Curve";
		case icSigSaturationRenderingIntentGamutTag:
			return "Saturation Rendering Intent Gamut";
		case icSigScreeningDescTag:
			return "Screening Description";
		case icSigScreeningTag:
			return "Screening Attributes";
		case icSigTechnologyTag:
			return "Device Technology";
		case icSigUcrBgTag:
			return "Under Color Removal & Black Generation";
		case icSigVideoCardGammaTag:
			return "Video Card Gamma Curve";
		case icSigViewingCondDescTag:
			return "Viewing Condition Description";
		case icSigViewingConditionsTag:
			return "Viewing Condition Paramaters";

		/* ArgyllCMS private tag: */
		case icmSigAbsToRelTransSpace:
			return "Absolute to Media Relative Transformation Space Matrix";

		default: {
			static char bufs[5][50];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized - %s",icmtag2str(sig));
			return buf;
		}
	}
}

/* tag type signatures */
static const char *icmTypeSignature2str(icTagTypeSignature sig) {
	switch (sig) {
		case icSigChromaticityType:
			return "Phosphor/Colorant Chromaticity";
		case icSigCrdInfoType:
			return "CRD Info";
		case icSigCurveType:
			return "Curve";
		case icSigDataType:
			return "Data";
		case icSigDateTimeType:
			return "DateTime";
		case icSigDeviceSettingsType:
			return "Device Settings";
		case icSigLut16Type:
			return "Lut16";
		case icSigLut8Type:
			return "Lut8";
		case icSigMeasurementType:
			return "Measurement";
		case icSigNamedColorType:
			return "Named Color 1";
		case icSigNamedColor2Type:
			return "Named Color 2";
		case icSigProfileSequenceDescType:
			return "Profile Sequence Description";
		case icSigResponseCurveSet16Type:
			return "Device Response Curve";
		case icSigS15Fixed16ArrayType:
			return "S15Fixed16 Array";
		case icSigScreeningType:
			return "Screening";
		case icSigSignatureType:
			return "Signature";
		case icSigTextType:
			return "Text";
		case icSigTextDescriptionType:
			return "Text Description";
		case icSigU16Fixed16ArrayType:
			return "U16Fixed16 Array";
		case icSigUcrBgType:
			return "Under Color Removal & Black Generation";
		case icSigUInt16ArrayType:
			return "UInt16 Array";
		case icSigUInt32ArrayType:
			return "UInt32 Array";
		case icSigUInt64ArrayType:
			return "UInt64 Array";
		case icSigUInt8ArrayType:
			return "UInt8 Array";
		case icSigVideoCardGammaType:
			return "Video Card Gamma";
		case icSigViewingConditionsType:
			return "Viewing Conditions";
		case icSigXYZType:
			return "XYZ (Array?)";


		default: {
			static char bufs[5][50];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized - %s",icmtag2str(sig));
			return buf;
		}
	}
}

/* Color Space Signatures */
static const char *icmColorSpaceSignature2str(icColorSpaceSignature sig) {
	switch (sig) {
		case icSigXYZData:
			return "XYZ";
		case icSigLabData:
			return "Lab";
		case icSigLuvData:
			return "Luv";
		case icSigYCbCrData:
			return "YCbCr";
		case icSigYxyData:
			return "Yxy";
		case icSigRgbData:
			return "RGB";
		case icSigGrayData:
			return "Gray";
		case icSigHsvData:
			return "HSV";
		case icSigHlsData:
			return "HLS";
		case icSigCmykData:
			return "CMYK";
		case icSigCmyData:
			return "CMY";
		case icSig2colorData:
			return "2 Color";
		case icSig3colorData:
			return "3 Color";
		case icSig4colorData:
			return "4 Color";
		case icSig5colorData:
		case icSigMch5Data:
			return "5 Color";
		case icSig6colorData:
		case icSigMch6Data:
			return "6 Color";
		case icSig7colorData:
		case icSigMch7Data:
			return "7 Color";
		case icSig8colorData:
		case icSigMch8Data:
			return "8 Color";
		case icSig9colorData:
			return "9 Color";
		case icSig10colorData:
			return "10 Color";
		case icSig11colorData:
			return "11 Color";
		case icSig12colorData:
			return "12 Color";
		case icSig13colorData:
			return "13 Color";
		case icSig14colorData:
			return "14 Color";
		case icSig15colorData:
			return "15 Color";

		/* Non-standard and Pseudo spaces */
		case icmSigYuvData:
			return "Yu'v'";
		case icmSigYData:
			return "Y";
		case icmSigLData:
			return "L";
		case icmSigL8Data:
			return "L";
		case icmSigLptData:
			return "Lpt";

		case icmSigLV2Data:
			return "L";
		case icmSigLV4Data:
			return "L";
		case icmSigPCSData:
			return "PCS";
		case icmSigLab8Data:
			return "Lab";
		case icmSigLabV2Data:
			return "Lab";
		case icmSigLabV4Data:
			return "Lab";

		default: {
			static char bufs[5][50];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized - %s",icmtag2str(sig));
			return buf;
		}
	}
}

/* profileClass enumerations */
static const char *icmProfileClassSignature2str(icProfileClassSignature sig) {
	switch (sig) {
		case icSigInputClass:
			return "Input";
		case icSigDisplayClass:
			return "Display";
		case icSigOutputClass:
			return "Output";
		case icSigLinkClass:
			return "Link";
		case icSigAbstractClass:
			return "Abstract";
		case icSigColorSpaceClass:
			return "Color Space";
		case icSigNamedColorClass:
			return "Named Color";
		default: {
			static char bufs[5][50];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized - %s",icmtag2str(sig));
			return buf;
		}
	}
}

/* Platform Signatures */
static const char *icmPlatformSignature2str(icPlatformSignature sig) {
	static char buf[50];
	switch ((int)sig) {
		case icSigMacintosh:
			return "Macintosh";
		case icSigMicrosoft:
			return "Microsoft";
		case icSigSolaris:
			return "Solaris";
		case icSigSGI:
			return "SGI";
		case icSigTaligent:
			return "Taligent";
		case icmSig_nix:
			return "*nix";
		default:
			sprintf(buf,"Unrecognized - %s",icmtag2str(sig));
			return buf;
	}
}

/* Device Manufacturer Signatures */
static const char *icmDeviceManufacturerSignature2str(icDeviceManufacturerSignature sig) {
	return icmtag2str(sig);
}

/* Device Model Signatures */
static const char *icmDeviceModelSignature2str(icDeviceModelSignature sig) {
	return icmtag2str(sig);
}

/* CMM Signatures */
static const char *icmCMMSignature2str(icCMMSignature sig) {
	switch ((int)sig) {
		case icSigAdobeCMM:
			return "Adobe CMM";
		case icSigAgfaCMM:
			return "Agfa CMM";
		case icSigAppleCMM:
			return "Apple CMM";
		case icSigColorGearCMM:
			return "ColorGear CMM";
		case icSigColorGearLiteCMM:
			return "ColorGear CMM Lite";
		case icSigColorGearCCMM:
			return "ColorGear CMM C";
		case icSigEFICMM:
			return "EFI CMM";
		case icSigFujiFilmCMM:
			return "Fujifilm CMM";
		case icSigExactScanCMM:
			return "ExactScan CMM";
		case icSigHarlequinCMM:
			return "Harlequin RIP CMM";
		case icSigArgyllCMM:
			return "ArgyllCMS CMM";
		case icSigLogoSyncCMM:
			return "LogoSync CMM";
		case icSigHeidelbergCMM:
			return "Heidelberg CMM";
		case icSigLittleCMSCMM:
			return "Little CMS CMM";
		case icSigReflccMAXCMM:
			return "RefIccMAX CMM";
		case icSigDemoIccMAXCMM:
			return "DemoIccMAX CMM";
		case icSigKodakCMM:
			return "Kodak CMM";
		case icSigKonicaMinoltaCMM:
			return "Konica Minolta CMM";
		case icSigMicrosoftCMM:
			return "Windows Color System CMM";
		case icSigMutohCMM:
			return "Mutoh CMM";
		case icSigOnyxGraphicsCMM:
			return "Onyx Graphics CMM";
		case icSigDeviceLinkCMM:
			return "DeviceLink CMM";
		case icSigSampleICCCMM:
			return "SampleICC CMM";
		case icSigToshibaCMM:
			return "Toshiba CMM";
		case icSigImagingFactoryCMM:
			return "the imaging factory CMM";
		case icSigVivoCMM:
			return "Vivo CMM";
		case icSigWareToGoCMM:
			return "Ware to Go CMM";
		case icSigZoranCMM:
			return "Zoran CMM";
		default: {
			static char buf[50];
			sprintf(buf,"Unrecognized - %s",icmtag2str(sig));
			return buf;
		}
	}
}

/* Reference Medium Gamut Signatures */
static const char *icmReferenceMediumGamutSignature2str(icReferenceMediumGamutSignature sig) {
	static char buf[50];
	switch ((int)sig) {
		case icSigPerceptualReferenceMediumGamut:
			return "Perceptual Reference Medium Gamut";
		default:
			sprintf(buf,"Unrecognized - %s",icmtag2str(sig));
			return buf;
	}
}

/* technology signature descriptions */
static const char *icmTechnologySignature2str(icTechnologySignature sig) {
	switch (sig) {
		case icSigDigitalCamera:
			return "Digital Camera";
		case icSigFilmScanner:
			return "Film Scanner";
		case icSigReflectiveScanner:
			return "Reflective Scanner";
		case icSigInkJetPrinter:
			return "InkJet Printer";
		case icSigThermalWaxPrinter:
			return "Thermal WaxPrinter";
		case icSigElectrophotographicPrinter:
			return "Electrophotographic Printer";
		case icSigElectrostaticPrinter:
			return "Electrostatic Printer";
		case icSigDyeSublimationPrinter:
			return "DyeSublimation Printer";
		case icSigPhotographicPaperPrinter:
			return "Photographic Paper Printer";
		case icSigFilmWriter:
			return "Film Writer";
		case icSigVideoMonitor:
			return "Video Monitor";
		case icSigVideoCamera:
			return "Video Camera";
		case icSigProjectionTelevision:
			return "Projection Television";
		case icSigCRTDisplay:
			return "Cathode Ray Tube Display";
		case icSigPMDisplay:
			return "Passive Matrix Display";
		case icSigAMDisplay:
			return "Active Matrix Display";
		case icSigPhotoCD:
			return "Photo CD";
		case icSigPhotoImageSetter:
			return "Photo ImageSetter";
		case icSigGravure:
			return "Gravure";
		case icSigOffsetLithography:
			return "Offset Lithography";
		case icSigSilkscreen:
			return "Silkscreen";
		case icSigFlexography:
			return "Flexography";
		default: {
			static char buf[50];
			sprintf(buf,"Unrecognized - %s",icmtag2str(sig));
			return buf;
		}
	}
}

/* Measurement Geometry, used in the measurmentType tag */
static const char *icmMeasurementGeometry2str(icMeasurementGeometry sig) {
	switch (sig) {
		case icGeometryUnknown:
			return "Unknown";
		case icGeometry045or450:
			return "0/45 or 45/0";
		case icGeometry0dord0:
			return "0/d or d/0";
		default: {
			static char buf[50];
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
		}
	}
}

/* Measurement Flare */
static const char *icmMeasurementFlare2str(icMeasurementFlare sig) {
	switch (sig) {
		case icFlare0:
			return "0% flare";
		case icFlare100:
			return "1000% flare";
		default: {
			static char buf[50];
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
		}
	}
}

/* Rendering Intents, used in the profile header */
static const char *icmRenderingIntent2str(icRenderingIntent sig) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];		/* String buffers */
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	switch(sig & 0xffff) {
		case icPerceptual:
			sprintf(cp,"Perceptual"); break;
		case icRelativeColorimetric:
    		sprintf(cp,"Relative Colorimetric"); break;
		case icSaturation:
    		sprintf(cp,"Saturation"); break;
		case icAbsoluteColorimetric:
    		sprintf(cp,"Absolute Colorimetric"); break;

		case icmAbsolutePerceptual:				/* icclib specials */
			sprintf(cp,"Absolute Perceptual"); break;
		case icmAbsoluteSaturation:				/* icclib specials */
			sprintf(cp,"Absolute Saturation"); break;
		case icmDefaultIntent:					/* icclib specials */
    		sprintf(cp,"Default Intent"); break;

		default:
			sprintf(cp,"Unrecognized - 0x%x",sig); break;
	}
	cp = cp + strlen(cp);
	sig &= ~0xffff;

	if (sig != 0) {
		sprintf(cp," + Unknown 0x%x",sig);
		cp = cp + strlen(cp);
	}
	return bp;
}

/* Different Spot Shapes currently defined, used for screeningType */
static const char *icmSpotShape2str(icSpotShape sig) {
	switch(sig) {
		case icSpotShapeUnknown:
			return "Unknown";
		case icSpotShapePrinterDefault:
			return "Printer Default";
		case icSpotShapeRound:
			return "Round";
		case icSpotShapeDiamond:
			return "Diamond";
		case icSpotShapeEllipse:
			return "Ellipse";
		case icSpotShapeLine:
			return "Line";
		case icSpotShapeSquare:
			return "Square";
		case icSpotShapeCross:
			return "Cross";
		default: {
			static char buf[50];
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
		}
	}
}

/* Standard Observer, used in the measurmentType tag */
static const char *icmStandardObserver2str(icStandardObserver sig) {
	switch(sig) {
		case icStdObsUnknown:
			return "Unknown";
		case icStdObs1931TwoDegrees:
			return "1931 Two Degrees";
		case icStdObs1964TenDegrees:
			return "1964 Ten Degrees";
		default: {
				static char buf[50];
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
		}
	}
}

/* Pre-defined illuminants, used in measurement and viewing conditions type */
static const char *icmIlluminant2str(icIlluminant sig) {
	switch(sig) {
		case icIlluminantUnknown:
			return "Unknown";
		case icIlluminantD50:
			return "D50";
		case icIlluminantD65:
			return "D65";
		case icIlluminantD93:
			return "D93";
		case icIlluminantF2:
			return "F2";
		case icIlluminantD55:
			return "D55";
		case icIlluminantA:
			return "A";
		case icIlluminantEquiPowerE:
			return "Equi-Power(E)";
		case icIlluminantF8:
			return "F8";
		default: {
			static char buf[50];
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
		}
	}
}

/* A language code */
static const char *icmLanguageCode2str(icEnumLanguageCode sig) {
	unsigned int sigv = (unsigned int)sig;
	static char buf[50];

	if ((sigv & 0xff) >= 'a' && (sigv & 0xff)  <= 'z'
	 && ((sigv >> 8) & 0xff) >= 'a' && ((sigv >> 8) & 0xff)  <= 'z'
	 && (sigv >> 16) == 0) {
		sprintf(buf,"%c%c",sigv & 0xff, (sigv >> 8) & 0xff);
	} else {
		sprintf(buf,"Unrecognized - 0x%x",sig);
	}
	return buf;
}

/* A region code */
static const char *icmRegionCode2str(icEnumRegionCode sig) {
	unsigned int sigv = (unsigned int)sig;
	static char buf[50];

	if ((sigv & 0xff) >= 'a' && (sigv & 0xff)  <= 'z'
	 && ((sigv >> 8) & 0xff) >= 'a' && ((sigv >> 8) & 0xff)  <= 'z'
	 && (sigv >> 16) == 0) {
		sprintf(buf,"%c%c",sigv & 0xff, (sigv >> 8) & 0xff);
	} else {
		sprintf(buf,"Unrecognized - 0x%x",sig);
	}
	return buf;
}

/* Microsoft platform Device Settings ID Signatures */
static const char *icmDevSetMsftID2str(icDevSetMsftIDSignature sig) {
	switch(sig) {
    	case icSigMsftResolution:
			return "Resolution";
    	case icSigMsftMedia:
			return "Media";
    	case icSigMsftHalftone:
			return "Halftone";
		default: {
			static char buf[50];
			sprintf(buf,"Unrecognized - %s",icmtag2str(sig));
			return buf;
		}
	}
}

/* Microsoft platform Media Type Encoding */
static const char *icmDevSetMsftMedia2str(icDevSetMsftMedia sig) {
	int usern;
	static char buf[50];
	/* We're allowing for 256 user settings, but this is */
	/* arbitrary, and not defined in the ICC spec. */
	if (sig > icMsftMediaUser1 && sig < (icMsftMediaUser1 + 255)) {
		usern = 1 + sig - icMsftMediaUser1;
		sig = icMsftMediaUser1;
	}
	switch(sig) {
    	case icMsftMediaStandard:
			return "Standard";
    	case icMsftMediaTrans:
			return "Transparency";
    	case icMsftMediaGloss:
			return "Glossy";
    	case icMsftMediaUser1:
			sprintf(buf,"User%d",usern);
			return buf;
		default: {
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
		}
	}
}

/* Microsoft platform Halftone Values */
static const char *icmDevSetMsftDither2str(icDevSetMsftDither sig) {
	static char buf[50];
	int usern;
	/* We're allowing for 256 user settings, but this is */
	/* arbitrary, and not defined in the ICC spec. */
	if (sig > icMsftDitherUser1 && sig < (icMsftDitherUser1 + 255)) {
		usern = 1 + sig - icMsftDitherUser1;
		sig = icMsftDitherUser1;
	}
	switch(sig) {
    	case icMsftDitherNone:
			return "None";
    	case icMsftDitherCoarse:
			return "Coarse brush";
    	case icMsftDitherFine:
			return "Fine brush";
    	case icMsftDitherLineArt:
			return "Line art";
    	case icMsftDitherErrorDiffusion:
			return "Error Diffusion";
    	case icMsftDitherReserved6:
			return "Reserved 6";
    	case icMsftDitherReserved7:
			return "Reserved 7";
    	case icMsftDitherReserved8:
			return "Reserved 8";
    	case icMsftDitherReserved9:
			return "Reserved 9";
    	case icMsftDitherGrayScale:
			return "Grayscale";
    	case icMsftDitherUser1:
			sprintf(buf,"User%d",usern);
			return buf;
		default:
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
	}
}

/* Measurement units for the icResponseCurveSet16Type */
static const char *icmMeasUnitsSignature2str(icMeasUnitsSig sig) {
	static char buf[50];
	switch(sig) {
    	case icSigStatusA:
			return "Status A";
    	case icSigStatusE:
			return "Status E";
    	case icSigStatusI:
			return "Status I";
    	case icSigStatusT:
			return "Status T";
    	case icSigStatusM:
			return "Status M";
    	case icSigDN:
			return "DIN no polarising filter";
    	case icSigDNP:
			return "DIN with polarising filter";
    	case icSigDNN:
			return "Narrow band DIN";
    	case icSigDNNP:
			return "Narrow band DIN with polarising filter";
		default:
			sprintf(buf,"Unrecognized - %s",icmtag2str(sig));
			return buf;
	}
}

/* Colorant and Phosphor Encodings used in chromaticity type */
static const char *icmPhColEncoding2str(icPhColEncoding sig) {
	static char buf[50];
	switch(sig) {
		case icPhColUnknown:
			return "Unknown";
		case icPhColITU_R_BT_709:
			return "ITU-R BT.709";
		case icPhColSMPTE_RP145_1994:
			return "SMPTE RP145-1994";
		case icPhColEBU_Tech_3213_E:
			return "EBU Tech.3213-E";
		case icPhColP22:
			return "P22";
		case icPhColP3:
			return "P3";
		case icPhColITU_R_BT2020:
			return "ITU-R BT.2020";
		default:
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
	}
}

/* Parametric curve types for icSigParametricCurveType */
static const char *icmParametricCurveFunction2str(icParametricCurveFunctionType sig) {
	static char buf[50];
	switch(sig) {
    	case icCurveFunction1:
			return "1 parameter function";
    	case icCurveFunction2:
			return "3 parameter function";
    	case icCurveFunction3:
			return "4 parameter function";
    	case icCurveFunction4:
			return "5 parameter function";
    	case icCurveFunction5:
			return "7 parameter function";
		default:
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
	}
}

/* Transform Lookup function */
static const char *icmLookupFunc2str(icmLookupFunc sig) {
	switch(sig) {
		case icmFwd:
			return "Forward";
		case icmBwd:
    		return "Backward";
		case icmGamut:
    		return "Gamut";
		case icmPreview:
    		return "Preview";
		default: {
			static char bufs[5][30];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
		}
	}
}

/* Lookup Order */
static const char *icmLookupOrder2str(icmLookupOrder sig) {
	switch(sig) {
		case icmLuOrdNorm:
			return "Normal";
		case icmLuOrdRev:
    		return "Reverse";
		default: {
			static char bufs[5][30];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
		}
	}
}

/* Return a text abreviation of a color lookup algorithm */
static const char *icmLuAlgType2str(icmLuAlgType alg) {
	switch(alg) {
    	case icmMonoFwdType:
			return "MonoFwd";
    	case icmMonoBwdType:
			return "MonoBwd";
    	case icmMatrixFwdType:
			return "MatrixFwd";
    	case icmMatrixBwdType:
			return "MatrixBwd";
    	case icmLutType:
			return "Lut";
		default: {
			static char bufs[5][30];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized - %d",alg);
			return buf;
		}
	}
}

/* Return a string description of the given enumeration value */
/* Public: */
const char *icm2str(icmEnumType etype, int enumval) {
	switch(etype) {
	    case icmScreenEncodings:
			return icmScreenEncodings2str((unsigned int) enumval);
	    case icmDeviceAttributes:
			return icmDeviceAttributes2str((unsigned int) enumval);
		case icmProfileHeaderFlags:
			return icmProfileHeaderFlags2str((unsigned int) enumval);
		case icmAsciiOrBinaryData:
			return icmAsciiOrBinaryData2str((unsigned int) enumval);
		case icmVideoCardGammaFormat:
			return icmVideoCardGammaFormatData2str((unsigned int) enumval);
		case icmTagSignature:
			return icmTagSignature2str((icTagSignature) enumval);
		case icmTypeSignature:
			return icmTypeSignature2str((icTagTypeSignature) enumval);
		case icmColorSpaceSignature:
			return icmColorSpaceSignature2str((icColorSpaceSignature) enumval);
		case icmProfileClassSignature:
			return icmProfileClassSignature2str((icProfileClassSignature) enumval);
		case icmPlatformSignature:
			return icmPlatformSignature2str((icPlatformSignature) enumval);
		case icmDeviceManufacturerSignature:
			return icmDeviceManufacturerSignature2str((icDeviceManufacturerSignature) enumval);
		case icmDeviceModelSignature:
			return icmDeviceModelSignature2str((icDeviceModelSignature) enumval);
		case icmCMMSignature:
			return icmCMMSignature2str((icCMMSignature) enumval);
		case icmReferenceMediumGamutSignature:
			return icmReferenceMediumGamutSignature2str((icReferenceMediumGamutSignature) enumval);
		case icmTechnologySignature:
			return icmTechnologySignature2str((icTechnologySignature) enumval);
		case icmMeasurementGeometry:
			return icmMeasurementGeometry2str((icMeasurementGeometry) enumval);
		case icmMeasurementFlare:
			return icmMeasurementFlare2str((icMeasurementFlare) enumval);
		case icmRenderingIntent:
			return icmRenderingIntent2str((icRenderingIntent) enumval);
		case icmSpotShape:
			return icmSpotShape2str((icSpotShape) enumval);
		case icmStandardObserver:
			return icmStandardObserver2str((icStandardObserver) enumval);
		case icmIlluminant:
			return icmIlluminant2str((icIlluminant) enumval);
		case icmLanguageCode:
			return icmLanguageCode2str((icEnumLanguageCode) enumval);
		case icmRegionCode:
			return icmRegionCode2str((icEnumRegionCode) enumval);
		case icmDevSetMsftID:
			return icmDevSetMsftID2str((icDevSetMsftIDSignature) enumval);
		case icmDevSetMsftMedia:
			return icmDevSetMsftMedia2str((icDevSetMsftMedia) enumval);
		case icmDevSetMsftDither:
			return icmDevSetMsftDither2str((icDevSetMsftDither) enumval);
		case icmMeasUnitsSignature:
			return icmMeasUnitsSignature2str((icMeasUnitsSig) enumval);
		case icmPhColEncoding:
			return icmPhColEncoding2str((icPhColEncoding) enumval);
		case icmParametricCurveFunction:
			return icmParametricCurveFunction2str((icParametricCurveFunctionType) enumval);
		case icmTransformLookupFunc:
			return icmLookupFunc2str((icmLookupFunc) enumval);
		case icmTransformLookupOrder:
			return icmLookupOrder2str((icmLookupOrder) enumval);
		case icmTransformLookupAlgorithm:
			return icmLuAlgType2str((icmLuAlgType) enumval);
		default: {
			static char bufs[5][100];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"icm2str got unknown type, value 0x%x", enumval);
			return buf;
		}
	}
}


/* Return a string that shows the XYZ number value */
/* Returned buffer is static */
char *icmXYZNumber2str(icmXYZNumber *p) {
	static char buf[80];

	sprintf(buf,"%f, %f, %f", p->XYZ[0], p->XYZ[1], p->XYZ[2]);
	return buf;
}

/* Return a string that shows the XYZ number value, */
/* and the Lab D50 number in paren. */
/* Returned string is static */
char *icmXYZNumber_and_Lab2str(icmXYZNumber *p) {
	static char buf[100];
	double lab[3];
	icmXYZ2Lab(&icmD50, lab, p->XYZ);
	sprintf(buf,"%f, %f, %f    [Lab %f, %f, %f]", p->XYZ[0], p->XYZ[1], p->XYZ[2],
	                                              lab[0], lab[1], lab[2]);
	return buf;
}
			

/* ======================================================================== */
/* NEW: File Buffer for serialisation to work on */

/* Offset the current location by a relative amount within the buffer. */
/* Note that the resulting offset is checked for wrap around and being */
/* before the start or beyond the end of the buffer. */
static void icmFBuf_roff(icmFBuf *p, INR32 off) {
	ORD8 *nbp;
	if (p->icp->e.c != ICM_ERR_OK)
		return;
	nbp = p->bp + off;
	if ((off > 0 && nbp < p->bp)
	 || (off < 0 && nbp > p->bp)
	 || nbp < p->buf
	 || nbp > p->ep) {
		icm_err(p->icp, ICM_ERR_BUFFER_BOUND, "icmFBuf_roff: bounds error");
		return;
	}
	p->bp = nbp;
}

/* Set the current location to an absolute location within the buffer. */
/* Note that the resulting offset is checked  for being before the start */
/* or beyond the end of the buffer. */
static void icmFBuf_aoff(icmFBuf *p, ORD32 off) {
	ORD8 *nbp;
	if (p->icp->e.c != ICM_ERR_OK)
		return;
	nbp = p->buf + off;

	if (nbp < p->buf
	 || nbp > p->ep) {
		icm_err(p->icp, ICM_ERR_BUFFER_BOUND, "icmFBuf_aoff: bounds error");
		return;
	}
	p->bp = nbp;
}

/* Return the current absolute offset within the buffer */
/* Returns 0 on an error */
static ORD32 icmFBuf_get_off(icmFBuf *p) {
	if (p->icp->e.c != ICM_ERR_OK)
		return 0;
	if (p->bp < p->buf
	 || p->bp > p->ep) {
		icm_err(p->icp, ICM_ERR_BUFFER_BOUND, "icmFBuf_get_off: bounds error");
		return 0;
	}
	return p->bp - p->buf;
}

/* Return the current available space within the buffer */
/* Returns 0 on an error */
static ORD32 icmFBuf_get_space(icmFBuf *p) {
	if (p->icp->e.c != ICM_ERR_OK)
		return 0;
	if (p->bp < p->buf
	 || p->bp > p->ep) {
		icm_err(p->icp, ICM_ERR_BUFFER_BOUND, "icmFBuf_get_space: bounds error");
		return 0;
	}
	return p->ep - p->bp;
}

/* Finalise (ie. Write), free object and buffer, return size used */
static ORD32 icmFBuf_done(icmFBuf *p) {
	ORD32 rv = 0;

	if (p->icp->e.c == ICM_ERR_OK) {

		/* Write the buffer to the file */
		if (p->op == icmSnWrite) {
			ORD32 size = p->ep - p->buf;
			if (p->fp->seek(p->fp, p->of) != 0) {
				icm_err(p->icp, ICM_ERR_FILE_SEEK, "done_icmFBuf: seek to %u failed",p->of);
				p->icp->al->free(p->icp->al, p->buf);
				p->icp->al->free(p->icp->al, p);
				return rv;
			}
	       	if (p->fp->write(p->fp, p->buf, 1, size) != size) {
				icm_err(p->icp, ICM_ERR_FILE_WRITE,
				                       "done_icmFBuf: write at %u size %u failed", p->of,size);
				p->icp->al->free(p->icp->al, p->buf);
				p->icp->al->free(p->icp->al, p);
				return rv;
			}
		}
		/* We return the size arrived at by serialisation */
		if (p->bp < p->buf
		 || p->bp > p->ep) {
			icm_err(p->icp, ICM_ERR_BUFFER_BOUND, "done_icmFBuf: pointer wrapped around");
			rv = 0;
		} else
			rv = p->bp - p->buf;
	}

	p->icp->al->free(p->icp->al, p->buf);
	p->icp->al->free(p->icp->al, p);

	return rv;
}

/* Create a new file buffer. */
/* If op & icmSnDumyBuf, then f, of and size are ignored. */
/* of is the offset of the buffer in the file */
/* Sets icmFBuf and icc op */
/* Returns NULL on error & set icp->e  */
icmFBuf *new_icmFBuf(icc *icp, icmSnOp op, icmFile *fp, unsigned int of, ORD32 size) {
	icmFBuf *p;

	if (icp->e.c != ICM_ERR_OK)
		return NULL;		/* Pre-exitsting error */

	if ((p = (icmFBuf *) icp->al->calloc(icp->al, 1, sizeof(icmFBuf))) == NULL) {
		icm_err(icp, ICM_ERR_MALLOC, "new_icmFBuf: malloc failed");
		return NULL;
	}
	p->icp       = icp;				/* icc profile */
	p->op        = op;				/* Current operation */
	p->roff      = icmFBuf_roff;
	p->aoff      = icmFBuf_aoff;
	p->get_off   = icmFBuf_get_off;
	p->get_space = icmFBuf_get_space;
	p->done      = icmFBuf_done;

	if (p->op & icmSnDumyBuf) {		/* We're just Sizing, allocating or freeing */
		p->fp = NULL;
		p->of = 0;
		p->size = 0;
		p->buf = p->bp = (ORD8 *)0;	/* Allow size to compute using pointer differences */
		p->ep = ((ORD8 *)0)-1;		/* Maximum pointer value */

	} else {						/* We're reading or writing */
		p->fp = fp;
		p->of = of;
		p->size = size;

		/* Allocate the buffer. Zero it so padding is zero. */
		if ((p->buf = (ORD8 *) icp->al->calloc(icp->al, size, sizeof(ORD8))) == NULL) {
			icm_err(icp, ICM_ERR_MALLOC, "new_icmFBuf: malloc failed");
			icp->al->free(icp->al, p);
			return NULL;
		}
		p->bp = p->buf;
		p->ep = p->buf + size;
		if (p->ep < p->buf) {	/* Some calloc's have overflow bugs.. */
			icm_err(icp, ICM_ERR_FILE_SEEK, "new_icmFBuf: calloc allocated bad buffer");
			p->icp->al->free(p->icp->al, p->buf);
			p->icp->al->free(p->icp->al, p);
			return NULL;
		}

		/* Read the segment from the file */
		if (p->op == icmSnRead) {
			if (p->fp->seek(p->fp, p->of) != 0) {
				icm_err(icp, ICM_ERR_FILE_SEEK, "new_icmFBuf: seek to %u failed",p->of);
				p->icp->al->free(p->icp->al, p->buf);
				p->icp->al->free(p->icp->al, p);
				return NULL;
			}
       		if (p->fp->read(p->fp, p->buf, 1, size) != size) {
				icm_err(icp, ICM_ERR_FILE_READ,
				                          "new_icmFBuf: read at %u size %u failed",p->of,size);
				p->icp->al->free(p->icp->al, p->buf);
				p->icp->al->free(p->icp->al, p);
				return NULL;
			}
		}
	}

	return p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Primitives */
/* All serialisation has to be done via primitives. */

/* Primitive functions are made polimorphic so that they can be */
/* involked from a single place using a table lookup. */
/* Convert between ICC style file storage types and native C types. */
/* Return the number of bytes read or written from the buffer on success, */
/* and 0 on failure (this will only happen on write if value is not representable) */

// !!!!! should add check that native types are >= ICC types they conduct values for.

/* - - - - - - - - - */
/* Unsigned numbers: */

static ORD32 icmSnImp_uc_UInt8(icmSnOp op, void *vp, ORD8 *p) {
	unsigned char v;

	if (op == icmSnRead) {
		v = (unsigned char)p[0];
		*((unsigned char *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((unsigned char *)vp);
		p[0] = (ORD8)v;
	}
	return 1;
}

static ORD32 icmSnImp_us_UInt8(icmSnOp op, void *vp, ORD8 *p) {
	unsigned short v;

	if (op == icmSnRead) {
		v = (unsigned short)p[0];
		*((unsigned short *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((unsigned short *)vp);
		if (v > 255)
			return 0;
		p[0] = (ORD8)v;
	}
	return 1;
}

static ORD32 icmSnImp_ui_UInt8(icmSnOp op, void *vp, ORD8 *p) {
	unsigned int v;

	if (op == icmSnRead) {
		v = (unsigned int)p[0];
		*((unsigned int *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((unsigned int *)vp);
		if (v > 255)
			return 0;
		p[0] = (ORD8)v;
	}
	return 1;
}

static ORD32 icmSnImp_us_UInt16(icmSnOp op, void *vp, ORD8 *p) {
	unsigned short v;

	if (op == icmSnRead) {
		v = 256 * (unsigned short)p[0]
		  +       (unsigned short)p[1];
		*((unsigned short *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((unsigned short *)vp);
		p[0] = (ORD8)(v >> 8);
		p[1] = (ORD8)(v);
	}
	return 2;
}

static ORD32 icmSnImp_ui_UInt16(icmSnOp op, void *vp, ORD8 *p) {
	unsigned int v;

	if (op == icmSnRead) {
		v = 256 * (unsigned int)p[0]
		  +       (unsigned int)p[1];
		*((unsigned int *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((unsigned int *)vp);
		if (v > 65535)
			return 0;
		p[0] = (ORD8)(v >> 8);
		p[1] = (ORD8)(v);
	}
	return 2;
}

static ORD32 icmSnImp_ui_UInt32(icmSnOp op, void *vp, ORD8 *p) {
	unsigned int v;

	if (op == icmSnRead) {
		v = 16777216 * (unsigned int)p[0]
		  +    65536 * (unsigned int)p[1]
		  +      256 * (unsigned int)p[2]
		  +            (unsigned int)p[3];
		*((unsigned int *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((unsigned int *)vp);
		p[0] = (ORD8)(v >> 24);
		p[1] = (ORD8)(v >> 16);
		p[2] = (ORD8)(v >> 8);
		p[3] = (ORD8)(v);
	}
	return 4;
}

static ORD32 icmSnImp_uii_UInt64(icmSnOp op, void *vp, ORD8 *p) {
	icmUInt64 v;

	if (op == icmSnRead) {
		v.h = 16777216 * (unsigned int)p[0]
		    +    65536 * (unsigned int)p[1]
		    +      256 * (unsigned int)p[2]
		    +            (unsigned int)p[3];
		v.l = 16777216 * (unsigned int)p[4]
		    +    65536 * (unsigned int)p[5]
		    +      256 * (unsigned int)p[6]
		    +            (unsigned int)p[7];
		*((icmUInt64 *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((icmUInt64 *)vp);
		p[0] = (ORD8)(v.h >> 24);
		p[1] = (ORD8)(v.h >> 16);
		p[2] = (ORD8)(v.h >> 8);
		p[3] = (ORD8)(v.h);
		p[4] = (ORD8)(v.l >> 24);
		p[5] = (ORD8)(v.l >> 16);
		p[6] = (ORD8)(v.l >> 8);
		p[7] = (ORD8)(v.l);
	}
	return 8;
}

static ORD32 icmSnImp_d_U8Fix8(icmSnOp op, void *vp, ORD8 *p) {
	ORD32 o32;
	double v;

	if (op == icmSnRead) {
		o32 = 256 * (ORD32)p[0]		/* Read big endian 16 bit unsigned */
	        +       (ORD32)p[1];
		v = (double)o32/256.0;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((double *)vp);
		v = floor(v * 256.0 + 0.5);
		if (v < 0.0 || v > 65535.0)
			return 0;
		o32 = (ORD32)v;
		p[0] = (ORD8)(o32 >> 8);
		p[1] = (ORD8)(o32);
	}
	return 2;
}

static ORD32 icmSnImp_d_U1Fix15(icmSnOp op, void *vp, ORD8 *p) {
	ORD32 o32;
	double v;

	if (op == icmSnRead) {
		o32 = 256 * (ORD32)p[0]		/* Read big endian 16 bit unsigned */
	        +       (ORD32)p[1];
		v = (double)o32/32768.0;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((double *)vp);
		v = floor(v * 32768.0 + 0.5);
		if (v < 0.0 || v > 65535.0)
			return 0;
		o32 = (ORD32)v;
		p[0] = (ORD8)(o32 >> 8);
		p[1] = (ORD8)(o32);
	}
	return 2;
}

static ORD32 icmSnImp_d_U16Fix16(icmSnOp op, void *vp, ORD8 *p) {
	ORD32 o32;
	double v;

	if (op == icmSnRead) {
		o32 = 16777216 * (ORD32)p[0]		/* Read big endian 32 bit unsigned */
		    +    65536 * (ORD32)p[1]
		    +      256 * (ORD32)p[2]
		    +            (ORD32)p[3];
		v = (double)o32/65536.0;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((double *)vp);
		v = floor(v * 65536.0 + 0.5);
		if (v < 0.0 || v > 4294967295.0)
			return 0;
		o32 = (ORD32)v;
		p[0] = (ORD8)((o32) >> 24);
		p[1] = (ORD8)((o32) >> 16);
		p[2] = (ORD8)((o32) >> 8);
		p[3] = (ORD8)((o32));
	}
	return 4;
}

/* - - - - - - - - - */
/* Signed numbers: */

static ORD32 icmSnImp_c_SInt8(icmSnOp op, void *vp, ORD8 *p) {
	signed char v;

	if (op == icmSnRead) {
		v = (signed char)((INR8 *)p)[0];
		*((signed char *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((signed char *)vp);
		((INR8 *)p)[0] = (INR8)v;
	}
	return 1;
}

static ORD32 icmSnImp_s_SInt8(icmSnOp op, void *vp, ORD8 *p) {
	short v;

	if (op == icmSnRead) {
		v = (short)((INR8 *)p)[0];
		*((short *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((short *)vp);
		if (v > 127)
			return 0;
		else if (v < -128)
			return 0;
		((INR8 *)p)[0] = (INR8)v;
	}
	return 1;
}

static ORD32 icmSnImp_i_SInt8(icmSnOp op, void *vp, ORD8 *p) {
	int v;

	if (op == icmSnRead) {
		v = (int)((INR8 *)p)[0];
		*((int *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((int *)vp);
		if (v > 127)
			return 0;
		else if (v < -128)
			return 0;
		((INR8 *)p)[0] = (INR8)v;
	}
	return 1;
}

static ORD32 icmSnImp_s_SInt16(icmSnOp op, void *vp, ORD8 *p) {
	short v;

	if (op == icmSnRead) {
		v = 256 * (short)((INR8 *)p)[0]
		  +       (short)p[1];
		*((short *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((short *)vp);
		((INR8 *)p)[0] = (INR8)(v >> 8);
		p[1] = (ORD8)(v);
	}
	return 2;
}

static ORD32 icmSnImp_i_SInt16(icmSnOp op, void *vp, ORD8 *p) {
	int v;

	if (op == icmSnRead) {
		v = 256 * (int)((INR8 *)p)[0]
		  +       (int)p[1];
		*((int *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((int *)vp);
		if (v > 32767)
			return 0;
		else if (v < -32768)
			return 0;
		((INR8 *)p)[0] = (INR8)(v >> 8);
		p[1] = (ORD8)(v);
	}
	return 2;
}

static ORD32 icmSnImp_i_SInt32(icmSnOp op, void *vp, ORD8 *p) {
	int v;

	if (op == icmSnRead) {
		v = 16777216 * (int)((INR8 *)p)[0]
		  +    65536 * (int)p[1]
		  +      256 * (int)p[2]
		  +            (int)p[3];
		*((int *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((int *)vp);
		((INR8 *)p)[0] = (INR8)(v >> 24);
		p[1] = (ORD8)(v >> 16);
		p[2] = (ORD8)(v >> 8);
		p[3] = (ORD8)(v);
	}
	return 4;
}
	
static ORD32 icmSnImp_ii_SInt64(icmSnOp op, void *vp, ORD8 *p) {
	icmInt64 v;

	if (op == icmSnRead) {
		v.h = 16777216 * (int)((INR8 *)p)[0]
		    +    65536 * (int)p[1]
		    +      256 * (int)p[2]
		    +            (int)p[3];
		v.l = 16777216 * (unsigned int)p[4]
		    +    65536 * (unsigned int)p[5]
		    +      256 * (unsigned int)p[6]
		    +            (unsigned int)p[7];
		*((icmInt64 *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((icmInt64 *)vp);
		((INR8 *)p)[0] = (INR8)(v.h >> 24);
		p[1] = (ORD8)(v.h >> 16);
		p[2] = (ORD8)(v.h >> 8);
		p[3] = (ORD8)(v.h);
		p[4] = (ORD8)(v.l >> 24);
		p[5] = (ORD8)(v.l >> 16);
		p[6] = (ORD8)(v.l >> 8);
		p[7] = (ORD8)(v.l);
	}
	return 8;
}

static ORD32 icmSnImp_d_S7Fix8(icmSnOp op, void *vp, ORD8 *p) {
	INR32 i32;
	double v;

	if (op == icmSnRead) {
		i32 = 256 * (INR32)((INR8 *)p)[0]		/* Read big endian 16 bit signed */
		    +       (INR32)p[1];
		v = (double)i32/256.0;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((double *)vp);
		v = floor(v * 256.0 + 0.5);		/* Beware! (int)(v + 0.5) doesn't work! */
		if (v < -32768.0 || v > 32767.0)
			return 0;
		i32 = (INR32)v;
		((INR8 *)p)[0] = (INR8)((i32) >> 8);		/* Write big endian 16 bit signed */
		p[1] = (ORD8)((i32));
	}
	return 2;
}

static ORD32 icmSnImp_d_S15Fix16(icmSnOp op, void *vp, ORD8 *p) {
	INR32 i32;
	double v;

	if (op == icmSnRead) {
		i32 = 16777216 * (INR32)((INR8 *)p)[0]		/* Read big endian 32 bit signed */
		    +    65536 * (INR32)p[1]
		    +      256 * (INR32)p[2]
		    +            (INR32)p[3];
		v = (double)i32/65536.0;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((double *)vp);
		v = floor(v * 65536.0 + 0.5);		/* Beware! (int)(v + 0.5) doesn't work! */
		if (v < -2147483648.0 || v > 2147483647.0)
			return 0;
		i32 = (INR32)v;
		((INR8 *)p)[0] = (INR8)((i32) >> 24);		/* Write big endian 32 bit signed */
		p[1] = (ORD8)((i32) >> 16);
		p[2] = (ORD8)((i32) >> 8);
		p[3] = (ORD8)((i32));
	}
	return 4;
}

/* - - - - - - - - - - - - - - - - */
/* Normalised numbers (0.0 - 1.0) */

static ORD32 icmSnImp_d_NFix8(icmSnOp op, void *vp, ORD8 *p) {
	ORD32 o32;
	double v;

	if (op == icmSnRead) {
		o32 = (ORD32)p[0];
		v = (double)o32/255.0;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((double *)vp);
		v = floor(v * 255.0 + 0.5);
		if (v < 0.0 || v > 255.0)
			return 0;
		o32 = (ORD32)v;
		p[0] = (ORD8)(o32);
	}
	return 1;
}

static ORD32 icmSnImp_d_NFix16(icmSnOp op, void *vp, ORD8 *p) {
	ORD32 o32;
	double v;

	if (op == icmSnRead) {
		o32 = 256 * (ORD32)p[0]
		    +       (ORD32)p[1];
		v = (double)o32/65535.0;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((double *)vp);
		v = floor(v * 65535.0 + 0.5);
		if (v < 0.0 || v > 65535.0)
			return 0;
		o32 = (ORD32)v;
		p[0] = (ORD8)(o32 >> 8);
		p[1] = (ORD8)(o32);
	}
	return 2;
}

static ORD32 icmSnImp_d_NFix32(icmSnOp op, void *vp, ORD8 *p) {
	ORD32 o32;
	double v;

	if (op == icmSnRead) {
		o32 = 16777216 * (ORD32)p[0]
		  +      65536 * (ORD32)p[1]
		  +        256 * (ORD32)p[2]
		  +              (ORD32)p[3];
		v = (double)o32/4294967295.0;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((unsigned int *)vp);
		v = floor(v * 4294967295.0 + 0.5);
		if (v < 0.0 || v > 4294967295.0)
			return 0;
		o32 = (ORD32)v;
		p[0] = (ORD8)(o32 >> 24);
		p[1] = (ORD8)(o32 >> 16);
		p[2] = (ORD8)(o32 >> 8);
		p[3] = (ORD8)(o32);
	}
	return 4;
}

/* PCS and other colorspace encodings are handled at a layer abover */
/* the primitives. */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Central primitive serialiasation implementation */

/* Identifier for primitive type. */
/* Must correspond to primitive table index below. */
/*
	First 1/2 letters are native machine type, i.e. 
	u  = unsigned ordinal
	c  = char
	s  = short
	i  = int
	ii = long int
	d  = double

	Second is file storage size & type abreviation.
	Smaller sizes can be stored in several possible native types
 */ 
typedef enum {
	icmSnPrim_pad        = 0,
	icmSnPrim_uc_UInt8   = 1,
	icmSnPrim_us_UInt8   = 2,
	icmSnPrim_ui_UInt8   = 3,
	icmSnPrim_us_UInt16  = 4,
	icmSnPrim_ui_UInt16  = 5,
	icmSnPrim_ui_UInt32  = 6,
	icmSnPrim_uii_UInt64 = 7,
	icmSnPrim_d_U8Fix8   = 8,
	icmSnPrim_d_U1Fix15  = 9,
	icmSnPrim_d_U16Fix16 = 10,
	icmSnPrim_c_SInt8    = 11,
	icmSnPrim_s_SInt8    = 12,
	icmSnPrim_i_SInt8    = 13,
	icmSnPrim_s_SInt16   = 14,
	icmSnPrim_i_SInt16   = 15,
	icmSnPrim_i_SInt32   = 16,
	icmSnPrim_ii_SInt64  = 17,
	icmSnPrim_d_S7Fix8   = 18,
	icmSnPrim_d_S15Fix16 = 19,
	icmSnPrim_d_NFix8    = 20,
	icmSnPrim_d_NFix16   = 21,
	icmSnPrim_d_NFix32   = 22
} icmSnPrim;

/* In same order as above: */
struct {
	int size;		/* File size */
	ORD32 (* func)(icmSnOp op, void *vp, ORD8 *p);
	char *name;
} primitive_table[] = {
	{ 0, NULL,                "Zero Padding" },
	{ 1, icmSnImp_uc_UInt8,   "Unsigned Int 8" },
	{ 1, icmSnImp_us_UInt8,   "Unsigned Int 8" },
	{ 1, icmSnImp_ui_UInt8,   "Unsigned Int 8" },
	{ 2, icmSnImp_us_UInt16,  "Unsigned Int 16" },
	{ 2, icmSnImp_ui_UInt16,  "Unsigned Int 16" },
	{ 4, icmSnImp_ui_UInt32,  "Unsigned Int 32" },
	{ 8, icmSnImp_uii_UInt64, "Unsigned Int 64" },
	{ 2, icmSnImp_d_U8Fix8,   "Unsigned Fixed 8.8" },
	{ 2, icmSnImp_d_U1Fix15,  "Unsigned Fixed 1.15" },
	{ 4, icmSnImp_d_U16Fix16, "Unsigned Fixed 16.16" },
	{ 1, icmSnImp_c_SInt8,    "Signed Int 8" },
	{ 1, icmSnImp_s_SInt8,    "Signed Int 8" },
	{ 1, icmSnImp_i_SInt8,    "Signed Int 8" },
	{ 2, icmSnImp_s_SInt16,   "Signed Int 16" },
	{ 2, icmSnImp_i_SInt16,   "Signed Int 16" },
	{ 4, icmSnImp_i_SInt32,   "Signed Int 32" },
	{ 8, icmSnImp_ii_SInt64,  "Signed Int 64" },
	{ 2, icmSnImp_d_S7Fix8,   "Signed Fixed 8.8" },
	{ 4, icmSnImp_d_S15Fix16, "Signed Fixed 16.16" },
	{ 1, icmSnImp_d_NFix8,    "Normalised 8" },
	{ 2, icmSnImp_d_NFix16,   "Normalised 16" },
	{ 4, icmSnImp_d_NFix32,   "Normalised 32" }
};


/* All primitive serialisation goes through this function, */
/* so that there is a single place to check for buffer */
/* bounding errors. */
static void icmSn_primitive(icmFBuf *b, void *vp, icmSnPrim pt, ORD32 size) {
	ORD8 *nbp;

	/* Do nothing if an error has already occurred */
	/* or we're just resizing or freeing */
	if (b->icp->e.c != ICM_ERR_OK
	 || (b->op & icmSnSerialise) == 0)
		return;

	/* Buffer pointer after this access */
	if (pt == icmSnPrim_pad)
		nbp = b->bp + size;
	else
		nbp = b->bp + primitive_table[pt].size;

	/* Check for possible access outside buffer. */
	/* We're being paranoid, because this is one place that */
	/* file format attacks will show up. */
	if (nbp < b->bp
	 || b->bp < b->buf
	 || b->bp >= b->ep
	 || nbp < b->buf
	 || nbp > b->ep) {
#ifdef DEBUG_BOUNDARY_EXCEPTION 
		printf("icmSn_primitive:\n");
		printf("  buffer pointer %p, next buffer pointer %p\n",b->bp, nbp);
		printf("  buffer start   %p, buffer end+1        %p\n",b->buf, b->ep);
		DebugBreak();
#endif /* DEBUG_BOUNDARY_EXCEPTION */
		icm_err(b->icp, ICM_ERR_BUFFER_BOUND, "icmSn_primitive: buffer boundary exception");
		return;
	}

	/* Invoke primitive serialisation */
	if (b->op != icmSnSize) {
		if (pt == icmSnPrim_pad) {
			if (b->op == icmSnWrite) {
				ORD32 i;
				unsigned int tt = 0;
				for (i = 0; i < size; i++) {
					primitive_table[icmSnPrim_ui_UInt8].func(b->op, (void *)&tt, b->bp + i);
				}
			}

		} else {
			ORD32 rv;
			rv = primitive_table[pt].func(b->op, vp, b->bp);
			if (rv != primitive_table[pt].size)
				icm_err(b->icp, ICM_ERR_ENCODING,
				    "icmSn_primitive: unable to encode value to '%s'",primitive_table[pt].name);
		}
	}
	b->bp = nbp;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Public serialisation routines, used to implement tags or compund */
/* serialisable types. These provide type checking that is otherwise */
/* ignored in the implementation above. */

void icmSn_pad(icmFBuf *b, ORD32 size) {
	icmSn_primitive(b, NULL, icmSnPrim_pad, size);
}

void icmSn_align(icmFBuf *b, ORD32 align) {
	ORD32 size = 0;
	if (align < 1)
		align = 1;
	size = (0 - b->get_off(b)) & (align-1); 
	icmSn_primitive(b, NULL, icmSnPrim_pad, size);
}

void icmSn_uc_UInt8(icmFBuf *b, unsigned char *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_uc_UInt8, 0);
}

void icmSn_us_UInt8(icmFBuf *b, unsigned short *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_us_UInt8, 0);
}

void icmSn_ui_UInt8(icmFBuf *b, unsigned int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_ui_UInt8, 0);
}

void icmSn_us_UInt16(icmFBuf *b, unsigned short *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_us_UInt16, 0);
}

void icmSn_ui_UInt16(icmFBuf *b, unsigned int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_ui_UInt16, 0);
}

void icmSn_ui_UInt32(icmFBuf *b, unsigned int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_ui_UInt32, 0);
}

void icmSn_uii_UInt64(icmFBuf *b, icmUInt64 *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_uii_UInt64, 0);
}

void icmSn_d_U8Fix8(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_U8Fix8, 0);
}

void icmSn_d_U1Fix15(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_U1Fix15, 0);
}

void icmSn_d_U16Fix16(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_U16Fix16, 0);
}

void icmSn_c_SInt8(icmFBuf *b, signed char *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_c_SInt8, 0);
}

void icmSn_s_SInt8(icmFBuf *b, short *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_s_SInt8, 0);
}

void icmSn_i_SInt8(icmFBuf *b, int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_i_SInt8, 0);
}

void icmSn_s_SInt16(icmFBuf *b, short *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_s_SInt16, 0);
}

void icmSn_i_SInt16(icmFBuf *b, int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_i_SInt16, 0);
}

void icmSn_i_SInt32(icmFBuf *b, int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_i_SInt32, 0);
}

void icmSn_ii_SInt64(icmFBuf *b, icmInt64 *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_ii_SInt64, 0);
}

void icmSn_d_S7Fix8(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_S7Fix8, 0);
}

void icmSn_d_S15Fix16(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_S15Fix16, 0);
}

void icmSn_d_NFix8(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_NFix8, 0);
}

void icmSn_d_NFix16(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_NFix16, 0);
}

void icmSn_d_NFix32(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_NFix32, 0);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Encoding check support routine. */

/* Deal with a non immediately fatal formatting error. */
/* Sets error code if Format errors are fatal. */
/* else sets the warning cflags if set to warn. */
/* Return the current error code. */
static int icmVFormatWarning(icc *icp, int sub, const char *format, va_list vp) {
	int err;

	err = (icp->op == icmSnWrite) ? ICM_ERR_WR_FORMAT : ICM_ERR_RD_FORMAT;
	err |= ICM_FMT_MASK & sub;

	if ((icp->op == icmSnRead && !(icp->cflags & icmCFlagRdFormatWarn))
	 || (icp->op == icmSnWrite && !(icp->cflags & icmCFlagWrFormatWarn))) {
		icm_verr(icp, err, format, vp);
	} else {
		icp->cflags |= icp->op == icmSnRead ? icmCFlagRdWarning : icmCFlagWrWarning;
		if (icp->warning != NULL)
			icp->warning(icp, err, format, vp);
	}
	return icp->e.c;
}

/* Same as above with varargs (used by icc_check_sig() */
/* (Caller must set icp->op to icmSnRead or icmSnWrite) */
int icmFormatWarning(icc *icp, int sub, const char *format, ...) {
	int rv;
	va_list vp;
	va_start(vp, format);
	rv = icmVFormatWarning(icp, sub, format, vp);
	va_end(vp);
	return rv;
}

/* Same as above, but with buffer as argument for use in icmFBuf routines */
static int icmFmtWarn(icmFBuf *b, int sub, const char *format, ...) {
	int rv;
	va_list vp;
	va_start(vp, format);
	b->icp->op = b->op;
	rv = icmVFormatWarning(b->icp, sub, format, vp);
	va_end(vp);
	return rv;
}

/* Deal with a version mismatch error. */ 
/* sub is sub error/warning code */
/* Sets error code if Version errors are fatal. */
/* Sets the warning cflags if set to warn. */
/* (Caller must set icp->op to icmSnRead or icmSnWrite) */
/* Return the current error code. */
static int icmVVersionWarning(icc *icp, int sub, const char *format, va_list vp) {
	int err;

	err = (icp->op == icmSnWrite) ? ICM_ERR_WR_FORMAT : ICM_ERR_RD_FORMAT;
	err |= ICM_FMT_MASK & sub;

	if ((icp->op == icmSnRead && !(icp->cflags & icmCFlagRdVersionWarn))
	 || (icp->op == icmSnWrite && !(icp->cflags & icmCFlagWrVersionWarn))) {
		icm_verr(icp, err, format, vp);

	} else {
		icp->cflags |= (icp->op == icmSnWrite) ? ICM_ERR_WR_FORMAT : ICM_ERR_RD_FORMAT;
		if (icp->warning != NULL)
			icp->warning(icp, err, format, vp);
	}
	return icp->e.c;
}

/* Same as above with varargs */
int icmVersionWarning(icc *icp, int sub, const char *format, ...) {
	int rv;
	va_list vp;
	va_start(vp, format);
	rv = icmVVersionWarning(icp, sub, format, vp);
	va_end(vp);
	return rv;
}


/* Issue a quirk or general warning. */ 
/* if noset is nz, then don't set format warning flags (i.e. this is a usage warning) */
/* (Assumes cflags icmCFlagAllowQuirks is done by called if noset is z) */
/* Return the current error code. */
static int icmCQuirkWarning(icc *icp, int sub, int noset, const char *format, va_list vp) {
	int err;

	if (!noset)
		icp->cflags |= (icp->op == icmSnWrite) ? ICM_ERR_WR_FORMAT : ICM_ERR_RD_FORMAT;
	if (icp->warning != NULL)
		icp->warning(icp, sub, format, vp);
	return icp->e.c;
}

/* Same as above with varargs */
int icmQuirkWarning(icc *icp, int sub, int noset, const char *format, ...) {
	int rv;
	va_list vp;
	va_start(vp, format);
	rv = icmCQuirkWarning(icp, sub, noset, format, vp);
	va_end(vp);
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Encoding checked serialisation functions */

/* Check screening encodings */
static int icmCheckScreening(icmFBuf *b, icScreening flags) {
	unsigned int valid;

	valid = icPrtrDefaultScreensTrue
	      | icLinesPerInch;
	if (flags & ~valid)
		icmFmtWarn(b, ICM_FMT_SCREEN, "Screen Encodings '0x%x' contains unknown flags",flags);
	return b->icp->e.c;
}

/* Screening Encoding uses 4 bytes */
void icmSn_Screening32(icmFBuf *b, icScreening *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckScreening(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, p);
	if (b->op == icmSnRead)
		icmCheckScreening(b, *p);
}


/* ICC Device Attributes encodings */
static int icmCheckDeviceAttributes(icmFBuf *b, icDeviceAttributes flags) {
	unsigned int valid;

	valid = icTransparency
	      | icMatte
	      | icNegative
	      | icBlackAndWhite;
	if (flags & ~valid)
		icmFmtWarn(b, ICM_FMT_DEVICE, "Device Attributes '0x%x' contains unknown flags",flags);
	return b->icp->e.c;
}

/* Device Attributes Encoding uses 8 bytes */
void icmSn_DeviceAttributes64(icmFBuf *b, icmUInt64 *p) {

	if (b->op == icmSnWrite) {		/* ls 32 are defined by ICC */
		if (icmCheckDeviceAttributes(b, (icDeviceAttributes)p->l) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_uii_UInt64(b, p);
	if (b->op == icmSnRead)
		icmCheckDeviceAttributes(b, p->l);
}


/* Check Profile Flags Encodings */
static int icmCheckProfileFlags(icmFBuf *b, icProfileFlags flags) {
	unsigned int valid;

	valid = icEmbeddedProfileTrue
	      | icUseWithEmbeddedDataOnly;
	if (flags & 0xffff & ~valid)
		icmFmtWarn(b, ICM_FMT_PROFFLGS, "Profile Flags Encodings '0x%x' contains unknown flags",
		                                                                        flags & 0xffff);
	return b->icp->e.c;
}

/* Profile Flags Encodings uses 4 bytes */
void icmSn_ProfileFlags32(icmFBuf *b, icProfileFlags *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckProfileFlags(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, p);
	if (b->op == icmSnRead)
		icmCheckProfileFlags(b, *p);
}


/* Check Ascii/Binary encodings */
static int icmCheckAsciiOrBinary(icmFBuf *b, icAsciiOrBinary *flags) {
	unsigned int valid;

	valid = icBinaryData;
	if (*flags & ~valid) {
		icmFmtWarn(b, ICM_FMT_ASCBIN,
		                   "Ascii or Binary data encodings '0x%x' contains unknown flags",*flags);

		/* Correct ProfileMaker problem on read */
		if (b->op == icmSnRead && *flags == 0x01000000) {
			*flags = 0x01;
		}
	}
	return b->icp->e.c;
}

/* Ascii/Binary flag encoding uses 4 bytes */
void icmSn_AsciiOrBinary32(icmFBuf *b, icAsciiOrBinary *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckAsciiOrBinary(b, p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, p);
	if (b->op == icmSnRead)
		icmCheckAsciiOrBinary(b, p);
}


/* Check VideoCardGammaFormat encodings */
static int icmCheckVideoCardGammaFormat(icmFBuf *b, icVideoCardGammaFormat flags) {
	unsigned int valid;

	valid = icVideoCardGammaFormula;
	if (flags & ~valid)
		icmFmtWarn(b, ICM_FMT_GAMMA, "Video Card Gamma Format Encodings '0x%x' contains unknown flags",flags);
	return b->icp->e.c;
}

/* VideoCardGammaFormat uses 4 bytes */
void icmSn_VideoCardGammaFormat32(icmFBuf *b, icVideoCardGammaFormat *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckVideoCardGammaFormat(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, p);
	if (b->op == icmSnRead)
		icmCheckVideoCardGammaFormat(b, *p);
}


/* We don't check tag signatures, because custom or private tags are permitted */
void icmSn_TagSig32(icmFBuf *b, icTagSignature *p) {
	icmSn_ui_UInt32(b, (unsigned int *)p);
}

/* We don't check tag types, because custom or private tags are permitted */
void icmSn_TagTypeSig32(icmFBuf *b, icTagTypeSignature *p) {
	icmSn_ui_UInt32(b, (unsigned int *)p);
}


/* Check a Color Space Signature */
static int icmCheckColorSpaceSig(icmFBuf *b, icColorSpaceSignature sig) {
	switch(sig) {
		case icSigXYZData:
		case icSigLabData:
		case icSigLuvData:
		case icSigYCbCrData:
		case icSigYxyData:
		case icSigRgbData:
		case icSigGrayData:
		case icSigHsvData:
		case icSigHlsData:
		case icSigCmykData:
		case icSigCmyData:
			return b->icp->e.c;
	}

	switch(sig) {
		case icSig2colorData:
		case icSig3colorData:
		case icSig4colorData:
		case icSig5colorData:
		case icSig6colorData:
		case icSig7colorData:
		case icSig8colorData:
		case icSig9colorData:
		case icSig10colorData:
		case icSig11colorData:
		case icSig12colorData:
		case icSig13colorData:
		case icSig14colorData:
		case icSig15colorData: {
			icmTVRange v21p = ICMTVRANGE_21_PLUS;

			if (!icmVersInRange(b->icp, &v21p)) {
				icmFmtWarn(b, ICM_FMT_COLSP,
				   "ColorSpace Signature %s is not valid for file version %s (valid %s)\n",
				   icmtag2str(sig), icmProfileVers2str(b->icp),
				   icmTVersRange2str(&v21p));
			}

			return b->icp->e.c;
		}
	}

	// ~~88 should chane this to be version flag reported in warning

	/* ICCLIB extensions */
	if (b->icp->cflags & icmCFlagAllowExtensions) {
		switch(sig) {

			case icSig1colorData:
			case icSigMch1Data:
			case icSigMch2Data:
			case icSigMch3Data:
			case icSigMch4Data:
			case icSigMch5Data:
			case icSigMch6Data:
			case icSigMch7Data:
			case icSigMch8Data:
			case icSigMch9Data:
			case icSigMchAData:
			case icSigMchBData:
			case icSigMchCData:
			case icSigMchDData:
			case icSigMchEData:
			case icSigMchFData:
			case icmSigYuvData:
			case icmSigLData:
			case icmSigYData:
			case icmSigLptData: {
				return b->icp->e.c;
			}
		}
	}

	icmFmtWarn(b, ICM_FMT_COLSP, "ColorSpace Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* Color Space signature uses 4 bytes */
void icmSn_ColorSpaceSig32(icmFBuf *b, icColorSpaceSignature *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckColorSpaceSig(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckColorSpaceSig(b, *p);
}


/* Check a Profile Class signature */
static int icmCheckProfileClassSig(icmFBuf *b, icProfileClassSignature sig) {

	switch(sig) {
		case icSigInputClass:
		case icSigDisplayClass:
		case icSigOutputClass:
		case icSigLinkClass:
		case icSigAbstractClass:
		case icSigColorSpaceClass:
		case icSigNamedColorClass:
			return b->icp->e.c;
	}
	icmFmtWarn(b, ICM_FMT_PROF, "Profile Class Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* Profile Class signature uses 4 bytes */
void icmSn_ProfileClassSig32(icmFBuf *b, icProfileClassSignature *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckProfileClassSig(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckProfileClassSig(b, *p);
}


/* Check a Platform Signature */
static int icmCheckPlatformSig(icmFBuf *b, icPlatformSignature sig) {

	switch(sig) {
		case 0x0:					// ~8 Permitted in version 2.2+
		case icSigMacintosh:
		case icSigMicrosoft:
		case icSigSolaris:
		case icSigSGI:
		case icSigTaligent:
			return b->icp->e.c;
	}

	/* ICCLIB extensions */
	if (b->icp->cflags & icmCFlagAllowExtensions) {
		switch(sig) {
			case icmSig_nix:
				return b->icp->e.c;
		}
	}

	icmFmtWarn(b, ICM_FMT_PLAT, "Platform Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* Platform signature uses 4 bytes */
void icmSn_PlatformSig32(icmFBuf *b, icPlatformSignature *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckPlatformSig(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckPlatformSig(b, *p);
}


/* Check a Device Manufacturer Signature */
static int icmCheckDeviceManufacturerSig(icmFBuf *b, icDeviceManufacturerSignature sig) {
	/* Note that 0x0 is permitted */
	/* (Omitted due to difficulty in getting a machine readable list) */
	return b->icp->e.c;
}

/* Device Manufacturer signature uses 4 bytes */
void icmSn_DeviceManufacturerSig32(icmFBuf *b, icDeviceManufacturerSignature *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckDeviceManufacturerSig(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckDeviceManufacturerSig(b, *p);
}


/* Check a Device Model Signature */
static int icmCheckDeviceModel(icmFBuf *b, icDeviceModelSignature sig) {
	/* Note that 0x0 is permitted */
	/* (Omitted due to difficulty in getting a machine readable list) */
	return b->icp->e.c;
}

/* DeviceModel signature uses 4 bytes */
void icmSn_DeviceModel(icmFBuf *b, icDeviceModelSignature *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckDeviceModel(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckDeviceModel(b, *p);
}


/* Check a CMM Signature */
static int icmCheckCMMSig(icmFBuf *b, icCMMSignature sig) {

	switch(sig) {
		case icSigAdobeCMM:
		case icSigAgfaCMM:
		case icSigAppleCMM:
		case icSigColorGearCMM:
		case icSigColorGearLiteCMM:
		case icSigColorGearCCMM:
		case icSigEFICMM:
		case icSigFujiFilmCMM:
		case icSigExactScanCMM:
		case icSigHarlequinCMM:
		case icSigArgyllCMM:
		case icSigLogoSyncCMM:
		case icSigHeidelbergCMM:
		case icSigLittleCMSCMM:
		case icSigReflccMAXCMM:
		case icSigDemoIccMAXCMM:
		case icSigKodakCMM:
		case icSigKonicaMinoltaCMM:
		case icSigMicrosoftCMM:
		case icSigMutohCMM:
		case icSigOnyxGraphicsCMM:
		case icSigDeviceLinkCMM:
		case icSigSampleICCCMM:
		case icSigToshibaCMM:
		case icSigImagingFactoryCMM:
		case icSigVivoCMM:
		case icSigWareToGoCMM:
		case icSigZoranCMM:
			return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_PLAT, "CMM Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* CMM signature uses 4 bytes */
void icmSn_CMMSig32(icmFBuf *b, icCMMSignature *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckCMMSig(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckCMMSig(b, *p);
}


/* Check a Reference Medium Gamut Signature */
static int icmCheckReferenceMediumGamutSig(icmFBuf *b, icReferenceMediumGamutSignature sig) {

	switch(sig) {
		case icSigPerceptualReferenceMediumGamut:
			return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_RMGAM, "Reference Medium Gamut Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* Reference Medium Gamut signature uses 4 bytes */
void icmSn_ReferenceMediumGamutSig32(icmFBuf *b, icReferenceMediumGamutSignature *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckReferenceMediumGamutSig(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckReferenceMediumGamutSig(b, *p);
}


/* Check a technology signature */
static int icmCheckTechnologySig(icmFBuf *b, icTechnologySignature sig) {

	switch(sig) {
		case icSigDigitalCamera:
		case icSigFilmScanner:
		case icSigReflectiveScanner:
		case icSigInkJetPrinter:
		case icSigThermalWaxPrinter:
		case icSigElectrophotographicPrinter:
		case icSigElectrostaticPrinter:
		case icSigDyeSublimationPrinter:
		case icSigPhotographicPaperPrinter:
		case icSigFilmWriter:
		case icSigVideoMonitor:
		case icSigVideoCamera:
		case icSigProjectionTelevision:
		case icSigCRTDisplay:
		case icSigPMDisplay:
		case icSigAMDisplay:
		case icSigPhotoCD:
		case icSigPhotoImageSetter:
		case icSigGravure:
		case icSigOffsetLithography:
		case icSigSilkscreen:
		case icSigFlexography:
			return b->icp->e.c;
	}
	icmFmtWarn(b, ICM_FMT_TECH, "Technology Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* Technology signature uses 4 bytes */
void icmSn_TechnologySig32(icmFBuf *b, icTechnologySignature *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckTechnologySig(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckTechnologySig(b, *p);
}


/* Check a Measurement Geometry */
static int icmCheckMeasurementGeometry(icmFBuf *b, icMeasurementGeometry sig) {

	switch(sig) {
		case icGeometryUnknown:
		case icGeometry045or450:
		case icGeometry0dord0:
			return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_MESGEOM, "Measurement Geometry 0x%x is unknown",sig);
	return b->icp->e.c;
}

/* Measurement Geometry uses 4 bytes */
void icmSn_MeasurementGeometry32(icmFBuf *b, icMeasurementGeometry *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckMeasurementGeometry(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckMeasurementGeometry(b, *p);
}


/* Check a Measurement Flare */
static int icmCheckMeasurementFlare(icmFBuf *b, icMeasurementFlare sig) {

	switch(sig) {
		case icFlare0:
		case icFlare100:
			return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_MESGEOM, "Measurement Flare 0x%x is unknown",sig);
	return b->icp->e.c;
}

/* Measurement Flare uses 4 bytes */
void icmSn_MeasurementFlare32(icmFBuf *b, icMeasurementFlare *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckMeasurementFlare(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckMeasurementFlare(b, *p);
}


/* Check a Rendering Intent */
static int icmCheckRenderingIntent(icmFBuf *b, icRenderingIntent sig) {

	switch(sig & 0xffff) {
		case icPerceptual:
		case icRelativeColorimetric:
		case icSaturation:
		case icAbsoluteColorimetric:
			return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_INTENT, "Rendering Intent 0x%x is unknown",sig & 0xffff);
	return b->icp->e.c;
}

/* Rendering Intent uses 4 bytes */
void icmSn_RenderingIntent32(icmFBuf *b, icRenderingIntent *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckRenderingIntent(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckRenderingIntent(b, *p);
}


/* Check a Spot Shape */
static int icmCheckSpotShape(icmFBuf *b, icSpotShape sig) {

	switch(sig) {
		case icSpotShapeUnknown:
		case icSpotShapePrinterDefault:
		case icSpotShapeRound:
		case icSpotShapeDiamond:
		case icSpotShapeEllipse:
		case icSpotShapeLine:
		case icSpotShapeSquare:
		case icSpotShapeCross:
			return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_SPSHAPE, "Spot Shape 0x%x is unknown",sig);
	return b->icp->e.c;
}

/*  Spot Shape uses 4 bytes */
void icmSn_SpotShape32(icmFBuf *b, icSpotShape *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckSpotShape(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckSpotShape(b, *p);
}


/* Check a Standard Observer */
static int icmCheckStandardObserver(icmFBuf *b, icStandardObserver sig) {

	switch(sig) {
		case icStdObsUnknown:
		case icStdObs1931TwoDegrees:
		case icStdObs1964TenDegrees:
			return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_STOBS, "Standard Observer 0x%x is unknown",sig);
	return b->icp->e.c;
}

/*  Standard Observer uses 4 bytes */
void icmSn_StandardObserver32(icmFBuf *b, icStandardObserver *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckStandardObserver(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckStandardObserver(b, *p);
}


/* Check a Predefined Illuminant */
static int icmCheckPredefinedIlluminant(icmFBuf *b, icIlluminant sig) {

	switch(sig) {
		case icIlluminantUnknown:
		case icIlluminantD50:
		case icIlluminantD65:
		case icIlluminantD93:
		case icIlluminantF2:
		case icIlluminantD55:
		case icIlluminantA:
		case icIlluminantEquiPowerE:
		case icIlluminantF8:
			return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_PRILL, "Predefined Illuminant 0x%x is unknown",sig);
	return b->icp->e.c;
}


/*  PredefinedIlluminant uses 4 bytes */
void icmSn_PredefinedIlluminant32(icmFBuf *b, icIlluminant *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckPredefinedIlluminant(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckPredefinedIlluminant(b, *p);
}


/* Check a Language Code */
static int icmCheckLanguageCode(icmFBuf *b, icEnumLanguageCode sig) {
	unsigned int sigv = (unsigned int)sig;

	/* We only have a selection of codes in the enumeration, */
	/* and they can be changed in the ISO registry anyway. */
	/* Do a sanity check that they are two lower case alphabetic letters */

	if ((sigv & 0xff) >= 'a' && (sigv & 0xff)  <= 'z'
	 && ((sigv >> 8) & 0xff) >= 'a' && ((sigv >> 8) & 0xff)  <= 'z'
	 && (sigv >> 16) == 0) {
		return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_LANG, "Language Code 0x%x is unknown",sig);
	return b->icp->e.c;
}

/*  Language Code uses 2 bytes */
void icmSn_LanguageCode16(icmFBuf *b, icEnumLanguageCode *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckLanguageCode(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt16(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckLanguageCode(b, *p);
}


/* Check a Region Code */
static int icmCheckRegionCode(icmFBuf *b, icEnumRegionCode sig) {
	unsigned int sigv = (unsigned int)sig;

	/* We only have a selection of codes in the enumeration, */
	/* and they can be changed in the ISO registry anyway. */
	/* Do a sanity check that they are two upper case alphabetic letters */

	if ((sigv & 0xff) >= 'A' && (sigv & 0xff)  <= 'Z'
	 && ((sigv >> 8) & 0xff) >= 'A' && ((sigv >> 8) & 0xff)  <= 'Z'
	 && (sigv >> 16) == 0) {
		return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_REGION, "Region Code 0x%x is unknown",sig);
	return b->icp->e.c;
}

/*  Region Code uses 2 bytes */
void icmSn_RegionCode16(icmFBuf *b, icEnumRegionCode *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckRegionCode(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt16(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckRegionCode(b, *p);
}


/* Check a Microsoft platform Device Settings ID Signature */
static int icmCheckDevSetMsftIDSig(icmFBuf *b, icDevSetMsftIDSignature sig) {
	unsigned int sigv = (unsigned int)sig;

	switch(sig) {
    	case icSigMsftResolution:
    	case icSigMsftMedia:
    	case icSigMsftHalftone:
			return b->icp->e.c;
	}
	icmFmtWarn(b, ICM_FMT_MSDEVID, "Microsoft platform Device Settings ID Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* Microsoft platform Device Settings ID signature uses 4 bytes */
void icmSn_DevSetMsftIDSig32(icmFBuf *b, icDevSetMsftIDSignature *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckDevSetMsftIDSig(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckDevSetMsftIDSig(b, *p);
}


/* Check a Microsoft platform Media Type Encoding */
static int icmCheckDevSetMsftMedia(icmFBuf *b, icDevSetMsftMedia sig) {

	/* We're allowing for 256 user settings, but this is */
	/* arbitrary, and not defined in the ICC spec. */
	if (sig > icMsftMediaUser1 && sig < (icMsftMediaUser1 + 255))
		sig = icMsftMediaUser1;

	switch(sig) {
    	case icMsftMediaStandard:
    	case icMsftMediaTrans:
    	case icMsftMediaGloss:
    	case icMsftMediaUser1:
			return b->icp->e.c;
	}
	icmFmtWarn(b, ICM_FMT_MSMETYP, "Microsoft platform Media Type Encoding 0x%x is unknown",sig);
	return b->icp->e.c;
}

/* Microsoft platform Media Type Encoding uses 4 bytes */
void icmSn_DevSetMsftMedia32(icmFBuf *b, icDevSetMsftMedia *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckDevSetMsftMedia(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckDevSetMsftMedia(b, *p);
}


/* Check a Microsoft platform Halftone Encoding */
static int icmCheckDevSetMsftDither(icmFBuf *b, icDevSetMsftDither sig) {

	/* We're allowing for 256 user settings, but this is */
	/* arbitrary, and not defined in the ICC spec. */
	if (sig > icMsftDitherUser1 && sig < (icMsftDitherUser1 + 255)) 
		sig = icMsftDitherUser1;
	switch(sig) {
    	case icMsftDitherNone:
    	case icMsftDitherCoarse:
    	case icMsftDitherFine:
    	case icMsftDitherLineArt:
    	case icMsftDitherErrorDiffusion:
    	case icMsftDitherReserved6:
    	case icMsftDitherReserved7:
    	case icMsftDitherReserved8:
    	case icMsftDitherReserved9:
    	case icMsftDitherGrayScale:
    	case icMsftDitherUser1:
			return b->icp->e.c;
	}
	icmFmtWarn(b, ICM_FMT_MSHALFTN, "Microsoft platform Halftone Encoding 0x%x is unknown",sig);
	return b->icp->e.c;
}

/* Microsoft platform Halftone Encoding uses 4 bytes */
void icmSn_DevSetMsftDither32(icmFBuf *b, icDevSetMsftDither *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckDevSetMsftDither(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckDevSetMsftDither(b, *p);
}


/* Check a Measurement units for the icResponseCurveSet16Type signature */
static int icmCheckMeasUnitsSig(icmFBuf *b, icMeasUnitsSig sig) {
	unsigned int sigv = (unsigned int)sig;

	switch(sig) {
    	case icSigStatusA:
    	case icSigStatusE:
    	case icSigStatusI:
    	case icSigStatusT:
    	case icSigStatusM:
    	case icSigDN:
    	case icSigDNP:
    	case icSigDNN:
    	case icSigDNNP:
			return b->icp->e.c;
	}
	icmFmtWarn(b, ICM_FMT_RESCVUNITS, "ResponseCurve Measurement units Signature %s is unknown",
	                                                                             icmtag2str(sig));
	return b->icp->e.c;
}

/* Measurement units for the icResponseCurveSet16Type signature uses 4 bytes */
void icmSn_MeasUnitsSig32(icmFBuf *b, icMeasUnitsSig *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckMeasUnitsSig(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckMeasUnitsSig(b, *p);
}


/* Check Phosphor and Colorant Encodings used in chromaticity type */
static int icmCheckPhCol(icmFBuf *b, icPhColEncoding sig) {

	switch(sig) {
		case icPhColUnknown:
		case icPhColITU_R_BT_709:
		case icPhColSMPTE_RP145_1994:
		case icPhColEBU_Tech_3213_E:
		case icPhColP22:
		case icPhColP3:
		case icPhColITU_R_BT2020:
			return b->icp->e.c;
	}
	icmFmtWarn(b, ICM_FMT_COLPHOS, "Phosphor and Colorant Encoding 0x%x is unknown",sig);
	return b->icp->e.c;
}

/* Phosphor and Colorant Encodings uses 2 bytes */
void icmSn_PhCol16(icmFBuf *b, icPhColEncoding *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckPhCol(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt16(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckPhCol(b, *p);
}


/* Check Parametric curve types for icSigParametricCurveType */
static int icmCheckParametricCurveFunctionType(icmFBuf *b, icParametricCurveFunctionType sig) {

	switch(sig) {
    	case icCurveFunction1:
    	case icCurveFunction2:
    	case icCurveFunction3:
    	case icCurveFunction4:
    	case icCurveFunction5:
			return b->icp->e.c;
	}
	icmFmtWarn(b, ICM_FMT_PARCVTYP, "Parametric curve type Encoding 0x%x is unknown",sig);
	return b->icp->e.c;
}

/* Parametric curve types for icSigParametricCurveType uses 2 bytes */
void icmSn_ParametricCurveFunctionType16(icmFBuf *b, icParametricCurveFunctionType *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckParametricCurveFunctionType(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt16(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckParametricCurveFunctionType(b, *p);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Independent Check functions */

/* Check a variable length, null terminated ASCII string, */
/* and insert a null terminator if it is missing. */
/* It is checked for having a single null, right at the end */
int icmCheckVariableNullASCII(icmFBuf *b, char *p, int length) {
	unsigned int i;

	if (length  == 0  || p[length-1] != '\000') {
		if (length > 0)
			p[length-1] = '\000';	/* Fix it */
		else
			p = "";
		icmFmtWarn(b, ICM_FMT_VZ8STRING,
		                         "String length %u: '%s' is unterminated",length,p);
	}

	if (length > 0) {
		/* Check that there are no other null's */
		if (strlen(p) != (length-1))
			icmFmtWarn(b, ICM_FMT_VZ8STRING,
		                         "String length %u: '%s' has multiple terminators", length,p);
	}
	return b->icp->e.c;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compound serialisation functions */

/* XYZ number uses 12 bytes */
void icmSn_XYZNumber12b(icmFBuf *b, icmXYZNumber *p) {
	if (b->op & icmSnSerialise) {
		icmSn_d_S15Fix16(b, &p->XYZ[0]);
		icmSn_d_S15Fix16(b, &p->XYZ[1]);
		icmSn_d_S15Fix16(b, &p->XYZ[2]);
	}
}

/* xy coordinate uses 8 bytes */
void icmSn_xyCoordinate8b(icmFBuf *b, icmxyCoordinate *p) {
	if (b->op & icmSnSerialise) {
		icmSn_d_U16Fix16(b, &p->xy[0]);
		icmSn_d_U16Fix16(b, &p->xy[1]);
	}
}

/* PositionNumber uses 8 bytes */
void icmSn_PositionNumbe8b(icmFBuf *b, icmPositionNumber *p) {
	if (b->op & icmSnSerialise) {
		icmSn_ui_UInt32(b, &p->offset);
		icmSn_ui_UInt32(b, &p->size);
	}
}

/* Response16Number uses 8 bytes */
void icmSn_Response16Number8b(icmFBuf *b, icmResponse16Number *p) {
	if (b->op & icmSnSerialise) {
		icmSn_d_NFix16(b, &p->deviceValue);
		icmSn_d_S15Fix16(b, &p->measurement);
	}
}

/* Serialise a colorspace */
static void icmCSSn_serialise(icmCSSn *p, icmFBuf *b, double *vp) {
	unsigned int i;
	double tt[MAX_CHAN];

	if (b->op & icmSnSerialise) {

		if (b->op == icmSnWrite)
			p->wrnfunc(tt, vp);

		for (i = 0; i < p->n; i++)
			p->snfuncn(b, &tt[i]);

		if (b->op == icmSnRead)
			p->rdnfunc(vp, tt);
	}
}

/* Serialise a colorspace - float version */
static void icmCSSn_serialise_f(icmCSSn *p, icmFBuf *b, float *vp) {
	unsigned int i;
	double tt[MAX_CHAN];

	if (b->op & icmSnSerialise) {
		if (b->op == icmSnWrite) {
			for (i = 0; i < p->n; i++)
				tt[i] = (double)vp[i];
			p->wrnfunc(tt, tt);
		}
	
		for (i = 0; i < p->n; i++)
			p->snfuncn(b, &tt[i]);

		if (b->op == icmSnRead) {
			p->rdnfunc(tt, tt);
			for (i = 0; i < p->n; i++)
				vp[i] = (float)tt[i];
		}
	}
}

/* Serialise a colorspace channel N */
static void icmCSSn_serialise_n(icmCSSn *p, icmFBuf *b, double *vp, unsigned int ch) {
	double tt;

	if (b->op & icmSnSerialise) {
		if (b->op == icmSnWrite)
			p->wrnfuncn(&tt, vp, ch);

		p->snfuncn(b, &tt);

		if (b->op == icmSnRead)
			p->rdnfuncn(vp, &tt, ch);
	}
}

/* Delete a colorspace serialisation object */
static void icmCSSn_delete(icmCSSn *p) {
	p->icp->al->free(p->icp->al, p);
}

/* Initailise an icmCSSn instance for a particular colorspace and encoding */
int init_icmCSSn(
	icmCSSn              *p,		/* Object to initialise */
	icc                  *icp, 		/* icc profile - for allocator, error reporting etc. */
	icColorSpaceSignature csig, 	/* Colorspace to serialise */
	icmGNVers             ver,		/* Lab version */
	icmGNRep              rep		/* Representation */
) {
	if (icp->e.c != ICM_ERR_OK)
		return icp->e.c;		/* Pre-existing error */

	p->icp    = icp;
	p->sn     = icmCSSn_serialise;
	p->snf    = icmCSSn_serialise_f;
	p->snn    = icmCSSn_serialise_n;

	if (icmGetNormFunc(&p->rdnfunc, &p->rdnfuncn, csig, icmGNtoCS, ver, rep) != 0
	 || icmGetNormFunc(&p->wrnfunc, &p->wrnfuncn, csig, icmGNtoNorm, ver, rep) != 0) {
		icm_err(icp, ICM_ERR_BADCS, "icmGetNormFunc failed or colorspace '%s'",
		                                               icmColorSpaceSignature2str(csig));
		return icp->e.c;
	}
	if ((p->n = icmCSSig2nchan(csig)) == 0) {
		icm_err(icp, ICM_ERR_BADCS, "icmCSSig2nchan failed or colorspace '%s'",
		                                               icmColorSpaceSignature2str(csig));
		return icp->e.c;
	}
	if (p->n > MAX_CHAN) {
		icm_err(icp, ICM_ERR_INTERNAL, "icmCSSig2nchan returned no channels %u > MAX_CHAN",p->n);
		return icp->e.c;
	}

	switch(rep) {
		case icmGN8bit:
			p->snfuncn = icmSn_d_NFix8;
			break;
		case icmGN16bit:
			p->snfuncn = icmSn_d_NFix16;
			break;
		default:
			icm_err(icp, ICM_ERR_INTERNAL, "new_icmCSSn got unknown rep 0x%x",rep);
			return icp->e.c;
	}
	return icp->e.c;
}


/* Create an icmCSSn instance for a PCS colorspace and encoding */
int init_icmPCSSn(
	icmCSSn              *p,		/* Object to initialise */
	icc                  *icp,		/* icc profile - for allocator, error reporting etc. */
	icColorSpaceSignature csig, 	/* Colorspace to serialise - must be XYZ or Lab! */
	icmGNVers             ver,		/* Lab version */
	icmGNRep              rep		/* Representation */
) {
	if (csig != icSigXYZData
	 && csig != icSigLabData
	 && csig != icmSigLab8Data
	 && csig != icmSigLabV2Data
	 && csig != icmSigLabV4Data) {
		icm_err(icp, ICM_ERR_INTERNAL, "new_icmPCSSn colorspace '%s' not PCS",
		                                               icmColorSpaceSignature2str(csig));
		return icp->e.c;
	}
	return init_icmCSSn(p, icp, csig, ver, rep);
}

static int icmCheckDateTimeNumber(icmFBuf *b, icmDateTimeNumber *p) {

	/* Sanity check the date and time */
	if (p->year >= 1900 && p->year <= 3000
	 && p->month != 0 && p->month <= 12
	 && p->day != 0 && p->day <= 31
	 && p->hours <= 23
	 && p->minutes <= 59
	 && p->seconds <= 59) {
		return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_DATETIME, "Bad date time '%s'", icmDateTimeNumber2str(p));

	/* Correct the date/time on read */ 
	if (b->op == icmSnRead) {

		/* Check for Adobe problem */
		if (p->month >= 1900 && p->month <= 3000
		 && p->year != 0 && p->year <= 12
		 && p->hours != 0 && p->hours <= 31
		 && p->day <= 23
		 && p->seconds <= 59
		 && p->minutes <= 59) {
			unsigned int tt; 
	
			/* Correct Adobe's faulty profile */
			tt = p->month; p->month = p->year; p->year = tt;
			tt = p->hours; p->hours = p->day; p->day = tt;
			tt = p->seconds; p->seconds = p->minutes; p->minutes = tt;
			return b->icp->e.c;
		}

		/* Hmm. some other sort of corruption. Limit values to sane */
		if (p->year < 1900) {
			if (p->year < 100)			/* Hmm. didn't use 4 digit year, guess it's 19xx ? */
				p->year += 1900;
			else
				p->year = 1900;
		} else if (p->year > 3000)
			p->year = 3000;
	
		if (p->month == 0)
			p->month = 1;
		else if (p->month > 12)
			p->month = 12;
	
		if (p->day == 0)
			p->day = 1;
		else if (p->day > 31)
			p->day = 31;
	
		if (p->hours > 23)
			p->hours = 23;
	
		if (p->minutes > 59)
			p->minutes = 59;
	
		if (p->seconds > 59)
			p->seconds = 59;
	}
	return b->icp->e.c;
}

/* DateTime uses 12 bytes */
void icmSn_DateTimeNumber12b(icmFBuf *b, icmDateTimeNumber *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckDateTimeNumber(b, p) != ICM_ERR_OK)
			return;
	}

	if (b->op & icmSnSerialise) {
		icmSn_ui_UInt16(b, &p->year);
		icmSn_ui_UInt16(b, &p->month);
		icmSn_ui_UInt16(b, &p->day);
		icmSn_ui_UInt16(b, &p->hours);
		icmSn_ui_UInt16(b, &p->minutes);
		icmSn_ui_UInt16(b, &p->seconds);
	}

	if (b->op == icmSnRead)
		icmCheckDateTimeNumber(b, p);
}

/* Check a file version number is within the known valid range */
static int icmCheckVersionNumber(icmFBuf *b, icmVers *p) {
	if (p->majv != 2 && p->majv  != 4) {
		icmFmtWarn(b, ICM_FMT_MAJV, "Major version '%d' is not recognized",p->majv);
	} else {
		if (p->majv == 2 && p->minv > 4) {
			icmFmtWarn(b, ICM_FMT_MINV, "Minor version '%d.%d' is not recognized",
			                                                            p->majv,p->minv);
		}
		/* There are no known bfv != 0, but this should be ignored */
	}
	return b->icp->e.c;
}

/* Version Number uses 4 bytes */
void icmSn_VersionNumber4b(icmFBuf *b, icmVers *p) {
	unsigned int t1, t2;

	if (b->op == icmSnWrite) {
		if (icmCheckVersionNumber(b, p) != ICM_ERR_OK)
			return;
	    t1 = ((p->majv/10) << 4) + (p->majv % 10);
		t2 = (p->minv << 4) + p->bfv;
	}
	if (b->op & icmSnSerialise) {
		icmSn_ui_UInt8(b, &t1);				/* 0: Raw major version number */
		icmSn_ui_UInt8(b, &t2);				/* 1: Raw minor/bfv version number */
		icmSn_pad(b, 2);					/* 2-3: Zero */
	}
	if (b->op == icmSnRead) {
		if ((t1 & 0xf) > 9 || ((t1 >> 4) & 0xf) > 9) {
			icm_err(b->icp, ICM_ERR_ENCODING, "Major Version BCD coding error (0x%x)",t1);
			return;
		}
		if ((t2 & 0xf) > 9 || ((t2 >> 4) & 0xf) > 9) {
			icm_err(b->icp, ICM_ERR_ENCODING, "Minor/Bugfix Version BCD coding error (0x%x)",t2);
			return;
		}
	    p->majv  = (t1 >> 4) * 10 + (t1 & 0xf);	/* Integer major version number */
	    p->minv  = (t2 >> 4);					/* Integer minor version number */
		p->bfv   = (t2 & 0xf);					/* Integer bug fix version number */
		icmCheckVersionNumber(b, p);
	}
}

/* Check a fixed length, null terminated ASCII string, */
/* and insert a null terminator if it is missing. */
static int icmCheckFixedNullASCII(icmFBuf *b, char *p, int length) {
	unsigned int i;

	for (i = 0; i < length; i++) {
		if (p[i] == '\000')
			break;
	}
	if (i >= length) {		/* No terminator */
		if (length > 0)
			p[length-1] = '\000';
		else
			p = "";
		icmFmtWarn(b, ICM_FMT_FZ8STRING, "String length %u: '%s' is unterminated",length,p);
	}
	return b->icp->e.c;
}

/* A fixed length, null terminated ASCII string */
void icmSn_FixedNullASCII(icmFBuf *b, char *p, unsigned int length) {
	unsigned int i;
	if (b->op == icmSnWrite) {
		if (icmCheckFixedNullASCII(b, p, length) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		for (i = 0; i < length; i++)
			icmSn_c_SInt8(b, (signed char *)&p[i]);
	if (b->op == icmSnRead) {
		icmCheckFixedNullASCII(b, p, length);
	}
}


/* A variable length, null terminated ASCII string */
void icmSn_VariableNullASCII(icmFBuf *b, char *p, unsigned int length) {
	unsigned int i;
	if (b->op == icmSnWrite) {
		if (icmCheckVariableNullASCII(b, p, length) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		for (i = 0; i < length; i++)
			icmSn_c_SInt8(b, (signed char *)&p[i]);
	if (b->op == icmSnRead) {
		icmCheckVariableNullASCII(b, p, length);
	}
}

/* ================================================================ */
/* Default implementation of the tag type methods             */
/* The default implementation uses the serialise method */
/* to do all the work, reducing the size of most */
/* tag type implementations. */

/* Return the number of bytes needed to write this tag */
/* Return 0 on error */
unsigned int icmGeneric_get_size(icmBase *p) {
	ICM_TAG_NEW_SIZE_BUFFER
	p->serialise(p, b);
    return b->done(b);
}

/* Read the object, return error code */
int icmGeneric_read(icmBase *p, unsigned int size, unsigned int of) {
	ICM_TAG_NEW_READ_BUFFER
	p->serialise(p, b);
    b->done(b);
	return p->icp->e.c;
}

/* Write the object. return error code */
int icmGeneric_write(icmBase *p, unsigned int size, unsigned int of) {
	ICM_TAG_NEW_WRITE_BUFFER
	p->serialise(p, b);
    b->done(b);
	return p->icp->e.c;
}

/* Free all storage in the object */
void icmGeneric_delete(icmBase *p) {
	ICM_TAG_NEW_FREE_BUFFER
	p->serialise(p, b);
    b->done(b);
	p->icp->al->free(p->icp->al, p);
}

/* Allocate variable sized data elements */
/* Return error code */
int icmGeneric_allocate(icmBase *p) {
	ICM_TAG_NEW_RESIZE_BUFFER
	p->serialise(p, b);
    b->done(b);
	return p->icp->e.c;
}

/* ---------------------------------------------------------- */
/* Implementation of a general array resizing utility function,         */
/* used within XXXX__serialise() functions.                   */

/* Return zero on sucess, nz if there is an error */
int icmArrayRdAllocResize(
	icmFBuf *b,					/* Buffer we're dealing with */
	icmAResizeCountType ct,		/* explicit count or count from buffer size ? */

	unsigned int *p_count,		/* Pointer to current count of items */
	unsigned int *pcount,		/* Pointer to new count of items */
	void **pdata,				/* Pointer to items allocation */
	unsigned int *p_msize,		/* Pointer to previous memory size of each item (may be NULL) */
	unsigned int msize,			/* Current Memory size of each item */

	unsigned int maxsize,		/* Maximum size permitted for buffer (use UINT_MAX if not needed) */
	unsigned int fixedsize,		/* Fixed size to be allowed for in buffer. */
	                            /* Buffer space allowed is min(maxsize, buf->space - fixedsize) */
	unsigned int bsize,			/* Buffer size of each item */
	char *tagdesc				/* Stringification of tag type used for warning/error reporting */
) {
	unsigned int _msize;

	if (p_msize != NULL)
		_msize = *p_msize;
	else
		_msize = msize;

	/* Check the new size for sanity */
	if (b->op == icmSnRead) {
		unsigned int tsize, tavail;

		if (b->icp->e.c != ICM_ERR_OK)
			return 1;
		tavail = b->get_space(b);

		if (ct == icmAResizeByCount) {

			if (tavail > maxsize)
				tavail = maxsize;
	
			tsize = sat_add(fixedsize, sat_mul(*pcount, bsize));
	
			if (tsize > tavail) {
				icm_err(b->icp, ICM_ERR_BUFFER_BOUND,
				                 "%s tag count/size %u is too big ",tagdesc, *pcount);
				return 1;
			}

		} else if (ct == icmAResizeBySize) {
			if (tavail < fixedsize) {
				icm_err(b->icp, ICM_ERR_BUFFER_BOUND,
				                 " %s tag size %u is too small",tagdesc, b->size);
				return 1;
			}
			*pcount = (tavail - fixedsize) / bsize;
			tsize = *pcount * bsize + fixedsize;
			if (tsize != tavail) {
				icmFmtWarn(b, ICM_FMT_PARTIALEL,
				         "%s (imp) tag has a partial array element (%u/%u bytes)",
				         tagdesc, tsize - tavail, bsize);
			}

		} else {
			icm_err(b->icp, ICM_ERR_INTERNAL,
			        "icmArrayRdAllocResize: unknown icmAResizeCountType");
			return 1;
		}
	}

	/* Resize or Read, resize the array */
	if (b->op & icmSnAlloc) {
		if (*pcount != *p_count
		 ||  msize  != _msize) {
			void *_np;
			if ((_np = (void *) b->icp->al->recalloc(b->icp->al, *pdata,
			     *p_count, _msize, *pcount, msize)) == NULL) {
				icm_err(b->icp, ICM_ERR_MALLOC,
				         "Allocating %s data size %d failed",tagdesc, *pcount);
				return 1;
			}
			*pdata = _np;
			*p_count = *pcount;
			if (p_msize != NULL)
				*p_msize = msize;
		}
	}

	return 0;
}

/* ================================================================ */
/* LEGACY: Conversion support functions */
/* Convert between (Big Endian) ICC storage types and native C types */
/* Write routine return non-zero if numbers can't be represented */

/* Unsigned */
static unsigned int read_UInt8Number(char *p) {
	unsigned int rv;
	rv = (unsigned int)((ORD8 *)p)[0];
	return rv;
}

static int write_UInt8Number(unsigned int d, char *p) {
	if (d > 255)
		return 1;
	((ORD8 *)p)[0] = (ORD8)d;
	return 0;
}

static unsigned int read_UInt16Number(char *p) {
	unsigned int rv;
	rv = 256 * (unsigned int)((ORD8 *)p)[0]
	   +       (unsigned int)((ORD8 *)p)[1];
	return rv;
}

static int write_UInt16Number(unsigned int d, char *p) {
	if (d > 65535)
		return 1;
	((ORD8 *)p)[0] = (ORD8)(d >> 8);
	((ORD8 *)p)[1] = (ORD8)(d);
	return 0;
}

static unsigned int read_UInt32Number(char *p) {
	unsigned int rv;
	rv = 16777216 * (unsigned int)((ORD8 *)p)[0]
	   +    65536 * (unsigned int)((ORD8 *)p)[1]
	   +      256 * (unsigned int)((ORD8 *)p)[2]
	   +            (unsigned int)((ORD8 *)p)[3];
	return rv;
}

static int write_UInt32Number(unsigned int d, char *p) {
	((ORD8 *)p)[0] = (ORD8)(d >> 24);
	((ORD8 *)p)[1] = (ORD8)(d >> 16);
	((ORD8 *)p)[2] = (ORD8)(d >> 8);
	((ORD8 *)p)[3] = (ORD8)(d);
	return 0;
}

static void read_UInt64Number(icmUInt64 *d, char *p) {
	d->h = 16777216 * (unsigned int)((ORD8 *)p)[0]
	     +    65536 * (unsigned int)((ORD8 *)p)[1]
	     +      256 * (unsigned int)((ORD8 *)p)[2]
	     +            (unsigned int)((ORD8 *)p)[3];
	d->l = 16777216 * (unsigned int)((ORD8 *)p)[4]
	     +    65536 * (unsigned int)((ORD8 *)p)[5]
	     +      256 * (unsigned int)((ORD8 *)p)[6]
	     +            (unsigned int)((ORD8 *)p)[7];
}

static int write_UInt64Number(icmUInt64 *d, char *p) {
	((ORD8 *)p)[0] = (ORD8)(d->h >> 24);
	((ORD8 *)p)[1] = (ORD8)(d->h >> 16);
	((ORD8 *)p)[2] = (ORD8)(d->h >> 8);
	((ORD8 *)p)[3] = (ORD8)(d->h);
	((ORD8 *)p)[4] = (ORD8)(d->l >> 24);
	((ORD8 *)p)[5] = (ORD8)(d->l >> 16);
	((ORD8 *)p)[6] = (ORD8)(d->l >> 8);
	((ORD8 *)p)[7] = (ORD8)(d->l);
	return 0;
}

static double read_U8Fixed8Number(char *p) {
	ORD32 o32;
	o32 = 256 * (ORD32)((ORD8 *)p)[0]		/* Read big endian 16 bit unsigned */
        +       (ORD32)((ORD8 *)p)[1];
	return (double)o32/256.0;
}

static int write_U8Fixed8Number(double d, char *p) {
	ORD32 o32;
	d = d * 256.0 + 0.5;
	if (d >= 65536.0)
		return 1;
	if (d < 0.0)
		return 1;
	o32 = (ORD32)d;
	((ORD8 *)p)[0] = (ORD8)((o32) >> 8);
	((ORD8 *)p)[1] = (ORD8)((o32));
	return 0;
}

static double read_U16Fixed16Number(char *p) {
	ORD32 o32;
	o32 = 16777216 * (ORD32)((ORD8 *)p)[0]		/* Read big endian 32 bit unsigned */
        +    65536 * (ORD32)((ORD8 *)p)[1]
	    +      256 * (ORD32)((ORD8 *)p)[2]
	    +            (ORD32)((ORD8 *)p)[3];
	return (double)o32/65536.0;
}

static int write_U16Fixed16Number(double d, char *p) {
	ORD32 o32;
	d = d * 65536.0 + 0.5;
	if (d >= 4294967296.0)
		return 1;
	if (d < 0.0)
		return 1;
	o32 = (ORD32)d;
	((ORD8 *)p)[0] = (ORD8)((o32) >> 24);
	((ORD8 *)p)[1] = (ORD8)((o32) >> 16);
	((ORD8 *)p)[2] = (ORD8)((o32) >> 8);
	((ORD8 *)p)[3] = (ORD8)((o32));
	return 0;
}


/* Signed numbers */
static int read_SInt8Number(char *p) {
	int rv;
	rv = (int)((INR8 *)p)[0];
	return rv;
}

static int write_SInt8Number(int d, char *p) {
	if (d > 127)
		return 1;
	else if (d < -128)
		return 1;
	((INR8 *)p)[0] = (INR8)d;
	return 0;
}

static int read_SInt16Number(char *p) {
	int rv;
	rv = 256 * (int)((INR8 *)p)[0]
	   +       (int)((ORD8 *)p)[1];
	return rv;
}

static int write_SInt16Number(int d, char *p) {
	if (d > 32767)
		return 1;
	else if (d < -32768)
		return 1;
	((INR8 *)p)[0] = (INR8)(d >> 8);
	((ORD8 *)p)[1] = (ORD8)(d);
	return 0;
}

static int read_SInt32Number(char *p) {
	int rv;
	rv = 16777216 * (int)((INR8 *)p)[0]
	   +    65536 * (int)((ORD8 *)p)[1]
	   +      256 * (int)((ORD8 *)p)[2]
	   +            (int)((ORD8 *)p)[3];
	return rv;
}

static int write_SInt32Number(int d, char *p) {
	((INR8 *)p)[0] = (INR8)(d >> 24);
	((ORD8 *)p)[1] = (ORD8)(d >> 16);
	((ORD8 *)p)[2] = (ORD8)(d >> 8);
	((ORD8 *)p)[3] = (ORD8)(d);
	return 0;
}

static void read_SInt64Number(icmInt64 *d, char *p) {
	d->h = 16777216 * (int)((INR8 *)p)[0]
	     +    65536 * (int)((ORD8 *)p)[1]
	     +      256 * (int)((ORD8 *)p)[2]
	     +            (int)((ORD8 *)p)[3];
	d->l = 16777216 * (unsigned int)((ORD8 *)p)[4]
	     +    65536 * (unsigned int)((ORD8 *)p)[5]
	     +      256 * (unsigned int)((ORD8 *)p)[6]
	     +            (unsigned int)((ORD8 *)p)[7];
}

static int write_SInt64Number(icmInt64 *d, char *p) {
	((INR8 *)p)[0] = (INR8)(d->h >> 24);
	((ORD8 *)p)[1] = (ORD8)(d->h >> 16);
	((ORD8 *)p)[2] = (ORD8)(d->h >> 8);
	((ORD8 *)p)[3] = (ORD8)(d->h);
	((ORD8 *)p)[4] = (ORD8)(d->l >> 24);
	((ORD8 *)p)[5] = (ORD8)(d->l >> 16);
	((ORD8 *)p)[6] = (ORD8)(d->l >> 8);
	((ORD8 *)p)[7] = (ORD8)(d->l);
	return 0;
}

static double read_S15Fixed16Number(char *p) {
	INR32 i32;
	i32 = 16777216 * (INR32)((INR8 *)p)[0]		/* Read big endian 32 bit signed */
        +    65536 * (INR32)((ORD8 *)p)[1]
	    +      256 * (INR32)((ORD8 *)p)[2]
	    +            (INR32)((ORD8 *)p)[3];
	return (double)i32/65536.0;
}

static int write_S15Fixed16Number(double d, char *p) {
	INR32 i32;
	d = floor(d * 65536.0 + 0.5);		/* Beware! (int)(d + 0.5) doesn't work! */
	if (d >= 2147483648.0)
		return 1;
	if (d < -2147483648.0)
		return 1;
	i32 = (INR32)d;
	((INR8 *)p)[0] = (INR8)((i32) >> 24);		/* Write big endian 32 bit signed */
	((ORD8 *)p)[1] = (ORD8)((i32) >> 16);
	((ORD8 *)p)[2] = (ORD8)((i32) >> 8);
	((ORD8 *)p)[3] = (ORD8)((i32));
	return 0;
}

/* Device coordinate as 8 bit value range 0.0 - 1.0 */
static double read_DCS8Number(char *p) {
	unsigned int rv;
	rv =   (unsigned int)((ORD8 *)p)[0];
	return (double)rv/255.0;
}

static int write_DCS8Number(double d, char *p) {
	ORD32 o32;
	d = d * 255.0 + 0.5;
	if (d >= 256.0)
		return 1;
	if (d < 0.0)
		return 1;
	o32 = (ORD32)d;
	((ORD8 *)p)[0] = (ORD8)(o32);
	return 0;
}

/* Device coordinate as 16 bit value range 0.0 - 1.0 */
static double read_DCS16Number(char *p) {
	unsigned int rv;
	rv = 256 * (unsigned int)((ORD8 *)p)[0]
	   +       (unsigned int)((ORD8 *)p)[1];
	return (double)rv/65535.0;
}

static int write_DCS16Number(double d, char *p) {
	ORD32 o32;
	d = d * 65535.0 + 0.5;
	if (d >= 65536.0)
		return 1;
	if (d < 0.0)
		return 1;
	o32 = (ORD32)d;
	((ORD8 *)p)[0] = (ORD8)(o32 >> 8);
	((ORD8 *)p)[1] = (ORD8)(o32);
	return 0;
}

static void icmNormToXYZ(double *out, double *in);
static void icmXYZToNorm(double *out, double *in);
static void icmNormToLab8(double *out, double *in);
static void icmLab8ToNorm(double *out, double *in);
static void icmNormToLab16v2(double *out, double *in);
static void icmLab16v2ToNorm(double *out, double *in);
static void icmNormToLab16v4(double *out, double *in);
static void icmLab16v4ToNorm(double *out, double *in);

/* read a PCS number. PCS can be profile PCS, profile version Lab, */
/* or a specific type of Lab, depending on the value of csig: */
/*   icmSigPCSData, icSigXYZData, icmSigLab8Data, icSigLabData, */
/*   icmSigLabV2Data or icmSigLabV4Data */
/* Do nothing if not one of the above. */
static void read_PCSNumber(icc *icp, icColorSpaceSignature csig, double pcs[3], char *p) {

	if (csig == icmSigPCSData)
		csig = icp->header->pcs;
	if (csig == icSigLabData) {
		if (icp->header->vers.majv >= 4)
			csig = icmSigLabV4Data;
		else
			csig = icmSigLabV2Data;
	}

	if (csig == icmSigLab8Data) {
		pcs[0] = read_DCS8Number(p);
		pcs[1] = read_DCS8Number(p+1);
		pcs[2] = read_DCS8Number(p+2);
	} else {
		pcs[0] = read_DCS16Number(p);
		pcs[1] = read_DCS16Number(p+2);
		pcs[2] = read_DCS16Number(p+4);
	}
	switch ((int)csig) {
		case icSigXYZData:
			icmNormToXYZ(pcs, pcs);
			break;
		case icmSigLab8Data:
			icmNormToLab8(pcs, pcs);
			break;
		case icmSigLabV2Data:
			icmNormToLab16v2(pcs, pcs);
			break;
		case icmSigLabV4Data:
			icmNormToLab16v4(pcs, pcs);
			break;
		default:
			break;
	}
}

/* write a PCS number. PCS can be profile PCS, profile version Lab, */
/* or a specific type of Lab, depending on the value of csig: */
/*   icmSigPCSData, icSigXYZData, icmSigLab8Data, icSigLabData, */
/*   icmSigLabV2Data or icmSigLabV4Data */
/* Return 1 if error */
static int write_PCSNumber(icc *icp, icColorSpaceSignature csig, double pcs[3], char *p) {
	double v[3];
	int j;

	if (csig == icmSigPCSData)
		csig = icp->header->pcs;
	if (csig == icSigLabData) {
		if (icp->header->vers.majv >= 4)
			csig = icmSigLabV4Data;
		else
			csig = icmSigLabV2Data;
	}

	switch ((int)csig) {
		case icSigXYZData:
			icmXYZToNorm(v, pcs);
			break;
		case icmSigLab8Data:
			icmLab8ToNorm(v, pcs);
			break;
		case icmSigLabV2Data:
			icmLab16v2ToNorm(v, pcs);
			break;
		case icmSigLabV4Data:
			icmLab16v4ToNorm(v, pcs);
			break;
		default:
			return 1;
	}
	if (csig == icmSigLab8Data) {
		for (j = 0; j < 3; j++) {
			if (write_DCS8Number(v[j], p+j))
				return 1;
		}
	} else {
		for (j = 0; j < 3; j++) {
			if (write_DCS16Number(v[j], p+(2 * j)))
				return 1;
		}
	}

	return 0;
}

/* ---------------------------------------------------------- */
/* Auiliary function - return a string that represents a tag */
/* Note - returned buffers are static, can only be used 5 */
/* times before buffers get reused. */
char *icmtag2str(
	int tag
) {
	int i;
	static int si = 0;			/* String buffer index */
	static char buf[5][50];		/* String buffers */
	char *bp;
	unsigned char c[4];

	bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	c[0] = 0xff & (tag >> 24);
	c[1] = 0xff & (tag >> 16);
	c[2] = 0xff & (tag >> 8);
	c[3] = 0xff & (tag >> 0);
	for (i = 0; i < 4; i++) {	/* Can we represent it as a string ? */
		if (!isprint(c[i]))
			break;
	}
	if (i < 4) {	/* Not printable - use hex */
		sprintf(bp,"0x%x",tag);
	} else {		/* Printable */
		sprintf(bp,"'%c%c%c%c'",c[0],c[1],c[2],c[3]);
	}
	return bp;
}

/* Auiliary function - return a tag created from a string */
/* Note there is also the icmMakeTag() macro */
unsigned int icmstr2tag(
	const char *str
) {
	unsigned int tag;
	tag = (((unsigned int)str[0]) << 24)
	    + (((unsigned int)str[1]) << 16)
	    + (((unsigned int)str[2]) << 8)
	    + (((unsigned int)str[3]));
	return tag;
}

/* Helper - return 1 if the string doesn't have a */
/* null terminator within len, return 0 if it has null at exactly len, */
/* and 2 if it has null before len. */
/* Note: will return 1 if len == 0 */
static int check_null_string(char *cp, int len) {
	for (; len > 0; len--) {
		if (cp[0] == '\000')
			break;
		cp++;
	}
	if (len == 0)
		return 1;
	if (len > 1)
		return 2;
	return 0;
}

/* helper - return 1 if the string doesn't have a */
/* null terminator within len, return 0 has null at exactly len, */
/* and 2 if it has null before len. */
/* Note: will return 1 if len == 0 */
/* Unicode version */
static int check_null_string16(char *cp, int len) {
	for (; len > 0; len--) {	/* Length is in characters */
		if (cp[0] == 0 && cp[1] == 0)
			break;
		cp += 2;
	}
	if (len == 0) 
		return 1;
	if (len > 1)
		return 2;
	return 0;
}

/* ========================================================== */
/* Object I/O routines                                        */
/* ========================================================== */
/* icmUnknown object */

/* The tag table icmTagRec.ttype retains the actual ttype, */
/* while the icmUnknown.ttype is icmSigUnknownType, */
/* and has a uttype of the actual type. */

/* Serialise this tag type */
static void icmUnknown_serialise(icmUnknown *p, icmFBuf *b) {
	unsigned int i;

    icmSn_TagTypeSig32(b, &p->uttype);		/* 0-3: Unknown Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */
	if (icmArrayRdAllocResize(b, icmAResizeBySize, &p->_count, &p->count,
	    (void **)&p->data, NULL, sizeof(unsigned char),
	    UINT_MAX, 0, 1, "icmUnknown"))
		return;
	if (b->op & icmSnSerialise)				/* (Optimise for speed) */
		for (i = 0; i < p->count; i++)
			icmSn_uc_UInt8(b, &p->data[i]);	/* 8-n: Unknown tag data */
	ICMFREEARRAY(p, count, data)
}

/* Dump a text description of the object */
static void icmUnknown_dump(icmUnknown *p, icmFile *op, int verb) {
	unsigned int i, ii, r, ph;

	if (verb <= 1)
		return;

	op->printf(op,"Unknown:\n");
	op->printf(op,"  Payload size in bytes = %u\n",p->count);

	/* Print one row of binary and ASCII interpretation if verb == 2, All if == 3 */
	/* else print all of it. */
	ii = i = ph = 0;
	for (r = 1;; r++) {		/* size rows */
		int c = 1;			/* Character location */

		c = 1;
		if (ph != 0) {	/* Print ASCII under binary */
			op->printf(op,"           ");
			i = ii;				/* Swap */
			c += 11;
		} else {
			op->printf(op,"    0x%04lx: ",i);
			ii = i;				/* Swap */
			c += 10;
		}
		while (i < p->count && c < 75) {
			if (ph == 0) 
				op->printf(op,"%02x ",p->data[i]);
			else {
				if (isprint(p->data[i]))
					op->printf(op," %c ",p->data[i]);
				else
					op->printf(op,"   ",p->data[i]);
			}
			c += 3;
			i++;
		}
		if (ph == 0 || i < p->count)
			op->printf(op,"\n");

		if (ph == 1 && i >= p->count) {
			op->printf(op,"\n");
			break;
		}
		if (ph == 1 && r > 1 && verb < 3) {
			op->printf(op,"    ...\n");
			break;			/* Print 1 row if not verbose */
		}

		if (ph == 0)
			ph = 1;
		else
			ph = 0;

	}
}

/* Check Unknown */
static int icmUnknown_check(icmUnknown *p, icTagSignature sig) {
	// TBD
	return ICM_ERR_OK;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUnknown(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT(icmUnknown, icmSigUnknownType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt8Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUInt8Array_get_size(
	icmUInt8Array *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->count, 1);	/* 1 byte for each UInt8 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUInt8Array_read(
	icmUInt8Array *p,
	unsigned int len,	/* tag length */
	unsigned int of	/* start offset within file */
) {
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		return icm_err(icp, 1, "icmUInt8Array_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2, "icmUInt8Array_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1, "icmUInt8Array_read: fseek() or fread() failed");
	}
	p->count = size = (len - 8)/1;		/* Number of elements in the array */

	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1, "icmUInt8Array_read: Wrong tag type for icmUInt8Array");
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 1) {
		p->data[i] = read_UInt8Number(bp);
	}
	icp->al->free(p->icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUInt8Array_write(
	icmUInt8Array *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1, "icmUInt8Array_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2, "icmUInt8Array_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv, "icmUInt8Array_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp += 8;	/* Skip padding */

	/* Write all the data to the buffer */
	for (i = 0; i < p->count; i++, bp += 1) {
		if ((rv = write_UInt8Number(p->data[i],bp)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv, "icmUInt8Array_write: write_UInt8umber() failed");
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2, "icmUInt8Array_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUInt8Array_dump(
	icmUInt8Array *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"UInt8Array:\n");
	op->printf(op,"  No. elements = %lu\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %lu:  %u\n",i,p->data[i]);
	}
}

/* Allocate variable sized data elements */
static int icmUInt8Array_allocate(
	icmUInt8Array *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(unsigned int))) {
			return icm_err(icp, 1, "icmUInt8Array_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (unsigned int *) icp->al->calloc(icp->al, p->count, sizeof(unsigned int)))
		                                                                              == NULL) {
			return icm_err(icp, 2, "icmUInt8Array_alloc: malloc() of icmUInt8Array data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUInt8Array_delete(
	icmUInt8Array *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt8Array(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmUInt8Array, icSigUInt8ArrayType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt16Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUInt16Array_get_size(
	icmUInt16Array *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->count, 2);	/* 2 bytes for each UInt16 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUInt16Array_read(
	icmUInt16Array *p,
	unsigned int len,	/* tag length */
	unsigned int of	/* start offset within file */
) {
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		return icm_err(icp, 1, "icmUInt16Array_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2, "icmUInt16Array_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1, "icmUInt16Array_read: fseek() or fread() failed");
	}
	p->count = size = (len - 8)/2;		/* Number of elements in the array */

	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1, "icmUInt16Array_read: Wrong tag type for icmUInt16Array");
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 2) {
		p->data[i] = read_UInt16Number(bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUInt16Array_write(
	icmUInt16Array *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1, "icmUInt16Array_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmUInt16Array_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmUInt16Array_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->count; i++, bp += 2) {
		if ((rv = write_UInt16Number(p->data[i],bp)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmUInt16Array_write: write_UInt16umber() failed");
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmUInt16Array_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUInt16Array_dump(
	icmUInt16Array *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"UInt16Array:\n");
	op->printf(op,"  No. elements = %lu\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %lu:  %u\n",i,p->data[i]);
	}
}

/* Allocate variable sized data elements */
static int icmUInt16Array_allocate(
	icmUInt16Array *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(unsigned int))) {
			return icm_err(icp, 1,"icmUInt16Array_alloc:: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (unsigned int *) icp->al->calloc(icp->al, p->count, sizeof(unsigned int)))
		                                                                              == NULL) {
			return icm_err(icp, 2,"icmUInt16Array_alloc: malloc() of icmUInt16Array data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUInt16Array_delete(
	icmUInt16Array *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt16Array(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmUInt16Array, icSigUInt16ArrayType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt32Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUInt32Array_get_size(
	icmUInt32Array *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->count, 4);	/* 4 bytes for each UInt32 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUInt32Array_read(
	icmUInt32Array *p,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		return icm_err(icp, 1,"icmUInt32Array_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmUInt32Array_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmUInt32Array_read: fseek() or fread() failed");
	}
	p->count = size = (len - 8)/4;		/* Number of elements in the array */

	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmUInt32Array_read: Wrong tag type for icmUInt32Array");
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 4) {
		p->data[i] = read_UInt32Number(bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUInt32Array_write(
	icmUInt32Array *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmUInt32Array_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmUInt32Array_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmUInt32Array_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->count; i++, bp += 4) {
		if ((rv = write_UInt32Number(p->data[i],bp)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmUInt32Array_write: write_UInt32umber() failed");
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmUInt32Array_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUInt32Array_dump(
	icmUInt32Array *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"UInt32Array:\n");
	op->printf(op,"  No. elements = %lu\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %lu:  %u\n",i,p->data[i]);
	}
}

/* Allocate variable sized data elements */
static int icmUInt32Array_allocate(
	icmUInt32Array *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(unsigned int))) {
			return icm_err(icp, 2,"icmUInt32Array_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (unsigned int *) icp->al->calloc(icp->al, p->count, sizeof(unsigned int)))
		                                                                              == NULL) {
			return icm_err(icp, 3,"icmUInt32Array_alloc: malloc() of icmUInt32Array data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUInt32Array_delete(
	icmUInt32Array *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt32Array(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmUInt32Array, icSigUInt32ArrayType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt64Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUInt64Array_get_size(
	icmUInt64Array *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->count, 8);	/* 8 bytes for each UInt64 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUInt64Array_read(
	icmUInt64Array *p,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		return icm_err(icp, 1,"icmUInt64Array_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmUInt64Array_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmUInt64Array_read: fseek() or fread() failed");
	}
	p->count = size = (len - 8)/8;		/* Number of elements in the array */

	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmUInt64Array_read: Wrong tag type for icmUInt64Array");
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 8) {
		read_UInt64Number(&p->data[i], bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUInt64Array_write(
	icmUInt64Array *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmUInt64Array_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmUInt64Array_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmUInt64Array_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->count; i++, bp += 8) {
		if ((rv = write_UInt64Number(&p->data[i],bp)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmUInt64Array_write: write_UInt64umber() failed");
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmUInt64Array_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUInt64Array_dump(
	icmUInt64Array *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"UInt64Array:\n");
	op->printf(op,"  No. elements = %lu\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %lu:  h=%lu, l=%lu\n",i,p->data[i].h,p->data[i].l);
	}
}

/* Allocate variable sized data elements */
static int icmUInt64Array_allocate(
	icmUInt64Array *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(icmUInt64))) {
			return icm_err(icp, 1,"icmUInt64Array_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmUInt64 *) icp->al->calloc(icp->al, p->count, sizeof(icmUInt64)))
		                                                                        == NULL) {
			return icm_err(icp, 2,"icmUInt64Array_alloc: malloc() of icmUInt64Array data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUInt64Array_delete(
	icmUInt64Array *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt64Array(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmUInt64Array, icSigUInt64ArrayType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmU16Fixed16Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmU16Fixed16Array_get_size(
	icmU16Fixed16Array *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->count, 4);	/* 4 byte for each U16Fixed16 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmU16Fixed16Array_read(
	icmU16Fixed16Array *p,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		return icm_err(icp, 1,"icmU16Fixed16Array_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmU16Fixed16Array_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmU16Fixed16Array_read: fseek() or fread() failed");
	}
	p->count = size = (len - 8)/4;		/* Number of elements in the array */

	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmU16Fixed16Array_read: Wrong tag type for icmU16Fixed16Array");
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 4) {
		p->data[i] = read_U16Fixed16Number(bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmU16Fixed16Array_write(
	icmU16Fixed16Array *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmU16Fixed16Array_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmU16Fixed16Array_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmU16Fixed16Array_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->count; i++, bp += 4) {
		if ((rv = write_U16Fixed16Number(p->data[i],bp)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmU16Fixed16Array_write: write_U16Fixed16umber() failed");
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmU16Fixed16Array_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmU16Fixed16Array_dump(
	icmU16Fixed16Array *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"U16Fixed16Array:\n");
	op->printf(op,"  No. elements = %lu\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %lu:  %.8f\n",i,p->data[i]);
	}
}

/* Allocate variable sized data elements */
static int icmU16Fixed16Array_allocate(
	icmU16Fixed16Array *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(double))) {
			return icm_err(icp, 1,"icmU16Fixed16Array_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (double *) icp->al->calloc(icp->al, p->count, sizeof(double))) == NULL) {
			return icm_err(icp, 2,"icmU16Fixed16Array_alloc: malloc() of icmU16Fixed16Array data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmU16Fixed16Array_delete(
	icmU16Fixed16Array *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmU16Fixed16Array(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmU16Fixed16Array, icSigU16Fixed16ArrayType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmS15Fixed16Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmS15Fixed16Array_get_size(
	icmS15Fixed16Array *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->count, 4);	/* 4 byte for each S15Fixed16 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmS15Fixed16Array_read(
	icmS15Fixed16Array *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		return icm_err(icp, 1,"icmS15Fixed16Array_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmS15Fixed16Array_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmS15Fixed16Array_read: fseek() or fread() failed");
	}
	p->count = size = (len - 8)/4;		/* Number of elements in the array */

	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmS15Fixed16Array_read: Wrong tag type for icmS15Fixed16Array");
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 4) {
		p->data[i] = read_S15Fixed16Number(bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmS15Fixed16Array_write(
	icmS15Fixed16Array *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmS15Fixed16Array_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmS15Fixed16Array_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmS15Fixed16Array_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->count; i++, bp += 4) {
		if ((rv = write_S15Fixed16Number(p->data[i],bp)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmS15Fixed16Array_write: write_S15Fixed16umber() failed");
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmS15Fixed16Array_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmS15Fixed16Array_dump(
	icmS15Fixed16Array *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"S15Fixed16Array:\n");
	op->printf(op,"  No. elements = %lu\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %lu:  %.8f\n",i,p->data[i]);
	}
}

/* Allocate variable sized data elements */
static int icmS15Fixed16Array_allocate(
	icmS15Fixed16Array *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(double))) {
			return icm_err(icp, 2,"icmS15Fixed16Array_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (double *) icp->al->calloc(icp->al, p->count, sizeof(double))) == NULL) {
			return icm_err(icp, 2,"icmS15Fixed16Array_alloc: malloc() of icmS15Fixed16Array data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmS15Fixed16Array_delete(
	icmS15Fixed16Array *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmS15Fixed16Array(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmS15Fixed16Array, icSigS15Fixed16ArrayType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */

/* Data conversion support functions */
static int write_XYZNumber(icmXYZNumber *p, char *d) {
	int rv;
	if ((rv = write_S15Fixed16Number(p->X, d + 0)) != 0)
		return rv;
	if ((rv = write_S15Fixed16Number(p->Y, d + 4)) != 0)
		return rv;
	if ((rv = write_S15Fixed16Number(p->Z, d + 8)) != 0)
		return rv;
	return 0;
}

static int read_XYZNumber(icmXYZNumber *p, char *d) {
	p->X = read_S15Fixed16Number(d + 0);
	p->Y = read_S15Fixed16Number(d + 4);
	p->Z = read_S15Fixed16Number(d + 8);
	return 0;
}

/* icmXYZArray object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmXYZArray_get_size(
	icmXYZArray *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->count, 12);	/* 12 bytes for each XYZ */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmXYZArray_read(
	icmXYZArray *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		return icm_err(icp, 1,"icmXYZArray_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmXYZArray_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmXYZArray_read: fseek() or fread() failed");
	}
	p->count = size = (len - 8)/12;		/* Number of elements in the array */

	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmXYZArray_read: Wrong tag type for icmXYZArray");
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 12) {
		read_XYZNumber(&p->data[i], bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmXYZArray_write(
	icmXYZArray *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmXYZArray_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmXYZArray_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmXYZArray_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->count; i++, bp += 12) {
		if ((rv = write_XYZNumber(&p->data[i],bp)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmXYZArray_write: write_XYZumber() failed");
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmXYZArray_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmXYZArray_dump(
	icmXYZArray *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"XYZArray:\n");
	op->printf(op,"  No. elements = %lu\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++) {
			op->printf(op,"    %lu:  %s\n",i,icmXYZNumber_and_Lab2str(&p->data[i]));
			
		}
	}
}

/* Allocate variable sized data elements */
static int icmXYZArray_allocate(
	icmXYZArray *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(icmXYZNumber))) {
			return icm_err(icp, 1,"icmXYZArray_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmXYZNumber *) icp->al->malloc(icp->al, sat_mul(p->count, sizeof(icmXYZNumber)))) == NULL) {
			return icm_err(icp, 2,"icmXYZArray_alloc: malloc() of icmXYZArray data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmXYZArray_delete(
	icmXYZArray *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmXYZArray(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmXYZArray, icSigXYZArrayType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmCurve object */

/* Do a forward lookup through the curve */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static int icmCurve_lookup_fwd(
	icmCurve *p,
	double *out,
	double *in
) {
	int rv = 0;
	if (p->flag == icmCurveLin) {
		*out = *in;
	} else if (p->flag == icmCurveGamma) {
		double val = *in;
		if (val <= 0.0)
			*out = 0.0;
		else
			*out = pow(val, p->data[0]);
	} else if (p->count == 0) { /* Table of 0 size */
		*out = *in;
	} else { /* Use linear interpolation */
		unsigned int ix;
		double val, w;
		double inputEnt_1 = (double)(p->count-1);

		val = *in * inputEnt_1;
		if (val < 0.0) {
			val = 0.0;
			rv |= 1;
		} else if (val > inputEnt_1) {
			val = inputEnt_1;
			rv |= 1;
		}
		ix = (unsigned int)floor(val);		/* Coordinate */
		if (ix > (p->count-2))
			ix = (p->count-2);
		w = val - (double)ix;		/* weight */
		val = p->data[ix];
		*out = val + w * (p->data[ix+1] - val);
	}
	return rv;
}

/* - - - - - - - - - - - - */
/* Support for reverse interpolation of 1D lookup tables */

/* Create a reverse curve lookup acceleration table */
/* return non-zero on error, 2 = malloc error. */
static int icmTable_setup_bwd(
	icc          *icp,			/* Base icc object */
	icmRevTable  *rt,			/* Reverse table data to setup */
	unsigned int size,			/* Size of fwd table */
	double       *data			/* Table */
) {
	unsigned int i;

	rt->count = size;		/* Stash pointers to these away */
	rt->data = data;
	
	/* Find range of output values */
	rt->rmin = 1e300;
	rt->rmax = -1e300;
	for (i = 0; i < rt->count; i++) {
		if (rt->data[i] > rt->rmax)
			rt->rmax = rt->data[i];
		if (rt->data[i] < rt->rmin)
			rt->rmin = rt->data[i];
	}

	/* Decide on reverse granularity */
	rt->rsize = sat_add(rt->count,2)/2;
	rt->qscale = (double)rt->rsize/(rt->rmax - rt->rmin);	/* Scale factor to quantize to */
	
	if (ovr_mul(rt->count, sizeof(unsigned int *))) {
		return 2;
	}
	/* Initialize the reverse lookup structures, and get overall min/max */
	if ((rt->rlists = (unsigned int **) icp->al->calloc(icp->al, rt->rsize, sizeof(unsigned int *))) == NULL) {
		return 2;
	}

	/* Assign each output value range bucket lists it intersects */
	for (i = 0; i < (rt->count-1); i++) {
		unsigned int s, e, j;	/* Start and end indexes (inclusive) */
		s = (unsigned int)((rt->data[i] - rt->rmin) * rt->qscale);
		e = (unsigned int)((rt->data[i+1] - rt->rmin) * rt->qscale);
		if (s >= rt->rsize)
			s = rt->rsize-1;
		if (e >= rt->rsize)
			e = rt->rsize-1;
		if (s > e) {	/* swap */
			unsigned int t;
			t = s; s = e; e = t;
		}

		/* For all buckets that may contain this output range, add index of this output */
		for (j = s; j <= e; j++) {
			unsigned int as;			/* Allocation size */
			unsigned int nf;			/* Next free slot */
			if (rt->rlists[j] == NULL) {	/* No allocation */
				as = 5;						/* Start with space for 5 */
				if ((rt->rlists[j] = (unsigned int *) icp->al->calloc(icp->al, as, sizeof(unsigned int))) == NULL) {
					return 2;
				}
				rt->rlists[j][0] = as;
				nf = rt->rlists[j][1] = 2;
			} else {
				as = rt->rlists[j][0];	/* Allocate space for this list */
				nf = rt->rlists[j][1];	/* Next free location in list */
				if (nf >= as) {			/* need to expand space */
					if ((as = sat_mul(as, 2)) == UINT_MAX
					 || ovr_mul(as, sizeof(unsigned int))) {
						return 2;
					}
					rt->rlists[j] = (unsigned int *) icp->al->realloc(icp->al,rt->rlists[j], as * sizeof(unsigned int));
					if (rt->rlists[j] == NULL) {
						return 2;
					}
					rt->rlists[j][0] = as;
				}
			}
			rt->rlists[j][nf++] = i;
			rt->rlists[j][1] = nf;
		}
	}
	rt->inited = 1;
	return 0;
}

/* Free up any data */
static void icmTable_delete_bwd(
	icc          *icp,			/* Base icc */
	icmRevTable  *rt			/* Reverse table data to setup */
) {
	if (rt->inited != 0) {
		while (rt->rsize > 0)
			icp->al->free(icp->al, rt->rlists[--rt->rsize]);
		icp->al->free(icp->al, rt->rlists);
		rt->count = 0;			/* Don't keep these */
		rt->data = NULL;
	}
}

/* Do a reverse lookup through the curve */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static int icmTable_lookup_bwd(
	icmRevTable *rt,
	double *out,
	double *in
) {
	int rv = 0;
	unsigned int ix, k, i;
	double oval, ival = *in, val;
	double rsize_1;

	/* Find appropriate reverse list */
	rsize_1 = (double)(rt->rsize-1);
	val = ((ival - rt->rmin) * rt->qscale);
	if (val < 0.0)
		val = 0.0;
	else if (val > rsize_1)
		val = rsize_1;
	ix = (unsigned int)floor(val);		/* Coordinate */

	if (ix > (rt->count-2))
		ix = (rt->count-2);
	if (rt->rlists[ix] != NULL)  {		/* There is a list of fwd candidates */
		/* For each candidate forward range */
		for (i = 2; i < rt->rlists[ix][1]; i++)  {	/* For all fwd indexes */
			double lv,hv;
			k = rt->rlists[ix][i];					/* Base index */
			lv = rt->data[k];
			hv = rt->data[k+1];
			if ((ival >= lv && ival <= hv)	/* If this slot contains output value */
			 || (ival >= hv && ival <= lv)) {
				/* Reverse linear interpolation */
				if (hv == lv) {	/* Technically non-monotonic - due to quantization ? */
					oval = (k + 0.5)/(rt->count-1.0);
				} else
					oval = (k + ((ival - lv)/(hv - lv)))/(rt->count-1.0);
				/* If we kept looking, we would find multiple */
				/* solution for non-monotonic curve */
				*out = oval;
				return rv;
			}
		}
	}

	/* We have failed to find an exact value, so return the nearest value */
	/* (This is slow !) */
	val = fabs(ival - rt->data[0]);
	for (k = 0, i = 1; i < rt->count; i++) {
		double er;
		er = fabs(ival - rt->data[i]);
		if (er < val) {	/* new best */
			val = er;
			k = i;
		}
	}
	*out = k/(rt->count-1.0);
	rv |= 1;
	return rv;
}


/* - - - - - - - - - - - - */

/* Do a reverse lookup through the curve */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
/* (Note that clipping means mathematical clipping, and is not */
/*  set just because a device value is out of gamut. */ 
static int icmCurve_lookup_bwd(
	icmCurve *p,
	double *out,
	double *in
) {
	icc *icp = p->icp;
	int rv = 0;
	if (p->flag == icmCurveLin) {
		*out = *in;
	} else if (p->flag == icmCurveGamma) {
		double val = *in;
		if (val <= 0.0)
			*out = 0.0;
		else
			*out = pow(val, 1.0/p->data[0]);
	} else if (p->count == 0) { /* Table of 0 size */
		*out = *in;
	} else { /* Use linear interpolation */
		if (p->rt.inited == 0) {	
			rv = icmTable_setup_bwd(icp, &p->rt, p->count, p->data);
			if (rv != 0) {
				return icm_err(icp, rv,"icmCurve_lookup: Malloc failure in reverse lookup init.");
			}
		}
		rv = icmTable_lookup_bwd(&p->rt, out, in);
	}
	return rv;
}

/* Return the number of bytes needed to write this tag */
static unsigned int icmCurve_get_size(
	icmCurve *p
) {
	unsigned int len = 0;
	len = sat_add(len, 12);				/* 12 bytes for tag, padding and count */
	len = sat_addmul(len, p->count, 2);	/* 2 bytes for each UInt16 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmCurve_read(
	icmCurve *p,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i;
	char *bp, *buf, *end;

	if (len < 12) {
		return icm_err(icp, 1,"icmCurve_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmCurve_read: malloc() failed");
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmCurve_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmCurve_read: Wrong tag type for icmCurve");
	}

	p->count = read_UInt32Number(bp+8);
	bp = bp + 12;

	/* Set flag up before allocating */
	if (p->count == 0) {		/* Linear curve */
		p->flag = icmCurveLin;
	} else if (p->count == 1) {	/* Gamma curve */
		p->flag = icmCurveGamma;
	} else {
		p->flag = icmCurveSpec;
		if (p->count > (len - 12)/2) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmCurve_read: size overflow");
		}
	}

	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	if (p->flag == icmCurveGamma) {	/* Gamma curve */
		if (bp > end || 1 > (end - bp)) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmCurve_read: Data too short for curve gamma");
		}
		p->data[0] = read_U8Fixed8Number(bp);
	} else if (p->flag == icmCurveSpec) {
		/* Read all the data from the buffer */
		for (i = 0; i < p->count; i++, bp += 2) {
			if (bp > end || 2 > (end - bp)) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, 1,"icmCurve_read: Data too short for curve value");
			}
			p->data[i] = read_DCS16Number(bp);
		}
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmCurve_write(
	icmCurve *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmCurve_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmCurve_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmCurve_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write count */
	if ((rv = write_UInt32Number(p->count,bp+8)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmCurve_write: write_UInt32Number() failed");
	}

	/* Write all the data to the buffer */
	bp += 12;	/* Skip padding */
	if (p->flag == icmCurveLin) {
		if (p->count != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmCurve_write: Must be exactly 0 entry for Linear");
		}
	} else if (p->flag == icmCurveGamma) {
		if (p->count != 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmCurve_write: Must be exactly 1 entry for Gamma");
		}
		if ((rv = write_U8Fixed8Number(p->data[0],bp)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmCurve_write: write_U8Fixed8umber(%.8f) failed",p->data[0]);
		}
	} else if (p->flag == icmCurveSpec) {
		if (p->count < 2) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmCurve_write: Must be 2 or more entries for Specified curve");
		}
		for (i = 0; i < p->count; i++, bp += 2) {
			if ((rv = write_DCS16Number(p->data[i],bp)) != 0) {
				return icm_err(icp, rv,"icmCurve_write: write_UInt16umber(%.8f) failed",p->data[i]);
				icp->al->free(icp->al, buf);
			}
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmCurve_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmCurve_dump(
	icmCurve *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"Curve:\n");

	if (p->flag == icmCurveLin) {
		op->printf(op,"  Curve is linear\n");
	} else if (p->flag == icmCurveGamma) {
		op->printf(op,"  Curve is gamma of %.8f\n",p->data[0]);
	} else {
		op->printf(op,"  No. elements = %lu\n",p->count);
		if (verb >= 2) {
			unsigned int i;
			for (i = 0; i < p->count; i++)
				op->printf(op,"    %3lu:  %.8f\n",i,p->data[i]);
		}
	}
}

/* Allocate variable sized data elements */
static int icmCurve_allocate(
	icmCurve *p
) {
	icc *icp = p->icp;

	if (p->flag == icmCurveUndef) {
		return icm_err(icp, 1,"icmCurve_alloc: flag not set");
	} else if (p->flag == icmCurveLin) {
		p->count = 0;
	} else if (p->flag == icmCurveGamma) {
		p->count = 1;
	}
	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(double))) {
			return icm_err(icp, 1,"icmCurve_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (double *) icp->al->calloc(icp->al, p->count, sizeof(double))) == NULL) {
			return icm_err(icp, 2,"icmCurve_alloc: malloc() of icmCurve data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmCurve_delete(
	icmCurve *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icmTable_delete_bwd(icp, &p->rt);	/* Free reverse table info */
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmCurve(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmCurve, icSigCurveType)
	if (p == NULL)
		return NULL;

	p->lookup_fwd = icmCurve_lookup_fwd;
	p->lookup_bwd = icmCurve_lookup_bwd;

	p->rt.inited = 0;

	p->flag = icmCurveUndef;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmData object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmData_get_size(
	icmData *p
) {
	unsigned int len = 0;
	len = sat_add(len, 12);				/* 12 bytes for tag and padding */
	len = sat_addmul(len, p->count, 1);	/* 1 byte for each data element */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmData_read(
	icmData *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv;
	unsigned size, f;
	char *bp, *buf;

	if (len < 12) {
		return icm_err(icp, 1,"icmData_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmData_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmData_read: fseek() or fread() failed");
	}
	p->count = size = (len - 12)/1;		/* Number of elements in the array */

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmData_read: Wrong tag type for icmData");
	}
	/* Read the data type flag */
	f = read_UInt32Number(bp+8);
	if (f == 0) {
		p->flag = icmDataASCII;
	} else if (f == 1) {
		p->flag = icmDataBin;
	/* Profile maker sometimes has a problem */
	} else if ((p->icp->cflags & icmCFlagAllowQuirks) != 0
		     && f == 0x01000000) {
		p->icp->op = icmSnRead;			/* Let icmQuirkWarning know direction */
		icmQuirkWarning(p->icp, ICM_FMT_DATA_FLAG, 0, "Bad SigData flag value 0x%x", f);
		p->flag = icmDataBin;		/* Fix it */
	} else {
		icp->al->free(icp->al, buf);
		return icm_err(icp, ICM_FMT_DATA_FLAG,"icmData_read: Unknown flag value 0x%x",f);
	}
	bp += 12;	/* Skip padding and flag */

	if (p->count > 0) {
		if (p->flag == icmDataASCII) {
			if ((rv = check_null_string(bp,p->count)) == 1) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, ICM_FMT_DATA_TERM,"icmData_read: ACSII is not null terminated");
			}
			/* Haven't checked if rv == 2 is legal or not */
		}
		if ((rv = p->allocate(p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}

		memmove((void *)p->data, (void *)bp, p->count);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmData_write(
	icmData *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int len, f;
	char *bp, *buf;		/* Buffer to write from */
	int rv;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmData_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmData_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmData_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	switch(p->flag) {
		case icmDataASCII:
			f = 0;
			break;
		case icmDataBin:
			f = 1;
			break;
		default:
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmData_write: Unknown Data Flag value");
	}
	/* Write data flag descriptor to the buffer */
	if ((rv = write_UInt32Number(f,bp+8)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmData_write: write_SInt32Number() failed");
	}
	bp += 12;	/* Skip padding */

	if (p->data != NULL) {
		if (p->flag == icmDataASCII) {
			if ((rv = check_null_string((char *)p->data, p->count)) == 1) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, 1,"icmData_write: ASCII is not null terminated");
			}
			/* Haven't checked if rv == 2 is legal or not */
		}
		memmove((void *)bp, (void *)p->data, p->count);
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmData_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmData_dump(
	icmData *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	unsigned int i, r, c, ii, size = 0;
	int ph = 0;		/* Phase */

	if (verb <= 0)
		return;

	op->printf(op,"Data:\n");
	switch(p->flag) {
		case icmDataASCII:
			op->printf(op,"  ASCII data\n");
			size = p->count > 0 ? p->count-1 : 0;
			break;
		case icmDataBin:
			op->printf(op,"  Binary data\n");
			size = p->count;
			break;
		case icmDataUndef:
			op->printf(op,"  Undefined data\n");
			size = p->count;
			break;
	}
	op->printf(op,"  No. elements = %lu\n",p->count);

	ii = i = 0;
	for (r = 1;; r++) {		/* count rows */
		if (i >= size) {
			op->printf(op,"\n");
			break;
		}
		if (r > 1 && verb < 2) {
			op->printf(op,"...\n");
			break;			/* Print 1 row if not verbose */
		}

		c = 1;
		if (ph != 0) {	/* Print ASCII under binary */
			op->printf(op,"           ");
			i = ii;
			c += 11;
		} else {
			op->printf(op,"    0x%04lx: ",i);
			ii = i;
			c += 10;
		}
		while (i < size && c < 75) {
			if (p->flag == icmDataASCII) {
				if (isprint(p->data[i])) {
					op->printf(op,"%c",p->data[i]);
					c++;
				} else {
					op->printf(op,"\\%03o",p->data[i]);
					c += 4;
				}
			} else {
				if (ph == 0) 
					op->printf(op,"%02x ",p->data[i]);
				else {
					if (isprint(p->data[i]))
						op->printf(op," %c ",p->data[i]);
					else
						op->printf(op,"   ",p->data[i]);
				}
				c += 3;
			}
			i++;
		}
		if (i < size)
			op->printf(op,"\n");
		if (verb > 2 && p->flag != icmDataASCII && ph == 0)
			ph = 1;
		else
			ph = 0;
	}
}

/* Allocate variable sized data elements */
static int icmData_allocate(
	icmData *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(unsigned char))) {
			return icm_err(icp, 1,"icmData_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (unsigned char *) icp->al->calloc(icp->al, p->count, sizeof(unsigned char))) == NULL) {
			return icm_err(icp, 2,"icmData_alloc: malloc() of icmData data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmData_delete(
	icmData *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmData(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmData, icSigDataType)
	if (p == NULL)
		return NULL;

	p->flag = icmDataUndef;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmText object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmText_get_size(
	icmText *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->count, 1);	/* 1 byte for each character element (inc. null) */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmText_read(
	icmText *p,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv, chrv;
	char *bp, *buf;

	if (len < 8) {
		return icm_err(icp, 1,"icmText_read: Tag too short to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmText_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmText_read: fseek() or fread() failed");
	}
	p->count = (len - 8)/1;		/* Number of elements in the array */

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmText_read: Wrong tag type for icmText");
	}
	bp = bp + 8;

	if (p->count > 0) {
		if ((chrv = check_null_string(bp,p->count)) == 1) {
			if ((icp->cflags & icmCFlagAllowQuirks) == 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, ICM_FMT_TEXT_ANOTTERM,"icmText_read: ASCII string is not nul terminated");
			}
			p->icp->op = icmSnRead;			/* Let icmQuirkWarning know direction */
			icmQuirkWarning(icp, ICM_FMT_TEXT_ANOTTERM, 0, "icmText_read: ASCII string is not null terminated");

			p->count++;		/* Fix it */
			if ((rv = p->allocate(p)) != 0) {
				icp->al->free(icp->al, buf);
				return rv;
			}
			memmove((void *)p->data, (void *)bp, p->count-1);
			p->data[p->count-1] = '\000';
		} else {
			if (chrv == 2) {
				if ((icp->cflags & icmCFlagAllowQuirks) == 0)
					return icm_err(icp, ICM_FMT_TEXT_ASHORT,"icmText_read: ASCII string is shorter than count");
				p->icp->op = icmSnRead;			/* Let icmQuirkWarning know direction */
				icmQuirkWarning(icp, ICM_FMT_TEXT_ASHORT, 0, "icmText_read: ASCII string is shorter than count");
			}

			if ((rv = p->allocate(p)) != 0) {
				icp->al->free(icp->al, buf);
				return rv;
			}
			memmove((void *)p->data, (void *)bp, p->count);
			if (chrv == 2)
				p->count = strlen(p->data)+1; /* Repair string */
		}
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmText_write(
	icmText *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmText_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmText_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmText_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp = bp + 8;

	if (p->data != NULL) {
		if ((rv = check_null_string(p->data, p->count)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmText_write: text is not null terminated");
		}
		/* Haven't checked if rv == 2 is legal or not */
		memmove((void *)bp, (void *)p->data, p->count);
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmText_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmText_dump(
	icmText *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	unsigned int i, r, c, size;

	if (verb <= 0)
		return;

	op->printf(op,"Text:\n");
	op->printf(op,"  No. chars = %lu\n",p->count);

	size = p->count > 0 ? p->count-1 : 0;
	i = 0;
	for (r = 1;; r++) {		/* count rows */
		if (i >= size) {
			op->printf(op,"\n");
			break;
		}
		if (r > 1 && verb < 2) {
			op->printf(op,"...\n");
			break;			/* Print 1 row if not verbose */
		}
		c = 1;
		op->printf(op,"    0x%04lx: ",i);
		c += 10;
		while (i < size && c < 75) {
			if (isprint(p->data[i])) {
				op->printf(op,"%c",p->data[i]);
				c++;
			} else {
				op->printf(op,"\\%03o",p->data[i]);
				c += 4;
			}
			i++;
		}
		if (i < size)
			op->printf(op,"\n");
	}
}

/* Allocate variable sized data elements */
static int icmText_allocate(
	icmText *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(char))) {
			return icm_err(icp, 1,"icmText_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (char *) icp->al->calloc(icp->al, p->count, sizeof(char))) == NULL) {
			return icm_err(icp, 2,"icmText_alloc: malloc() of icmText data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmText_delete(
	icmText *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmText(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmText, icSigTextType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */

/* Data conversion support functions */
static int write_DateTimeNumber(icmDateTimeNumber *p, char *d) {
	int rv;
	if (p->year < 1900 || p->year > 3000
	 || p->month == 0 || p->month > 12
	 || p->day == 0 || p->day > 31
	 || p->hours > 23
	 || p->minutes > 59
	 || p->seconds > 59)
		return 1;

	if ((rv = write_UInt16Number(p->year,    d + 0)) != 0)
		return rv;
	if ((rv = write_UInt16Number(p->month,   d + 2)) != 0)
		return rv;
	if ((rv = write_UInt16Number(p->day,     d + 4)) != 0)
		return rv;
	if ((rv = write_UInt16Number(p->hours,   d + 6)) != 0)
		return rv;
	if ((rv = write_UInt16Number(p->minutes, d + 8)) != 0)
		return rv;
	if ((rv = write_UInt16Number(p->seconds, d + 10)) != 0)
		return rv;
	return 0;
}

static int read_DateTimeNumber(icc *icp, icmDateTimeNumber *p, char *d) {

	p->year    = read_UInt16Number(d + 0);
	p->month   = read_UInt16Number(d + 2);
	p->day     = read_UInt16Number(d + 4);
	p->hours   = read_UInt16Number(d + 6);
	p->minutes = read_UInt16Number(d + 8);
	p->seconds = read_UInt16Number(d + 10);

	/* Sanity check the date and time */
	if (p->year >= 1900 && p->year <= 3000
	 && p->month != 0 && p->month <= 12
	 && p->day != 0 && p->day <= 31
	 && p->hours <= 23
	 && p->minutes <= 59
	 && p->seconds <= 59)
		return 0;

#ifdef NEVER
	printf("Raw year = %d, month = %d, day = %d\n",p->year, p->month, p->day);
	printf("Raw hour = %d, minutes = %d, seconds = %d\n", p->hours, p->minutes, p->seconds);
#endif /* NEVER */

	icp->op = icmSnRead;			/* Let icmQuirkWarning know direction */
	icmQuirkWarning(icp, ICM_FMT_DATETIME, 0, "Bad date time '%s'", icmDateTimeNumber2str(p));

	if ((icp->cflags & icmCFlagAllowQuirks) == 0) {
		return 1;			/* Not legal */
	}

	/* Be more forgiving */

	/* Check for Adobe problem */
	if (p->month >= 1900 && p->month <= 3000
	 && p->year != 0 && p->year <= 12
	 && p->hours != 0 && p->hours <= 31
	 && p->day <= 23
	 && p->seconds <= 59
	 && p->minutes <= 59) {
		unsigned int tt; 

		/* Correct Adobe's faulty profile */
		tt = p->month; p->month = p->year; p->year = tt;
		tt = p->hours; p->hours = p->day; p->day = tt;
		tt = p->seconds; p->seconds = p->minutes; p->minutes = tt;

		return 0;
	}

	/* Hmm. some other sort of corruption. Limit values to sane */
	if (p->year < 1900) {
		if (p->year < 100)			/* Hmm. didn't use 4 digit year, guess it's 19xx ? */
			p->year += 1900;
		else
			p->year = 1900;
	} else if (p->year > 3000)
		p->year = 3000;

	if (p->month == 0)
		p->month = 1;
	else if (p->month > 12)
		p->month = 12;

	if (p->day == 0)
		p->day = 1;
	else if (p->day > 31)
		p->day = 31;

	if (p->hours > 23)
		p->hours = 23;

	if (p->minutes > 59)
		p->minutes = 59;

	if (p->seconds > 59)
		p->seconds = 59;

	return 0;
}

/* Return a string that shows the given date and time */
/* This can only be called once before reusing returned static buffer */
char *icmDateTimeNumber2str(icmDateTimeNumber *p) {
	static const char *mstring[13] = {"Bad", "Jan","Feb","Mar","Apr","May","Jun",
					  "Jul","Aug","Sep","Oct","Nov","Dec"};
	static char buf[80];

	sprintf(buf,"%d %s %4d, %d:%02d:%02d", 
	                p->day, mstring[p->month > 12 ? 0 : p->month], p->year,
	                p->hours, p->minutes, p->seconds);
	return buf;
}

/* NOTE that these standard time routines are not thread safe, */
/* and only opperate correctly for dates in the UNIX epoc, 1970-2038 */
/* Set the DateTime structure to the current date and time */

/* Set the DateTimeNumber to the current (UTC) date and time */
void icmDateTimeNumber_setcur(icmDateTimeNumber *p) {
	time_t cclk;
	struct tm *ctm;
	
	cclk = time(NULL);
	ctm = gmtime(&cclk);
	
	p->year    = ctm->tm_year + 1900;
	p->month   = ctm->tm_mon + 1;
	p->day     = ctm->tm_mday;
	p->hours   = ctm->tm_hour;
	p->minutes = ctm->tm_min;
	p->seconds = ctm->tm_sec;
}

/* Convert a DateTimeNumber from UTC to local time */
void icmDateTimeNumber_tolocal(icmDateTimeNumber *d, icmDateTimeNumber *s) {
	time_t cclk = 0, dclk;
	struct tm *ctm;
	
	/* Since not all systems support the timegm() or mkgmtime() function, */
	/* there is no standard way to convert a UTC tm to time_t, */
	/* so we use an offset trick that makes use of gmtime() and mktime(). */

	/* Transfer UTC icmDateTimeNumber to tm */
	cclk = time(NULL);
	ctm = localtime(&cclk);		/* Get the pointer */

	ctm->tm_year  = s->year - 1900;
	ctm->tm_mon   = s->month - 1;
	ctm->tm_mday  = s->day;
	ctm->tm_hour  = s->hours;   
	ctm->tm_min   = s->minutes; 
	ctm->tm_sec   = s->seconds;
	ctm->tm_isdst = -1;
	/* Convert time as if local time to time_t */
	if ((cclk = mktime(ctm)) == (time_t)-1) {
		d->year    = 1900;		/* Can't be represented by time_t */
		d->month   = 1;
		d->day     = 1;
		d->hours   = 0;
		d->minutes = 0;
		d->seconds = 0;

	} else {
		ctm = gmtime(&cclk);		/* time_t to UTC */
		dclk = cclk - mktime(ctm);	/* Compute adjustment of local to time_t */
		cclk += dclk;				/* Adjust from local to UTC */
		ctm = localtime(&cclk);		/* Convert from UTC to local */
	
		d->year    = ctm->tm_year + 1900;
		d->month   = ctm->tm_mon + 1;
		d->day     = ctm->tm_mday;
		d->hours   = ctm->tm_hour;
		d->minutes = ctm->tm_min;
		d->seconds = ctm->tm_sec;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Return the number of bytes needed to write this tag */
static unsigned int icmDateTime_get_size(
	icmDateTime *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);		/* 8 bytes for tag and padding */
	len = sat_add(len, 12);		/* 12 bytes for Date & Time */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmDateTime_read(
	icmDateTime *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv;
	char *bp, *buf;

	if (len < 20) {
		return icm_err(icp, 1,"icmDateTime_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmDateTime_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmDateTime_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmDateTime_read: Wrong tag type for icmDateTime");
	}
	bp += 8;	/* Skip padding */

	/* Read the time and date from buffer */
	if((rv = read_DateTimeNumber(p->icp, &p->date, bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmDateTime_read: Corrupted DateTime");
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmDateTime_write(
	icmDateTime *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmDateTime_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmDateTime_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmDateTime_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	if ((rv = write_DateTimeNumber(&p->date, bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmDateTime_write: write_DateTime() failed");
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmDateTime_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmDateTime_dump(
	icmDateTime *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"DateTime:\n");
	op->printf(op,"  Date = %s\n", icmDateTimeNumber2str(&p->date));
}

/* Allocate variable sized data elements */
static int icmDateTime_allocate(
	icmDateTime *p
) {
	/* Nothing to do */
	return 0;
}

/* Free all storage in the object */
static void icmDateTime_delete(
	icmDateTime *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmDateTime(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmDateTime, icSigDateTimeType)
	if (p == NULL)
		return NULL;

	icmDateTimeNumber_setcur(&p->date);	/* Default to current date and time */

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmLut object */

/* Check if the matrix is non-zero */
static int icmLut_nu_matrix(
	icmLut *p		/* Pointer to Lut object */
) {
	int i, j;
	
	for (j = 0; j < 3; j++) {		/* Rows */
		for (i = 0; i < 3; i++) {	/* Columns */
			if (   (i == j && p->e[j][i] != 1.0)
			    || (i != j && p->e[j][i] != 0.0))
				return 1;
		}
	}
	return 0;
}

/* return the locations of the minimum and */
/* maximum values of the given channel, in the clut */
static void icmLut_min_max(
	icmLut *p,		/* Pointer to Lut object */
	double *minp,	/* Return position of min/max */
	double *maxp,
	int chan		/* Channel, -1 for average of all */
) {
	double *tp;
	double minv, maxv;	/* Values */
	unsigned int e, ee, f;
	int gc[MAX_CHAN];	/* Grid coordinate */

	minv = 1e6;
	maxv = -1e6;

	for (e = 0; e < p->inputChan; e++)
		gc[e] = 0;	/* init coords */

	/* Search the whole table */
	for (tp = p->clutTable, e = 0; e < p->inputChan; tp += p->outputChan) {
		double v;
		if (chan == -1) {
			for (v = 0.0, f = 0; f < p->outputChan; f++)
				v += tp[f];
		} else {
			v = tp[chan];
		}
		if (v < minv) {
			minv = v;
			for (ee = 0; ee < p->inputChan; ee++)
				minp[ee] = gc[ee]/(p->clutPoints-1.0);
		}
		if (v > maxv) {
			maxv = v;
			for (ee = 0; ee < p->inputChan; ee++)
				maxp[ee] = gc[ee]/(p->clutPoints-1.0);
		}

		/* Increment coord */
		for (e = 0; e < p->inputChan; e++) {
			if (++gc[e] < p->clutPoints)
				break;	/* No carry */
			gc[e] = 0;
		}
	}
}

/* Convert XYZ throught Luts matrix */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static int icmLut_lookup_matrix(
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[outputChan] in ICC order - see Table 39 in 6.5.5 */
double *in		/* Input array[inputChan] */
) {
	double t0,t1;	/* Take care if out == in */
	t0     = p->e[0][0] * in[0] + p->e[0][1] * in[1] + p->e[0][2] * in[2];
	t1     = p->e[1][0] * in[0] + p->e[1][1] * in[1] + p->e[1][2] * in[2];
	out[2] = p->e[2][0] * in[0] + p->e[2][1] * in[1] + p->e[2][2] * in[2];
	out[0] = t0;
	out[1] = t1;

	return 0;
}

/* Convert normalized numbers though this Luts per channel input tables. */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static int icmLut_lookup_input(
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[inputChan] */
) {
	int rv = 0;
	unsigned int ix, n;
	double inputEnt_1 = (double)(p->inputEnt-1);
	double *table = p->inputTable;

	if (p->inputEnt == 0) {		/* Hmm. */
		for (n = 0; n < p->inputChan; n++)
			out[n] = in[n];
	} else {
		/* Use linear interpolation */
		for (n = 0; n < p->inputChan; n++, table += p->inputEnt) {
			double val, w;
			val = in[n] * inputEnt_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > inputEnt_1) {
				val = inputEnt_1;
				rv |= 1;
			}
			ix = (unsigned int)floor(val);		/* Grid coordinate */
			if (ix > (p->inputEnt-2))
				ix = (p->inputEnt-2);
			w = val - (double)ix;		/* weight */
			val = table[ix];
			out[n] = val + w * (table[ix+1] - val);
		}
	}
	return rv;
}

/* Convert normalized numbers though this Luts multi-dimensional table. */
/* using multi-linear interpolation. */
static int icmLut_lookup_clut_nl(
/* Return 0 on success, 1 if clipping occured, 2 on other error */
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	icc *icp = p->icp;
	int rv = 0;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	double *gw, GW[1 << 8];		/* weight for each grid cube corner */

	if (p->inputChan <= 8) {
		gw = GW;				/* Use stack allocation */
	} else {
		if ((gw = (double *) icp->al->malloc(icp->al, sat_mul((1 << p->inputChan), sizeof(double)))) == NULL) {
			return icm_err(icp, 2,"icmLut_lookup_clut: malloc() failed");
		}
	}

	/* We are using an multi-linear (ie. Trilinear for 3D input) interpolation. */
	/* The implementation here uses more multiplies that some other schemes, */
	/* (for instance, see "Tri-Linear Interpolation" by Steve Hill, */
	/* Graphics Gems IV, page 521), but has less involved bookeeping, */
	/* needs less local storage for intermediate output values, does fewer */
	/* output and intermediate value reads, and fp multiplies are fast on */
	/* todays processors! */

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int e;
		double clutPoints_1 = (double)(p->clutPoints-1);
		int    clutPoints_2 = p->clutPoints-2;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			unsigned int x;
			double val;
			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= 1;
			}
			x = (unsigned int)floor(val);	/* Grid coordinate */
			if (x > clutPoints_2)
				x = clutPoints_2;
			co[e] = val - (double)x;	/* 1.0 - weight */
			gp += x * p->dinc[e];		/* Add index offset for base of cube */
		}
	}
	/* Compute corner weights needed for interpolation */
	{
		unsigned int e;
		int i, g = 1;
		gw[0] = 1.0;
		for (e = 0; e < p->inputChan; e++) {
			for (i = 0; i < g; i++) {
				gw[g+i] = gw[i] * co[e];
				gw[i] *= (1.0 - co[e]);
			}
			g *= 2;
		}
	}
	/* Now compute the output values */
	{
		int i;
		unsigned int f;
		double w = gw[0];
		double *d = gp + p->dcube[0];
		for (f = 0; f < p->outputChan; f++)			/* Base of cube */
			out[f] = w * d[f];
		for (i = 1; i < (1 << p->inputChan); i++) {	/* For all other corners of cube */
			w = gw[i];				/* Strength reduce */
			d = gp + p->dcube[i];
			for (f = 0; f < p->outputChan; f++)
				out[f] += w * d[f];
		}
	}
	if (gw != GW)
		icp->al->free(icp->al, (void *)gw);
	return rv;
}

/* Convert normalized numbers though this Luts multi-dimensional table */
/* using simplex interpolation. */
static int icmLut_lookup_clut_sx(
/* Return 0 on success, 1 if clipping occured, 2 on other error */
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	int rv = 0;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	int    si[MAX_CHAN];		/* co[] Sort index, [0] = smallest */

	/* We are using a simplex (ie. tetrahedral for 3D input) interpolation. */
	/* This method is more appropriate for XYZ/RGB/CMYK input spaces, */

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int e;
		double clutPoints_1 = (double)(p->clutPoints-1);
		int    clutPoints_2 = p->clutPoints-2;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			unsigned int x;
			double val;
			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= 1;
			}
			x = (unsigned int)floor(val);		/* Grid coordinate */
			if (x > clutPoints_2)
				x = clutPoints_2;
			co[e] = val - (double)x;	/* 1.0 - weight */
			gp += x * p->dinc[e];		/* Add index offset for base of cube */
		}
	}
#ifdef NEVER
	/* Do selection sort on coordinates, smallest to largest. */
	{
		int e, f;
		for (e = 0; e < p->inputChan; e++)
			si[e] = e;						/* Initial unsorted indexes */
		for (e = 0; e < (p->inputChan-1); e++) {
			double cosn;
			cosn = co[si[e]];				/* Current smallest value */
			for (f = e+1; f < p->inputChan; f++) {	/* Check against rest */
				int tt;
				tt = si[f];
				if (cosn > co[tt]) {
					si[f] = si[e]; 			/* Exchange */
					si[e] = tt;
					cosn = co[tt];
				}
			}
		}
	}
#else
	/* Do insertion sort on coordinates, smallest to largest. */
	{
		int f, vf;
		unsigned int e;
		double v;
		for (e = 0; e < p->inputChan; e++)
			si[e] = e;						/* Initial unsorted indexes */

		for (e = 1; e < p->inputChan; e++) {
			f = e;
			v = co[si[f]];
			vf = f;
			while (f > 0 && co[si[f-1]] > v) {
				si[f] = si[f-1];
				f--;
			}
			si[f] = vf;
		}
	}
#endif
	/* Now compute the weightings, simplex vertices and output values */
	{
		unsigned int e, f;
		double w;		/* Current vertex weight */

		w = 1.0 - co[si[p->inputChan-1]];		/* Vertex at base of cell */
		for (f = 0; f < p->outputChan; f++)
			out[f] = w * gp[f];

		for (e = p->inputChan-1; e > 0; e--) {	/* Middle verticies */
			w = co[si[e]] - co[si[e-1]];
			gp += p->dinc[si[e]];				/* Move to top of cell in next largest dimension */
			for (f = 0; f < p->outputChan; f++)
				out[f] += w * gp[f];
		}

		w = co[si[0]];
		gp += p->dinc[si[0]];		/* Far corner from base of cell */
		for (f = 0; f < p->outputChan; f++)
			out[f] += w * gp[f];
	}
	return rv;
}

/* Convert normalized numbers though this Luts per channel output tables. */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static int icmLut_lookup_output(
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[outputChan] */
double *in		/* Input array[outputChan] */
) {
	int rv = 0;
	unsigned int ix, n;
	double outputEnt_1 = (double)(p->outputEnt-1);
	double *table = p->outputTable;

	if (p->outputEnt == 0) {		/* Hmm. */
		for (n = 0; n < p->outputChan; n++)
			out[n] = in[n];
	} else {
		/* Use linear interpolation */
		for (n = 0; n < p->outputChan; n++, table += p->outputEnt) {
			double val, w;
			val = in[n] * outputEnt_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > outputEnt_1) {
				val = outputEnt_1;
				rv |= 1;
			}
			ix = (unsigned int)floor(val);		/* Grid coordinate */
			if (ix > (p->outputEnt-2))
				ix = (p->outputEnt-2);
			w = val - (double)ix;		/* weight */
			val = table[ix];
			out[n] = val + w * (table[ix+1] - val);
		}
	}
	return rv;
}

/* ----------------------------------------------- */
/* Tune a single interpolated value. Based on lookup_clut functions (above) */

/* Helper function to fine tune a single value interpolation */
/* Return 0 on success, 1 if input clipping occured, 2 if output clipping occured */
int icmLut_tune_value_nl(
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	icc *icp = p->icp;
	int rv = 0;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	double *gw, GW[1 << 8];		/* weight for each grid cube corner */
	double cout[MAX_CHAN];		/* Current output value */

	if (p->inputChan <= 8) {
		gw = GW;				/* Use stack allocation */
	} else {
		if ((gw = (double *) icp->al->malloc(icp->al, sat_mul((1 << p->inputChan), sizeof(double)))) == NULL) {
			return icm_err(icp, 2,"icmLut_lookup_clut: malloc() failed");
		}
	}

	/* We are using an multi-linear (ie. Trilinear for 3D input) interpolation. */
	/* The implementation here uses more multiplies that some other schemes, */
	/* (for instance, see "Tri-Linear Interpolation" by Steve Hill, */
	/* Graphics Gems IV, page 521), but has less involved bookeeping, */
	/* needs less local storage for intermediate output values, does fewer */
	/* output and intermediate value reads, and fp multiplies are fast on */
	/* todays processors! */

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int e;
		double clutPoints_1 = (double)(p->clutPoints-1);
		int    clutPoints_2 = p->clutPoints-2;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			unsigned int x;
			double val;
			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= 1;
			}
			x = (unsigned int)floor(val);	/* Grid coordinate */
			if (x > clutPoints_2)
				x = clutPoints_2;
			co[e] = val - (double)x;	/* 1.0 - weight */
			gp += x * p->dinc[e];		/* Add index offset for base of cube */
		}
	}
	/* Compute corner weights needed for interpolation */
	{
		unsigned int e;
		int i, g = 1;
		gw[0] = 1.0;
		for (e = 0; e < p->inputChan; e++) {
			for (i = 0; i < g; i++) {
				gw[g+i] = gw[i] * co[e];
				gw[i] *= (1.0 - co[e]);
			}
			g *= 2;
		}
	}
	/* Now compute the current output value, and distribute the correction */
	{
		int i;
		unsigned int f;
		double w, *d, ww = 0.0;
		for (f = 0; f < p->outputChan; f++)
			cout[f] = 0.0;
		for (i = 0; i < (1 << p->inputChan); i++) {	/* For all other corners of cube */
			w = gw[i];				/* Strength reduce */
			ww += w * w;			/* Sum of weights squared */
			d = gp + p->dcube[i];
			for (f = 0; f < p->outputChan; f++)
				cout[f] += w * d[f];
		}

		/* We distribute the correction needed in proportion to the */
		/* interpolation weighting, so the biggest correction is to the */
		/* closest vertex. */

		for (f = 0; f < p->outputChan; f++)
			cout[f] = (out[f] - cout[f])/ww;	/* Amount to distribute */

		for (i = 0; i < (1 << p->inputChan); i++) {	/* For all other corners of cube */
			w = gw[i];				/* Strength reduce */
			d = gp + p->dcube[i];
			for (f = 0; f < p->outputChan; f++) {
				d[f] += w * cout[f];			/* Apply correction */
				if (d[f] < 0.0) {
					d[f] = 0.0;
					rv |= 2;
				} else if (d[f] > 1.0) {
					d[f] = 1.0;
					rv |= 2;
				} 
			}
		}
	}

	if (gw != GW)
		icp->al->free(icp->al, (void *)gw);
	return rv;
}

/* Helper function to fine tune a single value interpolation */
/* Return 0 on success, 1 if input clipping occured, 2 if output clipping occured */
int icmLut_tune_value_sx(
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	int rv = 0;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	int    si[MAX_CHAN];		/* co[] Sort index, [0] = smallest */

	/* We are using a simplex (ie. tetrahedral for 3D input) interpolation. */
	/* This method is more appropriate for XYZ/RGB/CMYK input spaces, */

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int e;
		double clutPoints_1 = (double)(p->clutPoints-1);
		int    clutPoints_2 = p->clutPoints-2;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			unsigned int x;
			double val;
			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= 1;
			}
			x = (unsigned int)floor(val);		/* Grid coordinate */
			if (x > clutPoints_2)
				x = clutPoints_2;
			co[e] = val - (double)x;	/* 1.0 - weight */
			gp += x * p->dinc[e];		/* Add index offset for base of cube */
		}
	}
	/* Do insertion sort on coordinates, smallest to largest. */
	{
		int f, vf;
		unsigned int e;
		double v;
		for (e = 0; e < p->inputChan; e++)
			si[e] = e;						/* Initial unsorted indexes */

		for (e = 1; e < p->inputChan; e++) {
			f = e;
			v = co[si[f]];
			vf = f;
			while (f > 0 && co[si[f-1]] > v) {
				si[f] = si[f-1];
				f--;
			}
			si[f] = vf;
		}
	}
	/* Now compute the current output value, and distribute the correction */
	{
		unsigned int e, f;
		double w, ww = 0.0;		/* Current vertex weight, sum of weights squared */
		double cout[MAX_CHAN];		/* Current output value */
		double *ogp = gp;		/* Pointer to grid cube base */

		w = 1.0 - co[si[p->inputChan-1]];		/* Vertex at base of cell */
		ww += w * w;							/* Sum of weights squared */
		for (f = 0; f < p->outputChan; f++)
			cout[f] = w * gp[f];

		for (e = p->inputChan-1; e > 0; e--) {	/* Middle verticies */
			w = co[si[e]] - co[si[e-1]];
			ww += w * w;							/* Sum of weights squared */
			gp += p->dinc[si[e]];				/* Move to top of cell in next largest dimension */
			for (f = 0; f < p->outputChan; f++)
				cout[f] += w * gp[f];
		}

		w = co[si[0]];
		ww += w * w;							/* Sum of weights squared */
		gp += p->dinc[si[0]];					/* Far corner from base of cell */
		for (f = 0; f < p->outputChan; f++)
			cout[f] += w * gp[f];

		/* We distribute the correction needed in proportion to the */
		/* interpolation weighting, so the biggest correction is to the */
		/* closest vertex. */
		for (f = 0; f < p->outputChan; f++)
			cout[f] = (out[f] - cout[f])/ww;	/* Amount to distribute */

		gp = ogp;
		w = 1.0 - co[si[p->inputChan-1]];		/* Vertex at base of cell */
		for (f = 0; f < p->outputChan; f++) {
			gp[f] += w * cout[f];			/* Apply correction */
			if (gp[f] < 0.0) {
				gp[f] = 0.0;
				rv |= 2;
			} else if (gp[f] > 1.0) {
				gp[f] = 1.0;
				rv |= 2;
			}
		}

		for (e = p->inputChan-1; e > 0; e--) {	/* Middle verticies */
			w = co[si[e]] - co[si[e-1]];
			gp += p->dinc[si[e]];				/* Move to top of cell in next largest dimension */
			for (f = 0; f < p->outputChan; f++) {
				gp[f] += w * cout[f];			/* Apply correction */
				if (gp[f] < 0.0) {
					gp[f] = 0.0;
					rv |= 2;
				} else if (gp[f] > 1.0) {
					gp[f] = 1.0;
					rv |= 2;
				}
			}
		}

		w = co[si[0]];
		gp += p->dinc[si[0]];					/* Far corner from base of cell */
		for (f = 0; f < p->outputChan; f++) {
			gp[f] += w * cout[f];			/* Apply correction */
			if (gp[f] < 0.0) {
				gp[f] = 0.0;
				rv |= 2;
			} else if (gp[f] > 1.0) {
				gp[f] = 1.0;
				rv |= 2;
			}
		}
	}
	return rv;
}


/* ----------------------------------------------- */
/* Pseudo - Hilbert count sequencer */

/* This multi-dimensional count sequence is a distributed */
/* Gray code sequence, with direction reversal on every */
/* alternate power of 2 scale. */
/* It is intended to aid cache coherence in multi-dimensional */
/* regular sampling. It approximates the Hilbert curve sequence. */

/* Initialise, returns total usable count */
unsigned
psh_init(
psh *p,	/* Pointer to structure to initialise */
int      di,		/* Dimensionality */
unsigned int res,	/* Size per coordinate */
int co[]			/* Coordinates to initialise (May be NULL) */
) {
	int e;

	p->di = di;
	p->res = res;

	/* Compute bits */
	for (p->bits = 0; (1u << p->bits) < res; p->bits++)
		;

	/* Compute the total count mask */
	p->tmask = ((((unsigned)1) << (p->bits * di))-1);

	/* Compute usable count */
	p->count = 1;
	for (e = 0; e < di; e++)
		p->count *= res;

	p->ix = 0;

	if (co != NULL) {
		for (e = 0; e < di; e++)
			co[e] = 0;
	}

	return p->count;
}

/* Reset the counter */
void
psh_reset(
psh *p	/* Pointer to structure */
) {
	p->ix = 0;
}

/* Increment pseudo-hilbert coordinates */
/* Return non-zero if count rolls over to 0 */
int
psh_inc(
psh *p,	/* Pointer to structure */
int co[]		/* Coordinates to return */
) {
	int di = p->di;
	unsigned int res = p->res;
	unsigned int bits = p->bits;
	int e;

	do {
		unsigned int b;
		int gix;	/* Gray code index */
		
		p->ix = (p->ix + 1) & p->tmask;

		gix = p->ix ^ (p->ix >> 1);		/* Convert to gray code index */
	
		for (e = 0; e < di; e++) 
			co[e] = 0;
		
		for (b = 0; b < bits; b++) {	/* Distribute bits */
			if (b & 1) {
				for (e = di-1; e >= 0; e--)  {		/* In reverse order */
					co[e] |= (gix & 1) << b;
					gix >>= 1;
				}
			} else {
				for (e = 0; e < di; e++)  {			/* In normal order */
					co[e] |= (gix & 1) << b;
					gix >>= 1;
				}
			}
		}
	
		/* Convert from Gray to binary coordinates */
		for (e = 0; e < di; e++)  {
			unsigned int sh, tv;

			for(sh = 1, tv = co[e];; sh <<= 1) {
				unsigned ptv = tv;
				tv ^= (tv >> sh);
				if (ptv <= 1 || sh == 16)
					break;
			}
			if (tv >= res)	/* Dumbo filter - increment again if outside cube range */
				break;
			co[e] = tv;
		}

	} while (e < di);
	
	return (p->ix == 0);
}

/* ------------------------------------------------------- */

#ifndef COUNTERS_H

/* Macros for a multi-dimensional counter. */

/* Declare the counter name nn, maximum di mxdi, dimensions di, & count */
/* This counter can have each dimension range clipped */

#define FCOUNT(nn, mxdi, di) 				\
	int nn[mxdi];			/* counter value */				\
	int nn##_di = (di);		/* Number of dimensions */		\
	int nn##_stt[mxdi];		/* start count value */			\
	int nn##_res[mxdi]; 	/* last count +1 */				\
	int nn##_e				/* dimension index */

#define FRECONF(nn, start, endp1) 							\
	for (nn##_e = 0; nn##_e < nn##_di; nn##_e++) {			\
		nn##_stt[nn##_e] = (start);	/* start count value */	\
		nn##_res[nn##_e] = (endp1); /* last count +1 */		\
	}

/* Set the counter value to 0 */
#define FC_INIT(nn) 								\
{													\
	for (nn##_e = 0; nn##_e < nn##_di; nn##_e++)	\
		nn[nn##_e] = nn##_stt[nn##_e];				\
	nn##_e = 0;										\
}

/* Increment the counter value */
#define FC_INC(nn)									\
{													\
	for (nn##_e = 0; nn##_e < nn##_di; nn##_e++) {	\
		nn[nn##_e]++;								\
		if (nn[nn##_e] < nn##_res[nn##_e])			\
			break;	/* No carry */					\
		nn[nn##_e] = nn##_stt[nn##_e];				\
	}												\
}

/* After increment, expression is TRUE if counter is done */
#define FC_DONE(nn)								\
	(nn##_e >= nn##_di)

#endif /* COUNTERS_H */

#define CLIP_MARGIN 0.005		/* Margine to allow before reporting clipping = 0.5% */

/* NOTE that ICM_CLUT_SET_FILTER turns out to be not very useful, */
/* as it can result in reversals. Could #ifdef out the code ?? */

/* Helper function to set multiple Lut tables simultaneously. */
/* Note that these tables all have to be compatible in */
/* having the same configuration and resolution. */
/* Set errc and return error number in underlying icc */
/* Set warnc if there is clipping in the output values: */
/*  1 = input table, 2 = main clut, 3 = clut midpoint, 4 = midpoint interp, 5 = output table */
/* Note that clutfunc in[] value has "index under", ie: */
/* at ((int *)in)[-chan-1], and for primary grid is simply the */
/* grid index (ie. 5,3,8), and for the center of cells grid, is */
/* the -index-1, ie. -6,-3,-8 */
int icmSetMultiLutTables(
	int ntables,						/* Number of tables to be set, 1..n */
	icmLut **pp,						/* Pointer to array of Lut objects */
	int     flags,						/* Setting flags */
	void   *cbctx,						/* Opaque callback context pointer value */
	icColorSpaceSignature insig, 		/* Input color space */
	icColorSpaceSignature outsig, 		/* Output color space */
	void (*infunc)(void *cbctx, double *out, double *in),
							/* Input transfer function, inspace->inspace' (NULL = default) */
							/* Will be called ntables times for each input grid value */
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
) {
	icmLut *p, *pn;				/* Pointer to 0'th nd tn'th Lut object */
	icc *icp;					/* Pointer to common icc */
	int tn;
	unsigned int e, f, i, n;
	double **clutTable2 = NULL;		/* Cell center values for ICM_CLUT_SET_APXLS */ 
	double *clutTable3 = NULL;		/* Vertex smoothing radius values [ntables] per entry */
	int dinc3[MAX_CHAN];			/* Dimensional increment through clut3 (in doubles) */
	int dcube3[1 << MAX_CHAN];		/* Hyper cube offsets throught clut3 (in doubles) */
	int ii[MAX_CHAN];		/* Index value */
	psh counter;			/* Pseudo-Hilbert counter */
//	double _iv[4 * MAX_CHAN], *iv = &_iv[MAX_CHAN], *ivn;	/* Real index value/table value */
	int maxchan;			/* Actual max of input and output */
	double *_iv, *iv, *ivn;	/* Real index value/table value */
	double imin[MAX_CHAN], imax[MAX_CHAN];
	double omin[MAX_CHAN], omax[MAX_CHAN];
	int def_apxls_gmin[MAX_CHAN], def_apxls_gmax[MAX_CHAN];
	void (*ifromindex)(double *out, double *in);	/* Index to input color space function */
	void (*itoentry)(double *out, double *in);		/* Input color space to entry function */
	void (*ifromentry)(double *out, double *in);	/* Entry to input color space function */
	void (*otoentry)(double *out, double *in);		/* Output colorspace to table value function */
	void (*ofromentry)(double *out, double *in);	/* Table value to output color space function */
	int clip = 0;

	/* Check that everything is OK to proceed */
	if (ntables < 1 || ntables > MAX_CHAN) {
		if (ntables >= 1) {
			icp = pp[0]->icp;
			return icm_err(icp, 1,"icmSetMultiLutTables has illegal number of tables %d",ntables);
		} else {
			/* Can't write error message anywhere */
			return 1;
		}
	}

	p   = pp[0];
	icp = p->icp;

	for (tn = 1; tn < ntables; tn++) {
		if (pp[tn]->icp != icp) {
			return icm_err(icp, 1,"icmSetMultiLutTables Tables base icc is different");
		}
		if (pp[tn]->ttype != p->ttype) {
			return icm_err(icp, 1,"icmSetMultiLutTables Tables have different Tage Type");
		}

		if (pp[tn]->inputChan != p->inputChan) {
			return icm_err(icp, 1,"icmSetMultiLutTables Tables have different inputChan");
		}
		if (pp[tn]->outputChan != p->outputChan) {
			return icm_err(icp, 1,"icmSetMultiLutTables Tables have different outputChan");
		}
		if (pp[tn]->clutPoints != p->clutPoints) {
			return icm_err(icp, 1,"icmSetMultiLutTables Tables have different clutPoints");
		}
	}

	{
	icmGNRep irep = icmTagType2GNRep(insig, p->ttype);		/* 8/16 bit encoding */
	icmGNRep orep = icmTagType2GNRep(outsig, p->ttype);

	if (icmGetNormFunc(&ifromindex, NULL, insig, icmGNtoCS, icmGNV2, irep) != 0) { 
		return icm_err(icp, 1,"icmLut_set_tables index to input colorspace function lookup failed");
	}

	if (icmGetNormFunc(&itoentry, NULL, insig, icmGNtoNorm, icmGNV2, irep) != 0) { 
		return icm_err(icp, 1,"icmLut_set_tables input colorspace to table entry function lookup failed");
	}

	if (icmGetNormFunc(&ifromentry, NULL, insig, icmGNtoCS, icmGNV2, irep) != 0) { 
		return icm_err(icp, 1,"icmLut_set_tables table entry to input colorspace function lookup failed");
	}

	if (icmGetNormFunc(&otoentry, NULL, outsig, icmGNtoNorm, icmGNV2, orep) != 0) { 
		return icm_err(icp, 1,"icmLut_set_tables output colorspace to table entry function lookup failed");
	}
	if (icmGetNormFunc(&ofromentry, NULL, outsig, icmGNtoCS, icmGNV2, orep) != 0) { 
		return icm_err(icp, 1,"icmLut_set_tables table entry to output colorspace function lookup failed");
	}
	}

	/* Allocate an array to hold the input and output values. */
	/* It needs to be able to hold di "index under valus as in[], */
	/* and ntables ICM_CLUT_SET_FILTER values as out[], so we assume maxchan >= di */
	maxchan = p->inputChan > p->outputChan ? p->inputChan : p->outputChan;
	if ((_iv = (double *) icp->al->malloc(icp->al, sizeof(double) * maxchan * (ntables+1)))
	                                                                              == NULL) {
		return icm_err(icp, 2,"icmLut_read: malloc() failed");
	}
	iv = _iv + maxchan;		/* Allow for "index under" and smoothing radius values */

	/* Setup input table value min-max */
	if (inmin == NULL || inmax == NULL) {
#ifdef SYMETRICAL_DEFAULT_LAB_RANGE	/* Symetrical default range. */
		/* We are assuming V2 Lab16 encoding, since this is a lut16type that always uses */
		/* this encoding */
		if (insig == icSigLabData) { /* Special case Lab */
			double mn[3], mx[3];
			/* This is to ensure that Lab 100,0,0 maps exactly to a clut grid point. */
			/* This should work well if there is an odd grid resolution, */
			/* and icclib is being used, as input lookup will */
			/* be computed using floating point, so that the CLUT input value */
			/* 0.5 can be represented exactly. */
			/* Because the symetric range will cause slight clipping, */
			/* only do it if the input table has sufficient resolution */
			/* to represent the clipping faithfuly. */
			if (p->inputEnt >= 64) {
				if (p->ttype == icSigLut8Type) {
					mn[0] =   0.0, mn[1] = mn[2] = -127.0;
					mx[0] = 100.0, mx[1] = mx[2] =  127.0;
				} else {
					mn[0] =   0.0, mn[1] = mn[2] = -127.0 - 255.0/256.0;
					mx[0] = 100.0, mx[1] = mx[2] =  127.0 + 255.0/256.0;
				}
				itoentry(imin, mn);	/* Convert from input color space to table representation */
				itoentry(imax, mx);
			} else {
				for (e = 0; e < p->inputChan; e++) {
					imin[e] = 0.0;
					imax[e] = 1.0;
				}
			}
		} else
#endif
		{
			for (e = 0; e < p->inputChan; e++) {
				imin[e] = 0.0;		/* We are assuming this is true for all other color spaces. */
				imax[e] = 1.0;
			}
		}
	} else {
		itoentry(imin, inmin);	/* Convert from input color space to table representation */
		itoentry(imax, inmax);
	}

	/* Setup output table value min-max */
	if (clutmin == NULL || clutmax == NULL) {
#ifdef SYMETRICAL_DEFAULT_LAB_RANGE
		/* This really isn't doing much, since the full range encoding doesn't need */
		/* any adjustment to map a*b* 0 to an integer value. */
		/* We are tweaking the 16 bit L* = 100 to the last index into */
		/* the output table, which may help its accuracy slightly. */
		/* We are assuming V2 Lab16 encoding, since this is a lut16type that always uses */
		/* this encoding */
		if (outsig == icSigLabData) { /* Special case Lab */
			double mn[3], mx[3];
			/* The output of the CLUT will be an 8 or 16 bit value, and we want to */
			/* adjust the range so that the input grid point holding the white */
			/* point can encode 0.0 exactly. */
			/* Note that in the case of the a & b values, the range equates to */
			/* normalised 0.0 .. 1.0, since 0 can be represented exactly in it. */
			if (p->outputEnt >= 64) {
				if (p->ttype == icSigLut8Type) {
					mn[0] =   0.0, mn[1] = mn[2] = -128.0;
					mx[0] = 100.0, mx[1] = mx[2] = 127.0;
				} else {
					mn[0] =   0.0, mn[1] = mn[2] = -128.0;
					mx[0] = 100.0, mx[1] = mx[2] =  (65535.0 * 255.0)/65280.0 - 128.0;
				}
				otoentry(omin, mn);/* Convert from output color space to table representation */
				otoentry(omax, mx);

			} else {
				for (e = 0; e < p->inputChan; e++) {
					omin[e] = 0.0;
					omax[e] = 1.0;
				}
			}
		} else
#endif
		{
			for (f = 0; f < p->outputChan; f++) {
				omin[f] = 0.0;		/* We are assuming this is true for all other color spaces. */
				omax[f] = 1.0;
			}
		}
	} else {
		otoentry(omin, clutmin);/* Convert from output color space to table representation */
		otoentry(omax, clutmax);
	}

	/* Create the input table entry values */
	for (tn = 0; tn < ntables; tn++) {
		pn = pp[tn];
		for (n = 0; n < pn->inputEnt; n++) {
			double fv;
			fv = n/(pn->inputEnt-1.0);
			for (e = 0; e < pn->inputChan; e++)
				iv[e] = fv;

			ifromindex(iv,iv);			/* Convert from index value to input color space value */

			if (infunc != NULL)
				infunc(cbctx, iv, iv);	/* In colorspace -> input table -> In colorspace. */

			itoentry(iv,iv);			/* Convert from input color space value to table value */

			/* Expand used range to 0.0 - 1.0, and clip to legal values */
			/* Note that if the range is reduced, and clipping occurs, */
			/* then there should be enough resolution within the input */
			/* table, to represent the sharp edges of the clipping. */
			for (e = 0; e < pn->inputChan; e++) {
				double tt;
				tt = (iv[e] - imin[e])/(imax[e] - imin[e]);
				if (tt < 0.0) {
					DBGSLC(("iclip: tt = %f, iv = %f, omin = %f, omax = %f\n",tt,iv[e],omin[e],omax[e]));
					if (tt < -CLIP_MARGIN)
						clip = 1;
					tt = 0.0;
				} else if (tt > 1.0) {
					DBGSLC(("iclip: tt = %f, iv = %f, omin = %f, omax = %f\n",tt,iv[e],omin[e],omax[e]));
					if (tt > (1.0 + CLIP_MARGIN))
						clip = 1;
					tt = 1.0;
				}
				iv[e] = tt;
			}

			for (e = 0; e < pn->inputChan; e++) 		/* Input tables */
				pn->inputTable[e * pn->inputEnt + n] = iv[e];
		}
	}

	/* Allocate space for cell center value lookup */
	if (flags & ICM_CLUT_SET_APXLS) {
		if (apxls_gmin == NULL) {
			apxls_gmin = def_apxls_gmin;
			for (e = 0; e < p->inputChan; e++)
				apxls_gmin[e] = 0;
		}
		if (apxls_gmax == NULL) {
			apxls_gmax = def_apxls_gmax;
			for (e = 0; e < p->inputChan; e++)
				apxls_gmax[e] = p->clutPoints-1;
		}

		if ((clutTable2 = (double **) icp->al->calloc(icp->al,sizeof(double *), ntables)) == NULL) {
			icp->al->free(icp->al, _iv);
			return icm_err(icp, 1,"icmLut_set_tables malloc of cube center array failed");
		}
		for (tn = 0; tn < ntables; tn++) {
			if ((clutTable2[tn] = (double *) icp->al->calloc(icp->al,sizeof(double),
			                                               p->clutTable_size)) == NULL) {
				for (--tn; tn >= 0; tn--)
					icp->al->free(icp->al, clutTable2[tn]);
				icp->al->free(icp->al, _iv);
				icp->al->free(icp->al, clutTable2);
				return icm_err(icp, 1,"icmLut_set_tables malloc of cube center array failed");
			}
		}
	}

	/* Allocate space for smoothing radius values */
	if (flags & ICM_CLUT_SET_FILTER) {
		unsigned int j, g, size;

		/* Private: compute dimensional increment though clut3 */
		i = p->inputChan-1;
		dinc3[i--] = ntables;
		for (; i < p->inputChan; i--)
			dinc3[i] = dinc3[i+1] * p->clutPoints;
	
		/* Private: compute offsets from base of cube to other corners */
		for (dcube3[0] = 0, g = 1, j = 0; j < p->inputChan; j++) {
			for (i = 0; i < g; i++)
				dcube3[g+i] = dcube3[i] + dinc3[j];
			g *= 2;
		}

		if ((size = sat_mul(ntables, sat_pow(p->clutPoints,p->inputChan))) == UINT_MAX) {
			if (flags & ICM_CLUT_SET_APXLS) {
				for (tn = 0; tn < ntables; tn++)
					icp->al->free(icp->al, clutTable2[tn]);
			}
			icp->al->free(icp->al, clutTable2);
			icp->al->free(icp->al, _iv);
			return icm_err(icp, 1,"icmLut_alloc size overflow");
		}

		if ((clutTable3 = (double *) icp->al->calloc(icp->al,sizeof(double),
		                                               size)) == NULL) {
			if (flags & ICM_CLUT_SET_APXLS) {
				for (tn = 0; tn < ntables; tn++)
					icp->al->free(icp->al, clutTable2[tn]);
			}
			icp->al->free(icp->al, clutTable2);
			icp->al->free(icp->al, _iv);
			return icm_err(icp, 1,"icmLut_set_tables malloc of vertex smoothing value array failed");
		}
	}

	/* Create the multi-dimensional lookup table values */

	/* To make this clut function cache friendly, we use the pseudo-hilbert */
	/* count sequence. This keeps each point close to the last in the */
	/* multi-dimensional space. This is the point of setting multiple Luts at */ 
	/* once too - the assumption is that these tables are all related (different */
	/* gamut compressions for instance), and hence calling the clutfunc() with */
	/* close values will maximise reverse lookup cache hit rate. */

	psh_init(&counter, p->inputChan, p->clutPoints, ii);	/* Initialise counter */

	/* Itterate through all verticies in the grid */
	for (;;) {
		int ti, ti3;		/* Table indexes */
	
		for (ti = e = 0; e < p->inputChan; e++) { 	/* Input tables */
			ti += ii[e] * p->dinc[e];				/* Clut index */
			iv[e] = ii[e]/(p->clutPoints-1.0);		/* Vertex coordinates */
			iv[e] = iv[e] * (imax[e] - imin[e]) + imin[e]; /* Undo expansion to 0.0 - 1.0 */
			*((int *)&iv[-((int)e)-1]) = ii[e];		/* Trick to supply grid index in iv[] */
		}
	
		if (flags & ICM_CLUT_SET_FILTER) {
			for (ti3 = e = 0; e < p->inputChan; e++) 	/* Input tables */
				ti3 += ii[e] * dinc3[e];				/* Clut3 index */
		}
	
		DBGSL(("\nix %s\n",icmPiv(p->inputChan, ii)));
		DBGSL(("raw itv %s to iv'",icmPdv(p->inputChan, iv)));
		ifromentry(iv,iv);			/* Convert from table value to input color space */
		DBGSL((" %s\n",icmPdv(p->inputChan, iv)));
	
		/* Apply incolor -> outcolor function we want to represent for all tables */
		DBGSL(("iv: %s to ov'",icmPdv(p->inputChan, iv)));
		clutfunc(cbctx, iv, iv);
		DBGSL((" %s\n",icmPdv(p->outputChan, iv)));
	
		/* Save the results to the output tables */
		for (tn = 0, ivn = iv; tn < ntables; ivn += p->outputChan, tn++) {
			pn = pp[tn];
		
			DBGSL(("tn %d, ov' %s -> otv",tn,icmPdv(p->outputChan, ivn)));
			otoentry(ivn,ivn);			/* Convert from output color space value to table value */
			DBGSL((" %s\n  -> oval",icmPdv(p->outputChan, ivn)));
	
			/* Expand used range to 0.0 - 1.0, and clip to legal values */
			for (f = 0; f < pn->outputChan; f++) {
				double tt;
				tt = (ivn[f] - omin[f])/(omax[f] - omin[f]);
				if (tt < 0.0) {
					DBGSLC(("lclip: tt = %f, ivn= %f, omin = %f, omax = %f\n",tt,ivn[f],omin[f],omax[f]));
					if (tt < -CLIP_MARGIN)
						clip = 2;
					tt = 0.0;
				} else if (tt > 1.0) {
					DBGSLC(("lclip: tt = %f, ivn= %f, omin = %f, omax = %f\n",tt,ivn[f],omin[f],omax[f]));
					if (tt > (1.0 + CLIP_MARGIN))
						clip = 2;
					tt = 1.0;
				}
				ivn[f] = tt;
			}
		
			for (f = 0; f < pn->outputChan; f++) 	/* Output chans */
				pn->clutTable[ti + f] = ivn[f];
			DBGSL((" %s\n",icmPdv(pn->outputChan, ivn)));

			if (flags & ICM_CLUT_SET_FILTER) {
				clutTable3[ti3 + tn] = iv[-1 -tn];	/* Filter radiuses */
			}
		}
	
		/* Lookup cell center value if ICM_CLUT_SET_APXLS */
		if (clutTable2 != NULL) {

			for (e = 0; e < p->inputChan; e++) {
				if (ii[e] < apxls_gmin[e]
				 || ii[e] >= apxls_gmax[e])
					break;							/* Don't lookup outside least squares area */
				iv[e] = (ii[e] + 0.5)/(p->clutPoints-1.0);		/* Vertex coordinates + 0.5 */
				iv[e] = iv[e] * (imax[e] - imin[e]) + imin[e]; /* Undo expansion to 0.0 - 1.0 */
				*((int *)&iv[-((int)e)-1]) = -ii[e]-1;	/* Trick to supply -ve grid index in iv[] */
											    /* (Not this is only the base for +0.5 center) */
			}

			if (e >= p->inputChan) {	/* We're not on the last row */
		
				ifromentry(iv,iv);			/* Convert from table value to input color space */
			
				/* Apply incolor -> outcolor function we want to represent */
				clutfunc(cbctx, iv, iv);
			
				/* Save the results to the output tables */
				for (tn = 0, ivn = iv; tn < ntables; ivn += p->outputChan, tn++) {
					pn = pp[tn];
				
					otoentry(ivn,ivn);			/* Convert from output color space value to table value */
			
					/* Expand used range to 0.0 - 1.0, and clip to legal values */
					for (f = 0; f < pn->outputChan; f++) {
						double tt;
						tt = (ivn[f] - omin[f])/(omax[f] - omin[f]);
						if (tt < 0.0) {
							DBGSLC(("lclip: tt = %f, ivn= %f, omin = %f, omax = %f\n",tt,ivn[f],omin[f],omax[f]));
							if (tt < -CLIP_MARGIN)
								clip = 3;
							tt = 0.0;
						} else if (tt > 1.0) {
							DBGSLC(("lclip: tt = %f, ivn= %f, omin = %f, omax = %f\n",tt,ivn[f],omin[f],omax[f]));
							if (tt > (1.0 + CLIP_MARGIN))
								clip = 3;
							tt = 1.0;
						}
						ivn[f] = tt;
					}
				
					for (f = 0; f < pn->outputChan; f++) 	/* Output chans */
						clutTable2[tn][ti + f] = ivn[f];
				}
			}
		}

		/* Increment index within block (Reverse index significancd) */
		if (psh_inc(&counter, ii))
			break;
	}

#define APXLS_WHT 0.5
#define APXLS_DIFF_THRHESH 0.2
	/* Deal with cell center value, aproximate least squares adjustment. */
	/* Subtract some of the mean of the surrounding center values from each grid value. */
	/* Skip the range edges so that things like the white point or Video sync are not changed. */
	/* Avoid modifying the value if the difference between the */
	/* interpolated value and the current value is too great, */
	/* and there is the possibility of different color aliases. */
	if (clutTable2 != NULL) {
		int ti;				/* cube vertex table index */
		int ti2;			/* cube center table2 index */
		int ee;
		double cw = 1.0/(double)(1 << p->inputChan);		/* Weight for each cube corner */

		/* For each cell center point except last row because we access ii[e]+1 */  
		for (e = 0; e < p->inputChan; e++)
			ii[e] = apxls_gmin[e];	/* init coords */

		/* Compute linear interpolated value from center values */
		for (ee = 0; ee < p->inputChan;) {

			/* Compute base index for table2 */
			for (ti2 = e = 0; e < p->inputChan; e++)  	/* Input tables */
				ti2 += ii[e] * p->dinc[e];				/* Clut index */

			ti = ti2 + p->dcube[(1 << p->inputChan)-1];	/* +1 to each coord for vertex index */

			for (tn = 0; tn < ntables; tn++) {
				double mval[MAX_CHAN], vv;
				double maxd = 0.0;

				pn = pp[tn];
			
				/* Compute mean of center values */
				for (f = 0; f < pn->outputChan; f++) { 	/* Output chans */

					mval[f] = 0.0;
					for (i = 0; i < (1 << p->inputChan); i++) { /* For surrounding center values */
						mval[f] += clutTable2[tn][ti2 + p->dcube[i] + f];
					}
					mval[f] = pn->clutTable[ti + f] - mval[f] * cw;		/* Diff to mean */
					vv = fabs(mval[f]);
					if (vv > maxd)
						maxd = vv;
				}
			
				if (pn->outputChan <= 3 || maxd < APXLS_DIFF_THRHESH) {
					for (f = 0; f < pn->outputChan; f++) { 	/* Output chans */
				
						vv = pn->clutTable[ti + f] + APXLS_WHT * mval[f];
	
						/* Hmm. This is a bit crude. How do we know valid range is 0-1 ? */
						/* What about an ink limit ? */
						if (vv < 0.0) {
							vv = 0.0;
						} else if (vv > 1.0) {
							vv = 1.0;
						}
						pn->clutTable[ti + f] = vv;
					}
					DBGSL(("nix %s apxls ov %s\n",icmPiv(p->inputChan, ii), icmPdv(pn->outputChan, ivn)));
				}
			}

			/* Increment coord */
			for (ee = 0; ee < p->inputChan; ee++) {
				if (++ii[ee] < (apxls_gmax[ee]-1))		/* Stop short of upper row of clutTable2 */
					break;	/* No carry */
				ii[ee] = apxls_gmin[ee];
			}
		}

		/* Done with center values */
		for (tn = 0; tn < ntables; tn++)
			icp->al->free(icp->al, clutTable2[tn]);
		icp->al->free(icp->al, clutTable2);
	}

	/* Apply any smoothing in the clipped region to the resulting clutTable */
	/* !!! should avoid smoothing outside apxls_gmin[e] & apxls_gmax[e] region !!! */
	if (clutTable3 != NULL) {
		double *clutTable1;		/* Copy of current unfilted values */
		FCOUNT(cc, MAX_CHAN, p->inputChan);   /* Surrounding counter */
		
		if ((clutTable1 = (double *) icp->al->calloc(icp->al,sizeof(double),
		                                               p->clutTable_size)) == NULL) {
			icp->al->free(icp->al, clutTable3);
			icp->al->free(icp->al, _iv);
			return icm_err(icp, 1,"icmLut_set_tables malloc of grid copy failed");
		}

		for (tn = 0; tn < ntables; tn++) {
			int aa;
			int ee;
			int ti, ti3;		/* Table indexes */

			pn = pp[tn];

			/* For each pass */
			for (aa = 0; aa < 2; aa++) {
	
				/* Copy current values */
				memcpy(clutTable1, pn->clutTable, sizeof(double) * pn->clutTable_size);
	
				/* Filter each point */
				for (e = 0; e < pn->inputChan; e++)
					ii[e] = 0;	/* init coords */
	
				/* Compute linear interpolated error to actual cell center value */
				for (ee = 0; ee < pn->inputChan;) {
					double rr;		/* Filter radius */
					int ir;			/* Integer radius */
					double tw;		/* Total weight */
	
					/* Compute base index for this cell */
					for (ti3 = ti = e = 0; e < pn->inputChan; e++) {  	/* Input tables */
						ti += ii[e] * pn->dinc[e];				/* Clut index */
						ti3 += ii[e] * dinc3[e];				/* Clut3 index */
					}
					rr = clutTable3[ti3 + tn] * (pn->clutPoints-1.0);
					ir = (int)floor(rr + 0.5);			/* Don't bother unless 1/2 over vertex */
	
					if (ir < 1)
						goto next_vert;
	
					//FRECONF(cc, -ir, ir + 1);		/* Set size of surroundign grid */
	
					/* Clip scanning cube to be within grid */
					for (e = 0; e < pn->inputChan; e++) {
						int cr = ir;
						if ((ii[e] - ir) < 0)
							cr = ii[e];
						if ((ii[e] + ir) >= pn->clutPoints)
							cr = pn->clutPoints -1 -ii[e];
	
						cc_stt[e] = -cr;
						cc_res[e] = cr + 1;
					}
	
					for (f = 0; f < pn->outputChan; f++)
						pn->clutTable[ti + f] = 0.0;
					tw = 0.0;
	
					FC_INIT(cc)
					for (tw = 0.0; !FC_DONE(cc);) {
						double r;
						int tti;
	
						/* Radius of this cell */
						for (r = 0.0, tti = e = 0; e < pn->inputChan; e++) {
							int ix;
							r += cc[e] * cc[e];
							tti += (ii[e] + cc[e]) * p->dinc[e];
						}
						r = sqrt(r);
	
						if (r <= rr && e >= pn->inputChan) {
							double w = (rr - r)/rr;		/* Triangle weighting */
							w = sqrt(w);
							for (f = 0; f < pn->outputChan; f++) 
								pn->clutTable[ti+f] += w * clutTable1[tti + f];
							tw += w;
						}
						FC_INC(cc);
					}
					for (f = 0; f < pn->outputChan; f++) { 
						double vv = pn->clutTable[ti+f] / tw;
						if (vv < 0.0) {
							vv = 0.0;
						} else if (vv > 1.0) {
							vv = 1.0;
						}
						pn->clutTable[ti+f] = vv;
					}
	
					/* Increment coord */
				next_vert:;
					for (ee = 0; ee < pn->inputChan; ee++) {
						if (++ii[ee] < (pn->clutPoints-1))		/* Don't go through upper edge */
							break;	/* No carry */
						ii[ee] = 0;
					}
				}	/* Next grid point to filter */
			}	/* Next pass */
		}	/* Next table */

		icp->al->free(icp->al, clutTable1);
		icp->al->free(icp->al, clutTable3);
	}

	/* Create the 1D output table entry values */
	for (tn = 0; tn < ntables; tn++) {
		pn = pp[tn];
		for (n = 0; n < pn->outputEnt; n++) {
			double fv;
			fv = n/(pn->outputEnt-1.0);
			for (f = 0; f < pn->outputChan; f++)
				iv[f] = fv;

			/* Undo expansion to 0.0 - 1.0 */
			for (f = 0; f < pn->outputChan; f++) 		/* Output tables */
				iv[f] = iv[f] * (omax[f] - omin[f]) + omin[f];

			ofromentry(iv,iv);			/* Convert from table value to output color space value */

			if (outfunc != NULL)
				outfunc(cbctx, iv, iv);	/* Out colorspace -> output table -> out colorspace. */

			otoentry(iv,iv);			/* Convert from output color space value to table value */

			/* Clip to legal values */
			for (f = 0; f < pn->outputChan; f++) {
				double tt;
				tt = iv[f];
				if (tt < 0.0) {
					DBGSLC(("oclip: tt = %f\n",tt));
					if (tt < -CLIP_MARGIN)
						clip = 5;
					tt = 0.0;
				} else if (tt > 1.0) {
					DBGSLC(("oclip: tt = %f\n",tt));
					if (tt > (1.0 + CLIP_MARGIN))
						clip = 5;
					tt = 1.0;
				}
				iv[f] = tt;
			}

			for (f = 0; f < pn->outputChan; f++) 		/* Input tables */
				pn->outputTable[f * pn->outputEnt + n] = iv[f];
		}
	}

	icp->al->free(icp->al, _iv);

	icp->warnc = 0;
	if (clip) {
		DBGSLC(("Returning clip status = %d\n",clip));
		icp->warnc = clip;
	}
	
	return 0;
}

/* Helper function to initialize a Lut tables contents */
/* from supplied transfer functions. */
/* Set errc and return error number */
/* Set warnc if there is clipping in the output values */
static int icmLut_set_tables (
icmLut *p,							/* Pointer to Lut object */
int     flags,						/* Setting flags */
void   *cbctx,						/* Opaque callback context pointer value */
icColorSpaceSignature insig, 		/* Input color space */
icColorSpaceSignature outsig, 		/* Output color space */
void (*infunc)(void *cbcntx, double *out, double *in),
								/* Input transfer function, inspace->inspace' (NULL = default) */
double *inmin, double *inmax,		/* Maximum range of inspace' values (NULL = default) */
void (*clutfunc)(void *cbctx, double *out, double *in),
								/* inspace' -> outspace' transfer function */
double *clutmin, double *clutmax,	/* Maximum range of outspace' values (NULL = default) */
void (*outfunc)(void *cbctx, double *out, double *in),
									/* Output transfer function, outspace'->outspace (NULL = deflt) */
int *apxls_gmin, int *apxls_gmax	/* If not NULL, the grid indexes not to be affected */
									/* by ICM_CLUT_SET_APXLS, defaulting to 0..>clutPoints-1 */
) {
	struct _icmLut *pp[3];
	
	/* Simply call the multiple table function with one table */
	pp[0] = p;
	return icmSetMultiLutTables(1, pp, flags, 
	                            cbctx, insig, outsig,
	                            infunc,
	                            inmin, inmax,
	                            clutfunc,
	                            clutmin, clutmax,
	                            outfunc,
	                            apxls_gmin, apxls_gmax);
}

/* - - - - - - - - - - - - - - - - */
/* Return the number of bytes needed to write this tag */
static unsigned int icmLut_get_size(
	icmLut *p
) {
	unsigned int len = 0;

	if (p->ttype == icSigLut8Type) {
		len = sat_add(len, 48);			/* tag and header */
		len = sat_add(len, sat_mul3(1, p->inputChan, p->inputEnt));
		len = sat_add(len, sat_mul3(1, p->outputChan, sat_pow(p->clutPoints,p->inputChan)));
		len = sat_add(len, sat_mul3(1, p->outputChan, p->outputEnt));
	} else {
		len = sat_add(len, 52);			/* tag and header */
		len = sat_add(len, sat_mul3(2, p->inputChan, p->inputEnt));
		len = sat_add(len, sat_mul3(2, p->outputChan, sat_pow(p->clutPoints,p->inputChan)));
		len = sat_add(len, sat_mul3(2, p->outputChan, p->outputEnt));
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmLut_read(
	icmLut *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, j, g, size;
	char *bp, *buf;

	if (len < 4) {
		return icm_err(icp, 1,"icmLut_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmLut_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmLut_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	p->ttype = (icTagTypeSignature)read_SInt32Number(bp);
	if (p->ttype != icSigLut8Type && p->ttype != icSigLut16Type) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmLut_read: Wrong tag type for icmLut");
	}

	if (p->ttype == icSigLut8Type) {
		if (len < 48) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmLut_read: Tag too small to be legal");
		}
	} else {
		if (len < 52) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmLut_read: Tag too small to be legal");
		}
	}

	/* Read in the info common to 8 and 16 bit Lut */
	p->inputChan = read_UInt8Number(bp+8);
	p->outputChan = read_UInt8Number(bp+9);
	p->clutPoints = read_UInt8Number(bp+10);

	if (icp->allowclutPoints256 && p->clutPoints == 0)		/* Special case */
		p->clutPoints = 256;

	/* Read 3x3 transform matrix */
	for (j = 0; j < 3; j++) {		/* Rows */
		for (i = 0; i < 3; i++) {	/* Columns */
			p->e[j][i] = read_S15Fixed16Number(bp + 12 + ((j * 3 + i) * 4));
		}
	}
	/* Read 16 bit specific stuff */
	if (p->ttype == icSigLut8Type) {
		p->inputEnt  = 256;	/* By definition */
		p->outputEnt = 256;	/* By definition */
		bp = buf+48;
	} else {
		p->inputEnt  = read_UInt16Number(bp+48);
		p->outputEnt = read_UInt16Number(bp+50);
		bp = buf+52;
	}

	/* Sanity check tag size. This protects against */
	/* subsequent integer overflows involving the dimensions. */
	if ((size = icmLut_get_size(p)) == UINT_MAX
	 || size > len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmLut_read: Tag wrong size for contents");
	}

	/* Sanity check the dimensions and resolution values agains limits, */
	/* allocate space for them and generate internal offset tables. */
	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read the input tables */
	size = (p->inputChan * p->inputEnt);
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1)
			p->inputTable[i] = read_DCS8Number(bp);
	} else {
		for (i = 0; i < size; i++, bp += 2)
			p->inputTable[i] = read_DCS16Number(bp);
	}

	/* Read the clut table */
	size = (p->outputChan * sat_pow(p->clutPoints,p->inputChan));
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1)
			p->clutTable[i] = read_DCS8Number(bp);
	} else {
		for (i = 0; i < size; i++, bp += 2)
			p->clutTable[i] = read_DCS16Number(bp);
	}

	/* Read the output tables */
	size = (p->outputChan * p->outputEnt);
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1)
			p->outputTable[i] = read_DCS8Number(bp);
	} else {
		for (i = 0; i < size; i++, bp += 2)
			p->outputTable[i] = read_DCS16Number(bp);
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmLut_write(
	icmLut *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i,j;
	unsigned int len, size;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmLut_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmLut_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmLut_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write the info common to 8 and 16 bit Lut */
	if ((rv = write_UInt8Number(p->inputChan, bp+8)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmLut_write: write_UInt8Number() failed");
	}
	if ((rv = write_UInt8Number(p->outputChan, bp+9)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmLut_write: write_UInt8Number() failed");
	}
	
	if (icp->allowclutPoints256 && p->clutPoints == 256) {
		if ((rv = write_UInt8Number(0, bp+10)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmLut_write: write_UInt8Number() failed");
		}
	} else {
		if ((rv = write_UInt8Number(p->clutPoints, bp+10)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmLut_write: write_UInt8Number() failed");
		}
	}

	write_UInt8Number(0, bp+11);		/* Set padding to 0 */

	/* Write 3x3 transform matrix */
	for (j = 0; j < 3; j++) {		/* Rows */
		for (i = 0; i < 3; i++) {	/* Columns */
			if ((rv = write_S15Fixed16Number(p->e[j][i],bp + 12 + ((j * 3 + i) * 4))) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmLut_write: write_S15Fixed16Number() failed");
			}
		}
	}

	/* Write 16 bit specific stuff */
	if (p->ttype == icSigLut8Type) {
		if (p->inputEnt != 256 || p->outputEnt != 256) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmLut_write: 8 bit Input and Output tables must be 256 entries");
		}
		bp = buf+48;
	} else {
		if (p->inputEnt > 4096 || p->outputEnt > 4096) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmLut_write: 16 bit Input and Output tables must each be less than 4096 entries");
		}
		if ((rv = write_UInt16Number(p->inputEnt, bp+48)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmLut_write: write_UInt16Number() failed");
		}
		if ((rv = write_UInt16Number(p->outputEnt, bp+50)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmLut_write: write_UInt16Number() failed");
		}
		bp = buf+52;
	}

	/* Write the input tables */
	size = (p->inputChan * p->inputEnt);
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1) {
			if ((rv = write_DCS8Number(p->inputTable[i], bp)) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmLut_write: inputTable write_DCS8Number() failed");
			}
		}
	} else {
		for (i = 0; i < size; i++, bp += 2) {
			if ((rv = write_DCS16Number(p->inputTable[i], bp)) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmLut_write: inputTable write_DCS16Number(%.8f) failed",p->inputTable[i]);
			}
		}
	}

	/* Write the clut table */
	size = (p->outputChan * sat_pow(p->clutPoints,p->inputChan));
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1) {
			if ((rv = write_DCS8Number(p->clutTable[i], bp)) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmLut_write: clutTable write_DCS8Number() failed");
			}
		}
	} else {
		for (i = 0; i < size; i++, bp += 2) {
			if ((rv = write_DCS16Number(p->clutTable[i], bp)) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmLut_write: clutTable write_DCS16Number(%.8f) failed",p->clutTable[i]);
			}
		}
	}

	/* Write the output tables */
	size = (p->outputChan * p->outputEnt);
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1) {
			if ((rv = write_DCS8Number(p->outputTable[i], bp)) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmLut_write: outputTable write_DCS8Number() failed");
			}
		}
	} else {
		for (i = 0; i < size; i++, bp += 2) {
			if ((rv = write_DCS16Number(p->outputTable[i], bp)) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmLut_write: outputTable write_DCS16Number(%.8f) failed",p->outputTable[i]);
			}
		}
	}

	/* Write buffer to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmLut_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmLut_dump(
	icmLut *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	if (p->ttype == icSigLut8Type) {
		op->printf(op,"Lut8:\n");
	} else {
		op->printf(op,"Lut16:\n");
	}
	op->printf(op,"  Input Channels = %u\n",p->inputChan);
	op->printf(op,"  Output Channels = %u\n",p->outputChan);
	op->printf(op,"  CLUT resolution = %u\n",p->clutPoints);
	op->printf(op,"  Input Table entries = %u\n",p->inputEnt);
	op->printf(op,"  Output Table entries = %u\n",p->outputEnt);
	op->printf(op,"  XYZ matrix =  %.8f, %.8f, %.8f\n",p->e[0][0],p->e[0][1],p->e[0][2]);
	op->printf(op,"                %.8f, %.8f, %.8f\n",p->e[1][0],p->e[1][1],p->e[1][2]);
	op->printf(op,"                %.8f, %.8f, %.8f\n",p->e[2][0],p->e[2][1],p->e[2][2]);

	if (verb >= 2) {
		unsigned int i, j, size;
		unsigned int ii[MAX_CHAN];	/* maximum no of input channels */

		op->printf(op,"  Input table:\n");
		for (i = 0; i < p->inputEnt; i++) {
			op->printf(op,"    %3u: ",i);
			for (j = 0; j < p->inputChan; j++)
				op->printf(op," %1.10f",p->inputTable[j * p->inputEnt + i]);
			op->printf(op,"\n");
		}

		op->printf(op,"\n  CLUT table:\n");
		if (p->inputChan > MAX_CHAN) {
			op->printf(op,"  !!Can't dump > %d input channel CLUT table!!\n",MAX_CHAN);
		} else {
			size = (p->outputChan * sat_pow(p->clutPoints,p->inputChan));
			for (j = 0; j < p->inputChan; j++)
				ii[j] = 0;
			for (i = 0; i < size;) {
				unsigned int k;
				/* Print table entry index */
				op->printf(op,"   ");
				for (j = p->inputChan-1; j < p->inputChan; j--)
					op->printf(op," %2u",ii[j]);
				op->printf(op,":");
				/* Print table entry contents */
				for (k = 0; k < p->outputChan; k++, i++)
					op->printf(op," %1.10f",p->clutTable[i]);
				op->printf(op,"\n");
			
				for (j = 0; j < p->inputChan; j++) { /* Increment index */
					ii[j]++;
					if (ii[j] < p->clutPoints)
						break;	/* No carry */
					ii[j] = 0;
				}
			}
		}

		op->printf(op,"\n  Output table:\n");
		for (i = 0; i < p->outputEnt; i++) {
			op->printf(op,"    %3u: ",i);
			for (j = 0; j < p->outputChan; j++)
				op->printf(op," %1.10f",p->outputTable[j * p->outputEnt + i]);
			op->printf(op,"\n");
		}

	}
}

/* Sanity check the input & output dimensions, and */
/* allocate variable sized data elements, and */
/* generate internal dimension offset tables */
static int icmLut_allocate(
	icmLut *p
) {
	unsigned int i, j, g, size;
	icc *icp = p->icp;

	/* Sanity check, so that dinc[] comp. won't fail */
	if (p->inputChan < 1) {
		return icm_err(icp, 1,"icmLut_alloc: Can't handle %d input channels",p->inputChan);
	}

	if (p->inputChan > MAX_CHAN) {
		return icm_err(icp, 1,"icmLut_alloc: Can't handle > %d input channels",MAX_CHAN);
	}

	if (p->outputChan > MAX_CHAN) {
		return icm_err(icp, 1,"icmLut_alloc: Can't handle > %d output channels",MAX_CHAN);
	}

	if ((size = sat_mul(p->inputChan, p->inputEnt)) == UINT_MAX) {
		return icm_err(icp, 1,"icmLut_alloc size overflow");
	}
	if (size != p->inputTable_size) {
		if (ovr_mul(size, sizeof(double))) {
			return icm_err(icp, 1,"icmLut_alloc: size overflow");
		}
		if (p->inputTable != NULL)
			icp->al->free(icp->al, p->inputTable);
		if ((p->inputTable = (double *) icp->al->calloc(icp->al,size, sizeof(double))) == NULL) {
			return icm_err(icp, 2,"icmLut_alloc: calloc() of Lut inputTable data failed");
		}
		p->inputTable_size = size;
	}
	if ((size = sat_mul(p->outputChan, sat_pow(p->clutPoints,p->inputChan))) == UINT_MAX) {
		return icm_err(icp, 1,"icmLut_alloc size overflow");
	}
	if (size != p->clutTable_size) {
		if (ovr_mul(size, sizeof(double))) {
			return icm_err(icp, 1,"icmLut_alloc: size overflow");
		}
		if (p->clutTable != NULL)
			icp->al->free(icp->al, p->clutTable);
		if ((p->clutTable = (double *) icp->al->calloc(icp->al,size, sizeof(double))) == NULL) {
			return icm_err(icp, 2,"icmLut_alloc: calloc() of Lut clutTable data failed");
		}
		p->clutTable_size = size;
	}
	if ((size = sat_mul(p->outputChan, p->outputEnt)) == UINT_MAX) {
		return icm_err(icp, 1,"icmLut_alloc size overflow");
	}
	if (size != p->outputTable_size) {
		if (ovr_mul(size, sizeof(double))) {
			return icm_err(icp, 1,"icmLut_alloc: size overflow");
		}
		if (p->outputTable != NULL)
			icp->al->free(icp->al, p->outputTable);
		if ((p->outputTable = (double *) icp->al->calloc(icp->al,size, sizeof(double))) == NULL) {
			return icm_err(icp, 2,"icmLut_alloc: calloc() of Lut outputTable data failed");
		}
		p->outputTable_size = size;
	}

	/* Private: compute dimensional increment though clut */
	/* Note that first channel varies least rapidly. */
	i = p->inputChan-1;
	p->dinc[i--] = p->outputChan;
	for (; i < p->inputChan; i--)
		p->dinc[i] = p->dinc[i+1] * p->clutPoints;

	/* Private: compute offsets from base of cube to other corners */
	for (p->dcube[0] = 0, g = 1, j = 0; j < p->inputChan; j++) {
		for (i = 0; i < g; i++)
			p->dcube[g+i] = p->dcube[i] + p->dinc[j];
		g *= 2;
	}
	
	return 0;
}

/* Free all storage in the object */
static void icmLut_delete(
	icmLut *p
) {
	icc *icp = p->icp;
	int i;

	if (p->inputTable != NULL)
		icp->al->free(icp->al, p->inputTable);
	if (p->clutTable != NULL)
		icp->al->free(icp->al, p->clutTable);
	if (p->outputTable != NULL)
		icp->al->free(icp->al, p->outputTable);
	for (i = 0; i < p->inputChan; i++)
		icmTable_delete_bwd(icp, &p->rit[i]);
	for (i = 0; i < p->outputChan; i++)
		icmTable_delete_bwd(icp, &p->rot[i]);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmLut(
	icc *icp
) {
	int i,j;
	ICM_TTYPE_ALLOCINIT_LEGACY(icmLut, icSigLut16Type)

	if (p == NULL)
		return NULL;

	/* Lookup methods */
	p->nu_matrix      = icmLut_nu_matrix;
	p->min_max        = icmLut_min_max;
	p->lookup_matrix  = icmLut_lookup_matrix;
	p->lookup_input   = icmLut_lookup_input;
	p->lookup_clut_nl = icmLut_lookup_clut_nl;
	p->lookup_clut_sx = icmLut_lookup_clut_sx;
	p->lookup_output  = icmLut_lookup_output;

	/* Set method */
	p->set_tables = icmLut_set_tables;
	p->tune_value = icmLut_tune_value_sx;		/* Default to most likely simplex */

	/* Set matrix to reasonable default */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++) {
			if (i == j)
				p->e[i][j] = 1.0;
			else
				p->e[i][j] = 0.0;
		}

	/* Init lookups to non-dangerous values */
	for (i = 0; i < MAX_CHAN; i++)
		p->dinc[i] = 0;

	for (i = 0; i < (1 << MAX_CHAN); i++)
		p->dcube[i] = 0;

	for (i = 0; i < MAX_CHAN; i++) {
		p->rit[i].inited = 0;
		p->rot[i].inited = 0;
	}

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* Measurement */

/* Return the number of bytes needed to write this tag */
static unsigned int icmMeasurement_get_size(
	icmMeasurement *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);		/* 8 bytes for tag and padding */
	len = sat_add(len, 4);		/* 4 for standard observer */
	len = sat_add(len, 12);		/* 12 for XYZ of measurement backing */
	len = sat_add(len, 4);		/* 4 for measurement geometry */
	len = sat_add(len, 4);		/* 4 for measurement flare */
	len = sat_add(len, 4);		/* 4 for standard illuminant */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmMeasurement_read(
	icmMeasurement *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv;
	char *bp, *buf;

	if (len < 36) {
		return icm_err(icp, 1,"icmMeasurement_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmMeasurement_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmMeasurement_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmMeasurement_read: Wrong tag type for icmMeasurement");
	}

	/* Read the encoded standard observer */
	p->observer = (icStandardObserver)read_SInt32Number(bp + 8);

	/* Read the XYZ values for measurement backing */
	if ((rv = read_XYZNumber(&p->backing, bp+12)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmMeasurement: read_XYZNumber error");
	}

	/* Read the encoded measurement geometry */
	p->geometry = (icMeasurementGeometry)read_SInt32Number(bp + 24);

	/* Read the proportion of flare  */
	p->flare = read_U16Fixed16Number(bp + 28);

	/* Read the encoded standard illuminant */
	p->illuminant = (icIlluminant)read_SInt32Number(bp + 32);

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmMeasurement_write(
	icmMeasurement *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmMeasurement_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmMeasurement_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmMeasurement_write, type: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write the encoded standard observer */
	if ((rv = write_SInt32Number((int)p->observer, bp + 8)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmMeasurementa_write, observer: write_SInt32Number() failed");
	}

	/* Write the XYZ values for measurement backing */
	if ((rv = write_XYZNumber(&p->backing, bp+12)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmMeasurement, backing: write_XYZNumber error");
	}

	/* Write the encoded measurement geometry */
	if ((rv = write_SInt32Number((int)p->geometry, bp + 24)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmMeasurementa_write, geometry: write_SInt32Number() failed");
	}

	/* Write the proportion of flare */
	if ((rv = write_U16Fixed16Number(p->flare, bp + 28)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmMeasurementa_write, flare: write_U16Fixed16Number() failed");
	}

	/* Write the encoded standard illuminant */
	if ((rv = write_SInt32Number((int)p->illuminant, bp + 32)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmMeasurementa_write, illuminant: write_SInt32Number() failed");
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmMeasurement_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmMeasurement_dump(
	icmMeasurement *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"Measurement:\n");
	op->printf(op,"  Standard Observer = %s\n", icmStandardObserver2str(p->observer));
	op->printf(op,"  XYZ for Measurement Backing = %s\n", icmXYZNumber_and_Lab2str(&p->backing));
	op->printf(op,"  Measurement Geometry = %s\n", icmMeasurementGeometry2str(p->geometry));
	op->printf(op,"  Measurement Flare = %5.1f%%\n", p->flare * 100.0);
	op->printf(op,"  Standard Illuminant = %s\n", icmIlluminant2str(p->illuminant));
}

/* Allocate variable sized data elements */
static int icmMeasurement_allocate(
	icmMeasurement *p
) {
	/* Nothing to do */
	return 0;
}

/* Free all storage in the object */
static void icmMeasurement_delete(
	icmMeasurement *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmMeasurement(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmMeasurement, icSigMeasurementType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */

/* Named color structure read/write support */
static int read_NamedColorVal(
	icmNamedColorVal *p,
	char *bp,
	char *end,
	icColorSpaceSignature pcs,		/* Header Profile Connection Space */
	unsigned int ndc				/* Number of device corrds */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int mxl;	/* Max possible string length */
	int rv;

	if (bp > end) {
		return icm_err(icp, 1,"icmNamedColorVal_read: Data too short to read");
	}
	mxl = (end - bp) < 32 ? (end - bp) : 32;
	if ((rv = check_null_string(bp,mxl)) == 1) {
		return icm_err(icp, 1,"icmNamedColorVal_read: Root name string not terminated");
	}
	/* Haven't checked if rv == 2 is legal or not */
	strcpy(p->root, bp);
	bp += strlen(p->root) + 1;
	if (bp > end || ndc > (end - bp)) {
		return icm_err(icp, 1,"icmNamedColorVal_read: Data too short to read device coords");
	}
	for (i = 0; i < ndc; i++) {
		p->deviceCoords[i] = read_DCS8Number(bp);
		bp += 1;
	}
	return 0;
}

static int read_NamedColorVal2(
	icmNamedColorVal *p,
	char *bp,
	char *end,
	icColorSpaceSignature pcs,		/* Header Profile Connection Space */
	unsigned int ndc				/* Number of device coords */
) {
	int rv;
	icc *icp = p->icp;
	unsigned int i;

	if (bp > end
	 || (32 + 6) > (end - bp)
	 || ndc > (end - bp - 32 - 6)/2) {
		return icm_err(icp, 1,"icmNamedColorVal2_read: Data too short to read");
	}
	if ((rv = check_null_string(bp,32)) == 1) {
		return icm_err(icp, 1,"icmNamedColorVal2_read: Root name string not terminated");
	}
	memmove((void *)p->root,(void *)(bp + 0),32);
	switch(pcs) {
		case icSigXYZData:
			read_PCSNumber(icp, icSigXYZData, p->pcsCoords, bp+32);
			break;
	   	case icSigLabData:
			/* namedColor2Type retains legacy Lab encoding */
			read_PCSNumber(icp, icmSigLabV2Data, p->pcsCoords, bp+32);
			break;
		default:
			return 1;		/* Unknown PCS */
	}
	for (i = 0; i < ndc; i++)
		p->deviceCoords[i] = read_DCS16Number(bp + 32 + 6 + 2 * i);
	return 0;
}

static int write_NamedColorVal(
	icmNamedColorVal *p,
	char *d,
	icColorSpaceSignature pcs,		/* Header Profile Connection Space */
	unsigned int ndc				/* Number of device corrds */
) {
	icc *icp = p->icp;
	unsigned int i;
	int rv;

	if ((rv = check_null_string(p->root,32)) == 1) {
		return icm_err(icp, 1,"icmNamedColorVal_write: Root string names is unterminated");
	}
	strcpy(d, p->root);
	d += strlen(p->root) + 1;
	for (i = 0; i < ndc; i++) {
		if ((rv = write_DCS8Number(p->deviceCoords[i], d)) != 0) {
			return icm_err(icp, 1,"icmNamedColorVal_write: write of device coord failed");
		}
		d += 1;
	}
	return 0;
}

static int write_NamedColorVal2(
	icmNamedColorVal *p,
	char *bp,
	icColorSpaceSignature pcs,		/* Header Profile Connection Space */
	unsigned int ndc				/* Number of device coords */
) {
	icc *icp = p->icp;
	unsigned int i;
	int rv;

	if ((rv = check_null_string(p->root,32)) == 1) {
		return icm_err(icp, 1,"icmNamedColorVal2_write: Root string names is unterminated");
	}
	rv = 0;
	memmove((void *)(bp + 0),(void *)p->root,32);
	switch(pcs) {
		case icSigXYZData:
			rv |= write_PCSNumber(icp, icSigXYZData, p->pcsCoords, bp+32);
			break;
	   	case icSigLabData:
			/* namedColor2Type retains legacy Lab encoding */
			rv |= write_PCSNumber(icp, icmSigLabV2Data, p->pcsCoords, bp+32);
			break;
		default:
			return icm_err(icp, 1,"icmNamedColorVal2_write: Unknown PCS");
	}
	if (rv) {
		return icm_err(icp, 1,"icmNamedColorVal2_write: write of PCS coord failed");
	}
	for (i = 0; i < ndc; i++) {
		if ((rv = write_DCS16Number(p->deviceCoords[i], bp + 32 + 6 + 2 * i)) != 0) {
			return icm_err(icp, 1,"icmNamedColorVal2_write: write of device coord failed");
		}
	}
	return 0;
}

/* - - - - - - - - - - - */
/* icmNamedColor object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmNamedColor_get_size(
	icmNamedColor *p
) {
	unsigned int len = 0;
	if (p->ttype == icSigNamedColorType) {
		unsigned int i;
		len = sat_add(len, 8);			/* 8 bytes for tag and padding */
		len = sat_add(len, 4);			/* 4 for vendor specific flags */
		len = sat_add(len, 4);			/* 4 for count of named colors */
		len = sat_add(len, strlen(p->prefix) + 1); /* prefix of color names */
		len = sat_add(len, strlen(p->suffix) + 1); /* suffix of color names */
		for (i = 0; i < p->count; i++) {
			len = sat_add(len, strlen(p->data[i].root) + 1); /* color names */
			len = sat_add(len, p->nDeviceCoords * 1);	/* bytes for each named color */
		}
	} else {	/* Named Color 2 */
		len = sat_add(len, 8);			/* 8 bytes for tag and padding */
		len = sat_add(len, 4);			/* 4 for vendor specific flags */
		len = sat_add(len, 4);			/* 4 for count of named colors */
		len = sat_add(len, 4);			/* 4 for number of device coords */
		len = sat_add(len, 32);			/* 32 for prefix of color names */
		len = sat_add(len, 32);			/* 32 for suffix of color names */
		len = sat_add(len, sat_mul(p->count, (32 + 6 + p->nDeviceCoords * 2)));
		                                /* bytes for each named color */
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmNamedColor_read(
	icmNamedColor *p,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	unsigned int i;
	char *bp, *buf, *end;
	int rv;

	if (len < 4) {
		return icm_err(icp, 1,"icmNamedColor_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmNamedColor_read: malloc() failed");
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmNamedColor_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	p->ttype = (icTagTypeSignature)read_SInt32Number(bp);
	if (p->ttype != icSigNamedColorType && p->ttype != icSigNamedColor2Type) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmNamedColor_read: Wrong tag type for icmNamedColor");
	}

	if (p->ttype == icSigNamedColorType) {
		if (len < 16) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_read: Tag too small to be legal");
		}
		/* Make sure that the number of device coords in known */
		p->nDeviceCoords = icmCSSig2nchan(icp->header->colorSpace);
		if (p->nDeviceCoords > MAX_CHAN) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_read: Can't handle more than %d device channels",MAX_CHAN);
		}

	} else {	/* icmNC2 */
		if (len < 84) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_read: Tag too small to be legal");
		}
	}

	/* Read vendor specific flag */
	p->vendorFlag = read_UInt32Number(bp+8);

	/* Read count of named colors */
	p->count = read_UInt32Number(bp+12);

	if (p->ttype == icSigNamedColorType) {
		unsigned int mxl;	/* Max possible string length */
		bp = bp + 16;

		/* Prefix for each color name */
		if (bp > end) {
			return icm_err(icp, 1,"icmNamedColor_read: Data too short to read");
		}
		mxl = (end - bp) < 32 ? (end - bp) : 32;
		if ((rv = check_null_string(bp,mxl)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_read: Color prefix is not null terminated");
		}
		/* Haven't checked if rv == 2 is legal or not */
		strcpy(p->prefix, bp);
		bp += strlen(p->prefix) + 1;
	
		/* Suffix for each color name */
		if (bp > end) {
			return icm_err(icp, 1,"icmNamedColor_read: Data too short to read");
		}
		mxl = (end - bp) < 32 ? (end - bp) : 32;
		if ((rv = check_null_string(bp,mxl)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_read: Color suffix is not null terminated");
		}
		/* Haven't checked if rv == 2 is legal or not */
		strcpy(p->suffix, bp);
		bp += strlen(p->suffix) + 1;
	
		if ((rv = p->allocate(p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	
		/* Read all the data from the buffer */
		for (i = 0; i < p->count; i++) {
			if ((rv = read_NamedColorVal(p->data+i, bp, end, icp->header->pcs, p->nDeviceCoords)) != 0) {
				icp->al->free(icp->al, buf);
				return rv;
			}
			bp += strlen(p->data[i].root) + 1;
			bp += p->nDeviceCoords * 1;
		}
	} else {  /* icmNC2 */
		/* Number of device coords per color */
		p->nDeviceCoords = read_UInt32Number(bp+16);
		if (p->nDeviceCoords > MAX_CHAN) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_read: Can't handle more than %d device channels",MAX_CHAN);
		}
	
		/* Prefix for each color name */
		memmove((void *)p->prefix, (void *)(bp + 20), 32);
		if ((rv = check_null_string(p->prefix,32)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_read: Color prefix is not null terminated");
		}
	
		/* Suffix for each color name */
		memmove((void *)p->suffix, (void *)(bp + 52), 32);
		if ((rv = check_null_string(p->suffix,32)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_read: Color suffix is not null terminated");
		}
	
		if ((rv = p->allocate(p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	
		/* Read all the data from the buffer */
		bp = bp + 84;
		for (i = 0; i < p->count; i++) {
			if ((rv = read_NamedColorVal2(p->data+i, bp, end, icp->header->pcs, p->nDeviceCoords)) != 0) {
				icp->al->free(icp->al, buf);
				return rv;
			}
			bp += 32 + 6 + p->nDeviceCoords * 2;
		}
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmNamedColor_write(
	icmNamedColor *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmNamedColor_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmNamedColor_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmNamedColor_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write vendor specific flag */
	if ((rv = write_UInt32Number(p->vendorFlag, bp+8)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmNamedColor_write: write_UInt32Number() failed");
	}

	/* Write count of named colors */
	if ((rv = write_UInt32Number(p->count, bp+12)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmNamedColor_write: write_UInt32Number() failed");
	}

	if (p->ttype == icSigNamedColorType) {
		bp = bp + 16;
	
		/* Prefix for each color name */
		if ((rv = check_null_string(p->prefix,32)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_write: Color prefix is not null terminated");
		}
		strcpy(bp, p->prefix);
		bp += strlen(p->prefix) + 1;
	
		/* Suffix for each color name */
		if ((rv = check_null_string(p->suffix,32)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_write: Color sufix is not null terminated");
		}
		strcpy(bp, p->suffix);
		bp += strlen(p->suffix) + 1;
	
		/* Write all the data to the buffer */

		for (i = 0; i < p->count; i++) {
			if ((rv = write_NamedColorVal(p->data+i, bp, icp->header->pcs, p->nDeviceCoords)) != 0) {
				icp->al->free(icp->al, buf);
				return rv;
			}
			bp += strlen(p->data[i].root) + 1;
			bp += p->nDeviceCoords * 1;
		}
	} else {	/* icmNC2 */
		/* Number of device coords per color */
		if ((rv = write_UInt32Number(p->nDeviceCoords, bp+16)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmNamedColor_write: write_UInt32Number() failed");
		}
	
		/* Prefix for each color name */
		if ((rv = check_null_string(p->prefix,32)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_write: Color prefix is not null terminated");
		}
		memmove((void *)(bp + 20), (void *)p->prefix, 32);
	
		/* Suffix for each color name */
		if ((rv = check_null_string(p->suffix,32)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmNamedColor_write: Color sufix is not null terminated");
		}
		memmove((void *)(bp + 52), (void *)p->suffix, 32);
	
		/* Write all the data to the buffer */
		bp = bp + 84;
		for (i = 0; i < p->count; i++, bp += (32 + 6 + p->nDeviceCoords * 2)) {
			if ((rv = write_NamedColorVal2(p->data+i, bp, icp->header->pcs, p->nDeviceCoords)) != 0) {
				icp->al->free(icp->al, buf);
				return rv;
			}
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmNamedColor_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmNamedColor_dump(
	icmNamedColor *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icc *icp = p->icp;
	if (verb <= 0)
		return;

	if (p->ttype == icSigNamedColorType)
		op->printf(op,"NamedColor:\n");
	else
		op->printf(op,"NamedColor2:\n");
	op->printf(op,"  Vendor Flag = 0x%x\n",p->vendorFlag);
	op->printf(op,"  No. colors  = %u\n",p->count);
	op->printf(op,"  No. dev. coords = %u\n",p->nDeviceCoords);
	op->printf(op,"  Name prefix = '%s'\n",p->prefix);
	op->printf(op,"  Name suffix = '%s'\n",p->suffix);
	if (verb >= 2) {
		unsigned int i, n;
		icmNamedColorVal *vp;
		for (i = 0; i < p->count; i++) {
			vp = p->data + i;
			op->printf(op,"    Color %lu:\n",i);
			op->printf(op,"      Name root = '%s'\n",vp->root);

			if (p->ttype == icSigNamedColor2Type) {
				switch(icp->header->pcs) {
					case icSigXYZData:
							op->printf(op,"      XYZ = %.8f, %.8f, %.8f\n",
							        vp->pcsCoords[0],vp->pcsCoords[1],vp->pcsCoords[2]);
						break;
			    	case icSigLabData:
							op->printf(op,"      Lab = %f, %f, %f\n",
							        vp->pcsCoords[0],vp->pcsCoords[1],vp->pcsCoords[2]);
						break;
					default:
							op->printf(op,"      Unexpected PCS\n");
						break;
				}
			}
			if (p->nDeviceCoords > 0) {
				op->printf(op,"      Device Coords = ");
				for (n = 0; n < p->nDeviceCoords; n++) {
					if (n > 0)
						op->printf(op,", ");
					op->printf(op,"%.8f",vp->deviceCoords[n]);
				}
				op->printf(op,"\n");
			}
		}
	}
}

/* Allocate variable sized data elements */
static int icmNamedColor_allocate(
	icmNamedColor *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		unsigned int i;
		if (ovr_mul(p->count, sizeof(icmNamedColorVal))) {
			return icm_err(icp, 1,"icmNamedColor_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmNamedColorVal *) icp->al->calloc(icp->al,p->count, sizeof(icmNamedColorVal))) == NULL) {
			return icm_err(icp, 2,"icmNamedColor_alloc: malloc() of icmNamedColor data failed");
		}
		for (i = 0; i < p->count; i++) {
			p->data[i].icp = icp;	/* Do init */
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmNamedColor_delete(
	icmNamedColor *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmNamedColor(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmNamedColor, icSigNamedColor2Type)
	if (p == NULL)
		return NULL;

	/* Default the the number of device coords appropriately for NamedColorType */
	p->nDeviceCoords = icmCSSig2nchan(icp->header->colorSpace);

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* Colorant table structure read/write support */
/* (Contribution from Piet Vandenborre) */

static int read_ColorantTableVal(
	icmColorantTableVal *p,
	char *bp,
	char *end,
	icColorSpaceSignature pcs		/* Header Profile Connection Space */
) {
	int rv;
	icc *icp = p->icp;

	if (bp > end || (32 + 6) > (end - bp)) {
		return icm_err(icp, 1,"icmColorantTableVal_read: Data too short to read");
	}
	if ((rv = check_null_string(bp,32)) == 1) {
		return icm_err(icp, 1,"icmColorantTableVal_read: Name string not terminated");
	}
	memmove((void *)p->name,(void *)(bp + 0),32);
	switch(pcs) {
		case icSigXYZData:
    	case icSigLabData:
			read_PCSNumber(icp, pcs, p->pcsCoords, bp+32);
			break;
		default:
			return 1;		/* Unknown PCS */
	}
	return 0;
}

static int write_ColorantTableVal(
	icmColorantTableVal *p,
	char *bp,
	icColorSpaceSignature pcs		/* Header Profile Connection Space */
) {
	int rv;
	icc *icp = p->icp;

	if ((rv = check_null_string(p->name,32)) == 1) {
		return icm_err(icp, 1,"icmColorantTableVal_write: Name string is unterminated");
	}
	memmove((void *)(bp + 0),(void *)p->name,32);
	rv = 0;
	switch(pcs) {
		case icSigXYZData:
    	case icSigLabData:
			rv |= write_PCSNumber(icp, pcs, p->pcsCoords, bp+32);
			break;
		default:
			return icm_err(icp, 1,"icmColorantTableVal_write: Unknown PCS");
	}
	if (rv) {
		return icm_err(icp, 1,"icmColorantTableVal_write: write of PCS coord failed");
	}
	return 0;
}

/* - - - - - - - - - - - */
/* icmColorantTable object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmColorantTable_get_size(
	icmColorantTable *p
) {
	unsigned int len = 0;
	if (p->ttype == icSigColorantTableType
	 || p->ttype == icmSigAltColorantTableType) {
		unsigned int i;
		len = sat_add(len, 8);			/* 8 bytes for tag and padding */
		len = sat_add(len, 4);			/* 4 for count of colorants */
		for (i = 0; i < p->count; i++) {
			len = sat_add(len, 32); /* colorant names - 32 bytes*/
			len = sat_add(len, 6); /* colorant pcs value - 3 x 16bit number*/
		}
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmColorantTable_read(
	icmColorantTable *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	icColorSpaceSignature pcs;
	unsigned int i;
	char *bp, *buf, *end;
	int rv = 0;

	if (icp->header->deviceClass != icSigLinkClass)
		pcs = icp->header->pcs;
	else
		pcs = icSigLabData;

	if (len < 4) {
		return icm_err(icp, 1,"icmColorantTable_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmColorantTable_read: malloc() failed");
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmColorantTable_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	p->ttype = (icTagTypeSignature)read_SInt32Number(bp);
	if (p->ttype != icSigColorantTableType
	 && p->ttype != icmSigAltColorantTableType) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmColorantTable_read: Wrong tag type for icmColorantTable");
	}

	if (len < 12) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmColorantTable_read: Tag too small to be legal");
	}

	/* Read count of colorants */
	if (p->ttype == icmSigAltColorantTableType)
		p->count = read_UInt8Number(bp+8);		/* Hmm. This is Little Endian */
	else
		p->count = read_UInt32Number(bp+8);

	if (p->count > ((len - 12) / (32 + 6))) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmColorantTable_read count overflow, count %x, len %d",p->count,len);
	}

	bp = bp + 12;

	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read all the data from the buffer */
	for (i = 0; i < p->count; i++, bp += (32 + 6)) {
		if (p->ttype == icmSigAltColorantTableType	/* Hack to reverse little endian */
		 && (end - bp) >= 38) {
			int tt;
			tt = *(bp + 32);
			*(bp+32) = *(bp+33);
			*(bp+33) = tt;
			tt = *(bp + 34);
			*(bp+34) = *(bp+35);
			*(bp+35) = tt;
			tt = *(bp + 36);
			*(bp+36) = *(bp+37);
			*(bp+37) = tt;
		}
		if ((rv = read_ColorantTableVal(p->data+i, bp, end, pcs)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	}

	icp->al->free(icp->al, buf);
	return rv;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmColorantTable_write(
	icmColorantTable *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	icColorSpaceSignature pcs;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	if (icp->header->deviceClass != icSigLinkClass)
		pcs = icp->header->pcs;
	else
		pcs = icSigLabData;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmColorantTable_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmColorantTable_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		return icm_err(icp, rv,"icmColorantTable_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write count of colorants */
	if ((rv = write_UInt32Number(p->count, bp+8)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmColorantTable_write: write_UInt32Number() failed");
	}

	bp = bp + 12;
	
	/* Write all the data to the buffer */
	for (i = 0; i < p->count; i++, bp += (32 + 6)) {
		if ((rv = write_ColorantTableVal(p->data+i, bp, pcs)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmColorantTable_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmColorantTable_dump(
	icmColorantTable *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icc *icp = p->icp;
	icColorSpaceSignature pcs;

	if (icp->header->deviceClass != icSigLinkClass)
		pcs = icp->header->pcs;
	else
		pcs = icSigLabData;

	if (verb <= 0)
		return;

	if (p->ttype == icSigColorantTableType
	 || p->ttype == icmSigAltColorantTableType)
		op->printf(op,"ColorantTable:\n");
	op->printf(op,"  No. colorants  = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		icmColorantTableVal *vp;
		for (i = 0; i < p->count; i++) {
			vp = p->data + i;
			op->printf(op,"    Colorant %lu:\n",i);
			op->printf(op,"      Name = '%s'\n",vp->name);

			if (p->ttype == icSigColorantTableType
			 || p->ttype == icmSigAltColorantTableType) {
			
				switch(pcs) {
					case icSigXYZData:
							op->printf(op,"      XYZ = %.8f, %.8f, %.8f\n",
							        vp->pcsCoords[0],vp->pcsCoords[1],vp->pcsCoords[2]);
						break;
			    	case icSigLabData:
							op->printf(op,"      Lab = %f, %f, %f\n",
							        vp->pcsCoords[0],vp->pcsCoords[1],vp->pcsCoords[2]);
						break;
					default:
							op->printf(op,"      Unexpected PCS\n");
						break;
				}
			}
		}
	}
}

/* Allocate variable sized data elements */
static int icmColorantTable_allocate(
	icmColorantTable *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		unsigned int i;
		if (ovr_mul(p->count, sizeof(icmColorantTableVal))) {
			return icm_err(icp, 1,"icmColorantTable_alloc: count overflow (%d of %lu bytes)",
			                        p->count,(unsigned int)sizeof(icmColorantTableVal));
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmColorantTableVal *) icp->al->calloc(icp->al,p->count, sizeof(icmColorantTableVal))) == NULL) {
			return icm_err(icp, 2,"icmColorantTable_alloc: malloc() of icmColorantTable data failed");
		}
		for (i = 0; i < p->count; i++) {
			p->data[i].icp = icp;	/* Do init */
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmColorantTable_delete(
	icmColorantTable *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmColorantTable(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmColorantTable, icSigColorantTableType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* textDescription */

/* Return the number of bytes needed to write this tag */
static unsigned int icmTextDescription_get_size(
	icmTextDescription *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);						/* 8 bytes for tag and padding */
	len = sat_addadd(len, 4, p->count);			/* Ascii string length + ASCII string */

	if (p->ucCount > 0) {
		p->uc16Count = icmUTF8toUTF16BE(NULL, NULL, p->uc8Desc, p->ucCount)/2;
	} else {
		p->uc16Count = 0; 
	}
	len = sat_addaddmul(len, 8, 2, p->uc16Count);	/* Unicode language code + length + string */
	len = sat_addadd(len, 3, 67);					/* ScriptCode code + length + string */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmTextDescription_read(
	icmTextDescription *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv;
	char *bp, *buf, *end;

	if (len < (8 + 4 + 8 + 3)) {
		return icm_err(icp, 1,"icmTextDescription_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmTextDescription_read: malloc() failed");
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmTextDescription_read: fseek() or fread() failed");
	}

	/* Read from the buffer into the structure */
	if ((rv = p->core_read(p, &bp, end)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* core read the object, return 0 on success, error code on fail */
static int icmTextDescription_core_read(
	icmTextDescription *p,
	char **bpp,				/* Pointer to buffer pointer, returns next after read */
	char *end				/* Pointer to past end of read buffer */
) {
	icc *icp = p->icp;
	int rv;
	char *bp = *bpp;

	if (bp > end || 8 > (end - bp)) {
		*bpp = bp;
		return icm_err(icp, 1,"icmTextDescription_read: Data too short to type descriptor");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		*bpp = bp;
		return icm_err(icp, 1,"icmTextDescription_read: Wrong tag type ('%s') for icmTextDescription",
		        icmtag2str((icTagTypeSignature)read_SInt32Number(bp)));
	}
	bp += 8;

	/* Read the Ascii string */
	if (bp > end || 4 > (end - bp)) {
		*bpp = bp;
		return icm_err(icp, 1,"icmTextDescription_read: Data too short to read Ascii header");
	}
	p->count = read_UInt32Number(bp);
	bp += 4;
	if (p->count > 0) {
		int chrv;
		if (bp > end || p->count > (end - bp)) {
			*bpp = bp;
			return icm_err(icp, 1,"icmTextDescription_read: Data too short to read Ascii string");
		}
		if ((chrv = check_null_string(bp, p->count)) == 1) {
			if ((icp->cflags & icmCFlagAllowQuirks) == 0) {
				*bpp = bp;
				return icm_err(icp, ICM_FMT_TEXT_ANOTTERM,"icmTextDescription_read: ASCII string is not nul terminated");
			}
			p->icp->op = icmSnRead;			/* Let icmQuirkWarning know direction */
			icmQuirkWarning(icp, ICM_FMT_TEXT_ANOTTERM, 0, "icmTextDescription_read: ASCII string is not null terminated");

			/* Fix it */
			p->count++;
			if ((rv = p->allocate(p)) != 0) {
				*bpp = bp;
				return rv;
			}
			memmove((void *)p->desc, (void *)bp, p->count-1);
			p->desc[p->count-1] = '\000';
			bp += p->count-1;

		} else {
			if (chrv == 2) {
				*bpp = bp;
				if ((icp->cflags & icmCFlagAllowQuirks) == 0)
					return icm_err(icp, ICM_FMT_TEXT_ASHORT,"icmTextDescription_read: ASCII string is shorter than count");
				p->icp->op = icmSnRead;			/* Let icmQuirkWarning know direction */
				icmQuirkWarning(icp, ICM_FMT_TEXT_ASHORT, 0, "icmTextDescription_read: ASCII string is shorter than count");
			}
			if ((rv = p->allocate(p)) != 0) {
				*bpp = bp;
				return rv;
			}
			strcpy(p->desc, bp);
			bp += p->count;
	
			if (chrv == 2)
				p->count = strlen(p->desc)+1; /* Repair string */
		}
	}
	
	/* Read the Unicode string */
	if (bp > end || 8 > (end - bp)) {
		*bpp = bp;
		return icm_err(icp, 1,"icmTextDescription_read: Data too short to read Unicode string");
	}
	p->ucLangCode = read_UInt32Number(bp);
	bp += 4;
	p->uc16Count = read_UInt32Number(bp);
	bp += 4;
	if (p->uc16Count > 0) {
		icmUTFerr utferr;

		if (bp > end || p->uc16Count > (end - bp)/2) {
			*bpp = bp;
			return icm_err(icp, 1,"icmTextDescription_read: Data too short to read Unicode string");
		}

		/* See how long utf-8 string will be */
		p->ucCount = icmUTF16BEtoUTF8(NULL, NULL, bp, p->uc16Count);

		if ((rv = p->allocate(p)) != 0) {
			*bpp = bp;
			return rv;
		}

		/* Do the translation */
		icmUTF16BEtoUTF8(&utferr, p->uc8Desc, bp, p->uc16Count);
		
		if (utferr != icmUTF_ok) {
			if ((icp->cflags & icmCFlagAllowQuirks) == 0) {
				*bpp = bp;
				return icm_err(icp, ICM_FMT_TEXT_UERR,"icmTextDescription_read: utf-16 to utf-8 translate returned error '%s'",icmUTFerr2str(utferr));
			}
			p->icp->op = icmSnRead;			/* Let icmQuirkWarning know direction */
			icmQuirkWarning(icp, ICM_FMT_TEXT_UERR, 0, "icmTextDescription_read: utf-16 to utf-8 translate returned error '%s'",icmUTFerr2str(utferr)); 
		}
		bp += p->uc16Count * 2;
	}
	
	/* Read the ScriptCode string */
	if (bp > end || 3 > (end - bp)) {
		*bpp = bp;
		return icm_err(icp, 1,"icmTextDescription_read: Data too short to read ScriptCode header");
	}
	p->scCode = read_UInt16Number(bp);
	bp += 2;
	p->scCount = read_UInt8Number(bp);
	bp += 1;
	if (p->scCount > 0) {
		if (p->scCount > 67) {
			*bpp = bp;
			return icm_err(icp, 1,"icmTextDescription_read: ScriptCode string too long (%d)",p->scCount);
		}
		if (bp > end || p->scCount > (end - bp)) {
			*bpp = bp;
			return icm_err(icp, 1,"icmTextDescription_read: Data too short to read ScriptCode string");
		}
		if ((rv = check_null_string(bp,p->scCount)) == 1) {
			*bpp = bp;
			if ((icp->cflags & icmCFlagAllowQuirks) == 0)
				return icm_err(icp, ICM_FMT_TEXT_SCRTERM,"icmTextDescription_read: ScriptCode string is not terminated");
			p->icp->op = icmSnRead;			/* Let icmQuirkWarning know direction */
			icmQuirkWarning(icp, ICM_FMT_TEXT_SCRTERM, 0, "icmTextDescription_read: ScriptCode string is not terminated");
			/* Patch it up */
			bp[p->scCount-1] = '\000';
		}
		memmove((void *)p->scDesc, (void *)bp, p->scCount);
	} else {
		memset((void *)p->scDesc, 0, 67);
	}
	bp += 67;
	
	*bpp = bp;
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmTextDescription_write(
	icmTextDescription *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmTextDescription_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmTextDescription_write: malloc() failed");
	}
	bp = buf;

	/* Write to the buffer from the structure */
	if ((rv = p->core_write(p, &bp)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmTextDescription_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Core write the contents of the object. Return 0 on sucess, error code on failure */
static int icmTextDescription_core_write(
	icmTextDescription *p,
	char **bpp				/* Pointer to buffer pointer, returns next after write */
) {
	icc *icp = p->icp;
	char *bp = *bpp;
	int rv;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		*bpp = bp;
		return icm_err(icp, rv,"icmTextDescription_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp += 8;

	/* Write the Ascii string */
	if ((rv = write_UInt32Number(p->count,bp)) != 0) {
		return icm_err(icp, rv,"icmTextDescription_write: write_UInt32Number() failed");
		*bpp = bp;
	}
	bp += 4;
	if (p->count > 0) {
		if ((rv = check_null_string(p->desc,p->count)) == 1) {
			*bpp = bp;
			return icm_err(icp, 1,"icmTextDescription_write: ASCII string is not terminated");
		}
		if (rv == 2) {
			*bpp = bp;
			return icm_err(icp, 1,"icmTextDescription_write: ASCII string is shorter than length");
		}
		strcpy(bp, p->desc);
		bp += strlen(p->desc) + 1;
	}
	
	/* Write the Unicode string */
	if ((rv = write_UInt32Number(p->ucLangCode, bp)) != 0) {
		*bpp = bp;
		return icm_err(icp, rv,"icmTextDescription_write: write_UInt32Number() failed");
	}
	bp += 4;
	if ((rv = write_UInt32Number(p->uc16Count, bp)) != 0) {
		*bpp = bp;
		return icm_err(icp, rv,"icmTextDescription_write: write_UInt32Number() failed");
	}
	bp += 4;
	if (p->uc16Count > 0) {
		icmUTFerr utferr;
		size_t len;

		len = icmUTF8toUTF16BE(&utferr, bp, p->uc8Desc, p->ucCount);
		bp += len;

		if ((len/2) != p->uc16Count) {
			*bpp = bp;
			return icm_err(icp, 1,"icmTextDescription_write: Unicode string has changed length");
		}
		if (utferr != icmUTF_ok) {
			*bpp = bp;
			return icm_err(icp, 1,"icmTextDescription_write: utf-8 to utf-16 translate returned error '%s'",icmUTFerr2str(utferr));
		}
	}
	
	/* Write the ScriptCode string */
	if ((rv = write_UInt16Number(p->scCode, bp)) != 0) {
		return icm_err(icp, rv,"icmTextDescription_write: write_UInt16Number() failed");
		*bpp = bp;
	}
	bp += 2;
	if ((rv = write_UInt8Number(p->scCount, bp)) != 0) {
		*bpp = bp;
		return icm_err(icp, rv,"icmTextDescription_write: write_UInt8Number() failed");
	}
	bp += 1;
	if (p->scCount > 0) {
		if (p->scCount > 67) {
			*bpp = bp;
			return icm_err(icp, 1,"icmTextDescription_write: ScriptCode string too long (%d)",p->scCount);
		}
		if ((rv = check_null_string((char *)p->scDesc,p->scCount)) == 1) {
			*bpp = bp;
			return icm_err(icp, 1,"icmTextDescription_write: ScriptCode string is not terminated");
		}
		memmove((void *)bp, (void *)p->scDesc, 67);
	} else {
		memset((void *)bp, 0, 67);
	}
	bp += 67;

	*bpp = bp;
	return 0;
}

/* Dump a text description of the object */
static void icmTextDescription_dump(
	icmTextDescription *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	unsigned int i, r, c;

	if (verb <= 0)
		return;

	op->printf(op,"TextDescription:\n");

	if (p->count > 0) {
		unsigned int size = p->count > 0 ? p->count-1 : 0;
		op->printf(op,"  ASCII data, length %lu chars:\n",p->count);

		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->printf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->printf(op,"...\n");
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->printf(op,"    0x%04lx: ",i);
			c += 10;
			while (i < size && c < 75) {
				if (isprint(p->desc[i])) {
					op->printf(op,"%c",p->desc[i]);
					c++;
				} else {
					op->printf(op,"\\%03o",p->desc[i]);
					c += 4;
				}
				i++;
			}
			if (i < size)
				op->printf(op,"\n");
		}
	} else {
		op->printf(op,"  No ASCII data\n");
	}

	/* Don't want to dump Unicode or ScriptCode to console, as its interpretation */
	/* will be unknown. */
	if (p->ucCount > 0) {
		unsigned int size = p->ucCount > 0 ? p->ucCount-1 : 0;
		op->printf(op,"  Unicode Data, Language code 0x%x, UTF-16 length %lu chars\n",
		        p->ucLangCode, p->uc16Count);
		op->printf(op,"  UTF-8 unicode translation length %lu chars\n", size);
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->printf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->printf(op,"...\n");
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->printf(op,"    0x%04lx: ",i);
			c += 10;
			while (i < size && c < 75) {
				if (isprint(p->uc8Desc[i])) {
					op->printf(op,"%c",p->uc8Desc[i]);
					c++;
				} else {
					op->printf(op,"\\%03o",p->uc8Desc[i]);
					c += 4;
				}
				i++;
			}
			if (i < size)
				op->printf(op,"\n");
		}
	} else {
		op->printf(op,"  No Unicode data\n");
	}

	if (p->scCount > 0) {
		unsigned int size = p->scCount > 0 ? p->scCount-1 : 0;
		op->printf(op,"  ScriptCode Data, Code 0x%x, length %lu chars\n",
		        p->scCode, p->scCount);
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->printf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->printf(op,"...\n");
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->printf(op,"    0x%04lx: ",i);
			c += 10;
			while (i < size && c < 75) {
				if (isprint(p->scDesc[i])) {
					op->printf(op,"%c",p->scDesc[i]);
					c++;
				} else {
					op->printf(op,"\\%03o",p->scDesc[i]);
					c += 4;
				}
				i++;
			}
			if (i < size)
				op->printf(op,"\n");
		}
	} else {
		op->printf(op,"  No ScriptCode data\n");
	}
}

/* Allocate variable sized data elements */
static int icmTextDescription_allocate(
	icmTextDescription *p
) {
	icc *icp = p->icp;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(char))) {
			return icm_err(icp, 1,"icmTextDescription_alloc: size overflow");
		}
		if (p->desc != NULL)
			icp->al->free(icp->al, p->desc);
		if ((p->desc = (char *) icp->al->calloc(icp->al, p->count, sizeof(char))) == NULL) {
			return icm_err(icp, 2,"icmTextDescription_alloc: malloc() of Ascii description failed");
		}
		p->_count = p->count;
	}
	if (p->ucCount != p->uc_count) {
		if (ovr_mul(p->ucCount, sizeof(ORD16))) {
			return icm_err(icp, 1,"icmTextDescription_alloc: size overflow");
		}
		if (p->uc8Desc != NULL)
			icp->al->free(icp->al, p->uc8Desc);
		if ((p->uc8Desc = (char *) icp->al->calloc(icp->al, p->ucCount, sizeof(char))) == NULL) {
			return icm_err(icp, 2,"icmTextDescription_alloc: malloc() of UTF-8 description failed");
		}
		p->uc_count = p->ucCount;
	}
	return 0;
}

/* Free all variable sized elements */
static void icmTextDescription_unallocate(
	icmTextDescription *p
) {
	icc *icp = p->icp;

	if (p->desc != NULL)
		icp->al->free(icp->al, p->desc);
	if (p->uc8Desc != NULL)
		icp->al->free(icp->al, p->uc8Desc);
}

/* Free all storage in the object */
static void icmTextDescription_delete(
	icmTextDescription *p
) {
	icc *icp = p->icp;

	icmTextDescription_unallocate(p);
	icp->al->free(icp->al, p);
}

/* Initialze a named object */
static void icmTextDescription_init(
	icmTextDescription *p,
	icc *icp
) {
	memset((void *)p, 0, sizeof(icmTextDescription));	/* Imitate calloc */

	ICM_TTYPE_INIT_LEGACY(icmTextDescription, icSigTextDescriptionType)

	p->core_read  = icmTextDescription_core_read;
	p->core_write = icmTextDescription_core_write;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmTextDescription(
	icc *icp
) {
	icmTextDescription *p = NULL;

	if (icp->e.c == ICM_ERR_OK) {
		if ((p = (icmTextDescription *)icp->al->calloc(icp->al, 1, sizeof(icmTextDescription))) == NULL) {
			icm_err(icp, ICM_ERR_MALLOC, "Allocating tag %s failed","icmTextDescription");
		}
	}
	if (p == NULL)
		return NULL;

	icmTextDescription_init(p,icp);
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */

/* Support for Profile sequence icmDescStruct */

/* Return the number of bytes needed to write this tag */
static unsigned int icmDescStruct_get_size(
	icmDescStruct *p
) {
	unsigned int len = 0;
	len = sat_add(len, 20);				/* 20 bytes for header info */
	len = sat_add(len, p->device.get_size(&p->device));
	if (p->device.count == 0)
		len = sat_add(len, 1);			/* Extra 1 because of zero length desciption */
	len = sat_add(len, p->model.get_size(&p->model));
	if (p->model.count == 0)
		len = sat_add(len, 1);			/* Extra 1 because of zero length desciption */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmDescStruct_read(
	icmDescStruct *p,
	char **bpp,				/* Pointer to buffer pointer, returns next after read */
	char *end				/* Pointer to past end of read buffer */
) {
	icc *icp = p->icp;
	char *bp = *bpp;
	int rv = 0;

	if (bp > end || 20 > (end - bp)) {
		*bpp = bp;
		return icm_err(icp, 1,"icmDescStruct_read: Data too short read header");
	}

    p->deviceMfg = read_SInt32Number(bp + 0);
    p->deviceModel = read_UInt32Number(bp + 4);
    read_UInt64Number(&p->attributes, bp + 8);
	p->technology = (icTechnologySignature) read_UInt32Number(bp + 16);
	*bpp = bp += 20;

	/* Read the device text description */
	if ((rv = p->device.core_read(&p->device, bpp, end)) != 0) {
		return rv;
	}

	/* Read the model text description */
	if ((rv = p->model.core_read(&p->model, bpp, end)) != 0) {
		return rv;
	}
	
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmDescStruct_write(
	icmDescStruct *p,
	char **bpp				/* Pointer to buffer pointer, returns next after read */
) {
	icc *icp = p->icp;
	char *bp = *bpp;
	int rv = 0;
	char *ttd = NULL;
	unsigned int tts = 0;

    if ((rv = write_SInt32Number(p->deviceMfg, bp + 0)) != 0) {
		return icm_err(icp, rv,"icmDescStruct_write: write_SInt32Number() failed");
		*bpp = bp;
	}
    if ((rv = write_UInt32Number(p->deviceModel, bp + 4)) != 0) {
		*bpp = bp;
		return icm_err(icp, rv,"icmDescStruct_write: write_UInt32Number() failed");
	}
    if ((rv = write_UInt64Number(&p->attributes, bp + 8)) != 0) {
		*bpp = bp;
		return icm_err(icp, rv,"icmDescStruct_write: write_UInt64Number() failed");
	}
	if ((rv = write_UInt32Number(p->technology, bp + 16)) != 0) {
		*bpp = bp;
		return icm_err(icp, rv,"icmDescStruct_write: write_UInt32Number() failed");
	}
	*bpp = bp += 20;

	/* Make sure the ASCII device text is a minimum size of 1, as per the spec. */
	ttd = p->device.desc;
	tts = p->device.count;

	if (p->device.count == 0) {
		p->device.desc = "";
		p->device.count = 1;
	}

	/* Write the device text description */
	if ((rv = p->device.core_write(&p->device, bpp)) != 0) {
		return rv;
	}

	p->device.desc = ttd;
	p->device.count = tts;

	/* Make sure the ASCII model text is a minimum size of 1, as per the spec. */
	ttd = p->model.desc;
	tts = p->model.count;

	if (p->model.count == 0) {
		p->model.desc = "";
		p->model.count = 1;
	}

	/* Write the model text description */
	if ((rv = p->model.core_write(&p->model, bpp)) != 0) {
		return rv;
	}

	p->model.desc = ttd;
	p->model.count = tts;

	/* Make sure the ASCII model text is a minimum size of 1, as per the spec. */
	ttd = p->device.desc;
	tts = p->device.count;

	return 0;
}

/* Dump a text description of the object */
static void icmDescStruct_dump(
	icmDescStruct *p,
	icmFile *op,	/* Output to dump to */
	int   verb,		/* Verbosity level */
	int   index		/* Description index */
) {
	if (verb <= 0)
		return;

	op->printf(op,"DescStruct %u:\n",index);
	if (verb >= 1) {
		op->printf(op,"  Dev. Mnfctr.    = %s\n",icmtag2str(p->deviceMfg));	/* ~~~ */
		op->printf(op,"  Dev. Model      = %s\n",icmtag2str(p->deviceModel));	/* ~~~ */
		op->printf(op,"  Dev. Attrbts    = %s\n", icmDeviceAttributes2str(p->attributes.l));
		op->printf(op,"  Dev. Technology = %s\n", icmTechnologySignature2str(p->technology));
		p->device.dump(&p->device, op,verb);
		p->model.dump(&p->model, op,verb);
		op->printf(op,"\n");
	}
}

/* Allocate variable sized data elements (ie. descriptions) */
static int icmDescStruct_allocate(
	icmDescStruct *p
) {
	int rv;

	if ((rv = p->device.allocate(&p->device)) != 0) {
		return rv;
	}
	if ((rv = p->model.allocate(&p->model)) != 0) {
		return rv;
	}
	return 0;
}

/* Free all storage in the object */
static void icmDescStruct_delete(
	icmDescStruct *p
) {
	icmTextDescription_unallocate(&p->device);
	icmTextDescription_unallocate(&p->model);
}

/* Init a DescStruct object */
static void icmDescStruct_init(
	icmDescStruct *p,
	icc *icp
) {

	p->allocate = icmDescStruct_allocate;
	p->icp = icp;

	icmTextDescription_init(&p->device, icp);
	icmTextDescription_init(&p->model, icp);
}

/* - - - - - - - - - - - - - - - */
/* icmProfileSequenceDesc object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmProfileSequenceDesc_get_size(
	icmProfileSequenceDesc *p
) {
	unsigned int len = 0;
	unsigned int i;
	len = sat_add(len, 12);				/* 12 bytes for tag, padding and count */
	for (i = 0; i < p->count; i++) {	/* All the description structures */
		len = sat_add(len, icmDescStruct_get_size(&p->data[i]));
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmProfileSequenceDesc_read(
	icmProfileSequenceDesc *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	unsigned int i;
	char *bp, *buf, *end;
	int rv = 0;

	if (len < 12) {
		return icm_err(icp, 1,"icmProfileSequenceDesc_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmProfileSequenceDesc_read: malloc() failed");
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmProfileSequenceDesc_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmProfileSequenceDesc_read: Wrong tag type for icmProfileSequenceDesc");
	}
	bp += 8;	/* Skip padding */

	p->count = read_UInt32Number(bp);	/* Number of sequence descriptions */
	bp += 4;

	/* Read all the sequence descriptions */
	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}
	for (i = 0; i < p->count; i++) {
		if ((rv = icmDescStruct_read(&p->data[i], &bp, end)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmProfileSequenceDesc_write(
	icmProfileSequenceDesc *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmProfileSequenceDesc_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmProfileSequenceDesc_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmProfileSequenceDesc_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	if ((rv = write_UInt32Number(p->count,bp+8)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmProfileSequenceDesc_write: write_UInt32Number() failed");
	}
	bp = bp + 12;

	/* Write all the description structures */
	for (i = 0; i < p->count; i++) {
		if ((rv = icmDescStruct_write(&p->data[i], &bp)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmProfileSequenceDesc_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmProfileSequenceDesc_dump(
	icmProfileSequenceDesc *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"ProfileSequenceDesc:\n");
	op->printf(op,"  No. elements = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			icmDescStruct_dump(&p->data[i], op, verb-1, i);
	}
}

/* Allocate variable sized data elements (ie. count of profile descriptions) */
static int icmProfileSequenceDesc_allocate(
	icmProfileSequenceDesc *p
) {
	icc *icp = p->icp;
	unsigned int i;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(icmDescStruct))) {
			return icm_err(icp, 1,"icmProfileSequenceDesc_allocate: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmDescStruct *) icp->al->calloc(icp->al, p->count, sizeof(icmDescStruct))) == NULL) {
			return icm_err(icp, 2,"icmProfileSequenceDesc_allocate Allocation of DescStruct array failed");
		}
		/* Now init the DescStructs */
		for (i = 0; i < p->count; i++) {
			icmDescStruct_init(&p->data[i], icp);
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmProfileSequenceDesc_delete(
	icmProfileSequenceDesc *p
) {
	icc *icp = p->icp;
	unsigned int i;

	for (i = 0; i < p->count; i++) {
		icmDescStruct_delete(&p->data[i]);	/* Free allocated contents */
	}
	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmProfileSequenceDesc(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmProfileSequenceDesc, icSigProfileSequenceDescType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* Signature */

/* Return the number of bytes needed to write this tag */
static unsigned int icmSignature_get_size(
	icmSignature *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);			/* 8 bytes for tag and padding */
	len = sat_add(len, 4);			/* 4 for signature */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmSignature_read(
	icmSignature *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	char *bp, *buf;

	if (len < 12) {
		return icm_err(icp, 1,"icmSignature_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmSignature_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmSignature_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmSignaturSignatureng tag type for icmSignature");
	}

	/* Read the encoded measurement geometry */
	p->sig = (icTechnologySignature)read_SInt32Number(bp + 8);

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmSignature_write(
	icmSignature *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmSignature_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmSignature_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmSignature_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write the signature */
	if ((rv = write_SInt32Number((int)p->sig, bp + 8)) != 0) {
		return icm_err(icp, rv,"icmSignaturea_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmSignature_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmSignature_dump(
	icmSignature *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"Signature\n");
	op->printf(op,"  Technology = %s\n", icmTechnologySignature2str(p->sig));
}

/* Allocate variable sized data elements */
static int icmSignature_allocate(
	icmSignature *p
) {
	/* Nothing to do */
	return 0;
}

/* Free all storage in the object */
static void icmSignature_delete(
	icmSignature *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmSignature(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmSignature, icSigSignatureType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */

/* Data conversion support functions */
static int read_ScreeningData(icmScreeningData *p, char *d) {
	p->frequency = read_S15Fixed16Number(d + 0);
	p->angle     = read_S15Fixed16Number(d + 4);
	p->spotShape = (icSpotShape)read_SInt32Number(d + 8);
	return 0;
}

static int write_ScreeningData(icmScreeningData *p, char *d) {
	int rv;
	if ((rv = write_S15Fixed16Number(p->frequency, d + 0)) != 0)
		return rv;
	if ((rv = write_S15Fixed16Number(p->angle, d + 4)) != 0)
		return rv;
	if ((rv = write_SInt32Number((int)p->spotShape, d + 8)) != 0)
		return rv;
	return 0;
}


/* icmScreening object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmScreening_get_size(
	icmScreening *p
) {
	unsigned int len = 0;
	len = sat_add(len, 16);					/* 16 bytes for tag, padding, flag & channeles */
	len = sat_addmul(len, p->channels, 12);	/* 12 bytes for each channel */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmScreening_read(
	icmScreening *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i;
	char *bp, *buf, *end;

	if (len < 12) {
		return icm_err(icp, 1,"icmScreening_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmScreening_read: malloc() failed");
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmScreening_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmScreening_read: Wrong tag type for icmScreening");
	}
	p->screeningFlag = read_UInt32Number(bp+8);		/* Flags */
	p->channels      = read_UInt32Number(bp+12);	/* Number of channels */
	bp = bp + 16;

	if ((rv = p->allocate(p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read all the data from the buffer */
	for (i = 0; i < p->channels; i++, bp += 12) {
		if (bp > end || 12 > (end - bp)) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmScreening_read: Data too short to read Screening Data");
		}
		read_ScreeningData(&p->data[i], bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmScreening_write(
	icmScreening *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmScreening_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmScreening_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmScreening_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	if ((rv = write_UInt32Number(p->screeningFlag,bp+8)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmScreening_write: write_UInt32Number() failed");
		}
	if ((rv = write_UInt32Number(p->channels,bp+12)) != 0) {
			return icm_err(icp, rv,"icmScreening_write: write_UInt32NumberXYZumber() failed");
			icp->al->free(icp->al, buf);
		}
	bp = bp + 16;

	/* Write all the data to the buffer */
	for (i = 0; i < p->channels; i++, bp += 12) {
		if ((rv = write_ScreeningData(&p->data[i],bp)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmScreening_write: write_ScreeningData() failed");
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmScreening_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmScreening_dump(
	icmScreening *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"Screening:\n");
	op->printf(op,"  Flags = %s\n", icmScreenEncodings2str(p->screeningFlag));
	op->printf(op,"  No. channels = %u\n",p->channels);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->channels; i++) {
			op->printf(op,"    %lu:\n",i);
			op->printf(op,"      Frequency:  %f\n",p->data[i].frequency);
			op->printf(op,"      Angle:      %f\n",p->data[i].angle);
			op->printf(op,"      Spot shape: %s\n", icmSpotShape2str(p->data[i].spotShape));
		}
	}
}

/* Allocate variable sized data elements */
static int icmScreening_allocate(
	icmScreening *p
) {
	icc *icp = p->icp;

	if (p->channels != p->_channels) {
		if (ovr_mul(p->channels, sizeof(icmScreeningData))) {
			return icm_err(icp, 1,"icmScreening_alloc: size overflow");
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmScreeningData *) icp->al->malloc(icp->al, p->channels * sizeof(icmScreeningData))) == NULL) {
			return icm_err(icp, 2,"icmScreening_alloc: malloc() of icmScreening data failed");
		}
		p->_channels = p->channels;
	}
	return 0;
}

/* Free all storage in the object */
static void icmScreening_delete(
	icmScreening *p
) {
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmScreening(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmScreening, icSigScreeningType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUcrBg object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUcrBg_get_size(
	icmUcrBg *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);							/* 8 bytes for tag and padding */
	len = sat_addaddmul(len, 4, p->UCRcount, 2);	/* Undercolor Removal */
	len = sat_addaddmul(len, 4, p->BGcount, 2);		/* Black Generation */
	len = sat_add(len, p->count);					/* Description string */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUcrBg_read(
	icmUcrBg *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	unsigned int i;
	int rv;
	char *bp, *buf, *end;

	if (len < 16) {
		return icm_err(icp, 1,"icmUcrBg_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmUcrBg_read: malloc() failed");
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmUcrBg_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmUcrBg_read: Wrong tag type for icmUcrBg");
	}
	p->UCRcount = read_UInt32Number(bp+8);	/* First curve count */
	bp = bp + 12;

	if (p->UCRcount > 0) {
		if ((rv = p->allocate(p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
		for (i = 0; i < p->UCRcount; i++, bp += 2) {
			if (bp > end || 2 > (end - bp)) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, 1,"icmUcrBg_read: Data too short to read UCR Data");
			}
			if (p->UCRcount == 1)	/* % */
				p->UCRcurve[i] = (double)read_UInt16Number(bp);
			else					/* 0.0 - 1.0 */
				p->UCRcurve[i] = read_DCS16Number(bp);
		}
	} else {
		p->UCRcurve = NULL;
	}

	if (bp > end || 4 > (end - bp)) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmData_read: Data too short to read Black Gen count");
	}
	p->BGcount = read_UInt32Number(bp);	/* First curve count */
	bp += 4;

	if (p->BGcount > 0) {
		if ((rv = p->allocate(p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
		for (i = 0; i < p->BGcount; i++, bp += 2) {
			if (bp > end || 2 > (end - bp)) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, 1,"icmUcrBg_read: Data too short to read BG Data");
			}
			if (p->BGcount == 1)	/* % */
				p->BGcurve[i] = (double)read_UInt16Number(bp);
			else					/* 0.0 - 1.0 */
				p->BGcurve[i] = read_DCS16Number(bp);
		}
	} else {
		p->BGcurve = NULL;
	}

	p->count = end - bp;		/* Nominal string length */
	if (p->count > 0) {
		if ((rv = check_null_string(bp, p->count)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmUcrBg_read: string is not null terminated");
		}
		p->count = strlen(bp) + 1;
		if ((rv = p->allocate(p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
		memmove((void *)p->string, (void *)bp, p->count);
		bp += p->count;
	} else {
		p->string = NULL;
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUcrBg_write(
	icmUcrBg *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmUcrBg_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmUcrBg_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmUcrBg_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp = bp + 8;

	/* Write UCR curve */
	if ((rv = write_UInt32Number(p->UCRcount,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmUcrBg_write: write_UInt32Number() failed");
	}
	bp += 4;

	for (i = 0; i < p->UCRcount; i++, bp += 2) {
		if (p->UCRcount == 1) { /* % */
			if ((rv = write_UInt16Number((unsigned int)(p->UCRcurve[i]+0.5),bp)) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmUcrBg_write: write_UInt16umber() failed");
			}
		} else {
			if ((rv = write_DCS16Number(p->UCRcurve[i],bp)) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmUcrBg_write: write_DCS16umber(%.8f) failed",p->UCRcurve[i]);
			}
		}
	}

	/* Write BG curve */
	if ((rv = write_UInt32Number(p->BGcount,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmUcrBg_write: write_UInt32Number() failed");
	}
	bp += 4;

	for (i = 0; i < p->BGcount; i++, bp += 2) {
		if (p->BGcount == 1) { /* % */
			if ((rv = write_UInt16Number((unsigned int)(p->BGcurve[i]+0.5),bp)) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmUcrBg_write: write_UInt16umber() failed");
			}
		} else {
			if ((rv = write_DCS16Number(p->BGcurve[i],bp)) != 0) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmUcrBg_write: write_DCS16umber(%.8f) failed",p->BGcurve[i]);
			}
		}
	}

	if (p->string != NULL) {
		if ((rv = check_null_string(p->string,p->count)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmUcrBg_write: text is not null terminated");
		}
		if (rv == 2) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmUcrBg_write: text is shorter than length");
		}
		memmove((void *)bp, (void *)p->string, p->count);
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmUcrBg_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUcrBg_dump(
	icmUcrBg *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"Undercolor Removal Curve & Black Generation:\n");

	if (p->UCRcount == 0) {
		op->printf(op,"  UCR: Not specified\n");
	} else if (p->UCRcount == 1) {
		op->printf(op,"  UCR: %f%%\n",p->UCRcurve[0]);
	} else {
		op->printf(op,"  UCR curve no. elements = %u\n",p->UCRcount);
		if (verb >= 2) {
			unsigned int i;
			for (i = 0; i < p->UCRcount; i++)
				op->printf(op,"  %3lu:  %f\n",i,p->UCRcurve[i]);
		}
	}
	if (p->BGcount == 0) {
		op->printf(op,"  BG: Not specified\n");
	} else if (p->BGcount == 1) {
		op->printf(op,"  BG: %f%%\n",p->BGcurve[0]);
	} else {
		op->printf(op,"  BG curve no. elements = %u\n",p->BGcount);
		if (verb >= 2) {
			unsigned int i;
			for (i = 0; i < p->BGcount; i++)
				op->printf(op,"  %3lu:  %f\n",i,p->BGcurve[i]);
		}
	}

	{
		unsigned int i, r, c, size;
		op->printf(op,"  Description:\n");
		op->printf(op,"    No. chars = %lu\n",p->count);
	
		size = p->count > 0 ? p->count-1 : 0;
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->printf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->printf(op,"...\n");
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->printf(op,"      0x%04lx: ",i);
			c += 10;
			while (i < size && c < 73) {
				if (isprint(p->string[i])) {
					op->printf(op,"%c",p->string[i]);
					c++;
				} else {
					op->printf(op,"\\%03o",p->string[i]);
					c += 4;
				}
				i++;
			}
			if (i < size)
				op->printf(op,"\n");
		}
	}
}

/* Allocate variable sized data elements */
static int icmUcrBg_allocate(
	icmUcrBg *p
) {
	icc *icp = p->icp;

	if (p->UCRcount != p->UCR_count) {
		if (ovr_mul(p->UCRcount, sizeof(double))) {
			return icm_err(icp, 1,"icmUcrBg_allocate: size overflow");
		}
		if (p->UCRcurve != NULL)
			icp->al->free(icp->al, p->UCRcurve);
		if ((p->UCRcurve = (double *) icp->al->calloc(icp->al, p->UCRcount, sizeof(double))) == NULL) {
			return icm_err(icp, 2,"icmUcrBg_allocate: malloc() of UCR curve data failed");
		}
		p->UCR_count = p->UCRcount;
	}
	if (p->BGcount != p->BG_count) {
		if (ovr_mul(p->BGcount, sizeof(double))) {
			return icm_err(icp, 1,"icmUcrBg_allocate: size overflow");
		}
		if (p->BGcurve != NULL)
			icp->al->free(icp->al, p->BGcurve);
		if ((p->BGcurve = (double *) icp->al->calloc(icp->al, p->BGcount, sizeof(double))) == NULL) {
			return icm_err(icp, 2,"icmUcrBg_allocate: malloc() of BG curve data failed");
		}
		p->BG_count = p->BGcount;
	}
	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(char))) {
			return icm_err(icp, 1,"icmUcrBg_allocate: size overflow");
		}
		if (p->string != NULL)
			icp->al->free(icp->al, p->string);
		if ((p->string = (char *) icp->al->calloc(icp->al, p->count, sizeof(char))) == NULL) {
			return icm_err(icp, 2,"icmUcrBg_allocate: malloc() of string data failed");
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUcrBg_delete(
	icmUcrBg *p
) {
	icc *icp = p->icp;

	if (p->UCRcurve != NULL)
		icp->al->free(icp->al, p->UCRcurve);
	if (p->BGcurve != NULL)
		icp->al->free(icp->al, p->BGcurve);
	if (p->string != NULL)
		icp->al->free(icp->al, p->string);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUcrBg(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmUcrBg, icSigUcrBgType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* VideoCardGamma (ColorSync 2.5 specific - c/o Neil Okamoto) */
/* 'vcgt' */

static unsigned int icmVideoCardGamma_get_size(
	icmVideoCardGamma *p
) {
	unsigned int len = 0;

	len = sat_add(len, 8);			/* 8 bytes for tag and padding */
	len = sat_add(len, 4);			/* 4 for gamma type */

	/* compute size of remainder */
	if (p->tagType == icmVideoCardGammaTableType) {
		len = sat_add(len, 2);       /* 2 bytes for channels */
		len = sat_add(len, 2);       /* 2 for entry count */
		len = sat_add(len, 2);       /* 2 for entry size */
		len = sat_add(len, sat_mul3(p->u.table.channels, /* compute table size */
		                            p->u.table.entryCount, p->u.table.entrySize));
	}
	else if (p->tagType == icmVideoCardGammaFormulaType) {
		len = sat_add(len, 12);		/* 4 bytes each for red gamma, min, & max */
		len = sat_add(len, 12);		/* 4 bytes each for green gamma, min & max */
		len = sat_add(len, 12);		/* 4 bytes each for blue gamma, min & max */
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmVideoCardGamma_read(
	icmVideoCardGamma *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv, c;
	char *bp, *buf;
	ORD8 *pchar;
	ORD16 *pshort;

	if (len < 18) {
		return icm_err(icp, 1,"icmVideoCardGamma_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmVideoCardGamma_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmVideoCardGamma_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmVideoCardGamma_read: Wrong tag type for icmVideoCardGamma");
	}

	/* Read gamma format (eg. table or formula) from the buffer */
	p->tagType = (icmVideoCardGammaTagType)read_UInt32Number(bp+8);

	/* Read remaining gamma data based on format */
	if (p->tagType == icmVideoCardGammaTableType) {
		p->u.table.channels   = read_UInt16Number(bp+12);
		p->u.table.entryCount = read_UInt16Number(bp+14);
		p->u.table.entrySize  = read_UInt16Number(bp+16);
		if ((len-18) < sat_mul3(p->u.table.channels, p->u.table.entryCount,
		                        p->u.table.entrySize)) {
			return icm_err(icp, 1,"icmVideoCardGamma_read: Tag too small to be legal");
		}
		if ((rv = p->allocate(p)) != 0) {  /* make space for table */
			icp->al->free(icp->al, buf);
			return icp->e.c = rv;
		}
		/* ~~~~ This should be a table of doubles like the rest of icclib ! ~~~~ */
		pchar = (ORD8 *)p->u.table.data;
		pshort = (ORD16 *)p->u.table.data;
		for (c=0, bp=bp+18; c<p->u.table.channels*p->u.table.entryCount; c++) {
			switch (p->u.table.entrySize) {
			case 1:
				*pchar++ = read_UInt8Number(bp);
				bp++;
				break;
			case 2:
				*pshort++ = read_UInt16Number(bp);
				bp+=2;
				break;
			default:
				p->del(p);
				icp->al->free(icp->al, buf);
				return icm_err(icp, 1,"icmVideoCardGamma_read: unsupported table entry size");
			}
		}
	} else if (p->tagType == icmVideoCardGammaFormulaType) {
		if (len < 48) {
			return icm_err(icp, 1,"icmVideoCardGamma_read: Tag too small to be legal");
		}
		p->u.table.channels     = 3;	/* Always 3 for formula */
		p->u.formula.redGamma   = read_S15Fixed16Number(bp+12);
		p->u.formula.redMin     = read_S15Fixed16Number(bp+16);
		p->u.formula.redMax     = read_S15Fixed16Number(bp+20);
		p->u.formula.greenGamma = read_S15Fixed16Number(bp+24);
		p->u.formula.greenMin   = read_S15Fixed16Number(bp+28);
		p->u.formula.greenMax   = read_S15Fixed16Number(bp+32);
		p->u.formula.blueGamma  = read_S15Fixed16Number(bp+36);
		p->u.formula.blueMin    = read_S15Fixed16Number(bp+40);
		p->u.formula.blueMax    = read_S15Fixed16Number(bp+44);
	} else {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmVideoCardGammaTable_read: Unknown gamma format for icmVideoCardGamma");
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmVideoCardGamma_write(
	icmVideoCardGamma *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0, c;
	ORD8 *pchar;
	ORD16 *pshort;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmViewingConditions_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmViewingConditions_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmVideoCardGamma_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write gamma format (eg. table of formula) */
	if ((rv = write_UInt32Number(p->tagType,bp+8)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmVideoCardGamma_write: write_UInt32Number() failed");
	}

	/* Write remaining gamma data based on format */
	if (p->tagType == icmVideoCardGammaTableType) {
		if ((rv = write_UInt16Number(p->u.table.channels,bp+12)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_UInt16Number() failed");
		}
		if ((rv = write_UInt16Number(p->u.table.entryCount,bp+14)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_UInt16Number() failed");
		}
		if ((rv = write_UInt16Number(p->u.table.entrySize,bp+16)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_UInt16Number() failed");
		}
		pchar = (ORD8 *)p->u.table.data;
		pshort = (ORD16 *)p->u.table.data;
		for (c=0, bp=bp+18; c<p->u.table.channels*p->u.table.entryCount; c++) {
			switch (p->u.table.entrySize) {
			case 1:
				write_UInt8Number(*pchar++,bp);
				bp++;
				break;
			case 2:
				write_UInt16Number(*pshort++,bp);
				bp+=2;
				break;
			default:
				icp->al->free(icp->al, buf);
				return icm_err(icp, 1,"icmVideoCardGamma_write: unsupported table entry size");
			}
		}
	} else if (p->tagType == icmVideoCardGammaFormulaType) {
		if ((rv = write_S15Fixed16Number(p->u.formula.redGamma,bp+12)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.redMin,bp+16)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.redMax,bp+20)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.greenGamma,bp+24)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.greenMin,bp+28)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.greenMax,bp+32)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.blueGamma,bp+36)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.blueMin,bp+40)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.blueMax,bp+44)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
		}
	} else {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmVideoCardGammaTable_write: Unknown gamma format for icmVideoCardGamma");
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmViewingConditions_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmVideoCardGamma_dump(
	icmVideoCardGamma *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	int c,i;

	if (verb <= 0)
		return;

	if (p->tagType == icmVideoCardGammaTableType) {
		op->printf(op,"VideoCardGammaTable:\n");
		op->printf(op,"  channels  = %d\n", p->u.table.channels);
		op->printf(op,"  entries   = %d\n", p->u.table.entryCount);
		op->printf(op,"  entrysize = %d\n", p->u.table.entrySize);
		if (verb >= 2) {
			/* dump array contents also */
			for (c=0; c<p->u.table.channels; c++) {
				op->printf(op,"  channel #%d\n",c);
				for (i=0; i<p->u.table.entryCount; i++) {
					if (p->u.table.entrySize == 1) {
						op->printf(op,"    %d: %d\n",i,((ORD8 *)p->u.table.data)[c*p->u.table.entryCount+i]);
					}
					else if (p->u.table.entrySize == 2) {
						op->printf(op,"    %d: %d\n",i,((ORD16 *)p->u.table.data)[c*p->u.table.entryCount+i]);
					}
				}
			}
		}
	} else if (p->tagType == icmVideoCardGammaFormulaType) {
		op->printf(op,"VideoCardGammaFormula:\n");
		op->printf(op,"  red gamma   = %.8f\n", p->u.formula.redGamma);
		op->printf(op,"  red min     = %.8f\n", p->u.formula.redMin);
		op->printf(op,"  red max     = %.8f\n", p->u.formula.redMax);
		op->printf(op,"  green gamma = %.8f\n", p->u.formula.greenGamma);
		op->printf(op,"  green min   = %.8f\n", p->u.formula.greenMin);
		op->printf(op,"  green max   = %.8f\n", p->u.formula.greenMax);
		op->printf(op,"  blue gamma  = %.8f\n", p->u.formula.blueGamma);
		op->printf(op,"  blue min    = %.8f\n", p->u.formula.blueMin);
		op->printf(op,"  blue max    = %.8f\n", p->u.formula.blueMax);
	} else {
		op->printf(op,"  Unknown tag format\n");
	}
}

/* Allocate variable sized data elements */
static int icmVideoCardGamma_allocate(
	icmVideoCardGamma *p
) {
	icc *icp = p->icp;
	unsigned int size;

	/* note: allocation is only relevant for table type
	 * and in that case the channels, entryCount, and entrySize
	 * fields must all be set prior to getting here
	 */

	if (p->tagType == icmVideoCardGammaTableType) {
		size = sat_mul(p->u.table.channels, p->u.table.entryCount);
		switch (p->u.table.entrySize) {
			case 1:
				size = sat_mul(size, sizeof(ORD8));
				break;
			case 2:
				size = sat_mul(size, sizeof(unsigned short));
				break;
			default:
				return icm_err(icp, 1,"icmVideoCardGamma_alloc: unsupported table entry size");
		}
		if (size == UINT_MAX) {
			return icm_err(icp, 1,"icmVideoCardGamma_alloc: size overflow");
		}
		if (p->u.table.data != NULL)
			icp->al->free(icp->al, p->u.table.data);
		if ((p->u.table.data = (void*) icp->al->malloc(icp->al, size)) == NULL) {
			return icm_err(icp, 2,"icmVideoCardGamma_alloc: malloc() of table data failed");
		}
	}

	return 0;
}

/* Read a value */
static double icmVideoCardGamma_lookup(
	icmVideoCardGamma *p,
	int chan,		/* Channel, 0, 1 or 2 */
	double iv		/* Input value 0.0 - 1.0 */
) {
	double ov = 0.0;

	if (chan < 0 || chan > (p->u.table.channels-1)
	 || iv < 0.0 || iv > 1.0)
		return iv;

	if (p->tagType == icmVideoCardGammaTableType && p->u.table.entryCount == 0) {
		/* Deal with siliness */
		ov = iv;
	} else if (p->tagType == icmVideoCardGammaTableType) {
		/* Use linear interpolation */
		unsigned int ix;
		double val0, val1, w;
		double inputEnt_1 = (double)(p->u.table.entryCount-1);

		val0 = iv * inputEnt_1;
		if (val0 < 0.0)
			val0 = 0.0;
		else if (val0 > inputEnt_1)
			val0 = inputEnt_1;
		ix = (unsigned int)floor(val0);		/* Coordinate */
		if (ix > (p->u.table.entryCount-2))
			ix = (p->u.table.entryCount-2);
		w = val0 - (double)ix;		/* weight */
		if (p->u.table.entrySize == 1) {
			val0 = ((ORD8 *)p->u.table.data)[chan * p->u.table.entryCount + ix]/255.0;
			val1 = ((ORD8 *)p->u.table.data)[chan * p->u.table.entryCount + ix + 1]/255.0;
		} else if (p->u.table.entrySize == 2) {
			val0 = ((ORD16 *)p->u.table.data)[chan * p->u.table.entryCount + ix]/65535.0;
			val1 = ((ORD16 *)p->u.table.data)[chan * p->u.table.entryCount + ix + 1]/65535.0;
		} else {
			val0 = val1 = iv;
		}
		ov = val0 + w * (val1 - val0);

	} else if (p->tagType == icmVideoCardGammaFormulaType) {
		double min, max, gam;

		if (iv == 0) {
			min = p->u.formula.redMin;
			max = p->u.formula.redMax;
			gam = p->u.formula.redGamma;
		} else if (iv == 1) {
			min = p->u.formula.greenMin;
			max = p->u.formula.greenMax;
			gam = p->u.formula.greenGamma;
		} else {
			min = p->u.formula.blueMin;
			max = p->u.formula.blueMax;
			gam = p->u.formula.blueGamma;
		}

		/* The Apple OSX doco confirms this is the formula */
		ov = min + (max - min) * pow(iv, gam);
	}
	return ov;
}

/* Free all storage in the object */
static void icmVideoCardGamma_delete(
	icmVideoCardGamma *p
) {
	icc *icp = p->icp;

	if (p->tagType == icmVideoCardGammaTableType && p->u.table.data != NULL)
		icp->al->free(icp->al, p->u.table.data);

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmVideoCardGamma(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmVideoCardGamma, icSigVideoCardGammaType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* ViewingConditions */

/* Return the number of bytes needed to write this tag */
static unsigned int icmViewingConditions_get_size(
	icmViewingConditions *p
) {
	unsigned int len = 0;
	len = sat_add(len, 8);			/* 8 bytes for tag and padding */
	len = sat_add(len, 12);			/* 12 for XYZ of illuminant */
	len = sat_add(len, 12);			/* 12 for XYZ of surround */
	len = sat_add(len, 4);			/* 4 for illuminant type */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmViewingConditions_read(
	icmViewingConditions *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	int rv;
	char *bp, *buf;

	if (len < 36) {
		return icm_err(icp, 1,"icmViewingConditions_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmViewingConditions_read: malloc() failed");
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmViewingConditions_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmViewingConditions_read: Wrong tag type for icmViewingConditions");
	}

	/* Read the XYZ values for the illuminant */
	if ((rv = read_XYZNumber(&p->illuminant, bp+8)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmViewingConditions: read_XYZNumber error");
	}

	/* Read the XYZ values for the surround */
	if ((rv = read_XYZNumber(&p->surround, bp+20)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmViewingConditions: read_XYZNumber error");
	}

	/* Read the encoded standard illuminant */
	p->stdIlluminant = (icIlluminant)read_SInt32Number(bp + 32);

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmViewingConditions_write(
	icmViewingConditions *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmViewingConditions_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmViewingConditions_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmViewingConditions_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write the XYZ values for the illuminant */
	if ((rv = write_XYZNumber(&p->illuminant, bp+8)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmViewingConditions: write_XYZNumber error");
	}

	/* Write the XYZ values for the surround */
	if ((rv = write_XYZNumber(&p->surround, bp+20)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmViewingConditions: write_XYZNumber error");
	}

	/* Write the encoded standard illuminant */
	if ((rv = write_SInt32Number((int)p->stdIlluminant, bp + 32)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmViewingConditionsa_write: write_SInt32Number() failed");
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmViewingConditions_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmViewingConditions_dump(
	icmViewingConditions *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"Viewing Conditions:\n");
	op->printf(op,"  XYZ value of illuminant in cd/m^2 = %s\n", icmXYZNumber2str(&p->illuminant));
	op->printf(op,"  XYZ value of surround in cd/m^2   = %s\n", icmXYZNumber2str(&p->surround));
	op->printf(op,"  Illuminant type = %s\n", icmIlluminant2str(p->stdIlluminant));
}

/* Allocate variable sized data elements */
static int icmViewingConditions_allocate(
	icmViewingConditions *p
) {
	/* Nothing to do */
	return 0;
}

/* Free all storage in the object */
static void icmViewingConditions_delete(
	icmViewingConditions *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmViewingConditions(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmViewingConditions, icSigViewingConditionsType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmCrdInfo object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmCrdInfo_get_size(
	icmCrdInfo *p
) {
	unsigned int len = 0, t;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addadd(len, 4, p->ppsize);	/* Postscript product name */
	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		len = sat_addadd(len, 4, p->crdsize[t]);	/* crd names */ 
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmCrdInfo_read(
	icmCrdInfo *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	unsigned int t;
	int rv;
	char *bp, *buf, *end;

	if (len < 28) {
		return icm_err(icp, 1,"icmCrdInfo_read: Tag too small to be legal");
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmCrdInfo_read: malloc() failed");
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmCrdInfo_read: fseek() or fread() failed");
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmCrdInfo_read: Wrong tag type for icmCrdInfo");
	}
	bp = bp + 8;

	/* Postscript product name */
	if (bp > end || 4 > (end - bp)) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 1,"icmCrdInfo_read: Data too short to read Postscript product name");
	}
	p->ppsize = read_UInt32Number(bp);
	bp += 4;
	if (p->ppsize > 0) {
		if (p->ppsize > (end - bp)) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmCrdInfo_read: Data to short to read Postscript product string");
		}
		if ((rv = check_null_string(bp,p->ppsize)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmCrdInfo_read: Postscript product name is not terminated");
		}
		/* Haven't checked if rv == 2 is legal or not */
		if ((rv = p->allocate(p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
		memmove((void *)p->ppname, (void *)bp, p->ppsize);
		bp += p->ppsize;
	}
	
	/* CRD names for the four rendering intents */
	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		if (bp > end || 4 > (end - bp)) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmCrdInfo_read: Data too short to read CRD%d name",t);
		}
		p->crdsize[t] = read_UInt32Number(bp);
		bp += 4;
		if (p->crdsize[t] > 0) {
			if (p->crdsize[t] > (end - bp)) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, 1,"icmCrdInfo_read: Data to short to read CRD%d string",t);
			}
			if ((rv = check_null_string(bp,p->crdsize[t])) == 1) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, 1,"icmCrdInfo_read: CRD%d name is not terminated",t);
			}
			/* Haven't checked if rv == 2 is legal or not */
			if ((rv = p->allocate(p)) != 0) { 
				icp->al->free(icp->al, buf);
				return rv;
			}
			memmove((void *)p->crdname[t], (void *)bp, p->crdsize[t]);
			bp += p->crdsize[t];
		}
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmCrdInfo_write(
	icmCrdInfo *p,
	unsigned int ilen,		/* tag length */
	unsigned int of		/* File offset to write from */
) {
	icc *icp = p->icp;
	unsigned int t;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		return icm_err(icp, 1,"icmCrdInfo_write get_size overflow");
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		return icm_err(icp, 2,"icmCrdInfo_write: malloc() failed");
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmCrdInfo_write: write_SInt32Number() failed");
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp = bp + 8;

	/* Postscript product name */
	if ((rv = write_UInt32Number(p->ppsize,bp)) != 0) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, rv,"icmCrdInfo_write: write_UInt32Number() failed");
	}
	bp += 4;
	if (p->ppsize > 0) {
		if ((rv = check_null_string(p->ppname,p->ppsize)) == 1) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, 1,"icmCrdInfo_write: Postscript product name is not terminated");
		}
		/* Haven't checked if rv == 2 is legal or not */
		memmove((void *)bp, (void *)p->ppname, p->ppsize);
		bp += p->ppsize;
	}

	/* CRD names for the four rendering intents */
	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		if ((rv = write_UInt32Number(p->crdsize[t],bp)) != 0) {
			icp->al->free(icp->al, buf);
			return icm_err(icp, rv,"icmCrdInfo_write: write_UInt32Number() failed");
		}
		bp += 4;
		if (p->ppsize > 0) {
			if ((rv = check_null_string(p->crdname[t],p->crdsize[t])) == 1) {
				icp->al->free(icp->al, buf);
				return icm_err(icp, rv,"icmCrdInfo_write: CRD%d name is not terminated",t);
			}
			/* Haven't checked if rv == 2 is legal or not */
			memmove((void *)bp, (void *)p->crdname[t], p->crdsize[t]);
			bp += p->crdsize[t];
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		icp->al->free(icp->al, buf);
		return icm_err(icp, 2,"icmCrdInfo_write: fseek() or fwrite() failed");
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmCrdInfo_dump(
	icmCrdInfo *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	unsigned int i, r, c, size, t;

	if (verb <= 0)
		return;

	op->printf(op,"PostScript Product name and CRD names:\n");

	op->printf(op,"  Product name:\n");
	op->printf(op,"    No. chars = %lu\n",p->ppsize);
	
	size = p->ppsize > 0 ? p->ppsize-1 : 0;
	i = 0;
	for (r = 1;; r++) {		/* count rows */
		if (i >= size) {
			op->printf(op,"\n");
			break;
		}
		if (r > 1 && verb < 2) {
			op->printf(op,"...\n");
			break;			/* Print 1 row if not verbose */
		}
		c = 1;
		op->printf(op,"      0x%04lx: ",i);
		c += 10;
		while (i < size && c < 73) {
			if (isprint(p->ppname[i])) {
				op->printf(op,"%c",p->ppname[i]);
				c++;
			} else {
				op->printf(op,"\\%03o",p->ppname[i]);
				c += 4;
			}
			i++;
		}
		if (i < size)
			op->printf(op,"\n");
	}

	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		op->printf(op,"  CRD%ld name:\n",t);
		op->printf(op,"    No. chars = %lu\n",p->crdsize[t]);
		
		size = p->crdsize[t] > 0 ? p->crdsize[t]-1 : 0;
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->printf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->printf(op,"...\n");
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->printf(op,"      0x%04lx: ",i);
			c += 10;
			while (i < size && c < 73) {
				if (isprint(p->crdname[t][i])) {
					op->printf(op,"%c",p->crdname[t][i]);
					c++;
				} else {
					op->printf(op,"\\%03o",p->crdname[t][i]);
					c += 4;
				}
				i++;
			}
			if (i < size)
				op->printf(op,"\n");
		}
	}
}

/* Allocate variable sized data elements */
static int icmCrdInfo_allocate(
	icmCrdInfo *p
) {
	icc *icp = p->icp;
	unsigned int t;

	if (p->ppsize != p->_ppsize) {
		if (ovr_mul(p->ppsize, sizeof(char))) {
			return icm_err(icp, 1,"icmCrdInfo_alloc: size overflow");
		}
		if (p->ppname != NULL)
			icp->al->free(icp->al, p->ppname);
		if ((p->ppname = (char *) icp->al->calloc(icp->al, p->ppsize, sizeof(char))) == NULL) {
			return icm_err(icp, 2,"icmCrdInfo_alloc: malloc() of string data failed");
		}
		p->_ppsize = p->ppsize;
	}
	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		if (p->crdsize[t] != p->_crdsize[t]) {
			if (ovr_mul(p->crdsize[t], sizeof(char))) {
				return icm_err(icp, 1,"icmCrdInfo_alloc: size overflow");
			}
			if (p->crdname[t] != NULL)
				icp->al->free(icp->al, p->crdname[t]);
			if ((p->crdname[t] = (char *) icp->al->calloc(icp->al, p->crdsize[t], sizeof(char))) == NULL) {
				return icm_err(icp, 2,"icmCrdInfo_alloc: malloc() of CRD%d name string failed",t);
			}
			p->_crdsize[t] = p->crdsize[t];
		}
	}
	return 0;
}

/* Free all storage in the object */
static void icmCrdInfo_delete(
	icmCrdInfo *p
) {
	icc *icp = p->icp;
	unsigned int t;

	if (p->ppname != NULL)
		icp->al->free(icp->al, p->ppname);
	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		if (p->crdname[t] != NULL)
			icp->al->free(icp->al, p->crdname[t]);
	}
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmCrdInfo(
	icc *icp
) {
	ICM_TTYPE_ALLOCINIT_LEGACY(icmCrdInfo, icSigCrdInfoType)
	return (icmBase *)p;
}

/* ========================================================== */
/* icmHeader object */
/* ========================================================== */

static void icmHeader_serialise(icmHeader *p, icmFBuf *b) {
	unsigned int tt;

	/* On read, check that the magic number is right before doing anything else. */
	if (b->op == icmSnRead) {

		tt = 0;
		b->aoff(b, 36);
		icmSn_ui_UInt32(b, &tt);
		b->aoff(b, 0);

		if (tt != icMagicNumber) {				/* Check magic number */
			icm_err(b->icp, ICM_ERR_MAGIC_NUMBER, "ICC profile has bad magic number");
			return;
		}
	}

	icmSn_ui_UInt32(b, &p->size);				/* 0-3:  Profile size in bytes */
// ~8 should check that size is multiple of 4 
    icmSn_i_SInt32(b, &p->cmmId);				/* 4-7:  CMM for profile */
	icmSn_VersionNumber4b(b, &p->vers);			/* 8-11: Version number */
	if (b->icp->e.c != ICM_ERR_OK)
		return;									/* Don't continue if we don't understand version */
    icmSn_ProfileClassSig32(b, &p->deviceClass);/* 12-15: Type of profile */
    icmSn_ColorSpaceSig32(b, &p->colorSpace);	/* 16-19: Color space of data (input space) */
    icmSn_ColorSpaceSig32(b, &p->pcs);			/* 20-23: PCS: XYZ or Lab (output space) */
	icmSn_DateTimeNumber12b(b, &p->date);		/* 24-35: Creation Date */
	if (b->op == icmSnWrite)
		tt = icMagicNumber;
	icmSn_ui_UInt32(b, &tt);					/* 36-39: magic number */
    icmSn_PlatformSig32(b, &p->platform);		/* 40-43: Primary platform */
	if (p->doid) {
		icProfileFlags zf = 0;
	    icmSn_ProfileFlags32(b, &zf);			/* 44-47: Various flag bits */
	} else {
	    icmSn_ProfileFlags32(b, &p->flags);		/* 44-47: Various flag bits */
	}
    icmSn_i_SInt32(b, &p->manufacturer);		/* 48-51: Dev manufacturer */
    icmSn_i_SInt32(b, &p->model);				/* 52-55: Dev model */
    icmSn_DeviceAttributes64(b, &p->attributes);/* 56-63: Device attributes */
	if (b->op == icmSnWrite)
		p->extraRenderingIntent = (~0xffff & p->extraRenderingIntent)
		                         | (0xffff & p->renderingIntent);
	if (p->doid) {
		icRenderingIntent zri = 0;
	    icmSn_RenderingIntent32(b, &zri);		/* 64-67: Rendering intent */
	} else {
	    icmSn_RenderingIntent32(b, &p->extraRenderingIntent);
												/* 64-67: Rendering intent */
	}
	if (b->op == icmSnRead)
		p->renderingIntent = 0xffff & p->extraRenderingIntent;
	icmSn_XYZNumber12b(b, &p->illuminant);		/* 68-79: Profile illuminant */
    icmSn_i_SInt32(b, &p->creator);				/* 80-83: Profile creator */

	if (p->vers.majv >= 4) {
		if (p->doid) {
			unsigned char id[16] = { 0 };
			for (tt = 0; tt < 16; tt++)
				icmSn_uc_UInt8(b, &id[tt]);		/* 84-99: Profile ID */	
		} else {
			for (tt = 0; tt < 16; tt++)
				icmSn_uc_UInt8(b, &p->id[tt]);	/* 84-99: Profile ID */	
		}
		icmSn_pad(b, 28);						/* 100-127 Zero */
	} else {
		if (b->op == icmSnRead) {
			for (tt = 0; tt < 16; tt++)
				p->id[tt] = 0;
		}
		icmSn_pad(b, 44);						/* 84-127 Zero */
	}
	if ((b->op & icmSnSerialise) && b->get_off(b) != 128)			/* Assert */
		icm_err(b->icp, ICM_ERR_HEADER_LENGTH, "Internal: ICC profile header is wrong length");
}

/* Return the size of the header */
static unsigned int icmHeader_get_size(icmHeader *p) {
	icmFBuf *b;
	if ((b = new_icmFBuf(p->icp, icmSnSize, NULL, 0, 0)) == NULL)
		return 0;
	icmHeader_serialise(p, b);
    return b->done(b);
}

/* Read the header. size is the size in bytes to be read, */
/* of is the file offest it should be read from. */
/* Return error code */
static int icmHeader_read(icmHeader *p, unsigned int size, unsigned int of) {
	icmFBuf *b;
	
	if ((b = new_icmFBuf(p->icp, icmSnRead, p->icp->rfp, of, size)) == NULL)
		return p->icp->e.c;
	icmHeader_serialise(p, b);
    b->done(b);
	return p->icp->e.c;
}

/* Write the header. size is the size in bytes to be written, */
/* of is the file offest it should be read from. */
/* Return error code */
static int icmHeader_write(icmHeader *p, unsigned int size, unsigned int of) {
	icmFBuf *b;
	if ((b = new_icmFBuf(p->icp, icmSnWrite, p->icp->wfp, of, size)) == NULL)
		return p->icp->e.c;
	icmHeader_serialise(p, b);
    b->done(b);
	return p->icp->e.c;
}

/* Dump a text description of the object */
static void icmHeader_dump(
	icmHeader *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmDateTimeNumber l;
	icRenderingIntent fullri;
	int i;
	if (verb <= 0)
		return;

	op->printf(op,"Header:\n");
	op->printf(op,"  Profile size    = %d bytes\n",p->size);
	op->printf(op,"  CMM             = %s\n",icmtag2str(p->cmmId));
	op->printf(op,"  Version         = %d.%d.%d\n",p->vers.majv, p->vers.minv, p->vers.bfv);
	op->printf(op,"  Device Class    = %s\n", icmProfileClassSignature2str(p->deviceClass));
	op->printf(op,"  Color Space     = %s\n", icmColorSpaceSignature2str(p->colorSpace));
	op->printf(op,"  Conn. Space     = %s\n", icmColorSpaceSignature2str(p->pcs));
	op->printf(op,"  UTC Date&Time   = %s\n", icmDateTimeNumber2str(&p->date));
	icmDateTimeNumber_tolocal(&l, &p->date);
	op->printf(op,"  Local Date&Time = %s\n", icmDateTimeNumber2str(&l));
	op->printf(op,"  Platform        = %s\n", icmPlatformSignature2str(p->platform));
	op->printf(op,"  Flags           = %s\n", icmProfileHeaderFlags2str(p->flags));
	op->printf(op,"  Dev. Mnfctr.    = %s\n", icmtag2str(p->manufacturer));	/* ~~~ */
	op->printf(op,"  Dev. Model      = %s\n", icmtag2str(p->model));	/* ~~~ */
	op->printf(op,"  Dev. Attrbts    = %s\n", icmDeviceAttributes2str(p->attributes.l));
	fullri = (~0xffff & p->extraRenderingIntent) | (0xffff & p->renderingIntent);
	op->printf(op,"  Rndrng Intnt    = %s\n", icmRenderingIntent2str(fullri));
	op->printf(op,"  Illuminant      = %s\n", icmXYZNumber_and_Lab2str(&p->illuminant));
	op->printf(op,"  Creator         = %s\n", icmtag2str(p->creator));	/* ~~~ */
	if (p->vers.majv >= 4) {			/* V4.0+ feature */
		for (i = 0; i < 16; i++) {		/* Check if ID has been set */
			if (p->id[i] != 0)
				break;
		}
		if (i < 16)
			op->printf(op,"  ID              = %02X%02X%02X%02X%02X%02X%02X%02X"
			                                  "%02X%02X%02X%02X%02X%02X%02X%02X\n",
				p->id[0], p->id[1], p->id[2], p->id[3], p->id[4], p->id[5], p->id[6], p->id[7],
				p->id[8], p->id[9], p->id[10], p->id[11], p->id[12], p->id[13], p->id[14], p->id[15]);
		else
			op->printf(op,"  ID           = <Not set>\n");
	}
	op->printf(op,"\n");
}

/* Allocate variable sized data elements */
/* Return error code */
static int icmHeader_allocate(icmHeader *p) {
	return p->icp->e.c;
}

static int icmHeader_check(icmHeader *p, icTagSignature sig) {
	// ~~~888 add header check code here. Move in-line check code to here ???
	return p->icp->e.c;
}

static void icmHeader_del(icmHeader *p) {
	p->icp->al->free(p->icp->al, p);
}

/* Create an empty object. On error, set mp->e and return NULL */
static icmHeader *new_icmHeader(icc *icp) {
	int i;

	/* Allocate *p, and do base class init */
	ICM_TTYPE_ALLOCINIT(icmHeader, 0)
	if (p == NULL)
		return NULL;

	/* Characteristic values */
	p->hsize = 128;		/* Header is 128 bytes for ICC */

	/* Values that must be set before writing */
	p->deviceClass = icMaxEnumClass;/* Type of profile - must be set! */
    p->colorSpace = icMaxEnumData;	/* Clr space of data - must be set! */
    p->pcs = icMaxEnumData;			/* PCS: XYZ or Lab - must be set! */
    p->renderingIntent = icMaxEnumIntent;	/* Rendering intent - must be set ! */

	/* Values that should be set before writing */
	p->manufacturer = -1;			/* Dev manufacturer - should be set ! */
    p->model = -1;					/* Dev model number - should be set ! */
    p->attributes.l = 0;			/* ICC Device attributes - should set ! */
    p->flags = 0;					/* Embedding flags - should be set ! */
	
	/* Values that may be set before writing */
    p->attributes.h = 0;			/* Dev Device attributes - may be set ! */
    p->creator = icmstr2tag("argl");/* Profile creator - Argyll - may be set ! */

	/* Init default values in header */
	p->cmmId = icmstr2tag("argl");	/* CMM for profile - Argyll CMM */
    p->vers.majv = 2;				/* Default version 2.2.0 */
	p->vers.minv = 2;
	p->vers.bfv  = 0;
	icmDateTimeNumber_setcur(&p->date);/* Creation Date */
#ifdef NT
	p->platform = icSigMicrosoft;
#endif
#ifdef __APPLE__
	p->platform = icSigMacintosh;
#endif
#if defined(UNIX) && !defined(__APPLE__)
	p->platform = icmSig_nix;
#endif
    p->illuminant = icmD50;			/* Profile illuminant - D50 */

	/* Values that will be created automatically */
	for (i = 0; i < 16; i++)
		p->id[i] = 0;

	return p;
}

/* ------------------------------------------------------------- */

const icmTVRange icmtvrange_all = ICMTVRANGE_ALL;
const icmTVRange icmtvrange_none = ICMTVRANGE_NONE;

/* Return a string that shows the icc's version */
char *icmICCvers2str(icc *p) {
	static char buf[50];
	sprintf(buf,"%d.%d.%d", p->header->vers.majv, p->header->vers.minv, p->header->vers.bfv);
	return buf;
}

/* Return a string that shows the given version */
/* This can only be called 5 times before reusing returned static buffer */
char *icmTVers2str(icmTV tv) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	sprintf(bp,"%d.%d.%d", tv / 10000, (tv/100) % 100, tv % 100);
	return bp;
}

/* Return a string that shows the given version range. */
/* This can only be called once before reusing returned static buffer */
char *icmTVersRange2str(icmTVRange *tvr) {
	static char buf[100];
	if (tvr->min == ICMTV_MAX && tvr->max == ICMTV_MIN) {
		return "for no versions";
	} else if (tvr->min == ICMTV_MIN && tvr->max == ICMTV_MAX) {
		return "for all versions";
	} else if (tvr->min == ICMTV_MIN && tvr->max != ICMTV_MIN) {
		sprintf(buf,"if %d.%d.%d or less",tvr->max / 10000, (tvr->max/100) % 100, (tvr->max) % 100);
	} else if (tvr->max == ICMTV_MAX) {
		sprintf(buf,"if %d.%d.%d or more", tvr->min / 10000, (tvr->min/100) % 100, (tvr->min) % 100);
	} else
	{
		sprintf(buf,"over %d.%d.%d - %d.%d.%d",
			tvr->min / 10000, (tvr->min/100) % 100, (tvr->min) % 100, 
			tvr->max / 10000, (tvr->max/100) % 100, (tvr->max) % 100);
	}
	return buf;
}

/* Return true if icc version is within TV range */
icmVersInRange(struct _icc *icp, icmTVRange *tvr) {
	int rv;
	icmTV icctv = ICMVERS2TV(icp->header->vers);
	rv = ICMTV_IN_RANGE(icctv, tvr);
	return rv;
}

/* Return true if the two version ranges have an overlap */
int icmVersOverlap(icmTVRange *tvr1, icmTVRange *tvr2) {
	return (tvr1->min <= tvr2->max && tvr2->min <= tvr1->max);
}

/* Return a string that shows the icc version */
char *icmProfileVers2str(struct _icc *icp) {
	icmTV icctv = ICMVERS2TV(icp->header->vers);
	return icmTVers2str(icctv);
}

/* Table of Tag Types valid version range and constructor. */
/* Last has ttype icMaxEnumType */
static icmTagTypeVersConstrRec icmTagTypeTable[] = {
//	{icSigChromaticityType,         ICMTVRANGE_23_PLUS,			new_icmChromaticity},
	{icSigCrdInfoType,              {ICMTV_21, ICMTV_40},		new_icmCrdInfo},
	{icSigCurveType,                ICMTVRANGE_20_PLUS,			new_icmCurve},
	{icSigDataType,                 ICMTVRANGE_20_PLUS,			new_icmData},
	{icSigDateTimeType,             ICMTVRANGE_20_PLUS,			new_icmDateTime},
//	{icSigDeviceSettingsType,		{ICMTV_22, ICMTV_40},		new_icmDeviceSettings},
	{icSigLut16Type,				ICMTVRANGE_20_PLUS,			new_icmLut},
	{icSigLut8Type,					ICMTVRANGE_20_PLUS,			new_icmLut},
	{icSigMeasurementType,			ICMTVRANGE_20_PLUS,			new_icmMeasurement},
	{icSigNamedColorType,			{ICMTV_20, ICMTV_24},		new_icmNamedColor},
	{icSigNamedColor2Type,          ICMTVRANGE_20_PLUS,			new_icmNamedColor},
	{icSigProfileSequenceDescType,	ICMTVRANGE_20_PLUS,			new_icmProfileSequenceDesc},
//	{icSigResponseCurveSet16Type,	ICMTVRANGE_22_PLUS,			new_icmResponseCurveSet16},
	{icSigS15Fixed16ArrayType,		ICMTVRANGE_20_PLUS,			new_icmS15Fixed16Array},
	{icSigScreeningType,			{ICMTV_21, ICMTV_40},		new_icmScreening},
	{icSigSignatureType,			ICMTVRANGE_20_PLUS,			new_icmSignature},
	{icSigTextType,					ICMTVRANGE_20_PLUS,			new_icmText},
	{icSigTextDescriptionType,		{ICMTV_20, ICMTV_24},		new_icmTextDescription},
	{icSigU16Fixed16ArrayType,		ICMTVRANGE_20_PLUS,			new_icmU16Fixed16Array},
	{icSigUcrBgType,				{ICMTV_20, ICMTV_40},		new_icmUcrBg},
	{icSigUInt16ArrayType,			ICMTVRANGE_20_PLUS,			new_icmUInt16Array},
	{icSigUInt32ArrayType,			ICMTVRANGE_20_PLUS,			new_icmUInt32Array},
	{icSigUInt64ArrayType,			ICMTVRANGE_20_PLUS,			new_icmUInt64Array},
	{icSigUInt8ArrayType,			ICMTVRANGE_20_PLUS,			new_icmUInt8Array},
	{icSigVideoCardGammaType,		ICMTVRANGE_20_PLUS,			new_icmVideoCardGamma},
	{icSigViewingConditionsType,	ICMTVRANGE_20_PLUS,			new_icmViewingConditions},
	{icSigXYZArrayType,				ICMTVRANGE_20_PLUS,			new_icmXYZArray},
	{ icMaxEnumType }
};

/* Table of Tags valid version range and possible Tag Types. */
/* Version after each TagType is intersected with Tag Type version range. */ 
/* Last has sig icMaxEnumTag */
static icmTagSigVersTypesRec icmTagSigTable[] = {

	{icSigAToB0Tag,	ICMTVRANGE_20_PLUS,	icmTCLutFwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigAToB1Tag,	ICMTVRANGE_20_PLUS,	icmTCLutFwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigAToB2Tag,	ICMTVRANGE_20_PLUS,	icmTCLutFwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigBlueMatrixColumnTag,	ICMTVRANGE_20_PLUS,	icmTCNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigBlueTRCTag, 			ICMTVRANGE_20_PLUS, icmTCNone,				
											{{ icSigCurveType,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigBToA0Tag,	ICMTVRANGE_20_PLUS,	icmTCLutBwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigBToA1Tag,	ICMTVRANGE_20_PLUS,	icmTCLutBwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigBToA2Tag,	ICMTVRANGE_20_PLUS,	icmTCLutBwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigCalibrationDateTimeTag,	ICMTVRANGE_20_PLUS,	icmTCNone,
											{{ icSigDateTimeType,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigCharTargetTag,			ICMTVRANGE_20_PLUS,	icmTCNone,
											{{ icSigTextType,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigChromaticAdaptationTag,	ICMTVRANGE_24_PLUS, icmTCNone,
											{{ icSigS15Fixed16ArrayType,	ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigChromaticityTag,			ICMTVRANGE_23_PLUS, icmTCNone,
											{{ icSigChromaticityType,		ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigCopyrightTag,				ICMTVRANGE_20_PLUS, icmTCNone,
											{{ icSigTextType,			{ ICMTV_20, ICMTV_24 }},
											 { icMaxEnumType}}},
	{icSigCrdInfoTag,				{ ICMTV_21, ICMTV_40 },	icmTCNone,
											{{ icSigCrdInfoType,			ICMTVRANGE_ALL },		
											 { icMaxEnumType}}},
#ifdef NEVER
	{icSigDataTag,					{ ICMTV_20, ICMTV_24 },	icmTCNone,
											{{ icSigDataType,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigDateTimeTag,				{ ICMTV_20, ICMTV_24 },	icmTCNone,
											{{ icSigDataType,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
#endif
	{icSigDeviceMfgDescTag,			ICMTVRANGE_20_PLUS, icmTCNone,
											{{ icSigTextDescriptionType,	{ ICMTV_20, ICMTV_24 }},
											 { icMaxEnumType}}},
	{icSigDeviceModelDescTag,		ICMTVRANGE_20_PLUS, icmTCNone,
											{{ icSigTextDescriptionType,	{ ICMTV_20, ICMTV_24 }},
											 { icMaxEnumType}}},
	{icSigDeviceSettingsTag,		{ ICMTV_22, ICMTV_40 },	icmTCNone,
											{{ icSigDeviceSettingsType,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigGamutTag,		ICMTVRANGE_20_PLUS,	icmTCLutGamut,
											{{ icSigLut16Type,				ICMTVRANGE_ALL },
											 { icSigLut8Type,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigGrayTRCTag,	ICMTVRANGE_20_PLUS, icmTCNone,
											{{ icSigCurveType,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigGreenMatrixColumnTag,	ICMTVRANGE_20_PLUS,	icmTCNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigGreenTRCTag, 			ICMTVRANGE_20_PLUS, icmTCNone,				
											{{ icSigCurveType,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigLuminanceTag,			ICMTVRANGE_20_PLUS, icmTCNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigMeasurementTag,		ICMTVRANGE_20_PLUS, icmTCNone,
											{{ icSigMeasurementType,		ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigMediaBlackPointTag,	{ ICMTV_20, ICMTV_42 }, icmTCNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigMediaWhitePointTag,	ICMTVRANGE_20_PLUS, icmTCNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigNamedColorTag,		{ ICMTV_20, ICMTV_20 }, icmTCNone,
											{{ icSigNamedColorType,			ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigNamedColor2Tag,		ICMTVRANGE_21_PLUS, icmTCNone,
											{{ icSigNamedColor2Type,		ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigOutputResponseTag,	ICMTVRANGE_22_PLUS, icmTCNone,
											{{ icSigResponseCurveSet16Type,	ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigPreview0Tag,		ICMTVRANGE_20_PLUS,	icmTCLutPreview,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigPreview1Tag,		ICMTVRANGE_20_PLUS,	icmTCLutPreview,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigPreview2Tag,		ICMTVRANGE_20_PLUS,	icmTCLutPreview,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigProfileDescriptionTag,		ICMTVRANGE_20_PLUS, icmTCNone,
											{{ icSigTextDescriptionType,		ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigProfileSequenceDescTag,		ICMTVRANGE_20_PLUS, icmTCNone,
											{{ icSigProfileSequenceDescType,	ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigPs2CRD0Tag,				{ ICMTV_20, ICMTV_40 }, icmTCNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigPs2CRD1Tag,				{ ICMTV_20, ICMTV_40 },	icmTCNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigPs2CRD2Tag,				{ ICMTV_20, ICMTV_40 },	icmTCNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigPs2CRD3Tag,				{ ICMTV_20, ICMTV_40 },	icmTCNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigPs2CSATag,				{ ICMTV_20, ICMTV_40 },	icmTCNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigPs2RenderingIntentTag,	{ ICMTV_20, ICMTV_40 },	icmTCNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigRedMatrixColumnTag,	ICMTVRANGE_20_PLUS,	icmTCNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigRedTRCTag, 			ICMTVRANGE_20_PLUS, icmTCNone,				
											{{ icSigCurveType,				ICMTVRANGE_ALL },
											 { icMaxEnumType}}},
	{icSigScreeningDescTag,		{ ICMTV_20, ICMTV_40 }, icmTCNone,
											{{ icSigTextDescriptionType,	ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigScreeningTag,			{ ICMTV_20, ICMTV_40 },	icmTCNone,
											{{ icSigScreeningType,			ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigTechnologyTag,		ICMTVRANGE_20_PLUS,	icmTCNone,
											{{ icSigSignatureType,			ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigUcrBgTag,				{ ICMTV_20, ICMTV_40 },	icmTCNone,
											{{ icSigUcrBgType,				ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigVideoCardGammaTag,	ICMTVRANGE_20_PLUS,	icmTCNone,
											{{ icSigVideoCardGammaType,		ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigViewingCondDescTag,		{ ICMTV_20, ICMTV_40 }, icmTCNone,
											{{ icSigTextDescriptionType,	ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icSigViewingConditionsTag,	ICMTVRANGE_20_PLUS,	icmTCNone,
											{{ icSigViewingConditionsType,	ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icmSigAbsToRelTransSpace,	ICMTVRANGE_20_PLUS, icmTCNone,
											{{ icSigS15Fixed16ArrayType,	ICMTVRANGE_ALL },
											 { icMaxEnumType }}},
	{icMaxEnumTag}
};

/*
	Additional tag requirement that can't be coded in icmClassTagTable:

	if (icSigMediaWhitePointTag) and if data is adopted white is not D50,
	then shall have icSigChromaticAdaptationTag.

*/

/* Table of Class Signature instance requirements. */
/* All profiles shall have the required tags, and may have the optional tags. */
/* Need to check that profile has all tags of at least one matching record. */
/* Matching record has matching Class Sig + Src + Dst color signatures within version range, */
/* Tags are only required if in Tag version + icmTagSigVersRec.vrange. */
/* Last has sig icMaxEnumClass */
static icmRequiredTagType icmClassTagTable[] = {
    {icSigInputClass,			ICMCSMR_DEV(1),	ICMCSMR_PCS,	ICMTVRANGE_ALL, 
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigGrayTRCTag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL }, 
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA0Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icMaxEnumTag }
		}
	},

    {icSigInputClass,			ICMCSMR_DEV(3),	ICMCSMR_XYZ,	ICMTVRANGE_ALL, 
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigRedColorantTag,				ICMTVRANGE_ALL },
		 { icSigGreenColorantTag,			ICMTVRANGE_ALL },
		 { icSigBlueColorantTag,			ICMTVRANGE_ALL },
		 { icSigRedTRCTag,					ICMTVRANGE_ALL },
		 { icSigGreenTRCTag,				ICMTVRANGE_ALL },
		 { icSigBlueTRCTag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA0Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigGamutTag,					ICMTVRANGE_23_PLUS },		/* Why ? */
		 { icMaxEnumTag }
		}
	},

    {icSigInputClass,     		ICMCSMR_DEVN,	ICMCSMR_PCS,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,							ICMTVRANGE_ALL },
		 { icSigAToB0Tag,										ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,								ICMTVRANGE_ALL },
		 { icSigCopyrightTag, 									ICMTVRANGE_ALL },
		 { icMaxEnumTag },
		}, {
		/* Optional: */
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA0Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigDToB0Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigDToB1Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigDToB2Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigDToB3Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD0Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD1Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD2Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD3Tag,		 			ICMTVRANGE_23_PLUS },
		 { icSigGamutTag,					ICMTVRANGE_23_PLUS },		/* Why ? */
		 { icMaxEnumTag }
		}
	},


    {icSigDisplayClass,			ICMCSMR_DEV(1),	ICMCSMR_PCS,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigGrayTRCTag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA0Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icMaxEnumTag }
		}
	},

    {icSigDisplayClass,     	ICMCSMR_DEV(3),	ICMCSMR_XYZ,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigRedColorantTag,				ICMTVRANGE_ALL },
		 { icSigGreenColorantTag,			ICMTVRANGE_ALL },
		 { icSigBlueColorantTag,			ICMTVRANGE_ALL },
		 { icSigRedTRCTag,					ICMTVRANGE_ALL },
		 { icSigGreenTRCTag,				ICMTVRANGE_ALL },
		 { icSigBlueTRCTag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA0Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigGamutTag,					ICMTVRANGE_23_PLUS },
		 { icMaxEnumTag }
		}
	},

    {icSigDisplayClass,     	ICMCSMR_DEVN, 	ICMCSMR_PCS,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigBToA0Tag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigDToB0Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigDToB1Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigDToB2Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigDToB3Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD0Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD1Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD2Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD3Tag,		 			ICMTVRANGE_23_PLUS },
		 { icSigGamutTag,					ICMTVRANGE_23_PLUS },
		 { icMaxEnumTag }
		}
	},


    {icSigOutputClass,			ICMCSMR_DEV(1),    ICMCSMR_PCS,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigGrayTRCTag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB1Tag,					ICMTVRANGE_ALL },
		 { icSigAToB2Tag,					ICMTVRANGE_ALL },
		 { icSigBToA0Tag,					ICMTVRANGE_ALL },
		 { icSigBToA1Tag,					ICMTVRANGE_ALL },
		 { icSigBToA2Tag,					ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}
	},

    {icSigOutputClass,			ICMCSMR_DEV_NOT_NCOL,	ICMCSMR_PCS,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigBToA0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB1Tag,					ICMTVRANGE_ALL },
		 { icSigBToA1Tag,					ICMTVRANGE_ALL },
		 { icSigAToB2Tag,					ICMTVRANGE_ALL },
		 { icSigBToA2Tag,					ICMTVRANGE_ALL },
		 { icSigGamutTag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigDToB0Tag, 					ICMTVRANGE_ALL },
		 { icSigDToB1Tag, 					ICMTVRANGE_ALL },
		 { icSigDToB2Tag, 					ICMTVRANGE_ALL },
		 { icSigDToB3Tag, 					ICMTVRANGE_ALL },
		 { icSigBToD0Tag, 					ICMTVRANGE_ALL },
		 { icSigBToD1Tag, 					ICMTVRANGE_ALL },
		 { icSigBToD2Tag, 					ICMTVRANGE_ALL },
		 { icSigBToD3Tag,		 			ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}
	},

    {icSigOutputClass,			ICMCSMR_NCOL,	ICMCSMR_PCS,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigBToA0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB1Tag,					ICMTVRANGE_ALL },
		 { icSigBToA1Tag,					ICMTVRANGE_ALL },
		 { icSigAToB2Tag,					ICMTVRANGE_ALL },
		 { icSigBToA2Tag,					ICMTVRANGE_ALL },
		 { icSigGamutTag,					ICMTVRANGE_ALL },
		 { icSigColorantTableTag,			ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigDToB0Tag, 					ICMTVRANGE_ALL },
		 { icSigDToB1Tag, 					ICMTVRANGE_ALL },
		 { icSigDToB2Tag, 					ICMTVRANGE_ALL },
		 { icSigDToB3Tag, 					ICMTVRANGE_ALL },
		 { icSigBToD0Tag, 					ICMTVRANGE_ALL },
		 { icSigBToD1Tag, 					ICMTVRANGE_ALL },
		 { icSigBToD2Tag, 					ICMTVRANGE_ALL },
		 { icSigBToD3Tag,		 			ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}
	},


    {icSigLinkClass,	ICMCSMR_NOT_NCOL,	ICMCSMR_NOT_NCOL,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigProfileSequenceDescTag,		ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigDToB0Tag, 					ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}
	},

    {icSigLinkClass,		ICMCSMR_NCOL,	ICMCSMR_NOT_NCOL,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigColorantTableTag,			ICMTVRANGE_ALL },
		 { icSigProfileSequenceDescTag,		ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigDToB0Tag, 					ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}
	},

    {icSigLinkClass,		ICMCSMR_NOT_NCOL,	ICMCSMR_NCOL,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigColorantTableOutTag,		ICMTVRANGE_ALL },
		 { icSigProfileSequenceDescTag,		ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigDToB0Tag, 					ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}
	},

    {icSigLinkClass,			ICMCSMR_NCOL,	ICMCSMR_NCOL,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigColorantTableTag,			ICMTVRANGE_ALL },
		 { icSigProfileSequenceDescTag,		ICMTVRANGE_ALL },
		 { icSigColorantTableOutTag,		ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigDToB0Tag, 					ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}
	},

    {icSigColorSpaceClass,		ICMCSMR_ANY,	ICMCSMR_PCS,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigBToA0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigDToB0Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigDToB1Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigDToB2Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigDToB3Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD0Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD1Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD2Tag, 					ICMTVRANGE_23_PLUS },
		 { icSigBToD3Tag,		 			ICMTVRANGE_23_PLUS },
		 { icSigGamutTag,					ICMTVRANGE_23_PLUS },		/* Why ? */
		 { icMaxEnumTag }
		}
	},

    {icSigAbstractClass,		ICMCSMR_PCS,	ICMCSMR_PCS,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigDToB0Tag, 					ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}
	},

    {icSigNamedColorClass,		ICMCSMR_ANY,    ICMCSMR_PCS,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigNamedColorTag,				ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icMaxEnumTag }
		}
	},

    {icSigNamedColorClass,		ICMCSMR_ANY,    ICMCSMR_PCS,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigNamedColor2Tag,				ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icMaxEnumTag }
		}
	},

	{icMaxEnumClass}
};

/* Table of transform tags. */
/* None of these tags should be present if they are not */
/* in the required or optional list. */
icmTagSigVersRec icmTransTagTable[] = { 
	{ icSigRedColorantTag,		ICMTVRANGE_ALL },
	{ icSigGreenColorantTag,	ICMTVRANGE_ALL },
	{ icSigBlueColorantTag,		ICMTVRANGE_ALL },
	{ icSigRedTRCTag,			ICMTVRANGE_ALL },
	{ icSigGreenTRCTag,			ICMTVRANGE_ALL },
	{ icSigBlueTRCTag,			ICMTVRANGE_ALL },

	{ icSigAToB0Tag, 			ICMTVRANGE_ALL },
	{ icSigAToB1Tag, 			ICMTVRANGE_ALL },
	{ icSigAToB2Tag, 			ICMTVRANGE_ALL },
	{ icSigBToA0Tag, 			ICMTVRANGE_ALL },
	{ icSigBToA1Tag, 			ICMTVRANGE_ALL },
	{ icSigBToA2Tag, 			ICMTVRANGE_ALL },

	{ icSigDToB0Tag, 			ICMTVRANGE_ALL },
	{ icSigDToB1Tag, 			ICMTVRANGE_ALL },
	{ icSigDToB2Tag, 			ICMTVRANGE_ALL },
	{ icSigDToB3Tag, 			ICMTVRANGE_ALL },
	{ icSigBToD0Tag, 			ICMTVRANGE_ALL },
	{ icSigBToD1Tag, 			ICMTVRANGE_ALL },
	{ icSigBToD2Tag, 			ICMTVRANGE_ALL },
	{ icSigBToD3Tag, 			ICMTVRANGE_ALL },

	{ icMaxEnumTag },
};

/* ===================================================================== */
/* icc profile object implementation */

/*

	Loose ends:

*/

/*
	Profile check functionality:
	see icc_write_check()

	Should check on read and before write.
	Check tag, tagtype on adding a tag too.

	+ Header layout, alignment etc.
	+ Header enumerations

	+ Tag table layout
	+ Tag table alignments, padding etc.
	+ Tag type layout, alignment, enumerations etc.

	Overall:

	+ Tags are only used once
	+ Check tag known, version, tagtype (cmTagSigTable[])
	+ Check tagType known, version (icmTagTypeTable[])
	+ Required tags (icmClassTagTable[] & icmTransTagTable[])


	Possible response to errors:

		* Fatal. Can't functionally proceed
	  These can be configured as errors, warnings, supressed:
		* Not to spec.
		* Known extension
		* Unknown extension
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Tag table functions */

/* tag table Serialisation */
static void tagtable_serialise(icc *p, icmFBuf *b) {
	unsigned int i;

	icmSn_ui_UInt32(b, &p->count);	/* 0-3: Tag count */
	
	if (b->op == icmSnRead) {		/* Allocate space for tag info on read */
		p->al->free(p->al, p->data);
		if ((p->data = (icmTagRec *)p->al->malloc(p->al, p->count * sizeof(icmTagRec))) == NULL) {
			icm_err(p, ICM_ERR_MALLOC, "tagtable_serialise: Tag table malloc() failed");
			return;
		}
	}
	/* For each table entry */
	for(i = 0; i < p->count; i++) {
		icmSn_TagSig32(b, &p->data[i].sig);		/* 0-3:  Signture */
		icmSn_ui_UInt32(b, &p->data[i].offset);	/* 4-7:  Tag offset */
		icmSn_ui_UInt32(b, &p->data[i].size);	/* 8-11: Tag size */
		if (b->op == icmSnRead) {
	    	p->data[i].psize = 0;			/* Not determined on read */
	    	p->data[i].objp = NULL;			/* Read on demand */
		}
	}
}

// ~8 is there any point in a tagtable_check() ? - see icc_write_check()

/* Return the size of the tag table in the object */
static unsigned int tagtable_get_size(icc *p) {
	icmFBuf *b;
	if ((b = new_icmFBuf(p, icmSnSize, NULL, 0, 0)) == NULL)
		return 0;
	tagtable_serialise(p, b);
    return b->done(b);
}

/* Read the tag table and the tag types. */
/* Be completely paranoid about the tag table and its contents */
/* of is the file offset it should be read from. */
/* Return error code */
static int tagtable_read(icc *p, unsigned int size, unsigned int of) {
	unsigned int i, j;
	icmFBuf *b;
	
	/* Read the tag count */
	if ((b = new_icmFBuf(p, icmSnRead, p->rfp, of, 4)) == NULL)
		return p->e.c;
	icmSn_ui_UInt32(b, &p->count);
    b->done(b);
	if (p->e.c != ICM_ERR_OK)
		return p->e.c;

	/* And sanity check it */
	if (p->count > 357913940	/* (2^32-5)/12 */
	 || (p->count * 12 + 4) > p->maxs) {
		return icm_err(p, ICM_ERR_RD_FORMAT, "Tag count %d is too big for nominated file size",p->count);
	}
	size = 4 + p->count * 12;		/* Tag table size */
	p->mino += size;				/* Minimum offset of tags */
	p->maxs -= size;				/* maximum size for tags */

	/* Read the tag table */
	if ((b = new_icmFBuf(p, icmSnRead, p->rfp, of, size)) == NULL)
		return p->e.c;
	tagtable_serialise(p, b);
    b->done(b);
	if (p->e.c != ICM_ERR_OK)
		return p->e.c;

	/* Check that each tag lies within the nominated space available, */
	/* and has sane values. */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].offset < p->mino
		 || p->data[i].offset > p->header->size
		 || p->data[i].size < 4
		 || p->data[i].size > p->maxs
		 || (p->data[i].offset + p->data[i].size) < p->data[i].offset	/* Overflow */
		 || (p->data[i].offset + p->data[i].size) > p->header->size) {
			return icm_err(p, ICM_ERR_RD_FORMAT, "Tag %d is out of range of the nominated file size",i);
		}
	}
		
	/* Check that each tag does not overlap another tag. */
	/* It is permitted to enclose or be enclosed by another tag though. */
	for (i = 0; i < p->count; i++) {
		for(j = i+1; j < p->count; j++) {
			unsigned int x0, x1, y0, y1;
			x0 = p->data[i].offset;
			x1 = p->data[i].offset + p->data[i].size;
			y0 = p->data[j].offset;
			y1 = p->data[j].offset + p->data[j].size;
			if ( (x1 > y0 && x1 <= y1 && x0 < y0)
			  || (x0 >= y0 && x0 < y1 && x1 > y1)) {
				return icm_err(p, ICM_ERR_RD_FORMAT, "Tag %d overlaps tag %d",i,j);
			}
		}
	}

// ~~~~999 should check that all tags start on an aligned boundary

// ~~~~999 should check that any gaps between header & tags & tags is
//         the minimal needed for padding. Need to sort for this ??

// ~~~~999 should check that all padding is zero too.

	/* Seek to, and then read each tag type signature */
	for (i = 0; i < p->count; i++) {
		if ((b = new_icmFBuf(p, icmSnRead, p->rfp, p->of + p->data[i].offset, 4)) == NULL)
			return p->e.c;
		icmSn_TagTypeSig32(b, &p->data[i].ttype);
	    b->done(b);
	}

	return p->e.c;
}


/* Write the tagtable. size is the size in bytes to be written, */
/* of is the file offset it should be written to. */
/* Return error code */
static int tagtable_write(icc *p, unsigned int size, unsigned int of) {
	icmFBuf *b;
	if ((b = new_icmFBuf(p, icmSnWrite, p->wfp, of, size)) == NULL)
		return p->e.c;
	tagtable_serialise(p, b);
    b->done(b);
	return p->e.c;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Forward declarations */
static int icc_check_sig(icc *p, unsigned int *ttix, int rd,
		icTagSignature sig, icTagTypeSignature ttype, icTagTypeSignature uttype);


/* Return the reference to current read fp (if any) */
/* Delete after finished with it */
/* (Beware of any interactions with icc that is sharing it!) */
static icmFile *icc_get_rfp(icc *p) {
	if (p->rfp == NULL)
		return NULL;
	return p->rfp->reference(p->rfp);
}


/* Change the version to be non-default (ie. not 2.2.0), */
/* Return 0 if OK */
/* Return 1 on error */
static int icc_set_version(icc *p, icmTV ver) {

	if (p->header == NULL) {
		return icm_err(p, ICM_ERR_INTERNAL, "icc_set_version: No Header available");
	}

	switch (ver) {
		case ICMTV_20:
		case ICMTV_21:
		case ICMTV_22:
		case ICMTV_23:
		case ICMTV_24:
		    p->header->vers.majv = ver/10000;
			p->header->vers.minv = (ver/100) % 100;
			p->header->vers.bfv  = ver % 100;
			break;
		default:
			return icm_err(p, ICM_ERR_UNKNOWN_VERSION, "icc_set_version: Unsupported version %s",
			                                                              icmTVers2str(ver));
	}
	return p->e.c;
}

/* Check the profiles MD5 ID. We assume the file has already been read. */
/* Return 0 if OK, 1 if no ID to check, 2 if doesn't match, 3 if some other error. */
/* NOTE: this reads the whole file again, to compute the checksum. */
static int icc_check_id(
	icc *p,
	ORD8 *rid		/* Optionaly return computed ID */
) {
	unsigned char buf[128];
	ORD8 id[16];
	icmMD5 *md5 = NULL;
	unsigned int len;
	
	if (p->header == NULL) {
		return icm_err(p, ICM_ERR_INTERNAL, "icc_check_id: No Header available");
	}

	/* See if there is an ID to compare against */
	for (len = 0; len < 16; len++) {
		if (p->header->id[len] != 0)
			break;
	}
	if (len >= 16) {
		return 1; 
	}

	if ((md5 = new_icmMD5_a(&p->e, p->al)) == NULL) {
		return p->e.c;
	}
		
	/* Check the header */
	if (p->rfp->seek(p->rfp, p->of) != 0) {
		return icm_err(p, ICM_ERR_FILE_SEEK, "icc_check_id: Seek to header failed");
	}
	if (p->rfp->read(p->rfp, buf, 1, 128) != 128) {
		return icm_err(p, ICM_ERR_FILE_READ, "icc_check_id: Read of header failed");
	}

	/* Zero the appropriate bytes in the header */
	buf[44] = buf[45] = buf[46] = buf[47] = 0;
	buf[64] = buf[65] = buf[66] = buf[67] = 0;
	buf[84] = buf[85] = buf[86] = buf[87] =
	buf[88] = buf[89] = buf[90] = buf[91] =
	buf[92] = buf[93] = buf[94] = buf[95] =
	buf[96] = buf[97] = buf[98] = buf[99] = 0;

	md5->add(md5, buf, 128);

	len = p->header->size - 128;

	/* Suck in the rest of the profile */
	for (;len > 0;) {
		unsigned int rsize = 128;
		if (rsize > len)
			rsize = len;
		if (p->rfp->read(p->rfp, buf, 1, rsize) != rsize) {
			return icm_err(p, ICM_ERR_FILE_READ, "icc_check_id: Read of file chunk failed");
		}
		md5->add(md5, buf, rsize);
		len -= rsize;
	}

	md5->get(md5, id);
	md5->del(md5);

	if (rid != NULL) {
		for (len = 0; len < 16; len++)
			rid[len] = id[len];
	}

	/* Check the ID */
	for (len = 0; len < 16; len++) {
		if (p->header->id[len] != id[len])
			break;
	}
	if (len >= 16) {
		return 0;		/* Matched */ 
	}
	return 2;			/* Didn't match */
}

/* Return true if sig & chans match icmCSMR */
static int icmCSMR_match(icmCSMR *p, icColorSpaceSignature sig, int nchans) {

	if (p->min == 0
	 || p->max == 0
	 || (nchans >= p->min && nchans <= p->max)) {
		unsigned int mask = icmCSSig2type(sig);

		switch(p->mf) {
    		case icmCSMF_ANY:
				return 1;
				
    		case icmCSMF_XYZ:
				return (sig == icSigXYZData);
				
    		case icmCSMF_Lab:
				return (sig == icSigLabData);
				
    		case icmCSMF_PCS:
				return (mask & CSSigType_PCS);
				
    		case icmCSMF_DEV:
				return (mask & CSSigType_DEV);
				
    		case icmCSMF_NCOL:
				return (mask & CSSigType_NCOL);
				
    		case icmCSMF_NOT_NCOL:
				return !(mask & CSSigType_NCOL);
				
    		case icmCSMF_DEV_NOT_NCOL:
				return (mask & CSSigType_DEV) && !(mask & CSSigType_NCOL);
		}
	}

	return 0;
}


/*
	~~~~8888
	Stuff that is missing ?

	From V2.2+:
		CMM Type and Primary Platform sig are now allowed to be set to zero.
		A Tag can only appear once in a profile
*/

/* Check that the ICC profile looks like it will be legal to write. */
/* Return non-zero and set error string if not */
static int icc_write_check(
	icc *p
) {
	int i, j, k;
	icProfileClassSignature clsig;
	icColorSpaceSignature colsig;
	int      	          dchans;
	icColorSpaceSignature pcssig;
	int      	          pchans;

	p->op = icmSnWrite;			/* Let check & quirk know direction */

	if (p->header == NULL) {
		return icm_err(p, 1, "icc_check: Header is missing");
	}

	/* Check the header */
	if (p->header->check(p->header, 0) != ICM_ERR_OK)
		return p->e.c;
	
	/* Check tags and tagtypes */
	for (i = 0; i < p->count; i++) {	/* For all the tag element data */
		icTagTypeSignature ttype, uttype;

		if (p->data[i].objp == NULL)
			return icm_err(p, ICM_ERR_INTERNAL, "icc_write_check: NULL tag element");

		uttype = p->data[i].ttype;			/* Actual type */
		ttype = p->data[i].objp->ttype;		/* Actual or icmSigUnknownType */ 

		/* Do tag & tagtype check */
		if (icc_check_sig(p, NULL, 0, p->data[i].sig, ttype, uttype) != ICM_ERR_OK)
			return p->e.c;

		/* If there are shared tagtypes, check compatibility */
		if (p->data[i].objp->refcount > 1) {
			int k;
			/* Search for shared tagtypes */
			for (k = 0; k < p->count; k++) {
				if (i == k || p->data[k].objp != p->data[i].objp)
					continue;

				/* Check the tagtype classes are compatible */
				if (p->get_tag_lut_class(p, p->data[i].sig) != p->get_tag_lut_class(p, p->data[k].sig)) {
					return icm_err(p, ICM_ERR_CLASSMISMATCH,
					     "icc_write_check: Tag '%s' is link to incompatible tag '%s'",
				          icmTagSignature2str(p->data[i].sig), icmTagSignature2str(p->data[k].sig));
				}
			}
		}

		/* Involke any other checking for this tag */
		if (p->data[i].objp->check != NULL)
			p->data[i].objp->check(p->data[i].objp, p->data[i].sig); 
		if (p->e.c != ICM_ERR_OK)
			return p->e.c;
	}

	if ((p->cflags & icmCFlagNoRequiredCheck) == 0) {
		/* Check that required tags are present */
		clsig  = p->header->deviceClass;
		colsig = p->header->colorSpace;
		dchans = icmCSSig2nchan(colsig);
		pcssig = p->header->pcs;
		pchans = icmCSSig2nchan(pcssig);

//	printf("dchans %d, pchans %d\n",dchans,pchans);

		/* Find a matching class entry */
		for (i = 0; p->classtagtable[i].clsig != icMaxEnumType; i++) {
			if (p->classtagtable[i].clsig == clsig
			 && icmCSMR_match(&p->classtagtable[i].colsig, colsig, dchans)
			 && icmCSMR_match(&p->classtagtable[i].pcssig, pcssig, pchans)
			 && icmVersInRange(p, &p->classtagtable[i].vrange)) {

				/* Found entry, so now check that all the required tags are present. */
				for (j = 0; p->classtagtable[i].tags[j].sig != icMaxEnumType; j++) {
					icColorSpaceSignature sig = p->classtagtable[i].tags[j].sig;

			 		if (!icmVersInRange(p, &p->classtagtable[i].tags[j].vrange))
						continue;		/* Only if applicable to this file version */

					if (p->find_tag(p, sig) != 0) {	/* Not present! */
#ifdef NEVER
						printf("icc_write_check: deviceClass %s is missing required tag %s\n", icmtag2str(clsig), sig);
#endif
						return icm_err(p, ICM_FMT_REQUIRED_TAG,
						          "icc_write_check: deviceClass %s is missing required tag %s",
						               icmtag2str(clsig), icmtag2str(sig));
					}
				}

				/* Found all required tags. */
				
				/* Check that any transtagtable[] tags are expected in this class */
				for (j = 0; p->transtagtable[j].sig != icMaxEnumType; j++) {
					icColorSpaceSignature sig = p->transtagtable[j].sig;

			 		if (!icmVersInRange(p, &p->transtagtable[j].vrange))
						continue;		/* Only if applicable to this file version */

					if (p->find_tag(p, sig) == 0) {

						/* See if this tag is in the required list */
						for (k = 0; p->classtagtable[i].tags[k].sig != icMaxEnumType; k++) {
					 		if (!icmVersInRange(p, &p->classtagtable[i].tags[k].vrange))
								continue;		/* Only if applicable to this file version */
							if (sig == p->classtagtable[i].tags[k].sig)
								break;			/* Found it */
						}
						if (p->classtagtable[i].tags[k].sig != icMaxEnumType)
							continue;		/* Yes it is */


						/* See if this tag is in the optional list */
						for (k = 0; p->classtagtable[i].otags[k].sig != icMaxEnumType; k++) {
					 		if (!icmVersInRange(p, &p->classtagtable[i].otags[k].vrange))
								continue;		/* Only if applicable to this file version */
							if (sig == p->classtagtable[i].otags[k].sig)
								break;			/* Found it */
						}
						if (p->classtagtable[i].otags[k].sig != icMaxEnumType)
							continue;		/* Yes it is */
		
						/* Naughty tag */
						icmQuirkWarning(p, ICM_FMT_UNEXPECTED_TAG, 0, 
						          "icc_write_check: deviceClass %s has unexpected tag %s",
						               icmtag2str(clsig), icmtag2str(sig));
					}
				}
				break;			/* Found a matching class entry */
			}
		}

		/* Didn't find matching class entry */
		if (p->classtagtable[i].clsig == icMaxEnumType) {
			return icm_err(p, ICM_FMT_UNKNOWN_CLASS,
			          "icc_write_check: deviceClass %s PCS %s dev %s is not known\n",
			               icmtag2str(clsig), icmColorSpaceSignature2str(pcssig),
							                icmColorSpaceSignature2str(colsig));
		}
	}

	return 0;	/* Assume anything is ok */
}


static void icc_setup_wpchtmx(icc *p);
void icmQuantize3x3S15Fixed16(double targ[3], double mat[3][3], double in[3]);

/* See if there is an ArgyllCMS 'arts' tag, and if so setup the wpchtmx[][] matrix from it. */
/* See if there is an 'chad' tag, and if so setup the chadmx[][] matrix from it. */
static void icc_read_arts_chad(icc *p) {

	/* Check if there is an ArgyllCMS 'arts' tag, and setup the wpchtmx[][] matrix from it. */
	{
		icmS15Fixed16Array *artsTag;
	
		if ((artsTag = (icmS15Fixed16Array *)p->read_tag(p, icmSigAbsToRelTransSpace)) != NULL
		 && artsTag->ttype == icSigS15Fixed16ArrayType
		 && artsTag->count >= 9) {
		
			p->wpchtmx[0][0] = artsTag->data[0];
			p->wpchtmx[0][1] = artsTag->data[1];
			p->wpchtmx[0][2] = artsTag->data[2];
			p->wpchtmx[1][0] = artsTag->data[3];
			p->wpchtmx[1][1] = artsTag->data[4];
			p->wpchtmx[1][2] = artsTag->data[5];
			p->wpchtmx[2][0] = artsTag->data[6];
			p->wpchtmx[2][1] = artsTag->data[7];
			p->wpchtmx[2][2] = artsTag->data[8];

			icmInverse3x3(p->iwpchtmx, p->wpchtmx);

			p->useArts = 1;		/* Save it if it was in profile */

		} else {

			/* If an ArgyllCMS created profile, or if it's a Display profile, */
			/* use Bradford. This makes sRGB and AdobeRGB etc. work correctly */
			/* for absolute colorimetic. Note that for display profiles that set */
			/* the WP to D50 and store their chromatic transform in the 'chad' tag, */
			/* (i.e. some V2 profiles and all V4 profiles) this will have no effect */
			/* on the Media Relative WP Transformation since D50 -> D50, and */
			/* the 'chad' tag will be used to set the internal MediaWhite value */
			/* and transform matrix. */
			if (p->header->creator == icmstr2tag("argl")
			 || p->header->deviceClass == icSigDisplayClass) {

				icmCpy3x3(p->wpchtmx, icmBradford);
				icmInverse3x3(p->iwpchtmx, p->wpchtmx);
	
			/* Default to ICC standard Wrong Von Kries */
			/* for non-ArgyllCMS, non-Display profiles. */
			} else {
				icmCpy3x3(p->wpchtmx, icmWrongVonKries);
				icmCpy3x3(p->iwpchtmx, icmWrongVonKries);
			}
		
			p->useArts = 0;		/* Don't save it, as it wasn't in profile */
		}

		p->wpchtmx_class = p->header->deviceClass;		/* It's set for this class now */
	}

	/* If this is a Display or Output profile, check if there is a 'chad' tag, and read it */
	/* in if it exists. We will use this latter when we interpret absolute colorimetric, */
	/* and this also prevents auto creation of a chad tag on write if wrD/OChad is set. */
	{
		icmS15Fixed16Array *chadTag;

	 	if ((p->header->deviceClass == icSigDisplayClass
	 	  || p->header->deviceClass == icSigOutputClass)
		 && (chadTag = (icmS15Fixed16Array *)p->read_tag(p, icSigChromaticAdaptationTag)) != NULL
		 && chadTag->ttype == icSigS15Fixed16ArrayType
		 && chadTag->count == 9) {
			
			p->chadmx[0][0] = chadTag->data[0];
			p->chadmx[0][1] = chadTag->data[1];
			p->chadmx[0][2] = chadTag->data[2];
			p->chadmx[1][0] = chadTag->data[3];
			p->chadmx[1][1] = chadTag->data[4];
			p->chadmx[1][2] = chadTag->data[5];
			p->chadmx[2][0] = chadTag->data[6];
			p->chadmx[2][1] = chadTag->data[7];
			p->chadmx[2][2] = chadTag->data[8];

			p->naturalChad = 1;
			p->chadmxValid = 1;
		}
	}

	/* It would be nice to have an option to convert 'chad' based profile */
	/* into non-chad profiles, but this is non trivial, since the wpchtmx would */
	/* need to be determined from the chad matrix. While this is technically */
	/* possible (see chex.c for an attempt at this), it is not easy, and */
	/* it's possible for the chad matrix to be a non Von Kries type transformation, */
	/* which cannot be exactly decomposed into a cone space matrix + Von Kries scaling. */ 
}

static icmBase *icc_add_tag_imp(icc *p, icTagSignature sig, icTagTypeSignature ttype);

/* Add any automatically created tags. */
/* If this is called from icc_get_size() then wr = z, */
/* If wr is nz, modify white point value (i.e. in middle of ->write()) */
/* The 'chad' tag is only added if there is no natural 'chad' tag, */
/* and will be remove once the write is complete. */
static int icc_add_auto_tags(icc *p, int wr) {

	p->op = wr ? icmSnWrite : icmSnRead;		/* Let check know direction */

	/* If we're using the ArgyllCMS 'arts' tag to record the chromatic */
	/* adapation cone matrix used for the Media Relative WP Transformation, */ 
	/* create it and set it from the wpchtmx[][] matrix. */
	/* Don't write it if there is no 'wtpt' tag (i.e. it's a device link) */
	if (p->useArts
	 && p->find_tag(p, icSigMediaWhitePointTag) == 0)  {
		int rv;
		icmBase *tag;
		icmS15Fixed16Array *artsTag;

		/* Make sure wpchtmx[][] has been set correctly for device class */
		if (p->wpchtmx_class != p->header->deviceClass) {
			icc_setup_wpchtmx(p);
		}

		/* Add an 'arts' tag if one doesn't exist or is the wrong tagtype */
		if ((tag = p->read_tag(p, icmSigAbsToRelTransSpace)) == NULL
		  || tag->ttype != icSigS15Fixed16ArrayType) {
			if (tag != NULL) {
				if (p->delete_tag_quiet(p, icmSigAbsToRelTransSpace) != ICM_ERR_OK) {
					return icm_err(p, 1,"icc_write: Deleting existing 'arts' tag failed");
				}
			}
			if ((artsTag = (icmS15Fixed16Array *)icc_add_tag_imp(p, icmSigAbsToRelTransSpace,
			                                     icSigS15Fixed16ArrayType)) == NULL) {
				return icm_err(p, 1,"icc_write: Adding 'arts' tag failed");
			}
		} else {
			artsTag = (icmS15Fixed16Array *)tag;
		}
		artsTag->count = 9;
		if ((rv = artsTag->allocate(artsTag)	) != 0) {
			return icm_err(p, 1,"icc_write: Allocating 'arts' tag failed");
		}

		/* The cone matrix is assumed to be arranged conventionaly for matrix */
		/* times vector multiplication. */
		/* Consistent with ICC usage, the dimension corresponding to the matrix */
		/* rows varies least rapidly while the one corresponding to the matrix */
		/* columns varies most rapidly. */
		artsTag->data[0] = p->wpchtmx[0][0];
		artsTag->data[1] = p->wpchtmx[0][1];
		artsTag->data[2] = p->wpchtmx[0][2];
		artsTag->data[3] = p->wpchtmx[1][0];
		artsTag->data[4] = p->wpchtmx[1][1];
		artsTag->data[5] = p->wpchtmx[1][2];
		artsTag->data[6] = p->wpchtmx[2][0];
		artsTag->data[7] = p->wpchtmx[2][1];
		artsTag->data[8] = p->wpchtmx[2][2];
	}

	/* If this is a Display profile, and we have been told to save it in */
	/* ICCV4 style, then set the media white point tag to D50 and save */
	/* the chromatic adapation matrix to the 'chad' tag. */
	{
		int rv;
		icmXYZArray *whitePointTag;
		icmS15Fixed16Array *chadTag;
		
	 	if (p->header->deviceClass == icSigDisplayClass
		 && p->wrDChad && !p->naturalChad
		 && (whitePointTag = (icmXYZArray *)p->read_tag(p, icSigMediaWhitePointTag)) != NULL
		 && whitePointTag->ttype == icSigXYZType
		 && whitePointTag->count >= 1) {
	
			/* If we've set this profile, not just read it, */
			/* compute the fromAbs/chad matrix from media white point and cone matrix */
			if (!p->chadmxValid) {
				double wp[3];
				p->chromAdaptMatrix(p, ICM_CAM_NONE, NULL, p->chadmx,
				                       icmD50, whitePointTag->data[0]);  

				/* Optimally quantize chad matrix to preserver white point */
				icmXYZ2Ary(wp, whitePointTag->data[0]);
				icmQuantize3x3S15Fixed16(icmD50_ary3, p->chadmx, wp);
				p->chadmxValid = 1;
			}
	
			/* Make sure no 'chad' tag currently exists */
			if (p->delete_tag_quiet(p, icSigChromaticAdaptationTag) != ICM_ERR_OK) {
				return icm_err(p, 1,"icc_write: Deleting existing 'chad' tag failed");
			}
	
			/* Add one */
			if ((chadTag = (icmS15Fixed16Array *)icc_add_tag_imp(p, icSigChromaticAdaptationTag,
				                                     icSigS15Fixed16ArrayType)) == NULL) {
				return icm_err(p, 1,"icc_write: Adding 'chad' tag failed");
			}
			chadTag->count = 9;
			if ((rv = chadTag->allocate(chadTag)	) != 0) {
				return icm_err(p, 1,"icc_write: Allocating 'chad' tag failed");
			}

			p->tempChad = 1;
	
			if (wr) {
				/* Save in ICC matrix order */
				chadTag->data[0] = p->chadmx[0][0];
				chadTag->data[1] = p->chadmx[0][1];
				chadTag->data[2] = p->chadmx[0][2];
				chadTag->data[3] = p->chadmx[1][0];
				chadTag->data[4] = p->chadmx[1][1];
				chadTag->data[5] = p->chadmx[1][2];
				chadTag->data[6] = p->chadmx[2][0];
				chadTag->data[7] = p->chadmx[2][1];
				chadTag->data[8] = p->chadmx[2][2];

				/* Set 'chad' adjusted white point */
				p->tempWP = whitePointTag->data[0];
				whitePointTag->data[0] = icmD50;
			}
		}
	}

	/* If this is an Output profile with a non-standard illuminant set, */
	/* and we have been told to save it using a 'chad' tag to represent */
	/* the illuminant difference, then adjust the media white point tag */
	/* for the illuminant, and change the 'chad' tag. */
	{
		int rv;
		icmXYZArray *whitePointTag;
		icmS15Fixed16Array *chadTag;
		
	 	if (p->header->deviceClass == icSigOutputClass
		 && p->chadmxValid
		 && p->wrOChad && !p->naturalChad
		 && (whitePointTag = (icmXYZArray *)p->read_tag(p, icSigMediaWhitePointTag)) != NULL
		 && whitePointTag->ttype == icSigXYZType
		 && whitePointTag->count >= 1) {
			double wp[3];
	
			/* Make sure no 'chad' tag currently exists */
			if (p->delete_tag_quiet(p, icSigChromaticAdaptationTag) != ICM_ERR_OK) {
				return icm_err(p, 1,"icc_write: Deleting existing 'chad' tag failed");
			}
	
			/* Add one */
			if ((chadTag = (icmS15Fixed16Array *)icc_add_tag_imp(p, icSigChromaticAdaptationTag,
				                                     icSigS15Fixed16ArrayType)) == NULL) {
				return icm_err(p, 1,"icc_write: Adding 'chad' tag failed");
			}
			chadTag->count = 9;
			if ((rv = chadTag->allocate(chadTag)	) != 0) {
				return icm_err(p, 1,"icc_write: Allocating 'chad' tag failed");
			}
	
			p->tempChad = 1;

			if (wr) {
				/* Save in ICC matrix order */
				chadTag->data[0] = p->chadmx[0][0];
				chadTag->data[1] = p->chadmx[0][1];
				chadTag->data[2] = p->chadmx[0][2];
				chadTag->data[3] = p->chadmx[1][0];
				chadTag->data[4] = p->chadmx[1][1];
				chadTag->data[5] = p->chadmx[1][2];
				chadTag->data[6] = p->chadmx[2][0];
				chadTag->data[7] = p->chadmx[2][1];
				chadTag->data[8] = p->chadmx[2][2];
	
				/* Transform white point to take 'chad' into account */
				p->tempWP = whitePointTag->data[0];
				icmXYZ2Ary(wp, whitePointTag->data[0]);
				icmMulBy3x3(wp, p->chadmx, wp);
				icmAry2XYZ(whitePointTag->data[0], wp);
			}
		}
	}

	return 0;
}

/* Restore profile after creating temporary 'chad' tag, and */
/* modifying the white point. */
static int icc_rem_temp_tags(icc *p) {

	/* Restore profile if Display 'chad' has been temporarily added. */
	{
		int rv;
		icmXYZArray *whitePointTag;
		icmS15Fixed16Array *chadTag;
		
	 	if (p->header->deviceClass == icSigDisplayClass
		 && p->tempChad && p->wrDChad && !p->naturalChad
		 && (whitePointTag = (icmXYZArray *)p->read_tag(p, icSigMediaWhitePointTag)) != NULL
		 && whitePointTag->ttype == icSigXYZType
		 && whitePointTag->count >= 1) {
	
			/* Remove temporary 'chad' tag */
			if (p->delete_tag_quiet(p, icSigChromaticAdaptationTag) != ICM_ERR_OK) {
				return icm_err(p, 1,"icc_write: Deleting temporary 'chad' tag failed");
			}
	
			/* Restore original white point */
			whitePointTag->data[0] = p->tempWP;
			p->tempChad = 0;
		}
	}

	/* Restore profile if Output 'chad' has been temporarily added. */
	{
		int rv;
		icmXYZArray *whitePointTag;
		icmS15Fixed16Array *chadTag;
		
	 	if (p->header->deviceClass == icSigOutputClass
		 && p->tempChad && p->wrOChad && !p->naturalChad
		 && (whitePointTag = (icmXYZArray *)p->read_tag(p, icSigMediaWhitePointTag)) != NULL
		 && whitePointTag->ttype == icSigXYZType
		 && whitePointTag->count >= 1) {
			double wp[3];
	
			/* Remove temporary 'chad' tag */
			if (p->delete_tag_quiet(p, icSigChromaticAdaptationTag) != ICM_ERR_OK) {
				return icm_err(p, 1,"icc_write: Deleting temporary 'chad' tag failed");
			}

			/* Restore original white point */
			whitePointTag->data[0] = p->tempWP;
			p->tempChad = 0;
		}
	}

	return 0;
}

/* Return the total size needed for the profile. */
/* Sets size and padded size values in various objects, */
/* and re-computes the reference counts. */
/* This will add any automatic tags such as 'arts' tag, */
/* so the current information needs to be final enough */
/* for the automatic tag creation to be correct. */
/* Auto 'chad' tag will be added and remove if so configured. */
/* Return 0 on error, and set error code and message in tag manager. */
static unsigned int icc_get_size(icc *p) {
	unsigned int i, size = 0, nsize;

	/* Compute the total size and tag element data offsets */
	if (p->header == NULL) {
		icm_err(p, ICM_ERR_INTERNAL, "icc_get_size: No Header available");
		return 0;
	}

	/* Add 'arts' tag and temporary 'chad' tag if so configured */
	icc_add_auto_tags(p, 0);

	nsize = sat_add(size, (p->header->hsize = p->header->get_size(p->header)));		/* Header */
	p->header->hsize = nsize - size;
	nsize = sat_align(ALIGN_SIZE, nsize);
	p->header->phsize = nsize - size;
	/* (Could call tagtable_serialise() with icmSnSize to compute tag table size) */
	size = sat_addaddmul(nsize, 4, p->count, 12);			/* Tag table length */
	size = sat_align(ALIGN_SIZE, size);						/* Header + tage table length */
	p->pttsize = size - nsize;

	if (size == UINT_MAX) {
		icm_err(p, 1,"icc_get_size: size overflow");
		return 0;
	}

	/* Check that every tag has an object */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].objp == NULL) {
			icm_err(p, ICM_ERR_INTERNAL, "icc_get_size: NULL tag element");
			icc_rem_temp_tags(p);
			return 0;
		}
	}

	/* Reset touched flag for each tag type */
	for (i = 0; i < p->count; i++)
		p->data[i].objp->touched = 0;

	for (i = 0; i < p->count; i++) {
		if (p->data[i].objp->touched == 0) {	/* Not seen this previously */
			p->data[i].offset = size;			/* Profile relative target */
			nsize = sat_add(size, (p->data[i].size = p->data[i].objp->get_size(p->data[i].objp)));
			nsize = sat_align(p->align, nsize);
			p->data[i].psize = nsize - size;
			size = nsize;
			p->data[i].objp->refcount = 1;
			p->data[i].objp->touched = 1;	/* Don't account for this again */

		} else { /* must be linked - copy allocation */
			unsigned int k;
			for (k = 0; k < p->count; k++) {	/* Find linked tag */
				if (p->data[k].objp == p->data[i].objp)
					break;
			}
			if (k == p->count) {	/* Didn't find link */
				icm_err(p, ICM_ERR_INTERNAL, "icc_get_size: Corrupted tag-tag link");
				icc_rem_temp_tags(p);
				return 0;
			}
		    p->data[i].offset = p->data[k].offset;
		    p->data[i].size   = p->data[k].size;
		    p->data[i].psize  = p->data[k].psize;
			p->data[i].objp->refcount++;
		}
	}

	icc_rem_temp_tags(p);

	return size;	/* Total size needed, or UINT_MAX if overflow */
}

/* read the profile, return 0 on success, error code on fail. */
/* NOTE: this doesn't read the tag types, it just reads the header */
/* and the tag table. Actual tag data is read on demand, or */
/* by calling read_all_tags(). The file object that was used */
/* for this read is saved and used for read_tag(), read_all_tags() and dump(). */
/* NOTE: fp ownership is taken even if the function fails. */
static int icc_read(
	icc *p,
	icmFile *fp,			/* File to read from */
	unsigned int of		/* File offset to read from */
) {
	p->rfp = fp->reference(fp);
	p->of = of;
	p->fp = p->rfp;					// ~~~8888 for legacy code

	p->op = icmSnRead;		// ~7

	if (p->header == NULL) {
		return icm_err(p, ICM_ERR_INTERNAL, "icc_read: Internal, no header defined");
	}

	/* Read the header. */
	if (p->header->read(p->header, p->header->hsize, of) != ICM_ERR_OK)
		return p->e.c;

	/* Set values needed by tag table read */ 
	p->mino = p->header->hsize;				/* Minimum offset allowing for header */
	p->maxs = p->header->size - p->mino;	/* Maximum size for tag table and tags */

	if (p->header->size < p->mino) {
		return icm_err(p, ICM_ERR_RD_FORMAT, "ICC Format error - size of file is smaller than header");
	}
	if (p->maxs < 4) {
		return icm_err(p, ICM_ERR_RD_FORMAT, "ICC Format error - size of file too small for tag table");
	}

	/* Read in the tag table */
	if (tagtable_read(p, 4, of + p->header->hsize) != ICM_ERR_OK)
		return p->e.c;

	/* Read any 'arts' and 'chad' and setup related matrices */
	icc_read_arts_chad(p);

	return p->e.c;
}

/* Write the contents of the profile. Return error code on failure */
/* NOTE: fp ownership is taken even if the function fails. */
static int icc_write(
	icc *p,
	icmFile *fp,		/* File to write to */
	unsigned int of	/* File offset to write to */
) {
	unsigned int len;
	unsigned int i, size;
	int rv;

	/* Add 'arts' tag and temporary 'chad' tag and modify white point, if so configured */
	if ((rv = icc_add_auto_tags(p, 1)) != 0)
		return rv;

	p->wfp = fp->reference(fp);	/* Open file pointer */
	p->of = of;					/* Offset of ICC profile */
	p->fp = p->wfp;				// ~~~8888 for legacy write routines

	p->op = icmSnSize;		// ~7

	/* Compute the total size and tag element data offsets, and */
	/* setup sizes, offsets, other dependent flags etc. */
	p->header->size = size = icc_get_size(p);

	p->op = icmSnWrite;		// ~7

	/* Before writing, run a complete validity check */
	if (p->write_check(p) != ICM_ERR_OK) {
		icc_rem_temp_tags(p);
		return p->e.c;
	}

	/* Reset touched flag for each tag type */
	for (i = 0; i < p->count; i++)
		p->data[i].objp->touched = 0;

	/* If V4.0+, Compute the MD5 id for the profile. */
	/* We do this by writing to a fake icmFile */
	if (p->header->vers.majv >= 4) {
		icmMD5 *md5 = NULL;
		icmFile *ofp, *dfp = NULL;

		if ((md5 = new_icmMD5_a(&p->e, p->al)) == NULL) {
			icc_rem_temp_tags(p);
			return icm_err(p, 2,"icc_write: new_icmMD5 failed");
		}
		
		if ((dfp = new_icmFileMD5_a(md5, p->al)) == NULL) {
			md5->del(md5);
			icc_rem_temp_tags(p);
			return icm_err(p, 2,"icc_write: new_icmFileMD5 failed");
		}
		
		ofp = p->wfp;
		p->wfp = dfp;
		p->fp = p->wfp;				// ~~~8888 for legacy write routines

		p->op = icmSnWrite;			// ~7

		/* Dummy write the header */
		p->header->doid = 1;
		if (p->header->write(p->header, p->header->phsize, of) != ICM_ERR_OK) {
			p->header->doid = 0;
			icc_rem_temp_tags(p);
			return p->e.c;
		}
		p->header->doid = 0;
	
		/* Dummy write the tag table */
		if (tagtable_write(p, p->pttsize, of + p->header->phsize) != ICM_ERR_OK) {
			icc_rem_temp_tags(p);
			return p->e.c;
		}
		
		/* Dummy write all the tag element data */
		for (i = 0; i < p->count; i++) {	/* For all the tag element data */
			if (p->data[i].objp->touched != 0)
				continue;		/* Must be linked, and we've already written it */
			if (p->data[i].objp->write(
			          p->data[i].objp, p->data[i].psize, of + p->data[i].offset) != ICM_ERR_OK) { 
				icc_rem_temp_tags(p);
				return p->e.c;
			}
			p->data[i].objp->touched = 1;	/* Written it, so don't write it again. */
		}
	
		if (p->wfp->flush(p->wfp) != 0) {
			icc_rem_temp_tags(p);
			return icm_err(p, ICM_ERR_FILE_WRITE, "icc_write: file flush failed");
		}

		/* Get the MD5 checksum ID */
		md5->get(md5, p->header->id);

		/* Restore fp */
		dfp->del(dfp);
		md5->del(md5);
		p->wfp = ofp;
		p->fp = p->wfp;				// ~~~8888 for legacy write routines

		/* Reset touched flag for each tag type again */
		for (i = 0; i < p->count; i++)
			p->data[i].objp->touched = 0;
	}

	/* Now write out the profile for real. */
	/* Although it may appear like we're seeking for each element, */
	/* in fact elements will be written in file order. */

	/* Write the header */
	if (p->header->write(p->header, p->header->phsize, of) != ICM_ERR_OK) {
		icc_rem_temp_tags(p);
		return p->e.c;
	}

	/* Write the tag table */
	if (tagtable_write(p, p->pttsize, of + p->header->phsize) != ICM_ERR_OK) {
		icc_rem_temp_tags(p);
		return p->e.c;
	}
	
	/* Write all the tag element data */
	for (i = 0; i < p->count; i++) {	/* For all the tag element data */
		if (p->data[i].objp->touched != 0)
			continue;		/* Must be linked, and we've already written it */
		if (p->data[i].objp->write(
		          p->data[i].objp, p->data[i].psize, of + p->data[i].offset) != ICM_ERR_OK) { 
			icc_rem_temp_tags(p);
			return p->e.c;
		}
		p->data[i].objp->touched = 1;	/* Written it, so don't write it again. */
	}

	if (p->wfp->flush(p->wfp) != 0) {
		return icm_err(p, ICM_ERR_FILE_WRITE, "icc_write: file flush failed");
	}

	icc_rem_temp_tags(p);

	return p->e.c;
}

static icmBase *icc_read_tag_ix(icc *p, unsigned int i);
static int icc_unread_tag_ix(icc *p, unsigned int i); 

/* Dump whole object */
static void icc_dump(
	icc *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level, 0 = print nothing. */
	                /* 1 = minimal (nothing from individual tags) */
	                /* 2 = moderate (some from individual tags) */
	                /* 3 = all (complete contents) */
) {
	unsigned int i;

	if (verb <= 0)
		return;

	op->printf(op,"icc:\n");

	/* Dump the header */
	if (p->header != NULL)
		p->header->dump(p->header, op, verb);

	/* Dump all the tag elements */
	for (i = 0; i < p->count; i++) {	/* For all the tag element data */
		icmBase *ob;
		int tr = 0;						/* Flag, temporarily loaded */
		op->printf(op,"tag %d:\n",i);
		op->printf(op,"  sig      %s\n",icmtag2str(p->data[i].sig)); 
		op->printf(op,"  type     %s\n",icmtag2str(p->data[i].ttype)); 
		op->printf(op,"  offset   %d\n", p->data[i].offset);
		op->printf(op,"  size     %d\n", p->data[i].size);

		if (p->data[i].objp == NULL) {
			/* The object is not loaded, so load it then free it */
			if (icc_read_tag_ix(p, i) == NULL)
				op->printf(op,"Got read error 0x%x, '%s'\n",p->e.c, p->e.m);
			tr = 1;
		}
		if ((ob = p->data[i].objp) != NULL) {
			/* op->printf(op,"  refcount %d\n", ob->refcount); */
			ob->dump(ob,op,verb-1);

			if (tr != 0) {		/* Cleanup if temporary */
				icc_unread_tag_ix(p, i);
			}
		}
		op->printf(op,"\n");
	}
}

/* Delete the icc */
static void icc_del(
	icc *p
) {
	unsigned int i;
	icmAlloc *al = p->al;

	/* Free up the header */
	if (p->header != NULL)
		p->header->del(p->header);

	/* Free up the tag data objects */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].objp != NULL) {
			if (--(p->data[i].objp->refcount) == 0)		/* decrement reference count */
				p->data[i].objp->del(p->data[i].objp);	/* Last reference */
	  	  	p->data[i].objp = NULL;
		}
	}

	/* Free tag table */
	al->free(al, p->data);

	/* Delete file objects if these are the last refernces to them */
	if (p->rfp != NULL)
		p->rfp->del(p->rfp);
	if (p->wfp != NULL)
		p->wfp->del(p->wfp);

	/* This object */
	al->free(al, p);

	/* Delete allocator if this is the last reference to it */
	al->del(al);
}

/* Check that a given tag and tag type look valid */
/* A ttype of icmSigUnknownType will be ignored. */
/* Set e.c and e.m if ttagtype is not recognized. */
/* If the sig is know, check that the tagtype is valid for that sig. */
static int icc_check_sig(
	icc *p,
	unsigned int *ttix,			/* If not NULL, return the tagtypetable index */
	int rd,						/* Flag, NZ if reading, else writing */
    icTagSignature sig,			/* Tag signature */
	icTagTypeSignature ttype,	/* Tag type, possibly icmSigUnknownType */
	icTagTypeSignature uttype	/* Actual tag type if Unknown */
) {
	unsigned int i, j;

	if (ttix != NULL)
		*ttix = 0xffffffff;			// Out of range value

	/* Check if the tag type is a known type */
	if (ttype != icmSigUnknownType) {

		for (j = 0; p->tagtypetable[j].ttype != ttype
		         && p->tagtypetable[j].ttype != icMaxEnumType; j++)
			;
	
		if (p->tagtypetable[j].ttype == icMaxEnumType) {	/* Unknown tagtype */
			return icm_err(p, rd ? ICM_ERR_RD_FORMAT : ICM_ERR_WR_FORMAT,
			        "Tag Type '%s' is not known", icmTypeSignature2str(ttype));
		}

		/* Check that this tagtype is valid for the version. */
		if (!icmVersInRange(p, &p->tagtypetable[j].vrange)
		 && (  p->op != icmSnWrite											/* Not write */
		    || (p->cflags & icmCFlagAllowWrVersion) == 0					/* Not allow */
			|| !icmVersOverlap(&p->tagtypetable[j].vrange, &p->vcrange))) {	/* Not o'lap range */

			if (icmVersionWarning(p, ICM_VER_TYPEVERS, 
				  "Tag Type '%s' is not valid for file version %s (valid %s)\n",
				  icmTypeSignature2str(ttype), icmProfileVers2str(p),
				  icmTVersRange2str(&p->tagtypetable[j].vrange)) != ICM_ERR_OK) {
				return p->e.c;
}
		}

		if (ttix != NULL)
			*ttix = j;
	}
	

	/* Check if the tag signature is known. */
	/* If it is, then it should use one of the valid range of tagtypes for that tag sig. */
	/* If not, the tagtype could be icmSigUnknownType or any known type. */ 
	for (i = 0; p->tagsigtable[i].sig != sig
	         && p->tagsigtable[i].sig != icMaxEnumType; i++)
		;

	if (p->tagsigtable[i].sig != icMaxEnumType) {	/* Known signature */

		/* Check that this tag signature is valid for this version of file */
		if (!icmVersInRange(p, &p->tagsigtable[i].vrange)
		 && (  p->op != icmSnWrite											/* Not write */
		    || (p->cflags & icmCFlagAllowWrVersion) == 0
			|| !icmVersOverlap(&p->tagsigtable[i].vrange, &p->vcrange))) {

			if (icmVersionWarning(p, ICM_VER_SIGVERS, 
				  "Tag Sig '%s' is not valid for file version %s (valid %s)\n",
				  icmTagSignature2str(sig), icmProfileVers2str(p),
				  icmTVersRange2str(&p->tagsigtable[i].vrange)) != ICM_ERR_OK)
				return p->e.c;
		}
	
		/* Check that the tag type is permitted for the tag signature */ 
		/* (icmSigUnknownType will not be in this table) */
		for (j = 0; p->tagsigtable[i].ttypes[j].ttype != ttype
		         && p->tagsigtable[i].ttypes[j].ttype != icMaxEnumType; j++)
			;

		if (p->tagsigtable[i].ttypes[j].ttype == icMaxEnumType) { /* unexpected tagtype */

			if (ttype == icmSigUnknownType) {		/* Permit UnknownType with a warning */ 
				icmQuirkWarning(p, ICM_FMT_SIG2TYPE, 0, 
			      "Tag Sig '%s' uses unexpected Tag Type '%s'",
		          icmTagSignature2str(sig), icmTypeSignature2str(uttype));
				return p->e.c;						/* OK */
			}

			if (icmFormatWarning(p, ICM_FMT_SIG2TYPE, 
			      "Tag Sig '%s' uses unexpected Tag Type '%s'",
		          icmTagSignature2str(sig), icmTypeSignature2str(uttype)) != ICM_ERR_OK)
				return p->e.c;						/* Error or OK */

		} else {	/* Recognised ttype */
			/* Check that the signature to type is permitted for this version of file */
			if (!icmVersInRange(p, &p->tagsigtable[i].ttypes[j].vrange)
			 && (  p->op != icmSnWrite											/* Not write */
			    || (p->cflags & icmCFlagAllowWrVersion) == 0
				|| !icmVersOverlap(&p->tagsigtable[i].ttypes[j].vrange, &p->vcrange))) {

				if (icmVersionWarning(p, ICM_VER_SIG2TYPEVERS, 
					  "Tag Sig '%s' can't use Tag Type '%s' in file version %s (valid %s)",
			          icmTagSignature2str(sig), icmTypeSignature2str(uttype), icmProfileVers2str(p),
					  icmTVersRange2str(&p->tagsigtable[i].ttypes[j].vrange)) != ICM_ERR_OK)
					return p->e.c;					/* Error or OK */
			}
		}
	}

	return p->e.c;
}

/* Return the lut class value for a tag signature */
/* Return 0 if there is not sigtypetable setup, */
/* or the signature is not recognised. */
static icmLutClass icc_get_tag_lut_class(
	icc *p,
    icTagSignature sig			/* Tag signature */
) {
	unsigned int i;

	if (p->tagsigtable == NULL)
		return 0;

	for (i = 0; p->tagsigtable[i].sig != sig
	         && p->tagsigtable[i].sig != icMaxEnumType; i++)
		;

	if (p->tagsigtable[i].sig == icMaxEnumType)
		return 0;

	return p->tagsigtable[i].cclass;
}

/* Search for a tag signature in the profile. */
/* return: */
/* 0 if found */
/* 1 if found but not handled type */
/* 2 if not found */
/* NOTE: doesn't set icc->e.c or icc->e.m[].
/* NOTE: we don't handle tag duplication - you'll always get the first in the file. */
static int icc_find_tag(
	icc *p,
    icTagSignature sig			/* Tag signature - may be unknown */
) {
	unsigned int i;
	int j;

	/* Search for signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig)		/* Found it */
			break;
	}
	if (i == p->count)
		return 2;						/* Not found */

	/* See if we can handle this type */
	for (j = 0; p->tagtypetable[j].ttype != icMaxEnumType; j++) {
		if (p->tagtypetable[j].ttype == p->data[i].ttype)
			break;
	}
	if (p->tagtypetable[j].ttype == icMaxEnumType)
		return 1;						/* Not handled */

	/* Check if this type is handled by this version format */
	/* (We don't warn as this should have been done on read or add) */
	if (!icmVersInRange(p, &p->tagtypetable[j].vrange)) {
		return 1;
	}

	return 0;						/* Found and handled */
}

/* Read as specific tag element data, and return a pointer to the object */
/* (This is an internal function) */
/* Returns NULL if the tag doesn't exist. (Doesn't set e.c, e.m) */
/* Returns NULL on other error, details in e.c, e.m */
/* Returns an icmSigUnknownType object if the tag type isn't handled by a */
/* specific object and icmCFlagAllowUnknown */
/* NOTE: we don't handle tag duplication - you'll always get the first in the file */
static icmBase *icc_read_tag_ix(
	icc *p,
	unsigned int i				/* Index of tag to read */
) {
	icTagTypeSignature ttype;	/* Tag type we will create */
	icTagTypeSignature uttype;	/* Underlying ttype if Unknown */
	icmBase *nob;
	unsigned int j, k;

	p->op = icmSnRead;			/* Let check know direction */

	if (i >= p->count) {
		return NULL;
	}

	/* See if it's already been read */
    if (p->data[i].objp != NULL) {
		return p->data[i].objp;		/* Just return it */
	}
	
	ttype = uttype = p->data[i].ttype;

	/* If unknown types are allowed, convert an unrecognized type */
	/* into icmSigUnknownType */
	// ~8 should we regard tags outside version as unrecognized ?
	if (p->cflags & icmCFlagAllowUnknown) {
		for (j = 0; p->tagtypetable[j].ttype != icMaxEnumType; j++) {
			if (p->tagtypetable[j].ttype == p->data[i].ttype)
				break;
		}

		if (p->tagtypetable[j].ttype == icMaxEnumType)
			ttype = icmSigUnknownType;
	}

	/* See if this is in fact a link */
	/* (data[].ttype is actual type, not icmSigUnknownType) */
	for (k = 0; k < p->count; k++) {
		if (i == k)
			continue;
	    if (p->data[i].ttype  == p->data[k].ttype	/* Exact match and already read */
	     && p->data[i].offset == p->data[k].offset
	     && p->data[i].size   == p->data[k].size
	     && p->data[k].objp != NULL)
			break;
	}
	if (k < p->count) {		/* Mark it as a link */

		/* Check that the tag signature and tag type are compatible */
		if (icc_check_sig(p, NULL, 1, p->data[i].sig, ttype, uttype) != ICM_ERR_OK)
			return NULL;

		/* Check the lut classes (if any) are compatible */
		if (p->get_tag_lut_class(p, p->data[i].sig) != p->get_tag_lut_class(p, p->data[k].sig)) {
			icm_err(p, ICM_ERR_CLASSMISMATCH,
			     "icc_read_tag_ix: Tag '%s' is link to incompatible tag '%s'",
		          icmTagSignature2str(p->data[i].sig), icmTagSignature2str(p->data[k].sig));
			return NULL;
		}

		nob = p->data[k].objp;

		/* Re-check this tags validity with different sig */
		if (nob->check != NULL) {
			if (nob->check(nob, p->data[i].sig) != ICM_ERR_OK) {
				return NULL;
			}
		}

		p->data[i].objp = nob;
		nob->refcount++;			/* Bump reference count */

		return nob;			/* Done */
	}

	/* See if we can handle this type */
	if (icc_check_sig(p, &j, 1, p->data[i].sig, ttype, uttype) != ICM_ERR_OK)
		return NULL;

	/* Create and read in the object */
	if (ttype == icmSigUnknownType)
		nob = new_icmUnknown(p);
	else
		nob = p->tagtypetable[j].new_obj(p);

	if (nob == NULL)
		return NULL;

	if ((nob->read(nob, p->data[i].size, p->of + p->data[i].offset)) != 0) {
		nob->del(nob);		/* Failed, so destroy it */
		return NULL;
	}
	/* Check this tags validity */
	if (nob->check != NULL) {
		if (nob->check(nob, p->data[i].sig) != ICM_ERR_OK) {
			nob->del(nob);	
			return NULL;
		}
	}
    p->data[i].objp = nob;
	return nob;
}

/* Read the tag element data, and return a pointer to the object */
/* Returns NULL if the tag doesn't exist. (Doesn't set e.c, e.m) */
/* Returns NULL on other error, details in e.c, e.m */
/* Returns an icmSigUnknownType object if the tag type isn't handled by a specific object. */ 
/* NOTE: we don't handle tag duplication - you'll always get the first in the file */
static icmBase *icc_read_tag(
	icc *p,
    icTagSignature sig			/* Tag signature */
) {
	unsigned int i;

	/* Search for signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig)		/* Found it */
			break;
	}
	if (i >= p->count) {
		return NULL;
	}
	return icc_read_tag_ix(p, i);
}

// ~~~888 get rid of this and switch client code to using icmCFlagAllowUnknown
/* Read the tag element data of the first matching, and return a pointer to the object */
/* Returns NULL if error. */
/* Returns an icmSigUnknownType object if the tag type isn't handled by a specific object. */
/* NOTE: we don't handle tag duplication - you'll always get the first in the file. */
static icmBase *icc_read_tag_any(
	icc *p,
    icTagSignature sig			/* Tag signature - may be unknown */
) {
	unsigned int i;
	icmBase *ob;
	icmCompatFlags cflags;

	/* Search for signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig)		/* Found it */
			break;
	}
	if (i >= p->count) {
		return NULL;
	}

	cflags = p->cflags;
	p->cflags |= icmCFlagAllowUnknown;
	ob = icc_read_tag_ix(p, i);
	p->cflags = cflags;

	return ob;
}

/* Create and add a tag with the given signature. */
/* Returns a pointer to the element object */
/* Caller should set p->op SnRead/SnWrite for icc_check_sig() */ 
/* Returns NULL if error - icc->e.c will contain */
/* 2 on system error, */
/* 3 if unknown tag */
/* 4 if duplicate tag */
/* NOTE: that we prevent tag duplication. */
/* NOTE: to create a tag type icmSigUnknownType, set ttype to icmSigUnknownType, */
/* and set the actual tag type in icmSigUnknownType->uttype. (Will get warning) */
static icmBase *icc_add_tag_imp(
	icc *p,
    icTagSignature sig,			/* Tag signature - may be unknown */
	icTagTypeSignature ttype	/* Tag type - may be icmSigUnknownType */
) {
	icmTagRec *tp;
	icmBase *nob;
	unsigned int i, j;

	/* Check that the tag signature and tag type are reasonable */
	/* (Handles icmSigUnknownType appropriately) */
	if (icc_check_sig(p, &j, 0, sig, ttype, ttype) != ICM_ERR_OK)
		return NULL;

	/* Check that this tag doesn't already exist. */
	/* (Should we allow this if file version is < 2.2 ?) */ 
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig) {
			icm_err(p, ICM_ERR_DUPLICATE, "icc_add_tag: Already have tag %s in profile",
			                                                          icmtag2str(p->data[i].sig));
			return NULL;
		}
	}

	/* Make space in tag table for new tag item */
	if (p->data == NULL)
		tp = (icmTagRec *)p->al->malloc(p->al, (p->count+1) * sizeof(icmTagRec));
	else
		tp = (icmTagRec *)p->al->realloc(p->al, (void *)p->data, (p->count+1) * sizeof(icmTagRec));
	if (tp == NULL) {
		icm_err(p, ICM_ERR_MALLOC, "icc_add_tag: Tag table realloc() failed");
		return NULL;
	}
	p->data = tp;

	if (ttype == icmSigUnknownType) {
		if ((nob = new_icmUnknown(p)) == NULL)
			return NULL;
	} else {
		/* Allocate the empty object */
		if ((nob = p->tagtypetable[j].new_obj(p)) == NULL)
			return NULL;
	}

	/* Fill out our tag table entry */
    p->data[p->count].sig = sig;		/* The tag signature */
	p->data[p->count].ttype = nob->ttype = ttype;	/* The tag type signature */
    p->data[p->count].offset = 0;		/* Unknown offset yet */
    p->data[p->count].size = 0;			/* Unknown size yet */
    p->data[p->count].objp = nob;		/* Empty object */
    nob->creatorsig = sig;				/* tag that created tagtype */
	p->count++;

	/* Track whether we have a natural 'chad' tag */
	if (sig == icSigChromaticAdaptationTag)
		p->naturalChad = 1;

	return nob;
}

/* Public version */
/* See above for arguments */
static icmBase *icc_add_tag(
	icc *p,
    icTagSignature sig,			/* Tag signature - may be unknown */
	icTagTypeSignature ttype	/* Tag type - may be icmSigUnknownType */
) {
	p->op = icmSnWrite;			/* Let check know assumed direction */

	return icc_add_tag_imp(p, sig, ttype);
}

/* Rename a tag signature */
/* Return error code */
static int icc_rename_tag(
	icc *p,
    icTagSignature sig,			/* Existing Tag signature - may be unknown */
    icTagSignature sigNew		/* New Tag signature - may be unknown */
) {
	unsigned int k;

	p->op = icmSnWrite;			/* Let check know direction */

	/* Search for signature in the tag table */
	for (k = 0; k < p->count; k++) {
		if (p->data[k].sig == sig)		/* Found it */
			break;
	}
	if (k >= p->count) {
		return icm_err(p, ICM_ERR_NOT_FOUND, "icc_rename_tag: Tag '%s' not found",
		                                                         icmTagSignature2str(sig));
	}

	/* Check that the tag signature and tag type are reasonable */
	if (icc_check_sig(p, NULL, 0, sigNew, p->data[k].ttype, p->data[k].ttype) != ICM_ERR_OK)
		return p->e.c;

	/* Check the classes are compatible */
	if (p->get_tag_lut_class(p, sig) != p->get_tag_lut_class(p, sigNew)) {
		return icm_err(p, ICM_ERR_CLASSMISMATCH,
		     "icc_rename_tag: New tag '%s' doesn't have the same class as old tag '%s'",
		                            icmTagSignature2str(sigNew), icmTagSignature2str(sig));
	}

	/* change its signature */
	p->data[k].sig = sigNew;

	/* Track whether we have a natural 'chad' tag */
	if (sig == icSigChromaticAdaptationTag)
		p->naturalChad = 0;
	if (sigNew == icSigChromaticAdaptationTag)
		p->naturalChad = 1;

	return p->e.c;
}

/* Create and add a tag which is a link to an existing (loaded) tag. */
/* Returns a pointer to the element object */
/* Returns NULL if error - icc->e.c will contain */
/* 3 or code if incompatible tag */
/* NOTE: that we prevent tag duplication */
static icmBase *icc_link_tag(
	icc *p,
    icTagSignature sig,			/* Tag signature to create */
    icTagSignature ex_sig		/* Existing tag signature to link to */
) {
	icmTagRec *tp;
	unsigned int i;

	p->op = icmSnWrite;			/* Let check know direction */

	/* Check that the new tag doesn't already exist. */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig) {
			icm_err(p, ICM_ERR_DUPLICATE, "icc_link_tag: Already have tag %s in profile",
			                                                          icmtag2str(p->data[i].sig));
			return NULL;
		}
	}

	/* Search for existing tag signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == ex_sig)		/* Found it */
			break;
	}
	if (i >= p->count) {
		icm_err(p, ICM_ERR_NOT_FOUND, "icc_link_tag: Can't find existing tag '%s'",
		                                                         icmTagSignature2str(ex_sig));
		return NULL;
	}

	/* Check existing tag is loaded. */
    if (p->data[i].objp == NULL) {
		icm_err(p, ICM_ERR_NOT_FOUND, "icc_link_tag: Existing tag '%s' isn't loaded",
		                                                                   icmtag2str(ex_sig)); 
		return NULL;
	}

	/* Check that the new tag signature and linked tag type are reasonable */
	if (icc_check_sig(p, NULL, 0, sig, p->data[i].objp->ttype, p->data[i].ttype) != ICM_ERR_OK)
		return NULL;

	/* Check the LUT classes are compatible */
	if (p->get_tag_lut_class(p, sig) != p->get_tag_lut_class(p, ex_sig)) {
		icm_err(p, ICM_ERR_CLASSMISMATCH,
		     "icc_link_tag: Link tag '%s' doesn't have the same LUT class as tag '%s'",
		                            icmTagSignature2str(sig), icmTagSignature2str(ex_sig));
		return NULL;
	}

	/* Make space in tag table for new tag item */
	if (p->data == NULL)
		tp = (icmTagRec *)p->al->malloc(p->al, (p->count+1) * sizeof(icmTagRec));
	else
		tp = (icmTagRec *)p->al->realloc(p->al, (void *)p->data, (p->count+1) * sizeof(icmTagRec));
	if (tp == NULL) {
		icm_err(p, ICM_ERR_MALLOC, "icc_link_tag: Tag table realloc() failed");
		return NULL;
	}
	p->data = tp;

	/* Fill out our tag table entry */
    p->data[p->count].sig  = sig;		/* The tag signature */
	p->data[p->count].ttype  = p->data[i].ttype;	/* The tag type signature */
    p->data[p->count].offset = p->data[i].offset;	/* Same offset (may not be allocated yet) */
    p->data[p->count].size = p->data[i].size;		/* Same size (may not be allocated yet) */
    p->data[p->count].objp = p->data[i].objp;		/* Shared object */
	p->data[i].objp->refcount++;					/* Bump reference count on tag type */
	p->count++;

	/* Track whether we have a natural 'chad' tag */
	if (sig == icSigChromaticAdaptationTag)
		p->naturalChad = 1;

	return p->data[i].objp;
}

/* Delete the tag, and free the underlying tag type, */
/* if this was the last reference to it. */
/* (This is an internal function) */
/* Returns non-zero on error: */
/* tag not found - icc->e.c will contain 2 */
static int icc_unread_tag_ix(
	icc *p,
	unsigned int i
) {

	if (i >= p->count) {
		return icm_err(p, ICM_ERR_NOT_FOUND, "icc_unread_tag_ix: Index %d is out of range", i);
	}

	/* See if it's been read */
    if (p->data[i].objp == NULL) {
		return icm_err(p, 2,"icc_unread_tag: Tag '%s' not currently loaded",icmTagSignature2str(p->data[i].sig));
	}
	
	if (--(p->data[i].objp->refcount) == 0)				/* decrement reference count */
			(p->data[i].objp->del)(p->data[i].objp);	/* Last reference */
  	p->data[i].objp = NULL;

	return 0;
}

/* Unread a specific tag, and free the underlying tag type data */
/* if this was the last reference to it. */
/* Returns non-zero on error: */
/* tag not found - icc->e.c will contain 2 */
/* tag not read - icc->e.c will contain 2 */
static int icc_unread_tag(
	icc *p,
    icTagSignature sig		/* Tag signature - may be unknown */
) {
	unsigned int i;

	/* Search for signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig)		/* Found it */
			break;
	}
	if (i >= p->count) {
		return icm_err(p, ICM_ERR_NOT_FOUND, "icc_unread_tag: Tag '%s' not found",
		                                                        icmTagSignature2str(sig));
	}

	return icc_unread_tag_ix(p, i);
}

/* Read all the tags into memory. */
/* Returns non-zero on error. */
static int icc_read_all_tags(
	icc *p
) {
	unsigned int i;

	for (i = 0; i < p->count; i++) {	/* For all the tag element data */
		icmBase *ob;
		if ((ob = icc_read_tag_ix(p, i)) == NULL) {
			return p->e.c;
		}
	}
	return p->e.c;
}


/* Delete a specific tag, and free the underlying tag type, */
/* if this was the last reference to it. */
/* (This is an internal function) */
/* Returns error code on error: */
/* tag not found - icc->e.c will contain 2 */
static int icc_delete_tag_imp(
	icc *p,
    icTagSignature sig,		/* Tag signature - may be unknown */
	int quiet				/* If nz don't error if the tag doesn't exist */
) {
	unsigned int i;
	int rv;

	/* Search for signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig)		/* Found it */
			break;
	}

	if (i >= p->count) {
		if (quiet)
			return ICM_ERR_OK;
		return icm_err(p, ICM_ERR_NOT_FOUND, "icc_delete_tag: Tag '%s' not found",
		                                                  icmTagSignature2str(sig));
	}

	/* If the tagtype is loaded, decrement the reference count */
    if (p->data[i].objp != NULL) {
		if (--(p->data[i].objp->refcount) == 0)			/* decrement reference count */
			p->data[i].objp->del(p->data[i].objp);	/* Last reference */
  		p->data[i].objp = NULL;
	}
	
	/* Now remove it from the tag list */
	for (; i < (p->count-1); i++)
		p->data[i] = p->data[i+1];	/* Copy the structure down */

	p->count--;		/* One less tag in list */

	/* Track whether we still have a natural 'chad' tag */
	if (sig == icSigChromaticAdaptationTag)
		p->naturalChad = 0;

	return p->e.c;
}

/* Delete the tag, and free the underlying tag type, */
/* if this was the last reference to it. */
/* Returns error code on error. */
/* NOTE: we don't handle tag duplication - you'll always read delete the first in the file */
static int icc_delete_tag(
	icc *p,
    icTagSignature sig		/* Tag signature - may be unknown */
) {
	return icc_delete_tag_imp(p, sig, 0);
}

/* Delete the tag quietly, and free the underlying tag type, */
/* if this was the last reference to it. */
/* Doesn't return error if tag doesn't exist */
/* Returns error code on error. */
/* NOTE: we don't handle tag duplication - you'll always read delete the first in the file */
static int icc_delete_tag_quiet(
	icc *p,
    icTagSignature sig		/* Tag signature - may be unknown */
) {
	return icc_delete_tag_imp(p, sig, 1);
}

/* Set the given compatibility flag */
static void icc_set_cflag(icc *p, icmCompatFlags flags) {
	p->cflags |= flags;
}

/* Unset the given compatibility flag */
static void icc_unset_cflag(icc  *p, icmCompatFlags flags) {
	p->cflags &= ~flags;
}

/* Return the state of the compatibility flags */
static icmCompatFlags icc_get_cflags(icc *p) {
	return p->cflags;
}

/* Set icmCFlagAllowWrVersion and tag  tagtype range over which it is enabled. */
/* Use ICMTVRANGE_NONE to disable */
static void icc_set_vcrange(icc *p, const icmTVRange *tvr) {
	if (tvr->min == ICMTV_MAX && tvr->max == ICMTV_MIN) {
		p->cflags &= ~icmCFlagAllowWrVersion;
		p->vcrange = *tvr;		/* Struct copy */
	} else {
		p->cflags |= icmCFlagAllowWrVersion;
		p->vcrange = *tvr;		/* Struct copy */
	}
}

static double icm_get_tac(icc *p, double *chmax,
                          void (*calfunc)(void *cntx, double *out, double *in), void *cntx);
static void icc_set_illum(struct _icc *p, double ill_wp[3]);
static void icc_chromAdaptMatrix(icc *p, int flags, double imat[3][3], double mat[3][3],
                                 icmXYZNumber d_wp, icmXYZNumber s_wp);
static icmLuBase* icc_get_luobj(icc *p, icmLookupFunc func, icRenderingIntent intent,
                                icColorSpaceSignature pcsor, icmLookupOrder order);
icmLuBase *icc_new_icmLuLut(icc *icp, icTagSignature ttag, icColorSpaceSignature inSpace,
                            icColorSpaceSignature outSpace, icColorSpaceSignature pcs,
                            icColorSpaceSignature e_inSpace, icColorSpaceSignature e_outSpace,
                            icColorSpaceSignature e_pcs, icRenderingIntent intent,
                            icmLookupFunc func);

/* Create an empty object. Return NULL on error */
icc *new_icc_a(
icmErr *e,				/* Return error (may be NULL) */
icmAlloc *al			/* Memory allocator to take reference of */
) {
	unsigned int i;
	icc *p = NULL;	

	if (e != NULL && e->c != ICM_ERR_OK)
		return NULL;

	if ((p = (icc *) al->calloc(al, 1,sizeof(icc))) == NULL) {
		if (e != NULL)
			icm_err_e(e, ICM_ERR_MALLOC, "Allocating icc failed");
		return NULL;
	}

	p->get_rfp            = icc_get_rfp;
	p->set_version   	  = icc_set_version;
	p->set_cflag          = icc_set_cflag;
	p->unset_cflag        = icc_unset_cflag;
	p->get_cflags         = icc_get_cflags;
	p->set_vcrange        = icc_set_vcrange;
	p->clear_err          = icm_err_clear;
	p->get_size           = icc_get_size;
	p->read               = icc_read;
	p->write              = icc_write;
	p->dump               = icc_dump;
	p->del                = icc_del;
	p->find_tag           = icc_find_tag;
	p->read_tag           = icc_read_tag;
	p->read_tag_any       = icc_read_tag_any;		// ~8 remove this
	p->add_tag            = icc_add_tag;
	p->rename_tag         = icc_rename_tag;
	p->link_tag           = icc_link_tag;
	p->unread_tag         = icc_unread_tag;
	p->read_all_tags      = icc_read_all_tags;
	p->delete_tag         = icc_delete_tag;
	p->delete_tag_quiet   = icc_delete_tag_quiet;
	p->check_id      	  = icc_check_id;
	p->write_check        = icc_write_check;
	p->get_tac            = icm_get_tac;
	p->get_tag_lut_class  = icc_get_tag_lut_class;
	p->set_illum          = icc_set_illum;
	p->chromAdaptMatrix   = icc_chromAdaptMatrix;

	p->get_luobj          = icc_get_luobj;
	p->new_clutluobj      = icc_new_icmLuLut;

	p->al                 = al->reference(al);
	p->tagtypetable  = icmTagTypeTable; 
	p->tagsigtable   = icmTagSigTable; 
	p->classtagtable = icmClassTagTable; 
	p->transtagtable = icmTransTagTable; 

	/* By default be leanient in what we read, and strict in what we write. */
	p->cflags |= icmCFlagRdFormatWarn;
	p->cflags |= icmCFlagRdVersionWarn;
	p->cflags |= icmCFlagAllowUnknown;
	p->cflags |= icmCFlagAllowExtensions;
	p->cflags |= icmCFlagAllowQuirks;
	p->vcrange = icmtvrange_none;

	p->align = icAlignSize;		/* Everything is 32 bit aligned */

	/* Allocate a header object and set default values */
	if ((p->header = new_icmHeader(p)) == NULL) {
		if (e != NULL)
			*e = p->e;		/* Copy error out */
		p->del(p);
		return NULL;
	}

	/* Should we use ICC standard Wrong Von Kries for */
	/* white point chromatic adapation for output class ? */
	if (getenv("ARGYLL_CREATE_WRONG_VON_KRIES_OUTPUT_CLASS_REL_WP") != NULL)
		p->useLinWpchtmx = 1;				/* Use Wrong Von Kries */
	else
		p->useLinWpchtmx = 0;				/* Use Bradford by default */
	p->wpchtmx_class = icMaxEnumClass;		/* Not set yet - auto set on create. */

	/* Default to saving ArgyllCMS private 'arts' tag (if appropriate type of */
	/* profile) to make white point chromatic adapation explicit. */
	p->useArts = 1;

	/* Should we create a V4 style Display profile with D50 media white point */
	/* tag and 'chad' tag ? - or - */
	/* Should we create an Output profile using a 'chad' tag if it uses */
	/* a non-standard illuminant ? */
	if (getenv("ARGYLL_CREATE_DISPLAY_PROFILE_WITH_CHAD") != NULL)
		p->wrDChad = 1;		/* For Display profile mark media WP as D50 and put */
							/* absolute to relative transform matrix in 'chad' tag. */
	else
		p->wrDChad = 0;		/* No by default - use Bradford and store real Media WP */

	/* Should we create an Output profile using a 'chad' tag if it uses */
	/* a non-standard illuminant ? */
	if (getenv("ARGYLL_CREATE_OUTPUT_PROFILE_WITH_CHAD") != NULL)
		p->wrOChad = 1;		/* For Output profile, put illuminant to D50 Bradford */
							/* matrix in 'chad' tag, and transform real WP by it. */
	else
		p->wrOChad = 0;		/* No by default - Media WP inclues effect of illuminant. */

	/* Set a default wpchtmx in case the profile being read or written */
	/* doesn't use a white point (i.e., it's a device link) */
	/* This will be reset if the wpchtmx_class gets changed. */
	if (!p->useLinWpchtmx) {
		icmCpy3x3(p->wpchtmx, icmBradford);
		icmInverse3x3(p->iwpchtmx, p->wpchtmx);
	} else {
		icmCpy3x3(p->wpchtmx, icmWrongVonKries);
		icmCpy3x3(p->iwpchtmx, icmWrongVonKries);
	}

	/* Return any error */
	if (p->e.c != ICM_ERR_OK) {
		*e = p->e;		/* Copy error out */
		p->del(p);
		return NULL;
	}

	return p;
}

/* ================================================== */
/* Lut Color normalizing and de-normalizing functions */

/* As a rule, I am representing Lut in memory as values in machine form as real */
/* numbers in the range 0.0 - 1.0. For many color spaces (ie. RGB, Gray, */
/* hsv, hls, cmyk and other device coords), this is entirely appropriate. */
/* For CIE based spaces though, this is not correct, since (I assume!) the binary */
/* representation will be consistent with the encoding in Annex A, page 74 */
/* of the standard. Note that the standard doesn't specify the encoding of */
/* many color spaces (ie. Yuv, Yxy etc.), and is unclear about PCS. */

/* The following functions convert to and from the CIE base spaces */
/* and the real Lut input/output values. These are used to convert real color */
/* space values into/out of the raw lut 0.0-1.0 representation (which subsequently */
/* get converted to ICC integer values in the obvious way as a mapping to 0 .. 2^n-1). */

/* This is used internally to support the Lut->lookup() function, */
/* and can also be used by someone writing a Lut based profile to determine */
/* the colorspace range that the input lut indexes cover, as well */
/* as processing the output luts values into normalized form ready */
/* for writing. */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* NEW:						                                 */

/* These routines convert real ICC colorspace values to/from */
/* a normalised range of 0.0-1.0. That can subsequently be */
/* converted to/from ICC integer encodings in the obvious way */
/* as a mapping to 0 .. 2^n-1). */

/* Convert Normalised value to XYZ */ 
static void icmNormToXYZ(double *out, double *in) {
	out[0] = in[0] * (1.0 + 32767.0/32768); /* X */
	out[1] = in[1] * (1.0 + 32767.0/32768); /* Y */
	out[2] = in[2] * (1.0 + 32767.0/32768); /* Z */
}

static void icmNormToXYZn(double *out, double *in, unsigned int ch) {
	out[0] = in[0] * (1.0 + 32767.0/32768); /* X */
}

/* Convert XYZ to Normalised */ 
static void icmXYZToNorm(double *out, double *in) {
	out[0] = in[0] * (1.0/(1.0 + 32767.0/32768));
	out[1] = in[1] * (1.0/(1.0 + 32767.0/32768));
	out[2] = in[2] * (1.0/(1.0 + 32767.0/32768));
}

static void icmXYZToNormn(double *out, double *in, unsigned int ch) {
	out[0] = in[0] * (1.0/(1.0 + 32767.0/32768));
}

/* Convert Normalised to Y */ 
static void icmNormToY(double *out, double *in) {
	out[0] = in[0] * (1.0 + 32767.0/32768); /* Y */
}

/* Convert Y Normalised */ 
static void icmYToNorm(double *out, double *in) {
	out[0] = in[0] * (1.0/(1.0 + 32767.0/32768));
}


/* Convert Normalised to 8 bit Lab */
static void icmNormToLab8(double *out, double *in) {
	out[0] = in[0] * 100.0;				/* L */
	out[1] = (in[1] * 255.0) - 128.0;	/* a */
	out[2] = (in[2] * 255.0) - 128.0;	/* b */
}

static void icmNormToLab8n(double *out, double *in, unsigned int ch) {
	if (ch == 0)
		out[0] = in[0] * 100.0;				/* L */
	else 
		out[0] = (in[0] * 255.0) - 128.0;	/* a, b */
}

/* Convert 8 bit Lab to Normalised */
static void icmLab8ToNorm(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;				/* L */
	out[1] = (in[1] + 128.0) * 1.0/255.0;	/* a */
	out[2] = (in[2] + 128.0) * 1.0/255.0;	/* b */
}

static void icmLab8ToNormn(double *out, double *in, unsigned int ch) {
	if (ch == 0)
		out[0] = in[0] * 1.0/100.0;				/* L */
	else
		out[0] = (in[0] + 128.0) * 1.0/255.0;	/* a, b */
}

/* Convert Normalised to 8 bit L */
static void icmNormToL8(double *out, double *in) {
	out[0] = in[0] * 100.0;					/* L */
}

/* Convert 8 bit L to Normalised */
static void icmL8ToNorm(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;				/* L */
}


/* Convert Normalised to 16 bit V2 Lab */
static void icmNormToLab16v2(double *out, double *in) {
	out[0] = in[0] * (100.0 * 65535.0)/65280.0;			/* L */
	out[1] = (in[1] * (255.0 * 65535.0)/65280) - 128.0;	/* a */
	out[2] = (in[2] * (255.0 * 65535.0)/65280) - 128.0;	/* b */
}

static void icmNormToLab16v2n(double *out, double *in, unsigned int ch) {
	if (ch == 0)
		out[0] = in[0] * (100.0 * 65535.0)/65280.0;			/* L */
	else
		out[0] = (in[0] * (255.0 * 65535.0)/65280) - 128.0;	/* a, b */
}

/* Convert 16 bit Lab V2 to Normalised */
static void icmLab16v2ToNorm(double *out, double *in) {
	out[0] = in[0] * 65280.0/(100.0 * 65535.0);				/* L */
	out[1] = (in[1] + 128.0) * 65280.0/(255.0 * 65535.0);	/* a */
	out[2] = (in[2] + 128.0) * 65280.0/(255.0 * 65535.0);	/* b */
}

static void icmLab16v2ToNormn(double *out, double *in, unsigned int ch) {
	if (ch == 0)
		out[0] = in[0] * 65280.0/(100.0 * 65535.0);				/* L */
	else
		out[0] = (in[0] + 128.0) * 65280.0/(255.0 * 65535.0);	/* a, b */
}

/* Convert Normalised to 16 bit V2 L */
static void icmNormToL16v2(double *out, double *in) {
	out[0] = in[0] * (100.0 * 65535.0)/65280.0;				/* L */
}

/* Convert 16 bit L V2 to Normalised */
static void icmL16v2ToNorm(double *out, double *in) {
	out[0] = in[0] * 65280.0/(100.0 * 65535.0);				/* L */
}


/* Convert Normalised to 16 bit V4 Lab */
static void icmNormToLab16v4(double *out, double *in) {
	out[0] = in[0] * 100.0;				/* L */
	out[1] = (in[1] * 255.0) - 128.0;	/* a */
	out[2] = (in[2] * 255.0) - 128.0;	/* b */
}

static void icmNormToLab16v4n(double *out, double *in, unsigned int ch) {
	if (ch == 0)
		out[0] = in[0] * 100.0;				/* L */
	else
		out[0] = (in[0] * 255.0) - 128.0;	/* a, b */
}

/* Convert 16 bit Lab V4 to Normalised */
static void icmLab16v4ToNorm(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;				/* L */
	out[1] = (in[1] + 128.0) * 1.0/255.0;	/* a */
	out[2] = (in[2] + 128.0) * 1.0/255.0;	/* b */
}

static void icmLab16v4ToNormn(double *out, double *in, unsigned int ch) {
	if (ch == 0)
		out[0] = in[0] * 1.0/100.0;				/* L */
	else
		out[0] = (in[0] + 128.0) * 1.0/255.0;	/* a, b */
}

/* Convert Normalised to 16 bit V4 L */
static void icmNormToL16v4(double *out, double *in) {
	out[0] = in[0] * 100.0;				/* L */
}

/* Convert 16 bit L V4 to Normalised */
static void icmL16v4ToNorm(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;				/* L */
}

/* (Luv encoding is taken Apples Colorsync specification, */
/*  since it is not specified by the ICC) */

/* Convert Normalised to Luv */
static void icmNormToLuv(double *out, double *in) {
	out[0] = in[0] * 100.0;						/* L */
	out[1] = (in[1] * 65535.0/256.0) - 128.0;	/* u */
	out[2] = (in[2] * 65535.0/256.0) - 128.0;	/* v */
}

static void icmNormToLuvn(double *out, double *in, unsigned int ch) {
	if (ch == 0)
		out[0] = in[0] * 100.0;						/* L */
	else
		out[0] = (in[0] * 65535.0/256.0) - 128.0;	/* u, v */
}

/* Convert Luv to Normalised */
static void icmLuvToNorm(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;					/* L */
	out[1] = (in[1] + 128.0) * 256.0/65535.0;	/* u */
	out[2] = (in[2] + 128.0) * 256.0/65535.0;	/* v */
}

static void icmLuvToNormn(double *out, double *in, unsigned int ch) {
	if (ch == 0)
		out[0] = in[0] * 1.0/100.0;					/* L */
	else
		out[0] = (in[0] + 128.0) * 256.0/65535.0;	/* u, v */
}

/* Default N component conversions */
static void icmNormTFN(double *out, double *in, int nc) {
	for (--nc; nc >= 0; nc--)
		out[nc] = in[nc];
}

static void icmNormTFn(double *out, double *in, unsigned int ch) {
	out[0] = in[0];
}

/* 1 */
static void icmNormTF1(double *out, double *in) {
	out[0] = in[0];
}

/* 2 */
static void icmNormTF2(double *out, double *in) {
	out[0] = in[0];
	out[1] = in[1];
}

/* 3 */
static void icmNormTF3(double *out, double *in) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

/* 4 */
static void icmNormTF4(double *out, double *in) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
}

/* 5 */
static void icmNormTF5(double *out, double *in) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
	out[4] = in[4];
}

/* 6 */
static void icmNormTF6(double *out, double *in) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
	out[4] = in[4];
	out[5] = in[5];
}

/* 7 */
static void icmNormTF7(double *out, double *in) {
	icmNormTFN(out, in, 7);
}

/* 8 */
static void icmNormTF8(double *out, double *in) {
	icmNormTFN(out, in, 8);
}

/* 9 */
static void icmNormTF9(double *out, double *in) {
	icmNormTFN(out, in, 9);
}

/* 10 */
static void icmNormTF10(double *out, double *in) {
	icmNormTFN(out, in, 10);
}

/* 11 */
static void icmNormTF11(double *out, double *in) {
	icmNormTFN(out, in, 11);
}

/* 12 */
static void icmNormTF12(double *out, double *in) {
	icmNormTFN(out, in, 12);
}

/* 13 */
static void icmNormTF13(double *out, double *in) {
	icmNormTFN(out, in, 13);
}

/* 14 */
static void icmNormTF14(double *out, double *in) {
	icmNormTFN(out, in, 14);
}

/* 15 */
static void icmNormTF15(double *out, double *in) {
	icmNormTFN(out, in, 15);
}

/* Find appropriate conversion functions from the normalised */
/* Lut data range 0.0 - 1.0 to/from a given colorspace value, */
/* given the color space and Lut type. */
/* If the signature specifies a specific Lab encoding, dir, ver & rep are ignored */
/* Function pointer addresses can be NULL */
/* Return 0 on success, 1 on match failure. */
int icmGetNormFunc(
	void (**nfunc)(double *out, double *in),	/* return function pointers */
	void (**nfuncn)(double *out, double *in, unsigned int ch),
	icColorSpaceSignature csig, 		/* Colorspace to convert to/from */
	icmGNDir              dir,			/* Direction: 0 = Norm -> CS, 1 = CS -> Norm */
	icmGNVers             ver,			/* Lab version: 0 = V2, 1 = V4 */
	icmGNRep              rep			/* Representation: 0 = 8 bit, 1 = 16 bit */
) {
	void (*null_nfunc)(double *out, double *in);					/* Dummy pointers */
	void (*null_nfuncn)(double *out, double *in, unsigned int ch);

	/* Allow for NULL pointers to function pointers */
	if (nfunc == NULL)
		nfunc = &null_nfunc;
	if (nfuncn == NULL)
		nfuncn = &null_nfuncn;

	switch(csig) {
		case icmSigYData:
			if(dir == icmGNtoCS) {
				*nfunc = icmNormToY;
				*nfuncn = icmNormToXYZn;
			} else {
				*nfunc = icmYToNorm;
				*nfuncn = icmXYZToNormn;
			}
			return 0;

		case icSigXYZData:
			if(dir == icmGNtoCS) {
				*nfunc = icmNormToXYZ;
				*nfuncn = icmNormToXYZn;
			} else {
				*nfunc = icmXYZToNorm;
				*nfuncn = icmXYZToNormn;
			}
			return 0;

		case icSigLabData:
			if(dir == icmGNtoCS) {
				if (rep == icmGN8bit) {
					*nfunc = icmNormToLab8;
					*nfuncn = icmNormToLab8n;
				} else {
					if (ver == icmGNV2) {
						*nfunc = icmNormToLab16v2;
						*nfuncn = icmNormToLab16v2n;
					} else {
						*nfunc = icmNormToLab16v4;
						*nfuncn = icmNormToLab16v4n;
					}
				}
			} else {
				if (rep == icmGN8bit) {
					*nfunc = icmLab8ToNorm;
					*nfuncn = icmLab8ToNormn;
				} else {
					if (ver == icmGNV2) {
						*nfunc = icmLab16v2ToNorm;
						*nfuncn = icmLab16v2ToNormn;
					} else {
						*nfunc = icmLab16v4ToNorm;
						*nfuncn = icmLab16v4ToNormn;
					}
				}
			}
			return 0;

		case icmSigLab8Data:
			if(dir == icmGNtoCS) {
				*nfunc = icmNormToLab8;
				*nfuncn = icmNormToLab8n;
			} else {
				*nfunc = icmLab8ToNorm;
				*nfuncn = icmLab8ToNormn;
			}
			return 0;

		case icmSigLabV2Data:
			if(dir == icmGNtoCS) {
				*nfunc = icmNormToLab16v2;
				*nfuncn = icmNormToLab16v2n;
			} else {
				*nfunc = icmLab16v2ToNorm;
				*nfuncn = icmLab16v2ToNormn;
			}
			return 0;

		case icmSigLabV4Data:
			if(dir == icmGNtoCS) {
				*nfunc = icmNormToLab16v4;
				*nfuncn = icmNormToLab16v4n;
			} else {
				*nfunc = icmLab16v4ToNorm;
				*nfuncn = icmLab16v4ToNormn;
			}
			return 0;

		case icmSigLData:
			if(dir == icmGNtoCS) {
				if (rep == icmGN8bit) {
					*nfunc = icmNormToL8;
					*nfuncn = icmNormToLab8n;
				} else {
					if (ver == icmGNV2) {
						*nfunc = icmNormToL16v2;
						*nfuncn = icmNormToLab16v2n;
					} else {
						*nfunc = icmNormToL16v4;
						*nfuncn = icmNormToLab16v4n;
					}
				}
			} else {
				if (rep == icmGN8bit) {
					*nfunc = icmL8ToNorm;
					*nfuncn = icmLab8ToNormn;
				} else {
					if (ver == icmGNV2) {
						*nfunc = icmL16v2ToNorm;
						*nfuncn = icmLab16v2ToNormn;
					} else {
						*nfunc = icmL16v4ToNorm;
						*nfuncn = icmLab16v4ToNormn;
					}
				}
			}
			return 0;

		case icmSigL8Data:
			if(dir == icmGNtoCS) {
				*nfunc = icmNormToL8;
				*nfuncn = icmNormToLab8n;
			} else {
				*nfunc = icmL8ToNorm;
				*nfuncn = icmLab8ToNormn;
			}
			return 0;

		case icmSigLV2Data:
			if(dir == icmGNtoCS) {
				*nfunc = icmNormToL16v2;
				*nfuncn = icmNormToLab16v2n;
			} else {
				*nfunc = icmL16v2ToNorm;
				*nfuncn = icmLab16v2ToNormn;
			}
			return 0;

		case icmSigLV4Data:
			if(dir == icmGNtoCS) {
				*nfunc = icmNormToL16v4;
				*nfuncn = icmNormToLab16v4n;
			} else {
				*nfunc = icmL16v4ToNorm;
				*nfuncn = icmLab16v4ToNormn;
			}
			return 0;

		case icSigLuvData:
			if(dir == icmGNtoCS) {
				*nfunc = icmNormToLuv;
				*nfuncn = icmNormToLuvn;
			} else {
				*nfunc = icmLuvToNorm;
				*nfuncn = icmLuvToNormn;
			}
			return 0;

		case icSigGrayData:
		case icSig1colorData:
		case icSigMch1Data:
			*nfunc = icmNormTF1;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig2colorData:
		case icSigMch2Data:
			*nfunc = icmNormTF2;
			*nfuncn = icmNormTFn;
			return 0;

		case icSigRgbData:
		case icSigCmyData:
		case icSig3colorData:
		case icSigMch3Data:
			*nfunc = icmNormTF3;
			*nfuncn = icmNormTFn;
			return 0;

		case icSigCmykData:
		case icSig4colorData:
		case icSigMch4Data:
			*nfunc = icmNormTF4;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig5colorData:
		case icSigMch5Data:
			*nfunc = icmNormTF5;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig6colorData:
		case icSigMch6Data:
			*nfunc = icmNormTF6;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig7colorData:
		case icSigMch7Data:
			*nfunc = icmNormTF7;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig8colorData:
		case icSigMch8Data:
			*nfunc = icmNormTF8;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig9colorData:
		case icSigMch9Data:
			*nfunc = icmNormTF9;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig10colorData:
		case icSigMchAData:
			*nfunc = icmNormTF10;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig11colorData:
		case icSigMchBData:
			*nfunc = icmNormTF11;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig12colorData:
		case icSigMchCData:
			*nfunc = icmNormTF12;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig13colorData:
		case icSigMchDData:
			*nfunc = icmNormTF13;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig14colorData:
		case icSigMchEData:
			*nfunc = icmNormTF14;
			*nfuncn = icmNormTFn;
			return 0;

		case icSig15colorData:
		case icSigMchFData:
			*nfunc = icmNormTF15;
			*nfuncn = icmNormTFn;
			return 0;

		/* We don't know how to handle these, so we punt: */
		case icSigHsvData:
		case icSigHlsData:
		case icSigYCbCrData:
		case icSigYxyData:
			*nfunc = icmNormTF3;
			*nfuncn = icmNormTFn;
			return 0;

		default:
			*nfunc = NULL;
			*nfuncn = NULL;
			return 1;
	}

	return 1;
}


/*
	The V4 specificatins are misleading on device space encoding,
	since they assume that all devices spaces however labeled,
	have no defined ICC encoding. The end result is simple enough though:

	V2 Lab encoding:

	ICC V2 Lab encoding should be used in all PCS encodings in
	the following tags:

		(icSigLut8Type encoding hasn't changed for V4)
		icSigLut16Type
		icSigNamedColorType
		icSigNamedColor2Type

	and can/should be used for _device_ space Lab encoding for these tags.

	ICC V4 Lab encoding should be used for all PCS encodings in
	all other situations, and can/should be used for device space Lab encoding
	for all other situations.

	V4 Tags that seem/need to use the default ICC V4 Lab encoding:

		icSigColorantTableType
		icSigLutAToBType	
		icSigLutBToAType

	If we were to get an V4 tag in a V2 profile then the handling
	is undefined, but a good default would be to use V4 encoding.

	[ Since the ICC spec. doesn't cover device spaces labeled as Lab,
      these are ripe for mis-matches between different implementations.]

	This logic has yet to be fully implemented here.
*/

/* Lookup colorspace icmGNRep from TagType */
icmGNRep icmTagType2GNRep(icColorSpaceSignature csig, icTagTypeSignature tagSig) {

	if (tagSig == icSigLut8Type && csig == icSigLabData) {
		return icmGN8bit;
	} else {
		return icmGN16bit;
	}
}

/* Lookup colorspace Lab encoding type from TagType */
icmGNVers icm(icc *icp, icColorSpaceSignature csig, icTagTypeSignature tagSig) {
	icmGNVers ver;

	/* Default */
	if (icp->header->vers.majv >= 4)
		ver = icmGNV4;
	else
		ver = icmGNV2;

	/* Override for specific TagTypes */
	if (csig == icSigLabData) {
		if (tagSig == icSigLut16Type			/* Lut16 retains legacy encoding */
		 || tagSig == icSigNamedColorType		/* NamedColor retains legacy encoding */
		 || tagSig == icSigNamedColor2Type)		/* NamedColor retains legacy encoding */
			ver = icmGNV2;

		/* Should add other V4 tagtypes here ? */
		else if (tagSig == icSigLutAToBType	
		      || tagSig == icSigLutBToAType	
		      || tagSig == icSigColorantTableType) {
			ver = icmGNV4;
		}
	}

	return ver;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Colorspace ranges - used instead of norm/denorm by Mono, Matrix and */
/* override PCS */

/* Function table - match ranges to color spaces. */
/* Anything not here, we don't know how to convert. */
/* Hmm. we're not handling Lab8 properly ?? ~~~8888 */
static struct {
	icColorSpaceSignature csig;
	int same;				/* Non zero if first entry applies to all channels */
	double min[3];			/* Minimum value for this colorspace */
	double max[3];			/* Maximum value for this colorspace */
} colorrangetable[] = {
	{icSigXYZData,     1, { 0.0 } , { 1.0 + 32767.0/32768.0 } },
	{icmSigLab8Data,   0, { 0.0, -128.0, -128.0 }, { 100.0, 127.0,  127.0 } }, 
	{icmSigLabV2Data,  0, { 0.0, -128.0, -128.0 },
	                      { 100.0 + 25500.0/65280.0, 127.0 + 255.0/256.0, 127.0 + 255.0/256.0 } }, 
	{icmSigLabV4Data,  0, { 0.0, -128.0, -128.0 }, { 100.0, 127.0,  127.0 } }, 
	{icmSigYData,      1, { 0.0 }, { 1.0 + 32767.0/32768.0 } },
	{icmSigL8Data,     1, { 0.0 }, { 100.0 } }, 
	{icmSigLV2Data,    1, { 0.0 }, { 100.0 + 25500.0/65280.0 } }, 
	{icmSigLV4Data,    1, { 0.0 }, { 100.0 } }, 
	{icSigLuvData,     0, { 0.0, -128.0, -128.0 },
	                      { 100.0, 127.0 + 255.0/256.0, 127.0 + 255.0/256.0 } },
	{icSigYCbCrData,   0, { 0.0, -0.5, -0.5 }, { 1.0, 0.5, 0.5 } },		/* Full range */
	{icSigYxyData,     1, { 0.0 }, { 1.0 } },			/* ??? */
	{icSigRgbData,     1, { 0.0 }, { 1.0 } },
	{icSigGrayData,    1, { 0.0 }, { 1.0 } },
	{icSigHsvData,     1, { 0.0 }, { 1.0 } },
	{icSigHlsData,     1, { 0.0 }, { 1.0 } },
	{icSigCmykData,    1, { 0.0 }, { 1.0 } },
	{icSigCmyData,     1, { 0.0 }, { 1.0 } },
	{icSigMch6Data,    1, { 0.0 }, { 1.0 } },
	{icSig2colorData,  1, { 0.0 }, { 1.0 } },
	{icSig3colorData,  1, { 0.0 }, { 1.0 } },
	{icSig4colorData,  1, { 0.0 }, { 1.0 } },
	{icSig5colorData,  1, { 0.0 }, { 1.0 } },
	{icSig6colorData,  1, { 0.0 }, { 1.0 } },
	{icSig7colorData,  1, { 0.0 }, { 1.0 } },
	{icSig8colorData,  1, { 0.0 }, { 1.0 } },
	{icSigMch5Data,    1, { 0.0 }, { 1.0 } },
	{icSigMch6Data,    1, { 0.0 }, { 1.0 } },
	{icSigMch7Data,    1, { 0.0 }, { 1.0 } },
	{icSigMch8Data,    1, { 0.0 }, { 1.0 } },
	{icSig9colorData,  1, { 0.0 }, { 1.0 } },
	{icSig10colorData, 1, { 0.0 }, { 1.0 } },
	{icSig11colorData, 1, { 0.0 }, { 1.0 } },
	{icSig12colorData, 1, { 0.0 }, { 1.0 } },
	{icSig13colorData, 1, { 0.0 }, { 1.0 } },
	{icSig14colorData, 1, { 0.0 }, { 1.0 } },
	{icSig15colorData, 1, { 0.0 }, { 1.0 } },
	{icMaxEnumData,    1, { 0.0 }, { 1.0 } }
};
	
/* Find appropriate typical encoding ranges for a */
/* colorspace given the color space. */
/* Return 0 on success, 1 on match failure */
static int getRange(
	icc *icp,
//	icProfileClassSignature psig,		/* Profile signature to use */
	icColorSpaceSignature   csig, 		/* Colorspace to use. */
//  int                     lutin,		/* 0 if this is for a icSigLut16Type input, nz for output */
//	icTagSignature          tagSig,		/* Tag signature involved (AtoB or B2A etc.) */
	icTagTypeSignature      tagType,	/* icSigLut8Type or icSigLut16Type or V4 lut */
	double *min, double *max			/* Return range values */
) {
	int i, e, ee;

	if (tagType == icSigLut8Type && csig == icSigLabData) {
		csig = icmSigLab8Data;
	}
	if (csig == icSigLabData) {
		if (tagType == icSigLut16Type)	/* Lut16 retains legacy encoding */
			csig = icmSigLabV2Data;
		else {							/* Other tag types use version specific encoding */
			if (icp->header->vers.majv >= 4)
				csig = icmSigLabV4Data;
			else
				csig = icmSigLabV2Data;
		}
	}

	for (i = 0; colorrangetable[i].csig != icMaxEnumData; i++) {
		if (colorrangetable[i].csig == csig)
			break;	/* Found it */
	}
	if (colorrangetable[i].csig == icMaxEnumData) {	/* Oops */
		return 1;
	}
	ee = icmCSSig2nchan(csig);		/* Get number of components */

	if (colorrangetable[i].same) {		/* All channels are the same */
		for (e = 0; e < ee; e++) {
			if (min != NULL)
				min[e] = colorrangetable[i].min[0];
			if (max != NULL)
				max[e] = colorrangetable[i].max[0];
		}
	} else {
		for (e = 0; e < ee; e++) {
			if (min != NULL)
				min[e] = colorrangetable[i].min[e];
			if (max != NULL)
				max[e] = colorrangetable[i].max[e];
		}
	}
	return 0;
}

/* Setup the wpchtmx appropriately for creating profile. */
/* This is called if the deviceClass has changed on a call */
/* to ->chromAdaptMatrix(), ->get_size() or ->write(). */
static void icc_setup_wpchtmx(icc *p) {
	int useBradford = 1;		/* Default use Bradford */

	/* If set by reading profile or already set appropriately */
	if (p->wpchtmx_class == p->header->deviceClass)
		return;

	/* If we should use ICC standard Wrong Von Kries for white point chromatic adapation */
	if (p->header->deviceClass == icSigOutputClass
	 && p->useLinWpchtmx) {
		useBradford = 0;
	} 

	if (useBradford) {
		icmCpy3x3(p->wpchtmx, icmBradford);
		icmInverse3x3(p->iwpchtmx, p->wpchtmx);
	} else {
		icmCpy3x3(p->wpchtmx, icmWrongVonKries);
		icmCpy3x3(p->iwpchtmx, icmWrongVonKries);
	}

	/* This is set for this profile class now */
	p->wpchtmx_class = p->header->deviceClass;
}

/* Clear any existing 'chad' matrix, and if Output type profile */
/* and ARGYLL_CREATE_OUTPUT_PROFILE_WITH_CHAD set and */
/* ill_wp != NULL, create a 'chad' matrix. */
static void icc_set_illum(struct _icc *p, double ill_wp[3]) {

	p->chadmxValid = 0;		/* Calling set_illum signals profile creation, */
							/* so discard any previous (i.e. read) chad matrix */

	if (ill_wp != NULL) {
		icmCpy3(p->illwp, ill_wp);
		p->illwpValid = 1;
	}

	/* Is illuminant chromatic adapation chad matrix needed ? */
 	if (p->header->deviceClass == icSigOutputClass
	 && p->illwpValid
	 && p->wrOChad) {
		double wp[3];
		icmXYZNumber iwp;

		/* Create Output illuminant 'chad' matrix */
		icmAry2XYZ(iwp, p->illwp);
		icmChromAdaptMatrix(ICM_CAM_BRADFORD, icmD50, iwp, p->chadmx);  

		/* Optimally quantize chad matrix to preserver white point */
		icmQuantize3x3S15Fixed16(icmD50_ary3, p->chadmx, p->illwp);

		p->chadmxValid = 1;
	}
}

/* Return an overall Chromatic Adaptation Matrix for the given source and */
/* destination white points. This will depend on the icc profiles current setup */
/* for Abs->Rel conversion (wpchtmx[][] set to wrong Von Kries or not, whether */
/* 'arts' tag has been read), and whether an Output profile 'chad' tag has bean read */
/* or will be created. (i.e. on creation, assumes icc->set_illum() called). */
/* Use icmMulBy3x3(dst, mat, src) */
/* NOTE that to transform primaries they must be mat[XYZ][RGB] format! */
static void icc_chromAdaptMatrix(
	icc *p,
	int flags,				/* ICM_CAM_NONE or ICM_CAM_MULMATRIX to mult by mat */
	double imat[3][3],		/* Optional inverse CAT matrix result */
	double mat[3][3],		/* CAT optional input if ICM_CAM_MULMATRIX & result matrix */
	icmXYZNumber d_wp,		/* Destination white point (Usually PCS D50) */
	icmXYZNumber s_wp		/* Source media absolute white point */
) {
	double dst[3], src[3];			/* Source & destination white points */
	double vkmat[3][3];				/* Von Kries matrix */
	double omat[3][3];				/* Output matrix */

	if (p->header->deviceClass == icMaxEnumClass) {
		fprintf(stderr,"icc_chromAdaptMatrix called with no deviceClass!\n");
	}

	/* Take a copy of src/dst */
	icmXYZ2Ary(src, s_wp);
	icmXYZ2Ary(dst, d_wp);

	/* See if the profile type has changed, re-evaluate wpchtmx */
	if (p->wpchtmx_class != p->header->deviceClass) {
		icc_setup_wpchtmx(p);
	}

	/* Set initial matrix to unity if creating from scratch */
	if (flags & ICM_CAM_MULMATRIX)
		icmCpy3x3(omat, mat);
	else
		icmSetUnity3x3(omat);

	/* Incorporate Output chad matrix if we will be creating one */
	if (p->header->deviceClass == icSigOutputClass
	 && p->chadmxValid) {
		icmMulBy3x3(src, p->chadmx, src);
		icmMul3x3(omat, p->chadmx);
	}

	/* Transform src/dst to cone space */
	icmMulBy3x3(src, p->wpchtmx, src);
	icmMulBy3x3(dst, p->wpchtmx, dst);

	/* Transform incoming matrix to cone space */
	icmMul3x3(omat, p->wpchtmx);

	/* Setup the Von Kries white point adaptation matrix */
	vkmat[0][0] = dst[0]/src[0];
	vkmat[1][1] = dst[1]/src[1];
	vkmat[2][2] = dst[2]/src[2];
	vkmat[0][1] = vkmat[0][2] = 0.0;
	vkmat[1][0] = vkmat[1][2] = 0.0;
	vkmat[2][0] = vkmat[2][1] = 0.0;

	/* Apply chromatic adaptation */
	icmMul3x3(omat, vkmat);

	/* Transform from con space */
	icmMul3x3(omat, p->iwpchtmx);

	if (mat != NULL)
		icmCpy3x3(mat, omat);

	if (imat != NULL)
		icmInverse3x3(imat, omat);
}

/* ============================================== */
/* MD5 checksum support */

/* Object for computing RFC 1321 MD5 checksums. */
/* Derived from Colin Plumb's 1993 public domain code. */

/* Reset the checksum */
static void icmMD5_reset(icmMD5 *p) {
	p->tlen = 0;

	p->sum[0] = 0x67452301;
	p->sum[1] = 0xefcdab89;
	p->sum[2] = 0x98badcfe;
	p->sum[3] = 0x10325476;

	p->fin = 0;
}

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

#define MD5STEP(f, w, x, y, z, pp, xtra, s) \
		data = (pp)[0] + ((pp)[3] << 24) + ((pp)[2] << 16) + ((pp)[1] << 8); \
		w += f(x, y, z) + data + xtra; \
		w = (w << s) | (w >> (32-s)); \
		w += x;

/* Add another 64 bytes to the checksum */
static void icmMD5_accume(icmMD5 *p, ORD8 *in) {
	ORD32 data, a, b, c, d;

	a = p->sum[0];
	b = p->sum[1];
	c = p->sum[2];
	d = p->sum[3];

	MD5STEP(F1, a, b, c, d, in + (4 * 0),  0xd76aa478, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 1),  0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 2),  0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 3),  0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in + (4 * 4),  0xf57c0faf, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 5),  0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 6),  0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 7),  0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in + (4 * 8),  0x698098d8, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 9),  0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 10), 0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 11), 0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in + (4 * 12), 0x6b901122, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 13), 0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 14), 0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 15), 0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in + (4 * 1),  0xf61e2562, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 6),  0xc040b340, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 11), 0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 0),  0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in + (4 * 5),  0xd62f105d, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 10), 0x02441453, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 15), 0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 4),  0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in + (4 * 9),  0x21e1cde6, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 14), 0xc33707d6, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 3),  0xf4d50d87, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 8),  0x455a14ed, 20);
	MD5STEP(F2, a, b, c, d, in + (4 * 13), 0xa9e3e905, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 2),  0xfcefa3f8, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 7),  0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 12), 0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in + (4 * 5),  0xfffa3942, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 8),  0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 11), 0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 14), 0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in + (4 * 1),  0xa4beea44, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 4),  0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 7),  0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 10), 0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in + (4 * 13), 0x289b7ec6, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 0),  0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 3),  0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 6),  0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in + (4 * 9),  0xd9d4d039, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 12), 0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 15), 0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 2),  0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in + (4 * 0),  0xf4292244, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 7),  0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 14), 0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 5),  0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in + (4 * 12), 0x655b59c3, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 3),  0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 10), 0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 1),  0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in + (4 * 8),  0x6fa87e4f, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 15), 0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 6),  0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 13), 0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in + (4 * 4),  0xf7537e82, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 11), 0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 2),  0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 9),  0xeb86d391, 21);

	p->sum[0] += a;
	p->sum[1] += b;
	p->sum[2] += c;
	p->sum[3] += d;
}

#undef F1
#undef F2
#undef F3
#undef F4
#undef MD5STEP

/* Add some bytes */
static void icmMD5_add(icmMD5 *p, ORD8 *ibuf, unsigned int len) {
	unsigned int bs;

	if (p->fin)
		return;				/* This is actually an error */

	bs = p->tlen;			/* Current bytes added */
	p->tlen = bs + len;		/* Update length after adding this buffer */
	bs &= 0x3f;				/* Bytes already in buffer */

	/* Deal with any existing partial bytes in p->buf */
	if (bs) {
		ORD8 *np = (ORD8 *)p->buf + bs;	/* Next free location in partial buffer */

		bs = 64 - bs;		/* Free space in partial buffer */

		if (len < bs) {		/* Not enought new to make a full buffer */
			memmove(np, ibuf, len);
			return;
		}

		memmove(np, ibuf, bs);	/* Now got one full buffer */
		icmMD5_accume(p, np);
		ibuf += bs;
		len -= bs;
	}

	/* Deal with input data 64 bytes at a time */
	while (len >= 64) {
		icmMD5_accume(p, ibuf);
		ibuf += 64;
		len -= 64;
	}

	/* Deal with any remaining bytes */
	memmove(p->buf, ibuf, len);
}

/* Finalise the checksum and return the result. */
static void icmMD5_get(icmMD5 *p, ORD8 chsum[16]) {
	int i;
	unsigned count;
	ORD32 bits1, bits0;
	ORD8 *pp;

	if (p->fin == 0) {
	
		/* Compute number of bytes processed mod 64 */
		count = p->tlen & 0x3f;

		/* Set the first char of padding to 0x80.  This is safe since there is
		   always at least one byte free */
		pp = p->buf + count;
		*pp++ = 0x80;

		/* Bytes of padding needed to make 64 bytes */
		count = 64 - 1 - count;

		/* Pad out to 56 mod 64, allowing 8 bytes for length in bits. */
		if (count < 8) {	/* Not enough space for padding and length */

			memset(pp, 0, count);
			icmMD5_accume(p, p->buf);

			/* Now fill the next block with 56 bytes */
			memset(p->buf, 0, 56);
		} else {
			/* Pad block to 56 bytes */
			memset(pp, 0, count - 8);
		}

		/* Compute number of bits */
		bits1 = 0x7 & (p->tlen >> (32 - 3)); 
		bits0 = p->tlen << 3;

		/* Append number of bits */
		p->buf[64 - 8] = bits0 & 0xff; 
		p->buf[64 - 7] = (bits0 >> 8) & 0xff; 
		p->buf[64 - 6] = (bits0 >> 16) & 0xff; 
		p->buf[64 - 5] = (bits0 >> 24) & 0xff; 
		p->buf[64 - 4] = bits1 & 0xff; 
		p->buf[64 - 3] = (bits1 >> 8) & 0xff; 
		p->buf[64 - 2] = (bits1 >> 16) & 0xff; 
		p->buf[64 - 1] = (bits1 >> 24) & 0xff; 

		icmMD5_accume(p, p->buf);

		p->fin = 1;
	}

	/* Return the result, lsb to msb */
	pp = chsum;
	for (i = 0; i < 4; i++) {
		*pp++ = p->sum[i] & 0xff; 
		*pp++ = (p->sum[i] >> 8) & 0xff; 
		*pp++ = (p->sum[i] >> 16) & 0xff; 
		*pp++ = (p->sum[i] >> 24) & 0xff; 
	}
}

/* Take a reference */
static icmMD5 *icmMD5_reference(icmMD5 *p) {
	p->refcount++;
	return p;
}

/* Delete the instance */
static void icmMD5_del(icmMD5 *p) {
	if (p == NULL || --p->refcount > 0)
		return;
	{
		icmAlloc *al = p->al;

		al->free(al, p);
		al->del(al);		/* delete if last reference to al */
	}
}

/* Create a new MD5 checksumming object, with a reset checksum value */
/* Return it or NULL if malloc failes */
/* Note that this will fail if e has an error already set */
icmMD5 *new_icmMD5_a(icmErr *e, icmAlloc *al) {
	icmMD5 *p;

	if (e != NULL && e->c != ICM_ERR_OK)
		return NULL;		/* Pre-existing error */

	if ((p = (icmMD5 *)al->calloc(al,1,sizeof(icmMD5))) == NULL) {
		icm_err_e(e, ICM_ERR_MALLOC, "Allocating icmMD5 object failed");	
		return NULL;
	}

	p->refcount  = 1;
	p->al        = al->reference(al);
	p->reset     = icmMD5_reset;
	p->add       = icmMD5_add;
	p->get       = icmMD5_get;
	p->reference = icmMD5_reference;
	p->del       = icmMD5_del;

	p->reset(p);

	return p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Dumy icmFile used to compute MD5 checksum on write */

/* Get the size of the file (Only valid for reading file). */
static size_t icmFileMD5_get_size(icmFile *pp) {
	icmFileMD5 *p = (icmFileMD5 *)pp;

	return p->size;
}

/* Set current position to offset. Return 0 on success, nz on failure. */
/* Seek can't be supported for MD5, so and seek must be to current location. */
static int icmFileMD5_seek(
icmFile *pp,
unsigned int offset
) {
	icmFileMD5 *p = (icmFileMD5 *)pp;

	if (p->of != offset) {
		icm_err_e(&p->e, ICM_ERR_INTERNAL, "icmFileMD5_seek: discontinuous write breaks MD5 calculation");	
		return 1;
	}
	if (p->of > p->size)
		p->size = p->of;
	return 0;
}

/* Read count items of size length. Return number of items successfully read. */
/* Read is not implemented */
static size_t icmFileMD5_read(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileMD5 *p = (icmFileMD5 *)pp;
	icm_err_e(&p->e, ICM_ERR_INTERNAL, "icmFileMD5_read: not implemented");	
	return 0;
}

/* write count items of size length. Return number of items successfully written. */
/* Simply pass to MD5 to compute checksum */
static size_t icmFileMD5_write(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileMD5 *p = (icmFileMD5 *)pp;
	size_t len = size * count;

	p->md5->add(p->md5, (ORD8 *)buffer, len);
	p->of += len;
	if (p->of > p->size)
		p->size = p->of;
	return count;
}

/* do a printf */
/* Not implemented */
static int icmFileMD5_printf(
icmFile *pp,
const char *format,
...
) {
	icmFileMD5 *p = (icmFileMD5 *)pp;
	icm_err_e(&p->e, ICM_ERR_INTERNAL, "icmFileMD5_printf: not implemented");	
	return 0;
}

/* flush all write data out to secondary storage. Return nz on failure. */
static int icmFileMD5_flush(
icmFile *pp
) {
	return 0;
}

/* Take a reference of the icmFileMD5 */
static icmFile *icmFileMD5_reference(
icmFile *pp
) {
	pp->refcount++;
	return pp;
}

/* we're done with the file object, return nz on failure */
static int icmFileMD5_delete(
icmFile *pp
) {
	if (pp == NULL || --pp->refcount > 0)
		return 0;
	{
		icmFileMD5 *p = (icmFileMD5 *)pp;
		icmAlloc *al = p->al;
	
		p->md5->del(p->md5);	/* Free reference to md5 */
		al->free(al, p);		/* Free this object */
		al->del(al);			/* Free allocator if this is the last reference */
		return 0;
	}
}

/* Create a checksum dump file access class with allocator */
icmFile *new_icmFileMD5_a(
icmMD5 *md5,		/* MD5 object to use */
icmAlloc *al		/* heap allocator to reference */
) {
	icmFileMD5 *p;

	if ((p = (icmFileMD5 *) al->calloc(al, 1, sizeof(icmFileMD5))) == NULL) {
		return NULL;
	}
	p->refcount  = 1;
	p->md5       = md5->reference(md5);		/* MD5 compute object */
	p->al        = al->reference(al);		/* Heap allocator */
	p->get_size  = icmFileMD5_get_size;
	p->seek      = icmFileMD5_seek;
	p->read      = icmFileMD5_read;
	p->write     = icmFileMD5_write;
	p->printf    = icmFileMD5_printf;
	p->flush     = icmFileMD5_flush;
	p->reference = icmFileMD5_reference;
	p->del       = icmFileMD5_delete;
	p->of = 0;

	return (icmFile *)p;
}

/* ============================================= */
/* Implementation of color transform lookups.    */

/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Methods common to all transforms (icmLuBase) : */

#define LUTSPACES(TTYPE) (void (*)(TTYPE *, 			\
	icColorSpaceSignature *, int *,	icColorSpaceSignature *, int *,	icColorSpaceSignature *))

/* Return information about the native lut in/out/pcs colorspaces. */
/* Any pointer may be NULL if value is not to be returned */
static void
icmLutSpaces(
	struct _icmLuBase *p,			/* This */
	icColorSpaceSignature *ins,		/* Return Native input color space */
	int *inn,						/* Return number of input components */
	icColorSpaceSignature *outs,	/* Return Native output color space */
	int *outn,						/* Return number of output components */
	icColorSpaceSignature *pcs		/* Return Native PCS color space */
									/* (this will be the same as ins or outs */
									/* depending on the lookup direction) */
) {
	if (ins != NULL)
		*ins = p->inSpace;
	if (inn != NULL)
		*inn = (int)icmCSSig2nchan(p->inSpace);

	if (outs != NULL)
		*outs = p->outSpace;
	if (outn != NULL)
		*outn = (int)icmCSSig2nchan(p->outSpace);
	if (pcs != NULL)
		*pcs = p->pcs;
}

#define LUSPACES(TTYPE) (void (*)(TTYPE *, 								\
	icColorSpaceSignature *, int *, icColorSpaceSignature *, int *,		\
	icmLuAlgType *, icRenderingIntent *, icmLookupFunc *,				\
	icColorSpaceSignature *, icmLookupOrder *))

/* Return information about the effective lookup in/out colorspaces, */
/* including allowance for PCS override. */
/* Any pointer may be NULL if value is not to be returned */
static void
icmLuSpaces(
	struct _icmLuBase *p,			/* This */
	icColorSpaceSignature *ins,		/* Return effective input color space */
	int *inn,						/* Return number of input components */
	icColorSpaceSignature *outs,	/* Return effective output color space */
	int *outn,						/* Return number of output components */
	icmLuAlgType *alg,				/* Return type of lookup algorithm used */
    icRenderingIntent *intt,		/* Return the intent being implented */
    icmLookupFunc *fnc,				/* Return the profile function being implemented */
	icColorSpaceSignature *pcs,		/* Return the profile effective PCS */
	icmLookupOrder *ord				/* return the search Order */
) {
	if (ins != NULL)
		*ins = p->e_inSpace;
	if (inn != NULL)
		*inn = (int)icmCSSig2nchan(p->e_inSpace);

	if (outs != NULL)
		*outs = p->e_outSpace;
	if (outn != NULL)
		*outn = (int)icmCSSig2nchan(p->e_outSpace);

	if (alg != NULL)
		*alg = p->ttype;

    if (intt != NULL)
		*intt = p->intent;

	if (fnc != NULL)
		*fnc = p->function;

	if (pcs != NULL)
		*pcs = p->e_pcs;

	if (ord != NULL)
		*ord = p->order;
}

#define LUXYZ_REL2ABS(TTYPE) (void (*)(TTYPE *, double *, double *))

/* Relative to Absolute for this WP in XYZ */
static void icmLuXYZ_Rel2Abs(icmLuBase *p, double *out, double *in) {
	icmMulBy3x3(out, p->toAbs, in);
}

#define LUXYZ_ABS2REL(TTYPE) (void (*)(TTYPE *, double *, double *))

/* Absolute to Relative for this WP in XYZ */
static void icmLuXYZ_Abs2Rel(icmLuBase *p, double *out, double *in) {
	icmMulBy3x3(out, p->fromAbs, in);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Methods common to all non-named transforms (icmLuBase) : */

#define LUINIT_WH_BK(TTYPE) (int (*)(TTYPE *))

/* Initialise the LU white and black points from the ICC tags, */
/* and the corresponding absolute<->relative conversion matrices */
/* return nz on error */
static int icmLuInit_Wh_bk(
struct _icmLuBase *lup
) {
	icmXYZArray *whitePointTag, *blackPointTag;
	icc *p = lup->icp;

	if ((whitePointTag = (icmXYZArray *)p->read_tag(p, icSigMediaWhitePointTag)) == NULL
        || whitePointTag->ttype != icSigXYZType || whitePointTag->count < 1) {
		if (p->header->deviceClass != icSigLinkClass 
		 && (lup->intent == icAbsoluteColorimetric
		  || lup->intent == icmAbsolutePerceptual
		  || lup->intent == icmAbsoluteSaturation)) {
			return icm_err(p, 1,"icc_lookup: Profile is missing Media White Point Tag");
		}
		lup->whitePoint = icmD50;					/* safe value */
	} else
		lup->whitePoint = whitePointTag->data[0];	/* Copy structure */

	if ((blackPointTag = (icmXYZArray *)p->read_tag(p, icSigMediaBlackPointTag)) == NULL
        || blackPointTag->ttype != icSigXYZType || blackPointTag->count < 1) {
		lup->blackPoint = icmBlack;					/* default */
		lup->blackisassumed = 1;					/* We assumed the default */
	} else  {
		lup->blackPoint = blackPointTag->data[0];	/* Copy structure */
		lup->blackisassumed = 0;					/* The black is from the tag */
	}

	/* If this is a Display profile, check if there is a 'chad' tag, then */
	/* setup the white point and toAbs/fromAbs matricies from that, so as to implement an */
	/* effective Absolute Colorimetric intent for such profiles. */
 	if (p->header->deviceClass == icSigDisplayClass
	 && p->naturalChad && p->chadmxValid) {
		double wp[3];

		/* Conversion matrix is chad matrix. */
		icmCpy3x3(lup->fromAbs, p->chadmx);
		icmInverse3x3(lup->toAbs, lup->fromAbs);

		/* Compute absolute white point. We deliberately ignore what's in the white point tag */
		/* and assume D50, since dealing with a non-D50 white point tag is contrary to ICCV4 */
		/* and full of ambiguity (i.e. is it a separate "media" white different to the */
		/* display white and not D50, or has the profile creator mistakenly put the display */
		/* white in the white point tag ?) */
		icmMulBy3x3(wp, lup->toAbs, icmD50_ary3); 
		icmAry2XYZ(lup->whitePoint, wp);

		DBLLL(("toAbs and fromAbs created from 'chad' tag\n"));
		DBLLL(("computed wp %.8f %.8f %.8f\n", lup->whitePoint.X,
		                               lup->whitePoint.Y, lup->whitePoint.Z));

	/* If this is an Output profile, check if there is a 'chad' tag, and */
	/* setup the toAbs/fromAbs matricies so that they include it, so as to implement an */
	/* effective Absolute Colorimetric intent for such profiles. */
 	} else if (p->header->deviceClass == icSigOutputClass
	 && p->naturalChad && p->chadmxValid) {
		double wp[3];
		double ichad[3][3];

		/* Convert the white point tag value backwards through the 'chad' */
		icmXYZ2Ary(wp, lup->whitePoint);
		icmInverse3x3(ichad, p->chadmx);
		icmMulBy3x3(wp, ichad, wp); 
		icmAry2XYZ(lup->whitePoint, wp);

		/* Create absolute <-> relative conversion matricies */
		p->chromAdaptMatrix(p, ICM_CAM_NONE, lup->toAbs, lup->fromAbs, icmD50, lup->whitePoint);
		DBLLL(("toAbs and fromAbs created from 'chad' tag & WP tag\n"));
		DBLLL(("toAbs and fromAbs created from wp %f %f %f and D50 %f %f %f\n", lup->whitePoint.X,
		                       lup->whitePoint.Y, lup->whitePoint.Z, icmD50.X, icmD50.Y, icmD50.Z));
	} else {
		/* Create absolute <-> relative conversion matricies */
		p->chromAdaptMatrix(p, ICM_CAM_NONE, lup->toAbs, lup->fromAbs, icmD50, lup->whitePoint);
		DBLLL(("toAbs and fromAbs created from wp %f %f %f and D50 %f %f %f\n", lup->whitePoint.X,
		                       lup->whitePoint.Y, lup->whitePoint.Z, icmD50.X, icmD50.Y, icmD50.Z));
	}

	DBLLL(("toAbs   = %f %f %f\n          %f %f %f\n          %f %f %f\n",
	        lup->toAbs[0][0], lup->toAbs[0][1], lup->toAbs[0][2],
	        lup->toAbs[1][0], lup->toAbs[1][1], lup->toAbs[1][2],
	        lup->toAbs[2][0], lup->toAbs[2][1], lup->toAbs[2][2]));
	DBLLL(("fromAbs = %f %f %f\n          %f %f %f\n          %f %f %f\n",
	        lup->fromAbs[0][0], lup->fromAbs[0][1], lup->fromAbs[0][2],
	        lup->fromAbs[1][0], lup->fromAbs[1][1], lup->fromAbs[1][2],
	        lup->fromAbs[2][0], lup->fromAbs[2][1], lup->fromAbs[2][2]));

	return 0;
}

#define LUWH_BK_POINTS(TTYPE) (int (*)(TTYPE *, double *, double *))

/* Return the media white and black points in absolute XYZ space. */
/* Note that if not in the icc, the black point will be returned as 0, 0, 0, */
/* and the function will return nz. */ 
/* Any pointer may be NULL if value is not to be returned */
static int icmLuWh_bk_points(
struct _icmLuBase *p,
double *wht,
double *blk
) {
	if (wht != NULL) {
		icmXYZ2Ary(wht,p->whitePoint);
	}

	if (blk != NULL) {
		icmXYZ2Ary(blk,p->blackPoint);
	}
	if (p->blackisassumed)
		return 1;
	return 0;
}

#define LULU_WH_BK_POINTS(TTYPE) (int (*)(TTYPE *, double *, double *))

/* Get the LU white and black points in LU PCS space, converted to XYZ. */
/* (ie. white and black will be relative if LU is relative intent etc.) */
/* Return nz if the black point is being assumed to be 0,0,0 rather */
/* than being from the tag. */															\
static int icmLuLu_wh_bk_points(
struct _icmLuBase *p,
double *wht,
double *blk
) {
	if (wht != NULL) {
		icmXYZ2Ary(wht,p->whitePoint);
	}

	if (blk != NULL) {
		icmXYZ2Ary(blk,p->blackPoint);
	}
	if (p->intent != icAbsoluteColorimetric
	 && p->intent != icmAbsolutePerceptual
	 && p->intent != icmAbsoluteSaturation) {
		if (wht != NULL)
			icmMulBy3x3(wht, p->fromAbs, wht);
		if (blk != NULL)
			icmMulBy3x3(blk, p->fromAbs, blk);
	}
	if (p->blackisassumed)
		return 1;
	return 0;
}

#define LU_GET_LUTRANGES(TTYPE) (void (*)(TTYPE *, double *, double *, double *, double *))

/* Get the native (internal) ranges for the Monochrome or Matrix profile */
/* Arguments may be NULL */
static void
icmLu_get_lutranges (
	struct _icmLuBase *p,
	double *inmin, double *inmax,		/* Return maximum range of inspace values */
	double *outmin, double *outmax		/* Return maximum range of outspace values */
) {
	icTagTypeSignature tagType;

	if (p->ttype == icmLutType) {
		icmLuLut *pp = (icmLuLut *)p;
		tagType = pp->lut->ttype;
	} else {
		tagType = icMaxEnumType;
	}

	/* Hmm. we have no way of handling an error from getRange. */
	/* It shouldn't ever return one unless there is a mismatch between */
	/* getRange and Lu creation... */
	getRange(p->icp, p->inSpace, tagType, inmin, inmax);
	getRange(p->icp, p->outSpace, tagType, outmin, outmax);
}

#define LU_GET_RANGES(TTYPE) (void (*)(TTYPE *, double *, double *, double *, double *))

/* Get the effective (externally visible) ranges for the all profile types */
/* Arguments may be NULL */
static void
icmLu_get_ranges (
	struct _icmLuBase *p,
	double *inmin, double *inmax,		/* Return maximum range of inspace values */
	double *outmin, double *outmax		/* Return maximum range of outspace values */
) {
	icTagTypeSignature tagType;

	if (p->ttype == icmLutType) {
		icmLuLut *pp = (icmLuLut *)p;
		tagType = pp->lut->ttype;
	} else {
		tagType = icMaxEnumType;
	}
	/* Hmm. we have no way of handling an error from getRange. */
	/* It shouldn't ever return one unless there is a mismatch between */
	/* getRange and Lu creation... */
	getRange(p->icp, p->e_inSpace, tagType, inmin, inmax);
	getRange(p->icp, p->e_outSpace, tagType, outmin, outmax);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Forward and Backward Monochrome type methods: */
/* Return 0 on success, 1 if clipping occured, 2 on other error */

/* Individual components of Fwd conversion: */

/* Actual device to linearised device */
static int
icmLuMonoFwd_curve (
icmLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icc *icp = p->icp;
	int rv = 0;

	/* Translate from device to PCS scale */
	if ((rv |= p->grayCurve->lookup_fwd(p->grayCurve,&out[0],&in[0])) > 1) {
		icm_err(icp, rv,"icc_lookup: Curve->lookup_fwd() failed");
		return 2;
	}

	return rv;
}

/* Linearised device to relative PCS */
static int
icmLuMonoFwd_map (
icmLuMono *p,		/* This */
double *out,		/* Vector of output values (native space) */
double *in			/* Vector of input values (native space) */
) {
	int rv = 0;
	double Y = in[0];		/* In case out == in */

	out[0] = p->pcswht.X;
	out[1] = p->pcswht.Y;
	out[2] = p->pcswht.Z;
	if (p->pcs == icSigLabData)
		icmXYZ2Lab(&p->pcswht, out, out);	/* in Lab */

	/* Scale linearized device level to PCS white */
	out[0] *= Y;
	out[1] *= Y;
	out[2] *= Y;

	return rv;
}

/* relative PCS to absolute PCS (if required) */
static int
icmLuMonoFwd_abs (	/* Abs comes last in Fwd conversion */
icmLuMono *p,		/* This */
double *out,		/* Vector of output values in Effective PCS */
double *in			/* Vector of input values in Native PCS */
) {
	int rv = 0;

	if (out != in) {	/* Don't alter input values */
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	}

	/* Do absolute conversion */
	if (p->intent == icAbsoluteColorimetric
	 || p->intent == icmAbsolutePerceptual
	 || p->intent == icmAbsoluteSaturation) {

		if (p->pcs == icSigLabData) 	/* Convert L to Y */
			icmLab2XYZ(&p->pcswht, out, out);
		
		/* Convert from Relative to Absolute colorimetric */
		icmMulBy3x3(out, p->toAbs, out);
		
		if (p->e_pcs == icSigLabData)
			icmXYZ2Lab(&p->pcswht, out, out);

	} else {

		/* Convert from Native to Effective output space */
		if (p->pcs == icSigLabData && p->e_pcs == icSigXYZData)
			icmLab2XYZ(&p->pcswht, out, out);
		else if (p->pcs == icSigXYZData && p->e_pcs == icSigLabData)
			icmXYZ2Lab(&p->pcswht, out, out);
	}

	return rv;
}


/* Overall Fwd conversion routine (Dev->PCS) */
static int
icmLuMonoFwd_lookup (
icmLuMono *p,
double *out,		/* Vector of output values */
double *in			/* Input value */
) {
	int rv = 0;
	rv |= icmLuMonoFwd_curve(p, out, in);
	rv |= icmLuMonoFwd_map(p, out, out);
	rv |= icmLuMonoFwd_abs(p, out, out);
	return rv;
}

/* Three stage conversion routines */
static int
icmLuMonoFwd_lookup_in(
icmLuMono *p,
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= icmLuMonoFwd_curve(p, out, in);
	return rv;
}

static int
icmLuMonoFwd_lookup_core(
icmLuMono *p,
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= icmLuMonoFwd_map(p, out, in);
	rv |= icmLuMonoFwd_abs(p, out, out);
	return rv;
}

static int
icmLuMonoFwd_lookup_out(
icmLuMono *p,
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	int rv = 0;
	out[0] = in[0]; 
	out[1] = in[1]; 
	out[2] = in[2]; 
	return rv;
}


/* -  -  -  -  -  -  -  -  -  -  -  -  -  - */
/* Individual components of Bwd conversion: */

/* Convert from relative PCS to absolute PCS (if required) */
static int
icmLuMonoBwd_abs (	/* Abs comes first in Bwd conversion */
icmLuMono *p,		/* This */
double *out,		/* Vector of output values in Native PCS */
double *in			/* Vector of input values in Effective PCS */
) {
	int rv = 0;

	if (out != in) {	/* Don't alter input values */
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	}

	/* Force to monochrome locus in correct space */
	if (p->e_pcs == icSigLabData) {
		double wp[3];

		if (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation) {
			wp[0] = p->whitePoint.X;
			wp[1] = p->whitePoint.Y;
			wp[2] = p->whitePoint.Z;
		} else {
			wp[0] = p->pcswht.X;
			wp[1] = p->pcswht.Y;
			wp[2] = p->pcswht.Z;
		}
		icmXYZ2Lab(&p->pcswht, wp, wp);	/* Convert to Lab white point */
		out[1] = out[0]/wp[0] * wp[1];
		out[2] = out[0]/wp[0] * wp[2];

	} else {
		if (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation) {
			out[0] = out[1]/p->whitePoint.Y * p->whitePoint.X;
			out[2] = out[1]/p->whitePoint.Y * p->whitePoint.Z;
		} else {
			out[0] = out[1]/p->pcswht.Y * p->pcswht.X;
			out[2] = out[1]/p->pcswht.Y * p->pcswht.Z;
		}
	}

	/* Do absolute conversion, and conversion to effective PCS */
	if (p->intent == icAbsoluteColorimetric
	 || p->intent == icmAbsolutePerceptual
	 || p->intent == icmAbsoluteSaturation) {

		if (p->e_pcs == icSigLabData)
			icmLab2XYZ(&p->pcswht, out, out);

		icmMulBy3x3(out, p->fromAbs, out);

		/* Convert from Effective to Native input space */
		if (p->pcs == icSigLabData)
			icmXYZ2Lab(&p->pcswht, out, out);

	} else {

		/* Convert from Effective to Native input space */
		if (p->e_pcs == icSigLabData && p->pcs == icSigXYZData)
			icmLab2XYZ(&p->pcswht, out, out);
		else if (p->e_pcs == icSigXYZData && p->pcs == icSigLabData)
			icmXYZ2Lab(&p->pcswht, out, out);
	}

	return rv;
}

/* Map from relative PCS to linearised device */
static int
icmLuMonoBwd_map (
icmLuMono *p,		/* This */
double *out,		/* Output value */
double *in			/* Vector of input values (native space) */
) {
	int rv = 0;
	double pcsw[3];

	pcsw[0] = p->pcswht.X;
	pcsw[1] = p->pcswht.Y;
	pcsw[2] = p->pcswht.Z;
	if (p->pcs == icSigLabData)
		icmXYZ2Lab(&p->pcswht, pcsw, pcsw);	/* in Lab (should be 100.0!) */

	/* Divide linearized device level into PCS white luminence */
	if (p->pcs == icSigLabData)
		out[0] = in[0]/pcsw[0];
	else
		out[0] = in[1]/pcsw[1];

	return rv;
}

/* Map from linearised device to actual device */
static int
icmLuMonoBwd_curve (
icmLuMono *p,		/* This */
double *out,		/* Output value */
double *in			/* Input value */
) {
	icc *icp = p->icp;
	int rv = 0;

	/* Convert to device value through curve */
	if ((rv = p->grayCurve->lookup_bwd(p->grayCurve,&out[0],&in[0])) > 1) {
		icm_err(icp, rv,"icc_lookup: Curve->lookup_bwd() failed");
		return 2;
	}

	return rv;
}

/* Overall Bwd conversion routine (PCS->Dev) */
static int
icmLuMonoBwd_lookup (
icmLuMono *p,
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	double temp[3];
	int rv = 0;
	rv |= icmLuMonoBwd_abs(p, temp, in);
	rv |= icmLuMonoBwd_map(p, out, temp);
	rv |= icmLuMonoBwd_curve(p, out, out);
	return rv;
}

/* Three stage conversion routines */
static int
icmLuMonoBwd_lookup_in(
icmLuMono *p,
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	int rv = 0;
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	return rv;
}

static int
icmLuMonoBwd_lookup_core(
icmLuMono *p,
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	double temp[3];
	int rv = 0;
	rv |= icmLuMonoBwd_abs(p, temp, in);
	rv |= icmLuMonoBwd_map(p, out, temp);
	return rv;
}

static int
icmLuMonoBwd_lookup_out(
icmLuMono *p,
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= icmLuMonoBwd_curve(p, out, in);
	return rv;
}

/* -  -  -  -  -  -  -  -  -  -  -  -  -  - */

static void
icmLuMono_delete(
icmLuMono *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

static icmLuBase *
new_icmLuMono(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func,			/* Functionality requested */
	int dir								/* 0 = fwd, 1 = bwd */
) {
	icmLuMono *p;

	if ((p = (icmLuMono *) icp->al->calloc(icp->al,1,sizeof(icmLuMono))) == NULL)
		return NULL;
	p->icp      = icp;
	p->del      = icmLuMono_delete;

	p->lutspaces = LUTSPACES(icmLuMono) icmLutSpaces;
	p->spaces   = LUSPACES(icmLuMono) icmLuSpaces;
	p->XYZ_Rel2Abs = LUXYZ_REL2ABS(icmLuMono) icmLuXYZ_Rel2Abs;
	p->XYZ_Abs2Rel = LUXYZ_ABS2REL(icmLuMono) icmLuXYZ_Abs2Rel;
	p->get_lutranges = LU_GET_LUTRANGES(icmLuMono) icmLu_get_lutranges;
	p->get_ranges = LU_GET_RANGES(icmLuMono) icmLu_get_ranges;
	p->init_wh_bk = LUINIT_WH_BK(icmLuMono) icmLuInit_Wh_bk;
	p->wh_bk_points = LUWH_BK_POINTS(icmLuMono) icmLuWh_bk_points;
	p->lu_wh_bk_points = LULU_WH_BK_POINTS(icmLuMono) icmLuLu_wh_bk_points;

	p->fwd_lookup = icmLuMonoFwd_lookup;
	p->fwd_curve  = icmLuMonoFwd_curve;
	p->fwd_map    = icmLuMonoFwd_map;
	p->fwd_abs    = icmLuMonoFwd_abs;
	p->bwd_lookup = icmLuMonoBwd_lookup;
	p->bwd_abs    = icmLuMonoFwd_abs;
	p->bwd_map    = icmLuMonoFwd_map;
	p->bwd_curve  = icmLuMonoFwd_curve;
	if (dir) {
		p->ttype         = icmMonoBwdType;
		p->lookup        = icmLuMonoBwd_lookup;
		p->lookup_in     = icmLuMonoBwd_lookup_in;
		p->lookup_core   = icmLuMonoBwd_lookup_core;
		p->lookup_out    = icmLuMonoBwd_lookup_out;
		p->lookup_inv_in = icmLuMonoFwd_lookup_out;		/* Opposite of Bwd_lookup_in */
	} else {
		p->ttype         = icmMonoFwdType;
		p->lookup        = icmLuMonoFwd_lookup;
		p->lookup_in     = icmLuMonoFwd_lookup_in;
		p->lookup_core   = icmLuMonoFwd_lookup_core;
		p->lookup_out    = icmLuMonoFwd_lookup_out;
		p->lookup_inv_in = icmLuMonoBwd_lookup_out;		/* Opposite of Fwd_lookup_in */
	}

	/* Lookup the white and black points */
	if (p->init_wh_bk(p)) {
		p->del(p);
		return NULL;
	}

	/* See if the color spaces are appropriate for the mono type */
	if (icmCSSig2nchan(icp->header->colorSpace) != 1
	  || ( icp->header->pcs != icSigXYZData && icp->header->pcs != icSigLabData)) {
		p->del(p);
		return NULL;
	}

	/* Find the appropriate tags */
	if ((p->grayCurve = (icmCurve *)icp->read_tag(icp, icSigGrayTRCTag)) == NULL
         || p->grayCurve->ttype != icSigCurveType) {
		p->del(p);
		return NULL;
	}

	p->pcswht = icp->header->illuminant;
	p->intent   = intent;
	p->function = func;
	p->inSpace  = inSpace;
	p->outSpace = outSpace;
	p->pcs      = pcs;
	p->e_inSpace  = e_inSpace;
	p->e_outSpace = e_outSpace;
	p->e_pcs      = e_pcs;

	return (icmLuBase *)p;
}

static icmLuBase *
new_icmLuMonoFwd(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func			/* Functionality requested */
) {
	return new_icmLuMono(icp, inSpace, outSpace, pcs, e_inSpace, e_outSpace, e_pcs,
	                     intent, func, 0);
}


static icmLuBase *
new_icmLuMonoBwd(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func			/* Functionality requested */
) {
	return new_icmLuMono(icp, inSpace, outSpace, pcs, e_inSpace, e_outSpace, e_pcs,
	                     intent, func, 1);
}

/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Forward and Backward Matrix type conversion */
/* Return 0 on success, 1 if clipping occured, 2 on other error */

/* Individual components of Fwd conversion: */
static int
icmLuMatrixFwd_curve (
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icc *icp = p->icp;
	int rv = 0;

	/* Curve lookups */
	if ((rv |= p->redCurve->lookup_fwd(  p->redCurve,  &out[0],&in[0])) > 1
	 || (rv |= p->greenCurve->lookup_fwd(p->greenCurve,&out[1],&in[1])) > 1
	 || (rv |= p->blueCurve->lookup_fwd( p->blueCurve, &out[2],&in[2])) > 1) {
		icm_err(icp, rv,"icc_lookup: Curve->lookup_fwd() failed");
		return 2;
	}

	return rv;
}

static int
icmLuMatrixFwd_matrix (
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	double tt[3];

	/* Matrix */
	tt[0] = p->mx[0][0] * in[0] + p->mx[0][1] * in[1] + p->mx[0][2] * in[2];
	tt[1] = p->mx[1][0] * in[0] + p->mx[1][1] * in[1] + p->mx[1][2] * in[2];
	tt[2] = p->mx[2][0] * in[0] + p->mx[2][1] * in[1] + p->mx[2][2] * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];

	return rv;
}

static int
icmLuMatrixFwd_abs (/* Abs comes last in Fwd conversion */
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;

	if (out != in) {	/* Don't alter input values */
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	}

	/* If required, convert from Relative to Absolute colorimetric */
	if (p->intent == icAbsoluteColorimetric
	 || p->intent == icmAbsolutePerceptual
	 || p->intent == icmAbsoluteSaturation) {
		icmMulBy3x3(out, p->toAbs, out);
	}

	/* If e_pcs is Lab, then convert XYZ to Lab */
	if (p->e_pcs == icSigLabData)
		icmXYZ2Lab(&p->pcswht, out, out);

	return rv;
}


/* Overall Fwd conversion (Dev->PCS)*/
static int
icmLuMatrixFwd_lookup (
icmLuMatrix *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= icmLuMatrixFwd_curve(p, out, in);
	rv |= icmLuMatrixFwd_matrix(p, out, out);
	rv |= icmLuMatrixFwd_abs(p, out, out);
	return rv;
}

/* Three stage conversion routines */
static int
icmLuMatrixFwd_lookup_in (
icmLuMatrix *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= icmLuMatrixFwd_curve(p, out, in);
	return rv;
}

static int
icmLuMatrixFwd_lookup_core (
icmLuMatrix *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= icmLuMatrixFwd_matrix(p, out, in);
	rv |= icmLuMatrixFwd_abs(p, out, out);
	return rv;
}

static int
icmLuMatrixFwd_lookup_out (
icmLuMatrix *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	return rv;
}

/* -  -  -  -  -  -  -  -  -  -  -  -  -  - */
/* Individual components of Bwd conversion: */

static int
icmLuMatrixBwd_abs (/* Abs comes first in Bwd conversion */
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;

	if (out != in) {	/* Don't alter input values */
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	}

	/* If e_pcs is Lab, then convert Lab to XYZ */
	if (p->e_pcs == icSigLabData)
		icmLab2XYZ(&p->pcswht, out, out);

	/* If required, convert from Absolute to Relative colorimetric */
	if (p->intent == icAbsoluteColorimetric
	 || p->intent == icmAbsolutePerceptual
	 || p->intent == icmAbsoluteSaturation) {
		icmMulBy3x3(out, p->fromAbs, out);
	}

	return rv;
}

static int
icmLuMatrixBwd_matrix (
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	double tt[3];

	tt[0] = in[0];
	tt[1] = in[1];
	tt[2] = in[2];

	/* Matrix */
	out[0] = p->bmx[0][0] * tt[0] + p->bmx[0][1] * tt[1] + p->bmx[0][2] * tt[2];
	out[1] = p->bmx[1][0] * tt[0] + p->bmx[1][1] * tt[1] + p->bmx[1][2] * tt[2];
	out[2] = p->bmx[2][0] * tt[0] + p->bmx[2][1] * tt[1] + p->bmx[2][2] * tt[2];

	return rv;
}

static int
icmLuMatrixBwd_curve (
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icc *icp = p->icp;
	int rv = 0;

	/* Curves */
	if ((rv |= p->redCurve->lookup_bwd(p->redCurve,&out[0],&in[0])) > 1
	 ||	(rv |= p->greenCurve->lookup_bwd(p->greenCurve,&out[1],&in[1])) > 1
	 || (rv |= p->blueCurve->lookup_bwd(p->blueCurve,&out[2],&in[2])) > 1) {
		icm_err(icp, rv,"icc_lookup: Curve->lookup_bwd() failed");
		return 2;
	}
	return rv;
}

/* Overall Bwd conversion (PCS->Dev) */
static int
icmLuMatrixBwd_lookup (
icmLuMatrix *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= icmLuMatrixBwd_abs(p, out, in);
	rv |= icmLuMatrixBwd_matrix(p, out, out);
	rv |= icmLuMatrixBwd_curve(p, out, out);
	return rv;
}

/* Three stage conversion routines */
static int
icmLuMatrixBwd_lookup_in (
icmLuMatrix *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	return rv;
}

static int
icmLuMatrixBwd_lookup_core (
icmLuMatrix *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= icmLuMatrixBwd_abs(p, out, in);
	rv |= icmLuMatrixBwd_matrix(p, out, out);
	return rv;
}

static int
icmLuMatrixBwd_lookup_out (
icmLuMatrix *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= icmLuMatrixBwd_curve(p, out, in);
	return rv;
}

/* -  -  -  -  -  -  -  -  -  -  -  -  -  - */

static void
icmLuMatrix_delete(
icmLuMatrix *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* We setup valid fwd and bwd component conversions, */
/* but setup only the asked for overal conversion. */
static icmLuBase *
new_icmLuMatrix(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func,			/* Functionality requested */
	int dir								/* 0 = fwd, 1 = bwd */
) {
	icmLuMatrix *p;

	if ((p = (icmLuMatrix *) icp->al->calloc(icp->al,1,sizeof(icmLuMatrix))) == NULL)
		return NULL;
	p->icp      = icp;
	p->del      = icmLuMatrix_delete;
	p->lutspaces= LUTSPACES(icmLuMatrix) icmLutSpaces;

	p->spaces   = LUSPACES(icmLuMatrix)  icmLuSpaces;
	p->XYZ_Rel2Abs = LUXYZ_REL2ABS(icmLuMatrix)  icmLuXYZ_Rel2Abs;
	p->XYZ_Abs2Rel = LUXYZ_ABS2REL(icmLuMatrix)  icmLuXYZ_Abs2Rel;
	p->get_lutranges = LU_GET_LUTRANGES(icmLuMatrix)  icmLu_get_lutranges;
	p->get_ranges = LU_GET_RANGES(icmLuMatrix)  icmLu_get_ranges;
	p->init_wh_bk = LUINIT_WH_BK(icmLuMatrix)  icmLuInit_Wh_bk;
	p->wh_bk_points = LUWH_BK_POINTS(icmLuMatrix)  icmLuWh_bk_points;
	p->lu_wh_bk_points = LULU_WH_BK_POINTS(icmLuMatrix)  icmLuLu_wh_bk_points;

	p->fwd_lookup = icmLuMatrixFwd_lookup;
	p->fwd_curve  = icmLuMatrixFwd_curve;
	p->fwd_matrix = icmLuMatrixFwd_matrix;
	p->fwd_abs    = icmLuMatrixFwd_abs;
	p->bwd_lookup = icmLuMatrixBwd_lookup;
	p->bwd_abs    = icmLuMatrixBwd_abs;
	p->bwd_matrix = icmLuMatrixBwd_matrix;
	p->bwd_curve  = icmLuMatrixBwd_curve;
	if (dir) {
		p->ttype         = icmMatrixBwdType;
		p->lookup        = icmLuMatrixBwd_lookup;
		p->lookup_in     = icmLuMatrixBwd_lookup_in;
		p->lookup_core   = icmLuMatrixBwd_lookup_core;
		p->lookup_out    = icmLuMatrixBwd_lookup_out;
		p->lookup_inv_in = icmLuMatrixFwd_lookup_out;		/* Opposite of Bwd_lookup_in */
	} else {
		p->ttype         = icmMatrixFwdType;
		p->lookup        = icmLuMatrixFwd_lookup;
		p->lookup_in     = icmLuMatrixFwd_lookup_in;
		p->lookup_core   = icmLuMatrixFwd_lookup_core;
		p->lookup_out    = icmLuMatrixFwd_lookup_out;
		p->lookup_inv_in = icmLuMatrixBwd_lookup_out;		/* Opposite of Fwd_lookup_in */
	}

	/* Lookup the white and black points */
	if (p->init_wh_bk(p)) {
		p->del(p);
		return NULL;
	}

	/* Note that we can use matrix type even if PCS is Lab, */
	/* by simply converting it. */

	/* Find the appropriate tags */
	if ((p->redCurve = (icmCurve *)icp->read_tag(icp, icSigRedTRCTag)) == NULL
     || p->redCurve->ttype != icSigCurveType
	 || (p->greenCurve = (icmCurve *)icp->read_tag(icp, icSigGreenTRCTag)) == NULL
     || p->greenCurve->ttype != icSigCurveType
	 || (p->blueCurve = (icmCurve *)icp->read_tag(icp, icSigBlueTRCTag)) == NULL
     || p->blueCurve->ttype != icSigCurveType
	 || (p->redColrnt = (icmXYZArray *)icp->read_tag(icp, icSigRedColorantTag)) == NULL
     || p->redColrnt->ttype != icSigXYZType || p->redColrnt->count < 1
	 || (p->greenColrnt = (icmXYZArray *)icp->read_tag(icp, icSigGreenColorantTag)) == NULL
     || p->greenColrnt->ttype != icSigXYZType || p->greenColrnt->count < 1
	 || (p->blueColrnt = (icmXYZArray *)icp->read_tag(icp, icSigBlueColorantTag)) == NULL
     || p->blueColrnt->ttype != icSigXYZType || p->blueColrnt->count < 1) {
		p->del(p);
		return NULL;
	}

	/* Setup the matrix */
	p->mx[0][0] = p->redColrnt->data[0].X;
	p->mx[0][1] = p->greenColrnt->data[0].X;
	p->mx[0][2] = p->blueColrnt->data[0].X;
	p->mx[1][1] = p->greenColrnt->data[0].Y;
	p->mx[1][0] = p->redColrnt->data[0].Y;
	p->mx[1][2] = p->blueColrnt->data[0].Y;
	p->mx[2][1] = p->greenColrnt->data[0].Z;
	p->mx[2][0] = p->redColrnt->data[0].Z;
	p->mx[2][2] = p->blueColrnt->data[0].Z;

	/* Workaround for buggy Kodak RGB profiles. Their matrix values */
	/* may be scaled to 100 rather than 1.0, and the colorant curves */
	/* may be scaled by 0.5 */
	if (icp->header->cmmId == icmstr2tag("KCMS")) {
		int i, j, oc = 0;
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				if (p->mx[i][j] > 5.0)
					oc++;

		if (oc > 4) {		/* Looks like it */
			if ((p->icp->cflags & icmCFlagAllowQuirks) != 0) {
				// Fix it
				for (i = 0; i < 3; i++)
					for (j = 0; j < 3; j++)
						p->mx[i][j] /= 100.0;
				p->icp->op = icmSnRead;			/* Let icmQuirkWarning know direction */
				icmQuirkWarning(icp, ICM_FMT_MATRIX_SCALE, 0, "Matrix profile values have wrong scale");
			} else {
				p->del(p);
				icm_err(icp, ICM_FMT_MATRIX_SCALE, "Matrix profile values have wrong scale");
				return NULL;
			}
		}
	}

	if (icmInverse3x3(p->bmx, p->mx) != 0) {	/* Compute inverse */
		p->del(p);
		icm_err(icp, 2,"icc_new_iccLuMatrix: Matrix wasn't invertable");
		return NULL;
	}

	p->pcswht = icp->header->illuminant;
	p->intent   = intent;
	p->function = func;
	p->inSpace  = inSpace;
	p->outSpace = outSpace;
	p->pcs      = pcs;
	p->e_inSpace  = e_inSpace;
	p->e_outSpace = e_outSpace;
	p->e_pcs      = e_pcs;

	/* Lookup the white and black points */
	if (p->init_wh_bk(p)) {
		p->del(p);
		return NULL;
	}

	return (icmLuBase *)p;
}

static icmLuBase *
new_icmLuMatrixFwd(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func			/* Functionality requested */
) {
	return new_icmLuMatrix(icp, inSpace, outSpace, pcs, e_inSpace, e_outSpace, e_pcs,
	                       intent, func, 0);
}


static icmLuBase *
new_icmLuMatrixBwd(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func			/* Functionality requested */
) {
	return new_icmLuMatrix(icp, inSpace, outSpace, pcs, e_inSpace, e_outSpace, e_pcs,
	                       intent, func, 1);
}

/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Forward and Backward Multi-Dimensional Interpolation type conversion */
/* Return 0 on success, 1 if clipping occured, 2 on other error */

/* Components of overall lookup, in order */
static int icmLuLut_in_abs(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	DBLLL(("icm in_abs: input %s\n",icmPdv(lut->inputChan, in)));
	if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)		/* Don't alter input values */
			out[i] = in[i];
	}

	/* If Bwd Lut, take care of Absolute color space and effective input space */
	if ((p->function == icmBwd || p->function == icmGamut || p->function == icmPreview)
		&& (p->e_inSpace == icSigLabData
		 || p->e_inSpace == icSigXYZData)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation)) {
	
		if (p->e_inSpace == icSigLabData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm in_abs: after Lab2XYZ %s\n",icmPdv(lut->inputChan, out)));
		}

		/* Convert from Absolute to Relative colorimetric */
		icmMulBy3x3(out, p->fromAbs, out);
		DBLLL(("icm in_abs: after fromAbs %s\n",icmPdv(lut->inputChan, out)));
		
		if (p->inSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm in_abs: after XYZ2Lab %s\n",icmPdv(lut->inputChan, out)));
		}

	} else {

		/* Convert from Effective to Native input space */
		if (p->e_inSpace == icSigLabData && p->inSpace == icSigXYZData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm in_abs: after Lab2XYZ %s\n",icmPdv(lut->inputChan, out)));
		} else if (p->e_inSpace == icSigXYZData && p->inSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm in_abs: after XYZ2Lab %s\n",icmPdv(lut->inputChan, out)));
		}
	}
	DBLLL(("icm in_abs: returning %s\n",icmPdv(lut->inputChan, out)));

	return rv;
}

/* Possible matrix lookup */
static int icmLuLut_matrix(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	if (p->usematrix)		
		rv |= lut->lookup_matrix(lut,out,in);
	else if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)
			out[i] = in[i];
	}
	return rv;
}

/* Do input -> input' lookup */
static int icmLuLut_input(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	p->in_normf(out, in); 						/* Normalize from input color space */
	rv |= lut->lookup_input(lut,out,out);		/* Lookup though input tables */
	p->in_denormf(out,out);						/* De-normalize to input color space */
	return rv;
}

/* Do input'->output' lookup */
static int icmLuLut_clut(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	double temp[MAX_CHAN];
	int rv = 0;

	p->in_normf(temp, in); 						/* Normalize from input color space */
	rv |= p->lookup_clut(lut,out,temp);			/* Lookup though clut tables */
	p->out_denormf(out,out);					/* De-normalize to output color space */
	return rv;
}

/* Do output'->output lookup */
static int icmLuLut_output(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	p->out_normf(out,in);						/* Normalize from output color space */
	rv |= lut->lookup_output(lut,out,out);		/* Lookup though output tables */
	p->out_denormf(out, out);					/* De-normalize to output color space */
	return rv;
}

static int icmLuLut_out_abs(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	DBLLL(("icm out_abs: input %s\n",icmPdv(lut->outputChan, in)));
	if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->outputChan; i++)		/* Don't alter input values */
			out[i] = in[i];
	}

	/* If Fwd Lut, take care of Absolute color space */
	/* and convert from native to effective out PCS */
	if ((p->function == icmFwd || p->function == icmPreview)
		&& (p->outSpace == icSigLabData
		 || p->outSpace == icSigXYZData)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation)) {

		if (p->outSpace == icSigLabData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm out_abs: after Lab2XYZ %s\n",icmPdv(lut->outputChan, out)));
		}
		
		/* Convert from Relative to Absolute colorimetric XYZ */
		icmMulBy3x3(out, p->toAbs, out);
		DBLLL(("icm out_abs: after toAbs %s\n",icmPdv(lut->outputChan, out)));

		if (p->e_outSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm out_abs: after XYZ2Lab %s\n",icmPdv(lut->outputChan, out)));
		}
	} else {

		/* Convert from Native to Effective output space */
		if (p->outSpace == icSigLabData && p->e_outSpace == icSigXYZData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm out_abs: after Lab2 %s\n",icmPdv(lut->outputChan, out)));
		} else if (p->outSpace == icSigXYZData && p->e_outSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm out_abs: after XYZ2Lab %s\n",icmPdv(lut->outputChan, out)));
		}
	}
	DBLLL(("icm out_abs: returning %s\n",icmPdv(lut->outputChan, out)));
	return rv;
}


/* Overall lookup */
static int
icmLuLut_lookup (
icmLuLut *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLut *lut = p->lut;
	double temp[MAX_CHAN];

	DBGLL(("icmLuLut_lookup: in = %s\n", icmPdv(p->lut->inputChan, in)));
	rv |= p->in_abs(p,temp,in);						/* Possible absolute conversion */
	DBGLL(("icmLuLut_lookup: in_abs = %s\n", icmPdv(p->lut->inputChan, temp)));
	if (p->usematrix) {
		rv |= lut->lookup_matrix(lut,temp,temp);/* If XYZ, multiply by non-unity matrix */
		DBGLL(("icmLuLut_lookup: matrix = %s\n", icmPdv(p->lut->inputChan, temp)));
	}
	p->in_normf(temp, temp);					/* Normalize for input color space */
	DBGLL(("icmLuLut_lookup: norm = %s\n", icmPdv(p->lut->inputChan, temp)));
	rv |= lut->lookup_input(lut,temp,temp);		/* Lookup though input tables */
	DBGLL(("icmLuLut_lookup: input = %s\n", icmPdv(p->lut->inputChan, temp)));
	rv |= p->lookup_clut(lut,out,temp);			/* Lookup though clut tables */
	DBGLL(("icmLuLut_lookup: clut = %s\n", icmPdv(p->lut->outputChan, out)));
	rv |= lut->lookup_output(lut,out,out);		/* Lookup though output tables */
	DBGLL(("icmLuLut_lookup: output = %s\n", icmPdv(p->lut->outputChan, out)));
	p->out_denormf(out,out);					/* Normalize for output color space */
	DBGLL(("icmLuLut_lookup: denorm = %s\n", icmPdv(p->lut->outputChan, out)));
	rv |= p->out_abs(p,out,out);				/* Possible absolute conversion */
	DBGLL(("icmLuLut_lookup: out_abse = %s\n", icmPdv(p->lut->outputChan, out)));

	return rv;
}

#ifdef NEVER	/* The following should be identical in effect to the above. */

/* Overall lookup */
static int
icmLuLut_lookup (
icmLuLut *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int i, rv = 0;
	icmLut *lut = p->lut;
	double temp[MAX_CHAN];

	rv |= p->in_abs(p,temp,in);
	rv |= p->matrix(p,temp,temp);
	rv |= p->input(p,temp,temp);
	rv |= p->clut(p,out,temp);
	rv |= p->output(p,out,out);
	rv |= p->out_abs(p,out,out);

	return rv;
}

#endif	/* NEVER */

/* Three stage conversion */
static int
icmLuLut_lookup_in (
icmLuLut *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLut *lut = p->lut;

	/* If in_abs() or matrix() are active, then we can't have a per component input curve */
	if (((p->function == icmBwd || p->function == icmGamut || p->function == icmPreview)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation))
	 || (p->e_inSpace != p->inSpace)
	 || (p->usematrix)) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)
			out[i] = in[i];
	} else {
		rv |= p->input(p,out,in);
	}
	return rv;
}

static int
icmLuLut_lookup_core (
icmLuLut *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;

	/* If in_abs() or matrix() are active, then we have to do the per component input curve here */
	if (((p->function == icmBwd || p->function == icmGamut || p->function == icmPreview)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation))
	 || (p->e_inSpace != p->inSpace)
	 || (p->usematrix)) {
		double temp[MAX_CHAN];
		rv |= p->in_abs(p,temp,in);
		rv |= p->matrix(p,temp,temp);
		rv |= p->input(p,temp,temp);
		rv |= p->clut(p,out,temp);
	} else {
		rv |= p->clut(p,out,in);
	}

	/* If out_abs() is active, then we can't have do per component out curve here */
	if (((p->function == icmFwd || p->function == icmPreview)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation))
	 || (p->outSpace != p->e_outSpace)) {
		rv |= p->output(p,out,out);
		rv |= p->out_abs(p,out,out);
	}

	return rv;
}

static int
icmLuLut_lookup_out (
icmLuLut *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLut *lut = p->lut;

	/* If out_abs() is active, then we can't have a per component out curve */
	if (((p->function == icmFwd || p->function == icmPreview)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation))
	 || (p->outSpace != p->e_outSpace)) {
		unsigned int i;
		for (i = 0; i < lut->outputChan; i++)
			out[i] = in[i];
	} else {
		rv |= p->output(p,out,in);
	}

	return rv;
}

/* Inverse three stage conversion (partly implemented) */
static int
icmLuLut_lookup_inv_in (
icmLuLut *p,
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLut *lut = p->lut;

	/* If in_abs() or matrix() are active, then we can't have a per component input curve */
	if (((p->function == icmBwd || p->function == icmGamut || p->function == icmPreview)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation))
	 || (p->e_inSpace != p->inSpace)
	 || (p->usematrix)) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)
			out[i] = in[i];
	} else {
		rv |= p->inv_input(p,out,in);
	}
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Some components of inverse lookup, in order */
/* ~~ should these be in icmLut (like all the fwd transforms)? */

static int icmLuLut_inv_out_abs(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	DBLLL(("icm inv_out_abs: input %s\n",icmPdv(lut->outputChan, in)));
	if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->outputChan; i++)		/* Don't alter input values */
			out[i] = in[i];
	}

	/* If Fwd Lut, take care of Absolute color space */
	/* and convert from effective to native inverse output PCS */
	/* OutSpace must be PCS: XYZ or Lab */
	if ((p->function == icmFwd || p->function == icmPreview)
		&& (p->e_outSpace == icSigLabData
		 || p->e_outSpace == icSigXYZData)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation)) {

		if (p->e_outSpace == icSigLabData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm inv_out_abs: after Lab2XYZ %s\n",icmPdv(lut->outputChan, out)));
		}
	
		/* Convert from Absolute to Relative colorimetric */
		icmMulBy3x3(out, p->fromAbs, out);
		DBLLL(("icm inv_out_abs: after fromAbs %s\n",icmPdv(lut->outputChan, out)));
		
		if (p->outSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm inv_out_abs: after XYZ2Lab %s\n",icmPdv(lut->outputChan, out)));
		}

	} else {

		/* Convert from Effective to Native output space */
		if (p->e_outSpace == icSigLabData && p->outSpace == icSigXYZData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm inv_out_abs: after Lab2XYZ %s\n",icmPdv(lut->outputChan, out)));
		} else if (p->e_outSpace == icSigXYZData && p->outSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm inv_out_abs: after XYZ2Lab %s\n",icmPdv(lut->outputChan, out)));
		}
	}
	return rv;
}

/* Do output->output' inverse lookup */
static int icmLuLut_inv_output(icmLuLut *p, double *out, double *in) {
	icc *icp = p->icp;
	icmLut *lut = p->lut;
	int i;
	int rv = 0;

	if (lut->rot[0].inited == 0) {	
		for (i = 0; i < lut->outputChan; i++) {
			rv = icmTable_setup_bwd(icp, &lut->rot[i], lut->outputEnt,
			                             lut->outputTable + i * lut->outputEnt);
			if (rv != 0) {
				return icm_err(icp, rv,"icc_Lut_inv_input: Malloc failure in inverse lookup init.");
			}
		}
	}

	p->out_normf(out,in);						/* Normalize from output color space */
	for (i = 0; i < lut->outputChan; i++) {
		/* Reverse lookup though output tables */
		rv |= icmTable_lookup_bwd(&lut->rot[i], &out[i], &out[i]);
	}
	p->out_denormf(out, out);					/* De-normalize to output color space */
	return rv;
}

/* No output' -> input inverse lookup. */
/* This is non-trivial ! */

/* Do input' -> input inverse lookup */
static int icmLuLut_inv_input(icmLuLut *p, double *out, double *in) {
	icc *icp = p->icp;
	icmLut *lut = p->lut;
	int i;
	int rv = 0;

	if (lut->rit[0].inited == 0) {	
		for (i = 0; i < lut->inputChan; i++) {
			rv = icmTable_setup_bwd(icp, &lut->rit[i], lut->inputEnt,
			                             lut->inputTable + i * lut->inputEnt);
			if (rv != 0) {
				return icm_err(icp, rv,"icc_Lut_inv_input: Malloc failure in inverse lookup init.");
			}
		}
	}

	p->in_normf(out, in); 						/* Normalize from input color space */
	for (i = 0; i < lut->inputChan; i++) {
		/* Reverse lookup though input tables */
		rv |= icmTable_lookup_bwd(&lut->rit[i], &out[i], &out[i]);
	}
	p->in_denormf(out,out);						/* De-normalize to input color space */
	return rv;
}

/* Possible inverse matrix lookup */
static int icmLuLut_inv_matrix(icmLuLut *p, double *out, double *in) {
	icc *icp = p->icp;
	icmLut *lut = p->lut;
	int rv = 0;

	if (p->usematrix) {
		double tt[3];
		if (p->imx_valid == 0) {
			if (icmInverse3x3(p->imx, lut->e) != 0) {	/* Compute inverse */
				return icm_err(icp, 2,"icc_new_iccLuMatrix: Matrix wasn't invertable");
			}
			p->imx_valid = 1;
		}
		/* Matrix multiply */
		tt[0] = p->imx[0][0] * in[0] + p->imx[0][1] * in[1] + p->imx[0][2] * in[2];
		tt[1] = p->imx[1][0] * in[0] + p->imx[1][1] * in[1] + p->imx[1][2] * in[2];
		tt[2] = p->imx[2][0] * in[0] + p->imx[2][1] * in[1] + p->imx[2][2] * in[2];
		out[0] = tt[0], out[1] = tt[1], out[2] = tt[2];
	} else if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)
			out[i] = in[i];
	}
	return rv;
}

static int icmLuLut_inv_in_abs(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	DBLLL(("icm inv_in_abs: input %s\n",icmPdv(lut->inputChan, in)));
	if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)		/* Don't alter input values */
			out[i] = in[i];
	}

	/* If Bwd Lut, take care of Absolute color space, and */
	/* convert from native to effective input space */
	if ((p->function == icmBwd || p->function == icmGamut || p->function == icmPreview)
		&& (p->inSpace == icSigLabData
		 || p->inSpace == icSigXYZData)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation)) {

		if (p->inSpace == icSigLabData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm inv_in_abs: after Lab2XYZ %s\n",icmPdv(lut->inputChan, out)));
		}

		/* Convert from Relative to Absolute colorimetric XYZ */
		icmMulBy3x3(out, p->toAbs, out);
		DBLLL(("icm inv_in_abs: after toAbs %s\n",icmPdv(lut->inputChan, out)));
		
		if (p->e_inSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm inv_in_abs: after XYZ2Lab %s\n",icmPdv(lut->inputChan, out)));
		}
	} else {

		/* Convert from Native to Effective input space */
		if (p->inSpace == icSigLabData && p->e_inSpace == icSigXYZData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm inv_in_abs: after Lab2XYZ %s\n",icmPdv(lut->inputChan, out)));
		} else if (p->inSpace == icSigXYZData && p->e_inSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm inv_in_abs: after XYZ2Lab %s\n",icmPdv(lut->inputChan, out)));
		}
	}
	DBLLL(("icm inv_in_abs: returning %s\n",icmPdv(lut->inputChan, out)));
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Return LuLut information */
static void icmLuLut_get_info(
	icmLuLut     *p,		/* this */
	icmLut       **lutp,	/* Pointer to icc lut type */
	icmXYZNumber *pcswhtp,	/* Pointer to profile PCS white point */
	icmXYZNumber *whitep,	/* Pointer to profile absolute white point */
	icmXYZNumber *blackp	/* Pointer to profile absolute black point */
) {
	if (lutp != NULL)
		*lutp = p->lut;
	if (pcswhtp != NULL)
		*pcswhtp = p->pcswht;
	if (whitep != NULL)
		*whitep = p->whitePoint;
	if (blackp != NULL)
		*blackp = p->blackPoint;
}

/* Get the native ranges for the LuLut */
/* This is computed differently to the mono & matrix types, to */
/* accurately take into account the different range for 8 bit Lab */
/* lut type. The range returned for the effective PCS is not so accurate. */
static void
icmLuLut_get_lutranges (
	icmLuLut *p,
	double *inmin, double *inmax,		/* Return maximum range of inspace values */
	double *outmin, double *outmax		/* Return maximum range of outspace values */
) {
	unsigned int i;

	for (i = 0; i < p->lut->inputChan; i++) {
		inmin[i] = 0.0;	/* Normalized range of input space values */
		inmax[i] = 1.0;
	}
	p->in_denormf(inmin,inmin);	/* Convert to real colorspace range */
	p->in_denormf(inmax,inmax);

	/* Make sure min and max are so. */
	for (i = 0; i < p->lut->inputChan; i++) {
		if (inmin[i] > inmax[i]) {
			double tt;
			tt = inmin[i];
			inmin[i] = inmax[i];
			inmax[i] = tt;
		}
	}

	for (i = 0; i < p->lut->outputChan; i++) {
		outmin[i] = 0.0;	/* Normalized range of output space values */
		outmax[i] = 1.0;
	}
	p->out_denormf(outmin,outmin);	/* Convert to real colorspace range */
	p->out_denormf(outmax,outmax);

	/* Make sure min and max are so. */
	for (i = 0; i < p->lut->outputChan; i++) {
		if (outmin[i] > outmax[i]) {
			double tt;
			tt = outmin[i];
			outmin[i] = outmax[i];
			outmax[i] = tt;
		}
	}
}

/* Get the effective (externaly visible) ranges for the LuLut */
/* This will be accurate if there is no override, but only */
/* aproximate if a PCS override is in place. */
static void
icmLuLut_get_ranges (
	icmLuLut *p,
	double *inmin, double *inmax,		/* Return maximum range of inspace values */
	double *outmin, double *outmax		/* Return maximum range of outspace values */
) {

	/* Get the native ranges first */
	icmLuLut_get_lutranges(p, inmin, inmax, outmin, outmax);

	/* And replace them if the effective space is different */
	if (p->e_inSpace != p->inSpace)
		getRange(p->icp, p->e_inSpace, p->lut->ttype, inmin, inmax);

	if (p->e_outSpace != p->outSpace)
		getRange(p->icp, p->e_outSpace, p->lut->ttype, outmin, outmax);
}

/* Return the underlying Lut matrix */
static void
icmLuLut_get_matrix (
	icmLuLut *p,
	double m[3][3]
) {
	int i, j;
	icmLut *lut = p->lut;

	if (p->usematrix) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				m[i][j] = lut->e[i][j];	/* Copy from Lut */

	} else {							/* return unity matrix */
		icmSetUnity3x3(m);
	}
}


static void
icmLuLut_delete(
	icmLuLut *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

icmLuBase *
icc_new_icmLuLut(
	icc                   *icp,
	icTagSignature        ttag,			/* Target Lut tag */
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS (from header) */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent (For absolute) */
	icmLookupFunc         func			/* Functionality requested (for icmLuSpaces()) */
) {
	icmLuLut *p;

	if ((p = (icmLuLut *) icp->al->calloc(icp->al,1,sizeof(icmLuLut))) == NULL)
		return NULL;
	p->ttype    = icmLutType;
	p->icp      = icp;
	p->del      = icmLuLut_delete;

	p->lutspaces= LUTSPACES(icmLuLut) icmLutSpaces;
	p->spaces   = LUSPACES(icmLuLut) icmLuSpaces;
	p->XYZ_Rel2Abs = LUXYZ_REL2ABS(icmLuLut) icmLuXYZ_Rel2Abs;
	p->XYZ_Abs2Rel = LUXYZ_ABS2REL(icmLuLut) icmLuXYZ_Abs2Rel;
	p->init_wh_bk  = LUINIT_WH_BK(icmLuLut) icmLuInit_Wh_bk;
	p->wh_bk_points = LUWH_BK_POINTS(icmLuLut) icmLuWh_bk_points;
	p->lu_wh_bk_points = LULU_WH_BK_POINTS(icmLuLut) icmLuLu_wh_bk_points;

	p->lookup        = icmLuLut_lookup;
	p->lookup_in     = icmLuLut_lookup_in;
	p->lookup_core   = icmLuLut_lookup_core;
	p->lookup_out    = icmLuLut_lookup_out;
	p->lookup_inv_in = icmLuLut_lookup_inv_in;

	p->in_abs   = icmLuLut_in_abs;
	p->matrix   = icmLuLut_matrix;
	p->input    = icmLuLut_input;
	p->clut     = icmLuLut_clut;
	p->output   = icmLuLut_output;
	p->out_abs  = icmLuLut_out_abs;

	p->inv_in_abs   = icmLuLut_inv_in_abs;
	p->inv_matrix   = icmLuLut_inv_matrix;
	p->inv_input    = icmLuLut_inv_input;
	p->inv_output   = icmLuLut_inv_output;
	p->inv_out_abs  = icmLuLut_inv_out_abs;

	p->pcswht   = icp->header->illuminant;
	p->intent   = intent;			/* used to trigger absolute processing */
	p->function = func;
	p->inSpace  = inSpace;
	p->outSpace = outSpace;
	p->pcs      = pcs;
	p->e_inSpace  = e_inSpace;
	p->e_outSpace = e_outSpace;
	p->e_pcs      = e_pcs;
	p->get_info = icmLuLut_get_info;
	p->get_lutranges = icmLuLut_get_lutranges;
	p->get_ranges = icmLuLut_get_ranges;
	p->get_matrix = icmLuLut_get_matrix;

	/* Lookup the white and black points */
	if (p->init_wh_bk(p)) {
		p->del(p);
		return NULL;
	}

	/* Get the Lut tag, & check that it is expected type */
	if ((p->lut = (icmLut *)icp->read_tag(icp, ttag)) == NULL
	 || (p->lut->ttype != icSigLut8Type && p->lut->ttype != icSigLut16Type)) {
		p->del(p);
		return NULL;
	}

	/* Check if matrix should be used */
	if (inSpace == icSigXYZData && p->lut->nu_matrix(p->lut))
		p->usematrix = 1;
	else
		p->usematrix = 0;

	{
		icmGNRep irep = icmTagType2GNRep(inSpace, p->lut->ttype);		/* 8/16 bit encoding */
		icmGNRep orep = icmTagType2GNRep(outSpace, p->lut->ttype);
		icmGNRep eirep = icmTagType2GNRep(e_inSpace, p->lut->ttype);		/* 8/16 bit encoding */
		icmGNRep eorep = icmTagType2GNRep(e_outSpace, p->lut->ttype);

		/* Lookup input color space to normalized index function */
		if (icmGetNormFunc(&p->in_normf, NULL, inSpace, icmGNtoNorm, icmGNV2, irep) != 0) { 
			p->del(p);
			icm_err(icp, 1,"icc_get_luobj: Unknown colorspace");
			return NULL;
		}

		/* Lookup normalized index to input color space function */
		if (icmGetNormFunc(&p->in_denormf, NULL, inSpace, icmGNtoCS, icmGNV2, irep) != 0) { 
			p->del(p);
			icm_err(icp, 1,"icc_get_luobj: Unknown colorspace");
			return NULL;
		}

		/* Lookup output color space to normalized Lut entry value function */
		if (icmGetNormFunc(&p->out_normf, NULL, outSpace, icmGNtoNorm, icmGNV2, orep) != 0) { 
			p->del(p);
			icm_err(icp, 1,"icc_get_luobj: Unknown colorspace");
			return NULL;
		}

		/* Lookup normalized Lut entry value to output color space function */
		if (icmGetNormFunc(&p->out_denormf, NULL, outSpace, icmGNtoCS, icmGNV2, orep) != 0) { 
			p->del(p);
			icm_err(icp, 1,"icc_get_luobj: Unknown colorspace");
			return NULL;
		}


		/* Note that the following two are only used in computing the expected */
		/* value ranges of the effective PCS. This might not be the best way of */
		/* doing this.... */

		/* Lookup normalized index to effective input color space function */
		if (icmGetNormFunc(&p->e_in_denormf, NULL, e_inSpace, icmGNtoCS, icmGNV2, eirep) != 0) { 
			p->del(p);
			icm_err(icp, 1,"icc_get_luobj: Unknown effective colorspace");
			return NULL;
		}

		/* Lookup normalized Lut entry value to effective output color space function */
		if (icmGetNormFunc(&p->e_out_denormf, NULL, e_outSpace, icmGNtoCS, icmGNV2, eorep) != 0) { 
			p->del(p);
			icm_err(icp, 1,"icc_get_luobj: Unknown effective colorspace");
			return NULL;
		}
	}

	/* Determine appropriate clut lookup algorithm */
	{
		int use_sx;				/* -1 = undecided, 0 = N-linear, 1 = Simplex lookup */
		icColorSpaceSignature ins, outs;	/* In and out Lut color spaces */
		int inn, outn;		/* in and out number of Lut components */

		p->lutspaces(p, &ins, &inn, &outs, &outn, NULL);

		/* Determine if the input space is "Device" like, */
		/* ie. luminance will be expected to vary most strongly */
		/* with the diagonal change in input coordinates. */
		switch(ins) {

			/* Luminence is carried by the sum of all the output channels, */
			/* so output luminence will dominantly be in diagonal direction. */
			case icSigXYZData:		/* This seems to be appropriate ? */
			case icSigRgbData:
			case icSigGrayData:
			case icSigCmykData:
			case icSigCmyData:
			case icSigMch6Data:
				use_sx = 1;		/* Simplex interpolation is appropriate */
				break;

			/* A single channel carries the luminence information */
			case icSigLabData:
			case icSigLuvData:
			case icSigYCbCrData:
			case icSigYxyData:
			case icSigHlsData:
			case icSigHsvData:
				use_sx = 0;		/* N-linear interpolation is appropriate */
				break;
			default:
				use_sx = -1;		/* undecided */
			    	break;
		}

		/* If we couldn't figure it out from the input space, */
		/* check output luminance variation with a diagonal input */
		/* change. */
		if (use_sx == -1) {
			int lc;		/* Luminance channel */

			/* Determine where the luminence is carried in the output */
			switch(outs) {

				/* Luminence is carried by the sum of all the output channels */
				case icSigRgbData:
				case icSigGrayData:
				case icSigCmykData:
				case icSigCmyData:
				case icSigMch6Data:
					lc = -1;		/* Average all channels */
					break;

				/* A single channel carries the luminence information */
				case icSigLabData:
				case icSigLuvData:
				case icSigYCbCrData:
				case icSigYxyData:
					lc = 0;
					break;

				case icSigXYZData:
				case icSigHlsData:
					lc = 1;
					break;

				case icSigHsvData:
					lc = 2;
					break;
				
				/* default means give up and use N-linear type lookup */
				default:
					lc = -2;
					break;
			}

			/* If we know how luminance is represented in output space */
			if (lc != -2) {
				double tout1[MAX_CHAN];		/* Test output values */
				double tout2[MAX_CHAN];
				double tt, diag;
				int n;

				/* Determine input space location of min and max of */
				/* given output channel (chan = -1 means average of all) */
				p->lut->min_max(p->lut, tout1, tout2, lc);
				
				/* Convert to vector and then calculate normalized */
				/* dot product with diagonal vector (1,1,1...) */
				for (tt = 0.0, n = 0; n < inn; n++) {
					tout1[n] = tout2[n] - tout1[n];
					tt += tout1[n] * tout1[n];
				}
				if (tt > 0.0)
					tt = sqrt(tt);			/* normalizing factor for maximum delta */
				else
					tt = 1.0;				/* Hmm. */
				tt *= sqrt((double)inn);	/* Normalizing factor for diagonal vector */
				for (diag = 0.0, n = 0; n < outn; n++)
					diag += tout1[n] / tt;
				diag = fabs(diag);

				/* I'm not really convinced that this is a reliable */
				/* indicator of whether simplex interpolation should be used ... */
				/* It does seem to do the right thing with YCC space though. */
				if (diag > 0.8)	/* Diagonal is dominant ? */
					use_sx = 1;

				/* If we couldn't figure it out, use N-linear interpolation */
				if (use_sx == -1)
					use_sx = 0;
			}
		}

		if (use_sx) {
			p->lookup_clut = p->lut->lookup_clut_sx;
			p->lut->tune_value = icmLut_tune_value_sx;
		} else {
			p->lookup_clut = p->lut->lookup_clut_nl;
			p->lut->tune_value = icmLut_tune_value_nl;
		}
	}
	return (icmLuBase *)p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - */

/* Return an appropriate lookup object */
/* Return NULL on error, and detailed error in icc */
static icmLuBase* icc_get_luobj (
	icc *p,						/* ICC */
	icmLookupFunc func,			/* Conversion functionality */
	icRenderingIntent intent,	/* Rendering intent, including icmAbsoluteColorimetricXYZ */
	icColorSpaceSignature pcsor,/* PCS override (0 = def) */
	icmLookupOrder order		/* Conversion representation search Order */
) {
	icmLuBase *luobj = NULL;	/* Lookup object to return */
	icColorSpaceSignature pcs, e_pcs;	/* PCS and effective PCS */
	
	/* Figure out the native and effective PCS */
	e_pcs = pcs = p->header->pcs;
	if (pcsor != icmSigDefaultData)
		e_pcs = pcsor;			/* Override */

	/* How we expect to execute the request depends firstly on the type of profile */
	switch (p->header->deviceClass) {
    	case icSigInputClass:
    	case icSigDisplayClass:
    	case icSigColorSpaceClass:

			/* Look for Intent based AToBX profile + optional BToAX reverse */
			/* or for AToB0 based profile + optional BToA0 reverse */
			/* or three component matrix profile (reversable) */
			/* or momochrome table profile (reversable) */ 
			/* No Lut intent for ICC < V2.4, but possible for >= V2.4, */
			/* so fall back if we can't find the chosen Lut intent.. */
			/* Device <-> PCS */
			/* Determine the algorithm and set its parameters */

			switch (func) {
				icRenderingIntent fbintent;		/* Fallback intent */
				icTagSignature ttag, fbtag;

		    	case icmFwd:	/* Device to PCS */
					if (intent == icmDefaultIntent)
						intent = icPerceptual;	/* Make this the default */

					switch ((int)intent) {
		    			case icAbsoluteColorimetric:
							ttag = icSigAToB1Tag;
							fbtag = icSigAToB0Tag;
							fbintent = intent;
							break;
		    			case icRelativeColorimetric:
							ttag = icSigAToB1Tag;
							fbtag = icSigAToB0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icPerceptual:
							ttag = icSigAToB0Tag;
							fbtag = icSigAToB0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icSaturation:
							ttag = icSigAToB2Tag;
							fbtag = icSigAToB0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
							ttag = icSigAToB0Tag;
							fbtag = icSigAToB0Tag;
							fbintent = intent;
							break;
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							ttag = icSigAToB2Tag;
							fbtag = icSigAToB0Tag;
							fbintent = intent;
							break;
						default:
							icm_err(p, 1,"icc_get_luobj: Unknown intent");
							return NULL;
					}

					if (order != icmLuOrdRev) {
						/* Try Lut type lookup with the chosen intent first */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try the fallback tag */
						if ((luobj = icc_new_icmLuLut(p, fbtag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     fbintent, func)) != NULL)
							break;
	
						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
					} else {
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;

						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try Lut type lookup last */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try the fallback tag */
						if ((luobj = icc_new_icmLuLut(p, fbtag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     fbintent, func)) != NULL)
							break;
					}
					break;

		    	case icmBwd:	/* PCS to Device */
					if (intent == icmDefaultIntent)
						intent = icPerceptual;	/* Make this the default */

					switch ((int)intent) {
		    			case icAbsoluteColorimetric:
							ttag = icSigBToA1Tag;
							fbtag = icSigBToA0Tag;
							fbintent = intent;
							break;
		    			case icRelativeColorimetric:
							ttag = icSigBToA1Tag;
							fbtag = icSigBToA0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icPerceptual:
							ttag = icSigBToA0Tag;
							fbtag = icSigBToA0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icSaturation:
							ttag = icSigBToA2Tag;
							fbtag = icSigBToA0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
							ttag = icSigBToA0Tag;
							fbtag = icSigBToA0Tag;
							fbintent = intent;
							break;
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							ttag = icSigBToA2Tag;
							fbtag = icSigBToA0Tag;
							fbintent = intent;
							break;
						default:
							icm_err(p, 1,"icc_get_luobj: Unknown intent");
							return NULL;
					}


					if (order != icmLuOrdRev) {
						/* Try Lut type lookup first */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;

						/* Try the fallback Lut */
						if ((luobj = icc_new_icmLuLut(p, fbtag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     fbintent, func)) != NULL)
							break;
	
						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
					} else {
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try Lut type lookup last */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;

						/* Try the fallback Lut */
						if ((luobj = icc_new_icmLuLut(p, fbtag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     fbintent, func)) != NULL)
							break;
					}
					break;

				default:
					icm_err(p, 1,"icc_get_luobj: Inaproptiate function requested");
					return NULL;
				}
			break;
			
    	case icSigOutputClass:
			/* Expect BToA Lut and optional AToB Lut, All intents, expect gamut */
			/* or momochrome table profile (reversable) */ 
			/* Device <-> PCS */
			/* Gamut Lut - no intent */
			/* Optional preview links PCS <-> PCS */
			
			/* Determine the algorithm and set its parameters */
			switch (func) {
				icTagSignature ttag;
				
		    	case icmFwd:	/* Device to PCS */

					if (intent == icmDefaultIntent)
						intent = icPerceptual;	/* Make this the default */

					switch ((int)intent) {
		    			case icRelativeColorimetric:
		    			case icAbsoluteColorimetric:
								ttag = icSigAToB1Tag;
							break;
		    			case icPerceptual:
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
								ttag = icSigAToB0Tag;
							break;
		    			case icSaturation:
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
								ttag = icSigAToB2Tag;
							break;
						default:
							icm_err(p, 1,"icc_get_luobj: Unknown intent");
							return NULL;
					}

					if (order != icmLuOrdRev) {
						/* Try Lut type lookup first */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL) {
							break;
						}
	
						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
					} else {
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;

						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try Lut type lookup last */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
					}
					break;

		    	case icmBwd:	/* PCS to Device */

					if (intent == icmDefaultIntent)
						intent = icPerceptual;			/* Make this the default */

					switch ((int)intent) {
		    			case icRelativeColorimetric:
		    			case icAbsoluteColorimetric:
								ttag = icSigBToA1Tag;
							break;
		    			case icPerceptual:
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
								ttag = icSigBToA0Tag;
							break;
		    			case icSaturation:
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
								ttag = icSigBToA2Tag;
							break;
						default:
							icm_err(p, 1,"icc_get_luobj: Unknown intent");
							return NULL;
					}

					if (order != icmLuOrdRev) {
						/* Try Lut type lookup first */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
					} else {
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;

						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try Lut type lookup last */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
					}
					break;

		    	case icmGamut:	/* PCS to 1D */

					if ((p->cflags & icmCFlagAllowQuirks) == 0
					 && (p->cflags & icmCFlagAllowExtensions) == 0) {

						/* Allow only default and absolute */
						if (intent != icmDefaultIntent
						 && intent != icAbsoluteColorimetric) {
							icm_err(p, ICM_TR_GAMUT_INTENT,"icc_get_luobj: Intent (0x%x)is unexpected for Gamut table",intent);
							return NULL;
						}
					} else {
						int warn = 0;

						/* Be more forgiving */
						switch ((int)intent) {
			    			case icAbsoluteColorimetric:
								break;
			    			case icmAbsolutePerceptual:		/* Special icclib intent */
			    			case icmAbsoluteSaturation:		/* Special icclib intent */
								warn = 1;
								break;
							case icmDefaultIntent:
								break;
			    			case icRelativeColorimetric:
			    			case icPerceptual:
			    			case icSaturation:
								intent = icmDefaultIntent;	/* Make all other look like default */
								warn = 1;
								break;
							default:
								icm_err(p, ICM_TR_GAMUT_INTENT,"icc_get_luobj: Unknown intent (0x%x)",intent);
								return NULL;
						}
						if (warn) { 
							/* (icmQuirkWarning noset doesn't care about p->op direction) */
							icmQuirkWarning(p, ICM_TR_GAMUT_INTENT, 1, "icc_get_luobj: Intent (0x%x)is unexpected for Gamut table",intent);
						}
					}

					/* If the target tag exists, and it is a Lut */
					luobj = icc_new_icmLuLut(p, icSigGamutTag,
					     pcs, icSigGrayData, pcs,
					     e_pcs, icSigGrayData, e_pcs,
					     intent, func);
					break;

		    	case icmPreview:	/* PCS to PCS */

					switch ((int)intent)  {
		    			case icRelativeColorimetric:
								ttag = icSigPreview1Tag;
							break;
		    			case icPerceptual:
								ttag = icSigPreview0Tag;
							break;
		    			case icSaturation:
								ttag = icSigPreview2Tag;
							break;
		    			case icAbsoluteColorimetric:
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							icm_err(p, 1,"icc_get_luobj: Intent is inappropriate for preview table");
							return NULL;
						default:
							icm_err(p, 1,"icc_get_luobj: Unknown intent");
							return NULL;
					}

					/* If the target tag exists, and it is a Lut */
					luobj = icc_new_icmLuLut(p, ttag,
					     pcs, pcs, pcs,
					     e_pcs, e_pcs, e_pcs,
					     intent, func);
					break;

				default:
					icm_err(p, 1,"icc_get_luobj: Inaproptiate function requested");
					return NULL;
			}
			break;

    	case icSigLinkClass:
			/* Expect AToB0 Lut and optional BToA0 Lut, One intent in header */
			/* Device <-> Device */

			if (intent != p->header->renderingIntent
			 && intent != icmDefaultIntent) {
				icm_err(p, 1,"icc_get_luobj: Intent is inappropriate for Link profile");
				return NULL;
			}
			intent = p->header->renderingIntent;

			/* Determine the algorithm and set its parameters */
			switch (func) {
		    	case icmFwd:	/* Device to PCS (== Device) */

					luobj = icc_new_icmLuLut(p, icSigAToB0Tag,
					     p->header->colorSpace, pcs, pcs,
					     p->header->colorSpace, pcs, pcs,
					     intent, func);
					break;

		    	case icmBwd:	/* PCS (== Device) to Device */

					luobj = icc_new_icmLuLut(p, icSigBToA0Tag,
					     pcs, p->header->colorSpace, pcs,
					     pcs, p->header->colorSpace, pcs,
					     intent, func);
					break;

				default:
					icm_err(p, 1,"icc_get_luobj: Inaproptiate function requested");
					return NULL;
			}
			break;

    	case icSigAbstractClass:
			/* Expect AToB0 Lut and option BToA0 Lut, with either relative or absolute intent. */
			/* PCS <-> PCS */
			/* Determine the algorithm and set its parameters */

			if (intent != icmDefaultIntent
			 && intent != icRelativeColorimetric
			 && intent != icAbsoluteColorimetric) {
				icm_err(p, 1,"icc_get_luobj: Intent is inappropriate for Abstract profile");
				return NULL;
			}

			switch (func) {
		    	case icmFwd:	/* PCS (== Device) to PCS */

					luobj = icc_new_icmLuLut(p, icSigAToB0Tag,
					     p->header->colorSpace, pcs, pcs,
					     e_pcs, e_pcs, e_pcs,
					     intent, func);
					break;

		    	case icmBwd:	/* PCS to PCS (== Device) */

					luobj = icc_new_icmLuLut(p, icSigBToA0Tag,
					     pcs, p->header->colorSpace, pcs,
					     e_pcs, e_pcs, e_pcs,
					     intent, func);
					break;

				default:
					icm_err(p, 1,"icc_get_luobj: Inaproptiate function requested");
					return NULL;
			}
			break;

    	case icSigNamedColorClass:
			/* Expect Name -> Device, Optional PCS */
			/* and a reverse lookup would be useful */
			/* (ie. PCS or Device coords to closest named color) */
			/* ~~ to be implemented ~~ */

			/* ~~ Absolute intent is valid for processing of */
			/* PCS from named Colors. Also allow for e_pcs */
			if (intent != icmDefaultIntent
			 && intent != icRelativeColorimetric
			 && intent != icAbsoluteColorimetric) {
				icm_err(p, 1,"icc_get_luobj: Intent is inappropriate for Named Color profile");
				return NULL;
			}

			icm_err(p, 1,"icc_get_luobj: Named Colors not handled yet");
			return NULL;

    	default:
			icm_err(p, 1,"icc_get_luobj: Unknown profile class");
			return NULL;
	}

	if (luobj == NULL) {
		icm_err(p, 1,"icc_get_luobj: Unable to locate usable conversion");
		return NULL;
	} else {
		luobj->order = order;
	}

	return luobj;
}

/* - - - - - - - - - - - - - - - - - - - - - - - */

/* Returns total ink limit and channel maximums. */
/* Returns -1.0 if not applicable for this type of profile. */
/* Returns -1.0 for grey, additive, or any profiles < 4 channels. */
/* This is a place holder that uses a heuristic, */
/* until there is a private or standard tag for this information */
static double icm_get_tac(		/* return TAC */
icc *p,
double *chmax,					/* device return channel sums. May be NULL */
void (*calfunc)(void *cntx, double *out, double *in),	/* Optional calibration func. */
void *cntx
) {
	icmHeader *rh = p->header;
	icmLuBase *luo;
	icmLuLut *ll;
	icmLut *lut;
	icColorSpaceSignature outs;		/* Type of output space */
	int inn, outn;					/* Number of components */
	icmLuAlgType alg;				/* Type of lookup algorithm */
	double tac = 0.0;
	double max[MAX_CHAN];			/* Channel maximums */
	int i, f;
	unsigned int uf;
	int size;						/* Lut table size */
	double *gp;						/* Pointer to grid cube base */

	/* If not something that can really have a TAC */
	if (rh->deviceClass != icSigDisplayClass
	 && rh->deviceClass != icSigOutputClass
	 && rh->deviceClass != icSigLinkClass) {
		return -1.0;
	}

	/* If not a suitable color space */
	switch (rh->colorSpace) {
		/* Not applicable */
    	case icSigXYZData:
    	case icSigLabData:
    	case icSigLuvData:
    	case icSigYCbCrData:
    	case icSigYxyData:
    	case icSigHsvData:
    	case icSigHlsData:
			return -1.0;

		/* Assume no limit */
    	case icSigGrayData:
    	case icSig2colorData:
    	case icSig3colorData:
    	case icSigRgbData:
			return -1.0;

		default:
			break;
	}

	/* Get a PCS->device colorimetric lookup */
	if ((luo = p->get_luobj(p, icmBwd, icRelativeColorimetric, icmSigDefaultData, icmLuOrdNorm)) == NULL) {
		if ((luo = p->get_luobj(p, icmBwd, icmDefaultIntent, icmSigDefaultData, icmLuOrdNorm)) == NULL) {
			return -1.0;
		}
	}

	/* Get details of conversion (Arguments may be NULL if info not needed) */
	luo->spaces(luo, NULL, &inn, &outs, &outn, &alg, NULL, NULL, NULL, NULL);

	/* Assume any non-Lut type doesn't have a TAC */
	if (alg != icmLutType) {
		return -1.0;
	}

	ll = (icmLuLut *)luo;

	/* We have a Lut type. Search the lut for the largest values */
	for (f = 0; f < outn; f++)
		max[f] = 0.0;

	lut = ll->lut;
	gp = lut->clutTable;		/* Base of grid array */
	size = sat_pow(lut->clutPoints,lut->inputChan);
	for (i = 0; i < size; i++) {
		double tot, vv[MAX_CHAN];			
		
		lut->lookup_output(lut,vv,gp);		/* Lookup though output tables */
		ll->out_denormf(vv,vv);				/* Normalize for output color space */

		if (calfunc != NULL)
			calfunc(cntx, vv, vv);			/* Apply device calibration */

		for (tot = 0.0, uf = 0; uf < lut->outputChan; uf++) {
			tot += vv[uf];
			if (vv[uf] > max[uf])
				max[uf] = vv[uf];
		}
		if (tot > tac)
			tac = tot;
		gp += lut->outputChan;
	}

	if (chmax != NULL) {
		for (f = 0; f < outn; f++)
			chmax[f] = max[f];
	}

	luo->del(luo);

	return tac;
}

/* ---------------------------------------------------------- */

