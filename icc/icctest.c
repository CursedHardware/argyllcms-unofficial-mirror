
/* 
 * International Color Consortium Format Library (icclib)
 * Library Read/Write test and example code.
 *
 * Author:  Graeme W. Gill
 * Date:    1999/11/29
 * Version: 2.15
 *
 * Copyright 1997 - 2012 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 */

/* TTBD:
 *
 *	Fix enums to be selected randomly (ie. header)
 *
 *	Should add test of ->delete_tag()
 *	Should add test of ->rename_tag()
 *
 *  Add many extra comments and explanations.
 *
 */

/*

	This file is intended to serve two purposes. One
	is to minimally test the ability of the icc library
	to read and write all tag types. The other is as
	a source code example of how to read and write
	each tag type, since icc.h might otherwise take
	some effort to understand.

    Note XYZ scaling to 1.0, not 100.0
 
 */

#define NTRIALS 100

#define TEST_UNICODE 0		/* Test unicode translation code */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#ifdef __sun
#include <unistd.h>
#endif
#include "icc.h"

void error(char *fmt, ...), warning(char *fmt, ...);

/* Set the seed */
void set_seed(ORD32 o32);

/* Rounded floating test numbers */
int rand_int(int low, int high);
unsigned int rand_o8(void), rand_o16(void), rand_o32(void);
double rand_8f(void), rand_L8(void), rand_ab8(void);
double rand_16f(void), rand_XYZ16(void), rand_u8f8(void), rand_L16(void), rand_ab16(void);
double rand_u16f16(void), rand_s15f16(void);
int dcomp(double a, double b);

/* random ICC specific values */
unsigned int rand_ScreenEncodings(void);
unsigned int rand_DeviceAttributes(void);
unsigned int rand_ProfileHeaderFlags(void);
unsigned int rand_AsciiOrBinaryData(void);
icColorSpaceSignature rand_ColorSpaceSignature(void);
icColorSpaceSignature rand_PCS(void);
icTechnologySignature rand_TechnologySignature(void);
icProfileClassSignature rand_ProfileClassSignature(void);
icPlatformSignature rand_PlatformSignature(void);
icMeasurementFlare rand_MeasurementFlare(void);
icMeasurementGeometry rand_MeasurementGeometry(void);
icRenderingIntent rand_RenderingIntent(void);
icSpotShape rand_SpotShape(void);
icStandardObserver rand_StandardObserver(void);
icIlluminant rand_Illuminant(void);
char *rand_ascii_string(int lenmin, int lenmax);
char *rand_utf8_string(int lenmin, int lenmax);
icmUTF16 *rand_utf16_string(int lenmin, int lenmax);
size_t strlen_utf16(icmUTF16 *in);
int strcmp_utf16(icmUTF16 *in1, icmUTF16 *in2);

/* declare some test functions */
#if TEST_UNICODE > 0
int unicode_test(void);
#endif
int md5_test(void);

/*
	I've split the functionality up into two pieces.
	The main() code does the overall file write/read,
	while the tag type code is all in the doit() function.
	The write/read logic is all sandwiched together (distinguished
	by the state of the mode flag), so that the code for each
	tag type is kept adjacent.

	In a real application, one wouldn't do it this way.
*/

int doit(int mode, icc *wr_icco, icc *rd_icco);

void usage(void) {
	printf("ICC library regression test, V%s\n",ICCLIB_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: icctest [-v level] [-1] [-n pass] [-w] [-r]\n");
	fprintf(stderr," -v level      Verbosity level\n");
	fprintf(stderr," -1            Do just first pass\n");
	fprintf(stderr," -n x          Do just pass x\n");
	fprintf(stderr," -w            Do write side of pass\n");
	fprintf(stderr," -r            Do read side of pass\n");
	fprintf(stderr," -W            Show any warnings\n");
	exit(1);
}

static void Warning(icc *p, int err, const char *fmt, va_list args) {
	fprintf(stderr, "Warning (0x%x): ",err);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
}

int
main(
	int argc,
	char *argv[]
) {
	int fa,nfa;
	int nointro = 0;
	int verb = 0;
	int passn = 0;
	int wonly = 0;
	int ronly = 0;
	int warn = 0;
	icmErr e = { 0, { '\000'} };					/* Place to get error details back */
	char *file_name = "xxxx.icm";
	icmFile *wr_fp, *rd_fp;
	icc *wr_icco, *rd_icco;		/* Keep object separate */
	int rv = 0;
	int i;
	unsigned int offset = 0;	/* File write/read offset, 0 for standard icc */

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage();

			/* no inro */
			else if (argv[fa][1] == 'N') {
				nointro = 1;
			}
			
			/* Verbosity */
			else if (argv[fa][1] == 'v') {
				fa = nfa;
				if (na == NULL)
					verb = 1;
				else
					verb = na[0] - '1';
			}

			/* Just first pass */
			else if (argv[fa][1] == '1') {
				passn = 1;
			}

			/* Just pass n */
			else if (argv[fa][1] == 'n') {
				fa = nfa;
				if (na == NULL)
					usage();
				else {
					passn = atoi(na);
					fa = nfa;
					if (passn < 1 || passn > NTRIALS)
						usage();
				}
			}

			/* just write */
			else if (argv[fa][1] == 'w') {
				wonly = 1;
			}

			/* Just read */
			else if (argv[fa][1] == 'r') {
				ronly = 1;
			}

			/* Warn */
			else if (argv[fa][1] == 'W') {
				warn = 1;
			}

			else 
				usage();
		} else
			break;
	}

	if (!nointro)
		printf("ICC library regression test, V%s\n",ICCLIB_VERSION_STR);

	/* Do any internal code tests. */

#if TEST_UNICODE > 0
	if (unicode_test())
		error ("Unicode translation routines are faulty");
#endif

	if (md5_test() != 0)
		error ("MD5 checksum routine is faulty");

	/* Outer loop does a number of file write/reads, */
	/* in order to exercise random tests, and to test file offsets. */

	i = 0;
	if (passn != 0)
		i = passn-1;
	for (; i < NTRIALS; i++) {
		unsigned int size;		/* Expected write size */

		set_seed(0x12345678 + i * 73);

		if (verb) {
			printf("Pass %d/%d:",i+1,NTRIALS); fflush(stdout);
		} else {
			printf(".");
			fflush(stdout);
		}

		if (i > 0) {
			/* choose another file offset to test */
			offset = rand_int(0,72789);
		}

		/* -------------------------- */
		/* Deal with writing the file */
	
		icm_err_clear_e(&e);

		if (!ronly) {
			/* Open up the file for writing */
			if ((wr_fp = new_icmFileStd_name(&e, file_name,"w")) == NULL)
				error("Write: Open file '%s' failed with 0x%x, '%s'",file_name,e.c, e.m);
		} else {
			/* Open up the file for reading */
			if ((wr_fp = new_icmFileStd_name(&e, file_name,"r")) == NULL)
				error("Write: Open file '%s' failed with 0x%x, '%s'",file_name,e.c, e.m);
		}
		
		if ((wr_icco = new_icc(&e)) == NULL)
			error("Write: Creation of ICC object failed with 0x%x, '%s'",e.c,e.m);
	
		if (warn)
			wr_icco->warning = Warning;

		/* Don't check for required tags, so that we can test range of profile */
		/* classes with full range of tags. */
		wr_icco->set_cflag(wr_icco, icmCFlagNoRequiredCheck);

		/* Relax format so we can use the full range of tagtypes */
//		wr_icco->set_cflag(wr_icco, icmCFlagWrVersionWarn);
		wr_icco->set_vcrange(wr_icco, &icmtvrange_all); 

		/* Add all the tags with their tag types */
		if ((rv = doit(0, wr_icco, NULL)) != 0)
			error ("Write tags: %d, 0x%x '%s'",rv,wr_icco->e.c,wr_icco->e.m);
	
		/* Write the file (including all tags) out */
		/* The last parameter is the offset to write the */
		/* ICC profile into the file. For a standard ICC profile, */
		/* this needs to be 0, but it might be non-zero if you are writing */
		/* an embedded profile. */
	
		/* Check that get_size() is working too. */
		if ((size = wr_icco->get_size(wr_icco)) == 0)
			error ("Write size: %d, 0x%x '%s'",rv,wr_icco->e.c,wr_icco->e.m);
	
		if (!ronly) {
			if ((rv = wr_icco->write(wr_icco,wr_fp,offset)) != 0)
				error ("Write file: %d, 0x%x '%s'",rv,wr_icco->e.c,wr_icco->e.m);
		}
	
		/* To check that get_size() is correct: */
		{
			icmFileStd *pp = (icmFileStd *)wr_fp;	/* Cheat - Look inside icmFile */

			if (fseek(pp->fp, 0, SEEK_END))
				error ("Write: seek to EOF failed");
			if ((unsigned int)ftell(pp->fp) != offset + size)
				error ("Write: get_size function didn't return correct value - got %d, expected %d",
				        ftell(pp->fp),offset+size);
		}
	
		/*
			Would normally call wr_icco->del(wr_icco);
			but leave it, so that we can verify the read.
		*/
	
		wr_fp->del(wr_fp);

		if (!wonly) {
			/* -------------------------- */
			/* Deal with reading and verifying the file */
	
			icm_err_clear_e(&e);

			/* Open up the file for reading */
			if ((rd_fp = new_icmFileStd_name(&e, file_name,"r")) == NULL)
				error("Read: Open file '%s' failed with 0x%x, '%s'",file_name,e.c, e.m);
	
			if ((rd_icco = new_icc(&e)) == NULL)
				error("Read: Creation of ICC object failed with 0x%x, '%s'",e.c,e.m);
	
			if (warn)
				rd_icco->warning = Warning;

			/* Read the header and tag list */
			/* The last parameter is the offset to read the */
			/* ICC profile from the file. For a standard ICC proifile, */
			/* this needs to be 0, but it might be non-zero if you are writing */
			/* an embedded profile this needs to be 0, but it might be non-zero */
			/* if you are writing an embedded profile. */
			if ((rv = rd_icco->read(rd_icco,rd_fp,offset)) != 0)
				error ("Read: %d, 0x%x '%s'",rv,rd_icco->e.c,rd_icco->e.m);
	
			/* Read and verify all the tags and their tag types */
			if ((rv = doit(1, wr_icco, rd_icco)) != 0)
				error ("Read: %d, 0x%x '%s'",rv,rd_icco->e.c,rd_icco->e.m);
	
			/* -------- */
			/* Clean up */
			wr_icco->del(wr_icco);
	
			rd_icco->del(rd_icco);
			rd_fp->del(rd_fp);

			if (verb)
				printf(" OK\n");
		} else {
			if (verb)
				printf(" Written\n");
		}

		if (passn != 0 || ronly || wonly)
			break;
	}

	return 0;
}

/* -------------------------------------------------------------- */
/* Internal routine checks. */

