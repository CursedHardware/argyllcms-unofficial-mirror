
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

/*
 * TTBD:
 *
 *      Need to fix all legacy icm_err() functions to have new error codes.
 *
 *		Should fix all write_number failure errors to indicate failed value.
 *		(Partially implemented - need to check all write_number functions)
 *
 *		Make write fail error messages be specific on which element failed.
 *
 *		Complete adding check() methods for TagTypes & improve existing ones.
 *
 *		Should add named color space lookup function support (see icmLuNamed type).
 *
 *		Should add dict color space lookup function support.
 *
 *      Would be nice to add generic ability to add new tag type handling,
 *      so that the base library doesn't need to be modified ?
 *
 */

// ~~88 Check all _FMT_ warnings and see if any more need converting to _FMTF_.
//		Make tag read code mark tag as not accessible if a _FMTF_ is returned from read.

// ~~88 Add support for ZXML type ??

#define _ICC_C_				/* Turn on implimentation code */

#undef BREAK_ON_ICMERR		/* [Und] Break on an icmErr() */
#undef DEBUG_BOUNDARY_EXCEPTION /* [Und] Show details of boundary exception & Break */
#undef DEBUG_SETLUT			/* [Und] Show each value being set in setting lut contents */
#undef DEBUG_SETLUT_CLIP	/* [Und] Show clipped values when setting LUT */
#undef DEBUG_LULUT			/* [Und] Show each value being looked up from lut contents */
#undef DEBUG_LLULUT			/* [Und] Debug individual lookup steps (not fully implemented) */
#define ENABLE_DEDUP		/* [def] Enable de-duplication of tag elements */

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
#include "icc.h"
#include "icc_util.h"

/*  Make the default grid points of the Lab clut be symetrical about */
/* a/b 0.0, and also make L = 100.0 fall on a grid point. */
#define PUT_PCS_WHITE_ON_GRIDPOINT		/* [def] In B2A creation map PCS white to cLUT grid point */


#ifdef BREAK_ON_ICMERR
# pragma message("######### BREAK_ON_ICMERR enabled ########")
#endif

#ifdef DEBUG_BOUNDARY_EXCEPTION
# pragma message("######### DEBUG_BOUNDARY_EXCEPTION enabled ########")
#endif


#ifndef ENABLE_DEDUP
# pragma message("######### ENABLE_DEDUP disabled ########")
#endif

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
		DEBUG_BREAK;
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

/* Should use icc->clear_err() */
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

/* unsigned + signed */
unsigned int sat_add_us(unsigned int a, int b) {
	if (b > 0)
		return sat_add(a, (unsigned int)b);
	else
		return sat_sub(a, (unsigned int)-b);
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
	if (len > (p->end - p->cur)) {		/* Too much */
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
	if (na <= 1024)
		na += 1024;
	else
		na += 4096;
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
	if (len > (size_t)(p->aend - p->cur))	/* Try and expand buffer */
		icmFileMem_filemem_resize(p, p->cur + len);

	if (len > (size_t)(p->aend - p->cur)) {
		if (size > 0)
			count = (p->aend - p->cur)/size;
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
	int alen, len;

	va_start(args, format);

	rv = 1;
	alen = 100;					/* Initial allocation for printf */
	icmFileMem_filemem_resize(p, p->cur + alen);

	/* We have to use the available printf functions to resize the buffer if needed. */
	for (;rv != 0;) {
		/* vsnprintf() either returns -1 if it doesn't fit, or */
		/* returns the size-1 needed in order to fit. */
		len = vsnprintf((char *)p->cur, (p->aend - p->cur), format, args);

		if (len > -1 && ((p->cur + len +1) <= p->aend))	/* Fitted in current allocation */
			break;

		if (len > -1)				/* vsnprintf returned needed size-1 */
			alen = len+2;			/* (In case vsnprintf returned 1 less than it needs) */
		else
			alen *= 2;				/* We just have to guess */

		/* Attempt to resize */
		icmFileMem_filemem_resize(p, p->cur + alen);

		/* If resize failed */
		if ((p->aend - p->cur) < alen) {
			rv = 0;
			break;			
		}
	}
	if (rv != 0) {
		/* Figure out where end of printf is */
		len = strlen((char *)p->cur);	/* Length excluding nul */
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
		case icmSigLptData:
			return 3;

#ifdef NEVER
		case icmSigYData:
		case icmSigY8Data:
		case icmSigY16Data:
		case icmSigLData:
		case icmSigL8Data:
		case icmSigLV2Data:
#endif
			return 1;

		case icmSigXYZ8Data:
		case icmSigXYZ16Data:
		case icmSigLab8Data:
		case icmSigLabV2Data:
		case icmSigLuv16Data:
		case icmSigYxy16Data:
		case icmSigYCbCr16Data:
			return 3;

		default:
			break;
	}
	return 0;
}

/* Given a number of channels, return a matching device color space. */
/* Return 0 if unknown. */
static icColorSpaceSignature icmNchan2CSSig(unsigned int nchan) {
	switch (nchan) {
		case 1:
			return icSigGrayData;
		case 2:
			return icSig2colorData;
		case 3:
			return icSig3colorData;
		case 4:
			return icSig4colorData;
		case 5:
			return icSig5colorData;
		case 6:
			return icSig6colorData;
		case 7:
			return icSig7colorData;
		case 8:
			return icSig8colorData;
		case 9:
			return icSig9colorData;
		case 10:
			return icSig10colorData;
		case 11:
			return icSig11colorData;
		case 12:
			return icSig12colorData;
		case 13:
			return icSig13colorData;
		case 14:
			return icSig14colorData;
		case 15:
			return icSig15colorData;

		default:
			break;
	}
	return 0;
}

/* Return a mask classifying the color space */
unsigned int icmCSSig2type(icColorSpaceSignature sig) {
	switch ((int)sig) {
		case icSigXYZData:
			return CSSigType_PCS | CSSigType_NDEV | CSSigType_gPCS | CSSigType_gXYZ;
		case icSigLabData:
			return CSSigType_PCS | CSSigType_NDEV | CSSigType_gPCS | CSSigType_gLab;

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

		case icmSigYCbCr16Data:
			return CSSigType_DEV | CSSigType_EXT | CSSigType_NORM;

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

//		case icmSigLData:
//		case icmSigYData:
		case icmSigYuvData:
		case icmSigLptData:
			return CSSigType_NDEV | CSSigType_EXT;

		case icmSigLuv16Data:
		case icmSigYxy16Data:
			return CSSigType_NDEV | CSSigType_EXT | CSSigType_NORM;

		case icmSigXYZ8Data:
		case icmSigXYZ16Data:
			return CSSigType_NDEV | CSSigType_EXT | CSSigType_gPCS | CSSigType_nPCS
			     | CSSigType_gXYZ | CSSigType_NORM;

		case icmSigLab8Data:
		case icmSigLabV2Data:
			return CSSigType_NDEV | CSSigType_EXT | CSSigType_gPCS | CSSigType_nPCS
			     | CSSigType_gLab | CSSigType_NORM;

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

/* tags */
static const char *icmTagSig2str_imp(icTagSignature sig, int alt) {
	switch ((icmSig) sig) {
		case icSigAToB0Tag:
			return "AToB0 (Perceptual) Multidimensional Transform";
		case icSigAToB1Tag:
			return "AToB1 (Colorimetric) Multidimensional Transform";
		case icSigAToB2Tag:
			return "AToB2 (Saturation) Multidimensional Transform";
		case icSigBlueMatrixColumnTag:		/* AKA icSigBlueColorantTag */
			return "Blue Matrix Column";	/* AKA "Blue Colorant" */
		case icSigBlueTRCTag:
			return "Blue Tone Reproduction Curve";
		case icSigBToA0Tag:
			return "BToA0 (Perceptual) Multidimensional Transform";
		case icSigBToA1Tag:
			return "BToA1 (Colorimetric) Multidimensional Transform";
		case icSigBToA2Tag:
			return "BToA2 (Saturation) Multidimensional Transform";
		case icSigBToD0Tag:
			return "BToD0 (Perceptual) Multidimensional Transform";
		case icSigBToD1Tag:
			return "BToD1 (Colorimetric) Multidimensional Transform";
		case icSigBToD2Tag:
			return "BToD2 (Saturation) Multidimensional Transform";
		case icSigBToD3Tag:
			return "BToD3 (Absolute Colorimetric) Multidimensional Transform";
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
			return "DToB0 (Perceptual) Multidimensional Transform";
		case icSigDToB1Tag:
			return "DToB1 (Colorimetric) Multidimensional Transform";
		case icSigDToB2Tag:
			return "DToB2 (Saturation) Multidimensional Transform";
		case icSigDToB3Tag:
			return "DToB3 (Absolute Colorimetric) Multidimensional Transform";
		case icSigGamutTag:
			return "Gamut";
		case icSigGrayTRCTag:
			if (alt)
				return "Shaper Mono";
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
			return "Profile Sequence Description";
		case icSigProfileSequenceIdentifierTag:
			return "Profile Sequence Identifier";
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
			if (alt)
				return "Shaper Matrix";
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

static const char *icmTagSig2str(icTagSignature sig) {
	return icmTagSig2str_imp(sig, 0);
}

/* Interpret alternate aliases for */
/* icmSigShaperMatrix and icmSigShaperMatrixType */
static const char *icmTagSig2str_alt(icTagSignature sig) {
	return icmTagSig2str_imp(sig, 1);
}

/* tag type signatures */
static const char *icmTypeSig2str(icTagTypeSignature sig) {
	switch ((icmSig)sig) {
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
			return "XYZ";

		/* Actually V4... */
		case icSigColorantTableType:
		case icmSigAltColorantTableType:
			return "N-component Input Colorant Identification";


		/* Fake sub-tagtypes that implement Lut8Type and Lut16type */ 
		case icmSig816Curves:
			return "Lut8, Lut16 Curves";
		case icmSig816Matrix:
			return "Lut8, Lut16 Matrix";
		case icmSig816CLUT:
			return "Lut8, Lut16 cLUT";

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
static const char *icmColorSpaceSig2str(icColorSpaceSignature sig) {
	switch ((icmSig)sig) {
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
		case icSig1colorData:
		case icSigMch1Data:
			return "1 Color";

		case icmSigYuvData:
			return "Yu'v'";
		case icmSigLptData:
			return "Lpt";

		case icmSigXYZ8Data:
			return "8b Norm XYZ";
		case icmSigXYZ16Data:
			return "16b Norm XYZ";

		case icmSigLab8Data:
			return "8 bit Norm Lab";
		case icmSigLabV2Data:
			return "V2 Norm Lab";

		case icmSigLuv16Data:
			return "16b Norm Luv";
		case icmSigYxy16Data:
			return "16b Norm Yxy";
		case icmSigYCbCr16Data:
			return "16b Norm YCbCr";

#ifdef NEVER	/* Not fully implemented */
		case icmSigYData:
			return "Y";
		case icmSigY8Data:
			return "8b Norm Y";
		case icmSigY16Data:
			return "16b Norm Y";

		case icmSigLData:
			return "L";
		case icmSigL8Data:
			return "8b Norm L";
		case icmSigLV2Data:
			return "V2 Norm L";
#endif /* NEVER */

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
static const char *icmProfileClassSig2str(icProfileClassSignature sig) {
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
static const char *icmPlatformSig2str(icPlatformSignature sig) {
	static char buf[50];
	switch ((int)sig) {
		case icSigNoPlatform:
			return "Not Specified";
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
static const char *icmDeviceManufacturerSig2str(icDeviceManufacturerSignature sig) {
	return icmtag2str(sig);
}

/* Device Model Signatures */
static const char *icmDeviceModelSig2str(icDeviceModelSignature sig) {
	return icmtag2str(sig);
}

/* CMM Signatures */
static const char *icmCMMSig2str(icCMMSignature sig) {
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

/* technology signature descriptions */
static const char *icmTechnologySig2str(icTechnologySignature sig) {
	switch (sig) {
		case icSigTechnologyUnknown:
			return "Unknown Technology";
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

	switch(sig) {
		case icLanguageCodeEnglish:
			return "English";
		case icLanguageCodeGerman:
			return "German";
		case icLanguageCodeItalian:
			return "Italian";
		case icLanguageCodeDutch:
			return "Dutch";
		case icLanguageCodeSweden:
			return "Swedish";
		case icLanguageCodeSpanish:
			return "Spanish";
		case icLanguageCodeDanish:
			return "Danish";
		case icLanguageCodeNorwegian:
			return "Norwegian";
		case icLanguageCodeJapanese:
			return "Japanese";
		case icLanguageCodeFinish:
			return "Finish";
		case icLanguageCodeTurkish:
			return "Turkish";
		case icLanguageCodeKorean:
			return "Korean";
		case icLanguageCodeChinese:
			return "Chinese";
		case icLanguageCodeFrench:
			return "French";
		default:
			break;
	}

	if ((sigv & 0xff) >= 'a' && (sigv & 0xff)  <= 'z'
	 && ((sigv >> 8) & 0xff) >= 'a' && ((sigv >> 8) & 0xff)  <= 'z'
	 && (sigv >> 16) == 0) {
		sprintf(buf,"%c%c",sigv & 0xff, (sigv >> 8) & 0xff);
	} else {
		sprintf(buf,"0x%x",sig);
	}
	return buf;
}

/* A region code */
static const char *icmRegionCode2str(icEnumRegionCode sig) {
	unsigned int sigv = (unsigned int)sig;
	static char buf[50];

	switch(sig) {
		case icRegionCodeUSA:
			return "U.S.A.";
		case icRegionCodeUnitedKingdom:
			return "U.K.";
		case icRegionCodeGermany:
			return "Germany";
		case icRegionCodeItaly:
			return "Italy";
		case icRegionCodeNetherlands:
			return "Netherlands";
		case icRegionCodeSpain:
			return "Spain";
		case icRegionCodeDenmark:
			return "Denmark";
		case icRegionCodeNorway:
			return "Norway";
		case icRegionCodeJapan:
			return "Japan";
		case icRegionCodeFinland:
			return "Finland";
		case icRegionCodeTurkey:
			return "Turkey";
		case icRegionCodeKorea:
			return "Korea";
		case icRegionCodeChina:
			return "China";
		case icRegionCodeTaiwan:
			return "Taiwan";
		case icRegionCodeFrance:
			return "France";
		case icRegionCodeAustralia:
			return "Australia";
		default:
			break;
	}

	if ((sigv & 0xff) >= 'a' && (sigv & 0xff)  <= 'z'
	 && ((sigv >> 8) & 0xff) >= 'a' && ((sigv >> 8) & 0xff)  <= 'z'
	 && (sigv >> 16) == 0) {
		sprintf(buf,"%c%c",sigv & 0xff, (sigv >> 8) & 0xff);
	} else {
		sprintf(buf,"0x%x",sig);
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
static const char *icmMeasUnitsSig2str(icMeasUnitsSig sig) {
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

/* Return a text abreviation of a color lookup type */
static const char *icmLu4Type2str(icmLu4Type typ) {
	switch(typ) {
    	case icmSpaceLu4Type:
			return "ColorSpace";
    	case icmNamedLu4Type:
			return "Named Color";
		default: {
			static char bufs[5][30];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized - %d",typ);
			return buf;
		}
	}
}

/* Return a text abreviation of a color lookup algorithm (legacy) */
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

/* Return a text abreviation of a lookup source tag */
static const char *icmLuSrcType2str(icTagSignature sig) {
	switch (sig) {
		case icmSigShaperMono:
			return "Mono";

		case icmSigShaperMatrix:
			return "Matrix";

		case icSigAToB0Tag:
			return "Lut_A2B0";
		case icSigAToB1Tag:
			return "Lut_A2B1";
		case icSigAToB2Tag:
			return "Lut_A2B2";
		case icSigBToA0Tag:
			return "Lut_B2A0";
		case icSigBToA1Tag:
			return "Lut_B2A1";
		case icSigBToA2Tag:
			return "Lut_B2A2";

		case icSigGamutTag:
			return "Gamut Lut";

		default: {
			static char bufs[5][30];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized sig 0x%x",sig);
			return buf;
		}
	}
}

/* Return a text abreviation of a processing element */
static const char *icmPeSig2str(icmPeSignature sig) {
	switch(sig) {
		case icmSigPeNone:
			return "Not a Processing Element";
		case icmSigPeCurveSet:
			return "Group of 1d segments";
		case icmSigPeCurve:
			return "Linear/gamma/table curve";
		case icmSigPeMatrix:
			return "N x M + F matrix";
		case icmSigPeClut:
			return "N x M cLUT";
		case icmSigPeLut816:
			return "Lut8 or Lut16";

		case icmSigPeContainer:
			return "PE Sequence Container";

		case icmSigPeInverter:
			return "PE Inverter";

		case icmSigPeMono:
			return "Monochrome to PCS";

		case icmSigPeShaperMatrix:
			return "Shaper/Matrix sequence";
		case icmSigPeShaperMono:
			return "Shaper/Mono sequence";

		case icmSigPeXYZ2Lab:
			return "XYZ to Lab";
		case icmSigPeAbs2Rel:
			return "Abs to Rel";
		case icmSigPeXYZ2XYZ8:
			return "XYZ to XYZ 8 bit";
		case icmSigPeXYZ2XYZ16:
			return "XYZ to XYZ 16 bit";
		case icmSigPeLab2Lab8:
			return "Lab to Lab 8 bit";
		case icmSigPeLab2LabV2:
			return "Lab to V2 Lab 16 bit";
		case icmSigPeGeneric2Norm:
			return "Generic Normalisation";
		case icmSigPeGridAlign:
			return "Grid Alignment";
		case icmSigPeNOP:
			return "No Operation";

		default: {
			static char bufs[5][50];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized Processing Element - %s",icmtag2str(sig));
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
		case icmTagSig:
			return icmTagSig2str((icTagSignature) enumval);
		case icmTagSig_alt:
			return icmTagSig2str_alt((icTagSignature) enumval);
		case icmTypeSig:
			return icmTypeSig2str((icTagTypeSignature) enumval);
		case icmColorSpaceSig:
			return icmColorSpaceSig2str((icColorSpaceSignature) enumval);
		case icmProfileClassSig:
			return icmProfileClassSig2str((icProfileClassSignature) enumval);
		case icmPlatformSig:
			return icmPlatformSig2str((icPlatformSignature) enumval);
		case icmDeviceManufacturerSig:
			return icmDeviceManufacturerSig2str((icDeviceManufacturerSignature) enumval);
		case icmDeviceModelSig:
			return icmDeviceModelSig2str((icDeviceModelSignature) enumval);
		case icmCMMSig:
			return icmCMMSig2str((icCMMSignature) enumval);
		case icmTechnologySig:
			return icmTechnologySig2str((icTechnologySignature) enumval);
		case icmMeasurementGeometry:
			return icmMeasurementGeometry2str((icMeasurementGeometry) enumval);
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
		case icmMeasUnitsSig:
			return icmMeasUnitsSig2str((icMeasUnitsSig) enumval);
		case icmPhColEncoding:
			return icmPhColEncoding2str((icPhColEncoding) enumval);
		case icmTransformLookupFunc:
			return icmLookupFunc2str((icmLookupFunc) enumval);
		case icmTransformLookupOrder:
			return icmLookupOrder2str((icmLookupOrder) enumval);
		case icmProcessingElementOp:
			return icmPe_Op2str((icmPeOp) enumval);
		case icmProcessingElementTag:
			return icmPeSig2str((icmPeSignature) enumval);
		case icmTransformType:
			return icmLu4Type2str((icmLu4Type) enumval);			/* Lu4 */
		case icmTransformLookupAlgorithm:
			return icmLuAlgType2str((icmLuAlgType) enumval);		/* Lu2 */
		case icmTransformSourceTag:
			return icmLuSrcType2str((icTagSignature) enumval);		/* Lu4 */
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

/* Convert icmPe_lurv to a comma separated list of descriptions */
struct {
	icmPe_lurv mask;
	char *string;

} icmPe_lurvStrings[] = {
	{ icmPe_lurv_clip, "Clip" },
	{ icmPe_lurv_num,  "Numerical" },
	{ icmPe_lurv_bwclp,"BwdFmlaClip" },
	{ icmPe_lurv_imp,  "Implimentation" },
	{ icmPe_lurv_cfg,  "Configuration" },
	{ 0, NULL },

};

char *icmPe_lurv2str(icmPe_lurv err) {
	static char buf[200];
	char *bp = buf;
	int ne = 0;
	int i;

	if (err == icmPe_lurv_OK)
		return "Ok";

	for (i = 0; icmPe_lurvStrings[i].string != NULL; i++) {
		if (err & icmPe_lurvStrings[i].mask) {
			if (ne)
				bp += sprintf(bp, ", ");
			bp += sprintf(bp, "%s",icmPe_lurvStrings[i].string);
			ne = 1;
		}
	}
	return buf;
}

char *icmPe_Op2str(icmPeOp op) {
	switch(op) {
		case icmPeOp_NOP:
			return "NOP";
		case icmPeOp_perch:
			return "Per-channel Op";
		case icmPeOp_matrix:
			return "Matrix Op";
		case icmPeOp_cLUT:
			return "cLut Op";
		case icmPeOp_fmt:
			return "Format Op";
		case icmPeOp_complex:
			return "Complex Op";

		default: {
			static char bufs[5][50];	/* String buffers */
			static int si = 0;			/* String buffer index */
			char *buf = bufs[si++];
			si %= 5;					/* Rotate through buffers */

			sprintf(buf,"Unrecognized Pe Op - %d",op);
			return buf;
		}
	}
}

char *icmPe_Attr2Str(icmPeAttr *attr) {
	static char buf[100];

	sprintf(buf, "comp %d, inv %d, norm %d, op %s, fwd %d, bwd %d",
	attr->comp, attr->inv, attr->norm, icmPe_Op2str(attr->op), attr->fwd, attr->bwd);
	return buf;
}

char *icmCSInfo2str(icmCSInfo *p)  {
	static char buf[200];
	
	sprintf(buf,"%s, nch %d, min %s, max %s",
		icm2str(icmColorSpaceSig,p->sig), p->nch,
		icmPdvf(p->nch, "%.6f", p->min), icmPdvf(p->nch, "%.6f", p->max));

	return buf;
}

/* Return a string that shows the XYZ number value */
/* Returned buffer is static */
char *icmXYZNumber2str(icmXYZNumber *p) {
	static char buf[80];

	sprintf(buf,"%.8f, %.8f, %.8f", p->XYZ[0], p->XYZ[1], p->XYZ[2]);
	return buf;
}

/* Return a string that shows the XYZ number value, */
/* and the Lab D50 number in paren. */
/* Returned string is static */
char *icmXYZNumber_and_Lab2str(icmXYZNumber *p) {
	static char buf[100];
	double lab[3];
	icmXYZ2Lab(&icmD50, lab, p->XYZ);
	sprintf(buf,"%.8f, %.8f, %.8f    [Lab %f, %f, %f]", p->XYZ[0], p->XYZ[1], p->XYZ[2],
	                                              lab[0], lab[1], lab[2]);
	return buf;
}
			

/* ======================================================================== */
/*                     Notes on _serialise functions                        */
/* ======================================================================== */
/*

 *_serialise gets passed a pointer to icmFBuf which holds file contents and
 the operation type icmSnOp:

    icmSnResize   Allocate or resize variable sized elements.
    icmSnFree     Free variable sized elements.
    icmSnSize     Return file size the element would use.
    icmSnWrite    Write elements.
    icmSnRead     Allocate and read elements.

 Primitive and compound Serialization routines are all named icmSn*

 Because file buffer is setup before calling _serialise, a Type
 can be embeded in full within another type by calling its _serialise.
 - i.e. TextDescriptionType, MultiLocalizedUnicodeType, Curve.

 The expected sequence of calls to _serialise is:

 For writing:

	1. The user code creates a Tag & TagType struct.
	loop:
		2. They set values and sizes needed.
		3. The call tagtype->allocate() which calls op == icmSnResize to allocate arrays.
		4. User code sets array values.
	5. On profile write tatype->write() which
		i.  calls _check() functions.
		ii. calls op == icmSnSize which does a dummy serialise to compute
			TagType and sub element sizes and sets corresponding offsets in headers.
			Shared data should be aligned and file allocated for op == icmSnSize || icmSnWrite,
			check shared tagtype data for match to previous elements and share offset & length
			rather than serialize.
		iii. calls op == icmSnWrite which does actual serialise into file using same
		    logic as icmSnSize, but now with correct sizes & offsets.
		iv. After icmSnSize and icmSnWrite the file offset needs to be left at the end
	        of the tagtype.
	6. TagType->del() calls op == icmSnFree which frees all arrays.

 For reading:
 
	1. Profile read creats a TagType
	2. Profiles read does a tagtype->read() which calls op == icmSnRead
	    which de-serialises from the file. 
		Before serializing arrays icmArrayRdAllocResize() is called.
		After serializing arrays ICMSNFREEARRAY() is invoked.
	   Data that is defined by offset uses b->aoff() to seek to the data
	   befor serialising it. (Seek only on op == icmSnRead).
	   Shared values are duplicated in 'C' struct.
	   _check() functions are called.
	4. TagType->del() calls op == icmSnFree which frees all arrays.

 Optimizations:

	For extra speed for alloc/de-alloc, pure serialization loops can be
	skipped, i.e.:

	if (b->op & icmSnSerialise) {
	    for (n = 0; n < p->rcount; n++) {
        	icmSn_ui_UInt16(b, &value);
		}
	}

 Range/boundary checking
 -----------------------

	Reading and writing the file buffer is fully checked,
	but a guard is needed against accessing machine storage
	that is out of range.

	With storage that is allocated as it is needed,
	icmArrayRdAllocResize() when fed with a suitable non-zero
	bsize protects against exausting virtual memory with an allocation,
	while the allocation will always be big enough for the file derived
	dimension size.

	For machine storage that is static in size (i.e. fred[MAX_CHAN]), then
	it is critical that the file dimension value be sanity checked
	when read, and before it is used to access the machine storage.
  	i.e. icmSn_check_ui_UInt32()

 Dealing with shared data areas within tags
 ------------------------------------------

  We are taking the approach of not explicitly supporting the sharing
  of variable elements in the C structure below tag types themselves.
  On read any shared values are duplicated into the C structure.
  On write we have implemented de-duplication of matching values.
  File size is minimized without any special logic in the client code. 

 Sub-buffers
 -----------

    Given a buffer, a new pseudo-buffer can be create from it
    using b->new_sub(). This uses the same actual buffer, but
    can have an offset and shorter length. When destroyed, the
    super buffer offset it updated to match the offset of the sub-buffer.
    This is useful if offsets are needed from a different
    point, or for easy sub-length calculation or checking.

 Dealing with sub-tags
 ---------------------

   Call icmSn_SubTagType() with a sub-buffer.

*/

/* ======================================================================== */
/* File Buffer for serialisation to work with. */

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

		if (p->super == NULL) {

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

		} else {	/* We're a sub-buffer - we don't write anything */

			/* Increment the super-buffer by the size arrived at by serialisation */
			if (p->bp < p->buf
			 || p->bp > p->ep) {
				icm_err(p->icp, ICM_ERR_BUFFER_BOUND, "sub done_icmFBuf: pointer wrapped around");
				rv = 0;
			} else {
				rv = p->bp - p->buf;
				p->super->bp += rv;
			}
		}
	}

	if (p->super == NULL)
		p->icp->al->free(p->icp->al, p->buf);
	p->icp->al->free(p->icp->al, p);

	return rv;
}

static icmFBuf *icmFBuf_new_sub(struct _icmFBuf *super, ORD32 size);

/* Create a new file buffer. */
/* If op & icmSnDumyBuf, then f, of and size are ignored. */
/* of is the offset of the buffer in the file */
/* Sets icmFBuf and icc op */
/* Returns NULL on error & set icp->e  */
/* If super != NULL, then create a sub-buffer */
/* Sub-buffer length is limited to be remainder of super-buffer or size > 0, whichever is smaller */
static icmFBuf *new_icmFBuf_imp(icc *icp, icmFBuf *super, icmSnOp op, icmFile *fp, unsigned int of, ORD32 size) {
	icmFBuf *p;

	if (icp->e.c != ICM_ERR_OK)
		return NULL;		/* Pre-exitsting error */

	if ((p = (icmFBuf *) icp->al->calloc(icp->al, 1, sizeof(icmFBuf))) == NULL) {
		icm_err(icp, ICM_ERR_MALLOC, "new_icmFBuf: malloc failed");
		return NULL;
	}
	p->icp       = icp;				/* icc profile */

	p->super     = super;
	p->op        = op;				/* Current operation */

	p->roff      = icmFBuf_roff;
	p->aoff      = icmFBuf_aoff;
	p->get_off   = icmFBuf_get_off;
	p->get_space = icmFBuf_get_space;
	p->new_sub   = icmFBuf_new_sub;
	p->done      = icmFBuf_done;

	if (super == NULL) {				/* We're reading or writing */
		if (p->op & icmSnDumyBuf) {		/* We're just Sizing, allocating or freeing */
			p->fp = NULL;
			p->buf = p->bp = (ORD8 *)0;	/* Allow size to compute using pointer differences */
			p->ep = ((ORD8 *)0)-1;		/* Maximum pointer value */
			p->of = 0;
			p->size = p->ep - p->buf;
	
		} else {
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
	} else {	/* We're a sub-buffer */
		unsigned int suboff = super->bp - super->buf;	/* Offset of sub-buffer from super */
		p->size = super->size - suboff;			/* Default remaining size in super */
		if (size > 0 && size < p->size)			/* But can be set smaller if known */
			p->size = size;
		p->fp   = super->fp;
		p->of   = super->of + suboff;
		p->buf  = super->buf + suboff;
		p->bp   = p->buf;						/* Start at begining of sub-buf */
		p->ep   = p->bp + p->size;
	}

	return p;
}

/* Create a new file buffer. */
/* If op & icmSnDumyBuf, then f, of and size are ignored. */
/* of is the offset of the buffer in the file */
/* Sets icmFBuf and icc op */
/* Returns NULL on error & set icp->e  */
static icmFBuf *new_icmFBuf(icc *icp, icmSnOp op, icmFile *fp, unsigned int of, ORD32 size) {
	return new_icmFBuf_imp(icp, NULL, op, fp, of, size);
}

/* Return a sub-buffer within the current buffer, starting at the current offset. */
/* On ->done the super-buffer will be set to the equivalent offset of the sub-buffer */
/* If size > 0 and smaller than super, then will set smaller sub limit. */
static icmFBuf *icmFBuf_new_sub(struct _icmFBuf *super, ORD32 size) {
	return new_icmFBuf_imp(super->icp, super, super->op, super->fp, super->of, size);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Primitives */
/* All serialisation has to be done via primitives. */

/* Primitive functions are made polimorphic so that they can be */
/* involked from a single place using a table lookup. */
/* Serialization converts between ICC style file storage types and native C types. */
/* Return the number of bytes read or written from the buffer on success, */
/* and 0 on failure (this will only happen on write if value is not representable) */

// ~8 should add check that native types are >= ICC types they conduct values for.

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

/* - - - - - - - - - - - - - - - - */
/* Rounded numbers */

static ORD32 icmSnImp_d_RFix8(icmSnOp op, void *vp, ORD8 *p) {
	ORD32 o32;
	double v;

	if (op == icmSnRead) {
		o32 = (ORD32)p[0];
		v = (double)o32;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((double *)vp);
		v = floor(v + 0.5);
		if (v < 0.0 || v > 255.0)
			return 0;
		o32 = (ORD32)v;
		p[0] = (ORD8)(o32);
	}
	return 1;
}

static ORD32 icmSnImp_d_RFix16(icmSnOp op, void *vp, ORD8 *p) {
	ORD32 o32;
	double v;

	if (op == icmSnRead) {
		o32 = 256 * (ORD32)p[0]
		    +       (ORD32)p[1];
		v = (double)o32;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((double *)vp);
		v = floor(v + 0.5);
		if (v < 0.0 || v > 65535.0)
			return 0;
		o32 = (ORD32)v;
		p[0] = (ORD8)(o32 >> 8);
		p[1] = (ORD8)(o32);
	}
	return 2;
}

static ORD32 icmSnImp_d_RFix32(icmSnOp op, void *vp, ORD8 *p) {
	ORD32 o32;
	double v;

	if (op == icmSnRead) {
		o32 = 16777216 * (ORD32)p[0]
		  +      65536 * (ORD32)p[1]
		  +        256 * (ORD32)p[2]
		  +              (ORD32)p[3];
		v = (double)o32;
		*((double *)vp) = v;
	} else if (op == icmSnWrite) {
		v = *((unsigned int *)vp);
		v = floor(v + 0.5);
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


/*******************************************/
/* Platform independent IEE754 conversions */
/*******************************************/

/* Convert a native double to an IEEE754 encoded single precision value, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
static ORD32 doubletoIEEE754(double d) {
	ORD32 sn = 0, ep = 0, ma;
	ORD32 id;

	/* Convert double to IEEE754 single precision. */
	/* This would be easy if we're running on an IEEE754 architecture, */
	/* but isn't generally portable, so we use ugly code: */

	if (d < 0.0) {
		sn = 1;
		d = -d;
	}
	if (d != 0.0) {
		int ee;
		ee = (int)floor(log(d)/log(2.0));
		if (ee < -126)			/* Allow for denormalized */
			ee = -126;
		d *= pow(0.5, (double)(ee - 23));
		ee += 127;
		if (ee < 1)				/* Too small */
			ee = 0;				/* Zero or denormalised */
		else if (ee > 254) {	/* Too large */
			ee = 255;			/* Infinity */
			d = 0.0;
		}
		ep = ee;
	} else {
		ep = 0;					/* Zero */
	}
	ma = ((ORD32)d) & ((1 << 23)-1);
	id = (sn << 31) | (ep << 23) | ma;

	return id;
}

/* Convert a an IEEE754 encoded single precision value to a native double, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
static double IEEE754todouble(ORD32 ip) {
	double op;
	ORD32 sn = 0, ep = 0, ma;

	sn = (ip >> 31) & 0x1;
	ep = (ip >> 23) & 0xff;
	ma = ip & 0x7fffff;

	if (ep == 0) { 		/* Zero or denormalised */
		op = (double)ma/(double)(1 << 23);
		op *= pow(2.0, (-126.0));
	} else {
		op = (double)(ma | (1 << 23))/(double)(1 << 23);
		op *= pow(2.0, (((int)ep)-127.0));
	}
	if (sn)
		op = -op;
	return op;
}

static ORD32 icmSnImp_d_Float32(icmSnOp op, void *vp, ORD8 *p) {
	ORD32 o32;
	double v;

	if (op == icmSnRead) {
		o32 = 16777216 * (ORD32)p[0]
		  +      65536 * (ORD32)p[1]
		  +        256 * (ORD32)p[2]
		  +              (ORD32)p[3];
		*((double *)vp) = IEEE754todouble(o32);
	} else if (op == icmSnWrite) {
		o32 = doubletoIEEE754(*((double *)vp));
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
	ii = 64 bit int
	d  = double

	Second is file storage size & type abreviation.
	Smaller sizes can be stored in several possible native types

	Use icmSnPrim2str() display.
 */ 
typedef enum {
	icmSnPrim_pad        = 0,
	icmSnPrim_seek       = 1,
	icmSnPrim_uc_UInt8   = 2,
	icmSnPrim_us_UInt8   = 3,
	icmSnPrim_ui_UInt8   = 4,
	icmSnPrim_us_UInt16  = 5,
	icmSnPrim_ui_UInt16  = 6,
	icmSnPrim_ui_UInt32  = 7,
	icmSnPrim_uii_UInt64 = 8,
	icmSnPrim_d_U8Fix8   = 9,
	icmSnPrim_d_U1Fix15  = 10,
	icmSnPrim_d_U16Fix16 = 11,
	icmSnPrim_c_SInt8    = 12,
	icmSnPrim_s_SInt8    = 13,
	icmSnPrim_i_SInt8    = 14,
	icmSnPrim_s_SInt16   = 15,
	icmSnPrim_i_SInt16   = 16,
	icmSnPrim_i_SInt32   = 17,
	icmSnPrim_ii_SInt64  = 18,
	icmSnPrim_d_S7Fix8   = 19,
	icmSnPrim_d_S15Fix16 = 20,
	icmSnPrim_d_NFix8    = 21,
	icmSnPrim_d_NFix16   = 22,
	icmSnPrim_d_NFix32   = 23,
	icmSnPrim_d_RFix8    = 24,
	icmSnPrim_d_RFix16   = 25,
	icmSnPrim_d_RFix32   = 26,
	icmSnPrim_d_Float32  = 27
} icmSnPrim;
#define icmSnPrim_LAST icmSnPrim_d_Float32

/* In same order as above: */
struct {
	int size;		/* File size */
	ORD32 (* func)(icmSnOp op, void *vp, ORD8 *p);
	char *name;
} primitive_table[] = {
	{ 0, NULL,                "Zero Padding" },
	{ 0, NULL,                "Seek" },
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
	{ 4, icmSnImp_d_NFix32,   "Normalised 32" },
	{ 1, icmSnImp_d_RFix8,    "Rounded 8" },
	{ 2, icmSnImp_d_RFix16,   "Rounded 16" },
	{ 4, icmSnImp_d_RFix32,   "Rounded 32" },
	{ 4, icmSnImp_d_Float32,  "Float 32" }
};


/* All primitive serialisation goes through this function, */
/* so that there is a single place to check for buffer */
/* bounding errors. size is used for padding or seeking. */
static void icmSn_primitive(icmFBuf *b, void *vp, icmSnPrim pt, INR32 size) {
	ORD8 *bp = b->bp;	/* Current pointer */
	ORD8 *nbp;			/* Buffer pointer after access */

	/* Do nothing if an error has already occurred */
	/* or we're just resizing or freeing */
	if (b->icp->e.c != ICM_ERR_OK
	 || (b->op & icmSnSerialise) == 0)
		return;

	/* Buffer pointer after this access */
	if (pt == icmSnPrim_pad
	 || pt == icmSnPrim_seek)
		nbp = b->bp + size;
	else
		nbp = b->bp + primitive_table[pt].size;

	/* Check for possible access outside buffer. */
	/* We're being paranoid, because this is one place that */
	/* file format attacks will show up. */
	/* (It's ok for nbp to land one after end of buffer) */
	if (nbp < b->bp
	 || b->bp < b->buf
	 || b->bp >= b->ep
	 || nbp < b->buf
	 || nbp > b->ep) {
# ifdef DEBUG_BOUNDARY_EXCEPTION 
		printf("icmSn_primitive:\n");
		printf("  buffer pointer %p, next buffer pointer %p\n",b->bp, nbp);
		printf("  buffer start   %p, buffer end+1        %p\n",b->buf, b->ep);
		DEBUG_BREAK;
# endif /* DEBUG_BOUNDARY_EXCEPTION */
		icm_err(b->icp, ICM_ERR_BUFFER_BOUND, "icmSn_primitive: buffer boundary exception");
		return;
	}

	/* Invoke primitive serialisation */
	if (b->op != icmSnSize && pt != icmSnPrim_seek) {

		if (pt == icmSnPrim_pad) {
			if (b->op == icmSnWrite && size > 0) {
				ORD32 i;
				unsigned int tt = 0;
				for (i = 0; i < size; i++) {
					primitive_table[icmSnPrim_ui_UInt8].func(b->op, (void *)&tt, bp + i);
				}
			}

		} else {
			ORD32 rv;
			rv = primitive_table[pt].func(b->op, vp, bp);
			if (rv != primitive_table[pt].size)
				icm_err(b->icp, ICM_ERR_ENCODING,
				    "icmSn_primitive: unable to encode value to '%s'" ,primitive_table[pt].name);
		}
	}
	b->bp = nbp;
}

static char *icmSnPrim2str(icmSnPrim pt) {
	if (pt > icmSnPrim_LAST) {
		static char buf[80];
		sprintf(buf,"Out of bounts icmSnPrim enum %d",pt);
		return buf;
	}
	return primitive_table[pt].name;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Public serialisation routines, used to implement tags or compund */
/* serialisable types. These provide type checking that is otherwise */
/* ignored in the implementation above. */

static int icmFmtWarn(icmFBuf *b, int sub, const char *format, ...);

/* Add zero padding of size */
static void icmSn_pad(icmFBuf *b, ORD32 size) {
	icmSn_primitive(b, NULL, icmSnPrim_pad, size);
}

/* Alight with zero padding */
static void icmSn_align(icmFBuf *b, ORD32 align) {
	ORD32 size = 0;
	if (align < 1)
		align = 1;
	size = (0 - b->get_off(b)) & (align-1); 
	if (size > 0)
		icmSn_primitive(b, NULL, icmSnPrim_pad, size);
}

/* Relative seek */
static void icmSn_rseek(icmFBuf *b, INR32 size) {
	icmSn_primitive(b, NULL, icmSnPrim_seek, size);
}

/* Unsigned char <-> unsigned int 8 bit */
static void icmSn_uc_UInt8(icmFBuf *b, unsigned char *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_uc_UInt8, 0);
}

/* Unsigned short <-> unsigned int 8 bit */
static void icmSn_us_UInt8(icmFBuf *b, unsigned short *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_us_UInt8, 0);
}

/* Unsigned int <-> unsigned int 8 bit */
static void icmSn_ui_UInt8(icmFBuf *b, unsigned int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_ui_UInt8, 0);
}

/* Unsigned short <-> unsigned int 16 bit */
static void icmSn_us_UInt16(icmFBuf *b, unsigned short *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_us_UInt16, 0);
}

/* Unsigned int <-> unsigned int 16 bit */
static void icmSn_ui_UInt16(icmFBuf *b, unsigned int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_ui_UInt16, 0);
}

/* Unsigned int <-> unsigned int 32 bit */
static void icmSn_ui_UInt32(icmFBuf *b, unsigned int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_ui_UInt32, 0);
}

/* Unsigned 32+32 bit int <-> unsigned int 64 bit */
static void icmSn_uii_UInt64(icmFBuf *b, icmUInt64 *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_uii_UInt64, 0);
}

/* Double <-> unsigned 8.8 16 bit fixed point */
static void icmSn_d_U8Fix8(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_U8Fix8, 0);
}

/* Double <-> unsigned 1.15 16 bit fixed point */
static void icmSn_d_U1Fix15(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_U1Fix15, 0);
}

/* Double <-> unsigned 16.16 32 bit fixed point */
static void icmSn_d_U16Fix16(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_U16Fix16, 0);
}

/* Signed char <-> signed int 8 bit */
static void icmSn_c_SInt8(icmFBuf *b, signed char *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_c_SInt8, 0);
}

/* Signed short <-> signed int 8 bit */
static void icmSn_s_SInt8(icmFBuf *b, short *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_s_SInt8, 0);
}

/* Signed int <-> signed int 8 bit */
static void icmSn_i_SInt8(icmFBuf *b, int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_i_SInt8, 0);
}

/* Signed short <-> signed int 16 bit */
static void icmSn_s_SInt16(icmFBuf *b, short *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_s_SInt16, 0);
}

/* Signed int <-> signed int 16 bit */
static void icmSn_i_SInt16(icmFBuf *b, int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_i_SInt16, 0);
}

/* Signed int <-> signed int 32 bit */
static void icmSn_i_SInt32(icmFBuf *b, int *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_i_SInt32, 0);
}

/* Signed 32+32 bit int <-> signed int 64 bit */
static void icmSn_ii_SInt64(icmFBuf *b, icmInt64 *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_ii_SInt64, 0);
}

/* double <-> signed 7.8 16 bit */
static void icmSn_d_S7Fix8(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_S7Fix8, 0);
}

/* double <-> signed 15.16 32 bit */
static void icmSn_d_S15Fix16(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_S15Fix16, 0);
}

/* double <-> Normalize 1.0-0.0 8 bit */
static void icmSn_d_NFix8(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_NFix8, 0);
}

/* double <-> Normalize 1.0-0.0 16 bit */
static void icmSn_d_NFix16(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_NFix16, 0);
}

/* double <-> Normalize 1.0-0.0 32 bit */
static void icmSn_d_NFix32(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_NFix32, 0);
}

/* double <-> Rounded 8 bit */
static void icmSn_d_RFix8(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_RFix8, 0);
}

