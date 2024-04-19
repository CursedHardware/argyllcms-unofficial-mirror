
/* 
 * ICC library stdio and malloc utility classes.
 *
 * Author:  Graeme W. Gill
 * Date:    2002/10/24
 * Version: 2.15
 *
 * Copyright 1997 - 2012 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 *
 * These are kept in a separate file to allow them to be
 * selectively ommitted from the icc library.
 *
 */

#ifndef COMBINED_STD

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

#endif /* !COMBINED_STD */

#if defined(SEPARATE_STD) || defined(COMBINED_STD)

#ifdef _MSC_VER
#define fileno _fileno
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)(-1))
#endif

static void Zmemory(void) { } 
#define ZMARKER ((void *)Zmemory)	/* Use address of function as zero memory marker */

/* -------------------------------------------------- */

/* Take a reference to the icmAlloc */
icmAlloc *icmAlloc_reference(
icmAlloc *pp
) {
	pp->refcount++;
	return pp;
}

/* -------------------------------------------------- */
/* Standard Heap allocator icmAlloc compatible class. */
/* Make the icclib malloc functions behave consistently, such as */
/* malloc(0) != NULL, realloc(NULL, size) and free(NULL) working, */
/* was well as providing recalloc() */

#ifndef ICC_DEBUG_MALLOC

static void icmAllocStd_free(
struct _icmAlloc *pp,
void *ptr
) {
	if (ptr != NULL && ptr != ZMARKER) {
		free(ptr);
	}
}

static void *icmAllocStd_malloc(
struct _icmAlloc *pp,
size_t size
) {
	if (size == 0)			/* Some malloc's return NULL for zero size */
		return ZMARKER;
	return malloc(size);
}

static void *icmAllocStd_realloc(
struct _icmAlloc *pp,
void *ptr,
size_t size
) {
	if (size == 0) {			/* Some malloc's return NULL for zero size */
		icmAllocStd_free(pp, ptr);
		return ZMARKER;
	}
	if (ptr == ZMARKER)
		ptr = NULL;

	if (ptr == NULL)			/* Some realloc's barf on reallocing NULL */
		return malloc(size);
	return realloc(ptr, size);
}

static void *icmAllocStd_calloc(
struct _icmAlloc *pp,
size_t num,
size_t size
) {
	int ind = 0;
	size_t tot = sati_mul(&ind, num, size);
	if (ind != 0)
		return NULL;			/* Overflow */

	if (tot == 0)				/* Some malloc's return NULL for zero size */
		return ZMARKER;
	return calloc(num, size);
}

static void *icmAllocStd_recalloc(
struct _icmAlloc *pp,
void *ptr,
size_t cnum,		/* Current number and size */
size_t csize,
size_t nnum,		/* New number and size */
size_t nsize
) {
	int ind = 0;
	size_t ctot, ntot;

	if (ptr == NULL)
		return icmAllocStd_calloc(pp, nnum, nsize); 

	ntot = sati_mul(&ind, nnum, nsize);
	if (ind != 0)
		return NULL;			/* Overflow */
	ctot = sati_mul(&ind, cnum, csize);
	if (ind != 0)
		return NULL;			/* Overflow */
	ptr = icmAllocStd_realloc(pp, ptr, ntot);

	if (ptr != NULL && ptr != ZMARKER && ntot > ctot)
		memset((char *)ptr + ctot, 0, ntot - ctot);			/* Clear the new region */
	return ptr;
}

/* we're done with the AllocStd object */
static void icmAllocStd_delete(
icmAlloc *pp
) {
	if (pp == NULL || --pp->refcount > 0)
		return;
	{
		icmAllocStd *p = (icmAllocStd *)pp;
		free(p);
	}
}