#if TEST_UNICODE > 0
int unicode_test() {
	size_t len, blen;
	icmUTFerr illegal;
	int i;

	icmUTF8  *in8;
	icmUTF16 *out16;
	ORD8 *out16BE;
	icmUTF8  *check8;

	icmUTF16 *in16;
	icmUTF8  *out8;
	icmUTF16 *check16;

	printf("Testing utf-8 -> utf-16 BE -> utf-8:\n");
	for (i = 0; i < TEST_UNICODE; i++) {
		in8 = rand_utf8_string(1, 237);
		len = strlen(in8) + 1;

		blen = icmUTF8toUTF16BE(&illegal, NULL, in8, len);
		if (illegal != 0) {
			printf("icmUTF8toUTF16BE returned error 0x%x\n",illegal);
			return 1;
		}
		if ((out16BE = malloc(blen)) == NULL)
			error("malloc of out16BE failed\n");
		icmUTF8toUTF16BE(NULL, out16BE, in8, len);

		len = blen/2;

		/* Now convert back */
		blen = icmUTF16BEtoUTF8(&illegal, NULL, out16BE, len);
		if (illegal != 0) {
			printf("icmUTF16BEtoUTF8 returned error 0x%x\n",illegal);
			return 1;
		}
		if ((check8 = malloc(blen)) == NULL)
			error("malloc of check8 failed\n");
		icmUTF16BEtoUTF8(NULL, check8, out16BE, len);

		if (strcmp(in8, check8) != 0) {
			printf("utf-8 BE string didn't verify\n");
			return 1;
		}

		/* Clean up */
		free(in8);
		free(out16BE);
		free(check8);
	}

	printf("Testing utf-8 -> utf-16 -> utf-8:\n");
	for (i = 0; i < TEST_UNICODE; i++) {
		in8 = rand_utf8_string(1, 237);

		blen = icmUTF8toUTF16(&illegal, NULL, in8);
		if (illegal != 0) {
			printf("icmUTF8toUTF16 returned error 0x%x\n",illegal);
			return 1;
		}
		if ((out16 = malloc(blen)) == NULL)
			error("malloc of out16 failed\n");
		icmUTF8toUTF16(NULL, out16, in8);

		/* Now convert back */
		blen = icmUTF16toUTF8(&illegal, NULL, out16);
		if (illegal != 0) {
			printf("icmUTF16toUTF8 returned error 0x%x\n",illegal);
			return 1;
		}
		if ((check8 = malloc(blen)) == NULL)
			error("malloc of check8 failed\n");
		icmUTF16toUTF8(NULL, check8, out16);

		if (strcmp(in8, check8) != 0) {
			printf("utf-8 string didn't verify\n");
			return 1;
		}

		/* Clean up */
		free(in8);
		free(out16);
		free(check8);
	}

	printf("Testing utf-16 -> utf-8 -> utf-16\n");
	for (i = 0; i < TEST_UNICODE; i++) {
		in16 = rand_utf16_string(1, 237);

		blen = icmUTF16toUTF8(&illegal, NULL, in16);
		if (illegal != 0) {
			printf("icmUTF16toUTF8 returned error 0x%x\n",illegal);
			return 1;
		}
		if ((out8 = malloc(blen)) == NULL)
			error("malloc of out8 failed\n");
		icmUTF16toUTF8(NULL, out8, in16);

		/* Now convert back */
		blen = icmUTF8toUTF16(&illegal, NULL, out8);
		if (illegal != 0) {
			printf("icmUTF8toUTF16 returned error 0x%x\n",illegal);
			return 1;
		}
		if ((check16 = malloc(blen)) == NULL)
			error("malloc of check16 failed\n");
		icmUTF8toUTF16(NULL, check16, out8);

		if (strcmp_utf16(in16, check16) != 0) {
			printf("utf-16 string didn't verify\n");
			return 1;
		}

		/* Clean up */
		free(in16);
		free(out8);
		free(check16);
	}

	printf("Testing utf-8 -> ASCII, HTML:\n");
	for (i = 0; i < TEST_UNICODE; i++) {
		len = rand_int(15, 42);		/* Max length */

		printf("%d\n",i+1);

		in8 = rand_utf8_string(1, len);
		printf("   utf8: '%s'\n",in8);

		blen = icmUTF8toHTMLESC(&illegal, NULL, NULL, in8);
		if ((out8 = malloc(blen)) == NULL)
			error("malloc of out8 failed\n");
		icmUTF8toHTMLESC(NULL, NULL, out8, in8);
		printf("   html  '%s'\n",out8);
		free(out8);

		blen = icmUTF8toASCII(&illegal, NULL, NULL, in8);
		if ((out8 = malloc(blen)) == NULL)
			error("malloc of out8 failed\n");
		icmUTF8toASCII(NULL, NULL, out8, in8);
		printf("   ascii '%s'\n",out8);
		free(out8);

		free(in8);
	}

	return 0;
}

#endif


/* Test the MD5 function. Return nz if fail. */
int md5_test() {
	int rv = 0;
	int i, j;
	icc *icco;
	icmErr e = { 0, { '\000'} };
	icmMD5 *m;

	unsigned char chs1[16];
	unsigned char chs2[16];
	
	/* Standard RFC 1321 test cases */
	struct {
		char *s;
		ORD32 sum[4];
	} tc[] = {
		{ "",  { 0xd41d8cd9, 0x8f00b204, 0xe9800998, 0xecf8427e } },
		{ "a", { 0x0cc175b9, 0xc0f1b6a8, 0x31c399e2, 0x69772661 } },
		{ "abc", { 0x90015098, 0x3cd24fb0, 0xd6963f7d, 0x28e17f72 } },
		{ "message digest", { 0xf96b697d, 0x7cb7938d, 0x525a2f31, 0xaaf161d0 } },
		{ "abcdefghijklmnopqrstuvwxyz", { 0xc3fcd3d7, 0x6192e400, 0x7dfb496c, 0xca67e13b } },
		{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
			 { 0xd174ab98, 0xd277d9f5, 0xa5611c2c, 0x9f419d9f } },
		{ "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
			 { 0x57edf4a2, 0x2be3c955, 0xac49da2e, 0x2107b67a } },
		{ NULL,  { 0,0,0,0 } }
	};

	icm_err_clear_e(&e);

	if ((icco = new_icc(&e)) == NULL)
		error("Read: Creation of ICC object failed with 0x%x, '%s'",e.c,e.m);

	if ((m = new_icmMD5_a(&e, icco->al)) == NULL)
		error("Read: Creation of MD5 object failed with 0x%x, '%s'",e.c,e.m);

	for (i = 0; ; i++) {
		if (tc[i].s == NULL)
			break;

		m->reset(m);

		m->add(m, (unsigned char *)tc[i].s, strlen(tc[i].s));
		m->get(m, chs2);

		/* Convert reference to a byte stream */
		chs1[0] = (tc[i].sum[0] >> 24) & 0xff,
		chs1[1] = (tc[i].sum[0] >> 16) & 0xff,
		chs1[2] = (tc[i].sum[0] >> 8) & 0xff,
		chs1[3] = tc[i].sum[0] & 0xff,
		chs1[4] = (tc[i].sum[1] >> 24) & 0xff,
		chs1[5] = (tc[i].sum[1] >> 16) & 0xff,
		chs1[6] = (tc[i].sum[1] >> 8) & 0xff,
		chs1[7] = tc[i].sum[1] & 0xff,
		chs1[8] = (tc[i].sum[2] >> 24) & 0xff,
		chs1[9] = (tc[i].sum[2] >> 16) & 0xff,
		chs1[10] = (tc[i].sum[2] >> 8) & 0xff,
		chs1[11] = tc[i].sum[2] & 0xff,
		chs1[12] = (tc[i].sum[3] >> 24) & 0xff,
		chs1[13] = (tc[i].sum[3] >> 16) & 0xff,
		chs1[14] = (tc[i].sum[3] >> 8) & 0xff,
		chs1[15] = tc[i].sum[3] & 0xff;

		for (j = 0; j < 16; j++) {
			if (chs1[j] != chs2[j]) {
				printf("MD5 check on '%s' fails at %d with:\n",tc[i].s,j);
				printf("Sum is    %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
					chs2[0], chs2[1], chs2[2], chs2[3], chs2[4], chs2[5], chs2[6], chs2[7],
					chs2[8], chs2[9], chs2[10], chs2[11], chs2[12], chs2[13], chs2[14], chs2[15]);
				printf("Should be %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
					chs1[0], chs1[1], chs1[2], chs1[3], chs1[4], chs1[5], chs1[6], chs1[7],
					chs1[8], chs1[9], chs1[10], chs1[11], chs1[12], chs1[13], chs1[14], chs1[15]);
				rv = 1;
				break;
			}
		}

	}
	m->del(m);
	icco->del(icco);

	return rv;
}

/* -------------------------------------------------------------- */
/* This is the code that inits and checks the header and tag data. */
/* Note that to undestand this, you need a copy of the ICC profile */
/* spec., and a copy of the icc header file (icc34.h in this code) */
/* All items that begin "icXXX" are from the ICC generic icc34.h file, */
/* while the items that begin "icmXXX" are machine versions of structures */
/* that are specific to this library. */