/* double <-> Rounded 16 bit */
static void icmSn_d_RFix16(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_RFix16, 0);
}

/* double <-> Rounded 32 bit */
static void icmSn_d_RFix32(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_RFix32, 0);
}

/* double <-> Float 32 bit */
static void icmSn_d_Float32(icmFBuf *b, double *p) {
	icmSn_primitive(b, (void *)p, icmSnPrim_d_Float32, 0);
}

/* Size checked routines. These always result in a fatal error */
/* if the size is exceeded - i.e. they cannot be converted to warnings. */

/* Checked unsigned int <-> unsigned int 8 bit */
static void icmSn_check_ui_UInt8(icmFBuf *b, unsigned int *p, unsigned int maxsize) {
	if (b->op == icmSnWrite) {
		if (*p > maxsize) {
			icmFmtWarn(b, ICM_FMTF_VRANGE,"icmSn_check_ui_UInt8 write: value %u > limit %u",
			                                                                    *p, maxsize);
			return;
		}
	}
	if (b->op & icmSnSerialise)
		icmSn_primitive(b, (void *)p, icmSnPrim_ui_UInt8, 0);

	if (b->op == icmSnRead) {
		if (*p > maxsize) {
			*p = maxsize;
			icmFmtWarn(b, ICM_FMTF_VRANGE,"icmSn_check_ui_UInt8 read: value %u > limit %u",
			                                                                    *p, maxsize);
			return;
		}
	}
}

/* Checked unsigned int <-> unsigned int 16 bit */
static void icmSn_check_ui_UInt16(icmFBuf *b, unsigned int *p, unsigned int maxsize) {
	if (b->op == icmSnWrite) {
		if (*p > maxsize) {
			icmFmtWarn(b, ICM_FMTF_VRANGE,"icmSn_check_ui_UInt8 write: value %u > limit %u",
			                                                                    *p, maxsize);
			return;
		}
	}
	if (b->op & icmSnSerialise)
		icmSn_primitive(b, (void *)p, icmSnPrim_ui_UInt16, 0);

	if (b->op == icmSnRead) {
		if (*p > maxsize) {
			*p = maxsize;
			icmFmtWarn(b, ICM_FMTF_VRANGE,"icmSn_check_ui_UInt8 read: value %u > limit %u",
			                                                                    *p, maxsize);
			return;
		}
	}
}