/* Create icmAllocStd */
/* Note that this will fail if e has an error already set */
icmAlloc *new_icmAllocStd(icmErr *e) {
	icmAllocStd *p;

	if (e != NULL && e->c != ICM_ERR_OK)
		return NULL;		/* Pre-existing error */

	if ((p = (icmAllocStd *) calloc(1,sizeof(icmAllocStd))) == NULL) {
		icm_err_e(e, ICM_ERR_MALLOC, "Allocating Standard Allocator object failed");
		return NULL;
	}
	p->refcount  = 1;
	p->malloc    = icmAllocStd_malloc;
	p->calloc    = icmAllocStd_calloc;
	p->realloc   = icmAllocStd_realloc;
	p->recalloc  = icmAllocStd_recalloc;
	p->free      = icmAllocStd_free;
	p->reference = icmAlloc_reference;
	p->del       = icmAllocStd_delete;

	return (icmAlloc *)p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
#else /* ICC_DEBUG_MALLOC */

/* Make sure that inline malloc #defines are turned off for this file */
#undef was_debug_malloc
#ifdef malloc
#undef malloc
#undef calloc
#undef realloc
#undef recalloc
#undef free
#define was_debug_malloc
#endif	/* dmalloc */


static void *icmAllocStd_dmalloc(
struct _icmAlloc *pp,
size_t size,
char *name,
int line
) {
	if (size == 0)			/* Some malloc's return NULL for zero size */
		return ZMARKER;
	return malloc(size);
}

static void *icmAllocStd_drealloc(
struct _icmAlloc *pp,
void *ptr,
size_t size,
char *name,
int line
) {
	if (size == 0) {			/* Some malloc's return NULL for zero size */
		icmAllocStd_dfree(pp, ptr, name, line);
		return ZMARKER;
	}
	if (ptr == NULL)			/* Some realloc's barf on reallocing NULL */
		return malloc(size);
	return realloc(ptr, size);
}

static void *icmAllocStd_dcalloc(
struct _icmAlloc *pp,
size_t num,
size_t size,
char *name,
int line
) {
	int ind = 0;
	size_t tot = sati_mul(&ind, num, size);
	if (ind != 0)
		return NULL;			/* Overflow */

	if (tot == 0)				/* Some malloc's return NULL for zero size */
		return ZMARKER;
	return calloc(num, size);
}

static void *icmAllocStd_drecalloc(
struct _icmAlloc *pp,
void *ptr,
size_t cnum,		/* Current number and size */
size_t csize,
size_t nnum,		/* New number and size */
size_t nsize,
char *name,
int line
) {
	int ind = 0;
	size_t ctot, ntot;

	if (ptr == NULL)
		return icmAllocStd_dcalloc(pp, nnum, nsize, name, line); 

	ntot = sati_mul(&ind, nnum, nsize);
	if (ind != 0)
		return NULL;			/* Overflow */
	ctot = sati_mul(&ind, cnum, csize);
	if (ind != 0)
		return NULL;			/* Overflow */
	ptr = icmAllocStd_realloc(pp, ptr, ntot, name, line);

	if (ptr != NULL && ptr != ZMARKER && ntot > ctot)
		memset((char *)ptr + ctot, 0, ntot - ctot);			/* Clear the new region */
	return ptr;

}

static void icmAllocStd_dfree(
struct _icmAlloc *pp,
void *ptr,
char *name,
int line
) {
	if (ptr != NULL && ptr != ZMARKER)
		free(ptr);
}

/* we're done with the AllocStd object */
static void icmAllocStd_delete(
icmAlloc *pp
) {
	if (p == NULL || --p->refcount > 0)
		return;
	{
		icmAllocStd *p = (icmAllocStd *)pp;
	
		free(p);
	}
}

/* Create icmAllocStd */
icmAlloc *new_icmAllocStd(icmErr *e) {
	icmAllocStd *p;

	if (e != NULL && e->c != ICM_ERR_OK)
		return NULL;		/* Pre-existing error */

	if ((p = (icmAllocStd *) calloc(1,sizeof(icmAllocStd))) == NULL) {
		icm_err_e(e, ICM_ERR_MALLOC, "Allocating Standard Allocator object failed");
		return NULL;
	}
	p->refcount  = 1;
	p->dmalloc   = icmAllocStd_dmalloc;
	p->drealloc  = icmAllocStd_drealloc;
	p->dcalloc   = icmAllocStd_dcalloc;
	p->drecalloc = icmAllocStd_drecalloc;
	p->dfree     = icmAllocStd_dfree;
	p->reference = icmAlloc_reference;
	p->del       = icmAllocStd_delete;

	return (icmAlloc *)p;
}

#ifdef was_debug_malloc
/* Re-enable debug malloc #defines */
#define malloc( p, size )	    dmalloc( p, size, __FILE__, __LINE__ )
#define calloc( p, num, size )	dcalloc( p, num, size, __FILE__, __LINE__ )
#define realloc( p, ptr, size )	drealloc( p, ptr, size, __FILE__, __LINE__ )
#define recalloc( p, ptr, cnum, csize, nnum, nsize ) drecalloc( p, ptr, cnum, csize, nnum, nsize, __FILE__, __LINE__ )
#define free( p, ptr )	        dfree( p, ptr , __FILE__, __LINE__ )
#undef was_debug_malloc
#endif	/* was_debug_malloc */

#endif	/* ICC_DEBUG_MALLOC */

/* ------------------------------------------------- */
/* Standard Stream file I/O icmFile compatible class */

/* Get the size of the file (Only valid for reading file). */
static size_t icmFileStd_get_size(icmFile *pp) {
	icmFileStd *p = (icmFileStd *)pp;

	return p->size;
}

/* Set current position to offset. Return 0 on success, nz on failure. */
static int icmFileStd_seek(
icmFile *pp,
unsigned int offset
) {
	icmFileStd *p = (icmFileStd *)pp;

	return fseek(p->fp, offset, SEEK_SET);
}

/* Read count items of size length. Return number of items successfully read. */
static size_t icmFileStd_read(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileStd *p = (icmFileStd *)pp;

	return fread(buffer, size, count, p->fp);
}

/* write count items of size length. Return number of items successfully written. */
static size_t icmFileStd_write(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileStd *p = (icmFileStd *)pp;

	return fwrite(buffer, size, count, p->fp);
}


/* do a printf */
static int icmFileStd_printf(
icmFile *pp,
const char *format,
...
) {
	int rv;
	va_list args;
	icmFileStd *p = (icmFileStd *)pp;

	va_start(args, format);
	rv = vfprintf(p->fp, format, args);
	va_end(args);
	return rv;
}

/* flush all write data out to secondary storage. Return nz on failure. */
static int icmFileStd_flush(
icmFile *pp
) {
	icmFileStd *p = (icmFileStd *)pp;

	return fflush(p->fp);
}

/* Return the memory buffer. Error if not icmFileMem */
static int icmFileStd_get_buf(
icmFile *pp,
unsigned char **buf,
size_t *len
) {
	return 1;
}

/* Take a reference to the icmFileStd */
static icmFile *icmFileStd_reference(
icmFile *pp
) {
	pp->refcount++;
	return pp;
}

/* we're done with the file object, return nz on failure */
static int icmFileStd_delete(
icmFile *pp
) {
	if (pp == NULL || --pp->refcount > 0)
		return 0;
	{
		int rv = 0;
		icmFileStd *p = (icmFileStd *)pp;
		icmAlloc *al = p->al;
	
		if (p->doclose != 0) {
			if (fclose(p->fp) != 0)
				rv = 2;
		}
	
		al->free(al, p);	/* Free this object */
		al->del(al);		/* Free allocator if this is the last reference */

		return rv;
	}
}

/* Create icmFile given a (binary) FILE* */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_fp(
icmErr *e,		/* Sticky return error, may be NULL */
FILE *fp
) {
	return new_icmFileStd_fp_a(e, fp, NULL);
}

/* Create icmFile given a (binary) FILE* and allocator */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_fp_a(
icmErr *e,			/* Sticky returned error, may be NULL */
FILE *fp,
icmAlloc *al		/* heap allocator, NULL for default */
) {
	icmFileStd *p = NULL;
	struct stat sbuf = { 0 };

	if (e != NULL && e->c != ICM_ERR_OK)
		return NULL;		/* Pre-existing error */

	if (al == NULL) {	/* None provided, create default */
		if ((al = new_icmAllocStd(e)) == NULL)
			return NULL;
	} else {
		al = al->reference(al);
	}

	if ((p = (icmFileStd *) al->calloc(al, 1, sizeof(icmFileStd))) == NULL) {
		al->del(al);
		icm_err_e(e, ICM_ERR_MALLOC, "Allocating Standard File object failed");
		return NULL;
	}
	p->refcount  = 1;
	p->al        = al;
	p->get_size  = icmFileStd_get_size;
	p->seek      = icmFileStd_seek;
	p->read      = icmFileStd_read;
	p->write     = icmFileStd_write;
	p->printf    = icmFileStd_printf;
	p->flush     = icmFileStd_flush;
	p->get_buf   = icmFileStd_get_buf;
	p->reference = icmFileStd_reference;
	p->del       = icmFileStd_delete;

	p->fp = fp;
	p->doclose = 0;

	if (fstat(fileno(fp), &sbuf) == 0) {
		p->size = sbuf.st_size;
	} else {
		p->size = 0;		/* Hmm. */
	}

	return (icmFile *)p;
}

/* Create icmFile given a file name */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_name(
icmErr *e,				/* sticky return error, may be NULL */
char *name,
char *mode
) {
	return new_icmFileStd_name_a(e, name, mode, NULL);
}

/* Create given a file name and allocator */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileStd_name_a(
icmErr *e,				/* Sticky return error, may be NULL */
char *name,
char *mode,
icmAlloc *al			/* heap allocator, NULL for default */
) {
	FILE *fp;
	icmFile *p;
	char nmode[50];

	if (e != NULL && e->c != ICM_ERR_OK)
		return NULL;		/* Pre-existing error */

	strcpy(nmode, mode);
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif

	if ((fp = fopen(name,nmode)) == NULL) {
		icm_err_e(e, ICM_ERR_FILE_OPEN, "Opening file '%s' failed",name);
		return NULL;
	}
	
	p = new_icmFileStd_fp_a(e, fp, al);

	if (p != NULL) {
		icmFileStd *pp = (icmFileStd *)p;
		pp->doclose = 1;
	}
	return p;
}

/* ------------------------------------------------- */

/* Create a memory image file access class with the std allocator */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileMem(
icmErr *e,			/* Sticky return error, may be NULL */
void *base,			/* Pointer to base of memory buffer */
size_t length		/* Number of bytes in buffer */
) {
	icmFile *p;
	icmAlloc *al;			/* memory allocator */

	if (e != NULL && e->c != ICM_ERR_OK)
		return NULL;		/* Pre-existing error */

	if ((al = new_icmAllocStd(e)) == NULL)
		return NULL;

	if ((p = new_icmFileMem_a(e, base, length, al)) == NULL) {
		al->del(al);
		return NULL;
	}

	al->del(al);			/* icmFile will have taken a reference */

	return p;
}

/* Create a memory image file access class with the std allocator */
/* and delete buffer when icmFile is deleted. */
/* Note that this will fail if e has an error already set */
icmFile *new_icmFileMem_d(
icmErr *e,			/* Sticky return error, may be NULL */
void *base,			/* Pointer to base of memory buffer */
size_t length		/* Number of bytes in buffer */
) {
	icmFile *fp;

	if ((fp = new_icmFileMem(e, base, length)) != NULL) {	
		((icmFileMem *)fp)->del_buf = 1;
	}
	return fp;
}

/* ------------------------------------------------- */

/* Create an icc with the std allocator */
/* Note that this will fail if e has an error already set */
icc *new_icc(icmErr *e) {
	icc *p;
	icmAlloc *al;			/* memory allocator */

	if (e != NULL && e->c != ICM_ERR_OK)
		return NULL;		/* Pre-existing error */

	if ((al = new_icmAllocStd(e)) == NULL)
		return NULL;

	if ((p = new_icc_a(e, al)) == NULL) {
		al->del(al);
		return NULL;
	}

	al->del(al);		/* new_icc_a will have taken a reference */
	return p;
}

/* ------------------------------------------------- */

/* Create an icmMD5 with the std allocator */
/* Note that this will fail if e has an error already set */
icmMD5 *new_icmMD5(icmErr *e) {
	icmMD5 *p;
	icmAlloc *al;			/* memory allocator */

	if ((al = new_icmAllocStd(e)) == NULL)
		return NULL;

	if ((p = new_icmMD5_a(e, al)) == NULL) {
		al->del(al);
		return NULL;
	}

	al->del(al);		/* new_icmMD5_a will have taken a reference */

	return p;
}

#endif /* defined(SEPARATE_STD) || defined(COMBINED_STD) */