int doit(
	int mode,		/* 0 - write, 1 = read/verify */
	icc *wr_icco,	/* The write icc object */
	icc *rd_icco	/* The read icc object */
) {
	int rv = 0;

	/* ----------- */
	/* The header: */
	if (mode == 0) {
		icmHeader *wh = wr_icco->header;

		/* Values that must be set before writing */
// ~~~999
//		wh->deviceClass     = icSigAbstractClass;
		wh->deviceClass     = rand_ProfileClassSignature();
    	wh->colorSpace      = rand_ColorSpaceSignature();
    	wh->pcs             = rand_PCS();
    	wh->renderingIntent = rand_RenderingIntent();

		/* Values that should be set before writing */
		wh->manufacturer = icmstr2tag("tst1");
    	wh->model        = icmstr2tag("1234");
    	wh->attributes.l = rand_DeviceAttributes();
    	wh->flags = rand_ProfileHeaderFlags();

		/* Values that may optionally be set before writing */
    	wh->attributes.h = 0x12345678;
    	wh->creator = icmstr2tag("tst2");

		/* Values that are not normally set. Set them to non-defaults for testing */
		wh->cmmId = icmstr2tag("tst3");
    	wh->vers.majv = 2;					/* Default version 2.1.0 */
		wh->vers.minv = 1;
		wh->vers.bfv  = 0;
    	wh->date.year    = rand_int(1900,3000);		/* Defaults to current date */
    	wh->date.month   = rand_int(1,12);
    	wh->date.day     = rand_int(1,31);
    	wh->date.hours   = rand_int(0,23);
    	wh->date.minutes = rand_int(0,59);
    	wh->date.seconds = rand_int(0,59);
    	wh->platform     = rand_PlatformSignature();
    	wh->illuminant.X = rand_XYZ16();		/* Defaults to D50 */
    	wh->illuminant.Y = rand_XYZ16();
    	wh->illuminant.Z = rand_XYZ16();
	} else {
		icmHeader *rh = rd_icco->header;
		icmHeader *wh = wr_icco->header;

		/* Check all the values */
		rv |= (rh->deviceClass != wh->deviceClass);
    	rv |= (rh->colorSpace != wh->colorSpace);
    	rv |= (rh->pcs != wh->pcs);
    	rv |= (rh->renderingIntent != wh->renderingIntent);
		rv |= (rh->manufacturer != wh->manufacturer);
    	rv |= (rh->model != wh->model);
    	rv |= (rh->attributes.l != wh->attributes.l);
    	rv |= (rh->attributes.h != wh->attributes.h);
    	rv |= (rh->flags != wh->flags);
    	rv |= (rh->creator != wh->creator);
		rv |= (rh->cmmId != wh->cmmId);
    	rv |= (rh->vers.majv != wh->vers.majv);
		rv |= (rh->vers.minv != wh->vers.minv);
		rv |= (rh->vers.bfv != wh->vers.bfv);
    	rv |= (rh->date.year != wh->date.year);
    	rv |= (rh->date.month != wh->date.month);
    	rv |= (rh->date.day != wh->date.day);
    	rv |= (rh->date.hours != wh->date.hours);
    	rv |= (rh->date.minutes != wh->date.minutes);
    	rv |= (rh->date.seconds != wh->date.seconds);
    	rv |= (rh->platform != wh->platform);
    	rv |= dcomp(rh->illuminant.X, wh->illuminant.X);
    	rv |= dcomp(rh->illuminant.Y, wh->illuminant.Y);
    	rv |= dcomp(rh->illuminant.Z, wh->illuminant.Z);
		if (rv)
			error ("Header verify failed");
	}
	/* --------------------- */
	/* Unknown */ 
	{
	icTagSignature sig = ((icTagTypeSignature)icmMakeTag('5','6','7','8'));
	static icmUnknown *wo;

	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmUnknown *)wr_icco->add_tag(
		           wr_icco, sig,	icmSigUnknownType)) == NULL) { 
			return 1;
		}

		wo->uttype = icmMakeTag('1','2','3','4');
		wo->count = rand_int(0,1034);	/* Space we need for data */
		wo->allocate(wo);/* Allocate space */
		for (i = 0; i < wo->count; i ++) 
			wo->data[i] = rand_o8();
	} else {
		icmUnknown *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmUnknown *)rd_icco->read_tag_any(rd_icco, sig);
		if (ro == NULL) { 
			return 1;
		}

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icmSigUnknownType)
			return 1;
	
		if (ro->uttype != wo->uttype)
			error ("Unknown uttype doesn't match");

		if (ro->count != wo->count)
			error ("Unknown count doesn't match (found %d should be %d)",ro->count,wo->count);

		for (i = 0; i < wo->count; i++)
			rv |= (ro->data[i] != wo->data[i]);

		if (rv)
			error ("Unknown verify failed");
	}
	}
	/* ------------- */
	/* CrdInfo info: */
	{
	char *str1 = "Product Name";
	char *str2[4] = { "Intent zero CRD Name",
	                  "Intent one CRD Name",
	                  "Intent two CRD Name",
	                  "Intent three CRD Name" };
	static icmCrdInfo *wo;
	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmCrdInfo *)wr_icco->add_tag(
		           wr_icco, icSigCrdInfoTag, icSigCrdInfoType)) == NULL) 
			return 1;

		wo->ppsize = strlen(str1)+1; 			/* Allocated and used size of text, inc null */
		for (i = 0; i < 4; i++)
			wo->crdsize[i] = strlen(str2[i])+1;	/* Allocated and used size of text, inc null */

		wo->allocate(wo);			/* Allocate space */
		/* Note we could allocate and copy as we go, rather than doing them all at once. */

		strcpy(wo->ppname, str1);				/* Copy the text in */
		for (i = 0; i < 4; i++)
			strcpy(wo->crdname[i], str2[i]);	/* Copy the text in */
	} else {
		icmCrdInfo *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmCrdInfo *)rd_icco->read_tag(rd_icco, icSigCrdInfoTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigCrdInfoType)
			return 1;
	
		/* Now check it out */	
		if (ro->ppsize != wo->ppsize)
		for (i = 0; i < 4; i++) {
			if (ro->crdsize[i] != wo->crdsize[i])
				error ("CrdInfo crdsize[%d] doesn't match",i);
		}

		rv |= strcmp(ro->ppname, wo->ppname);
		for (i = 0; i < 4; i++) {
			rv |= strcmp(ro->crdname[i], wo->crdname[i]);
		}

		if (rv)
			error ("CrdInfo verify failed");
	}
	}
	/* ---------------------- */
	/* Curve - Linear version */
	{
	static icmCurve *wo;
	if (mode == 0) {
		if ((wo = (icmCurve *)wr_icco->add_tag(
		           wr_icco, icSigRedTRCTag, icSigCurveType)) == NULL) 
			return 1;

		wo->flag = icmCurveLin; 	/* Linear version */
		wo->allocate(wo);/* Allocate space */
	} else {
		icmCurve *ro;

		/* Try and read the tag from the file */
		ro = (icmCurve *)rd_icco->read_tag(rd_icco, icSigRedTRCTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigCurveType)
			return 1;
	
		/* Now check it out */	
		if (ro->flag != wo->flag)
			error ("Curve flag doesn't match for Linear");

		if (ro->count != wo->count)
			error ("Curve size doesn't match");

		if (rv)
			error ("Curve verify failed");
	}
	}
	/* --------------------- */
	/* Curve - Gamma version */
	{
	static icmCurve *wo;
	if (mode == 0) {
		if ((wo = (icmCurve *)wr_icco->add_tag(
		           wr_icco, icSigGreenTRCTag, icSigCurveType)) == NULL) 
			return 1;

		wo->flag = icmCurveGamma; 		/* Gamma version */
		wo->allocate(wo);	/* Allocate space */
		wo->data[0] = rand_u8f8();		/* Gamma value */
	} else {
		icmCurve *ro;

		/* Try and read the tag from the file */
		ro = (icmCurve *)rd_icco->read_tag(rd_icco, icSigGreenTRCTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigCurveType)
			return 1;
	
		/* Now check it out */	
		if (ro->flag != wo->flag)
			error ("Curve flag doesn't match for Gamma");

		if (ro->count != wo->count)
			error ("Curve size doesn't match");

		rv |= dcomp(ro->data[0], wo->data[0]);

		if (rv)
			error ("Curve verify failed");
	}
	}
	/* ------------------------- */
	/* Curve - Specified version */
	{
	static icmCurve *wo;
	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmCurve *)wr_icco->add_tag(
		           wr_icco, icSigBlueTRCTag, icSigCurveType)) == NULL) 
			return 1;

		wo->flag = icmCurveSpec; 	/* Specified version */
		wo->count = rand_int(2,23);	/* Number of entries (min must be 2!) */
		wo->allocate(wo);/* Allocate space */
		for (i = 0; i < wo->count; i++)
			wo->data[i] = rand_16f();	/* Curve values 0.0 - 1.0 */
	} else {
		icmCurve *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmCurve *)rd_icco->read_tag(rd_icco, icSigBlueTRCTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigCurveType)
			return 1;
	
		/* Now check it out */	
		if (ro->flag != wo->flag)
			error ("Curve flag doesn't match for specified");

		if (ro->count != wo->count)
			error ("Curve size doesn't match");

		for (i = 0; i < wo->count; i++)
			rv |= dcomp(ro->data[i], wo->data[i]);

		if (rv)
			error ("Curve verify failed");
	}
	}
	/* ------------------- */
	/* Data - text version */
	{
	static icmData *wo;
	char *ts1 = "This is a data string";
	if (mode == 0) {
		if ((wo = (icmData *)wr_icco->add_tag(
		           wr_icco, icSigPs2CRD0Tag,	icSigDataType)) == NULL) 
			return 1;

		wo->flag = icmDataASCII;	/* Holding ASCII data */
		wo->count = strlen(ts1)+1; 	/* Allocated and used size of text, inc null */
		wo->allocate(wo);/* Allocate space */
		strcpy((char *)wo->data, ts1);		/* Copy the text in */
	} else {
		icmData *ro;

		/* Try and read the tag from the file */
		ro = (icmData *)rd_icco->read_tag(rd_icco, icSigPs2CRD0Tag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigDataType)
			return 1;
	
		/* Now check it out */	
		if (ro->flag != wo->flag)
			error ("Data size doesn't match");

		if (ro->count != wo->count)
			error ("Data size doesn't match");

		rv |= strcmp((char *)ro->data, (char *)wo->data);

		if (rv)
			error ("Data verify failed");
	}
	}
	/* --------------------- */
	/* Data - Binary version */
	{
	static icmData *wo;
	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmData *)wr_icco->add_tag(
		           wr_icco, icSigPs2CRD1Tag,	icSigDataType)) == NULL) 
			return 1;

		wo->flag = icmDataBin;		/* Holding binary data */
		wo->count = rand_int(0,43);	/* Space we need for data */
		wo->allocate(wo);/* Allocate space */
		for (i = 0; i < wo->count; i ++) 
			wo->data[i] = rand_o8();
	} else {
		icmData *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmData *)rd_icco->read_tag(rd_icco, icSigPs2CRD1Tag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigDataType)
			return 1;
	
		/* Now check it out */	
		if (ro->flag != wo->flag)
			error ("Data size doesn't match");

		if (ro->count != wo->count)
			error ("Data size doesn't match");

		for (i = 0; i < wo->count; i++)
			rv |= (ro->data[i] != wo->data[i]);

		if (rv)
			error ("Data verify failed");
	}
	}
	/* --------- */
	/* DateTime: */
	{
	static icmDateTime *wo;
	if (mode == 0) {
		if ((wo = (icmDateTime *)wr_icco->add_tag(
		           wr_icco, icSigCalibrationDateTimeTag, icSigDateTimeType)) == NULL) 
			return 1;

    	wo->date.year = rand_int(1900, 3000);
    	wo->date.month = rand_int(1, 12);
    	wo->date.day = rand_int(1, 31);
    	wo->date.hours = rand_int(0, 23);
    	wo->date.minutes = rand_int(0, 59);
    	wo->date.seconds = rand_int(0, 59);
	} else {
		icmDateTime *ro;

		/* Try and read the tag from the file */
		ro = (icmDateTime *)rd_icco->read_tag(rd_icco, icSigCalibrationDateTimeTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigDateTimeType)
			return 1;
	
		/* Now check it out */	
    	rv |= (ro->date.year    != wo->date.year);
    	rv |= (ro->date.month   != wo->date.month);
    	rv |= (ro->date.day     != wo->date.day);
    	rv |= (ro->date.hours   != wo->date.hours);
    	rv |= (ro->date.minutes != wo->date.minutes);
    	rv |= (ro->date.seconds != wo->date.seconds);

		if (rv)
			error ("DateTime verify failed");
	}
	}
	/* ----------- */
	/* 16 bit lut: */
	{
	static icmLut *wo;
	if (mode == 0) {
		unsigned int i, j, k;
		if ((wo = (icmLut *)wr_icco->add_tag(
		           wr_icco, icSigAToB0Tag,	icSigLut16Type)) == NULL) 
			return 1;

		wo->inputChan = 2;
		wo->outputChan = 3;
    	wo->clutPoints = 5;
    	wo->inputEnt = 56;
    	wo->outputEnt = 73;
		wo->allocate(wo);/* Allocate space */

		/* The matrix is only applicable to XYZ input space */
		for (i = 0; i < 3; i++)							/* Matrix */
			for (j = 0; j < 3; j++)
				wo->e[i][j] = rand_s15f16();

		/* See icc.getNormFuncs() for normalizing functions */
		/* The input table index range is over the normalized range 0.0 - 1.0. */
		/* The range in input color space can be determined by denormalizing */
		/* the values 0.0 - 1.0. */
		for (i = 0; i < wo->inputChan; i++)				/* Input tables */
			for (j = 0; j < wo->inputEnt; j++)
				wo->inputTable[i * wo->inputEnt + j] = rand_16f();

														/* Lut */
		/* The multidimentional lut has a normalized index range */
		/* of 0.0 - 1.0 in each dimension. Its entry values are also */
		/* normalized values in the range 0.0 - 1.0. */
		for (i = 0; i < wo->clutPoints; i++)			/* Input chan 0 - slow changing */
			for (j = 0; j < wo->clutPoints; j++)		/* Input chan 1 - faster changing */
				for (k = 0; k < wo->outputChan; k++)	/* Output chans */
					wo->clutTable[(i * wo->clutPoints + j) * wo->outputChan + k] = rand_16f();

		/* The output color space values should be normalized to the */
		/* range 0.0 - 1.0 for use as output table entry values. */
		for (i = 0; i < wo->outputChan; i++)			/* Output tables */
			for (j = 0; j < wo->outputEnt; j++)
				wo->outputTable[i * wo->outputEnt + j] = rand_16f();

	} else {
		icmLut *ro;
		unsigned int size;
		unsigned int i, j;

		/* Try and read the tag from the file */
		ro = (icmLut *)rd_icco->read_tag(rd_icco, icSigAToB0Tag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigLut16Type)
			return 1;
	
		/* Now check it out */	
		rv |= (ro->inputChan != wo->inputChan);
		rv |= (ro->outputChan != wo->outputChan);
    	rv |= (ro->clutPoints != wo->clutPoints);
    	rv |= (ro->inputEnt != wo->inputEnt);
    	rv |= (ro->outputEnt != wo->outputEnt);

		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				rv |= dcomp(ro->e[i][j], wo->e[i][j]);

		size = (wo->inputChan * wo->inputEnt);
		for (i = 0; i < size; i++)
			rv |= dcomp(ro->inputTable[i], wo->inputTable[i]);

		size = wo->outputChan;
		for (i = 0; i < wo->inputChan; i++)
			size *= wo->clutPoints;
		for (i = 0; i < size; i++)
			rv |= dcomp(ro->clutTable[i], wo->clutTable[i]);

		size = (wo->outputChan * wo->outputEnt);
		for (i = 0; i < size; i++)
			rv |= dcomp(ro->outputTable[i], wo->outputTable[i]);
		if (rv)
			error ("Lut16 verify failed");
	}
	}
	/* ------------------ */
	/* 16 bit lut - link: */
	{
	static icmLut *wo;
	if (mode == 0) {
		/* Just link to the existing LUT. This is often used when there */
		/* is no distinction between intents, and saves file and memory space. */
		if ((wo = (icmLut *)wr_icco->link_tag(
		           wr_icco, icSigAToB1Tag,	icSigAToB0Tag)) == NULL) 
			return 1;
	} else {
		icmLut *ro;
		unsigned int size;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmLut *)rd_icco->read_tag(rd_icco, icSigAToB1Tag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigLut16Type)
			return 1;
	
		/* Now check it out */	
		rv |= (ro->inputChan != wo->inputChan);
		rv |= (ro->outputChan != wo->outputChan);
    	rv |= (ro->clutPoints != wo->clutPoints);
    	rv |= (ro->inputEnt != wo->inputEnt);
    	rv |= (ro->outputEnt != wo->outputEnt);

		size = (wo->inputChan * wo->inputEnt);
		for (i = 0; i < size; i++)
			rv |= (ro->inputTable[i] != wo->inputTable[i]);

		size = wo->outputChan;
		for (i = 0; i < wo->inputChan; i++)
			size *= wo->clutPoints;
		for (i = 0; i < size; i++)
			rv |= (ro->clutTable[i] != wo->clutTable[i]);

		size = (wo->outputChan * wo->outputEnt);
		for (i = 0; i < size; i++)
			rv |= (ro->outputTable[i] != wo->outputTable[i]);
		if (rv)
			error ("Lut16 link verify failed");
	}
	}
	/* ---------- */
	/* 8 bit lut: */
	{
	static icmLut *wo;
	if (mode == 0) {
		unsigned int i, j, m, k;
		if ((wo = (icmLut *)wr_icco->add_tag(
		           wr_icco, icSigAToB2Tag,	icSigLut8Type)) == NULL) 
			return 1;

		wo->inputChan = 3;
		wo->outputChan = 2;
    	wo->clutPoints = 4;
    	wo->inputEnt = 256;			/* Must be 256 for Lut8 */
    	wo->outputEnt = 256;
		wo->allocate(wo);/* Allocate space */

		for (i = 0; i < 3; i++)						/* Matrix */
			for (j = 0; j < 3; j++)
				wo->e[i][j] = rand_s15f16();

		for (i = 0; i < wo->inputChan; i++)			/* Input tables */
			for (j = 0; j < wo->inputEnt; j++)
				wo->inputTable[i * wo->inputEnt + j] = rand_8f();

													/* Lut */
		for (i = 0; i < wo->clutPoints; i++)				/* Input chan 0 */
			for (j = 0; j < wo->clutPoints; j++)			/* Input chan 1 */
				for (m = 0; m < wo->clutPoints; m++)		/* Input chan 2 */
					for (k = 0; k < wo->outputChan; k++) {	/* Output chans */
						int idx = ((i * wo->clutPoints + j)
						              * wo->clutPoints + m)
						              * wo->outputChan + k;
						wo->clutTable[idx] = rand_8f();
						}

		for (i = 0; i < wo->outputChan; i++)		/* Output tables */
			for (j = 0; j < wo->outputEnt; j++)
				wo->outputTable[i * wo->outputEnt + j] = rand_8f();

	} else {
		icmLut *ro;
		unsigned int size;
		unsigned int i, j;

		/* Try and read the tag from the file */
		ro = (icmLut *)rd_icco->read_tag(rd_icco, icSigAToB2Tag);
		if (ro == NULL)
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigLut8Type)
			return 1;
	
		/* Now check it out */	
		rv |= (ro->inputChan != wo->inputChan);
		rv |= (ro->outputChan != wo->outputChan);
    	rv |= (ro->clutPoints != wo->clutPoints);
    	rv |= (ro->inputEnt != wo->inputEnt);
    	rv |= (ro->outputEnt != wo->outputEnt);

		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				rv |= dcomp(ro->e[i][j], wo->e[i][j]);

		size = (wo->inputChan * wo->inputEnt);
		for (i = 0; i < size; i++)
			rv |= dcomp(ro->inputTable[i], wo->inputTable[i]);

		size = wo->outputChan;
		for (i = 0; i < wo->inputChan; i++)
			size *= wo->clutPoints;
		for (i = 0; i < size; i++)
			rv |= dcomp(ro->clutTable[i], wo->clutTable[i]);

		size = (wo->outputChan * wo->outputEnt);
		for (i = 0; i < size; i++)
			rv |= dcomp(ro->outputTable[i], wo->outputTable[i]);
		if (rv)
			error ("Lut8 verify failed");
	}
	}
	/* ----------------- */
	/* Measurement: */
	{
	static icmMeasurement *wo;
	if (mode == 0) {
		if ((wo = (icmMeasurement *)wr_icco->add_tag(
		           wr_icco, icSigMeasurementTag, icSigMeasurementType)) == NULL) 
			return 1;

		/* Standard observer */
		switch(rand_int(0,2)) {
			case 0:
	    		wo->observer = icStdObsUnknown;
				break;
			case 1:
	    		wo->observer = icStdObs1931TwoDegrees;
				break;
			case 2:
	    		wo->observer = icStdObs1964TenDegrees;
				break;
		}

		/* XYZ for backing color */
    	wo->backing.X = rand_XYZ16();
    	wo->backing.Y = rand_XYZ16();
    	wo->backing.Z = rand_XYZ16();

	    /* Measurement geometry */
		switch(rand_int(0,2)) {
			case 0:
    			wo->geometry = icGeometryUnknown;
				break;
			case 1:
    			wo->geometry = icGeometry045or450;
				break;
			case 2:
    			wo->geometry = icGeometry0dord0;
				break;
		}

	    /* Measurement flare */
		wo->flare = rand_u16f16();

	    /* Illuminant */
		switch(rand_int(0,8)) {
			case 0:
    			wo->illuminant = icIlluminantUnknown;
				break;
			case 1:
    			wo->illuminant = icIlluminantD50;
				break;
			case 2:
    			wo->illuminant = icIlluminantD65;
				break;
			case 3:
    			wo->illuminant = icIlluminantD93;
				break;
			case 4:
    			wo->illuminant = icIlluminantF2;
				break;
			case 5:
    			wo->illuminant = icIlluminantD55;
				break;
			case 6:
    			wo->illuminant = icIlluminantA;
				break;
			case 7:
    			wo->illuminant = icIlluminantEquiPowerE;
				break;
			case 8:
    			wo->illuminant = icIlluminantF8;
				break;
			}
	} else {
		icmMeasurement *ro;

		/* Try and read the tag from the file */
		ro = (icmMeasurement *)rd_icco->read_tag(rd_icco, icSigMeasurementTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigMeasurementType)
			return 1;
	
		/* Now check it out */	
	    rv |= (ro->observer   != wo->observer);
    	rv |= dcomp(ro->backing.X, wo->backing.X);
    	rv |= dcomp(ro->backing.Y, wo->backing.Y);
    	rv |= dcomp(ro->backing.Z, wo->backing.Z);
		rv |= (ro->geometry   != wo->geometry);
    	rv |= dcomp(ro->flare, wo->flare);
    	rv |= (ro->illuminant != wo->illuminant);

		if (rv)
			error ("Measurement verify failed");
	}
	}
	/* ----------------- */
	/* Old style NamedColor: */
	{
	static icmNamedColor *wo;
	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmNamedColor *)wr_icco->add_tag(
		           wr_icco, icSigNamedColorTag, icSigNamedColorType)) == NULL) 
			return 1;

   		wo->vendorFlag = rand_int(0,65535) << 16;	/* Bottom 16 bits for IC use */
   		wo->count = 3;			/* Count of named colors */
		strcpy(wo->prefix,"Prefix"); /* Prefix for each color name, max 32, null terminated */
		strcpy(wo->suffix,"Suffix"); /* Suffix for each color name, max 32, null terminated */

		wo->allocate(wo);	/* Allocate named color structures */

		for (i = 0; i < wo->count; i++) {
			unsigned int j;
			sprintf(wo->data[i].root,"Color %d",i); /* Root name, max 32, null terminated */
			for (j = 0; j < wo->nDeviceCoords; j++)	/* nDeviceCoords defaults appropriately */
				wo->data[i].deviceCoords[j] = rand_8f(); /* Device coords of color */
		}
	} else {
		icmNamedColor *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmNamedColor *)rd_icco->read_tag(rd_icco, icSigNamedColorTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigNamedColorType)
			return 1;

		/* Now check it out */	
   		rv |= (ro->vendorFlag != wo->vendorFlag);
    	rv |= (ro->count != wo->count);
    	rv |= (ro->nDeviceCoords != wo->nDeviceCoords);
		rv |= strcmp(ro->prefix, wo->prefix);
		rv |= strcmp(ro->suffix, wo->suffix);

		if (rv)
			error ("NamedColor verify failed");

		for (i = 0; i < wo->count; i++) {
			unsigned int j;
			rv |= strcmp(ro->data[i].root, wo->data[i].root);
			for (j = 0; j < wo->nDeviceCoords; j++)
				rv |= dcomp(ro->data[i].deviceCoords[j], wo->data[i].deviceCoords[j]);
		}

		if (rv)
			error ("NamedColor verify failed");
	}
	}
	/* ----------------- */
	/* NamedColor2: */
	{
	static icmNamedColor *wo;	/* Shares same machine specific structure */
	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmNamedColor *)wr_icco->add_tag(
		           wr_icco, icSigNamedColor2Tag, icSigNamedColor2Type)) == NULL) 
			return 1;

   		wo->vendorFlag = rand_int(0,65535) << 16;	/* Bottom 16 bits for ICC use */
   		wo->count = 4;			/* Count of named colors */
   		wo->nDeviceCoords =	3;	/* Num of device coordinates */
		         /* Could set this different to that implied by wr_icco->header->colorSpace */
		strcpy(wo->prefix,"Prefix-ix"); /* Prefix for each color name, max 32, null terminated */
		strcpy(wo->suffix,"Suffix-ixix"); /* Suffix for each color name, max 32, null terminated */

		wo->allocate(wo);	/* Allocate named color structures */

		for (i = 0; i < wo->count; i++) {
			unsigned int j;
			sprintf(wo->data[i].root,"Pigment %d",i); /* Root name, max 32, null terminated */
			for (j = 0; j < wo->nDeviceCoords; j++)
				wo->data[i].deviceCoords[j] = rand_8f(); /* Device coords of color */
			switch(wo->icp->header->pcs) {
				case icSigXYZData:
						wo->data[i].pcsCoords[0] = rand_XYZ16();
						wo->data[i].pcsCoords[1] = rand_XYZ16();
						wo->data[i].pcsCoords[2] = rand_XYZ16();
					break;
			   	case icSigLabData:
						wo->data[i].pcsCoords[0] = rand_L16();
						wo->data[i].pcsCoords[1] = rand_ab16();
						wo->data[i].pcsCoords[2] = rand_ab16();
					break;
				default:
					break;
			}
		}
	} else {
		icmNamedColor *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmNamedColor *)rd_icco->read_tag(rd_icco, icSigNamedColor2Tag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigNamedColor2Type)
			return 1;

		/* Now check it out */	
   		rv |= (ro->vendorFlag != wo->vendorFlag);
    	rv |= (ro->count != wo->count);
    	rv |= (ro->nDeviceCoords != wo->nDeviceCoords);
		rv |= strcmp(ro->prefix, wo->prefix);
		rv |= strcmp(ro->suffix, wo->suffix);

		if (rv)
			error ("NamedColor2 verify failed");

		for (i = 0; i < wo->count; i++) {
			unsigned int j;
			rv |= strcmp(ro->data[i].root, wo->data[i].root);
			for (j = 0; j < wo->nDeviceCoords; j++)
				rv |= dcomp(ro->data[i].deviceCoords[j], wo->data[i].deviceCoords[j]);
			for (j = 0; j < 3; j++) {
				rv |= dcomp(ro->data[i].pcsCoords[j], wo->data[i].pcsCoords[j]);
			}
		}

		if (rv)
			error ("NamedColor2 verify failed");
	}
	}
	/* ----------------- */
	/* ColorantTable: */
	{
	static icmColorantTable *wo;
	if (mode == 0) {
		unsigned int i;
		icColorSpaceSignature pcs;

		if (wr_icco->header->deviceClass != icSigLinkClass)
			pcs = wr_icco->header->pcs;
		else
			pcs = icSigLabData;

		if ((wo = (icmColorantTable *)wr_icco->add_tag(
		           wr_icco, icSigColorantTableTag, icSigColorantTableType)) == NULL) 
			return 1;

   		wo->count = 4;		/* Count of colorants - should be same as implied by device space */
		wo->allocate(wo);	/* Allocate ColorantTable structures */

		for (i = 0; i < wo->count; i++) {
			sprintf(wo->data[i].name,"Color %d",i); /* Colorant name, max 32, null terminated */
			switch(pcs) {
				case icSigXYZData:
						wo->data[i].pcsCoords[0] = rand_XYZ16();
						wo->data[i].pcsCoords[1] = rand_XYZ16();
						wo->data[i].pcsCoords[2] = rand_XYZ16();
					break;
			   	case icSigLabData:
						wo->data[i].pcsCoords[0] = rand_L16();
						wo->data[i].pcsCoords[1] = rand_ab16();
						wo->data[i].pcsCoords[2] = rand_ab16();
					break;
				default:
					break;
			}
		}
	} else {
		icmColorantTable *ro;
		unsigned int i;
		icColorSpaceSignature pcs;

		/* Try and read the tag from the file */
		ro = (icmColorantTable *)rd_icco->read_tag(rd_icco, icSigColorantTableTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigColorantTableType)
			return 1;

		/* Now check it out */	
    	rv |= (ro->count != wo->count);

		if (rv)
			error ("ColorantTable count verify failed");

		for (i = 0; i < wo->count; i++) {
			int j;
			rv |= strcmp(ro->data[i].name, wo->data[i].name);
			for (j = 0; j < 3; j++) {
				rv |= dcomp(ro->data[i].pcsCoords[j], wo->data[i].pcsCoords[j]);
			}
		}

		if (rv)
			error ("ColorantTable name/value verify failed");
	}
	}
	/* ----------------- */
	/* ProfileSequenceDescTag: */
	{

	static icmProfileSequenceDesc *wo;
	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmProfileSequenceDesc *)wr_icco->add_tag(
		           wr_icco, icSigProfileSequenceDescTag, icSigProfileSequenceDescType)) == NULL) 
			return 1;

		wo->count = 3; 		/* Number of descriptions in sequence */
		wo->allocate(wo);	/* Allocate space for all the DescStructures */

		/* Fill in each description structure in sequence */
		for (i = 0; i < wo->count; i++) {
			char *ts1, *ts2, *ts3;

			wo->data[i].deviceMfg   = icmstr2tag("mfg7");
			wo->data[i].deviceModel = icmstr2tag("2345");
			wo->data[i].attributes.l = icTransparency | icMatte;
			wo->data[i].attributes.h = 0x98765432;
			wo->data[i].technology = rand_TechnologySignature();

			/* device Text description */
			ts1 = rand_ascii_string(9, 81);
			ts2 = rand_utf8_string(8, 111);
			ts3 = rand_ascii_string(7, 67-1);

			wo->data[i].device.count = strlen(ts1)+1;
			wo->data[i].allocate(&wo->data[i]); 		/* Allocate space */
			strcpy(wo->data[i].device.desc, ts1);		/* Copy the string in */
	
			/* utf-8 */
			wo->data[i].device.ucLangCode = 8765;		/* UniCode language code */
			wo->data[i].device.ucCount = strlen(ts2)+1;	/* Size in chars inc null */
			wo->data[i].allocate(&wo->data[i]);			/* Allocate space */
			strcpy((icmUTF8 *)wo->data[i].device.uc8Desc, ts2);	/* Copy the string in */
	
			/* Script code */
			wo->data[i].device.scCode = 37;				/* Fudge scriptCode code */
			wo->data[i].device.scCount = strlen(ts3)+1;	/* Used size of scDesc in bytes, inc null */
			if (wo->data[i].device.scCount > 67)
				error("Device ScriptCode string longer than 67 (%d)",wo->data[i].device.scCount);
			strcpy((char *)wo->data[i].device.scDesc, ts3);	/* Copy the string in */

			free(ts1);
			free(ts2);
			free(ts3);

			/* model Text description */
			ts1 = rand_ascii_string(9, 81);
			ts2 = rand_utf8_string(8, 111);
			ts3 = rand_ascii_string(7, 67-1);

			wo->data[i].model.count = strlen(ts1)+1;
			wo->data[i].allocate(&wo->data[i]);			/* Allocate space */
			strcpy(wo->data[i].model.desc, ts1);		/* Copy the string in */
	
			/* We'll fudge up the Unicode string */
			wo->data[i].model.ucLangCode = 7856;		/* UniCode language code */
			wo->data[i].model.ucCount = strlen(ts2)+1;	/* Size in chars inc null */
			wo->data[i].allocate(&wo->data[i]);			/* Allocate space */
			strcpy((icmUTF8 *)wo->data[i].model.uc8Desc, ts2);	/* Copy string in */
	
			wo->data[i].model.scCode = 67;				/* Fudge scriptCode code */
			wo->data[i].model.scCount = strlen(ts3)+1;	/* Used size of scDesc in bytes, inc null */
			if (wo->data[i].model.scCount > 67)
				error("Model ScriptCode string longer than 67 (%d)",wo->data[i].model.scCount);
			strcpy((char *)wo->data[i].model.scDesc, ts3);	/* Copy the string in */

			free(ts1);
			free(ts2);
			free(ts3);
		}
	} else {
		icmProfileSequenceDesc *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmProfileSequenceDesc *)rd_icco->read_tag(rd_icco, icSigProfileSequenceDescTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigProfileSequenceDescType)
			return 1;
	
		/* Now check it out */	
		if (ro->count != wo->count)
			error ("ProfileSequenceDesc count doesn't match");

		for (i = 0; i < wo->count; i++) {
			char *ts1, *ts2, *ts3;

			rv |= (ro->data[i].deviceMfg    != wo->data[i].deviceMfg);
			rv |= (ro->data[i].deviceModel  != wo->data[i].deviceModel);
			rv |= (ro->data[i].attributes.l != wo->data[i].attributes.l);
			rv |= (ro->data[i].attributes.h != wo->data[i].attributes.h);
			rv |= (ro->data[i].technology   != wo->data[i].technology);

			/* device Text description */
			ts1 = rand_ascii_string(9, 81);
			ts2 = rand_utf8_string(8, 111);
			ts3 = rand_ascii_string(7, 67);

			rv |= (ro->data[i].device.count  != wo->data[i].device.count);
			rv |= strcmp(ro->data[i].device.desc, wo->data[i].device.desc);
	
			rv |= (ro->data[i].device.ucLangCode != wo->data[i].device.ucLangCode);
			rv |= (ro->data[i].device.ucCount != wo->data[i].device.ucCount);
			rv |= strcmp((icmUTF8 *)ro->data[i].device.uc8Desc, (icmUTF8 *)wo->data[i].device.uc8Desc);
	
			rv |= (ro->data[i].device.scCode != wo->data[i].device.scCode);
			rv |= (ro->data[i].device.scCount != wo->data[i].device.scCount);
			rv |= strcmp((char *)ro->data[i].device.scDesc, (char *)wo->data[i].device.scDesc);

			free(ts1);
			free(ts2);
			free(ts3);

			/* model Text description */
			ts1 = rand_ascii_string(9, 81);
			ts2 = rand_utf8_string(8, 111);
			ts3 = rand_ascii_string(7, 67);

			rv |= (ro->data[i].model.count  != wo->data[i].model.count);
			rv |= strcmp(ro->data[i].model.desc, wo->data[i].model.desc);
	
			rv |= (ro->data[i].model.ucLangCode != wo->data[i].model.ucLangCode);
			rv |= (ro->data[i].model.ucCount != wo->data[i].model.ucCount);
			rv |= strcmp((icmUTF8 *)ro->data[i].model.uc8Desc, (icmUTF8 *)wo->data[i].model.uc8Desc);
	
			rv |= (ro->data[i].model.scCode != wo->data[i].model.scCode);
			rv |= (ro->data[i].model.scCount != wo->data[i].model.scCount);
			rv |= strcmp((char *)ro->data[i].model.scDesc, (char *)wo->data[i].model.scDesc);

			free(ts1);
			free(ts2);
			free(ts3);
		}

		if (rv)
			error ("ProfileSequenceDesc verify failed");
	}
	}
	/* ----------------- */
	/* S15Fixed16Array: */
	{
	static icmS15Fixed16Array *wo;
	if (mode == 0) {
		unsigned int i;
		/* There is no standard Tag that uses icSigS15Fixed16ArrayType, so use a 'custom' tag */
		if ((wo = (icmS15Fixed16Array *)wr_icco->add_tag(
		           wr_icco, icmstr2tag("sf32"), icSigS15Fixed16ArrayType)) == NULL) 
			return 1;

		wo->count = rand_int(0,17);		/* Number in array */
		wo->allocate(wo);	/* Allocate space */
		for (i = 0; i < wo->count; i++) {
			wo->data[i] = rand_s15f16(); /* Set numbers value */
		}
	} else {
		icmS15Fixed16Array *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmS15Fixed16Array *)rd_icco->read_tag(rd_icco, icmstr2tag("sf32"));
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigS15Fixed16ArrayType)
			return 1;
	
		/* Now check it out */	
		if (ro->count != wo->count)
			error ("S15Fixed16Array size doesn't match");

		for (i = 0; i < wo->count; i++) {
			rv |= dcomp(ro->data[i], wo->data[i]);
		}

		if (rv)
			error ("S15Fixed16Array verify failed");
	}
	}
	/* ----------------- */
	/* Screening: */
	{
	static icmScreening *wo;
	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmScreening *)wr_icco->add_tag(
		           wr_icco, icSigScreeningTag, icSigScreeningType)) == NULL) 
			return 1;

		wo->screeningFlag = rand_ScreenEncodings();
		wo->channels = rand_int(1,4);	/* Number of channels */
		wo->allocate(wo);	/* Allocate space */
		for (i = 0; i < wo->channels; i++) {
			wo->data[i].frequency = rand_s15f16();		/* Set screening frequency */
			wo->data[i].angle = rand_s15f16();			/* Set screening angle */
			wo->data[i].spotShape = rand_SpotShape();	/* Set spot shape */
		}
	} else {
		icmScreening *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmScreening *)rd_icco->read_tag(rd_icco, icSigScreeningTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigScreeningType)
			return 1;
	
		/* Now check it out */	
		if (ro->channels != wo->channels)
			error ("Screening channels doesn't match");

		rv |= (ro->screeningFlag != wo->screeningFlag);

		for (i = 0; i < wo->channels; i++) {
			rv |= dcomp(ro->data[i].frequency, wo->data[i].frequency);
			rv |= dcomp(ro->data[i].angle, wo->data[i].angle);
			rv |= (ro->data[i].spotShape != wo->data[i].spotShape);
		}

		if (rv)
			error ("Screening verify failed");
	}
	}
	/* ----------------- */
	/* Signature: */
	{
	static icmSignature *wo;
	if (mode == 0) {
		if ((wo = (icmSignature *)wr_icco->add_tag(
		           wr_icco, icSigTechnologyTag, icSigSignatureType)) == NULL) 
			return 1;

		wo->sig = rand_TechnologySignature();
	} else {
		icmSignature *ro;

		/* Try and read the tag from the file */
		ro = (icmSignature *)rd_icco->read_tag(rd_icco, icSigTechnologyTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigSignatureType)
			return 1;
	
		/* Now check it out */	
		rv |= (ro->sig != wo->sig);

		if (rv)
			error ("Signature verify failed");
	}
	}
	/* ----------------- */
	/* Text Description: */
	{
	static icmTextDescription *wo;

	if (mode == 0) {
		char *ts1, *ts2, *ts3;

		if ((wo = (icmTextDescription *)wr_icco->add_tag(
		           wr_icco, icSigProfileDescriptionTag,	icSigTextDescriptionType)) == NULL) 
			return 1;

		/* Create the test messages */
		ts1 = rand_ascii_string(10, 90);
		ts2 = rand_utf8_string(3, 131);
		ts3 = rand_ascii_string(5, 67-1);

		/* Data in tag type wojects is always allocated and freed by the woject */
		wo->count = strlen(ts1)+1; 	/* Allocated and used size of desc, inc null */
		wo->allocate(wo);/* Allocate space */
		strcpy(wo->desc, ts1);		/* Copy the string in */

		/* We can treat utf-8 just like an ascii string */
		wo->ucLangCode = 1234;			/* UniCode language code */
		wo->ucCount = strlen(ts2)+1; 	/* Allocated and used size of desc, inc null */
		wo->allocate(wo);/* Allocate space */
		strcpy((char *)wo->uc8Desc, ts2);		/* Copy the string in */

		/* Don't really know anything about scriptCode, but fudge some values */
		wo->scCode = 23;			/* ScriptCode code */
		wo->scCount = strlen(ts3)+1;	/* Used size of scDesc in bytes, inc null */
					/* No allocations, since this has a fixed max of 67 bytes */
		if (wo->scCount > 67)
			error("textDescription ScriptCode string longer than 67 (%d)",wo->scCount);
		strcpy((char *)wo->scDesc, ts3);	/* Copy the string in */

		free(ts1);
		free(ts2);
		free(ts3);

	} else {
		icmTextDescription *ro;
		char *ts1, *ts2, *ts3;

		/* Try and read the tag from the file */
		ro = (icmTextDescription *)rd_icco->read_tag(rd_icco, icSigProfileDescriptionTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		/* We could have left it icmBase, switched on ro->ttype, & then cast appropriately. */
		if (ro->ttype != icSigTextDescriptionType)
			return 1;
	
		/* Create the test messages */
		ts1 = rand_ascii_string(10, 90);
		ts2 = rand_utf8_string(3, 131);
		ts3 = rand_ascii_string(5, 67-1);

		/* Now check it out */	
		rv |= (ro->count != wo->count);
		rv |= strcmp(ro->desc, wo->desc);
		rv |= (ro->ucLangCode != wo->ucLangCode);
		rv |= (ro->ucCount != wo->ucCount);
		rv |= strcmp((char *)ro->uc8Desc, (char *)wo->uc8Desc);

		rv |= (ro->scCode != wo->scCode);
		rv |= (ro->scCount != wo->scCount);
		rv |= strcmp((char *)ro->scDesc, (char *)wo->scDesc);

		free(ts1);
		free(ts2);
		free(ts3);

		if (rv)
			error ("Text Description verify failed");
	}
	}
	/* ----- */
	/* Text: */
	{
	static icmText *wo;
	char *ts1 = "This is Copyright by me!";
	if (mode == 0) {
		if ((wo = (icmText *)wr_icco->add_tag(
		           wr_icco, icSigCopyrightTag,	icSigTextType)) == NULL) 
			return 1;

		wo->count = strlen(ts1)+1; 	/* Allocated and used size of text, inc null */
		wo->allocate(wo);			/* Allocate space */
		strcpy(wo->data, ts1);		/* Copy the text in */
	} else {
		icmText *ro;

		/* Try and read the tag from the file */
		ro = (icmText *)rd_icco->read_tag(rd_icco, icSigCopyrightTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigTextType)
			return 1;
	
		/* Now check it out */	
		if (ro->count != wo->count)
			error ("Text size doesn't match");

		rv |= strcmp(ro->data, wo->data);

		if (rv)
			error ("Text verify failed");
	}
	}
	/* ---------------- */
	/* U16Fixed16Array: */
	{
	static icmU16Fixed16Array *wo;
	if (mode == 0) {
		unsigned int i;
		/* There is no standard Tag that uses icSigU16Fixed16ArrayType, so use a 'custom' tag */
		if ((wo = (icmU16Fixed16Array *)wr_icco->add_tag(
		           wr_icco, icmstr2tag("uf32"), icSigU16Fixed16ArrayType)) == NULL) 
			return 1;

		wo->count = rand_int(0,17);		/* Number in array */
		wo->allocate(wo);	/* Allocate space */
		for (i = 0; i < wo->count; i++) {
			wo->data[i] = rand_u16f16(); /* Set numbers value */
		}
	} else {
		icmU16Fixed16Array *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmU16Fixed16Array *)rd_icco->read_tag(rd_icco, icmstr2tag("uf32"));
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigU16Fixed16ArrayType)
			return 1;
	
		/* Now check it out */	
		if (ro->count != wo->count)
			error ("U16Fixed16Array size doesn't match");

		for (i = 0; i < wo->count; i++) {
			rv |= dcomp(ro->data[i], wo->data[i]);
		}

		if (rv)
			error ("U16Fixed16Array verify failed");
	}
	}
	/* ------------------- */
	/* UcrBg - full curve: */
	{
	static icmUcrBg *wo;
	char *ts1 = "UcrBg - full curve info";
	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmUcrBg *)wr_icco->add_tag(
		           wr_icco, icSigUcrBgTag, icSigUcrBgType)) == NULL) 
			return 1;

		wo->UCRcount = rand_int(2,55);		/* Number in UCR curve */
		wo->BGcount  = rand_int(2,32);		/* Number in BG array */
		wo->allocate(wo);		/* Allocate space for both curves */
		for (i = 0; i < wo->UCRcount; i++)
			wo->UCRcurve[i] = rand_16f();	/* Set numbers value */
		for (i = 0; i < wo->BGcount; i++)
			wo->BGcurve[i] = rand_16f();	/* Set numbers value */
		wo->count = strlen(ts1)+1; 			/* Allocated and used size of text, inc null */
		wo->allocate(wo);		/* Allocate space */
		strcpy(wo->string, ts1);			/* Copy the text in */
	} else {
		icmUcrBg *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmUcrBg *)rd_icco->read_tag(rd_icco, icSigUcrBgTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigUcrBgType)
			return 1;
	
		/* Now check it out */	
		if (ro->UCRcount != wo->UCRcount)
			error ("UcrBg UCRcount doesn't match");

		if (ro->BGcount != wo->BGcount)
			error ("UcrBg BGcount doesn't match");

		for (i = 0; i < wo->UCRcount; i++)
			rv |= dcomp(ro->UCRcurve[i], wo->UCRcurve[i]);

		for (i = 0; i < wo->BGcount; i++)
			rv |= dcomp(ro->BGcurve[i], wo->BGcurve[i]);

		if (ro->count != wo->count)
			error ("Text size doesn't match");

		rv |= strcmp(ro->string, wo->string);

		if (rv)
			error ("UcrBg verify failed");
	}
	}
	/* ------------------- */
	/* UcrBg - percentage: */
	{
	static icmUcrBg *wo;
	char *ts1 = "UcrBg - percentage info";
	if (mode == 0) {
		if ((wo = (icmUcrBg *)wr_icco->add_tag(
		           wr_icco, icmstr2tag("bfd%"), icSigUcrBgType)) == NULL) 
			return 1;

		wo->UCRcount = 1;			/* 1 == UCR percentage */
		wo->BGcount  = 1;			/* 1 == BG percentage */
		wo->allocate(wo);/* Allocate space */
		wo->UCRcurve[0] = (double) rand_int(0,65535); 
		wo->BGcurve[0] = (double) rand_int(0,65535); 
		wo->count = strlen(ts1)+1; 	/* Allocated and used size of text, inc null */
		wo->allocate(wo);/* Allocate space */
		strcpy(wo->string, ts1);	/* Copy the text in */
	} else {
		icmUcrBg *ro;

		/* Try and read the tag from the file */
		ro = (icmUcrBg *)rd_icco->read_tag(rd_icco, icmstr2tag("bfd%"));
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigUcrBgType)
			return 1;
	
		/* Now check it out */	
		if (ro->UCRcount != wo->UCRcount)
			error ("UcrBg UCRcount doesn't match");

		if (ro->BGcount != wo->BGcount)
			error ("UcrBg BGcount doesn't match");

		rv |= (ro->UCRcurve[0] != wo->UCRcurve[0]);
		rv |= (ro->BGcurve[0] != wo->BGcurve[0]);

		if (ro->count != wo->count)
			error ("Text size doesn't match");

		rv |= strcmp(ro->string, wo->string);

		if (rv)
			error ("UcrBg verify failed");
	}
	}
	/* ------------ */
	/* UInt16Array: */
	{
	static icmUInt16Array *wo;
	if (mode == 0) {
		unsigned int i;
		/* There is no standard Tag that uses icSigUInt16ArrayType, so use a 'custom' tag */
		if ((wo = (icmUInt16Array *)wr_icco->add_tag(
		           wr_icco, icmstr2tag("ui16"), icSigUInt16ArrayType)) == NULL) 
			return 1;

		wo->count = rand_int(0,17);		/* Number in array */
		wo->allocate(wo);	/* Allocate space */
		for (i = 0; i < wo->count; i++) {
			wo->data[i] = rand_o16(); /* Set numbers value */
		}
	} else {
		icmUInt16Array *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmUInt16Array *)rd_icco->read_tag(rd_icco, icmstr2tag("ui16"));
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigUInt16ArrayType)
			return 1;
	
		/* Now check it out */	
		if (ro->count != wo->count)
			error ("UInt16Array size doesn't match");

		for (i = 0; i < wo->count; i++) {
			rv |= (ro->data[i] != wo->data[i]);
		}

		if (rv)
			error ("UInt16Array verify failed");
	}
	}
	/* ------------ */
	/* UInt32Array: */
	{
	static icmUInt32Array *wo;
	if (mode == 0) {
		unsigned int i;
		/* There is no standard Tag that uses icSigUInt32ArrayType, so use a 'custom' tag */
		if ((wo = (icmUInt32Array *)wr_icco->add_tag(
		           wr_icco, icmstr2tag("ui32"), icSigUInt32ArrayType)) == NULL) 
			return 1;

		wo->count = rand_int(0,18);		/* Number in array */
		wo->allocate(wo);	/* Allocate space */
		for (i = 0; i < wo->count; i++) {
			wo->data[i] = rand_o32(); /* Set numbers value */
		}
	} else {
		icmUInt32Array *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmUInt32Array *)rd_icco->read_tag(rd_icco, icmstr2tag("ui32"));
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigUInt32ArrayType)
			return 1;
	
		/* Now check it out */	
		if (ro->count != wo->count)
			error ("UInt32Array size doesn't match");

		for (i = 0; i < wo->count; i++) {
			rv |= (ro->data[i] != wo->data[i]);
		}

		if (rv)
			error ("UInt32Array verify failed");
	}
	}
	/* ------------ */
	/* UInt64Array: */
	{
	static icmUInt64Array *wo;
	if (mode == 0) {
		unsigned int i;
		/* There is no standard Tag that uses icSigUInt64ArrayType, so use a 'custom' tag */
		if ((wo = (icmUInt64Array *)wr_icco->add_tag(
		           wr_icco, icmstr2tag("ui64"), icSigUInt64ArrayType)) == NULL) 
			return 1;

		wo->count = rand_int(0,19);		/* Number in array */
		wo->allocate(wo);	/* Allocate space */
		for (i = 0; i < wo->count; i++) {
			wo->data[i].l = rand_o32(); /* Set numbers value - low 32 bits */
			wo->data[i].h = rand_o32(); /* Set numbers value - low 32 bits */
		}
	} else {
		icmUInt64Array *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmUInt64Array *)rd_icco->read_tag(rd_icco, icmstr2tag("ui64"));
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigUInt64ArrayType)
			return 1;
	
		/* Now check it out */	
		if (ro->count != wo->count)
			error ("UInt64Array size doesn't match");

		for (i = 0; i < wo->count; i++) {
			rv |= (ro->data[i].l != wo->data[i].l);
			rv |= (ro->data[i].h != wo->data[i].h);
		}

		if (rv)
			error ("UInt64Array verify failed");
	}
	}
	/* ----------- */
	/* UInt8Array: */
	{
	static icmUInt8Array *wo;
	if (mode == 0) {
		unsigned int i;
		/* There is no standard Tag that uses icSigUInt8ArrayType, so use a 'custom' tag */
		if ((wo = (icmUInt8Array *)wr_icco->add_tag(
		           wr_icco, icmstr2tag("ui08"), icSigUInt8ArrayType)) == NULL) 
			return 1;

		wo->count = rand_int(0,18);		/* Number in array */
		wo->allocate(wo);	/* Allocate space */
		for (i = 0; i < wo->count; i++) {
			wo->data[i] = rand_o8(); /* Set numbers value */
		}
	} else {
		icmUInt8Array *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmUInt8Array *)rd_icco->read_tag(rd_icco, icmstr2tag("ui08"));
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigUInt8ArrayType)
			return 1;
	
		/* Now check it out */	
		if (ro->count != wo->count)
			error ("UInt8Array size doesn't match");

		for (i = 0; i < wo->count; i++) {
			rv |= (ro->data[i] != wo->data[i]);
		}

		if (rv)
			error ("UInt8Array verify failed");
	}
	}
	/* --------------- */
	/* VideoCardGamma: (ColorSync specific) */
	{
		static icmVideoCardGamma *wo;
		if (mode == 0) {
			int i;
			if ((wo = (icmVideoCardGamma *)wr_icco->add_tag(
				 wr_icco, icSigVideoCardGammaTag, icSigVideoCardGammaType)) == NULL)
				return 1;

			wo->tagType = icmVideoCardGammaTableType;
			wo->u.table.channels = rand_int(1,3);
			wo->u.table.entryCount = rand_int(2,1024);
			wo->u.table.entrySize = rand_int(1,2);
			wo->allocate(wo);
			if (wo->u.table.entrySize == 1) {
				unsigned char *cp = wo->u.table.data;
				for (i=0; i<wo->u.table.channels*wo->u.table.entryCount;i++,cp++)
					*cp = (unsigned char)rand_int(0,255);
			} else {
				unsigned short *sp = wo->u.table.data;
				for (i=0; i<wo->u.table.channels*wo->u.table.entryCount;i++,sp++)
					*sp = (unsigned short)rand_int(0,65535);
			}
		} else {
			icmVideoCardGamma *ro;
			int i;

			/* Try and read tag from the file */
			ro = (icmVideoCardGamma *)rd_icco->read_tag(rd_icco, icSigVideoCardGammaTag);
			if (ro == NULL)
				return 1;

			/* Need to check that the cast is appropriate */
			if (ro->ttype != icSigVideoCardGammaType)
				return 1;

			/* Now check it out */
			rv |= (ro->tagType != wo->tagType);
			rv |= (ro->u.table.channels != wo->u.table.channels);
			rv |= (ro->u.table.entryCount != wo->u.table.entryCount);
			rv |= (ro->u.table.entrySize != wo->u.table.entrySize);
			for (i=0; i<ro->u.table.channels*ro->u.table.entryCount*ro->u.table.entrySize; i++) {
				rv |= (((char*)ro->u.table.data)[i] != ((char*)wo->u.table.data)[i]);
				if (rv) break;
			}
			if (rv)
				error ("VideoCardGamma verify failed");
		}
	}
	/* ------------------ */
	/* ViewingConditions: */
	{
	static icmViewingConditions *wo;
	if (mode == 0) {
		if ((wo = (icmViewingConditions *)wr_icco->add_tag(
		           wr_icco, icSigViewingConditionsTag, icSigViewingConditionsType)) == NULL) 
			return 1;

    	wo->illuminant.X = rand_XYZ16();	/* XYZ of illuminant in cd/m^2 */
    	wo->illuminant.Y = rand_XYZ16();
    	wo->illuminant.Z = rand_XYZ16();
    	wo->surround.X = rand_XYZ16();		/* XYZ of surround in cd/m^2 */
    	wo->surround.Y = rand_XYZ16();
    	wo->surround.Z = rand_XYZ16();
    	wo->stdIlluminant = rand_Illuminant();	/* Standard illuminent type */
	} else {
		icmViewingConditions *ro;

		/* Try and read the tag from the file */
		ro = (icmViewingConditions *)rd_icco->read_tag(rd_icco, icSigViewingConditionsTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigViewingConditionsType)
			return 1;
	
		/* Now check it out */	
    	rv |= dcomp(ro->illuminant.X, wo->illuminant.X);
    	rv |= dcomp(ro->illuminant.Y, wo->illuminant.Y);
    	rv |= dcomp(ro->illuminant.Z, wo->illuminant.Z);
    	rv |= dcomp(ro->surround.X  , wo->surround.X);
    	rv |= dcomp(ro->surround.Y  , wo->surround.Y);
    	rv |= dcomp(ro->surround.Z  , wo->surround.Z);
    	rv |= (ro->stdIlluminant != wo->stdIlluminant);

		if (rv)
			error ("ViewingConditions verify failed");
	}
	}
	/* ---------- */
	/* XYZ array: */
	{
	static icmXYZArray *wo;
	if (mode == 0) {
		unsigned int i;
		/* Note that tag types icSigXYZType and icSigXYZArrayType are identical */
		if ((wo = (icmXYZArray *)wr_icco->add_tag(
		           wr_icco, icSigMediaWhitePointTag, icSigXYZArrayType)) == NULL) 
			return 1;

		wo->count = rand_int(1,7);	/* Should be one XYZ number, but test more */
		wo->allocate(wo);	/* Allocate space */
		for (i = 0; i < wo->count; i++) {
			wo->data[i].X = rand_XYZ16();	/* Set numbers value */
			wo->data[i].Y = rand_XYZ16();
			wo->data[i].Z = rand_XYZ16();
		}
	} else {
		icmXYZArray *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmXYZArray *)rd_icco->read_tag(rd_icco, icSigMediaWhitePointTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigXYZArrayType)
			return 1;
	
		/* Now check it out */	
		if (ro->count != wo->count)
			error ("XYZArray size doesn't match");

		for (i = 0; i < wo->count; i++) {
			rv |= dcomp(ro->data[i].X, wo->data[i].X);
			rv |= dcomp(ro->data[i].Y, wo->data[i].Y);
	    	rv |= dcomp(ro->data[i].Z, wo->data[i].Z);
		}

		if (rv)
			error ("XYZArray verify failed");
	}
	}

	return 0;
}