/* Checked unsigned int <-> unsigned int 32 bit */
static void icmSn_check_ui_UInt32(icmFBuf *b, unsigned int *p, unsigned int maxsize) {
	if (b->op == icmSnWrite) {
		if (*p > maxsize) {
			icmFmtWarn(b, ICM_FMTF_VRANGE,"icmSn_check_ui_UInt8 write: value %u > limit %u",
			                                                                    *p, maxsize);
			return;
		}
	}
	if (b->op & icmSnSerialise)
		icmSn_primitive(b, (void *)p, icmSnPrim_ui_UInt32, 0);

	if (b->op == icmSnRead) {
		if (*p > maxsize) {
			*p = maxsize;
			icmFmtWarn(b, ICM_FMTF_VRANGE,"icmSn_check_ui_UInt8 read: value %u > limit %u",
			                                                                    *p, maxsize);
			return;
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Encoding check support routine. */

/* Deal with a non immediately fatal formatting error. */
/* Sets error code if Format errors are fatal. */
/* else sets the warning cflags if set to warn. */
/* Return the current error code. */
static int icmVFormatWarning(icc *icp, int sub, const char *format, va_list vp) {
	int err;

	sub &= ICM_FMT_MASK;
	err = (icp->op == icmSnWrite) ? ICM_ERR_WR_FORMAT : ICM_ERR_RD_FORMAT;
	err |= sub;

	if ((icp->op == icmSnRead && !(icp->cflags & icmCFlagRdFormatWarn))
	 || (icp->op == icmSnWrite && !(icp->cflags & icmCFlagWrFormatWarn))
	 || sub >= ICM_FMTF) {
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
/* If warn nz then force warning rather than error */
/* Return the current error code. */
static int icmVVersionWarning(icc *icp, int sub, int warn, const char *format, va_list vp) {
	int err;

	err = (icp->op == icmSnWrite) ? ICM_ERR_WR_FORMAT : ICM_ERR_RD_FORMAT;
	err |= ICM_FMT_MASK & sub;

	if (!warn &&
	 ((icp->op == icmSnRead && !(icp->cflags & icmCFlagRdVersionWarn))
	  || (icp->op == icmSnWrite && !(icp->cflags & icmCFlagWrVersionWarn)))) {
		icm_verr(icp, err, format, vp);

	} else {
		icp->cflags |= (icp->op == icmSnWrite) ? icmCFlagWrWarning : icmCFlagRdWarning;
		if (icp->warning != NULL)
			icp->warning(icp, err, format, vp);
	}
	return icp->e.c;
}

/* Same as above with varargs */
int icmVersionWarning(icc *icp, int sub, int warn, const char *format, ...) {
	int rv;
	va_list vp;
	va_start(vp, format);
	rv = icmVVersionWarning(icp, sub, warn, format, vp);
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
		icp->cflags |= (icp->op == icmSnWrite) ? icmCFlagWrWarning : icmCFlagRdWarning;
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
static void icmSn_Screening32(icmFBuf *b, icScreening *p) {

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
static void icmSn_DeviceAttributes64(icmFBuf *b, icmUInt64 *p) {

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
static void icmSn_ProfileFlags32(icmFBuf *b, icProfileFlags *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckProfileFlags(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, p);
	if (b->op == icmSnRead)
		icmCheckProfileFlags(b, *p);
}


/* Check Ascii/Binary encoding for SigData type  */
static int icmCheckAsciiOrBinary(icmFBuf *b, icAsciiOrBinary *flags) {
	unsigned int valid;

	valid = icBinaryData;
	if (*flags & ~valid) {

		/* Correct ProfileMaker problem on read */
		if (b->op == icmSnRead && *flags == 0x01000000
		 && (b->icp->cflags & icmCFlagAllowQuirks) != 0) {
			icmQuirkWarning(b->icp, ICM_FMT_DATA_FLAG, 0, "Fixed SigDataType flag value 0x%x",
			                                                                          *flags);
			*flags = icBinaryData;
		} else {
			icmFmtWarn(b, ICM_FMT_ASCBIN,
		                   "Ascii or Binary data encodings '0x%x' contains unknown flags",*flags);
		}
	}
	return b->icp->e.c;
}

/* Ascii/Binary flag encoding uses 4 bytes */
static void icmSn_AsciiOrBinary32(icmFBuf *b, icAsciiOrBinary *p) {

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
static void icmSn_VideoCardGammaFormat32(icmFBuf *b, icVideoCardGammaFormat *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckVideoCardGammaFormat(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, p);
	if (b->op == icmSnRead)
		icmCheckVideoCardGammaFormat(b, *p);
}


/* We don't check generic signatures, because custom or private tags are permitted */
static void icmSn_Sig32(icmFBuf *b, icSig *p) {
	icmSn_ui_UInt32(b, (unsigned int *)p);
}

/* We don't check tag signatures, because custom or private tags are permitted */
static void icmSn_TagSig32(icmFBuf *b, icTagSignature *p) {
	icmSn_ui_UInt32(b, (unsigned int *)p);
}

/* We don't check tag types, because custom or private tags are permitted */
static void icmSn_TagTypeSig32(icmFBuf *b, icTagTypeSignature *p) {
	icmSn_ui_UInt32(b, (unsigned int *)p);
}


/* Check a Color Space Signature */
static int icmCheckColorSpaceSig(icmFBuf *b, icColorSpaceSignature sig) {
	switch((icmSig)sig) {
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

	switch((icmSig)sig) {
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
			if (!icmVersInRange(b->icp, &icmtvrange_21_plus)) {
				icmFmtWarn(b, ICM_FMT_COLSP,
				   "ColorSpace Signature %s is not valid for file version %s (valid %s)\n",
				   icmtag2str(sig), icmProfileVers2str(b->icp),
				   icmTVersRange2str(&icmtvrange_21_plus));
			}

			return b->icp->e.c;
		}
	}

	/* ICCLIB extensions */
	if (b->icp->cflags & icmCFlagAllowExtensions) {
		switch((icmSig)sig) {

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
			case icmSigLptData: {
//			case icmSigLData:
//			case icmSigYData:
				return b->icp->e.c;
			}
		}
	}

	icmFmtWarn(b, ICM_FMT_COLSP, "ColorSpace Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* Color Space signature uses 4 bytes */
static void icmSn_ColorSpaceSig32(icmFBuf *b, icColorSpaceSignature *p) {

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

	switch((icmSig)sig) {
		case icSigInputClass:
		case icSigDisplayClass:
		case icSigOutputClass:
		case icSigLinkClass:
		case icSigColorSpaceClass:
		case icSigAbstractClass:
		case icSigNamedColorClass:
			return b->icp->e.c;
	}
	icmFmtWarn(b, ICM_FMT_PROF, "Profile Class Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* Profile Class signature uses 4 bytes */
static void icmSn_ProfileClassSig32(icmFBuf *b, icProfileClassSignature *p) {

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
	switch((icmSig)sig) {
		case icSigMacintosh:
		case icSigMicrosoft:
		case icSigSolaris:
		case icSigSGI:
		case icSigTaligent:
			return b->icp->e.c;
	}

	if (icmVersInRange(b->icp, &icmtvrange_22_plus)
	 && ((icmSig)sig) == icSigNoPlatform) {
		return b->icp->e.c;
	}

	/* ICCLIB extensions */
	if (b->icp->cflags & icmCFlagAllowExtensions) {
		switch((icmSig)sig) {
			case icmSig_nix:
				return b->icp->e.c;
		}
	}

	icmFmtWarn(b, ICM_FMT_PLAT, "Platform Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* Platform signature uses 4 bytes */
static void icmSn_PlatformSig32(icmFBuf *b, icPlatformSignature *p) {

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
static void icmSn_DeviceManufacturerSig32(icmFBuf *b, icDeviceManufacturerSignature *p) {

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
static void icmSn_DeviceModel32(icmFBuf *b, icDeviceModelSignature *p) {

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

	switch((icmSig)sig) {
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
static void icmSn_CMMSig32(icmFBuf *b, icCMMSignature *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckCMMSig(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckCMMSig(b, *p);
}



/* Check a technology signature */
static int icmCheckTechnologySig(icmFBuf *b, icTechnologySignature sig) {

	switch((icmSig)sig) {
		case icSigTechnologyUnknown:
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
static void icmSn_TechnologySig32(icmFBuf *b, icTechnologySignature *p) {

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

	switch((icmSig)sig) {
		case icGeometryUnknown:
		case icGeometry045or450:
		case icGeometry0dord0:
			return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_MESGEOM, "Measurement Geometry 0x%x is unknown",sig);
	return b->icp->e.c;
}

/* Measurement Geometry uses 4 bytes */
static void icmSn_MeasurementGeometry32(icmFBuf *b, icMeasurementGeometry *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckMeasurementGeometry(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt32(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckMeasurementGeometry(b, *p);
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
static void icmSn_RenderingIntent32(icmFBuf *b, icRenderingIntent *p) {

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

	switch((icmSig)sig) {
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
static void icmSn_SpotShape32(icmFBuf *b, icSpotShape *p) {

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

	switch((icmSig)sig) {
		case icStdObsUnknown:
		case icStdObs1931TwoDegrees:
		case icStdObs1964TenDegrees:
			return b->icp->e.c;
	}

	icmFmtWarn(b, ICM_FMT_STOBS, "Standard Observer 0x%x is unknown",sig);
	return b->icp->e.c;
}

/*  Standard Observer uses 4 bytes */
static void icmSn_StandardObserver32(icmFBuf *b, icStandardObserver *p) {

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

	switch((icmSig)sig) {
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
static void icmSn_PredefinedIlluminant32(icmFBuf *b, icIlluminant *p) {

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
static void icmSn_LanguageCode16(icmFBuf *b, icEnumLanguageCode *p) {

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
static void icmSn_RegionCode16(icmFBuf *b, icEnumRegionCode *p) {

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

	switch((icmSig)sig) {
    	case icSigMsftResolution:
    	case icSigMsftMedia:
    	case icSigMsftHalftone:
			return b->icp->e.c;
	}
	icmFmtWarn(b, ICM_FMT_MSDEVID, "Microsoft platform Device Settings ID Signature %s is unknown",icmtag2str(sig));
	return b->icp->e.c;
}

/* Microsoft platform Device Settings ID signature uses 4 bytes */
static void icmSn_DevSetMsftIDSig32(icmFBuf *b, icDevSetMsftIDSignature *p) {

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

	switch((icmSig)sig) {
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
static void icmSn_DevSetMsftMedia32(icmFBuf *b, icDevSetMsftMedia *p) {

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
	switch((icmSig)sig) {
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
static void icmSn_DevSetMsftDither32(icmFBuf *b, icDevSetMsftDither *p) {

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

	switch((icmSig)sig) {
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
static void icmSn_MeasUnitsSig32(icmFBuf *b, icMeasUnitsSig *p) {

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

	switch((icmSig)sig) {
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
static void icmSn_PhCol16(icmFBuf *b, icPhColEncoding *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckPhCol(b, *p) != ICM_ERR_OK)
			return;
	}
	if (b->op & icmSnSerialise)
		icmSn_ui_UInt16(b, (unsigned int *)p);
	if (b->op == icmSnRead)
		icmCheckPhCol(b, *p);
}



/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compound serialisation functions */

/* XYZ number uses 12 bytes */
static void icmSn_XYZNumber12b(icmFBuf *b, icmXYZNumber *p) {
	if (b->op & icmSnSerialise) {
		icmSn_d_S15Fix16(b, &p->XYZ[0]);
		icmSn_d_S15Fix16(b, &p->XYZ[1]);
		icmSn_d_S15Fix16(b, &p->XYZ[2]);
	}
}

/* xy coordinate uses 8 bytes */
static void icmSn_xyCoordinate8b(icmFBuf *b, icmxyCoordinate *p) {
	if (b->op & icmSnSerialise) {
		icmSn_d_U16Fix16(b, &p->xy[0]);
		icmSn_d_U16Fix16(b, &p->xy[1]);
	}
}

/* PositionNumber uses 8 bytes */
static void icmSn_PositionNumbe8b(icmFBuf *b, icmPositionNumber *p) {
	if (b->op & icmSnSerialise) {
		icmSn_ui_UInt32(b, &p->offset);
		icmSn_ui_UInt32(b, &p->size);
	}
}

/* Response16Number uses 8 bytes */
static void icmSn_Response16Number8b(icmFBuf *b, icmResponse16Number *p) {
	if (b->op & icmSnSerialise) {
		icmSn_d_NFix16(b, &p->deviceValue);
		icmSn_pad(b, 2);
		icmSn_d_S15Fix16(b, &p->measurement);
	}
}


static int icmCheckFixDateTimeNumber(icmFBuf *b, icmDateTimeNumber *p) {

	/* Sanity check the date and time */
	if (p->year >= 1900 && p->year <= 3000
	 && p->month != 0 && p->month <= 12
	 && p->day != 0 && p->day <= 31
	 && p->hours <= 23
	 && p->minutes <= 59
	 && p->seconds <= 59) {
		return b->icp->e.c;
	}

	/* Correct the date/time on read */ 
	if (b->op == icmSnRead && (b->icp->cflags & icmCFlagAllowQuirks) != 0) {

		/* Check for Adobe swap problem */
		if (p->month >= 1900 && p->month <= 3000
		 && p->year != 0 && p->year <= 12
		 && p->hours != 0 && p->hours <= 31
		 && p->day <= 23
		 && p->seconds <= 59
		 && p->minutes <= 59) {
			unsigned int tt; 
	
			icmQuirkWarning(b->icp, ICM_FMT_DATETIME, 0, "Fixed bad DateTime value '%s'",
			                                                     icmDateTimeNumber2str(p));

			/* Correct Adobe's faulty profile */
			tt = p->month; p->month = p->year; p->year = tt;
			tt = p->hours; p->hours = p->day; p->day = tt;
			tt = p->seconds; p->seconds = p->minutes; p->minutes = tt;
			return b->icp->e.c;
		}

		icmQuirkWarning(b->icp, ICM_FMT_DATETIME, 0, "Limited bad DateTime value '%s'",
			                                                     icmDateTimeNumber2str(p));

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


	} else {
		icmFmtWarn(b, ICM_FMT_DATETIME, "Bad date time '%s'", icmDateTimeNumber2str(p));
	}
	return b->icp->e.c;
}

/* DateTime uses 12 bytes */
static void icmSn_DateTimeNumber12b(icmFBuf *b, icmDateTimeNumber *p) {

	if (b->op == icmSnWrite) {
		if (icmCheckFixDateTimeNumber(b, p) != ICM_ERR_OK)
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
		icmCheckFixDateTimeNumber(b, p);
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
static void icmSn_VersionNumber4b(icmFBuf *b, icmVers *p) {
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

/* ---------------------------------------------------------------- */
/* Higher level text string serialization support code */

/* printf magic to add padding at start of output */
/* (Implemented in embeddable tagtypes) */
#define PAD(XX) "%*s" XX ,pad,""

/* Serialise a ASCIIZ string. Houskeeping of the tagtype offsets */
/* and allocations is taken care of. */
static void icmSn_ASCIIZ(
	icmFBuf *b,
	unsigned int  *p_count,		/* 'C' structure ASCIIZ string current allocation size */
	unsigned int  *pcount,		/* 'C' structure requested/current size */
	char         **pdesc,		/* 'C' structure string (Assumed possible UTF-8) */
	unsigned int  *pfcount,		/* String file size, including nul. Should be NULL if fxlen != 0 */
	int			   fxlen,		/* If > 0, fixed size buffer length.  */
								/* If < 0, maximum buffer length.  */
	char *id					/* String identifying tagtype or function */
) {
	unsigned int fcount = 0;	/* Dummy file count if pfcount == NULL */
	icmUTFerr utferr;
	
	if (fxlen != 0 && pfcount == NULL) {
		fcount = fxlen > 0 ? fxlen : -fxlen;
		pfcount = &fcount;
	} 

	if (b->op == icmSnSize || b->op == icmSnWrite) {

		*pfcount = icmUTF8toASCIIZSn(&utferr, b, (icmUTF8 *)*pdesc, *pcount, fxlen);

		if (utferr != icmUTF_ok) {
			icm_err(b->icp, 1,"%s write: utf-8 to ASCIIZ translate returned error '%s'",id,icmUTFerr2str(utferr));
			return;
		}

	} else {	/* Alloc/Free/Read */

		/* Figure length needed for UTF-8 */
		if (b->op == icmSnRead) {
			ORD32 off;

			off = b->get_off(b);
			*pcount = icmASCIIZSntoUTF8(NULL, NULL, b, *pfcount, fxlen);
			b->aoff(b, off);
		}

		/* Allocate UTF-8 string on read or for API client */
		if (icmArrayAllocResize(b, p_count, pcount,
	   	 (void **)pdesc, sizeof(icmUTF8), id))
			return;

		/* Read ASCIIZ and (possibly) repair it */
		if (b->op == icmSnRead) {
			icmASCIIZSntoUTF8(&utferr, (icmUTF8 *)*pdesc, b, *pfcount, fxlen);

			if (utferr != icmUTF_ok) {
				if ((b->icp->cflags & icmCFlagAllowQuirks) == 0) {
					icmFormatWarning(b->icp, ICM_FMT_TEXT_UERR,"%s read: ASCIIZ to utf-8 translate returned error '%s'",id,icmUTFerr2str(utferr));
					return;
				}
				icmQuirkWarning(b->icp, ICM_FMT_TEXT_UERR, 0, "%s read: ASCIIZ to utf-8 translate returned error '%s'",id,icmUTFerr2str(utferr)); 
			}
		}
		ICMSNFREEARRAY(b, *p_count, *pdesc)
	}
}

/* Dump a text description of an ASCIIZ string */
static void icmASCIIZ_dump(
	char *desc,			/* ASCIIZ */
	unsigned int count,	/* length */
	icmFile *op,		/* Output to dump to */
	int verb,			/* Verbosity level */
	int pad				/* Extra padding at start */
) {
	unsigned int i, r, c;

	if (verb <= 0)
		return;

	if (desc != NULL && count > 0) {
		unsigned int size = count > 0 ? count-1 : 0;
		op->printf(op,PAD("  ASCII data, length %u chars:\n"),count);

		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->printf(op,"\n");
				break;
			}
			if (r > 1 && verb <= 1) {
				op->printf(op,PAD("    ...\n"));
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->printf(op,PAD("    0x%04lx: "),i);
			c += 10;
			while (i < size && c < 75) {
				if (isprint(desc[i])) {
					op->printf(op,"%c",desc[i]);
					c++;
				} else {
					op->printf(op,"\\%03o",desc[i]);
					c += 4;
				}
				i++;
			}
			if (i < size)
				op->printf(op,"\n");
		}
	} else {
		op->printf(op,PAD("  No ASCII data\n"));		/* Shouldn't happen ? */
	}
}

/* Serialise a Unicode string. Houskeeping of the tagtype offsets */
/* and allocations is taken care of. */
/* If nonul is nz, then UTF-16 has not null terminator, and uc16count is bytes. */
/* If !nonul write and if utf-8 is NULL or zero length, then no utf-16 is written. */
/* If !nonul read and if utf-16 is zero length, then the utf-8 will be NULL and zero length. */
static void icmSn_Unicode(
	icmFBuf *b,
	unsigned int  *p_uc8count,	/* 'C' structure UTF-8 string allocation */
	unsigned int  *puc8count,	/* 'C' structure UTF-8 string wanted size */
	icmUTF8       **puc8desc,	/* 'C' structure UTF-8 string buffer */
	unsigned int   *puc16off,	/* UTF-16 file offset (may be NULL) */
	unsigned int *puc16count,	/* UTF-16 file length (chars or bytes) */
	char *id,					/* String identifying tagtype or function */
	int nonul
) {
	icmUTFerr utferr = icmUTF_ok;

	if (b->op == icmSnSize || b->op == icmSnWrite) {

		if (puc16off != NULL)
			*puc16off = b->get_off(b);

		if (!nonul && (*puc8desc == NULL || *puc8count == 0)) 
			*puc16count = 0;
		else
			*puc16count = icmUTF8toUTF16Sn(&utferr, b, *puc8desc, *puc8count, nonul) / (nonul ? 1 : 2);

		if (utferr != icmUTF_ok) {
			icm_err(b->icp, 1,"%s write: utf-8 to utf-16 translate returned error '%s'",id,icmUTFerr2str(utferr));
			return;
		}

	} else {	/* ReSize/Free/Read */

		/* Figure length needed for UTF-8 */
		if (b->op == icmSnRead) {

			if (puc16off != NULL)
				b->aoff(b, *puc16off);		/* Go to UTF-16 string location */

			if (!nonul && *puc16count == 0)
				*puc8count = 0;
			else {
				ORD32 off;
	
				off = b->get_off(b);
				*puc8count = icmUTF16SntoUTF8(NULL, NULL, b, *puc16count * (nonul ? 1 : 2), nonul);
				b->aoff(b, off);
			}
		}

		/* Allocate UTF-8 string on read or for API client */
		if (icmArrayAllocResize(b, p_uc8count, puc8count,
	   	 (void **)puc8desc, sizeof(icmUTF8), id))
			return;

		if (b->op == icmSnResize && !nonul && *p_uc8count == 0)
			*puc8desc = NULL;		/* Overwrite ZMARKER */

		/* Read UTF-16 and translate to UTF-8 */
		if (b->op == icmSnRead && (nonul || *puc16count > 0)) {
			icmUTF16SntoUTF8(&utferr, *puc8desc, b, *puc16count * (nonul ? 1 : 2), nonul);

			if (utferr != icmUTF_ok) {
				if ((b->icp->cflags & icmCFlagAllowQuirks) == 0) {
					icmFormatWarning(b->icp, ICM_FMT_TEXT_UERR,"%s read: utf-16 to utf-8 translate returned error '%s'",id,icmUTFerr2str(utferr));
					return;
				}
				icmQuirkWarning(b->icp, ICM_FMT_TEXT_UERR, 0, "%s read: utf-16 to utf-8 translate returned error '%s'",id,icmUTFerr2str(utferr)); 
			}
		}
		ICMSNFREEARRAY(b, *p_uc8count, *puc8desc)
	}
}

/* Dump a text description of a unicode string */
static void icmUnicode_dump(
	icmUTF8 *uc8desc,		/* UTF-8 unicode */
	unsigned int uc8count,	/* UTF-8 length */
	unsigned int lang,		/* Language code, icMaxEnumLanguageCode if none */
	unsigned int country,	/* Country, icMaxEnumRegionCode if none */
	unsigned int uc16off,	/* UTF-16 offset, icmMaxOff if none */
	unsigned int uc16count,	/* UTF-16 character count */
	icmFile *op,			/* Output to dump to */
	int verb,				/* Verbosity level */
	int pad					/* Extra padding at start */
) {
	unsigned int i, r, c;

	if (verb <= 0)
		return;

	/* Don't want to dump Unicode or ScriptCode to console, as its interpretation */
	/* will be unknown. */
	if (uc8desc != NULL && uc8count > 0) {
		unsigned int size = uc8count > 0 ? uc8count-1 : 0;

		op->printf(op,PAD("  Unicode Data, "));

		if (lang != icMaxEnumLanguageCode)
			op->printf(op,"Language code '%s', ",icmLanguageCode2str(lang));

		if (country != icMaxEnumRegionCode)
			op->printf(op,"Country code '%s', ",icmRegionCode2str(country));

		op->printf(op,"UTF-16 ");

		if (uc16off != icmMaxOff)
			op->printf(op,"offset %u, ",uc16off);

		op->printf(op,"length %u chars\n",uc16count);

		op->printf(op,PAD("  UTF-8 unicode translation length %u chars:\n"), size);
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->printf(op,"\n");
				break;
			}
			if (r > 1 && verb <= 1) {
				op->printf(op,PAD("    ...\n"));
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->printf(op,PAD("    0x%04lx: "),i);
			c += 10;
			while (i < size && c < 75) {
				if (isprint(uc8desc[i])) {
					op->printf(op,"%c",uc8desc[i]);
					c++;
				} else {
					op->printf(op,"\\%03o",uc8desc[i]);
					c += 4;
				}
				i++;
			}
			if (i < size)
				op->printf(op,"\n");
		}
	} else {
		op->printf(op,PAD("  No Unicode data\n"));
	}
}

/* Serialise a ScriptCode string. Houskeeping of the tagtype offsets */
/* and allocations is taken care of. */
/* If write and desc is NULL or zero length, then no ScriptCode is written. */
/* If read and ScriptCode is zero length, then the scDesc will be NULL and zero length. */
/* (In principle we could do a ScriptCode<->Unicode translation using Apples conversion */
/*  tables and documentation. This is fairly complex. ICCV2 profiles with non-Roman */
/*  ScriptCode seem very rare in modern use though...) */
static void icmSn_ScriptCode(
	icmFBuf *b,
	unsigned int  *p_count,		/* 'C' structure ScriptCode string current allocation size */
	unsigned int  *pcount,		/* 'C' structure requested/current size */
	unsigned char **pdesc,		/* 'C' structure string */
	unsigned int  *pocount,		/* Occupied size inc. nul */
	char *id					/* String identifying tagtype or function */
) {
	icmUTFerr utferr = icmUTF_ok;

	if (b->op == icmSnSize || b->op == icmSnWrite) {
		*pocount = icmScriptCodetoSn(&utferr, b, (icmUTF8 *)*pdesc, *pcount);

		if (utferr != icmUTF_ok) {
			icm_err(b->icp, 1,"%s write: ScriptCode translate returned error '%s'",id,icmUTFerr2str(utferr));
			return;
		}

	} else {	/* Alloc/Free/Read */

		/* Figure length needed for reading ScriptCode */
		if (b->op == icmSnRead) {
			ORD32 off;

			off = b->get_off(b);
			*pcount = icmSntoScriptCode(NULL, NULL, b, *pocount);
			b->aoff(b, off);
		}

		/* Allocate ScriptCode string on read or for API client */
		if (icmArrayAllocResize(b, p_count, pcount,
	   	 (void **)pdesc, sizeof(icmUTF8), id))
			return;

		/* Read ScriptCode and (possibly) repair it */
		if (b->op == icmSnRead) {
			icmSntoScriptCode(&utferr, (icmUTF8 *)*pdesc, b, *pocount);

			if (utferr != icmUTF_ok) {
				if ((b->icp->cflags & icmCFlagAllowQuirks) == 0) {
					icmFormatWarning(b->icp, ICM_FMT_TEXT_UERR,"%s read: ScriptCode translate returned error '%s'",id,icmUTFerr2str(utferr));
					return;
				}
				icmQuirkWarning(b->icp, ICM_FMT_TEXT_UERR, 0, "%s read: ScriptCode translate returned error '%s'",id,icmUTFerr2str(utferr)); 
			}
		}
		ICMSNFREEARRAY(b, *p_count, *pdesc)
	}
}

/* ---------------------------------------------------------------- */
/* Higher level sub-tagtype serialization support code */

static icmBase *icc_new_ttype_imp(icc *p, icTagTypeSignature ttype,
                              icTagTypeSignature pttype, int rdff);
static int icc_compare_ttype(icc *p, icmBase *idst, icmBase *isrc);

/* Serialize a sub-tagtype */
static void icmSn_SubTagType(
	icmFBuf *b,
	unsigned int *off,			/* If not NULL, return/seek to this offset */
	unsigned int *count,		/* If not NULL, return size of sub-tag */
	icmBase **psub,				/* pointer to sub-structure */
	icTagTypeSignature ttype,	/* Sub Tag Type to create if not optional and not client supplied */
	icTagTypeSignature pttype,	/* Parent TagType */
	int opt,					/* If 0, sub-tag will be created on ->allocate() */
								/* If 1, sub-tag is optional (don't create on ->allocate()) */
								/* and create on read if off && count > 0. */
								/* If 2, sub-tag is always created by client, */
								/* and always expected to be read. */
	void (*init)(icmFBuf *b, icmBase *p), /* If not NULL, initialise a new sub-struct on create */
	int rdff,					/* Was read from file */
	int xpad					/* Extra dump padding */
) {
	if (b->op == icmSnFree) {
		if (*psub != NULL)
			(*psub)->del(*psub);		/* This does a serialise(icmSnFree) */
	} else {
		icmFBuf *bb;

		if (b->op == icmSnSize && opt == 2 && *psub == NULL) {
			icmFmtWarn(b, ICM_FMT_SUB_MISSING,
				          "icmSn_SubTagType: parent ttype %s missing sub-tag on write\n",
				                                             icmTypeSig2str(pttype));
			*psub = NULL;
			return;
		}

		/* Make sure it exist. If not optional then we created it. */
		if (*psub == NULL && (b->op & icmSnAlloc)
		 && (opt == 0
		     || (opt == 1 && b->op == icmSnRead && off != NULL && *off > 0
	                                && count != NULL && *count > 0)
		     || (opt == 2 && b->op == icmSnRead))) {
			unsigned int i, j;

			/* If we are reading, figure out what type it is */
			if (b->op == icmSnRead) {
				ORD32 coff;

				if (off != NULL)
					b->aoff(b, *off);

				coff = b->get_off(b);
			    icmSn_TagTypeSig32(b, &ttype);
				b->aoff(b, coff);
			}

			if ((*psub = icc_new_ttype_imp(b->icp, ttype, pttype, rdff)) == NULL) {
				icmFmtWarn(b, ICM_FMT_SUB_TYPE_UNKN, "Sub-TagType %s not created()",
				                                 icmTypeSig2str(ttype));
				*psub = NULL;
				return;
			}

			(*psub)->emb = 1;

			if (init != NULL)
				init(b, *psub);

		}

		if (b->icp->e.c != ICM_ERR_OK)
			return;

		if (*psub != NULL) {
			unsigned int subsize = 0;

			(*psub)->dp = xpad;	/* Set extra text dump() padding */

			/* Sub element is in common data area... */
			if (off != NULL) {
				if (b->op == icmSnSize || b->op == icmSnWrite) {
					*off = b->get_off(b);
				} else if (b->op == icmSnRead) {
					b->aoff(b, *off);
				}
			}

			if (b->op == icmSnRead && count != NULL)
				subsize = *count;		/* We know how big sub-type should be */

			bb = b->new_sub(b, subsize);
			if ((*psub)->serialise == NULL) {
				icm_err(b->icp, ICM_ERR_TTYPE_SLZE, "TagType %s has no serialise()",
				                                    icmTypeSig2str((*psub)->ttype));
				*psub = NULL;			/* No TagType object */
				return;
			}
				
			(*psub)->serialise((*psub), bb);
	
			if (count != NULL
			 && (b->op == icmSnSize || b->op == icmSnWrite)) {
				*count = bb->get_off(bb);
			}
			bb->done(bb);

		/* if empty element */
		} else {
			if (b->op == icmSnSize || b->op == icmSnWrite) {
				if (off != NULL)
					*off = 0;
				if (count != NULL)
					*count = 0;
			}
		}
	}
}

/* Serialize a MultiProcessElements sub-tag */
static void icmSn_PeSubTag(
	icmFBuf *b,
	unsigned int *off,			/* If not NULL, return/seek to this offset */
	unsigned int *count,		/* If not NULL, return size of sub-tag */
	icmPe **psub,				/* pointer to sub-structure */
	icTagTypeSignature pttype,	/* Parent TagType */
	int rdff,					/* Was read from file */
	int xpad					/* Extra dump padding */
) {
	icTagTypeSignature ttype = icmSigUnknownType;
	if (*psub != NULL)
		pttype = (*psub)->ttype;

	icmSn_SubTagType(b, off, count, (icmBase **)psub, ttype, pttype, 2, NULL, rdff, xpad);

	if (b->op == icmSnRead && *psub == NULL) {
		icmFmtWarn(b, ICM_FMT_SUB_MISSING,
				          "icmSn_PeSubTag: parent ttype %s missing sub-tag on read\n",
				                                             icmTypeSig2str(pttype));
	} 
}

/* ================================================================ */
/* Default implementation of the tag type methods             */
/* The default implementation uses the serialise method */
/* to do all the work, reducing the size of most */
/* tag type implementations. */

/* Return the number of bytes needed to write this tag */
/* Return 0 on error */
static unsigned int icmGeneric_get_size(icmBase *p) {
	if (p->serialise != NULL) {
		ICM_TAG_NEW_SIZE_BUFFER
		p->serialise(p, b);
		return b->done(b);
	}
	return 0;
}

/* Read the object, return error code */
static int icmGeneric_read(icmBase *p, unsigned int size, unsigned int of) {
	if (p->serialise != NULL) {
		ICM_TAG_NEW_READ_BUFFER
		p->serialise(p, b);
   		b->done(b);
	}
	return p->icp->e.c;
}

/* Write the object. return error code */
static int icmGeneric_write(icmBase *p, unsigned int size, unsigned int of, unsigned int pad) {
	if (p->serialise != NULL) {
		ICM_TAG_NEW_WRITE_BUFFER
		p->serialise(p, b);
		if (pad > 0) {	/* Pad out to size */
			icmSn_pad(b, pad);
		}
		b->done(b);
	}
	return p->icp->e.c;
}

/* Increase the reference count because the */
/* object is being shared. */
/* Call ->del() to decrement reference and delete */
/* the object if it the last one. */
static icmBase *icmGeneric_reference(icmBase *p) {
	p->refcount++;
	return p;
}

/* If this is the last reference, free all storage */
/* in the object and the object itself. */
static void icmGeneric_delete(icmBase *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		if (p->serialise != NULL) {
			ICM_TAG_NEW_FREE_BUFFER
			p->serialise(p, b);
			b->done(b);
		}
		p->icp->al->free(p->icp->al, p);
	}
}


/* Allocate variable sized data elements */
/* Return error code */
static int icmGeneric_allocate(icmBase *p) {
	if (p->serialise != NULL) {
		ICM_TAG_NEW_RESIZE_BUFFER
		p->serialise(p, b);
   		b->done(b);
	}
	return p->icp->e.c;
}

/* ---------------------------------------------------------- */
/* Implementation of a general array resizing utility function,         */
/* used within XXXX__serialise() functions.                   */

/* The allocation is sanity checked against the remaining file buffer size, */
/* so can't be used for allocations that aren't directly related to file contents. */
/* Return zero on sucess, nz if there is an error */
static int icmArrayRdAllocResize(
	icmFBuf *b,					/* Buffer we're dealing with */
	icmAResizeCountType ct,		/* Explicit count or count from buffer size flag ? */

	unsigned int *p_count,		/* Pointer to current count of items */
	unsigned int *pcount,		/* Pointer to new count of items (~8 why is this a pointer ?) */
	void **pdata,				/* Pointer to array of items allocation */
	unsigned int isize,			/* Item size */

								/* Count value sanity check data: */
	unsigned int maxsize,		/* Maximum size permitted for buffer (use UINT_MAX if not less */
								/* than remaining in buffer) */
	int fixedsize,				/* Further fixed size from current location before variable */
								/* elements. Space allowed for variable elements is */
								/* avail = min(maxsize, buf->space - fixedsize) */
	unsigned int bsize,			/* File buffer (ie. serialized) size of each item. */
								/* Use minumum if variable ? */
								/* Error if (*pcount * bsize) > avail */
	char *tagdesc				/* Stringification of tag type used for warning/error reporting */
) {
	if (b->icp->e.c != ICM_ERR_OK)
		return b->icp->e.c;

	/* Check the new size for sanity */
	if (b->op == icmSnRead) {
		unsigned int tsize, tavail;

		tavail = b->get_space(b);

		if (ct == icmAResizeByCount) {

			if (tavail > maxsize)
				tavail = maxsize;
	
			tsize = sat_mul(*pcount, bsize);
			tsize = sat_add_us(tsize, fixedsize);
	
			if (tsize > tavail) {
				return icm_err(b->icp, ICM_ERR_BUFFER_BOUND,
				                 "%s tag read array count %u is too big for buffer (tsize %u > tavail %u)",
				                                          tagdesc, *pcount, tsize, tavail);
			}

		} else if (ct == icmAResizeBySize) {
			if (tavail < fixedsize) {
				return icm_err(b->icp, ICM_ERR_BUFFER_BOUND,
				                 " %s tag size %u is too small",tagdesc, b->size);
			}
			*pcount = (tavail - fixedsize) / bsize;
			tsize = *pcount * bsize + fixedsize;
			if (tsize != tavail) {
				icmFmtWarn(b, ICM_FMT_PARTIALEL,
				         "%s (imp) tag has a partial array element (%u/%u bytes)",
				         tagdesc, tsize - tavail, bsize);
			}

		} else {
			return icm_err(b->icp, ICM_ERR_INTERNAL,
			        "icmArrayRdAllocResize: unknown icmAResizeCountType");
		}
	}

	/* Resize or Read - resize the array */
	if (b->op & icmSnAlloc) {
		if (*pcount != *p_count) {
			void *_np;
			if ((_np = (void *) b->icp->al->recalloc(b->icp->al, *pdata,
			     *p_count, isize, *pcount, isize)) == NULL) {
				return icm_err(b->icp, ICM_ERR_MALLOC,
				         "Allocating %s data size %d failed",tagdesc, *pcount);
			}
			*pdata = _np;
			*p_count = *pcount;
		}
	}

	return b->icp->e.c;
}

/* This version can be used where allocations aren't directly related to file contents. */
/* Return zero on sucess, nz if there is an error */
static int icmArrayAllocResize(
	icmFBuf *b,					/* Buffer we're dealing with */

	unsigned int *p_count,		/* Pointer to current count of items */
	unsigned int *pcount,		/* Pointer to new count of items */
	void **pdata,				/* Pointer to array of items allocation */
	unsigned int isize,			/* Item size */

	char *tagdesc				/* Stringification of tag type used for warning/error reporting */
) {
	if (b->icp->e.c != ICM_ERR_OK)
		return b->icp->e.c;

	/* Resize or Read - resize the array */
	if (b->op & icmSnAlloc) {
		if (*pcount != *p_count) {
			void *_np;
			if ((_np = (void *) b->icp->al->recalloc(b->icp->al, *pdata,
			     *p_count, isize, *pcount, isize)) == NULL) {
				return icm_err(b->icp, ICM_ERR_MALLOC,
				         "Allocating %s data size %d failed",tagdesc, *pcount);
			}
			*pdata = _np;
			*p_count = *pcount;
		}
	}

	return b->icp->e.c;
}

/* This version does an immediate realloc (i.e. ignores op) */

/* Return zero on sucess, nz if there is an error */
static int icmArrayResize(
	icc *icp,

	unsigned int *p_count,		/* Pointer to current count of items */
	unsigned int *pcount,		/* Pointer to new count of items */
	void **pdata,				/* Pointer to array of items allocation */
	unsigned int isize,			/* Item size */

	char *tagdesc				/* Stringification of tag type used for warning/error reporting */
) {
	if (icp->e.c != ICM_ERR_OK)
		return icp->e.c;

	if (*pcount != *p_count) {
		void *_np;
		if ((_np = (void *) icp->al->recalloc(icp->al, *pdata,
		     *p_count, isize, *pcount, isize)) == NULL) {
			return icm_err(icp, ICM_ERR_MALLOC,
			         "Allocating %s data size %d failed",tagdesc, *pcount);
		}
		*pdata = _np;
		*p_count = *pcount;
	}

	return icp->e.c;
}

/* ---------------------------------------------------------- */

#ifdef NEVER		// Not clear if this will get used...

/* Implementation of a general matrix resizing utility function,         */
/* used within XXXX__serialise() functions.                   */

/* Return zero on sucess, nz if there is an error */
int icmMatrixRdAllocResize(
	icmFBuf *b,					/* Buffer we're dealing with */

	unsigned int *p_rcount,		/* Pointer to current row count */
	unsigned int *prcount,		/* Pointer to new row count */
	unsigned int *p_ccount,		/* Pointer to current column count */
	unsigned int *pccount,		/* Pointer to new column count */
	void ***pdata,				/* Pointer to matrix of items allocation */
	unsigned int isize,			/* Item size */

								/* Count value sanity check data: */
	unsigned int maxsize,		/* Maximum size permitted for buffer (use UINT_MAX if not less than
								/* remaining in buffer) */
	unsigned int fixedsize,		/* Further fixed size from current location before variable */
								/* elements. Space allowed for variable elements is */
								/* avail = min(maxsize, buf->space - fixedsize) */
								/* Can be -ve if array is bigger than number of elements to read. */
	unsigned int bsize,			/* File buffer (ie. serialized) size of each item. */
								/* Use minumum if variable ? */
								/* Error if (*pcount * bsize) > avail */
	char *tagdesc				/* Stringification of tag type used for warning/error reporting */
) {
	if (b->icp->e.c != ICM_ERR_OK)
		return b->icp->e.c;

	/* Check the new size for sanity */
	if (b->op == icmSnRead) {
		unsigned int tsize, tavail;

		if (b->icp->e.c != ICM_ERR_OK)
			return 1;

		tavail = b->get_space(b);

		if (tavail > maxsize)
			tavail = maxsize;
	
		tsize = sat_add(fixedsize, sat_mul(sat_mul(*prcount, *pccount), bsize));

		if (tsize > tavail)
			return icm_err(b->icp, ICM_ERR_BUFFER_BOUND,
			                 "%s tag read matrix %u rows %u columns /size %u is too big for buffer (tsize %u > tavail %u)",
			                                          tagdesc, *prcount, *pccount, tsize, tavail);
	}

	/* Check & re-allocate rows and columns */
	if (b->op & icmSnAlloc) {
		unsigned int j;

		// If number of rows has changed
		if (*prcount != *p_rcount) {
			void **_np;
		
			/* If we've reduced rows, free row contents */
			if (*prcount < *p_rcount) {
				for (j = *prcount; j < *p_rcount; j++) {
					free ((*pdata)[j]);
					(*pdata)[j] = NULL;
				}  
			}
		
			/* re-alloc larger or smaller array of pointers to row data */
			if ((_np = (void **) b->icp->al->recalloc(b->icp->al, *pdata,
			     *p_rcount, sizeof(void *), *prcount, sizeof(void *))) == NULL) {
				return icm_err(b->icp, ICM_ERR_MALLOC,
				         "Allocating %s row pointers size %d failed",tagdesc, *prcount);
			}
			*pdata = _np;
			*p_rcount = *prcount;
		}

		// If num columns changed, re-alloc all row contents
		if (*pccount != *p_ccount) {
			for (j = 0; j < *p_rcount; j++) {
				void *_np;

				if ((_np = (void **) b->icp->al->recalloc(b->icp->al, (*pdata)[j],
				     (*pdata)[j] == NULL ? 0 : *p_ccount, isize, *pccount, isize)) == NULL) {
					return icm_err(b->icp, ICM_ERR_MALLOC,
					         "Allocating %s row %d contents size %d failed",tagdesc, j,*pccount);
				}
				(*pdata)[j] = _np;
			}
			*p_ccount = *pccount;

		/* If num columns not changed, just alloc any new row contents */
		} else if (*prcount > *p_rcount) {
			for (j = *p_rcount; j < *prcount; j++) {
				void *_np;

				if ((_np = (void **) b->icp->al->recalloc(b->icp->al, (*pdata)[j],
				     0, isize, *pccount, isize)) == NULL) {
					return icm_err(b->icp, ICM_ERR_MALLOC,
					         "Allocating %s row %d contents size %d failed",tagdesc, j,*pccount);
				}
				(*pdata)[j] = _np;
			}
		}
	}

	return b->icp->e.c;
}

#endif /* NEVER */

/* ---------------------------------------------------------- */
/* Auiliary function - return a string that represents a tag 32 bit value */
/* as a 4 character string or hex number. */
/* Note - returned buffers are static, can only be used 5 */
/* times before buffers get reused. */
char *icmtag2str(
	unsigned int tag
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

/* ========================================================== */
/* Object I/O routines                                        */
/* ========================================================== */

/* Forward declarations */

/* Default implementation of the tag type methods, that all use */
/* serialise as their implementation. */
static unsigned int icmGeneric_get_size(icmBase *p);
static int icmGeneric_read(icmBase *p, unsigned int size, unsigned int of);
static int icmGeneric_write(icmBase *p, unsigned int size, unsigned int of, unsigned int pad);
static void icmGeneric_del(icmBase *p);
static int icmGeneric_allocate(icmBase *p);

static int icc_check_sig(icc *p, unsigned int *ttix, int rd,
		icTagSignature sig, icTagTypeSignature ttype, icTagTypeSignature uttype,
        int rdff);

/* ---------------------------------------------------------- */
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
	    (void **)&p->data, sizeof(unsigned char),
	    UINT_MAX, 0, 1, "icmUnknown"))
		return;
	if (b->op & icmSnSerialise)				/* (Optimise for speed) */
		for (i = 0; i < p->count; i++)
			icmSn_uc_UInt8(b, &p->data[i]);	/* 8-n: Unknown tag data */
	ICMSNFREEARRAY(b, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmUnknown)
}

/* Dump a text description of the object */
static void icmUnknown_dump(icmUnknown *p, icmFile *op, int verb) {
	unsigned int i, ii, r, ph;

	if (verb <= 0)
		return;

	op->printf(op,"Unknown:\n");
	op->printf(op,"  Payload size in bytes = %u\n",p->count);

	/* Print one row of binary and ASCII interpretation if verb == 1, All if == 2 */
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
		if (ph == 1 && r > 1 && verb <= 1) {
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
static int icmUnknown_check(icmUnknown *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUnknown(icc *icp) {
	ICM_BASE_ALLOCINIT(icmUnknown, icmSigUnknownType)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt8Array object */

static void icmUInt8Array_serialise(icmUInt8Array *p, icmFBuf *b) {
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: Data Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */

	if (icmArrayRdAllocResize(b, icmAResizeBySize, &p->_count, &p->count,
	    (void **)&p->data, sizeof(unsigned int),
	    UINT_MAX, 0, 1, "icmUInt8Array"))
		return;
	if (b->op & icmSnSerialise) {				/* (Optimise for speed) */
		unsigned int i;
		for (i = 0; i < p->count; i++)
			icmSn_ui_UInt8(b, &p->data[i]);		/* 8-n: 8 bit unsigned data */
	}
	ICMSNFREEARRAY(b, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmUInt8Array)
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
	op->printf(op,"  No. elements = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %u:  %u\n",i,p->data[i]);
	}
}

/* Check UInt8Array */
static int icmUInt8Array_check(icmUInt8Array *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt8Array(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmUInt8Array, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt16Array object */

static void icmUInt16Array_serialise(icmUInt16Array *p, icmFBuf *b) {
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: Data Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */

	if (icmArrayRdAllocResize(b, icmAResizeBySize, &p->_count, &p->count,
	    (void **)&p->data, sizeof(unsigned int),
	    UINT_MAX, 0, 2, "icmUInt16Array"))
		return;
	if (b->op & icmSnSerialise) {				/* (Optimise for speed) */
		unsigned int i;
		for (i = 0; i < p->count; i++)
			icmSn_ui_UInt16(b, &p->data[i]);	/* 8-n: 16 bit unsigned data */
	}
	ICMSNFREEARRAY(b, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmUInt16Array)
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
	op->printf(op,"  No. elements = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %u:  %u\n",i,p->data[i]);
	}
}

/* Check UInt16Array */
static int icmUInt16Array_check(icmUInt16Array *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt16Array(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmUInt16Array, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt32Array object */

static void icmUInt32Array_serialise(icmUInt32Array *p, icmFBuf *b) {
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: Data Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */

	if (icmArrayRdAllocResize(b, icmAResizeBySize, &p->_count, &p->count,
	    (void **)&p->data, sizeof(unsigned int),
	    UINT_MAX, 0, 4, "icmUInt32Array"))
		return;
	if (b->op & icmSnSerialise) {				/* (Optimise for speed) */
		unsigned int i;
		for (i = 0; i < p->count; i++)
			icmSn_ui_UInt32(b, &p->data[i]);		/* 8-n: 32 bit unsigned data */
	}
	ICMSNFREEARRAY(b, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmUInt32Array)
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
	op->printf(op,"  No. elements = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %u:  %u\n",i,p->data[i]);
	}
}

/* Check UInt32Array */
static int icmUInt32Array_check(icmUInt32Array *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt32Array(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmUInt32Array, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt64Array object */

static void icmUInt64Array_serialise(icmUInt64Array *p, icmFBuf *b) {
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: Data Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */

	if (icmArrayRdAllocResize(b, icmAResizeBySize, &p->_count, &p->count,
	    (void **)&p->data, sizeof(icmUInt64),
	    UINT_MAX, 0, 8, "icmUInt64Array"))
		return;
	if (b->op & icmSnSerialise) {				/* (Optimise for speed) */
		unsigned int i;
		for (i = 0; i < p->count; i++)
			icmSn_uii_UInt64(b, &p->data[i]);	/* 8-n: 64 bit unsigned data */
	}
	ICMSNFREEARRAY(b, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmUInt64Array)
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
	op->printf(op,"  No. elements = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %u:  h=%u, l=%u\n",i,p->data[i].h,p->data[i].l);
	}
}

/* Check UInt64Array */
static int icmUInt64Array_check(icmUInt64Array *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt64Array(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmUInt64Array, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmU16Fixed16Array object */

static void icmU16Fixed16Array_serialise(icmU16Fixed16Array *p, icmFBuf *b) {
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: Data Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */

	if (icmArrayRdAllocResize(b, icmAResizeBySize, &p->_count, &p->count,
	    (void **)&p->data, sizeof(double),
	    UINT_MAX, 0, 4, "icmU16Fixed16Array"))
		return;
	if (b->op & icmSnSerialise) {				/* (Optimise for speed) */
		unsigned int i;
		for (i = 0; i < p->count; i++)
			icmSn_d_U16Fix16(b, &p->data[i]);		/* 8-n: 16.16 bit unsigned data */
	}
	ICMSNFREEARRAY(b, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmU16Fixed16Array)
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
	op->printf(op,"  No. elements = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %u:  %.8f\n",i,p->data[i]);
	}
}

/* Check U16Fixed16Array */
static int icmU16Fixed16Array_check(icmU16Fixed16Array *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmU16Fixed16Array(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmU16Fixed16Array, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmS15Fixed16Array object */

static void icmS15Fixed16Array_serialise(icmS15Fixed16Array *p, icmFBuf *b) {
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: Data Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */

	if (icmArrayRdAllocResize(b, icmAResizeBySize, &p->_count, &p->count,
	    (void **)&p->data, sizeof(double),
	    UINT_MAX, 0, 4, "icmS15Fixed16Array"))
		return;
	if (b->op & icmSnSerialise) {				/* (Optimise for speed) */
		unsigned int i;
		for (i = 0; i < p->count; i++)
			icmSn_d_S15Fix16(b, &p->data[i]);		/* 8-n: 15.16 bit signed data */
	}
	ICMSNFREEARRAY(b, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmS15Fixed16Array)
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
	op->printf(op,"  No. elements = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			op->printf(op,"    %u:  %.8f\n",i,p->data[i]);
	}
}

/* Check S15Fixed16Array */
static int icmS15Fixed16Array_check(icmS15Fixed16Array *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmS15Fixed16Array(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmS15Fixed16Array, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmXYZArray object */

static void icmXYZArray_serialise(icmXYZArray *p, icmFBuf *b) {
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: Data Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */

	if (icmArrayRdAllocResize(b, icmAResizeBySize, &p->_count, &p->count,
	    (void **)&p->data, sizeof(icmXYZNumber),
	    UINT_MAX, 0, 12, "icmXYZArray"))
		return;
	if (b->op & icmSnSerialise) {				/* (Optimise for speed) */
		unsigned int i;
		for (i = 0; i < p->count; i++)
			icmSn_XYZNumber12b(b, &p->data[i]);		/* 8-n: 3 x 15.16 bit signed data */
	}
	ICMSNFREEARRAY(b, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmXYZArray)
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
	op->printf(op,"  No. elements = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++) {
			op->printf(op,"    %u:  %s\n",i,icmXYZNumber_and_Lab2str(&p->data[i]));
			
		}
	}
}

/* Check XYZArray */
static int icmXYZArray_check(icmXYZArray *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmXYZArray(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmXYZArray, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmChromaticity object */

/* Serialise this tag type */
static void icmChromaticity_serialise(icmChromaticity *p, icmFBuf *b) {
	unsigned int i;

    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: Chromaticity Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */
	icmSn_ui_UInt16(b, &p->count);			/* 8-9: Number of device channels */
	icmSn_PhCol16(b, &p->enc);				/* 10-11: Colorant encoding */

	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_count, &p->count,
	    (void **)&p->data, sizeof(icmxyCoordinate),
	    UINT_MAX, 0, 8, "icmChromaticity"))
		return;
	if (b->op & icmSnSerialise)				/* (Optimise for speed) */
		for (i = 0; i < p->count; i++)
			icmSn_xyCoordinate8b(b, &p->data[i]);	/* 12-n: Chromaticity coordinate data */
	ICMSNFREEARRAY(p, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmChromaticity)
}

/* Dump a text description of the object */
static void icmChromaticity_dump(icmChromaticity *p, icmFile *op, int verb) {
	unsigned int i;

	if (verb <= 0)
		return;

	op->printf(op,"Chromaticity:\n");
	op->printf(op,"  No. device channels = %u\n",p->count);
	if (verb >= 1) {
		for (i = 0; i < p->count; i++) {
			op->printf(op,"    Colorant %u, x = %f, y = %f:\n",i,p->data[i].xy[0],p->data[i].xy[1]);
		}
	}
}

/* Check tags validity */
static int icmChromaticity_check(icmChromaticity *p, icTagSignature sig, int rd) {
	icc *icp = p->icp;
	unsigned int dchan = icmCSSig2nchan(icp->header->colorSpace);

	/* Does number of channels match header device space encoding ? */
	if (p->count != dchan) {
		icmFormatWarning(icp, ICM_FMT_CHRMCHAN,
		                       "Chromaticity no. channels %u doesn't match header %u",
		                                                              p->count,dchan);
	}

	/* Does number of channels match encoding ? */
	switch(p->enc) {
		case icPhColUnknown:
		case icMaxEnumPhCol:
			break;
		case icPhColITU_R_BT_709:
		case icPhColSMPTE_RP145_1994:
		case icPhColEBU_Tech_3213_E:
		case icPhColP22:
		case icPhColP3:
		case icPhColITU_R_BT2020:
			if (p->count != 3)
				icmFormatWarning(icp, ICM_FMT_CHRMCHAN,
					   "Chromaticity channels %u doesn't match encoding %s",
				       p->count,icmPhColEncoding2str(p->enc));
			if (icSigRgbData != icp->header->colorSpace)
				icmFormatWarning(icp, ICM_FMT_CHRMENC,
				       "Chromaticity encoding %s doesn't match header device colorspace %s",
				       icmPhColEncoding2str(p->enc),
				       icmColorSpaceSig2str(icp->header->colorSpace));
			break;
	}

	/* Do chromaticities matches encoding ? */
	if (p->count >= 3) {
		switch(p->enc) {
			case icPhColUnknown:
			case icMaxEnumPhCol:
				break;
			case icPhColITU_R_BT_709:
				if (diff_u16f16(p->data[0].xy[0], 0.640) || diff_u16f16(p->data[0].xy[1], 0.330)
				 || diff_u16f16(p->data[1].xy[0], 0.300) || diff_u16f16(p->data[1].xy[1], 0.600)
				 || diff_u16f16(p->data[2].xy[0], 0.150) || diff_u16f16(p->data[2].xy[1], 0.060))
				icmFormatWarning(icp, ICM_FMT_CHRMVALS,
				                                 "Chromaticity values for ITU_R_BT_709 are wrong");
				break;
			case icPhColSMPTE_RP145_1994:
				if (diff_u16f16(p->data[0].xy[0], 0.630) || diff_u16f16(p->data[0].xy[1], 0.340)
				 || diff_u16f16(p->data[1].xy[0], 0.310) || diff_u16f16(p->data[1].xy[1], 0.595)
				 || diff_u16f16(p->data[2].xy[0], 0.155) || diff_u16f16(p->data[2].xy[1], 0.070))
				icmFormatWarning(icp, ICM_FMT_CHRMVALS,
				                                 "Chromaticity values for SMPTE_RP145_1994 are wrong");
				break;
			case icPhColEBU_Tech_3213_E:
				if (diff_u16f16(p->data[0].xy[0], 0.640) || diff_u16f16(p->data[0].xy[1], 0.330)
				 || diff_u16f16(p->data[1].xy[0], 0.290) || diff_u16f16(p->data[1].xy[1], 0.600)
				 || diff_u16f16(p->data[2].xy[0], 0.150) || diff_u16f16(p->data[2].xy[1], 0.060))
				icmFormatWarning(icp, ICM_FMT_CHRMVALS,
				                                 "Chromaticity values for EBU_Tech_3213_E are wrong");
				break;
			case icPhColP22:
				if (diff_u16f16(p->data[0].xy[0], 0.625) || diff_u16f16(p->data[0].xy[1], 0.340)
				 || diff_u16f16(p->data[1].xy[0], 0.280) || diff_u16f16(p->data[1].xy[1], 0.605)
				 || diff_u16f16(p->data[2].xy[0], 0.155) || diff_u16f16(p->data[2].xy[1], 0.070))
				icmFormatWarning(icp, ICM_FMT_CHRMVALS,
				                                 "Chromaticity values for P22 are wrong");
				break;
		case icPhColP3:
				if (diff_u16f16(p->data[0].xy[0], 0.680) || diff_u16f16(p->data[0].xy[1], 0.320)
				 || diff_u16f16(p->data[1].xy[0], 0.265) || diff_u16f16(p->data[1].xy[1], 0.690)
				 || diff_u16f16(p->data[2].xy[0], 0.150) || diff_u16f16(p->data[2].xy[1], 0.060))
				icmFormatWarning(icp, ICM_FMT_CHRMVALS,
				                                 "Chromaticity values for P3 are wrong");
				break;
		case icPhColITU_R_BT2020:
				if (diff_u16f16(p->data[0].xy[0], 0.780) || diff_u16f16(p->data[0].xy[1], 0.292)
				 || diff_u16f16(p->data[1].xy[0], 0.170) || diff_u16f16(p->data[1].xy[1], 0.797)
				 || diff_u16f16(p->data[2].xy[0], 0.131) || diff_u16f16(p->data[2].xy[1], 0.046))
				icmFormatWarning(icp, ICM_FMT_CHRMVALS,
				                                 "Chromaticity values for ITU_R_BT2020 are wrong");
				break;
		}
	}
	return icp->e.c;
}

/* Setup the tag with the defined phoshor encoding. */
/* (This is utility for the client code) */
static int icmChromaticity_setup(icmChromaticity *p) {
	icc *icp = p->icp;

	switch(p->enc) {
		case icPhColUnknown:
		case icPhColITU_R_BT_709:
		case icPhColSMPTE_RP145_1994:
		case icPhColEBU_Tech_3213_E:
		case icPhColP22:
		case icPhColP3:
		case icPhColITU_R_BT2020:
			break;
		default:
			return icm_err(icp, ICM_ERR_UNKNOWN_COLORANT_ENUM,
			               "icmChromaticity_setup() Unknown colorant enum 0x%x",p->enc);
	}

	p->count = 3;
	if (p->allocate(p) != ICM_ERR_OK)
		return icp->e.c;

	switch(p->enc) {
		case icPhColITU_R_BT_709:
			p->data[0].xy[0] = 0.640;
			p->data[0].xy[1] = 0.330;
			p->data[1].xy[0] = 0.300;
			p->data[1].xy[1] = 0.600;
			p->data[2].xy[0] = 0.150;
			p->data[2].xy[1] = 0.060;
			break;
		case icPhColSMPTE_RP145_1994:
			p->data[0].xy[0] = 0.630;
			p->data[0].xy[1] = 0.340;
			p->data[1].xy[0] = 0.310;
			p->data[1].xy[1] = 0.595;
			p->data[2].xy[0] = 0.155;
			p->data[2].xy[1] = 0.070;
			break;
		case icPhColEBU_Tech_3213_E:
			p->data[0].xy[0] = 0.640;
			p->data[0].xy[1] = 0.330;
			p->data[1].xy[0] = 0.290;
			p->data[1].xy[1] = 0.600;
			p->data[2].xy[0] = 0.150;
			p->data[2].xy[1] = 0.060;
			break;
		case icPhColP22:
			p->data[0].xy[0] = 0.625;
			p->data[0].xy[1] = 0.340;
			p->data[1].xy[0] = 0.280;
			p->data[1].xy[1] = 0.605;
			p->data[2].xy[0] = 0.155;
			p->data[2].xy[1] = 0.070;
			break;
		case icPhColP3:
			p->data[0].xy[0] = 0.680;
			p->data[0].xy[1] = 0.320;
			p->data[1].xy[0] = 0.265;
			p->data[1].xy[1] = 0.690;
			p->data[2].xy[0] = 0.150;
			p->data[2].xy[1] = 0.060;
			break;
		case icPhColITU_R_BT2020:
			p->data[0].xy[0] = 0.780;
			p->data[0].xy[1] = 0.292;
			p->data[1].xy[0] = 0.170;
			p->data[1].xy[1] = 0.797;
			p->data[2].xy[0] = 0.131;
			p->data[2].xy[1] = 0.046;
			break;
		default:
			break;
	}
	return icp->e.c;
}

/* Create an empty object. Return null on error */
icmBase *new_icmChromaticity(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmChromaticity, ttype)
	p->setup = icmChromaticity_setup;
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmData object */

static void icmData_serialise(icmData *p, icmFBuf *b) {
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: Data Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */
	icmSn_AsciiOrBinary32(b, &p->flag);		/* 8-11: Ascii/Binary flag */

	/* ASCIIZ text */
	if (p->flag == icAsciiData) {
		p->fcount = b->size - 12;				/* Implied */ 
		icmSn_ASCIIZ(b, &p->_count, &p->count, (char **)&p->data,
		                &p->fcount, 0, "icmData");

	/* Binary data */
	} else if (p->flag == icBinaryData) {

		if (icmArrayRdAllocResize(b, icmAResizeBySize, &p->_count, &p->count,
		    (void **)&p->data, sizeof(unsigned char),
		    UINT_MAX, 0, 1, "icmData"))
			return;
		if (b->op & icmSnSerialise) {				/* (Optimise for speed) */
			unsigned int i;
			for (i = 0; i < p->count; i++)
				icmSn_uc_UInt8(b, &p->data[i]);		/* 12-n: ASCII or Binary data */
		}
		ICMSNFREEARRAY(b, p->_count, p->data)
	} else {
		icmFormatWarning(p->icp, ICM_FMT_DATA_FLAG, 0, "Unknown SigData flag value 0x%x", p->flag);
		return;
	}
	ICMRDCHECKCONSUMED(icmData)
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
		case icAsciiData:
			op->printf(op,"  ASCII data\n");
			size = p->count > 0 ? p->count-1 : 0;
			break;
		case icBinaryData:
			op->printf(op,"  Binary data\n");
			size = p->count;
			break;
		default:
			op->printf(op,"  Undefined data\n");
			size = p->count;
			break;
	}
	op->printf(op,"  No. elements = %u\n",p->count);

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
			if (p->flag == icAsciiData) {
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
		if (verb > 2 && p->flag != icAsciiData && ph == 0)
			ph = 1;
		else
			ph = 0;
	}
}

/* Check Data */
static int icmData_check(icmData *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmData(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmData, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmText object */

/* Serialise this tag type */

static void icmText_serialise(icmText *p, icmFBuf *b) {
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: Text Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */

	p->fcount = b->size - 8;				/* Implied */ 

	/* ASCIIZ text */
	icmSn_ASCIIZ(b, &p->_count, &p->count, &p->desc,
	                &p->fcount, 0, "icmText");
	ICMRDCHECKCONSUMED(icmText)
}

/* Dump a text description of the object */
static void icmText_dump(
	icmText *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	int pad = p->dp;

	if (verb <= 0)
		return;

	op->printf(op,PAD("Text:\n"));
	icmASCIIZ_dump(p->desc, p->count, op, verb, p->dp);
}

/* Check Text */
static int icmText_check(icmText *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmText(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmText, ttype)
	return (icmBase *)p;
}


/* ============================================================ */

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
/* and only operate correctly for dates in the UNIX epoc, 1970-2038 */
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

/* Serialise this tag type */
static void icmDateTime_serialise(icmDateTime *p, icmFBuf *b) {
	unsigned int i;

    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3:  DateTime Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7:  Zero padding */
	icmSn_DateTimeNumber12b(b, &p->date);	/* 8-19: Date/Time */
	ICMRDCHECKCONSUMED(icmDateTime)
}

/* Dump a text description of the object */
static void icmDateTime_dump(icmDateTime *p, icmFile *op, int verb) {
	icmDateTimeNumber l;

	if (verb <= 0)
		return;

	icmDateTimeNumber_tolocal(&l, &p->date);
	op->printf(op,"DateTimeNumber:\n");
	op->printf(op,"  UTC   Date&Time = %s\n", icmDateTimeNumber2str(&p->date));
	op->printf(op,"  Local Date&Time = %s\n", icmDateTimeNumber2str(&l));
}

/* Check tags validity */
static int icmDateTime_check(icmDateTime *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
icmBase *new_icmDateTime(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmDateTime, ttype)
	return (icmBase *)p;
}


/* ---------------------------------------------------------- */
/* Measurement */

static void icmMeasurement_serialise(icmMeasurement *p, icmFBuf *b) {
	unsigned int n;

    icmSn_TagTypeSig32(b, &p->ttype);				/* 0-3: Measurement Tag Type signature */
	icmSn_pad(b, 4);								/* 4-7: Zero padding */

	icmSn_StandardObserver32(b, &p->observer);		/* 8-11: Encoded value for standard observer */
	icmSn_XYZNumber12b(b, &p->backing);				/* 12-23: CIEXYZ of measurement backing */
	icmSn_MeasurementGeometry32(b, &p->geometry);
	icmSn_d_U16Fix16(b, &p->flare);					/* 28-31: 16.16 encoded measurement flare */
	icmSn_PredefinedIlluminant32(b, &p->illuminant); /* 32-35: Encoded standard illuminant */

	ICMRDCHECKCONSUMED(icmMeasurement)
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

/* Check Measurement */
static int icmMeasurement_check(icmMeasurement *p, icTagSignature sig, int rd) {
	/* Check measurement flare is between  0 and 100% */
	if (p->flare < 0.0 || p->flare > 1.0) {
		icmFormatWarning(p->icp, ICM_FMT_FLARERANGE,
		                       "Measurement flare %5.1f%% is out of range", p->flare * 100.0);
	}
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmMeasurement(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmMeasurement, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmNamedColor object */

static icmPe *new_icmSig2NormPe(icc *icp, icmSnPrim *pt, icColorSpaceSignature sig,
                                      icmEncLabVer ver, icmEncBytes bytes);
static void icmSn_d_n_prim(icmFBuf *b, icmPe *npe, icmSnPrim pt, double *p);

/* Serialise this tag type */
static void icmNamedColor_serialise(icmNamedColor *p, icmFBuf *b) {
	icc *icp = p->icp;
	int txtlen;
	int minvalsize;
	icmPe *pcs_npe = NULL; icmSnPrim pcs_pt; 
	icmPe *dev_npe = NULL; icmSnPrim dev_pt; 
	unsigned int i, j;

    icmSn_TagTypeSig32(b, &p->ttype);			/* 0-3: NamedColor Tag Type signature */
	icmSn_pad(b, 4);							/* 4-7: Zero padding */

	icmSn_ui_UInt32(b, &p->vendorFlag);			/* 8-11: Vendor Specific Flags */
	icmSn_ui_UInt32(b, &p->count);				/* 12-15: Number of named colors */

	if (p->ttype == icSigNamedColorType) {
		if (b->op & icmSnSerialise)
			p->nDeviceCoords = icmCSSig2nchan(icp->header->colorSpace);
		txtlen = -32;		/* Up to 32 */
		minvalsize = 1 + p->nDeviceCoords;	/* nul + n channel of 8 bits */
	} else {
		icmSn_check_ui_UInt32(b, &p->nDeviceCoords, MAX_CHAN);
		txtlen = 32;		/* Always 32 */
		minvalsize = 38 + 2 * p->nDeviceCoords;	/* 32 byte root + 6 byte PCS + 2n Dev channel */
	}

	icmSn_ASCIIZ(b, &p->_pcount, &p->pcount, &p->prefix,
		                NULL, txtlen, "NamedColor");	/* Prefix */
	icmSn_ASCIIZ(b, &p->_scount, &p->scount, &p->suffix,
		                NULL, txtlen, "NamedColor");	/* Suffix */

	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_count, &p->count,
	    (void **)&p->data, sizeof(icmNamedColorVal),
	    UINT_MAX, 0, minvalsize, "icmNamedColor"))
		return;


	if (b->op & icmSnSerialise) {
		/* NamedColor is always ICCV2 encoding. */
		if (p->ttype != icSigNamedColorType) {
			pcs_npe = new_icmSig2NormPe(icp, &pcs_pt, icp->header->pcs, icmEncV2, icmEnc16bit); 
			dev_npe = new_icmSig2NormPe(icp, &dev_pt, icp->header->colorSpace, icmEncV2, icmEnc16bit); 
		} else 
			dev_npe = new_icmSig2NormPe(icp, &dev_pt, icp->header->colorSpace, icmEncV2, icmEnc8bit); 
		if (p->icp->e.c != ICM_ERR_OK)
			return;			/* there was an error */
	}

	for (i = 0; i < p->count; i++) {
		icmNamedColorVal *pp = &p->data[i];

		icmSn_ASCIIZ(b, &pp->_rcount, &pp->rcount, &pp->root,
			                NULL, txtlen, "NamedColor");	/* Root name */

		if (b->op & icmSnSerialise) {						/* If cssn's inited */
		 	if (p->ttype != icSigNamedColorType)					/* NamedColor2 */
				icmSn_d_n_prim(b, pcs_npe, pcs_pt, pp->pcsCoords);	/* Color PCS */
			icmSn_d_n_prim(b, dev_npe, dev_pt, pp->deviceCoords);	/* Device values */
		}
	}
	if (b->op & icmSnSerialise) {
		if (pcs_npe != NULL)
			pcs_npe->del(pcs_npe);
		dev_npe->del(dev_npe);
	}
	ICMSNFREEARRAY(p, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmNamedColor)
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
			op->printf(op,"    Color %u:\n",i);
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


/* Check tags validity */
static int icmNamedColor_check(icmNamedColor *p, icTagSignature sig, int rd) {
	if (p->ttype != icSigNamedColorType
	 && p->nDeviceCoords != icmCSSig2nchan(p->icp->header->colorSpace)) {
		icmFormatWarning(p->icp, ICM_FMT_NCOL_NCHAN,
		                       "Named Color number of channnels %d doesn't match header %d",
								p->nDeviceCoords, icmCSSig2nchan(p->icp->header->colorSpace)); 
	}
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmNamedColor(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmNamedColor, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* Colorant table structure read/write support */
/* (This is really ICC V4, but icclibv2 had suport for it) */

/* Serialise this tag type */
static void icmColorantTable_serialise(icmColorantTable *p, icmFBuf *b) {
	icc *icp = p->icp;
	unsigned int i;
	icmPe *pcs_npe; icmSnPrim pcs_pt;

	/* Setup the PCS serialiser */
	if (b->op & icmSnSerialise) {
		if (p->icp->header->deviceClass == icSigLinkClass)
			pcs_npe = new_icmSig2NormPe(icp, &pcs_pt, icSigLabData, icmEncProf, icmEnc16bit);
		else
			pcs_npe = new_icmSig2NormPe(icp, &pcs_pt, p->icp->header->pcs, icmEncProf, icmEnc16bit);
		if (p->icp->e.c != ICM_ERR_OK)
			return;			/* there was an error */

	    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3:  ColorantTable Tag Type signature */
		icmSn_pad(b, 4);						/* 4-7:  Zero padding */
		icmSn_ui_UInt32(b, &p->count);			/* 8-11: Number of device channels */
	}

	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_count, &p->count,
	    (void **)&p->data, sizeof(icmColorantTableVal),
	    UINT_MAX, 0, 38, "icmColorantTable"))
		return;

	for (i = 0; i < p->count; i++) {
		icmColorantTableVal *pp = &p->data[i];

		icmSn_ASCIIZ(b, &pp->_ncount, &pp->ncount, &pp->name,
		                NULL, 32, "icmColorantTableVal");	/* 12 + 38 * n: Colorant name */
		if (b->op & icmSnSerialise) {
			icmSn_d_n_prim(b, pcs_npe, pcs_pt, pp->pcsCoords);/* 12 + 38 * n + 32: Colorant color */
		}
	}
	ICMSNFREEARRAY(b, p->_count, p->data)
	ICMRDCHECKCONSUMED(icmColorantTable)
	if (b->op & icmSnSerialise) {
		pcs_npe->del(pcs_npe);
	}
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
	if (verb >= 1) {
		unsigned int i;
		icmColorantTableVal *vp;
		for (i = 0; i < p->count; i++) {
			vp = p->data + i;
			op->printf(op,"    Colorant %u:\n",i);
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

/* Check ColorantTable */
static int icmColorantTable_check(icmColorantTable *p, icTagSignature sig, int rd) {
	unsigned int cchan;

	if (sig == icSigColorantTableOutTag)
		cchan = icmCSSig2nchan(p->icp->header->pcs);		/* Output space */
	else
		cchan = icmCSSig2nchan(p->icp->header->colorSpace);	/* Device/input space */

	/* Does number of channels match header device space encoding ? */
	if (p->count != cchan)
		icmFormatWarning(p->icp, ICM_FMT_CORDCHAN,
		                       "ColorantTable channels %u doesn't match header",p->count);
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmColorantTable(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmColorantTable, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* textDescription */

static void icmTextDescription_serialise(icmTextDescription *p, icmFBuf *b) {

    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: TextDescription Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */

	/* ASCIIZ text */
	icmSn_ui_UInt32(b, &p->fcount);			/* ASCIIZ character count inc. nul */
	icmSn_ASCIIZ(b, &p->_count, &p->count, &p->desc,
	                &p->fcount, 0, "icmTextDescription");

	/* Unicode */
	icmSn_ui_UInt32(b, &p->ucLangCode);		/* UTF-16 language code */
	icmSn_ui_UInt32(b, &p->uc16count);		/* UTF-16 character count inc. nul */
	icmSn_Unicode(b, &p->_uc8count, &p->uc8count, &p->uc8desc,
	                 NULL, &p->uc16count, "icmTextDescription", 0); 

	/* ScriptCode */
	icmSn_us_UInt16(b, &p->scCode);			/* ScriptCode language code */
	icmSn_ui_UInt8(b, &p->scFcount);		/* ScriptCode character count */

	icmSn_ScriptCode(b, &p->_scCount, &p->scCount, &p->scDesc,
	                &p->scFcount, "icmTextDescription");

	if (b->super == NULL) {		/* Hmm. We don't have knowledge of expected sub-type size... */
		ICMRDCHECKCONSUMED(icmTextDescription)
	}
}

/* Dump a text description of the object */
static void icmTextDescription_dump(
	icmTextDescription *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	int pad = p->dp;
	unsigned int i, r, c;

	if (verb <= 0)
		return;

	/* ASCII */
	op->printf(op,PAD("TextDescription:\n"));
	icmASCIIZ_dump(p->desc, p->count, op, verb, pad); 

	/* Unicode */
	/* Don't want to dump Unicode or ScriptCode to console, as its interpretation */
	/* will be unknown. */
	icmUnicode_dump(p->uc8desc, p->uc8count, p->ucLangCode, icMaxEnumRegionCode, icmMaxOff,
	                                                         p->uc16count, op, verb, p->dp);
	/* ScriptCode */
	if (p->scCount > 0) {
		unsigned int size = p->scCount > 0 ? p->scCount-1 : 0;
		op->printf(op,PAD("  ScriptCode Data, Code 0x%x, length %u chars:\n"),
		        p->scCode, p->scCount);
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->printf(op,"\n");
				break;
			}
			if (r > 1 && verb <= 1) {
				op->printf(op,PAD("    ...\n"));
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->printf(op,PAD("    0x%04lx: "),i);
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
		op->printf(op,PAD("  No ScriptCode data\n"));
	}
}

/* Check TextDescription */
static int icmTextDescription_check(icmTextDescription *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Compare another with this */
static int icmTextDescription_cmp(icmTextDescription *dst, icmTextDescription *src) {

	if (dst->ttype != src->ttype) {
		icm_err(dst->icp, ICM_ERR_UNIMP_TTYPE_NOTSAME,"icmTextDescription_cmp: different tagtypes");
		return 1;
	}

	if (dst->count != src->count)
		return 1;
	if (dst->count > 0 && strcmp(dst->desc, src->desc))
		return 1;

	if (dst->ucLangCode != src->ucLangCode)
		return 1;
	if (dst->uc8count != src->uc8count)
		return 1;
	if (dst->uc8count > 0 && strcmp((char *)dst->uc8desc, (char *)src->uc8desc))
		return 1;

	if (dst->scCount != src->scCount)
		return 1;
	if (dst->scCode != src->scCode)
		return 1;
	if (dst->scCount > 0 && strcmp((char *)dst->scDesc, (char *)src->scDesc))
		return 1;

	return 0;
}

/* Copy or translate from source to this ttype */
static int icmTextDescription_cpy(icmTextDescription *dst, icmBase *isrc) {
	icc *p = dst->icp;

	/* If straight copy */
	if (dst->ttype == icSigTextDescriptionType
	 && isrc->ttype == icSigTextDescriptionType) {
		icmTextDescription *src = (icmTextDescription *)isrc;

		/* Easy */
		dst->count = src->count;
		dst->uc8count = src->uc8count;
		dst->scCount = src->scCount;
		if (dst->allocate(dst))
			return p->e.c;

		if (src->count > 0)
			strcpy(dst->desc, src->desc);

		dst->ucLangCode = src->ucLangCode;
		if (src->uc8count > 0)
			strcpy((char *)dst->uc8desc, (char *)src->uc8desc);

		dst->scCode = src->scCode;
		if (src->scCount > 0)
			strcpy((char *)dst->scDesc, (char *)src->scDesc);

		return ICM_ERR_OK;

	}

	return icm_err(p, ICM_ERR_UNIMP_TTYPE_COPY,"icmTextDescription_cpy: unimplemented tagtype");
}

/* Create an empty object. Return null on error */
static icmBase *new_icmTextDescription(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmTextDescription, ttype)
	p->cmp = icmTextDescription_cmp;
	p->cpy = icmTextDescription_cpy;
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmProfileSequenceDesc object */

/* Initialiser for TextDescriptionType sub-tag */
static void icmProfileSequenceDesc_Textinit(icmFBuf *b, icmBase *p) {

	/* Default TextDescription to placeholder "" */
	if (p->ttype == icSigTextDescriptionType) {
		icmTextDescription *pp = (icmTextDescription *)(p);

		if (pp->count == 0) {
			pp->count = 1;

			/* Allocate UTF-8 string on read or for API client */
			if (icmArrayResize(p->icp, &pp->_count, &pp->count,
		   	 (void **)&pp->desc, sizeof(icmUTF8), "icmTextDescription default"))
				return;

			pp->desc[0] = '\000';
		}
	}
}


/* Serialise this tag type */
static void icmProfileSequenceDesc_serialise(icmProfileSequenceDesc *p, icmFBuf *b) {
	icc *icp = p->icp;
	unsigned int n;

    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: ProfileSequenceDesc Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */
	icmSn_ui_UInt32(b, &p->count);			/* 8-11: Number of string records */

	/* icmPSDescStruct array */
	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_count, &p->count,
	    (void **)&p->data, sizeof(icmPSDescStruct),
	    UINT_MAX, 0, 20 + 2 * 16, "icmProfileSequenceDesc array"))
		return;

	for (n = 0; n < p->count; n++) {
		icmPSDescStruct *pp = &p->data[n];

		if (icp->e.c != ICM_ERR_OK)
			return;

		icmSn_DeviceManufacturerSig32(b, &pp->deviceMfg);
		icmSn_DeviceModel32(b, &pp->deviceModel);
		icmSn_DeviceAttributes64(b, &pp->attributes);
		icmSn_TechnologySig32(b, &pp->technology);

		/* Deal with sub-tags */
		icmSn_SubTagType(b, NULL, NULL, (icmBase **)(&pp->mfgDesc), icmSigCommonTextDescriptionType,
		                 p->ttype, 0, icmProfileSequenceDesc_Textinit, p->rdff, 4);
		icmSn_SubTagType(b, NULL, NULL, (icmBase **)&pp->modelDesc, icmSigCommonTextDescriptionType,
		                 p->ttype, 0, icmProfileSequenceDesc_Textinit, p->rdff, 4);
	}

	ICMSNFREEARRAY(b, p->_count, p->data)
	ICMRDCHECKCONSUMED(ProfileSequenceDesc)
}

/* Dump a text description of the object */
static void icmProfileSequenceDesc_dump(
	icmProfileSequenceDesc *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb < 1)
		return;

	op->printf(op,"ProfileSequenceDesc:\n");
	op->printf(op,"  No. elements = %u\n",p->count);
	if (verb >= 1) {
		unsigned int n;
		for (n = 0; n < p->count; n++) {
			icmPSDescStruct *pp = &p->data[n];

			op->printf(op,"Element %u:\n",n);
			op->printf(op,"  Dev. Manufacturer = %s\n",icmDeviceManufacturerSig2str(pp->deviceMfg));
			op->printf(op,"  Dev. Model        = %s\n",icmDeviceModelSig2str(pp->deviceModel));
			op->printf(op,"  Dev. Attrbts      = %s\n", icmDeviceAttributes2str(pp->attributes.l));
			op->printf(op,"  Dev. Technology   = %s\n", icmTechnologySig2str(pp->technology));
			if (verb >= 2) {
				op->printf(op,"  Dev. Manufacturer Description:\n");
				pp->mfgDesc->dump(pp->mfgDesc, op, verb-1);
				op->printf(op,"  Dev. Model Description:\n");
				pp->modelDesc->dump(pp->modelDesc, op, verb-1);
			}
		}
	}
}

/* Check ProfileSequenceDesc */
static int icmProfileSequenceDesc_check(icmProfileSequenceDesc *p, icTagSignature sig, int rd) {
	unsigned int n;

	/* Check that the *device and *model are correct TagType for profile version */
	for (n = 0; n < p->count; n++) {
		icmPSDescStruct *pp = &p->data[n];
		icTagTypeSignature ttype;

		ttype = pp->mfgDesc->ttype;
		if (icc_check_sig(p->icp, NULL, rd, icmSigUnknown, ttype, ttype, p->rdff) != ICM_ERR_OK) {
			return p->icp->e.c;
		}

		ttype = pp->modelDesc->ttype;
		if (icc_check_sig(p->icp, NULL, rd, icmSigUnknown, ttype, ttype, p->rdff) != ICM_ERR_OK) {
			return p->icp->e.c;
		}
	}
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmProfileSequenceDesc(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmProfileSequenceDesc, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmResponseCurveSet16 object */

/* Serialise this tag type */
static void icmResponseCurveSet16_serialise(icmResponseCurveSet16 *p, icmFBuf *b) {
	icc *icp = p->icp;
	unsigned int m;

    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3: ResponseCurveSet16 Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7: Zero padding */

	icmSn_ui_UInt16(b, &p->nchan);			/* 8-11: Number of channels */
	icmSn_ui_UInt16(b, &p->typeCount);		/* 12-15: Number of measurement types */

	/* (This is hard to follow because we are creating an array of */
	/*  matricies (3 dimensions total) where the final dimension */
	/*  may vary with the first two, i.e.: */
	/*  typedata[typeCount] { response[ nchan ][ nMeas[typeCount][nchan] ] }, */
	/*  so the array houskeeping itself needs allocating...) */

	/* icmRCS16Struct array, one for each measurement type */
	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_typeCount, &p->typeCount,
	    (void **)&p->typeData, sizeof(icmRCS16Struct),
	    UINT_MAX, 0, 4, "icmResponseCurveSet16 array"))
		return;

	for (m = 0; m < p->typeCount; m++) {
		icmRCS16Struct *pp = &p->typeData[m];
		icmSn_ui_UInt32(b, &pp->off);		/* Offset to each record */
	}

	for (m = 0; m < p->typeCount; m++) {
		icmRCS16Struct *pp = &p->typeData[m];
		unsigned int n, q;

		if (icp->e.c != ICM_ERR_OK)
			return;

		if (b->op == icmSnSize || b->op == icmSnWrite)
			pp->off = b->get_off(b);
		else if (b->op == icmSnRead)
			b->aoff(b, pp->off);

		icmSn_MeasUnitsSig32(b, &pp->measUnit);		/* Measurement unit signature */

		/* Separate nMeas[nchan] array allocation tracking for each struct */
		if (icmArrayRdAllocResize(b, icmAResizeByCount, &pp->__nMeasNchan, &p->nchan,
		    (void **)&pp->_nMeas, sizeof(unsigned int),
		    UINT_MAX, 0, 16, "icmResponseCurveSet16 _nMeas array"))
			return;
		if (icmArrayRdAllocResize(b, icmAResizeByCount, &pp->_nchan, &p->nchan,
		    (void **)&pp->nMeas, sizeof(unsigned int),
		    UINT_MAX, 0, 16, "icmResponseCurveSet16 nMeas array"))
			return;

		/* Column pointers to arrays of response values */
		if (icmArrayRdAllocResize(b, icmAResizeByCount, &pp->_respNchan, &p->nchan,
		    (void **)&pp->response, sizeof(icmResponse16Number *),
		    UINT_MAX, 0, 16, "icmResponseCurveSet16 response pointer array"))
			return;

		/* PCS XYZ value of maximum colorant value */
		if (icmArrayRdAllocResize(b, icmAResizeByCount, &pp->_pcsNchan, &p->nchan,
		    (void **)&pp->pcsData, sizeof(icmXYZNumber),
		    UINT_MAX, 0, 16, "icmResponseCurveSet16 pcsData array"))
			return;

		/* number or measurements for this channel */
		for (n = 0; n < p->nchan; n++) {
			icmSn_ui_UInt32(b, &pp->nMeas[n]);
		}

		/* PCS XYZ value of maximum colorant value */
		for (n = 0; n < p->nchan; n++) {
			icmSn_XYZNumber12b(b, &pp->pcsData[n]);
		}

		for (n = 0; n < p->nchan; n++) {
			/* Arrays of response values */
			if (icmArrayRdAllocResize(b, icmAResizeByCount, &pp->_nMeas[n], &pp->nMeas[n],
			    (void **)&(pp->response[n]), sizeof(icmResponse16Number),
			    UINT_MAX, 0, 8, "icmResponseCurveSet16 response data array"))
				return;

			/* Response numbers */
			for (q = 0; q < pp->nMeas[n]; q++) {
				icmSn_Response16Number8b(b, &pp->response[n][q]);
			} 
		}

		for (n = 0; n < p->nchan; n++) {
			ICMSNFREEARRAY(b, pp->_nMeas[n], pp->response[n])
		}
		ICMSNFREEARRAY(b, pp->_pcsNchan, pp->pcsData)
		ICMSNFREEARRAY(b, pp->_respNchan, pp->response)
		ICMSNFREEARRAY(b, pp->_nchan, pp->nMeas)
		ICMSNFREEARRAY(b, pp->__nMeasNchan, pp->_nMeas)
	}

	ICMSNFREEARRAY(b, p->_typeCount, p->typeData)
	// Can't ICMRDCHECKCONSUMED because we randomly seek on read
}

/* Dump a text description of the object */
static void icmResponseCurveSet16_dump(
	icmResponseCurveSet16 *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	if (verb <= 0)
		return;

	op->printf(op,"ResponseCurveSet16:\n");
	op->printf(op,"  No. device channels   = %u\n",p->nchan);
	op->printf(op,"  No. Measurement Types = %u\n",p->typeCount);
	if (verb >= 1) {
		unsigned int m;
		for (m = 0; m < p->typeCount; m++) {
			icmRCS16Struct *pp = &p->typeData[m];
			unsigned int n, q;

			op->printf(op,"  Measurement index %u: Units = %s\n",m,icmMeasUnitsSig2str(pp->measUnit));

			for (n = 0; n < p->nchan; n++) {
				unsigned int q;

				op->printf(op,"    Channel index %u:\n",n);
				op->printf(op,"    Max Colorant XYZ =  %s\n",icmXYZNumber_and_Lab2str(&pp->pcsData[n]));
				op->printf(op,"    No. of responses %u\n",pp->nMeas[n]);

				if (verb < 2)
					continue;
				op->printf(op,"    Response: Index, Device Value, Measurement Reading\n");
				for (q = 0; q < pp->nMeas[n]; q++) {
					op->printf(op,"      %u:  %f, %f\n",q,pp->response[n][q].deviceValue,
					                                    pp->response[n][q].measurement);
				}
			}
			op->printf(op,"\n");
		}
	}
}

/* Check ResponseCurveSet16 */
static int icmResponseCurveSet16_check(icmResponseCurveSet16 *p, icTagSignature sig, int rd) {
	icc *icp = p->icp;
	unsigned int dchan = icmCSSig2nchan(icp->header->colorSpace);

	/* Does number of channels match header device space encoding ? */
	if (p->nchan != dchan) {
		icmFormatWarning(icp, ICM_FMT_CHRMCHAN,
	                       "ResponseCurveSet16 no. channels %u doesn't match header %u",
		                                                              p->nchan,dchan);
	}
	return icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmResponseCurveSet16(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmResponseCurveSet16, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmDeviceSettings object */

/* Serialise this tag type */
static void icmDeviceSettings_serialise(icmDeviceSettings *p, icmFBuf *b) {
	icc *icp = p->icp;
	unsigned int h, i, j, k, m;

	/* Device Settings */
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3:  DeviceSettings Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7:  Zero padding */
	icmSn_ui_UInt32(b, &p->count);			/* 8-11: Number of platforms */

	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_count, &p->count,
	    (void **)&p->data, sizeof(icmPlatformEntry),
	    UINT_MAX, 0, 12, "icmDeviceSettings"))
		return;

	for (h = 0; h < p->count; h++) {
		/* Platform entry */
		icmPlatformEntry *pep = &p->data[h];
		unsigned int pe_mark = b->get_off(b);
		icmSn_PlatformSig32(b, &pep->platform);		/* 0-3:  Platform sig. */
		icmSn_ui_UInt32(b, &pep->size);				/* 4-7:  Size of this subpart in bytes */
		icmSn_ui_UInt32(b, &pep->count);			/* 8-11: Count of setting combinations */

		if (icmArrayRdAllocResize(b, icmAResizeByCount, &pep->_count, &pep->count,
		    (void **)&pep->data, sizeof(icmSettingComb),
		    pep->size, 0, 8, "icmDeviceSettings"))
			return;

		for (i = 0; i < pep->count; i++) {
			/* Setting combination */
			icmSettingComb *scp = &pep->data[i];
			unsigned int sc_mark = b->get_off(b);
			icmSn_ui_UInt32(b, &scp->size);	/* 0-3: Size of subpart in bytes */
			icmSn_ui_UInt32(b, &scp->count);	/* 4-7: Count of settings */

			if (icmArrayRdAllocResize(b, icmAResizeByCount, &scp->_count, &scp->count,
			    (void **)&scp->data, sizeof(icmSettingStruct),
			    scp->size, 0, 12, "icmDeviceSettings"))
				return;

			for (j = 0; j < scp->count; j++) {
				/* Setting */
				icmSettingStruct *ssp = &scp->data[j];

				if (pep->platform == icSigMicrosoft) {
					icmSn_DevSetMsftIDSig32(b, &ssp->msft.sig);
					icmSn_ui_UInt32(b, &ssp->size);	/* Per setting size */
					icmSn_ui_UInt32(b, &ssp->count);

					if (icSigMsftResolution == ssp->msft.sig) {
						if (b->op == icmSnRead && ssp->size != 8)
							icmFormatWarning(icp, ICM_FMT_DSSIZE,
							   "DeviceSettings MsftResolution setting size mismatch %u != 8",
							    ssp->size);
						if (b->op == icmSnSize)
							ssp->size = 8;
						ssp->ssize = sizeof(struct icmMsfResolution);
						if (icmArrayRdAllocResize(b, icmAResizeByCount, &ssp->_count, &ssp->count,
						    (void **)&ssp->msft.resolution, ssp->ssize,
						    scp->size-4, 0, 8, "icmDeviceSettings"))
							return;
						if (b->op & icmSnSerialise)		/* (Optimise for speed) */
							for (k = 0; k < ssp->count; k++) {
							 	icmSn_ui_UInt32(b, &ssp->msft.resolution[k].yres);
							 	icmSn_ui_UInt32(b, &ssp->msft.resolution[k].xres);
							}
						ICMSNFREEARRAY(b, ssp->_count, ssp->msft.resolution)
					} else if (icSigMsftMedia == ssp->msft.sig) {
						if (b->op == icmSnRead && ssp->size != 4)
							icmFormatWarning(icp, ICM_FMT_DSSIZE,
							   "DeviceSettings MsftMedia setting size mismatch %u != 4",
							    ssp->size);
						if (b->op == icmSnSize)
							ssp->size = 4;
						ssp->ssize = sizeof(icDevSetMsftMedia);
						if (icmArrayRdAllocResize(b, icmAResizeByCount, &ssp->_count, &ssp->count,
						    (void **)&ssp->msft.media, ssp->ssize,
						    scp->size-4, 0, 4, "icmDeviceSettings"))
							return;
						if (b->op & icmSnSerialise)		/* (Optimise for speed) */
							for (k = 0; k < ssp->count; k++)
								icmSn_DevSetMsftMedia32(b, &ssp->msft.media[k]);
						ICMSNFREEARRAY(b, ssp->_count, ssp->msft.media);
					} else if (icSigMsftHalftone == ssp->msft.sig) {
						if (b->op == icmSnRead && ssp->size != 4)
							icmFormatWarning(icp, ICM_FMT_DSSIZE,
							   "DeviceSettings MsftDither setting size mismatch %u != 4",
							    ssp->size);
						if (b->op == icmSnSize)
							ssp->size = 4;
						ssp->ssize = sizeof(icDevSetMsftDither);
						if (icmArrayRdAllocResize(b, icmAResizeByCount, &ssp->_count, &ssp->count,
						    (void **)&ssp->msft.halftone, ssp->ssize,
						    scp->size-4, 0, 4, "icmDeviceSettings"))
							return;
						if (b->op & icmSnSerialise)		/* (Optimise for speed) */
							for (k = 0; k < ssp->count; k++)
								icmSn_DevSetMsftDither32(b, &ssp->msft.halftone[k]); 
						ICMSNFREEARRAY(b, ssp->_count, ssp->msft.halftone)
					} else {
						/* User has set ssize for unknown */
						if (b->op == icmSnRead)
							ssp->ssize = ssp->size;		/* Set unknown byte size on read */
						if (b->op == icmSnSize)
							ssp->size = ssp->ssize;		/* Caller set size */
						if (icmArrayRdAllocResize(b, icmAResizeByCount, &ssp->_count, &ssp->count,
						    (void **)&ssp->msft.unknown, ssp->ssize,
						    scp->size-4, 0, ssp->size, "icmDeviceSettings"))
							return;
						if (b->op & icmSnSerialise) {	/* (Optimise for speed) */
							for (k = 0; k < ssp->count; k++)
								for (m = 0; m < ssp->ssize; m++)
									icmSn_uc_UInt8(b, &ssp->msft.unknown[k * ssp->size + m]); 
						}
						ICMSNFREEARRAY(b, ssp->_count, ssp->msft.unknown)
					}
				} else {
					icmSn_ui_UInt32(b, &ssp->unknown.sig);
					icmSn_ui_UInt32(b, &ssp->size);	/* Per setting size */
					icmSn_ui_UInt32(b, &ssp->count);

					/* User has set ssize for unknown */
					if (b->op == icmSnRead)
						ssp->ssize = ssp->size;		/* Set unknown byte size on read */
					if (b->op == icmSnSize)
						ssp->size = ssp->ssize;
					if (icmArrayRdAllocResize(b, icmAResizeByCount, &ssp->_count, &ssp->count,
					    (void **)&ssp->unknown.unknown, ssp->ssize,
					    scp->size-4, 0, ssp->size, "icmDeviceSettings"))
						return;
					if (b->op & icmSnSerialise) {	/* (Optimise for speed) */
						for (k = 0; k < ssp->count; k++)
							for (m = 0; m < ssp->ssize; m++)
								icmSn_uc_UInt8(b, &ssp->unknown.unknown[k * ssp->size + m]); 
					}
					ICMSNFREEARRAY(b, ssp->_count, ssp->unknown.unknown)
				}
			}
			if (b->op == icmSnRead && scp->size != (b->get_off(b) - sc_mark))
				icmFormatWarning(icp, ICM_FMT_DSSIZE,
				   "DeviceSettings sub-structure size mismatch %u != %u",
				                                    scp->size, b->get_off(b) - sc_mark);
			if (b->op == icmSnSize)		/* Compute size of setting combination sub-structure */
				scp->size = b->get_off(b) - sc_mark;
			ICMSNFREEARRAY(b, scp->_count, scp->data)
		}
		if (b->op == icmSnRead && pep->size != (b->get_off(b) - pe_mark))
			icmFormatWarning(icp, ICM_FMT_DSSIZE,
			   "DeviceSettings platform entry size mismatch %u != %u",
			                                    pep->size, b->get_off(b) - pe_mark);
		if (b->op == icmSnSize)		/* Compute size of platform entry sub-structure */
			pep->size = b->get_off(b) - pe_mark;
		ICMSNFREEARRAY(b, pep->_count, pep->data)
	}
	ICMRDCHECKCONSUMED(icmDeviceSettings)
	ICMSNFREEARRAY(b, p->_count, p->data)
}

/* Dump a text description of the object */
static void icmDeviceSettings_dump(icmDeviceSettings *p, icmFile *op, int verb) {
	unsigned int h, i, j, k, m;

	if (verb <= 0)
		return;

	op->printf(op,"DeviceSettings:\n");
	op->printf(op,"  No. platforms = %u\n",p->count);

	for (h = 0; h < p->count; h++) {
		icmPlatformEntry *pep = &p->data[h];
		op->printf(op,"    Platform = %s\n",icmPlatformSig2str(pep->platform));
		op->printf(op,"    No. setting combinations = %u\n",pep->count);
		if (verb <= 1)
			continue;
		for (i = 0; i < pep->count; i++) {
			icmSettingComb *scp = &pep->data[i];
			op->printf(op,"      Setting combination %u\n",i+1);
			op->printf(op,"      No. settings = %u\n",scp->count);
			for (j = 0; j < scp->count; j++) {
				icmSettingStruct *ssp = &scp->data[j];
				if (pep->platform == icSigMicrosoft) {
					if (icSigMsftResolution == ssp->msft.sig) {
						op->printf(op,"        No. of Microsoft Resolution values = %u: \n",
				                                                                   ssp->count);
						for (k = 0; k < ssp->count; k++)
							op->printf(op,"          %u: X = %u, Y = %u\n",
						                                    k+1, ssp->msft.resolution[k].xres,
						                                         ssp->msft.resolution[k].yres);
					} else if (icSigMsftMedia == ssp->msft.sig) {
						op->printf(op,"        No. of Microsoft Media values = %u: \n", ssp->count);
						for (k = 0; k < ssp->count; k++)
							op->printf(op,"          %u: '%s'\n", k+1, icmDevSetMsftMedia2str(
							                                           ssp->msft.media[k]));
					} else if (icSigMsftHalftone == ssp->msft.sig) {
						op->printf(op,"        No. of Microsoft Halftone values = %u: \n",
				                                                            k+1, ssp->count);
						for (k = 0; k < ssp->count; k++)
							op->printf(op,"          %u: '%s'\n", k+1, icmDevSetMsftDither2str(
							                                       ssp->msft.halftone[k]));
					} else {
						op->printf(op,"        No. of Microsoft unknown values = %u, size %u: \n",
						                                                ssp->count, ssp->ssize);
						// ~~~ replace with data dump ?
						for (k = 0; k < ssp->count; k++)
							for (m = 0; m < ssp->ssize; m++)
							op->printf(op,"          %u[%u]: 0x%x\n",
								k+1, m+1, ssp->msft.unknown[k * ssp->size + m]); 
					}
				} else {
					op->printf(op,"        Unknown sig = %s\n", icmtag2str(ssp->unknown.sig));
					op->printf(op,"        No. of Unknown values = %u, size %u: \n",
					                                           ssp->count, ssp->ssize);
					for (k = 0; k < ssp->count; k++)
						// ~8 replace with data dump ?
						for (m = 0; m < ssp->ssize; m++)
							op->printf(op,"          %u[%u]: 0x%x\n",
								k+1, m+1, ssp->unknown.unknown[k * ssp->size + m]); 
				}
			}
		}
	}
}

/* Check tags validity */
static int icmDeviceSettings_check(icmDeviceSettings *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmDeviceSettings(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmDeviceSettings, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* Signature */

static void icmSignature_serialise(icmSignature *p, icmFBuf *b) {
	icc *icp = p->icp;
	unsigned int h, i, j, k, m;

	/* Device Settings */
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3:  Signature Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7:  Zero padding */

	icmSn_Sig32(b, &p->sig);				/* 8-11: Signature */
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
	switch (p->creatorsig) {
		case icSigTechnologyTag:
			op->printf(op,"  Technology = %s\n", icmTechnologySig2str(p->sig));
			break;

		default:
			op->printf(op,"  Sig = %s\n", icmtag2str(p->sig));
			break;
	}
}

/* Check tags validity */
static int icmSignature_check(icmSignature *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmSignature(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmSignature, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmScreening object */

/* Serialise this tag type */
static void icmScreening_serialise(icmScreening *p, icmFBuf *b) {
	unsigned int i;

    icmSn_TagTypeSig32(b, &p->ttype);			/* 0-3: Screening Tag Type signature */
	icmSn_pad(b, 4);							/* 4-7: Zero padding */
	icmSn_Screening32(b, &p->screeningFlags);	/* 8-11: Screening flags */
	icmSn_ui_UInt32(b, &p->channels);			/* 12-15: Number of device channels */

	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_channels, &p->channels,
	    (void **)&p->data, sizeof(icmScreeningData),
	    UINT_MAX, 0, 12, "icmScreening"))
		return;
	if (b->op & icmSnSerialise) {				/* (Optimise for speed) */
		for (i = 0; i < p->channels; i++) {
			icmSn_d_S15Fix16(b, &p->data[i].frequency);	/* Frequency */
			icmSn_d_S15Fix16(b, &p->data[i].angle);		/* Angle */
			icmSn_SpotShape32(b, &p->data[i].spotShape);	/* Spot shape */
		}
	}
	ICMSNFREEARRAY(p, p->_channels, p->data)
	ICMRDCHECKCONSUMED(icmScreening)
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
	op->printf(op,"  Flags = %s\n", icmScreenEncodings2str(p->screeningFlags));
	op->printf(op,"  No. channels = %u\n",p->channels);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->channels; i++) {
			op->printf(op,"    %u:\n",i);
			op->printf(op,"      Frequency:  %f\n",p->data[i].frequency);
			op->printf(op,"      Angle:      %f\n",p->data[i].angle);
			op->printf(op,"      Spot shape: %s\n", icmSpotShape2str(p->data[i].spotShape));
		}
	}
}

/* Dump a text description of the object */
/* Check tags validity */
static int icmScreening_check(icmScreening *p, icTagSignature sig, int rd) {
	icc *icp = p->icp;
	unsigned int dchan = icmCSSig2nchan(icp->header->colorSpace);

	/* Does number of channels match header device space encoding ? */
	if (p->channels != dchan) {
		icmFormatWarning(icp, ICM_FMT_CHRMCHAN,
		                       "Screening no. channels %u doesn't match header %u",
		                                                              p->channels,dchan);
	}
	return icp->e.c;
}

/* Create an empty object. Return null on error */
icmBase *new_icmScreening(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmScreening, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUcrBg object */

static void icmUcrBg_serialise(icmUcrBg *p, icmFBuf *b) {
	icc *icp = p->icp;
	unsigned int i;

	/* Device Settings */
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3:  UcrBg Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7:  Zero padding */

	icmSn_ui_UInt32(b, &p->UCRcount);		/* 8-11: Count of UCR entries */
	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_UCRcount, &p->UCRcount,
	    (void **)&p->UCRcurve, sizeof(double),
	    UINT_MAX, 0, 2, "icmUcrBg"))
		return;
	if (b->op & icmSnSerialise) {			/* (Optimise for speed) */
		if (p->UCRcount == 1) {
			icmSn_d_RFix16(b, &p->UCRcurve[0]); 
		} else {
			for (i = 0; i < p->UCRcount; i++) {
				icmSn_d_NFix16(b, &p->UCRcurve[i]); 
			}
		}
	}
	ICMSNFREEARRAY(b, p->_UCRcount, p->UCRcurve)

	icmSn_ui_UInt32(b, &p->BGcount);		/* 8-11: Count of BG entries */
	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_BGcount, &p->BGcount,
	    (void **)&p->BGcurve, sizeof(double),
	    UINT_MAX, 0, 2, "icmUcrBg"))
		return;
	if (b->op & icmSnSerialise) {			/* (Optimise for speed) */
		if (p->BGcount == 1) {
			icmSn_d_RFix16(b, &p->BGcurve[0]); 
		} else {
			for (i = 0; i < p->BGcount; i++)
				icmSn_d_NFix16(b, &p->BGcurve[i]); 
		}
	}
	ICMSNFREEARRAY(b, p->_BGcount, p->BGcurve)

	p->fcount = b->get_space(b);			/* Implied */
	icmSn_ASCIIZ(b, &p->_count, &p->count, &p->string,
	                &p->fcount, 0, "icmUcrBg");

	ICMRDCHECKCONSUMED(icmUcrBg)
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
		op->printf(op,"    No. chars = %u\n",p->count);
	
		size = p->count > 0 ? p->count-1 : 0;
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->printf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->printf(op,"      ...\n");
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

/* Check tags validity */
static int icmUcrBg_check(icmUcrBg *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUcrBg(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmUcrBg, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* VideoCardGamma 'vcgt' (ColorSync 2.5 specific) */

static void icmVideoCardGamma_serialise(icmVideoCardGamma *p, icmFBuf *b) {
	icc *icp = p->icp;

	/* Device Settings */
    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3:  VideoCardGamma Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7:  Zero padding */

	icmSn_VideoCardGammaFormat32(b, &p->tagType);	/* 8-11: Table or Formula  */

	/* This will probably go badly wrong if client alloc's and then */
	/* changes tagType, writes table then changes back and re-alloc's/free's.... */
	/* It will also leak if >entryCount is changes after alloc'ing and */
	/* before free'ing. */

	if (p->tagType == icVideoCardGammaTable) {
		unsigned int c, i;

		icmSn_check_ui_UInt16(b, &p->u.table.channels, 3);
		icmSn_ui_UInt16(b, &p->u.table.entryCount);
		icmSn_ui_UInt16(b, &p->u.table.entrySize);

		if ((b->op & icmSnAlloc) || b->op == icmSnFree) {
			for (c = 0; c < p->u.table.channels; c++) { 
				if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->u.table._entryCount[c],
				    &p->u.table.entryCount, (void **)&p->u.table.data[c], sizeof(double),
				    UINT_MAX, 0, p->u.table.entrySize, "icmVideoCardGamma"))
					return;
			
				ICMSNFREEARRAY(b, p->u.table._entryCount[c], p->u.table.data[c])
			}
		}

		if (p->u.table.entrySize == 1) {
			for (c = 0; c < p->u.table.channels; c++)
				for (i = 0; i < p->u.table.entryCount; i++)
					icmSn_d_NFix8(b, &p->u.table.data[c][i]);
		} else if (p->u.table.entrySize == 2) {
			for (c = 0; c < p->u.table.channels; c++)
				for (i = 0; i < p->u.table.entryCount; i++)
					icmSn_d_NFix16(b, &p->u.table.data[c][i]);
		} else {
			icmFormatWarning(icp, ICM_FMT_VCGT_ENTRYSZ,"Unknown VideoCardGamma table entry size %d",p->u.table.entrySize);
			return;
		}

	} else if (p->tagType == icVideoCardGammaFormula) {
		unsigned int i;

		for (i = 0; i < 3; i++) {
			icmSn_d_S15Fix16(b, &p->u.formula.gamma[i]);
			icmSn_d_S15Fix16(b, &p->u.formula.min[i]);
			icmSn_d_S15Fix16(b, &p->u.formula.max[i]);
		}
	} else {
		icmFormatWarning(icp, ICM_FMT_VCGT_FORMAT,"Unknown VideoCardGamma format %d",p->tagType);
		return;
	}

	ICMRDCHECKCONSUMED(icmVideoCardGamma)
}

/* Dump a text description of the object */
static void icmVideoCardGamma_dump(
	icmVideoCardGamma *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	unsigned int c, i;

	if (verb <= 0)
		return;

	if (p->tagType == icVideoCardGammaTable) {
		op->printf(op,"VideoCardGammaTable:\n");
		op->printf(op,"  channels  = %d\n", p->u.table.channels);
		op->printf(op,"  entries   = %d\n", p->u.table.entryCount);
		op->printf(op,"  entrysize = %d\n", p->u.table.entrySize);
		if (verb >= 2) {
			for (c = 0; c < p->u.table.channels; c++) {
				op->printf(op,"  channel #%d\n",c);
				for (i = 0; i < p->u.table.entryCount; i++) {
					op->printf(op,"    %d: %f\n",i, p->u.table.data[c][i]);
				}
			}
		}
	} else if (p->tagType == icVideoCardGammaFormula) {
		char *cname[3] = { "red", "green", "blue" };
		op->printf(op,"VideoCardGammaFormula:\n");
		for (i = 0; i < 3; i++) {
			op->printf(op,"  %s gamma   = %.8f\n", cname[i],p->u.formula.gamma[i]);
			op->printf(op,"  %s min     = %.8f\n", cname[i],p->u.formula.min[i]);
			op->printf(op,"  %s max     = %.8f\n", cname[i],p->u.formula.max[i]);
		}
	} else {
		op->printf(op,"  Unknown tag format\n");
	}
}

/* Translate a value throught the gamma lookup */
static double icmVideoCardGamma_lookup(
	icmVideoCardGamma *p,
	int chan,		/* Channel, 0, 1 or 2 */
	double iv		/* Input value 0.0 - 1.0 */
) {
	double ov = 0.0;

	if (chan < 0 || chan > (p->u.table.channels-1)
	 || iv < 0.0 || iv > 1.0)
		return iv;

	if (p->tagType == icVideoCardGammaTable && p->u.table.entryCount == 0) {
		/* Deal with siliness */
		ov = iv;
	} else if (p->tagType == icVideoCardGammaTable) {
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
		val0 = p->u.table.data[chan][ix + 0];
		val1 = p->u.table.data[chan][ix + 1];
		ov = val0 + w * (val1 - val0);

	} else if (p->tagType == icVideoCardGammaFormula) {

		/* The Apple OSX doco confirms this is the formula: */
		ov = p->u.formula.min[chan] + (p->u.formula.max[chan] - p->u.formula.min[chan])
		                            * pow(chan, p->u.formula.gamma[chan]);
	}
	return ov;
}

/* Check tags validity */
static int icmVideoCardGamma_check(icmVideoCardGamma *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmVideoCardGamma(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmVideoCardGamma, ttype)
	p->lookup = icmVideoCardGamma_lookup;
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* ViewingConditions */

static void icmViewingConditions_serialise(icmViewingConditions *p, icmFBuf *b) {
	unsigned int n;

    icmSn_TagTypeSig32(b, &p->ttype);				/* 0-3: ViewingConditions Tag Type signature */
	icmSn_pad(b, 4);								/* 4-7: Zero padding */

	icmSn_XYZNumber12b(b, &p->illuminant);				/* 8-19: CIEXYZ of measurement backing */
	icmSn_XYZNumber12b(b, &p->surround);				/* 20-31: CIEXYZ of measurement backing */
	icmSn_PredefinedIlluminant32(b, &p->stdIlluminant); /* 32-35: Encoded standard illuminant */

	ICMRDCHECKCONSUMED(icmViewingConditions)
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

/* Check ViewingConditions */
static int icmViewingConditions_check(icmViewingConditions *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmViewingConditions(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmViewingConditions, ttype)
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmCrdInfo object, postscript Color Rendering Dictionary names type */

/* Serialise this tag type */
static void icmCrdInfo_serialise(icmCrdInfo *p, icmFBuf *b) {
	unsigned int i;

    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3:  CrdInfo Tag Type signature */
	icmSn_pad(b, 4);						/* 4-7:  Zero padding */


	icmSn_ui_UInt32(b, &p->fppcount);		/* 8-11: Number of product name characters */
	icmSn_ASCIIZ(b, &p->_ppcount, &p->ppcount, &p->ppname,
	                &p->fppcount, 0, "icmCrdInfo");


	for (i = 0; i < 4; i++) {
		icmSn_ui_UInt32(b, &p->fcrdcount[i]);		/* Number of crdN name characters */
		icmSn_ASCIIZ(b, &p->_crdcount[i], &p->crdcount[i], &p->crdname[i],
		                &p->fcrdcount[i], 0, "icmCrdInfo");
	}

	ICMRDCHECKCONSUMED(icmCrdInfo)
}

/* Dump a text description of the object */
static void icmCrdInfo_dump(
	icmCrdInfo *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	unsigned int i;

	if (verb <= 0)
		return;

	op->printf(op,"PostScript Product name and CRD names:\n");

	op->printf(op,"  Product name:\n");
	icmASCIIZ_dump(p->ppname, p->ppcount, op, verb, p->dp + 2);

	for (i = 0; i < 4; i++) {	/* For all 4 intents */
		op->printf(op,"  CRD%d name:\n",i);
		icmASCIIZ_dump(p->crdname[i], p->crdcount[i], op, verb, p->dp + 2);
	}
}

/* Check CrdInfo */
static int icmCrdInfo_check(icmCrdInfo *p, icTagSignature sig, int rd) {
	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmCrdInfo(icc *icp, icTagTypeSignature ttype) {
	ICM_BASE_ALLOCINIT(icmCrdInfo, ttype)
	return (icmBase *)p;
}

/* ======================================== */

/* icmPe transform implemenntation */
#include "icc_xf.c"

/* icmProcessing Element implementation */
#include "icc_pe.c"

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
    icmSn_ui_UInt32(b, &p->cmmId);				/* 4-7:  CMM for profile */
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
    icmSn_ui_UInt32(b, &p->manufacturer);		/* 48-51: Dev manufacturer */
    icmSn_ui_UInt32(b, &p->model);				/* 52-55: Dev model */
    icmSn_DeviceAttributes64(b, &p->attributes);/* 56-63: Device attributes */
	if (b->op == icmSnWrite) {
		p->extraRenderingIntent = (~0xffff & p->extraRenderingIntent)
		                         | (0xffff & p->renderingIntent);
	}
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
    icmSn_ui_UInt32(b, &p->creator);			/* 80-83: Profile creator */

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

	if (b->op == icmSnRead
	 && p->icp->e.c == ICM_ERR_OK
	 && p->vers.majv >= 4)
		fprintf(stderr,"Warning: ICC V4 not supported!\n");
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
	op->printf(op,"  Device Class    = %s\n", icmProfileClassSig2str(p->deviceClass));
	op->printf(op,"  Color Space     = %s\n", icmColorSpaceSig2str(p->colorSpace));
	op->printf(op,"  Conn. Space     = %s\n", icmColorSpaceSig2str(p->pcs));
	op->printf(op,"  UTC Date&Time   = %s\n", icmDateTimeNumber2str(&p->date));
	icmDateTimeNumber_tolocal(&l, &p->date);
	op->printf(op,"  Local Date&Time = %s\n", icmDateTimeNumber2str(&l));
	op->printf(op,"  Platform        = %s\n", icmPlatformSig2str(p->platform));
	op->printf(op,"  Flags           = %s\n", icmProfileHeaderFlags2str(p->flags));
	op->printf(op,"  Dev. Mnfctr.    = %s\n", icmDeviceManufacturerSig2str(p->manufacturer));
	op->printf(op,"  Dev. Model      = %s\n", icmDeviceModelSig2str(p->model));
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

static int icmHeader_check(icmHeader *p, icTagSignature sig, int rd) {
	// ~8 should check that size is multiple of 4 
	// ~8 add header check code here. Move in-line check code to here ???
	return p->icp->e.c;
}

static void icmHeader_del(icmHeader *p) {
	p->icp->al->free(p->icp->al, p);
}

/* Create an empty object. On error, set icp->e and return NULL */
static icmHeader *new_icmHeader(icc *icp) {
	int i;

	/* Allocate *p, and do base class init */
	ICM_BASE_ALLOCINIT(icmHeader, 0)

	/* Characteristic values */
	p->hsize = 128;		/* Header is 128 bytes for ICC */

	/* Values that must be set before writing */
	p->deviceClass = icMaxEnumClass;/* Type of profile - must be set! */
    p->colorSpace = icMaxEnumColorSpace;	/* Clr space of data - must be set! */
    p->pcs = icMaxEnumColorSpace;			/* PCS: XYZ or Lab - must be set! */
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

const icmTVRange icmtvrange_20_plus = ICMTVRANGE_20_PLUS;
const icmTVRange icmtvrange_21_plus = ICMTVRANGE_21_PLUS;
const icmTVRange icmtvrange_22_plus = ICMTVRANGE_22_PLUS;
const icmTVRange icmtvrange_23_plus = ICMTVRANGE_23_PLUS;
const icmTVRange icmtvrange_24_plus = ICMTVRANGE_24_PLUS;
const icmTVRange icmtvrange_40_plus = ICMTVRANGE_40_PLUS;
const icmTVRange icmtvrange_41_plus = ICMTVRANGE_41_PLUS;
const icmTVRange icmtvrange_42_plus = ICMTVRANGE_42_PLUS;
const icmTVRange icmtvrange_43_plus = ICMTVRANGE_43_PLUS;
const icmTVRange icmtvrange_44_plus = ICMTVRANGE_44_PLUS;


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
char *icmTVersRange2str(const icmTVRange *tvr) {
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
int icmVersInRange(struct _icc *icp, const icmTVRange *tvr) {
	int rv;
	icmTV icctv = ICMVERS2TV(icp->header->vers);
	rv = ICMTV_IN_RANGE(icctv, tvr);
	return rv;
}

/* Return true if the two version ranges have an overlap */
int icmVersOverlap(icmTVRange *tvr1, const icmTVRange *tvr2) {
	return (tvr1->min <= tvr2->max && tvr2->min <= tvr1->max);
}

/* Return a string that shows the icc version */
char *icmProfileVers2str(struct _icc *icp) {
	icmTV icctv = ICMVERS2TV(icp->header->vers);
	return icmTVers2str(icctv);
}

/* Table of Tag Types valid version range and constructor. */
/* Last has ttype icMaxEnumTagType */
static icmTagTypeVersConstrRec icmTagTypeTable[] = {
	{icSigChromaticityType,         ICMTVRANGE_23_PLUS,			new_icmChromaticity},
	{icSigCrdInfoType,              {ICMTV_21, ICMTV_40},		new_icmCrdInfo},
	{icSigCurveType,                ICMTVRANGE_20_PLUS,			new_icmPeCurve},
	{icSigDataType,                 ICMTVRANGE_20_PLUS,			new_icmData},
	{icSigDateTimeType,             ICMTVRANGE_20_PLUS,			new_icmDateTime},
	{icSigDeviceSettingsType,		{ICMTV_22, ICMTV_40},		new_icmDeviceSettings},
	{icSigLut16Type,				ICMTVRANGE_20_PLUS,			new_icmLut1},
	{icSigLut8Type,					ICMTVRANGE_20_PLUS,			new_icmLut1},
	{icSigMeasurementType,			ICMTVRANGE_20_PLUS,			new_icmMeasurement},
	{icSigNamedColorType,			{ICMTV_20, ICMTV_24},		new_icmNamedColor},
	{icSigNamedColor2Type,          ICMTVRANGE_20_PLUS,			new_icmNamedColor},
	{icSigProfileSequenceDescType,	ICMTVRANGE_20_PLUS,			new_icmProfileSequenceDesc},
	{icSigResponseCurveSet16Type,	ICMTVRANGE_22_PLUS,			new_icmResponseCurveSet16},
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
	/* This is really ICC V4, but icclibv2 had suport for it... */
	{icSigColorantTableType,        ICMTVRANGE_40_PLUS,			new_icmColorantTable},
	{icmSigAltColorantTableType,    ICMTVRANGE_40_PLUS,			new_icmColorantTable},
	/* Private aliases for Lut8 and Lut16 */
	{icmSig816Curves,				ICMTVRANGE_20_PLUS,			new_icmPeCurveSet},
	{icmSig816Curve,				ICMTVRANGE_20_PLUS,			new_icmPeCurve},
	{icmSig816Matrix,				ICMTVRANGE_20_PLUS,			new_icmPeMatrix},
	{icmSig816CLUT,					ICMTVRANGE_20_PLUS,			new_icmPeClut},
	{ icMaxEnumTagType }
};

/* Table of Tags valid version range and possible Tag Types. */
/* Version after each TagType is intersected with Tag Type version range. */ 
/* Last has sig icMaxEnumTag */
static icmTagSigVersTypesRec icmTagSigTable[] = {

	{icSigAToB0Tag,	ICMTVRANGE_20_PLUS,	icmTPLutFwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigAToB1Tag,	ICMTVRANGE_20_PLUS,	icmTPLutFwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigAToB2Tag,	ICMTVRANGE_20_PLUS,	icmTPLutFwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigBlueMatrixColumnTag,	ICMTVRANGE_20_PLUS,	icmTPNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigBlueTRCTag, 			ICMTVRANGE_20_PLUS, icmTPNone,				
											{{ icSigCurveType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigBToA0Tag,	ICMTVRANGE_20_PLUS,	icmTPLutBwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigBToA1Tag,	ICMTVRANGE_20_PLUS,	icmTPLutBwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigBToA2Tag,	ICMTVRANGE_20_PLUS,	icmTPLutBwd,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigCalibrationDateTimeTag,	ICMTVRANGE_20_PLUS,	icmTPNone,
											{{ icSigDateTimeType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigCharTargetTag,			ICMTVRANGE_20_PLUS,	icmTPNone,
											{{ icSigTextType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigChromaticAdaptationTag,	ICMTVRANGE_24_PLUS, icmTPNone,
											{{ icSigS15Fixed16ArrayType,	ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigChromaticityTag,			ICMTVRANGE_23_PLUS, icmTPNone,
											{{ icSigChromaticityType,		ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigColorantTableTag,         ICMTVRANGE_40_PLUS,	icmTPNone,
											{{ icSigColorantTableType,		ICMTVRANGE_ALL },
											 { icmSigAltColorantTableType,	ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigColorantTableOutTag,      ICMTVRANGE_40_PLUS, icmTPNone,
											{{ icSigColorantTableType,		ICMTVRANGE_ALL },
											 { icmSigAltColorantTableType,	ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigCopyrightTag,				ICMTVRANGE_20_PLUS, icmTPNone,
											{{ icSigTextType,			{ ICMTV_20, ICMTV_24 }},
											 { icMaxEnumTagType}}},
	{icSigCrdInfoTag,				{ ICMTV_21, ICMTV_40 },	icmTPNone,
											{{ icSigCrdInfoType,			ICMTVRANGE_ALL },		
											 { icMaxEnumTagType}}},
#ifdef NEVER	/* Not clear if these two were every official... */
	{icSigDataTag,					{ ICMTV_20, ICMTV_24 },	icmTPNone,
											{{ icSigDataType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigDateTimeTag,				{ ICMTV_20, ICMTV_24 },	icmTPNone,
											{{ icSigDataType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
#endif
	{icSigDeviceMfgDescTag,			ICMTVRANGE_20_PLUS, icmTPNone,
											{{ icSigTextDescriptionType,	{ ICMTV_20, ICMTV_24 }},
											 { icMaxEnumTagType}}},
	{icSigDeviceModelDescTag,		ICMTVRANGE_20_PLUS, icmTPNone,
											{{ icSigTextDescriptionType,	{ ICMTV_20, ICMTV_24 }},
											 { icMaxEnumTagType}}},
	{icSigDeviceSettingsTag,		{ ICMTV_22, ICMTV_40 },	icmTPNone,
											{{ icSigDeviceSettingsType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigGamutTag,		ICMTVRANGE_20_PLUS,	icmTPLutGamut,
											{{ icSigLut16Type,				ICMTVRANGE_ALL },
											 { icSigLut8Type,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigGrayTRCTag,	ICMTVRANGE_20_PLUS, icmTPNone,
											{{ icSigCurveType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigGreenMatrixColumnTag,	ICMTVRANGE_20_PLUS,	icmTPNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigGreenTRCTag, 			ICMTVRANGE_20_PLUS, icmTPNone,				
											{{ icSigCurveType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigLuminanceTag,			ICMTVRANGE_20_PLUS, icmTPNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigMeasurementTag,		ICMTVRANGE_20_PLUS, icmTPNone,
											{{ icSigMeasurementType,		ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigMediaBlackPointTag,	{ ICMTV_20, ICMTV_42 }, icmTPNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigMediaWhitePointTag,	ICMTVRANGE_20_PLUS, icmTPNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigNamedColorTag,		{ ICMTV_20, ICMTV_20 }, icmTPNone,
											{{ icSigNamedColorType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigNamedColor2Tag,		ICMTVRANGE_21_PLUS, icmTPNone,
											{{ icSigNamedColor2Type,		ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigOutputResponseTag,	ICMTVRANGE_22_PLUS, icmTPNone,
											{{ icSigResponseCurveSet16Type,	ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigPreview0Tag,		ICMTVRANGE_20_PLUS,	icmTPLutPreview,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigPreview1Tag,		ICMTVRANGE_20_PLUS,	icmTPLutPreview,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigPreview2Tag,		ICMTVRANGE_20_PLUS,	icmTPLutPreview,
											{{ icSigLut16Type,			ICMTVRANGE_ALL },
											 { icSigLut8Type,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigProfileDescriptionTag,		ICMTVRANGE_20_PLUS, icmTPNone,
											{{ icSigTextDescriptionType,		ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigProfileSequenceDescTag,		ICMTVRANGE_20_PLUS, icmTPNone,
											{{ icSigProfileSequenceDescType,	ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigPs2CRD0Tag,				{ ICMTV_20, ICMTV_40 }, icmTPNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigPs2CRD1Tag,				{ ICMTV_20, ICMTV_40 },	icmTPNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigPs2CRD2Tag,				{ ICMTV_20, ICMTV_40 },	icmTPNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigPs2CRD3Tag,				{ ICMTV_20, ICMTV_40 },	icmTPNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigPs2CSATag,				{ ICMTV_20, ICMTV_40 },	icmTPNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigPs2RenderingIntentTag,	{ ICMTV_20, ICMTV_40 },	icmTPNone,
											{{ icSigDataType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigRedMatrixColumnTag,	ICMTVRANGE_20_PLUS,	icmTPNone,
											{{ icSigXYZType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigRedTRCTag, 			ICMTVRANGE_20_PLUS, icmTPNone,				
											{{ icSigCurveType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType}}},
	{icSigScreeningDescTag,		{ ICMTV_20, ICMTV_40 }, icmTPNone,
											{{ icSigTextDescriptionType,	ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigScreeningTag,			{ ICMTV_20, ICMTV_40 },	icmTPNone,
											{{ icSigScreeningType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigTechnologyTag,		ICMTVRANGE_20_PLUS,	icmTPNone,
											{{ icSigSignatureType,			ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigUcrBgTag,				{ ICMTV_20, ICMTV_40 },	icmTPNone,
											{{ icSigUcrBgType,				ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigVideoCardGammaTag,	ICMTVRANGE_20_PLUS,	icmTPNone,
											{{ icSigVideoCardGammaType,		ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigViewingCondDescTag,		{ ICMTV_20, ICMTV_40 }, icmTPNone,
											{{ icSigTextDescriptionType,	ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icSigViewingConditionsTag,	ICMTVRANGE_20_PLUS,	icmTPNone,
											{{ icSigViewingConditionsType,	ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icmSigAbsToRelTransSpace,	ICMTVRANGE_20_PLUS, icmTPNone,
											{{ icSigS15Fixed16ArrayType,	ICMTVRANGE_ALL },
											 { icMaxEnumTagType }}},
	{icMaxEnumTag}
};

/*
	Additional tag requirement that can't be coded in icmClassTagTable:

	if (icSigMediaWhitePointTag) and if data is adopted white is not D50,
	then shall have icSigChromaticAdaptationTag.
	icSigChromaticAdaptationTag is currentlty listed in the optional list.

*/

/* Table of Class Signature instance requirements. */
/* All profiles shall have the required tags, and may have the optional tags. */
/* If any have tags in the icmTransTagTable[] list and are not optional, */
/* then this is a warning. */
/* Need to check that profile has all tags of at least one matching record. */
/* Matching record has matching Class Sig + Src + Dst color signatures within version range, */
/* Tags are only required if in Tag version + icmTagSigVersRec.vrange. */
/* Last has sig icMaxEnumClass */
/* A profile may match more than one record, so all matching records */
/* need to be checked. Order is specific to general, so that any error/warning */
/* is the most general one. */
static icmRequiredTagType icmClassTagTable[] = {
    {icSigInputClass,			ICMCSMR_ANY(1),	ICMCSMR_PCS,	ICMTVRANGE_ALL, 
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigGrayTRCTag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL }, 
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigChromaticAdaptationTag, 	ICMTVRANGE_40_PLUS },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA0Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icMaxEnumTag }
		}
	},

    {icSigInputClass,			ICMCSMR_ANY(3),	ICMCSMR_XYZ,	ICMTVRANGE_ALL, 
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
		 { icSigChromaticAdaptationTag, 	ICMTVRANGE_40_PLUS },
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

    {icSigInputClass,     		ICMCSMR_ANYN,	ICMCSMR_PCS,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag, 				ICMTVRANGE_ALL },
		 { icMaxEnumTag },
		}, {
		/* Optional: */
		 { icSigChromaticAdaptationTag, 	ICMTVRANGE_40_PLUS },
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA0Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigGamutTag,					ICMTVRANGE_23_PLUS },		/* Why ? */
		 { icMaxEnumTag }
		}
	},


    {icSigDisplayClass,			ICMCSMR_ANY(1),	ICMCSMR_PCS,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigGrayTRCTag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigChromaticAdaptationTag, 	ICMTVRANGE_40_PLUS },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA0Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icMaxEnumTag }
		}
	},

    {icSigDisplayClass,     	ICMCSMR_ANY(3),	ICMCSMR_XYZ,    ICMTVRANGE_ALL,
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
		 { icSigChromaticAdaptationTag, 	ICMTVRANGE_40_PLUS },
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

    {icSigDisplayClass,     	ICMCSMR_ANYN, 	ICMCSMR_PCS,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigBToA0Tag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigChromaticAdaptationTag, 	ICMTVRANGE_40_PLUS },
		 { icSigAToB1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigAToB2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA1Tag,					ICMTVRANGE_23_PLUS },
		 { icSigBToA2Tag,					ICMTVRANGE_23_PLUS },
		 { icSigGamutTag,					ICMTVRANGE_23_PLUS },
		 { icMaxEnumTag }
		}
	},


    {icSigOutputClass,			ICMCSMR_ANY(1),    ICMCSMR_PCS,    ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigGrayTRCTag,					ICMTVRANGE_ALL },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigChromaticAdaptationTag, 	ICMTVRANGE_40_PLUS },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigAToB1Tag,					ICMTVRANGE_ALL },
		 { icSigAToB2Tag,					ICMTVRANGE_ALL },
		 { icSigBToA0Tag,					ICMTVRANGE_ALL },
		 { icSigBToA1Tag,					ICMTVRANGE_ALL },
		 { icSigBToA2Tag,					ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}
	},

    {icSigOutputClass,			ICMCSMR_NOT_NCOL,	ICMCSMR_PCS,    ICMTVRANGE_ALL,
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
		 { icSigChromaticAdaptationTag, 	ICMTVRANGE_40_PLUS },
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
		 { icSigColorantTableTag,			ICMTVRANGE_40_PLUS },
		 { icSigMediaWhitePointTag,			ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icSigChromaticAdaptationTag, 	ICMTVRANGE_40_PLUS },
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
		 { icMaxEnumTag }
		}
	},

    {icSigLinkClass,		ICMCSMR_NCOL,	ICMCSMR_NOT_NCOL,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigColorantTableTag,			ICMTVRANGE_40_PLUS },
		 { icSigProfileSequenceDescTag,		ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icMaxEnumTag }
		}
	},

    {icSigLinkClass,		ICMCSMR_NOT_NCOL,	ICMCSMR_NCOL,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigColorantTableOutTag,		ICMTVRANGE_40_PLUS },
		 { icSigProfileSequenceDescTag,		ICMTVRANGE_ALL },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icMaxEnumTag }
		}
	},

    {icSigLinkClass,			ICMCSMR_NCOL,	ICMCSMR_NCOL,	ICMTVRANGE_ALL,
		/* Required: */
	 	{{ icSigProfileDescriptionTag,		ICMTVRANGE_ALL },
		 { icSigAToB0Tag,					ICMTVRANGE_ALL },
		 { icSigColorantTableTag,			ICMTVRANGE_40_PLUS },
		 { icSigProfileSequenceDescTag,		ICMTVRANGE_ALL },
		 { icSigColorantTableOutTag,		ICMTVRANGE_40_PLUS },
		 { icSigCopyrightTag,				ICMTVRANGE_ALL },
		 { icMaxEnumTag }
		}, {
		/* Optional: */
		 { icMaxEnumTag }
		}
	},

    {icSigColorSpaceClass,		ICMCSMR_ANYN,	ICMCSMR_PCS,    ICMTVRANGE_ALL,
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
		 { icMaxEnumTag }
		}
	},

    {icSigNamedColorClass,		ICMCSMR_ANYN,    ICMCSMR_PCS,    ICMTVRANGE_ALL,
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

    {icSigNamedColorClass,		ICMCSMR_ANYN,    ICMCSMR_PCS,    ICMTVRANGE_ALL,
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


	{ icMaxEnumTag },
};

/* Table of Types and valid Sub-Types to check against. */
/* (Note sub-types may be pseudo-type) */
/* Used by icc_new_ttype() */
/* Also used by icc_new_pe() starting at PESUBTAGTYPESTABLESTART entry */
static struct {
	icTagTypeSignature pttype;		/* Parent ttype */
	icTagTypeSignature ttypes[8];	/* Sub-ttypes */
} icmValidSubTagTypesTable[] = {
	{ icSigDictType, {icSigMultiLocalizedUnicodeType, icMaxEnumTagType} },

	{ icSigProfileSequenceDescType, {icmSigCommonTextDescriptionType,
	                                 icSigTextDescriptionType,
	                                 icMaxEnumTagType} },
	{ icSigProfileSequenceIdentifierType, {icmSigCommonTextDescriptionType,
	                                       icSigTextDescriptionType,
	                                       icMaxEnumTagType} },

	/* Pe sub tags. Tables used by icc_new_pe() from here: */
# define PESUBTAGTYPESTABLESTART icSigLut8Type
	{ icSigLut8Type, {icmSig816Matrix, icmSig816Curves, icmSig816CLUT, icMaxEnumTagType} },
	{ icSigLut16Type, {icmSig816Matrix, icmSig816Curves, icmSig816CLUT, icMaxEnumTagType} },
	{ icmSig816Curves, {icmSig816Curve, icMaxEnumTagType} },

	{ icMaxEnumTagType }
};

/* ===================================================================== */
/* icc profile object implementation */

/*

	~8 Loose ends:

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
	
	if (icmArrayAllocResize(b, &p->_count, &p->count, (void **)&p->data,
	                           sizeof(icmTagRec), "tagTable"))   
		return;

	/* For each table entry */
	for(i = 0; i < p->count; i++) {
		icmSn_TagSig32(b, &p->data[i].sig);		/* 0-3:  Signture */
		icmSn_ui_UInt32(b, &p->data[i].offset);	/* 4-7:  Tag offset */
		icmSn_ui_UInt32(b, &p->data[i].size);	/* 8-11: Tag size */
		if (b->op == icmSnRead) {
	    	p->data[i].pad = 0;				/* Not determined on read */
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
//				return icm_err(p, ICM_ERR_RD_FORMAT | ICM_FMT_TAGOLAP, "Tag %d overlaps tag %d",i,j);
				icmFormatWarning(p, ICM_FMT_TAGOLAP, "Tag %d overlaps tag %d",i,j);
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

static void icc_set_defaults(icc *p);

/* Return the reference to current read fp (if any) */
/* Delete after finished with it */
/* (Beware of any interactions with icc that is sharing it!) */
static icmFile *icc_get_rfp(icc *p) {
	if (p->rfp == NULL)
		return NULL;
	return p->rfp->reference(p->rfp);
}

/* Return file version in icmTV format, 0 on error */
icmTV icc_get_version(icc *p) {
	if (p->header == NULL) {
		icm_err(p, ICM_ERR_INTERNAL, "icc_get_version: No Header available");
		return ICMTV_MIN;
	}

	return ICMVERS2TV(p->header->vers);
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

	icc_set_defaults(p);

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
		}
	}

	return 0;
}


/*
	~8 Stuff that is missing ?

	From V2.2+:
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
	if (p->header->check(p->header, icmSigUnknown, 0) != ICM_ERR_OK)
		return p->e.c;
	
	/* Check tags and tagtypes */
	for (i = 0; i < p->count; i++) {	/* For all the tag element data */
		icTagTypeSignature ttype, uttype;

		if (p->data[i].objp == NULL)
			return icm_err(p, ICM_ERR_INTERNAL, "icc_write_check: NULL tag element");

		uttype = p->data[i].ttype;			/* Actual type */
		ttype = p->data[i].objp->ttype;		/* Actual or icmSigUnknownType */ 

		/* Do tag & tagtype check */
		if (icc_check_sig(p, NULL, 0, p->data[i].sig, ttype, uttype, p->data[i].objp->rdff)
			                                                                   != ICM_ERR_OK)
			return p->e.c;

		/* If there are shared tagtypes, check compatibility */
		if (p->data[i].objp->refcount > 1) {	/* Hint that it might be a link */
			int k;
			/* Search for shared tagtypes */
			for (k = 0; k < p->count; k++) {
				if (i == k || p->data[k].objp != p->data[i].objp)
					continue;

				/* Check the tagtype purposes are compatible */
				if (p->get_tag_lut_purpose(p, p->data[i].sig) != p->get_tag_lut_purpose(p, p->data[k].sig)) {
					return icm_err(p, ICM_ERR_PURPMISMATCH,
					     "icc_write_check: Tag '%s' is link to incompatible tag '%s'",
				          icmTagSig2str(p->data[i].sig), icmTagSig2str(p->data[k].sig));
				}
			}
		}

		/* Involke any other write checking for this tag */
		if (p->data[i].objp->check != NULL)
			p->data[i].objp->check(p->data[i].objp, p->data[i].sig, 0); 
		if (p->e.c != ICM_ERR_OK)
			return p->e.c;
	}

	/* Check that required tags are present */
	if ((p->cflags & icmCFlagNoRequiredCheck) == 0) {
		int quirk = 0;				/* nz if this is a quirk warning */
		icmErr e = { 0 };			/* Any warnings found */

		clsig  = p->header->deviceClass;
		colsig = p->header->colorSpace;
		dchans = icmCSSig2nchan(colsig);
		pcssig = p->header->pcs;
		pchans = icmCSSig2nchan(pcssig);

//	printf("dchans %d, pchans %d\n",dchans,pchans);

		/* Find a matching class entry. A profile may match more than one entry, */
		/* so we go through all matching entries looking for one */
		/* it is complient with, and noting any warnings for those it doesn't. */
		for (i = 0; p->classtagtable[i].clsig != icMaxEnumTagType; i++) {
			if (p->classtagtable[i].clsig == clsig
			 && icmCSMR_match(&p->classtagtable[i].colsig, colsig, dchans)
			 && icmCSMR_match(&p->classtagtable[i].pcssig, pcssig, pchans)
			 && icmVersInRange(p, &p->classtagtable[i].vrange)) {

				/* Found entry, so now check that all the required tags are present. */
				for (j = 0; p->classtagtable[i].tags[j].sig != icMaxEnumTagType; j++) {
					icTagSignature sig = p->classtagtable[i].tags[j].sig;

			 		if (!icmVersInRange(p, &p->classtagtable[i].tags[j].vrange))
						continue;		/* Only if applicable to this file version */

					if (p->find_tag(p, sig) != 0) {	/* Not present! */
						quirk = 0;
						e.c = ICM_FMT_REQUIRED_TAG;
						sprintf(e.m, "icc_write_check: deviceClass %s is missing required tag %s",
						               icmtag2str(clsig), icmtag2str(sig));
						break;
					}
				}

				/* we found an error */
				if (p->classtagtable[i].tags[j].sig != icMaxEnumTagType) {
					continue;		/* Look for more matching classes */
				}

				/* Found all required tags. */
				/* Check that any transtagtable[] tags are expected in this class */
				for (j = 0; p->transtagtable[j].sig != icMaxEnumTagType; j++) {
					icTagSignature sig = p->transtagtable[j].sig;

			 		if (!icmVersInRange(p, &p->transtagtable[j].vrange))
						continue;		/* Only if applicable to this file version */

					if (p->find_tag(p, sig) == 0) {

						/* See if this tag is in the required list */
						for (k = 0; p->classtagtable[i].tags[k].sig != icMaxEnumTagType; k++) {
					 		if (!icmVersInRange(p, &p->classtagtable[i].tags[k].vrange))
								continue;		/* Only if applicable to this file version */
							if (sig == p->classtagtable[i].tags[k].sig)
								break;			/* Found it */
						}
						if (p->classtagtable[i].tags[k].sig != icMaxEnumTagType) {
							continue;		/* Yes it is */
						}

						/* See if this tag is in the optional list */
						for (k = 0; p->classtagtable[i].otags[k].sig != icMaxEnumTagType; k++) {
					 		if (!icmVersInRange(p, &p->classtagtable[i].otags[k].vrange))
								continue;		/* Only if applicable to this file version */
							if (sig == p->classtagtable[i].otags[k].sig)
								break;			/* Found it */
						}
						if (p->classtagtable[i].otags[k].sig != icMaxEnumTagType) {
							continue;		/* Yes it is */
						}
		
						/* Naughty tag */
						if (e.c == ICM_ERR_OK || quirk) {
							quirk = 1;
							e.c = ICM_FMT_UNEXPECTED_TAG;
							sprintf(e.m, "icc_write_check: deviceClass %s has unexpected tag %s",
						               icmtag2str(clsig), icmtag2str(sig));
							break;
						}
					}
				}
				/* Found matching class with no errors */
				if (p->transtagtable[j].sig == icMaxEnumTagType) {
					return ICM_ERR_OK;
				}
				/* Found an unexpected tag */
			}
		}	/* next class */

		/* Either we didn't fin a matching class or we found an error */
		if (e.c != ICM_ERR_OK) {
			if (quirk) {
				icmQuirkWarning(p, e.c, 0, e.m); 
				return ICM_ERR_OK;
			} else {
				return icmFormatWarning(p, e.c, e.m);
			}
		}
		
		/* Didn't find matching class entry */
		return icmFormatWarning(p, ICM_FMT_UNKNOWN_CLASS,
		          "icc_write_check: deviceClass %s for PCS '%s' Dev '%s' is not known\n",
		               icmtag2str(clsig), icmColorSpaceSig2str(pcssig),
						                icmColorSpaceSig2str(colsig));
	}	/* End of checking required tags */

	return ICM_ERR_OK;	/* Assume anything is ok */
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

static icmBase *icc_add_tag_imp(icc *p, icTagSignature sig, icTagTypeSignature ttype, int rdff);

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
			                                     icSigS15Fixed16ArrayType, 0)) == NULL) {
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
	/* ICC V4 style, then set the media white point tag to D50 and save */
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
				                                     icSigS15Fixed16ArrayType, 0)) == NULL) {
				return icm_err(p, 1,"icc_write: Adding 'chad' tag failed");
			}
			chadTag->count = 9;
			if ((rv = chadTag->allocate(chadTag)	) != 0) {
				return icm_err(p, 1,"icc_write: Allocating 'chad' tag failed");
			}

			p->tempChad = 1;
	
			if (wr) {
				icmXYZArray *blackPointTag;

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

				/* Track change in black point too */
				if ((blackPointTag = (icmXYZArray *)p->read_tag(p, icSigMediaBlackPointTag)) != NULL
				 && blackPointTag->ttype == icSigXYZType
				 && blackPointTag->count >= 1) {
					double bp[3];

					p->tempBP = blackPointTag->data[0];
					icmXYZ2Ary(bp, blackPointTag->data[0]);
					icmMulBy3x3(bp, p->chadmx, bp); 
					icmAry2XYZ(blackPointTag->data[0], bp);
				}
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
				                                     icSigS15Fixed16ArrayType, 0)) == NULL) {
				return icm_err(p, 1,"icc_write: Adding 'chad' tag failed");
			}
			chadTag->count = 9;
			if ((rv = chadTag->allocate(chadTag)) != 0) {
				return icm_err(p, 1,"icc_write: Allocating 'chad' tag failed");
			}
	
			p->tempChad = 1;

			if (wr) {
				icmXYZArray *blackPointTag;

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

				/* Track change in black point too */
				if ((blackPointTag = (icmXYZArray *)p->read_tag(p, icSigMediaBlackPointTag)) != NULL
				 && blackPointTag->ttype == icSigXYZType
				 && blackPointTag->count >= 1) {
					double bp[3];

					p->tempBP = blackPointTag->data[0];
					icmXYZ2Ary(bp, blackPointTag->data[0]);
					icmMulBy3x3(bp, p->chadmx, bp); 
					icmAry2XYZ(blackPointTag->data[0], bp);
				}
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
		icmXYZArray *blackPointTag;
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

			/* Restore original black point */
			if ((blackPointTag = (icmXYZArray *)p->read_tag(p, icSigMediaBlackPointTag)) != NULL
			 && blackPointTag->ttype == icSigXYZType
			 && blackPointTag->count >= 1) {
				blackPointTag->data[0] = p->tempBP;
			}
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
			icmXYZArray *blackPointTag;
	
			/* Remove temporary 'chad' tag */
			if (p->delete_tag_quiet(p, icSigChromaticAdaptationTag) != ICM_ERR_OK) {
				return icm_err(p, 1,"icc_write: Deleting temporary 'chad' tag failed");
			}

			/* Restore original white point */
			whitePointTag->data[0] = p->tempWP;
			p->tempChad = 0;

			/* Restore original black point */
			if ((blackPointTag = (icmXYZArray *)p->read_tag(p, icSigMediaBlackPointTag)) != NULL
			 && blackPointTag->ttype == icSigXYZType
			 && blackPointTag->count >= 1) {
				blackPointTag->data[0] = p->tempBP;
			}
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
	nsize = sat_align(p->align, nsize);
	p->header->phsize = nsize - size;
	/* (Could call tagtable_serialise() with icmSnSize to compute tag table size) */
	size = sat_addaddmul(nsize, 4, p->count, 12);	/* Tag table length */
	size = sat_align(p->align, size);				/* Header + tage table length */
	p->pttsize = size - nsize;						/* Padded tag table size */

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
			size = sat_add(size, (p->data[i].size = p->data[i].objp->get_size(p->data[i].objp)));
			nsize = sat_align(p->align, size);
			p->data[i].pad = nsize - size;
			size = nsize;
			p->data[i].objp->touched = 1;	/* Don't account for this again */

		} else { /* must be linked - copy file allocation info. */
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
		    p->data[i].pad    = p->data[k].pad;
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

	p->op = icmSnRead;		// ~8 ?

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
	icmFile *fp,	/* File to write to */
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

	p->op = icmSnSize;		// ~8 ?

	/* Compute the total size and tag element data offsets, and */
	/* setup sizes, offsets, other dependent flags etc. */
	p->header->size = size = icc_get_size(p);

	p->op = icmSnWrite;		// ~8 ?

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

		p->op = icmSnWrite;

		/* Dummy write the header */
		p->header->doid = 1;
		if (p->header->write(p->header, p->header->phsize, of, 0) != ICM_ERR_OK) {
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
			          p->data[i].objp, p->data[i].size, of + p->data[i].offset, p->data[i].pad)
				                                                                 != ICM_ERR_OK) { 
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

		/* Reset touched flag for each tag type again */
		for (i = 0; i < p->count; i++)
			p->data[i].objp->touched = 0;
	}

	/* Now write out the profile for real. */
	/* Although it may appear like we're seeking for each element, */
	/* in fact elements will be written in file order. */

	/* Write the header */
	if (p->header->write(p->header, p->header->phsize, of, 0) != ICM_ERR_OK) {
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
		          p->data[i].objp, p->data[i].size, of + p->data[i].offset, p->data[i].pad)
			                                                                 != ICM_ERR_OK) { 
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
			if (icc_read_tag_ix(p, i) == NULL) {
				op->printf(op,"Got read error 0x%x, '%s'\n",p->e.c, p->e.m);
				p->clear_err(p);		/* Keep message to tag */
			}
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

	/* ~8 Free up color transforms */

	/* Free up the header */
	if (p->header != NULL)
		p->header->del(p->header);

	/* Free up the tag data objects */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].objp != NULL) {
			p->data[i].objp->del(p->data[i].objp);	/* Free if last reference */
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

/* Translate a pseudo tag type to a real type */
static int icc_translate_pseudotype(
	icc *p,
    icTagSignature sig,			/* Tag signature, possibly icmSigUnknown */
	icTagTypeSignature ttype	/* Tag type, possibly icmSigUnknownType */
) {
	unsigned int i, j;

	if (ttype == icmSigCommonTextDescriptionType) {


		if (sig != icmSigUnknown) {

			/* Find the sig in the tagsigtable */ 
			for (i = 0; p->tagsigtable[i].sig != sig
			         && p->tagsigtable[i].sig != icMaxEnumTagType; i++)
				;
	
			if (p->tagsigtable[i].sig != icMaxEnumTagType) {	/* Known signature */
	
				/* See if icSigTextType or icSigTextDescriptionType are expected */
				for (j = 0; p->tagsigtable[i].ttypes[j].ttype != icSigTextDescriptionType
					     && p->tagsigtable[i].ttypes[j].ttype != icSigTextType
				         && p->tagsigtable[i].ttypes[j].ttype != icMaxEnumTagType; j++)
					;
	
				if (p->tagsigtable[i].ttypes[j].ttype != icMaxEnumTagType) { /* found it */
					return p->tagsigtable[i].ttypes[j].ttype;
				}
			}
		}
		/* For an unknown tag and ICCV2, return icSigTextDescriptionType */
		return  icSigTextDescriptionType;
	}

	return ttype;
}

/* Check that a given tag and tagtype look valid. */
/* A ttype of icmSigUnknownType will be ignored. */
/* Set e.c and e.m if ttagtype is not recognized. */
/* If the sig is know, check that the tagtype is valid for that sig. */
static int icc_check_sig(
	icc *p,
	unsigned int *ttix,			/* If not NULL, return the tagtypetable index */
	int rd,						/* Flag, NZ if reading, else writing */
    icTagSignature sig,			/* Tag signature, possibly icmSigUnknown */
	icTagTypeSignature ttype,	/* Tag type, possibly icmSigUnknownType */
	icTagTypeSignature uttype,	/* Actual tag type if Unknown */
	int rdff					/* Tag was read from file */
) {
	unsigned int i, j;

	if (ttix != NULL)
		*ttix = 0xffffffff;			// Out of range value

	/* Check if the tag type is a known type */
	if (ttype != icmSigUnknownType) {

		for (j = 0; p->tagtypetable[j].ttype != ttype
		         && p->tagtypetable[j].ttype != icMaxEnumTagType; j++)
			;
	
		if (p->tagtypetable[j].ttype == icMaxEnumTagType) {	/* Unknown tagtype */
			return icm_err(p, rd ? ICM_ERR_RD_FORMAT : ICM_ERR_WR_FORMAT,
			        "icc_check_sig: Tag Type '%s' is not known", icmTypeSig2str(ttype));
		}

		/* Check that this tagtype is valid for the version. */
		if (!icmVersInRange(p, &p->tagtypetable[j].vrange)
		 && (  p->op != icmSnWrite											/* Not write */
		    || (p->cflags & icmCFlagAllowWrVersion) == 0					/* Not allow */
			|| !icmVersOverlap(&p->tagtypetable[j].vrange, &p->vcrange))) {	/* Not o'lap range */
			int warn = 0;		/* Force warning rather than error */

			/* Warn rather than error if icmCFlagWrFileRdWarn and tag was read from file */
			if (p->op == icmSnWrite
			 && (p->cflags & icmCFlagWrFileRdWarn)
			 && rdff)
				warn = 1;

			/* Special case for backwards compatibility */
			if (ttype != icSigColorantTableType
			 || p->op != icmSnWrite || getenv("ARGYLL_CREATE_V2COLORANT_TABLE") == NULL) {

				if (icmVersionWarning(p, ICM_VER_TYPEVERS, warn,
					  "Tag Type '%s' is not valid for file version %s (valid %s)\n",
					  icmTypeSig2str(ttype), icmProfileVers2str(p),
					  icmTVersRange2str(&p->tagtypetable[j].vrange)) != ICM_ERR_OK) {
					return p->e.c;
				}
			}
		}

		if (ttix != NULL)
			*ttix = j;
	}
	

	if (sig == icmSigUnknown)	/* Not known, so nothing to check */
		return p->e.c;

	/* Check if the tag signature is known. */
	/* If it is, then it should use one of the valid range of tagtypes for that tag sig. */
	/* If not, the tagtype could be icmSigUnknownType or any known type. */ 
	for (i = 0; p->tagsigtable[i].sig != sig
	         && p->tagsigtable[i].sig != icMaxEnumTagType; i++)
		;

	if (p->tagsigtable[i].sig != icMaxEnumTagType) {	/* Known signature */

		/* Check that this tag signature is valid for this version of file */
		if (!icmVersInRange(p, &p->tagsigtable[i].vrange)
		 && (  p->op != icmSnWrite											/* Not write */
		    || (p->cflags & icmCFlagAllowWrVersion) == 0
			|| !icmVersOverlap(&p->tagsigtable[i].vrange, &p->vcrange))) {

			/* Special case for backwards compatibility */
			if ((sig != icSigColorantTableTag && sig != icSigColorantTableOutTag)
			 || getenv("ARGYLL_CREATE_V2COLORANT_TABLE") == NULL) {
			/* Allow MediaBlackPointTag if quirks allowed */
			if (sig == icSigMediaBlackPointTag
			 && (p->cflags & icmCFlagAllowQuirks) != 0) {

				icmQuirkWarning(p, ICM_VER_TYPEVERS, 0, 
				  "Tag Sig '%s' is not valid for file version %s (valid %s)\n",
				  icmTagSig2str(sig), icmProfileVers2str(p),
				  icmTVersRange2str(&p->tagsigtable[i].vrange));

			} else {
				int warn = 0;		/* Force warning rather than error */
	
				/* Warn rather than error if icmCFlagWrFileRdWarn and tag was read from file */
				if (p->op == icmSnWrite
				 && (p->cflags & icmCFlagWrFileRdWarn)
				 && rdff)
					warn = 1;

				if (icmVersionWarning(p, ICM_VER_SIGVERS, warn, 
					  "Tag Sig '%s' is not valid for file version %s (valid %s)\n",
					  icmTagSig2str(sig), icmProfileVers2str(p),
					  icmTVersRange2str(&p->tagsigtable[i].vrange)) != ICM_ERR_OK)
					return p->e.c;
			}
			}
		}
	
		/* Check that the tag type is permitted for the tag signature */ 
		/* (icmSigUnknownType will not be in this table) */
		for (j = 0; p->tagsigtable[i].ttypes[j].ttype != ttype
		         && p->tagsigtable[i].ttypes[j].ttype != icMaxEnumTagType; j++)
			;

		if (p->tagsigtable[i].ttypes[j].ttype == icMaxEnumTagType) { /* unexpected tagtype */

			if (ttype == icmSigUnknownType) {		/* Permit UnknownType with a warning */ 
				icmQuirkWarning(p, ICM_FMT_SIG2TYPE, 0, 
			      "Tag Sig '%s' uses unexpected Tag Type '%s'",
		          icmTagSig2str(sig), icmTypeSig2str(uttype));
				return p->e.c;						/* OK */
			}

			if (icmFormatWarning(p, ICM_FMT_SIG2TYPE, 
			      "Tag Sig '%s' uses unexpected Tag Type '%s'",
		          icmTagSig2str(sig), icmTypeSig2str(uttype)) != ICM_ERR_OK)
				return p->e.c;						/* Error or OK */

		} else {	/* Recognised ttype */
			/* Check that the signature to type is permitted for this version of file */
			if (!icmVersInRange(p, &p->tagsigtable[i].ttypes[j].vrange)
			 && (  p->op != icmSnWrite											/* Not write */
			    || (p->cflags & icmCFlagAllowWrVersion) == 0
				|| !icmVersOverlap(&p->tagsigtable[i].ttypes[j].vrange, &p->vcrange))) {

				if (icmVersionWarning(p, ICM_VER_SIG2TYPEVERS, 0, 
					  "Tag Sig '%s' can't use Tag Type '%s' in file version %s (valid %s)",
			          icmTagSig2str(sig), icmTypeSig2str(uttype), icmProfileVers2str(p),
					  icmTVersRange2str(&p->tagsigtable[i].ttypes[j].vrange)) != ICM_ERR_OK)
					return p->e.c;					/* Error or OK */
			}
		}
	}

	return p->e.c;
}

/* Return the lut purpose value for a tag signature */
/* Return 0 if there is not sigtypetable setup, */
/* or the signature is not recognised. */
static icmLutPurpose icc_get_tag_lut_purpose(
	icc *p,
    icTagSignature sig			/* Tag signature */
) {
	unsigned int i;

	if (p->tagsigtable == NULL)
		return 0;

	for (i = 0; p->tagsigtable[i].sig != sig
	         && p->tagsigtable[i].sig != icMaxEnumTagType; i++)
		;

	if (p->tagsigtable[i].sig == icMaxEnumTagType)
		return 0;

	return p->tagsigtable[i].purp;
}

/* Search for a tag signature in the profile. */
/* return: */
/* 0 if found */
/* 1 if found but not handled type */
/* 2 if not found */
/* NOTE: doesn't set icc->e.c or icc->e.m[]. */
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
	for (j = 0; p->tagtypetable[j].ttype != icMaxEnumTagType; j++) {
		if (p->tagtypetable[j].ttype == p->data[i].ttype)
			break;
	}
	if (p->tagtypetable[j].ttype == icMaxEnumTagType)
		return 1;						/* Not handled */

	/* Check if this type is handled by this version format */
	/* (We don't warn as this should have been done on read or add) */
	if (!icmVersInRange(p, &p->tagtypetable[j].vrange)) {
		return 1;
	}

	return 0;						/* Found and handled */
}

/* Read as specific tag element data, and return a pointer to the object. */
/* (This is an internal function) */
/* Returns NULL if the tag doesn't exist. (Doesn't set e.c, e.m) */
/* Returns NULL on other error, details in e.c, e.m */
/* Returns an icmSigUnknownType object if the tag type isn't handled by a */
/* specific object and icmCFlagRdAllowUnknown */
/* icc owns returned object, and will delete it on icc deletion. */
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
	if (p->cflags & icmCFlagRdAllowUnknown) {
		for (j = 0; p->tagtypetable[j].ttype != icMaxEnumTagType; j++) {
			if (p->tagtypetable[j].ttype == p->data[i].ttype)
				break;
		}

		if (p->tagtypetable[j].ttype == icMaxEnumTagType)
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
		if (icc_check_sig(p, NULL, 1, p->data[i].sig, ttype, uttype, p->data[k].objp->rdff)
			                                                                   != ICM_ERR_OK) {
			return NULL;
		}

		/* Check the lut classes (if any) are compatible */
		if (p->get_tag_lut_purpose(p, p->data[i].sig) != p->get_tag_lut_purpose(p, p->data[k].sig)) {
			icm_err(p, ICM_ERR_PURPMISMATCH,
			     "icc_read_tag_ix: Tag '%s' is link to incompatible tag '%s'",
		          icmTagSig2str(p->data[i].sig), icmTagSig2str(p->data[k].sig));
			return NULL;
		}

		nob = p->data[k].objp;

		/* Re-check this tags validity with different sig */
		if (nob->check != NULL) {
			if (nob->check(nob, p->data[i].sig, 1) != ICM_ERR_OK) {
				return NULL;
			}
		}

		p->data[i].objp = nob;
		nob->refcount++;			/* Bump reference count */

		return nob;			/* Done */
	}

	p->rdff = 1;		/* Objects being created during reading a file */

	/* See if we can handle this type */
	if (icc_check_sig(p, &j, 1, p->data[i].sig, ttype, uttype, p->rdff) != ICM_ERR_OK) {
		return NULL;
	}

	/* Create and read in the object */
	if (ttype == icmSigUnknownType)
		nob = new_icmUnknown(p);
	else
		nob = p->tagtypetable[j].new_obj(p, ttype);

	if (nob == NULL) {
		p->rdff = 0;
		return NULL;
	}

	nob->creatorsig = p->data[i].sig;		/* Some types need to know this.. */

	if ((nob->read(nob, p->data[i].size, p->of + p->data[i].offset)) != 0) {
		nob->del(nob);		/* Failed, so destroy it */
		p->rdff = 0;
		return NULL;
	}
	p->rdff = 0;

	/* Check this tags validity */
	if (nob->check != NULL) {
		if (nob->check(nob, p->data[i].sig, 1) != ICM_ERR_OK) {
			nob->del(nob);	
			return NULL;
		}
	}
    p->data[i].objp = nob;
	return nob;
}

/* Read the tag element data, and return a pointer to the object. */
/* Returns NULL if the tag doesn't exist. (Doesn't set e.c, e.m) */
/* Returns NULL on other error, details in e.c, e.m */
/* Returns an icmSigUnknownType object if the tag type isn't handled by a specific object. */ 
/* icc owns returned object, and will delete it on icc deletion. */
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

/* Read the tag element data of the first matching, and return a pointer to the object */
/* Returns NULL if error. */
/* Returns an icmSigUnknownType object if the tag type isn't handled by a specific object. */
/* (Alternatively set icmCFlagRdAllowUnknown flag and use ->read_tag()) */ 
/* icc owns returned object, and will delete it on icc deletion. */
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
	p->cflags |= icmCFlagRdAllowUnknown;
	ob = icc_read_tag_ix(p, i);
	p->cflags = cflags;

	return ob;
}

/* Create and add a tag with the given signature. */
/* Any pseudo-tagtypes are translated to the actual tagtype. */
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
	icTagTypeSignature ttype,	/* Tag type - may be icmSigUnknownType */
	int rdff					/* Is read from file */
) {
	icmTagRec *tp;
	icmBase *nob;
	unsigned int i, j;

	/* Convert any pseudo-type to an actual type */
	ttype = icc_translate_pseudotype(p, sig, ttype);

	/* Check that the tag signature and tag type are reasonable */
	/* (Handles icmSigUnknownType appropriately) */
	if (icc_check_sig(p, &j, 0, sig, ttype, ttype, rdff) != ICM_ERR_OK) {
		return NULL;
	}

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
		if ((nob = new_icmUnknown(p)) == NULL) {
			return NULL;
		}
	} else {
		/* Allocate the empty object */
		if ((nob = p->tagtypetable[j].new_obj(p, ttype)) == NULL) {
			return NULL;
		}
	}

	/* Fill out our tag table entry */
    p->data[p->count].sig = sig;		/* The tag signature */
    nob->creatorsig = sig;				/* Tag that created tagtype */
	p->data[p->count].ttype = ttype;	/* The tag type signature */
    p->data[p->count].offset = 0;		/* Unknown offset yet */
    p->data[p->count].size = 0;			/* Unknown size yet */
    p->data[p->count].objp = nob;		/* Empty object */
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

	return icc_add_tag_imp(p, sig, ttype, 0);
}

/* Rename a tag signature */
/* Return error code */
static int icc_rename_tag(
	icc *p,
    icTagSignature sig,			/* Existing Tag signature - may be unknown */
    icTagSignature sigNew		/* New Tag signature - may be unknown */
) {
	unsigned int k;
	int rdff = 0;

	p->op = icmSnWrite;			/* Let check know direction */

	/* Search for signature in the tag table */
	for (k = 0; k < p->count; k++) {
		if (p->data[k].sig == sig)		/* Found it */
			break;
	}
	if (k >= p->count) {
		return icm_err(p, ICM_ERR_NOT_FOUND, "icc_rename_tag: Tag '%s' not found",
		                                                         icmTagSig2str(sig));
	}

	/* Get rdff flag if known */
	if (p->data[k].objp != NULL)
		rdff = p->data[k].objp->rdff;

	/* Check that the tag signature and tag type are reasonable */
	if (icc_check_sig(p, NULL, 0, sigNew, p->data[k].ttype, p->data[k].ttype, rdff) != ICM_ERR_OK)
		return p->e.c;

	/* Check the classes are compatible */
	if (p->get_tag_lut_purpose(p, sig) != p->get_tag_lut_purpose(p, sigNew)) {
		return icm_err(p, ICM_ERR_PURPMISMATCH,
		     "icc_rename_tag: New tag '%s' doesn't have the same purpose as old tag '%s'",
		                            icmTagSig2str(sigNew), icmTagSig2str(sig));
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
		                                                         icmTagSig2str(ex_sig));
		return NULL;
	}

	/* Check existing tag is loaded. */
    if (p->data[i].objp == NULL) {
		icm_err(p, ICM_ERR_NOT_FOUND, "icc_link_tag: Existing tag '%s' isn't loaded",
		                                                                   icmtag2str(ex_sig)); 
		return NULL;
	}

	/* Check that the new tag signature and linked tag type are reasonable */
	if (icc_check_sig(p, NULL, 0, sig, p->data[i].objp->ttype, p->data[i].ttype,
		                                 p->data[i].objp->rdff) != ICM_ERR_OK)
		return NULL;

	/* Check the LUT classes are compatible */
	if (p->get_tag_lut_purpose(p, sig) != p->get_tag_lut_purpose(p, ex_sig)) {
		icm_err(p, ICM_ERR_PURPMISMATCH,
		     "icc_link_tag: Link tag '%s' doesn't have the same LUT purpose as tag '%s'",
		                            icmTagSig2str(sig), icmTagSig2str(ex_sig));
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
		return icm_err(p, 2,"icc_unread_tag: Tag '%s' not currently loaded",icmTagSig2str(p->data[i].sig));
	}
	
	(p->data[i].objp->del)(p->data[i].objp);	/* Free if last reference */
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
		                                                        icmTagSig2str(sig));
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
		                                                  icmTagSig2str(sig));
	}

	/* If the tagtype is loaded, decrement the reference count */
    if (p->data[i].objp != NULL) {
		p->data[i].objp->del(p->data[i].objp);	/* Free if last reference */
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

/* (Internal implementation) */
/* Create tagtype for embedding in another tagtype (i.e. ProfileSequenceDescType). */
/* Returns pointer to object or NULL on error. */
static icmBase *icc_new_ttype_imp(
	icc *p,
	icTagTypeSignature ttype,		/* Sub-tagtype to create */
	icTagTypeSignature pttype,		/* tagtype of creator */
	int rdff						/* Is read from file */
) {
	icmBase *nob;
	unsigned int k;

	/* Convert any pseudo-type to an actual type */
	ttype = icc_translate_pseudotype(p, icmSigUnknown, ttype);

	/* Check that the tag type is reasonable */
	if (icc_check_sig(p, &k, 0, icmSigUnknown, ttype, ttype, rdff) != ICM_ERR_OK) {
		return NULL;
	}

	if (ttype == icmSigUnknownType) {
		if ((nob = new_icmUnknown(p)) == NULL)
			return NULL;
	} else {
		unsigned int i, j;

		/* Check that this combo of parent and sub-tagtype is valid */
		/* Find parent ttype */
		for (i = 0; icmValidSubTagTypesTable[i].pttype != icMaxEnumTagType; i++) {
			if (icmValidSubTagTypesTable[i].pttype == pttype)
				break;
		}
		if (icmValidSubTagTypesTable[i].pttype == icMaxEnumTagType) {
			icmFormatWarning(p, ICM_FMT_PAR_TYPE_INVALID,
			          "icc_new_ttype_imp: parent ttype %s cannot have sub-tags\n",
			                                             icmTypeSig2str(pttype));
			return NULL;
		}
		for (j = 0; icmValidSubTagTypesTable[i].ttypes[j] != icMaxEnumTagType; j++) {
			if (icmValidSubTagTypesTable[i].ttypes[j] == ttype)
				break;
		}
		if (icmValidSubTagTypesTable[i].ttypes[j] == icMaxEnumTagType) {
			icmFormatWarning(p, ICM_FMT_SUB_TYPE_INVALID,
			          "icc_new_ttype_imp: sub ttype %s is invalid for parent %s\n",
			                       icmTypeSig2str(ttype), icmTypeSig2str(pttype));
			return NULL;
		}
		
		/* Allocate the empty object */
		if ((nob = p->tagtypetable[k].new_obj(p, ttype)) == NULL)
			return NULL;
	}

	return nob;
}

/* Create tagtype for embedding in another tagtype (i.e. ProfileSequenceDescType). */
/* Returns pointer to object or NULL on error. */
static icmBase *icc_new_ttype(
	icc *p,
	icTagTypeSignature ttype,		/* Sub-tagtype to create */
	icTagTypeSignature pttype		/* tagtype of creator */
) {
	return icc_new_ttype_imp(p, ttype, pttype, 0);
}

/* Create icmPe for embedding (internal implementation). */
/* Returns pointer to object or NULL on error. */
/* (This has the same logic as icc_new_ttype with extra checking, since icmPe */
/*  inherets from icmBase) */
static icmPe *icc_new_pe_imp(
	icc *p,
	icTagTypeSignature ttype,		/* Sub-tagtype to create */
	icTagTypeSignature pttype,		/* tagtype of creator */
	int rdff						/* Is read from file */
) {
	icmBase *nob;
	unsigned int k, i, j;

	/* Check that the tag type is reasonable */
	if (icc_check_sig(p, &k, 0, icmSigUnknown, ttype, ttype, rdff) != ICM_ERR_OK) {
		return NULL;
	}

	/* Locate part of check table that has just Pe sub-types in it */
	for (i = 0; icmValidSubTagTypesTable[i].pttype != PESUBTAGTYPESTABLESTART; i++)
		;

	/* Check that this combo of parent and sub-tagtype is valid */
	/* Find parent ttype */
	for (; icmValidSubTagTypesTable[i].pttype != icMaxEnumTagType; i++) {
		if (icmValidSubTagTypesTable[i].pttype == pttype)
			break;
	}
	if (icmValidSubTagTypesTable[i].pttype == icMaxEnumTagType) {
		icmFormatWarning(p, ICM_FMT_PAR_TYPE_INVALID,
		          "icc_new_pe_imp: parent ttype %s cannot have sub-tags\n",
		                                             icmTypeSig2str(pttype));
		return NULL;
	}
	for (j = 0; icmValidSubTagTypesTable[i].ttypes[j] != icMaxEnumTagType; j++) {
		if (icmValidSubTagTypesTable[i].ttypes[j] == ttype)
			break;
	}
	if (icmValidSubTagTypesTable[i].ttypes[j] == icMaxEnumTagType) {
		icmFormatWarning(p, ICM_FMT_SUB_TYPE_INVALID,
		          "icc_new_pe_imp: sub ttype %s is invalid for parent %s\n",
		                       icmTypeSig2str(ttype), icmTypeSig2str(pttype));
		return NULL;
	}
		
	/* Allocate the empty object */
	if ((nob = p->tagtypetable[k].new_obj(p, ttype)) == NULL) {
		return NULL;
	}
	nob->emb = 1;

	return (icmPe *)nob;
}

/* Create icmPe for embedding. */
/* Returns pointer to object or NULL on error. */
static icmPe *icc_new_pe(
	icc *p,
	icTagTypeSignature ttype,		/* Sub-tagtype to create */
	icTagTypeSignature pttype		/* tagtype of creator */
) {
	return icc_new_pe_imp(p, ttype, pttype, 0);
}

/* Copy & possibly translate a TagTypes contents from one TagType to another. */
/* The source Type may be from another profile, the destination must for */
/* this profile. (Also used in testing de-duplication.) */
/* Only a small number of Types are supported: */
/* TextDescriptionType */
/* Returns error code. */
static int icc_copy_ttype(
	icc *p,
	icmBase *idst,
	icmBase *isrc
) {

	/* Check that the destination is for this icc */
	if (idst->icp != p) {
		return icm_err(p, ICM_ERR_FOREIGN_TTYPE,"icc_copy_ttype: dst is not for this icc");
	}

	if (idst->cpy == NULL) {
		return icm_err(p, ICM_ERR_UNIMP_TTYPE_COPY,"icc_copy_ttype: unimplemented for %s",
		                                                      icmTypeSig2str(idst->ttype));
	}

	return idst->cpy(idst, isrc);
}

/* Compare two TagTypes contents to see if they are the same. */
/* Will return Z on same, NZ on different/error */
/* Error will be set if they are different types or type is unimplemented. */
/* Supported Types are: */
/* TextDescriptionType */
static int icc_compare_ttype(
	icc *p,
	icmBase *dst,
	icmBase *src
) {
	if (dst->cmp == NULL) {
		icm_err(p,ICM_ERR_UNIMP_TTYPE_CMP,"icc_compare_ttype: unimplemented for %s",
		                                               icmTypeSig2str(dst->ttype));
		return 1;
	}

	return dst->cmp(dst, src);
}

/* Set (or) the given compatibility flag */
static void icc_set_cflag(icc *p, icmCompatFlags flags) {
	p->cflags |= flags;
}

/* Unset (and ~) the given compatibility flag */
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

static double icc_get_tac(icc *p, double *chmax,
                          void (*calfunc)(void *cntx, double *out, double *in), void *cntx);
static int icc_get_wb_points(icc *p, int *wpassumed, icmXYZNumber *wp,
	int *bpassumed, icmXYZNumber *bp, double toAbs[3][3], double fromAbs[3][3]);
static void icc_set_illum(struct _icc *p, double ill_wp[3]);
static void icc_chromAdaptMatrix(icc *p, int flags, double imat[3][3], double mat[3][3],
                                 icmXYZNumber d_wp, icmXYZNumber s_wp);


/* Set any version dependent defaults */
static void icc_set_defaults(icc *p) {

	/* Defaults: should we create a V4 style Display profile with D50 media white point */
	/* tag and 'chad' tag ? */
	if (p->header->vers.majv >= 4)
		p->wrDChad = 1;		/* For Display profile mark media WP as D50 and put */
							/* absolute to relative transform matrix in 'chad' tag. */
	else
		p->wrDChad = 0;		/* No by default - use Bradford and store real Media WP */

	/* Environment overides */
	if (getenv("ARGYLL_CREATE_DISPLAY_PROFILE_WITH_CHAD") != NULL)
		p->wrDChad = 1;

	if (getenv("ARGYLL_CREATE_DISPLAY_PROFILE_WITHOUT_CHAD") != NULL)
		p->wrDChad = 0;

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

	/* 'chad' tag requires at least V2.4 */
	if ((p->wrDChad || p->wrOChad)
	  && p->get_version(p) < ICMTV_24) {
		p->set_version(p, ICMTV_24);
	}
}

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
	p->get_version   	  = icc_get_version;
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
	p->read_tag_any       = icc_read_tag_any;
	p->add_tag            = icc_add_tag;
	p->rename_tag         = icc_rename_tag;
	p->link_tag           = icc_link_tag;
	p->unread_tag         = icc_unread_tag;
	p->read_all_tags      = icc_read_all_tags;
	p->delete_tag         = icc_delete_tag;
	p->delete_tag_quiet   = icc_delete_tag_quiet;
	p->new_ttype          = icc_new_ttype;
	p->new_pe             = icc_new_pe;
	p->copy_ttype         = icc_copy_ttype;
	p->check_id      	  = icc_check_id;
	p->write_check        = icc_write_check;
	p->get_tac            = icc_get_tac;
	p->get_wb_points      = icc_get_wb_points;
	p->get_tag_lut_purpose  = icc_get_tag_lut_purpose;
	p->set_illum          = icc_set_illum;
	p->chromAdaptMatrix   = icc_chromAdaptMatrix;

	p->get_luobj          = icc_get_lu4obj;
	p->create_mono_xforms    = icc_create_mono_xforms;
	p->create_matrix_xforms  = icc_create_matrix_xforms;
	p->create_lut_xforms     = icc_create_lut_xforms;


	p->al                 = al->reference(al);
	p->tagtypetable  = icmTagTypeTable; 
	p->tagsigtable   = icmTagSigTable; 
	p->classtagtable = icmClassTagTable; 
	p->transtagtable = icmTransTagTable; 

	/* By default be leanient in what we read, and strict in what we write. */
	/* Just warn if we write tags read from an existing file though. */
	p->cflags |= icmCFlagRdFormatWarn;
	p->cflags |= icmCFlagRdVersionWarn;
	p->cflags |= icmCFlagRdAllowUnknown;
	p->cflags |= icmCFlagAllowExtensions;
	p->cflags |= icmCFlagAllowQuirks;
	p->cflags |= icmCFlagWrFileRdWarn;
	p->vcrange = icmtvrange_none;

	p->align = icAlignSize;		/* Everything is 32 bit aligned */

	/* Allocate a header object and set default values */
	if ((p->header = new_icmHeader(p)) == NULL) {
		if (e != NULL)
			*e = p->e;		/* Copy error out */
		p->del(p);
		return NULL;
	}

	/* Set default values */
	icc_set_defaults(p);

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
/* hsv, hls, cmyk and other device coords), this is entirely appropriate, */
/* and is what the transform machinery expects between different stages. */
/* At the ends of the transform for CIE based spaces though, this is not correct, */
/* since the binary representation should be consistent with the encoding in Annex A, */
/* page 74 of the standard. (Note that the standard doesn't specify the encoding of */
/* many color spaces ie. Yuv, Yxy etc., and is unclear about PCS.) */

/* The following functions convert to and from the CIE base spaces */
/* and the real Lut input/output values. These are used to convert real color */
/* space values into/out of the raw lut 0.0-1.0 representation (which subsequently */
/* get converted to ICC integer values in the obvious way as a mapping to 0 .. 2^n-1). */

/* This is used internally to support the Lut->lookup() function, */
/* and can also be used by someone writing a Lut based profile to determine */
/* the colorspace range that the input lut indexes cover, as well */
/* as processing the output luts values into normalized form ready */
/* for writing. */

/*
	The V4 specificatins are misleading on device space encoding,
	since they assume that all devices spaces however labeled,
	have no defined ICC encoding. The end result is simple enough though:

	V2 Lab encoding:

	ICC V2 Lab encoding should be used in all PCS encodings in
	the following tags, even in V4 profiles:

		(icSigLut8Type encoding hasn't changed for V4)
		icSigLut16Type
		icSigNamedColorType
		icSigNamedColor2Type

	and can/should be used for _device_ space Lab encoding for these tags.

	ICC V4 Lab encoding should be used for all PCS encodings in
	all other situations, and can/should be used for device space Lab encoding
	for all other situations.

	V4 Tags that seem to use the ICC V4 Lab encoding:

		icSigColorantTableType
		icSigLutAToBType	
		icSigLutBToAType

	If we get an V4 tag in a V2 profile (like icSigColorantTableType) then the handling
	is undefined. We choose to use the corresponding profile version (i.e. mixed versions)
	for compatibility with icclibv2. 

	[ Since the ICC spec. doesn't cover device spaces labeled as Lab,
      these are ripe for mis-matches between different implementations.]

	The MPE based transforms encode PCS in 32 bit float directly,
	i.e. there is no scaling. This implies that the processing elements need to
	do the scaling needed to map PCS values into things like curves, cLUTs etc.

	Implicitly normal device encoding is the range 0.0 - 1.0, although
	MPE allows greater than that. The fallback transforms can't match
	this though.
*/


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

	/* Transform from cone space */
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

	bs = p->tlen;			/* Current total bytes added */
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
		icmMD5_accume(p, p->buf);
		ibuf += bs;
		len -= bs;
	}

	/* Deal directly with input data 64 bytes at a time */
	while (len >= 64) {
		icmMD5_accume(p, ibuf);
		ibuf += 64;
		len -= 64;
	}

	/* Copy any remaining bytes to our partial buffer */
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
		icm_err_e(&p->e, ICM_ERR_INTERNAL, "icmFileMD5_seek: discontinuous write breaks MD5 calculation (seek %d expect %d)",offset,p->of);	
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


/* ============================================= */

/* Returns total ink limit and channel maximums. */
/* Returns -1.0 if not applicable for this type of profile. */
/* Returns -1.0 for grey, additive, or any profiles < 4 channels. */
/* This is a place holder that uses a heuristic, */
/* until there is a private or standard tag for this information */
static double icc_get_tac(		/* return TAC */
icc *p,
double *chmax,					/* device return channel sums. May be NULL */
void (*calfunc)(void *cntx, double *out, double *in),	/* Optional calibration func. */
void *cntx
) {
	icmHeader *rh = p->header;
	icmLuSpace *luo;
	icmLuLut *ll;
	icColorSpaceSignature outs;		/* Type of output space */
	int inn, outn;					/* Number of components */
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
    	case icSigHlsData: {
			return -1.0;
		}

		/* Assume no limit */
    	case icSigGrayData:
    	case icSig2colorData:
    	case icSig3colorData:
    	case icSigRgbData: {
			return -1.0;
		}

		default:
			break;
	}

	/* Get a PCS->device colorimetric lookup */
	if ((luo = (icmLuSpace *)p->get_luobj(p, icmBwd, icRelativeColorimetric, icmSigDefaultData, icmLuOrdNorm)) == NULL) {
		if ((luo = (icmLuSpace *)p->get_luobj(p, icmBwd, icmDefaultIntent, icmSigDefaultData, icmLuOrdNorm)) == NULL) {
			return -1.0;
		}
	}

	tac = luo->get_tac(luo, chmax, calfunc, cntx);
	
	luo->del(luo);

	return tac;
	
}

/* ============================================= */

/* Return the white and black points from the ICC tags, */
/* and the corresponding absolute<->relative conversion matrices. */
/* Any argument may be NULL */
/* This method undoes any V4 style 'chrm' adapation so that the */
/* returned white & black represent the actual media color. */
/* The wpassumed flag returns 1 if the white point tag */
/* is missing because this type of profile doesn't have a wp tag. */
/* The bpassumed flag returns 1 if the black point tag */
/* is missing. */
/* Depends on: icSigMediaWhitePointTag, icSigMediaBlackPointTag, */
/* icc->chadmx, icc->wpchtmx which depends on 'arts' tag etc. */
/* return nz on error and sets error codes. */
static int icc_get_wb_points(
	icc *p,
	int *wpassumed, icmXYZNumber *wp,
	int *bpassumed, icmXYZNumber *bp,
	double toAbs[3][3],
	double fromAbs[3][3]
) {
	icmXYZArray *whitePointTag, *blackPointTag;
	int _wpassumed, _bpassumed;
	icmXYZNumber _wp, _bp;
	double _toAbs[3][3];
	double _fromAbs[3][3];

	/* Substitute local variables if arguments are NULL */
	if (wpassumed == NULL)
		wpassumed = &_wpassumed;
	if (wp == NULL)
		wp = &_wp;
	if (bpassumed == NULL)
		bpassumed = &_bpassumed;
	if (bp == NULL)
		bp = &_bp;
	if (toAbs == NULL)
		toAbs = _toAbs;
	if (fromAbs == NULL)
		fromAbs = _fromAbs;

	*wpassumed = *bpassumed = 0;

	if ((whitePointTag = (icmXYZArray *)p->read_tag(p, icSigMediaWhitePointTag)) == NULL
        || whitePointTag->ttype != icSigXYZType || whitePointTag->count < 1) {
		if (p->header->deviceClass != icSigLinkClass) { 
			return icm_err(p, 1,"icc_lookup: Profile is missing Media White Point Tag");
		}
		*wp = icmD50;					/* safe value */
		*wpassumed = 1;
	} else {
		*wp = whitePointTag->data[0];	/* Copy structure */
	}

	if ((blackPointTag = (icmXYZArray *)p->read_tag(p, icSigMediaBlackPointTag)) == NULL
        || blackPointTag->ttype != icSigXYZType || blackPointTag->count < 1) {
		*bp = icmBlack;					/* default */
		*bpassumed = 1;
	} else  {
		*bp = blackPointTag->data[0];	/* Copy structure */
	}

	/* If this is a Display profile, check if there is a 'chad' tag, then */
	/* setup the white point and toAbs/fromAbs matricies from that, so as to implement an */
	/* effective Absolute Colorimetric intent for such profiles. */
 	if (p->header->deviceClass == icSigDisplayClass
	 && p->naturalChad && p->chadmxValid) {
		double twp[3];
		double tbp[3];
		double ichad[3][3];

		/* Conversion matrix is chad matrix. */
		icmCpy3x3(fromAbs, p->chadmx);
		icmInverse3x3(toAbs, fromAbs);

		/* Compute absolute white point. We deliberately ignore what's in the white point tag */
		/* and assume D50, since dealing with a non-D50 white point tag is contrary to ICCV4 */
		/* and full of ambiguity (i.e. is it a separate "media" white different to the */
		/* display white and not D50, or has the profile creator mistakenly put the display */
		/* white in the white point tag ?) */
		icmMulBy3x3(twp, toAbs, icmD50_ary3); 
		icmAry2XYZ(*wp, twp);

		/* Track black point too */
		icmInverse3x3(ichad, p->chadmx);
		icmXYZ2Ary(tbp, *bp);
		icmMulBy3x3(tbp, ichad, tbp); 
		icmAry2XYZ(*bp, tbp);

		DBLLL(("toAbs and fromAbs created from 'chad' tag\n"));
		DBLLL(("computed wp %.8f %.8f %.8f\n", wp->X, wp->Y, wp->Z));

	/* If this is an Output profile, check if there is a 'chad' tag, and */
	/* setup the toAbs/fromAbs matricies so that they include it, so as to implement an */
	/* effective Absolute Colorimetric intent for such profiles. */
 	} else if (p->header->deviceClass == icSigOutputClass
	 && p->naturalChad && p->chadmxValid) {
		double twp[3];
		double tbp[3];
		double ichad[3][3];

		/* Convert the white point tag value backwards through the 'chad' */
		icmInverse3x3(ichad, p->chadmx);
		icmXYZ2Ary(twp, *wp);
		icmMulBy3x3(twp, ichad, twp); 
		icmAry2XYZ(*wp, twp);

		/* Track black point too */
		icmXYZ2Ary(tbp, *bp);
		icmMulBy3x3(tbp, ichad, tbp); 
		icmAry2XYZ(*bp, tbp);

		/* Create absolute <-> relative conversion matricies */
		p->chromAdaptMatrix(p, ICM_CAM_NONE, toAbs, fromAbs, icmD50, *wp);
		DBLLL(("toAbs and fromAbs created from 'chad' tag & WP tag\n"));
		DBLLL(("toAbs and fromAbs created from wp %f %f %f and D50 %f %f %f\n", wp->X, wp->Y, wp->Z,
		                                                           icmD50.X, icmD50.Y, icmD50.Z));
	} else {
		/* Create absolute <-> relative conversion matricies */
		p->chromAdaptMatrix(p, ICM_CAM_NONE, toAbs, fromAbs, icmD50, *wp);
		DBLLL(("toAbs and fromAbs created from wp %f %f %f and D50 %f %f %f\n", wp->X, wp->Y, wp->Z,
		                                                           icmD50.X, icmD50.Y, icmD50.Z));
	}

	DBLLL(("toAbs   = %f %f %f\n          %f %f %f\n          %f %f %f\n",
	        toAbs[0][0], toAbs[0][1], toAbs[0][2],
	        toAbs[1][0], toAbs[1][1], toAbs[1][2],
	        toAbs[2][0], toAbs[2][1], toAbs[2][2]));
	DBLLL(("fromAbs = %f %f %f\n          %f %f %f\n          %f %f %f\n",
	        fromAbs[0][0], fromAbs[0][1], fromAbs[0][2],
	        fromAbs[1][0], fromAbs[1][1], fromAbs[1][2],
	        fromAbs[2][0], fromAbs[2][1], fromAbs[2][2]));

	return ICM_ERR_OK;
}

/* ========================================================== */
/* Utility function declarations are in their own file */

#include "icc_util.c"