/* ------------------------------------------------ */
/* Floating point random number generators */
/* These are appropriate for the underlying integer */
/* representations in the icc format. */
/* This is simply as a convenience so that we can */
/* test the full range of representation, and */
/* get away with exact verification. */

/* 32 bit pseudo random sequencer */
static unsigned int seed = 0x12345678;

#define PSRAND(S) ((S) * 1103515245 + 12345)

void set_seed(ORD32 o32) {
	if (o32 == 0)
		o32 = 1;
	seed = o32;
}

unsigned int rand_o8() {
	ORD32 o32;
	seed = PSRAND(seed);
	o32 = seed & 0xff;
	return o32;
}

unsigned int rand_o16() {
	ORD32 o32;
	seed = PSRAND(seed);
	o32 = seed & 0xffff;
	return o32;
}

unsigned int rand_o32() {
	ORD32 o32;
	o32 = seed = PSRAND(seed);
	return o32;
}

int rand_int(int low, int high) {
	int i;
	seed = PSRAND(seed);
	i = seed % (high - low + 1);
	return i + low;
}

double rand_u8f8() {
	ORD32 o32;
	seed = PSRAND(seed);
	o32 = seed & 0xffff;
	return (double)o32/256.0;
}

double rand_u16f16() {
	ORD32 o32;
	seed = PSRAND(seed);
	o32 = seed;
	return (double)o32/65536.0;
}

double rand_s15f16() {
	INR32 i32;
	seed = PSRAND(seed);
	i32 = seed;
	return (double)i32/65536.0;
}

double rand_XYZ16() {
	ORD32 o32;
	seed = PSRAND(seed);
	o32 = seed & 0xffff;
	return (double)o32/32768.0;
}

double rand_L8() {
	ORD32 o32;
	seed = PSRAND(seed);
	o32 = seed & 0xff;
	return (double)o32/2.550;
}

double rand_ab8() {
	ORD32 o32;
	seed = PSRAND(seed);
	o32 = seed & 0xff;
	return (double)o32-128.0;
}

double rand_L16() {
	ORD32 o32;
	seed = PSRAND(seed);
	o32 = seed & 0xffff;
	return (double)o32/652.800;				/* 0xff00/100.0 */
}

double rand_ab16() {
	ORD32 o32;
	seed = PSRAND(seed);
	o32 = seed & 0xffff;
	return ((double)o32/256.0)-128.0;
}

double rand_8f() {
	unsigned int rv;
	seed = PSRAND(seed);
	rv = seed & 0xff;
	return (double)rv/255.0;
}

double rand_16f() {
	unsigned int rv;
	seed = PSRAND(seed);
	rv = seed & 0xffff;
	return (double)rv/65535.0;
}

/* Random selectors for ICC flags and enumerayions */

unsigned int rand_ScreenEncodings() {
	unsigned int flags = 0;
	
	if (rand_int(0,1) == 0)
		flags |= icPrtrDefaultScreensTrue;

	if (rand_int(0,1) == 0)
		flags |= icLinesPerInch;

	return flags;
}

/* Device attributes */
unsigned int rand_DeviceAttributes() {
	unsigned int flags = 0;

	if (rand_int(0,1) == 0)
		flags |= icTransparency;

	if (rand_int(0,1) == 0)
		flags |= icMatte;

	return flags;
}

/* Profile header flags */
unsigned int rand_ProfileHeaderFlags() {
	unsigned int flags = 0;

	if (rand_int(0,1) == 0)
		flags |= icEmbeddedProfileTrue;

	if (rand_int(0,1) == 0)
		flags |= icUseWithEmbeddedDataOnly;

	return flags;
}


unsigned int rand_AsciiOrBinaryData() {
	unsigned int flags = 0;

	if (rand_int(0,1) == 0)
		flags |= icBinaryData;

	return flags;
}

icColorSpaceSignature rand_ColorSpaceSignature() {
	switch(rand_int(0,25)) {
    	case 0:
			return icSigXYZData;
    	case 1:
			return icSigLabData;
    	case 2:
			return icSigLuvData;
    	case 3:
			return icSigYCbCrData;
    	case 4:
			return icSigYxyData;
    	case 5:
			return icSigRgbData;
    	case 6:
			return icSigGrayData;
    	case 7:
			return icSigHsvData;
    	case 8:
			return icSigHlsData;
    	case 9:
			return icSigCmykData;
    	case 10:
			return icSigCmyData;
    	case 11:
			return icSigMch6Data;
    	case 12:
			return icSig2colorData;
    	case 13:
			return icSig3colorData;
    	case 14:
			return icSig4colorData;
    	case 15:
			return icSig5colorData;
    	case 16:
			return icSig6colorData;
    	case 17:
			return icSig7colorData;
    	case 18:
			return icSig8colorData;
    	case 19:
			return icSig9colorData;
    	case 20:
			return icSig10colorData;
    	case 21:
			return icSig11colorData;
    	case 22:
			return icSig12colorData;
    	case 23:
			return icSig13colorData;
    	case 24:
			return icSig14colorData;
    	case 25:
			return icSig15colorData;
	}
    return icMaxEnumData;
}

icColorSpaceSignature rand_PCS() {
	switch(rand_int(0,1)) {
    	case 0:
			return icSigXYZData;
    	case 1:
			return icSigLabData;
	}
    return icMaxEnumData;
}

icTechnologySignature rand_TechnologySignature() {
	switch(rand_int(0,21)) {
    	case 0:
			return icSigDigitalCamera;
    	case 1:
			return icSigFilmScanner;
    	case 2:
			return icSigReflectiveScanner;
    	case 3:
			return icSigInkJetPrinter;
    	case 4:
			return icSigThermalWaxPrinter;
    	case 5:
			return icSigElectrophotographicPrinter;
    	case 6:
			return icSigElectrostaticPrinter;
    	case 7:
			return icSigDyeSublimationPrinter;
    	case 8:
			return icSigPhotographicPaperPrinter;
    	case 9:
			return icSigFilmWriter;
    	case 10:
			return icSigVideoMonitor;
    	case 11:
			return icSigVideoCamera;
    	case 12:
			return icSigProjectionTelevision;
    	case 13:
			return icSigCRTDisplay;
    	case 14:
			return icSigPMDisplay;
    	case 15:
			return icSigAMDisplay;
    	case 16:
			return icSigPhotoCD;
    	case 17:
			return icSigPhotoImageSetter;
    	case 18:
			return icSigGravure;
    	case 19:
			return icSigOffsetLithography;
    	case 20:
			return icSigSilkscreen;
    	case 21:
			return icSigFlexography;
	}
	return icMaxEnumTechnology;
}

icProfileClassSignature rand_ProfileClassSignature() {
	switch(rand_int(0,6)) {
		case 0:
			return icSigInputClass;
		case 1:
			return icSigDisplayClass;
		case 2:
			return icSigOutputClass;
		case 3:
			return icSigLinkClass;
		case 4:
			return icSigAbstractClass;
		case 5:
			return icSigColorSpaceClass;
		case 6:
			return icSigNamedColorClass;
	}
	return icMaxEnumClass;
}

icPlatformSignature rand_PlatformSignature() {
	switch(rand_int(0,4)) {
		case 0:
			return icSigMacintosh;
		case 1:
			return icSigMicrosoft;
		case 2:
			return icSigSolaris;
		case 3:
			return icSigSGI;
		case 4:
			return icSigTaligent;
	}
	return icMaxEnumPlatform;
}

icMeasurementFlare rand_MeasurementFlare() {
	switch(rand_int(0,1)) {
		case 0:
			return icFlare0;
		case 1:
			return icFlare100;
	}
	return icMaxEnumFlare;
}

icMeasurementGeometry rand_MeasurementGeometry() {
	switch(rand_int(0,2)) {
		case 0:
			return icGeometryUnknown;
		case 1:
			return icGeometry045or450;
		case 2:
			return icGeometry0dord0;
	}
	return icMaxEnumGeometry;
}

icRenderingIntent rand_RenderingIntent() {
	switch(rand_int(0,3)) {
		case 0:
			return icPerceptual;
	    case 1:
			return icRelativeColorimetric;
	    case 2:
			return icSaturation;
	    case 3:
			return icAbsoluteColorimetric;
	}
	return icMaxEnumIntent;
}

icSpotShape rand_SpotShape() {
	switch(rand_int(0,7)) {
		case 0:
			return icSpotShapeUnknown;
		case 1:
			return icSpotShapePrinterDefault;
		case 2:
			return icSpotShapeRound;
		case 3:
			return icSpotShapeDiamond;
		case 4:
			return icSpotShapeEllipse;
		case 5:
			return icSpotShapeLine;
		case 6:
			return icSpotShapeSquare;
		case 7:
			return icSpotShapeCross;
	}
	return icMaxEnumSpot;
}

icStandardObserver rand_StandardObserver() {
	switch(rand_int(0,2)) {
		case 0:
			return icStdObsUnknown;
		case 1:
			return icStdObs1931TwoDegrees;
		case 2:
			return icStdObs1964TenDegrees;
	}
	return icMaxEnumStdObs;
}

icIlluminant rand_Illuminant() {
	switch(rand_int(0,8)) {
		case 0:
			return icIlluminantUnknown;
		case 1:
			return icIlluminantD50;
		case 2:
			return icIlluminantD65;
		case 3:
			return icIlluminantD93;
		case 4:
			return icIlluminantF2;
		case 5:
			return icIlluminantD55;
		case 6:
			return icIlluminantA;
		case 7:
			return icIlluminantEquiPowerE;
		case 8:
			return icIlluminantF8;
	}
	return icMaxEnumIlluminant;
}

/* Malloc and initialize a random ascii string. */
/* Input is range of number of characters. */
/* Can free() buffer after use */
char *rand_ascii_string(int lenmin, int lenmax) {
	int len;	/* Length of output string */ 
	int olen;	/* Length of obuf allocation */
	char *obuf, *ebuf, *out;
	int i;

	len = rand_int(lenmin, lenmax);

	olen = len + 1;
	if ((obuf = malloc(olen)) == NULL)
		error("Malloc of rand_ascii_string len %d failed\n",olen);
	out = obuf;
	ebuf = obuf + olen;		/* One past end */

	for (i = 0; i < len; i++) {
		char ch;

		/* Create mostly printable ascii */
		if (rand_int(0, 6) != 4) {
			ch = rand_int(0x20, 0x7f);

		/* And less often control characters */
		} else {
			ch = rand_int(1, 0x1f);
		}

		*out++ = ch;
	}

	*out++ = '\000';

	return obuf;
}

/* Malloc and initialize a random utf-8 string */
/* Input is range of number of code points. */
/* Can free() buffer after use */
char *rand_utf8_string(int lenmin, int lenmax) {
	int len;	/* Length of output string */ 
	int olen;	/* Length of obuf allocation */
	ORD8 *obuf, *ebuf, *out;
	int i;

	len = rand_int(lenmin, lenmax);

	olen = len + len/2 + 1;
	if ((obuf = malloc(olen)) == NULL)
		error("Malloc of rand_utf8_string len %d failed\n",olen);
	out = obuf;
	ebuf = obuf + olen;		/* One past end */

	for (i = 0; i < len; i++) {
		const icmUTF8 firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
		const icmUTF32 byteMask = 0xBF;
		const icmUTF32 byteMark = 0x80; 
		unsigned short btw = 0;
		icmUTF32 ch;

		/* Create mostly ascii code points */
		if (rand_int(0, 6) != 4) {
			ch = rand_int(0x20, 0x7f);

		/* And less often contrl characters and non-ascii code points */
		} else {
			for (;;) {
				ch = rand_int(0, 0x10fff);		/* unicode range */

				if ( ch == 0							/* nul */
				 ||  ch == 0xfeff						/* BOM */
				 || (ch >= 0x20 && ch <= 0x7f)			/* ascii */
				 || (ch >= 0xd800 && ch <= 0xdfff))		/* surrogate */
					continue;				/* try again */
				break;
			}
		}

		/* We have a UTF32 value. Convert to UTF8 */

		/* Figure out how many bytes the result will require */
		if (ch < (icmUTF32)0x80) {			 	btw = 1;
		} else if (ch < (icmUTF32)0x800) {	 	btw = 2;
		} else if (ch < (icmUTF32)0x10000) {	btw = 3;
		} else if (ch < (icmUTF32)0x110000) {	btw = 4;
		} else {								btw = 3;
			error("rand_utf8_string got out of range UTF32 0x%x\n",ch);
		}

		/* Do we need to re-allocate output buffer ? */
		if ((out + btw + 1) > ebuf) {
			size_t off = out - obuf;
			olen += len/2;
			if ((obuf = realloc(obuf, olen)) == NULL)
				error("Realloc of rand_utf8_string len %d failed\n",olen);
			ebuf = obuf + olen;		/* One past end */
			out = obuf + off;
		}
	
		out += btw;
		switch (btw) { /* note: everything falls through. */
			case 4:
				*--out = (icmUTF8)((ch | byteMark) & byteMask); ch >>= 6;
			case 3:
				*--out = (icmUTF8)((ch | byteMark) & byteMask); ch >>= 6;
			case 2:
				*--out = (icmUTF8)((ch | byteMark) & byteMask); ch >>= 6;
			case 1:
				*--out =  (icmUTF8)(ch | firstByteMark[btw]);
		}
		out += btw;
	}

	*out++ = '\000';

	return obuf;
}

/* Malloc and initialize a random utf-16 string */
/* Input is range of number of code points. */
/* Can free() buffer after use */
icmUTF16 *rand_utf16_string(int lenmin, int lenmax) {
	int len;	/* Length of output string */ 
	int olen;	/* Length of obuf allocation */
	icmUTF16 *obuf, *ebuf, *out;
	int i;

	len = rand_int(lenmin, lenmax);

	olen = len + len/2 + 1;
	if ((obuf = (icmUTF16 *)malloc(olen * 2)) == NULL)
		error("Malloc of rand_utf8_string len %d failed\n",olen);
	out = obuf;
	ebuf = obuf + olen;		/* One past end */

	for (i = 0; i < len; i++) {
		icmUTF32 ch;

		/* Create mostly ascii code points */
		if (rand_int(0, 6) != 4) {
			ch = rand_int(0x20, 0x7f);

		/* And less often control characters and non-ascii code points */
		} else {
			for (;;) {
				ch = rand_int(0, 0x10fff);		/* unicode range */

				if ( ch == 0							/* nul */
				 ||  ch == 0xfeff						/* BOM */
				 || (ch >= 0x20 && ch <= 0x7f)			/* ascii */
				 || (ch >= 0xd800 && ch <= 0xdfff))		/* surrogate */
					continue;				/* try again */
				break;
			}
		}

		/* We have a UTF32 value. Convert to UTF16 */

		/* Do we need to re-allocate output buffer ? */
		if ((out + 2 + 1) > ebuf) {
			size_t off = out - obuf;
			olen += len/2;
			if ((obuf = (icmUTF16 *)realloc(obuf, olen * 2)) == NULL)
				error("Realloc of rand_utf16_string len %d failed\n",olen);
			ebuf = obuf + olen;		/* One past end */
			out = obuf + off;
		}
	
		/* Is a character <= 0xFFFF */
		if (ch <= 0x0000FFFF) {

			*out++ = (icmUTF16)ch;

		/* Output a surrogate pair */
		} else {
			ch -= 0x0010000;
			out[0] = (icmUTF16)(((ch >> 10) & 0x3FF) + 0xD800);
			out[1] = (icmUTF16)(( ch        & 0x3FF) + 0xDC00);
			out += 2;
		}
	}
	*out++ = 0;

	return obuf;
}

/* Return length of UTF-16 string */
size_t strlen_utf16(icmUTF16 *in) {
	size_t len;

	for (len = 0; *in != 0; in++, len++)
		;

	return len;
}

/* Compare two UTF-16 strings */
int strcmp_utf16(icmUTF16 *in1, icmUTF16 *in2) {

	for (;; in1++, in2++) {
		if (*in1 != *in2)
			return 1;
		if (*in1 == 0 || *in2 == 0)
			break;
	}
	return 0;
}

/* Compare doubles with a margine to allow */
/* for floating point handling funnies */
int dcomp(double a, double b) {
	double dif = fabs(a - b);
	double mag = fabs(a) + fabs(b);

	return dif > (mag * 1e-10) ? 1 : 0;
}

/* ------------------------------------------------ */
/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"icctest: Error - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit (-1);
}

void
warning(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"icctest: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}
