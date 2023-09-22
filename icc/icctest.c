
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

    (Note XYZ scaling to 1.0, not 100.0)
 
 */

#define NTRIALS 100

#define TEST_UNICODE 0		/* Test unicode translation code */
#define TEST_MATXINV 0		/* Test matrix inversion code (invmatrix() must be public) */

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
unsigned int rand_int(unsigned int low, unsigned int high);
unsigned int rand_o8(void), rand_o16(void), rand_o32(void);
double rand_8f(void), rand_XYZ8(void), rand_L8(void), rand_ab8(void);
double rand_16f(void), rand_32f(void);
double rand_XYZ16(void), rand_u8f8(void), rand_L16(void), rand_ab16(void);
double rand_u16f16(void), rand_s15f16(void);
double rand_rangef(double min, double max, int bits);
void rand_Color(double *vals, icColorSpaceSignature sig, int bits);
int dcomp(double a, double b);

/* random ICC specific values */
void rand_XYZ(icmXYZNumber *pXYZ);
icScreening rand_ScreenEncodings(void);
unsigned int rand_DeviceAttributes(void);
unsigned int rand_ProfileHeaderFlags(void);
unsigned int rand_AsciiOrBinaryData(void);
icColorSpaceSignature rand_ColorSpaceSignature(void);
icColorSpaceSignature rand_PCS(void);
icTechnologySignature rand_TechnologySignature(void);
icProfileClassSignature rand_ProfileClassSignature(void);
icPlatformSignature rand_PlatformSignature(void);
icMeasurementGeometry rand_MeasurementGeometry(void);
icRenderingIntent rand_RenderingIntent(void);
icSpotShape rand_SpotShape(void);
icStandardObserver rand_StandardObserver(void);
icIlluminant rand_Illuminant(void);
icPhColEncoding rand_ColorantEncoding(void);
icMeasUnitsSig rand_MeasUnitsSig();
icDevSetMsftIDSignature rand_DevSetMsftIDSignature(void);
icDevSetMsftMedia rand_DevSetMsftMedia(void);
icDevSetMsftDither rand_DevSetMsftDither(void);

char *rand_ascii_string(int lenmin, int lenmax);
icmUTF8 *rand_utf8_string(int lenmin, int lenmax);
icmUTF16 *rand_utf16_string(int lenmin, int lenmax);
size_t strlen_utf16(icmUTF16 *in);
int strcmp_utf16(icmUTF16 *in1, icmUTF16 *in2);
void rand_id(unsigned char id[16]);
int idcomp(unsigned char a[16], unsigned char b[16]);

/* declare some test functions */
#if TEST_UNICODE > 0
int unicode_test(void);
#endif
#if TEST_MATXINV > 0
int matxinv_test(void);
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

int doit(int pass, int mode, icc *wr_icco, icc *rd_icco, int v3);

void usage(void) {
	printf("ICC library regression test, V%s\n",ICCLIB_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: icctest [-v level] [-1] [-n pass] [-w] [-r]\n");
	fprintf(stderr," -v level      Verbosity level\n");
	fprintf(stderr," -N            No startup description\n");
	fprintf(stderr," -1            Do just first pass\n");
	fprintf(stderr," -n x          Do just pass x\n");
	fprintf(stderr," -w            Do write side of pass\n");
	fprintf(stderr," -r            Do read side of pass\n");
	fprintf(stderr," -2            Only do icclib v2 checks\n");
	fprintf(stderr," -z            Always put data at zero offset\n");
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
	int nointro = 0;							/* No introduction vebosity */
	int verb = 0;
	int passn = 0;
	int wonly = 0;
	int ronly = 0;
	int warn = 0;
	int v3 = 1;									/* Check icclib V3 additions */
	int zoffset = 0;							/* Always use zero file offset */
	icmErr e = { 0, { '\000'} };				/* Place to get error details back */
	char *file_name = "xxxx.icm";
	icmFile *wr_fp = NULL, *rd_fp = NULL;
	icc *wr_icco = NULL, *rd_icco = NULL;		/* Keep object separate */
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

			/* icclib v2 only */
			else if (argv[fa][1] == '2') {
				v3 = 0;
			}

			/* Zero file offset */
			else if (argv[fa][1] == 'z') {
				zoffset = 1;
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
#if TEST_MATXINV > 0
	if (matxinv_test())
		error ("Matrix inversion routine is faulty");
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
		if (zoffset)
			offset = 0;

		/* -------------------------- */
		/* Deal with writing the file */
	
		icm_err_clear_e(&e);

		if (!ronly) {
			/* Open up the file for writing */
			if ((wr_fp = new_icmFileStd_name(&e, file_name,"w")) == NULL)
				error("Write: Open file '%s' failed with 0x%x, '%s'",file_name,e.c, e.m);
		} else {
			/* Open up the file for reading rather than writing, so we can seek. */
			if ((wr_fp = new_icmFileStd_name(&e, file_name,"r")) == NULL)
				error("Write: Open file '%s' failed with 0x%x, '%s'",file_name,e.c, e.m);
		}
		
		if ((wr_icco = new_icc(&e)) == NULL)
			error("Write: Creation of ICC object failed with 0x%x, '%s'",e.c,e.m);
	
		if (warn)
			wr_icco->warning = Warning;

		/* Default to V2.4 */
#define TEST_DEFAULT_TV ICMTV_24
		wr_icco->set_version(wr_icco, TEST_DEFAULT_TV);

		/* Don't check for required tags, so that we can test range of profile */
		/* classes with full range of tags. */
		wr_icco->set_cflag(wr_icco, icmCFlagNoRequiredCheck);

		/* Relax format so we can use the full range of tagtypes */
		wr_icco->set_cflag(wr_icco, icmCFlagWrFormatWarn);
		wr_icco->set_vcrange(wr_icco, &icmtvrange_all); 

		/* Add all the tags with their tag types */
		if ((rv = doit(i, 0, wr_icco, NULL, v3)) != 0)
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
				error ("Read: icc read returned %d, 0x%x '%s'",rv,rd_icco->e.c,rd_icco->e.m);
	
			/* Read and verify all the tags and their tag types */
			if ((rv = doit(i, 1, wr_icco, rd_icco, v3)) != 0)
				error ("Read: do write/read/verify return %d, 0x%x '%s'",rv,rd_icco->e.c,rd_icco->e.m);
	
			/* -------- */
			/* Clean up */
			wr_icco->del(wr_icco);
	
			rd_icco->del(rd_icco);
			rd_fp->del(rd_fp);

			if (verb)
				printf(" OK\n");
		} else {
			wr_icco->del(wr_icco);
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

#if TEST_MATXINV > 0
int invmatrix(
	double out[MAX_CHAN][MAX_CHAN],
	double in[MAX_CHAN][MAX_CHAN],
	int n
);

static void dump_mtx(FILE *fp, char *id, char *pfx, double a[MAX_CHAN][MAX_CHAN], int nr,  int nc) {
	int i, j;
	fprintf(fp, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		fprintf(fp, "%s ",pfx);
		for (i = 0; i < nc; i++)
			fprintf(fp, "%f%s",a[j][i], i < (nc-1) ? ", " : "");
		fprintf(fp, "\n");
	}
}

int matxinv_test() {
	double mx[MAX_CHAN][MAX_CHAN];
	double imx[MAX_CHAN][MAX_CHAN];
	double ch[MAX_CHAN][MAX_CHAN];
	int m, n, i, j, k;
	double max;

	for (m = 0; m < 20; m++) {
		/* Create a random matrix */
		n = rand_int(2,8);
		for (i = 0;  i < n; i++) {
			for (j = 0;  j < n; j++) {
				mx[i][j] = rand_32f();
			}
		}
	
//		dump_mtx(stdout, "mx","",mx,n,n);
	
		if (invmatrix(imx, mx, n)) {
			printf("invmatrix failed\n");
			return 1;
		}
	
//		dump_mtx(stdout, "imx","",imx,n,n);
	
		for (i = 0;  i < n; i++) {
			for (j = 0;  j < n; j++) {
				ch[i][j] = 0.0;
				for (k = 0; k < n; k++) {
					ch[i][j] += mx[i][k] * imx[k][j]; 
				}
			}
		}
	
//		dump_mtx(stdout, "ch","",ch,n,n);
	
		max = 0.0;
		for (i = 0;  i < n; i++) {
			for (j = 0;  j < n; j++) {
				double vv = ch[i][j];
				if (i == j)
					vv -= 1.0;
				vv = fabs(vv);
				if (vv > max)
					max = vv;
			}
		}
	
		printf("max matrix invert error %e\n",max);
	
		if (max > 1e-11) {
			dump_mtx(stdout, "ch","",ch,n,n);
			return 1;
		}
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
	int pass,		/* Pass number */
	int mode,		/* 0 - write, 1 = read/verify */
	icc *wr_icco,	/* The write icc object */
	icc *rd_icco,	/* The read icc object */
	int v3			/* Do iccib v3 checks */
) {
	int rv = 0;
	int minc = 0;	/* Minimum arrary/number count */

	if (v3)
		minc = 1;	/* For compatibility with SampleIcc */

	/* ----------- */
	/* The header: */
	if (mode == 0) {
		icmHeader *wh = wr_icco->header;

		/* Values that must be set before writing */
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

		/* Avoid device encodings that aren't handled by icctest v2  */
		if (!v3 && (wh->colorSpace == icSigXYZData || wh->colorSpace == icSigLabData
		 || wh->colorSpace == icSigLuvData || wh->colorSpace == icSigYCbCrData
		 || wh->colorSpace == icSigYxyData))
			wh->colorSpace = icSigRgbData;        /* ICC doesn't say how to encode these... */

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
	icTagSignature sig = ((icTagSignature)icmMakeTag('5','6','7','8'));
	static icmUnknown *wo;

	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmUnknown *)wr_icco->add_tag(
		           wr_icco, sig,	icmSigUnknownType)) == NULL) { 
			return 1;
		}

		wo->uttype = icmMakeTag('1','2','3','4');
		wo->count = rand_int(minc,1034);	/* Space we need for data */
		if (wo->allocate(wo)) /* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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
			error ("Unknown uttype doesn't match (%s != %s",icmtag2str(ro->uttype),icmtag2str(wo->uttype));

		if (ro->count != wo->count)
			error ("Unknown count doesn't match (found %d should be %d)",ro->count,wo->count);

		for (i = 0; i < wo->count; i++)
			rv |= (ro->data[i] != wo->data[i]);

		if (rv)
			error ("Unknown verify failed");
	}
	}

	/* ------------- */
	/* Chromaticity: */

	if (v3) {		/* icclib v2 release doesn't have this */
	static icmChromaticity *wo;
	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmChromaticity *)wr_icco->add_tag(
		           wr_icco, icSigChromaticityTag, icSigChromaticityType)) == NULL) { 
			printf("Add icSigChromaticityType failed\n");
			return 1;
		}

		wo->enc = rand_ColorantEncoding();

		if (wo->enc != icPhColUnknown) {
			wo->setup(wo);		/* Create tag compatible with encoding */

		} else {				/* Invent chromaticities */
	   		wo->count = rand_int(minc, 15);	/* Should actually be same as device colorspace */
			if (wo->allocate(wo))				/* Allocate Chromaticity structures */
				error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

			for (i = 0; i < wo->count; i++) {
				wo->data[i].xy[0] = rand_u16f16();
				wo->data[i].xy[1] = rand_u16f16();
			}

		}
	} else {
		icmChromaticity *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmChromaticity *)rd_icco->read_tag(rd_icco, icSigChromaticityTag);
		if (ro == NULL) {
			printf("read_tag icSigChromaticityType failed\n");
			return 1;
		}

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigChromaticityType) {
			printf("cast icSigChromaticityType failed\n");
			return 1;
		}

		/* Now check it out */	
    	rv |= (ro->enc != wo->enc);
		if (rv) error("Chromaticity verify failed on encoding");

    	rv |= (ro->count != wo->count);
		if (rv) error("Chromaticity verify failed on count");

		for (i = 0; i < ro->count; i++) {
			rv |= diff_u16f16(ro->data[i].xy[0], wo->data[i].xy[0]);
			rv |= diff_u16f16(ro->data[i].xy[1], wo->data[i].xy[1]);
			if (rv) error("Chromaticity verify failed on values");
		}
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

		wo->ppcount = strlen(str1)+1; 			/* Allocated and used size of text, inc null */
		for (i = 0; i < 4; i++)
			wo->crdcount[i] = strlen(str2[i])+1;	/* Allocated and used size of text, inc null */

		if (wo->allocate(wo))			/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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
		if (ro->ppcount != wo->ppcount)
		for (i = 0; i < 4; i++) {
			if (ro->crdcount[i] != wo->crdcount[i])
				error ("CrdInfo crdcount[%d] doesn't match",i);
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

		wo->ctype = icmCurveLin; 	/* Linear version */
		if (wo->allocate(wo))/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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
		if (ro->ctype != wo->ctype)
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

		wo->ctype = icmCurveGamma; 		/* Gamma version */
		if (wo->allocate(wo))	/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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
		if (ro->ctype != wo->ctype)
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

		wo->ctype = icmCurveSpec; 	/* Specified version */
		wo->count = rand_int(2,23);	/* Number of entries (min must be 2!) */
		if (wo->allocate(wo))/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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
		if (ro->ctype != wo->ctype)
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

		wo->flag = icAsciiData;	/* Holding ASCII data */
		wo->count = strlen(ts1)+1; 	/* Allocated and used size of text, inc null */
		if (wo->allocate(wo))/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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

		wo->flag = icBinaryData;		/* Holding binary data */
		wo->count = rand_int(minc,43);	/* Space we need for data */
		if (wo->allocate(wo))/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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
	static icmLut1 *wo;
	if (mode == 0) {
		unsigned int i, j, k;

		if ((wo = (icmLut1 *)wr_icco->add_tag(
		           wr_icco, icSigAToB0Tag,	icSigLut16Type)) == NULL) 
			return 1;

		wo->inputChan = 2;			/* (Hard coded into cLut access) */
		wo->outputChan = 3;
    	wo->clutPoints = 5;
    	wo->inputEnt = 56;
    	wo->outputEnt = 73;
		if (wo->allocate(wo))/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

		/* The matrix is only applicable to XYZ input space */
		for (i = 0; i < 3; i++)							/* Matrix */
			for (j = 0; j < 3; j++)
				wo->pe_mx->mx[i][j] = rand_s15f16();

		/* See icc.getNormFuncs() for normalizing functions */
		/* The input table index range is over the normalized range 0.0 - 1.0. */
		/* The range in input color space can be determined by denormalizing */
		/* the values 0.0 - 1.0. */
		for (i = 0; i < wo->inputChan; i++)				/* Input tables */
			for (j = 0; j < wo->inputEnt; j++)
				wo->pe_ic[i]->data[j] = rand_16f();

														/* Lut */
		/* The multidimensional lut has a normalized index range */
		/* of 0.0 - 1.0 in each dimension. Its entry values are also */
		/* normalized values in the range 0.0 - 1.0. */
		for (i = 0; i < wo->clutPoints; i++)			/* Input chan 0 - slow changing */
			for (j = 0; j < wo->clutPoints; j++)		/* Input chan 1 - faster changing */
				for (k = 0; k < wo->outputChan; k++) {	/* Output chans */
					int idx = (i * wo->clutPoints + j) * wo->outputChan + k;
					wo->pe_cl->clutTable[idx] = rand_16f();
				}

		/* The output color space values should be normalized to the */
		/* range 0.0 - 1.0 for use as output table entry values. */
		for (i = 0; i < wo->outputChan; i++)			/* Output tables */
			for (j = 0; j < wo->outputEnt; j++)
				wo->pe_oc[i]->data[j] = rand_16f();

	} else {
		icmLut1 *ro;
		unsigned int i, j, k;

		/* Try and read the tag from the file */
		ro = (icmLut1 *)rd_icco->read_tag(rd_icco, icSigAToB0Tag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigLut16Type)
			error ("Lut16 ttype mismatch");
	
		/* Now check it out */	
		rv |= (ro->inputChan != wo->inputChan);
		rv |= (ro->outputChan != wo->outputChan);
    	rv |= (ro->clutPoints != wo->clutPoints);
    	rv |= (ro->inputEnt != wo->inputEnt);
    	rv |= (ro->outputEnt != wo->outputEnt);
		if (rv) error ("Lut16 dimensions mismatch");

		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				rv |= dcomp(ro->pe_mx->mx[i][j], wo->pe_mx->mx[i][j]);
		if (rv) error ("Lut16 matrix mismatch");

		for (i = 0; i < wo->inputChan; i++)
			for (j = 0; j < wo->inputEnt; j++) {
				rv |= dcomp(ro->pe_ic[i]->data[j], wo->pe_ic[i]->data[j]);
				if (rv) error ("Lut16 input table mismatch chan %d entry %d, is %f should be %f",
				i,j,ro->pe_ic[i]->data[j], wo->pe_ic[i]->data[j]);
			}

		for (i = 0; i < wo->clutPoints; i++)			/* Input chan 0 - slow changing */
			for (j = 0; j < wo->clutPoints; j++)		/* Input chan 1 - faster changing */
				for (k = 0; k < wo->outputChan; k++) {	/* Output chans */
					int idx = (i * wo->clutPoints + j) * wo->outputChan + k;
					rv |= dcomp( ro->pe_cl->clutTable[idx], wo->pe_cl->clutTable[idx]);
				}
		if (rv) error ("Lut16 cLut mismatch");

		for (i = 0; i < wo->outputChan; i++)
			for (j = 0; j < wo->outputEnt; j++)
				rv |= dcomp(ro->pe_oc[i]->data[j], wo->pe_oc[i]->data[j]);
		if (rv) error ("Lut16 output table mismatch");
	}
	}
	/* ------------------ */
	/* 16 bit lut - link: */
	{
	static icmLut1 *wo;
	if (mode == 0) {
		/* Just link to the existing LUT. This is often used when there */
		/* is no distinction between intents, and saves file and memory space. */
		if ((wo = (icmLut1 *)wr_icco->link_tag(
		           wr_icco, icSigAToB1Tag,	icSigAToB0Tag)) == NULL) 
			return 1;
	} else {
		icmLut1 *ro;
		unsigned int i, j, k;

		/* Try and read the tag from the file */
		ro = (icmLut1 *)rd_icco->read_tag(rd_icco, icSigAToB1Tag);
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
		if (rv) error ("Lut16 link dimensions mismatch");

		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				rv |= dcomp(ro->pe_mx->mx[i][j], wo->pe_mx->mx[i][j]);
		if (rv) error ("Lut16 link matrix mismatch");

		for (i = 0; i < wo->inputChan; i++)
			for (j = 0; j < wo->inputEnt; j++)
				rv |= dcomp(ro->pe_ic[i]->data[j], wo->pe_ic[i]->data[j]);
		if (rv) error ("Lut16 link input table mismatch");

		for (i = 0; i < wo->clutPoints; i++)			/* Input chan 0 - slow changing */
			for (j = 0; j < wo->clutPoints; j++)		/* Input chan 1 - faster changing */
				for (k = 0; k < wo->outputChan; k++) {	/* Output chans */
					int idx = (i * wo->clutPoints + j) * wo->outputChan + k;
					rv |= dcomp( ro->pe_cl->clutTable[idx], wo->pe_cl->clutTable[idx]);
				}
		if (rv) error ("Lut16 link cLut mismatch");

		for (i = 0; i < wo->outputChan; i++)
			for (j = 0; j < wo->outputEnt; j++)
				rv |= dcomp(ro->pe_oc[i]->data[j], wo->pe_oc[i]->data[j]);
		if (rv) error ("Lut16 link output table mismatch");
	}
	}
	/* ---------- */
	/* 8 bit lut: */
	{
	static icmLut1 *wo;
	if (mode == 0) {
		unsigned int i, j, m, k;
		if ((wo = (icmLut1 *)wr_icco->add_tag(
		           wr_icco, icSigAToB2Tag,	icSigLut8Type)) == NULL) 
			return 1;

		wo->inputChan = 3;			/* (Hard coded into cLut access) */
		wo->outputChan = 2;
    	wo->clutPoints = 4;
    	wo->inputEnt = 256;			/* Must be 256 for Lut8 */
    	wo->outputEnt = 256;
		if (wo->allocate(wo))			/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

		for (i = 0; i < 3; i++)							/* Matrix */
			for (j = 0; j < 3; j++)
				wo->pe_mx->mx[i][j] = rand_s15f16();

		for (i = 0; i < wo->inputChan; i++)				/* Input tables */
			for (j = 0; j < wo->inputEnt; j++)
				wo->pe_ic[i]->data[j] = rand_8f();

														/* Lut */
		for (i = 0; i < wo->clutPoints; i++)				/* Input chan 0 */
			for (j = 0; j < wo->clutPoints; j++)			/* Input chan 1 */
				for (m = 0; m < wo->clutPoints; m++)		/* Input chan 2 */
					for (k = 0; k < wo->outputChan; k++) {	/* Output chans */
						int idx = ((i * wo->clutPoints + j)
						              * wo->clutPoints + m)
						              * wo->outputChan + k;
						wo->pe_cl->clutTable[idx] = rand_8f();
						}

		for (i = 0; i < wo->outputChan; i++)			/* Output tables */
			for (j = 0; j < wo->outputEnt; j++)
				wo->pe_oc[i]->data[j] = rand_8f();

	} else {
		icmLut1 *ro;
		unsigned int size;
		unsigned int i, j, k, m;

		/* Try and read the tag from the file */
		ro = (icmLut1 *)rd_icco->read_tag(rd_icco, icSigAToB2Tag);
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
				rv |= dcomp(ro->pe_mx->mx[i][j], wo->pe_mx->mx[i][j]);
		if (rv) error ("Lut8 matrix mismatch");

		for (i = 0; i < wo->inputChan; i++)
			for (j = 0; j < wo->inputEnt; j++)
				rv |= dcomp(ro->pe_ic[i]->data[j], wo->pe_ic[i]->data[j]);
		if (rv) error ("Lut8 input table mismatch");

		for (i = 0; i < wo->clutPoints; i++)				/* Input chan 0 */
			for (j = 0; j < wo->clutPoints; j++)			/* Input chan 1 */
				for (m = 0; m < wo->clutPoints; m++)		/* Input chan 2 */
					for (k = 0; k < wo->outputChan; k++) {	/* Output chans */
						int idx = ((i * wo->clutPoints + j)
						              * wo->clutPoints + m)
						              * wo->outputChan + k;
						rv |= dcomp( ro->pe_cl->clutTable[idx], wo->pe_cl->clutTable[idx]);
						}

		if (rv) error ("Lut8 cLut mismatch");

		for (i = 0; i < wo->outputChan; i++)
			for (j = 0; j < wo->outputEnt; j++)
				rv |= dcomp(ro->pe_oc[i]->data[j], wo->pe_oc[i]->data[j]);
		if (rv) error ("Lut8 output table mismatch");
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
		char *prefix = "Prefix", *suffix = "Suffix";
		char buf[100];
		unsigned int i;

		if ((wo = (icmNamedColor *)wr_icco->add_tag(
		           wr_icco, icSigNamedColorTag, icSigNamedColorType)) == NULL) 
			return 1;

   		wo->vendorFlag = rand_int(0,65535) << 16;	/* Bottom 16 bits for IC use */
   		wo->count = 3;			/* Count of named colors */
		wo->pcount = strlen(prefix) + 1;
		wo->scount = strlen(suffix) + 1;

		if (wo->allocate(wo))	/* Allocate strings and named color structures */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

		strcpy(wo->prefix,prefix); /* Prefix for each color name, max 32, null terminated */
		strcpy(wo->suffix,suffix); /* Suffix for each color name, max 32, null terminated */

		for (i = 0; i < wo->count; i++) {
			unsigned int j;
			icColorSpaceSignature dsig = wr_icco->header->colorSpace;
			sprintf(buf,"Color %d",i); /* Root name, max 32, null terminated */
			wo->data[i].rcount = strlen(buf) + 1;
			if (wo->allocate(wo))	/* Allocate root string */
				error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
			strcpy(wo->data[i].root,buf);
			rand_Color(wo->data[i].deviceCoords, wr_icco->header->colorSpace, 8);
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
			error ("NamedColor info verify failed");

		for (i = 0; i < wo->count; i++) {
			unsigned int j;
			rv |= strcmp(ro->data[i].root, wo->data[i].root);
			if (rv) error ("NamedColor entry %d root verify failed",i);
			for (j = 0; j < wo->nDeviceCoords; j++) {
				rv |= dcomp(ro->data[i].deviceCoords[j], wo->data[i].deviceCoords[j]);
				if (rv) error ("NamedColor entry %d value %d verify failed: is %f should be %f",
				                i,j, ro->data[i].deviceCoords[j], wo->data[i].deviceCoords[j]);
			}
		}

	}
	}
	/* ----------------- */
	/* NamedColor2: */
	{
	static icmNamedColor *wo;	/* Shares same machine specific structure */
	if (mode == 0) {
		char *prefix = "Prefix-ix", *suffix = "Suffix-ixix";
		char buf[100];
		unsigned int i;

		if ((wo = (icmNamedColor *)wr_icco->add_tag(
		           wr_icco, icSigNamedColor2Tag, icSigNamedColor2Type)) == NULL) 
			return 1;

   		wo->vendorFlag = rand_int(0,65535) << 16;	/* Bottom 16 bits for ICC use */
   		wo->count = 4;			/* Count of named colors */
		wo->nDeviceCoords =	icmCSSig2nchan(wr_icco->header->colorSpace);
								/* Num of device coordinates */
		wo->pcount = strlen(prefix) + 1;
		wo->scount = strlen(suffix) + 1;

		if (wo->allocate(wo))	/* Allocate strings and named color structures */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

		strcpy(wo->prefix,"Prefix-ix"); /* Prefix for each color name, max 32, null terminated */
		strcpy(wo->suffix,"Suffix-ixix"); /* Suffix for each color name, max 32, null terminated */

		for (i = 0; i < wo->count; i++) {
			sprintf(buf,"Pigment %d",i); /* Root name, max 32, null terminated */
			wo->data[i].rcount = strlen(buf) + 1;
			if (wo->allocate(wo))	/* Allocate root string */
				error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
			strcpy(wo->data[i].root,buf);
			rand_Color(wo->data[i].pcsCoords, wr_icco->header->pcs, 16);
			rand_Color(wo->data[i].deviceCoords, wr_icco->header->colorSpace, 16);
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
			for (j = 0; j < wo->nDeviceCoords; j++) {
				rv |= dcomp(ro->data[i].deviceCoords[j], wo->data[i].deviceCoords[j]);
				if (rv) error ("NamedColor2 verify entry %d device value %d failed is %f should be %f",i,j,ro->data[i].deviceCoords[j], wo->data[i].deviceCoords[j]);
			}
			for (j = 0; j < 3; j++) {
				rv |= dcomp(ro->data[i].pcsCoords[j], wo->data[i].pcsCoords[j]);
				if (rv) error ("NamedColor2 verify entry %d pcs value %d failed, is %f should be %f",i,j,ro->data[i].pcsCoords[j], wo->data[i].pcsCoords[j]);
			}
		}

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

		wo->count = 3; 			/* Number of descriptions in sequence */
		if (wo->allocate(wo))	/* Allocate space for all the DescStructures */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

		/* Fill in each description structure in sequence */
		for (i = 0; i < wo->count; i++) {
			icmTextDescription *wp;		/* We're assuming ICCV2 here */
			char *ts1, *ts3;
			icmUTF8 *ts2;

			wo->data[i].deviceMfg   = icmstr2tag("mfg7");
			wo->data[i].deviceModel = icmstr2tag("2345");
			wo->data[i].attributes.l = icTransparency | icMatte;
			wo->data[i].attributes.h = 0x98765432;
			wo->data[i].technology = rand_TechnologySignature();

			/* device Text description */
			ts1 = rand_ascii_string(9, 81);
			ts2 = rand_utf8_string(8, 111);
			ts3 = rand_ascii_string(7, 67-1);

			wp = (icmTextDescription *)wo->data[i].mfgDesc;
			wp->count = strlen(ts1)+1;
			wp->allocate(wp); 		/* Allocate space */
			strcpy(wp->desc, ts1);		/* Copy the string in */
	
			/* utf-8 */
			wp->ucLangCode = 8765;		/* UniCode language code */
			wp->uc8count = strlen((char *)ts2)+1;/* Size in chars inc null */
			wp->allocate(wp);			/* Allocate space */
			strcpy((char *)wp->uc8desc, (char *)ts2);	/* Copy the string in */
	
			/* Script code */
			wp->scCode = 37;				/* Fudge scriptCode code */
			wp->scCount = strlen(ts3)+1;	/* Used size of scDesc in bytes, inc null */
			wp->allocate(wp);			/* Allocate space */
			strcpy((char *)wp->scDesc, ts3);	/* Copy the string in */

			free(ts1);
			free(ts2);
			free(ts3);

			/* model Text description */
			ts1 = rand_ascii_string(9, 81);
			ts2 = rand_utf8_string(8, 111);
			ts3 = rand_ascii_string(7, 67-1);

			wp = (icmTextDescription *)wo->data[i].modelDesc;
			wp->count = strlen(ts1)+1;
			wp->allocate(wp);			/* Allocate space */
			strcpy(wp->desc, ts1);		/* Copy the string in */
	
			/* We'll fudge up the Unicode string */
			wp->ucLangCode = 7856;		/* UniCode language code */
			wp->uc8count = strlen((char *)ts2)+1;	/* Size in chars inc null */
			wp->allocate(wp);			/* Allocate space */
			strcpy((char *)wp->uc8desc, (char *)ts2);	/* Copy string in */
	
			wp->scCode = 67;				/* Fudge scriptCode code */
			wp->scCount = strlen(ts3)+1;	/* Used size of scDesc in bytes, inc null */
			wp->allocate(wp);			/* Allocate space */
			strcpy((char *)wp->scDesc, ts3);	/* Copy the string in */

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
			icmTextDescription *wp, *rp;		/* We're assuming ICCV2 here */

			rv |= (ro->data[i].deviceMfg    != wo->data[i].deviceMfg);
			rv |= (ro->data[i].deviceModel  != wo->data[i].deviceModel);
			rv |= (ro->data[i].attributes.l != wo->data[i].attributes.l);
			rv |= (ro->data[i].attributes.h != wo->data[i].attributes.h);
			rv |= (ro->data[i].technology   != wo->data[i].technology);

			wp = (icmTextDescription *)wo->data[i].mfgDesc;
			rp = (icmTextDescription *)ro->data[i].mfgDesc;

			rv |= (rp->count  != wp->count);
			rv |= strcmp(rp->desc, wp->desc);
	
			rv |= (rp->ucLangCode != wp->ucLangCode);
			rv |= (rp->uc8count != wp->uc8count);
			rv |= strcmp((char *)rp->uc8desc, (char *)wp->uc8desc);
	
			rv |= (rp->scCode != wp->scCode);
			rv |= (rp->scCount != wp->scCount);
			rv |= strcmp((char *)rp->scDesc, (char *)wp->scDesc);

			wp = (icmTextDescription *)wo->data[i].modelDesc;
			rp = (icmTextDescription *)ro->data[i].modelDesc;

			rv |= (rp->count  != wp->count);
			rv |= strcmp(rp->desc, wp->desc);
	
			rv |= (rp->ucLangCode != wp->ucLangCode);
			rv |= (rp->uc8count != wp->uc8count);
			rv |= strcmp((char *)rp->uc8desc, (char *)wp->uc8desc);
	
			rv |= (rp->scCode != wp->scCode);
			rv |= (rp->scCount != wp->scCount);
			rv |= strcmp((char *)rp->scDesc, (char *)wp->scDesc);
		}

		if (rv)
			error ("ProfileSequenceDesc verify failed");
	}
	}
	/* ----------------- */
	/* ResponseCurveSet16: */

	if (v3) {
	static icmResponseCurveSet16 *wo;
	unsigned int m, n, q;
	if (mode == 0) {
		unsigned int i;
		if ((wo = (icmResponseCurveSet16 *)wr_icco->add_tag(
		           wr_icco, icSigOutputResponseTag, icSigResponseCurveSet16Type)) == NULL) 
			return 1;

   		wo->nchan = rand_int(1, 15);	/* Should actually be same as device colorspace */
   		wo->typeCount = rand_int(1, 4);	/* Number of measurement types. */
		if (wo->allocate(wo))				/* Allocate measurement types array. */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

		for (m = 0; m < wo->typeCount; m++) {
			icmRCS16Struct *wp = &wo->typeData[m];

			wp->measUnit = rand_MeasUnitsSig();

			for (n = 0; n < wo->nchan; n++)
	   			wp->nMeas[n] = rand_int(11, 23);	/* Number of measurements for each channel */
			if (wo->allocate(wo))						/* Allocate respose arrays */
				error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

			/* Fill in response device & measurement values for each channel for these units */
			for (n = 0; n < wo->nchan; n++) {

				rand_XYZ(&wp->pcsData[n]);		/* max. colorant value */
	
				for (q = 0; q < wp->nMeas[n]; q++) {
					wp->response[n][q].deviceValue = rand_16f(); 
					wp->response[n][q].measurement = rand_s15f16();
				}
			}
		}
	} else {
		icmResponseCurveSet16 *ro;
		unsigned int i;

		/* Try and read the tag from the file */
		ro = (icmResponseCurveSet16 *)rd_icco->read_tag(rd_icco, icSigOutputResponseTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigResponseCurveSet16Type)
			return 1;
	
		/* Now check it out */	
		if (ro->nchan != wo->nchan)
			error ("ResponseCurveSet16 nchan doesn't match");

		if (ro->typeCount != wo->typeCount)
			error ("ResponseCurveSet16 typeCount doesn't match");

		for (m = 0; m < wo->typeCount; m++) {
			icmRCS16Struct *wp = &wo->typeData[m];
			icmRCS16Struct *rp = &ro->typeData[m];

			if (rp->measUnit != wp->measUnit) 
				error ("ResponseCurveSet16 %u measUnit doesn't match",m);

			for (n = 0; n < wo->nchan; n++) {
	   			if (wp->nMeas[n] != rp->nMeas[n])
					error ("ResponseCurveSet16 %u,%u nMeas doesn't match",m,n);

		    	rv |= dcomp(rp->pcsData[n].X, wp->pcsData[n].X);
		   	 	rv |= dcomp(rp->pcsData[n].Y, wp->pcsData[n].Y);
    			rv |= dcomp(rp->pcsData[n].Z, wp->pcsData[n].Z);
				if (rv) error ("ResponseCurveSet16 %u,%u pcsData failed",m,n);
	
				for (q = 0; q < wp->nMeas[n]; q++) {
					rv |= wp->response[n][q].deviceValue != wp->response[n][q].deviceValue; 
					rv |= wp->response[n][q].measurement != wp->response[n][q].measurement;
					if (rv) error ("ResponseCurveSet16 %u,%u,%u response failed",m,n,q);
				}
			}
		}
		if (rv)
			error ("ResponseCurveSet16 verify failed");
	}
	}

	/* -------------- */
	/* DeviceSettings */

	if (v3) {
	static icmDeviceSettings *wo;
	if (mode == 0) {
		unsigned int h, i, j, k, m;
		if ((wo = (icmDeviceSettings *)wr_icco->add_tag(
		           wr_icco, icSigDeviceSettingsTag, icSigDeviceSettingsType)) == NULL)  {
			printf("Add icSigDeviceSettingsType failed\n");
			return 1;
		}

		wo->count = rand_int(0,7);		/* Number of platforms */
		if (wo->allocate(wo))				/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
		for (h = 0; h < wo->count; h++) {
			icmPlatformEntry *pep = &wo->data[h];

			if (rand_int(0,2) != 0)
				pep->platform = icSigMicrosoft;			/* Make Msft more common */
			else
				pep->platform = rand_PlatformSignature();
			pep->count = rand_int(0,7);		/* Number of setting combinations */
			if (wo->allocate(wo))				/* Allocate space */
				error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

			for (i = 0; i < pep->count; i++) {
				icmSettingComb *scp = &pep->data[i];

				scp->count = rand_int(0,10);	/* Number of settings */
				if (wo->allocate(wo))				/* Allocate space */
					error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

				for (j = 0; j < scp->count; j++) {
					icmSettingStruct *ssp = &scp->data[j];

					if (pep->platform == icSigMicrosoft) {
						ssp->msft.sig = rand_DevSetMsftIDSignature();
						ssp->count = rand_int(0, 4);
						ssp->ssize = rand_int(0, 4);	/* in case sig was unknown */
						if (wo->allocate(wo))				/* Allocate space */
							error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);

						if (icSigMsftResolution == ssp->msft.sig) {
							for (k = 0; k < ssp->count; k++) {
								ssp->msft.resolution[k].xres = rand_int(0, 0xffffffff);
								ssp->msft.resolution[k].yres = rand_int(0, 0xffffffff);
							}
						} else if (icSigMsftMedia == ssp->msft.sig) {
							for (k = 0; k < ssp->count; k++)
								ssp->msft.media[k] = rand_DevSetMsftMedia();
						} else if (icSigMsftHalftone == ssp->msft.sig) {
							for (k = 0; k < ssp->count; k++)
								ssp->msft.halftone[k] = rand_DevSetMsftDither();
						} else {
							for (k = 0; k < ssp->count; k++)
								for (m = 0; m < ssp->ssize; m++)
									ssp->msft.unknown[k * ssp->size + m] = rand_int(0,255);
						}
					} else {
						ssp->unknown.sig = rand_int(0, 0xffffffff);
						ssp->count = rand_int(0, 4);
						ssp->ssize = rand_int(0, 4);	/* Because this is unknown */
						if (wo->allocate(wo))				/* Allocate space */
							error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
						for (k = 0; k < ssp->count; k++)
							for (m = 0; m < ssp->ssize; m++)
								ssp->unknown.unknown[k * ssp->size + m] = rand_int(0,255);
					}
				}
			}
		}
	} else {
		icmDeviceSettings *ro;
		unsigned int h, i, j, k, m;

		/* Try and read the tag from the file */
		ro = (icmDeviceSettings *)rd_icco->read_tag(rd_icco, icSigDeviceSettingsTag);
		if (ro == NULL)  {
			printf("read_tag icSigDeviceSettingsType failed\n");
			return 1;
		}

		/* Need to check that the cast is appropriate. */
		if (ro->ttype != icSigDeviceSettingsType) {
			printf("cast icSigDeviceSettingsType failed\n");
			return 1;
		}
	
		if (ro->count != wo->count)
			error("DeviceSettings number of platforms doesn't match");

		/* Now check it out */	
		for (h = 0; h < wo->count; h++) {
			icmPlatformEntry *wpep = &wo->data[h];
			icmPlatformEntry *rpep = &ro->data[h];

			if (rpep->size != wpep->size)
				error("DeviceSettings platform entry size in bytes doesn't match");
			if (rpep->platform != wpep->platform)
				error("DeviceSettings platform doesn't match");
			if (rpep->count != wpep->count)
				error("DeviceSettings number of setting combinations doesn't match");

			for (i = 0; i < wpep->count; i++) {
				icmSettingComb *wscp = &wpep->data[i];
				icmSettingComb *rscp = &rpep->data[i];

				if (rscp->size != wscp->size)
					error("DeviceSettings combination size in bytes doesn't match");
				if (rscp->count != wscp->count)
					error("DeviceSettings number of settings doesn't match");

				for (j = 0; j < wscp->count; j++) {
					icmSettingStruct *wssp = &wscp->data[j];
					icmSettingStruct *rssp = &rscp->data[j];

					if (wpep->platform == icSigMicrosoft) {
						if (rssp->msft.sig != wssp->msft.sig)
							error("DeviceSettings Msft setting id doesn't match");
						if (rssp->count != wssp->count)
							error("DeviceSettings Msft setting values count doesn't match");
						if (rssp->size != wssp->size)
							error("DeviceSettings Msft setting values size doesn't match");

						if (icSigMsftResolution == wssp->msft.sig) {
							for (k = 0; k < wssp->count; k++) {
								if (rssp->msft.resolution[k].xres != wssp->msft.resolution[k].xres)
									error("DeviceSettings Msft X resolution doesn't match");
								if (rssp->msft.resolution[k].yres != wssp->msft.resolution[k].yres)
									error("DeviceSettings Msft Y resolution doesn't match");
							}
						} else if (icSigMsftMedia == wssp->msft.sig) {
							for (k = 0; k < wssp->count; k++)
								if (rssp->msft.media[k] != wssp->msft.media[k])
									error("DeviceSettings Msft media doesn't match");
						} else if (icSigMsftHalftone == wssp->msft.sig) {
							for (k = 0; k < wssp->count; k++)
								if (rssp->msft.halftone[k] != wssp->msft.halftone[k])
									error("DeviceSettings Msft halftone doesn't match");
						} else {
							for (k = 0; k < wssp->count; k++)
								for (m = 0; m < wssp->ssize; m++)
									if (rssp->msft.unknown[k * rssp->size + m]
									 != wssp->msft.unknown[k * wssp->size + m])
									error("DeviceSettings Msft unknown value doesn't match");
						}
					} else {
						if (rssp->unknown.sig != wssp->unknown.sig)
							error("DeviceSettings Unknown setting id doesn't match");
						if (rssp->count != wssp->count)
							error("DeviceSettings Unknown setting values count doesn't match");
						if (rssp->size != wssp->size)
							error("DeviceSettings Unknown setting values size doesn't match");

						for (k = 0; k < wssp->count; k++)
							for (m = 0; m < wssp->ssize; m++)
								if (rssp->unknown.unknown[k * rssp->size + m]
								 != wssp->unknown.unknown[k * wssp->size + m])
								error("DeviceSettings Unknown unknown value doesn't match");
					}
				}
			}
		}
		if (rv)
			error("DeviceSettings verify failed");
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

		wo->count = rand_int(minc,17);		/* Number in array */
		if (wo->allocate(wo))	/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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

		wo->screeningFlags = rand_ScreenEncodings();
		wo->channels = rand_int(1,4);	/* Number of channels */
		if (wo->allocate(wo))	/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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

		rv |= (ro->screeningFlags != wo->screeningFlags);

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
		char *ts1, *ts3;
		icmUTF8 *ts2;

		if ((wo = (icmTextDescription *)wr_icco->add_tag(
		           wr_icco, icSigProfileDescriptionTag,	icSigTextDescriptionType)) == NULL) 
			return 1;

		/* Create the test messages */
		ts1 = rand_ascii_string(10, 90);
		ts2 = rand_utf8_string(3, 131);
		ts3 = rand_ascii_string(5, 67-1);

		/* Data in tag type wojects is always allocated and freed by the woject */
		wo->count = strlen(ts1)+1; 	/* Allocated and used size of desc, inc null */
		if (wo->allocate(wo))/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
		strcpy(wo->desc, ts1);		/* Copy the string in */

		/* We can treat utf-8 just like an ascii string */
		wo->ucLangCode = 1234;			/* UniCode language code */
		wo->uc8count = strlen((char *)ts2)+1; 	/* Allocated and used size of desc, inc null */
		if (wo->allocate(wo))/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
		strcpy((char *)wo->uc8desc, (char *)ts2);		/* Copy the string in */

		/* Don't really know anything about scriptCode, but fudge some values */
		wo->scCode = 23;			/* ScriptCode code */
		wo->scCount = strlen(ts3)+1;	/* Used size of scDesc in bytes, inc null */
		if (wo->allocate(wo))			/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
		if (wo->scCount > 67)
			error("textDescription ScriptCode string longer than 67 (%d)",wo->scCount);
		strcpy((char *)wo->scDesc, ts3);	/* Copy the string in */

		free(ts1);
		free(ts2);
		free(ts3);

	} else {
		icmTextDescription *ro;

		/* Try and read the tag from the file */
		ro = (icmTextDescription *)rd_icco->read_tag(rd_icco, icSigProfileDescriptionTag);
		if (ro == NULL) 
			return 1;

		/* Need to check that the cast is appropriate. */
		/* We could have left it icmBase, switched on ro->ttype, & then cast appropriately. */
		if (ro->ttype != icSigTextDescriptionType)
			return 1;
	
		/* Now check it out */	
		rv |= (ro->count != wo->count);
		rv |= strcmp(ro->desc, wo->desc);
		rv |= (ro->ucLangCode != wo->ucLangCode);
		rv |= (ro->uc8count != wo->uc8count);
		rv |= strcmp((char *)ro->uc8desc, (char *)wo->uc8desc);
		rv |= (ro->scCode != wo->scCode);
		rv |= (ro->scCount != wo->scCount);
		rv |= strcmp((char *)ro->scDesc, (char *)wo->scDesc);

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
		if (wo->allocate(wo))			/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
		strcpy(wo->desc, ts1);		/* Copy the text in */
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

		rv |= strcmp(ro->desc, wo->desc);

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

		wo->count = rand_int(minc,17);		/* Number in array */
		if (wo->allocate(wo))	/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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
		if (wo->allocate(wo))		/* Allocate space for both curves */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
		for (i = 0; i < wo->UCRcount; i++)
			wo->UCRcurve[i] = rand_16f();	/* Set numbers value */
		for (i = 0; i < wo->BGcount; i++)
			wo->BGcurve[i] = rand_16f();	/* Set numbers value */
		wo->count = strlen(ts1)+1; 			/* Allocated and used size of text, inc null */
		if (wo->allocate(wo))		/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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
		if (wo->allocate(wo))/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
		wo->UCRcurve[0] = (double) rand_int(0,65535); 
		wo->BGcurve[0] = (double) rand_int(0,65535); 
		wo->count = strlen(ts1)+1; 	/* Allocated and used size of text, inc null */
		if (wo->allocate(wo))/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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

		if (v3)
			wo->count = rand_int(1,17);		/* Number in array */
		else
			wo->count = rand_int(minc,17);		/* Number in array */
		if (wo->allocate(wo))	/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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

		if (v3)
			wo->count = rand_int(1,18);		/* Number in array */
		else
			wo->count = rand_int(minc,18);		/* Number in array */
		if (wo->allocate(wo))	/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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

		wo->count = rand_int(minc,19);		/* Number in array */
		if (wo->allocate(wo))	/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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

		wo->count = rand_int(minc,18);		/* Number in array */
		if (wo->allocate(wo))	/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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
	/* VideoCardGamma - curves: (ColorSync specific) */
	{
		static icmVideoCardGamma *wo;
		if (mode == 0) {
			unsigned int c, i;
			if ((wo = (icmVideoCardGamma *)wr_icco->add_tag(
				 wr_icco, icSigVideoCardGammaTag, icSigVideoCardGammaType)) == NULL)
				return 1;

			wo->tagType = icVideoCardGammaTable;
			wo->u.table.channels = rand_int(1,3);
			wo->u.table.entryCount = rand_int(2,1024);
			wo->u.table.entrySize = rand_int(1,2);
			if (wo->allocate(wo))
				error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
			if (wo->u.table.entrySize == 1) {
				for (c = 0; c < wo->u.table.channels; c++)
					for (i = 0; i < wo->u.table.entryCount; i++)
						wo->u.table.data[c][i] = rand_8f();
			} else {
				for (c = 0; c < wo->u.table.channels; c++)
					for (i = 0; i < wo->u.table.entryCount; i++)
						wo->u.table.data[c][i] = rand_16f();
			}
		} else {
			icmVideoCardGamma *ro;
			unsigned int c, i;

			/* Try and read tag from the file */
			ro = (icmVideoCardGamma *)rd_icco->read_tag(rd_icco, icSigVideoCardGammaTag);
			if (ro == NULL)
				return 1;

			/* Need to check that the cast is appropriate */
			if (ro->ttype != icSigVideoCardGammaType)
				return 1;

			/* Now check it out */
			rv |= (ro->tagType != wo->tagType);
			if (rv) error ("VideoCardGamma table verify tagtype failed");
			rv |= (ro->u.table.channels != wo->u.table.channels);
			rv |= (ro->u.table.entryCount != wo->u.table.entryCount);
			rv |= (ro->u.table.entrySize != wo->u.table.entrySize);

			for (c = 0; c < wo->u.table.channels; c++) {
				for (i = 0; i < wo->u.table.entryCount; i++) {
					rv |= dcomp(ro->u.table.data[c][i], wo->u.table.data[c][i]);
					if (rv) break;
				}
			}
			if (rv)
				error ("VideoCardGamma table verify failed");
		}
	}
	/* --------------- */
	/* VideoCardGamma - formula: (ColorSync specific) */
	if (v3) {
		static icmVideoCardGamma *wo;
		if (mode == 0) {
			unsigned int i;
			if ((wo = (icmVideoCardGamma *)wr_icco->add_tag(
				 wr_icco, icmstr2tag("vcgf"), icSigVideoCardGammaType)) == NULL)
				return 1;

			wo->tagType = icVideoCardGammaFormula;
			for (i = 0; i < 3; i++) {
				wo->u.formula.gamma[i] = rand_s15f16();
				wo->u.formula.min[i]   = rand_s15f16();
				wo->u.formula.max[i]   = rand_s15f16();
			}
		} else {
			icmVideoCardGamma *ro;
			unsigned int c, i;

			/* Try and read tag from the file */
			ro = (icmVideoCardGamma *)rd_icco->read_tag(rd_icco, icmstr2tag("vcgf"));
			if (ro == NULL)
				return 1;

			/* Need to check that the cast is appropriate */
			if (ro->ttype != icSigVideoCardGammaType)
				return 1;

			/* Now check it out */
			rv |= (ro->tagType != wo->tagType);
			if (rv) error ("VideoCardGamma formula verify tagtype failed");

			for (i = 0; i < 3; i++) {
				rv |= dcomp(ro->u.formula.gamma[i], wo->u.formula.gamma[i]);
				rv |= dcomp(ro->u.formula.min[i], wo->u.formula.min[i]);
				rv |= dcomp(ro->u.formula.max[i], wo->u.formula.max[i]);
				if (rv) break;
			}
			if (rv)
				error ("VideoCardGamma formula verify failed");
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
		if (wo->allocate(wo))	/* Allocate space */
			error ("allocate: 0x%x '%s'",wr_icco->e.c,wr_icco->e.m);
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

unsigned int rand_int(unsigned int low, unsigned int high) {
	unsigned int m, i;
	seed = PSRAND(seed);
	m = high - low + 1;
	if (m != 0)
		i = seed % (high - low + 1);
	else
		i = seed;
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

/* This really isn't a thing - but will pop up with our random combo's */
double rand_XYZ8() {
	ORD32 o32;
	seed = PSRAND(seed);
	o32 = seed & 0xff;
	return (double)o32/128.0;
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

/* Generate float between 0.0 and 1.0 */
double rand_32f() {
	ORD32 sn = 0, ep = 0, ma;
	double op;
	seed = PSRAND(seed);

	//sn = (seed >> 31) & 0x1;		/* Sign */
	sn = 0;
	//ep = (seed >> 23) & 0xff;		/* Exponent */
	ep = 126;
	ma = seed & 0x7fffff;			/* Mantissa */
	if (ma == 0)
		ep = 0;

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

double rand_rangef(double min, double max, int bits) {
	if (bits == 8)
		return rand_8f() * (max - min) + min;
	else
		return rand_16f() * (max - min) + min;
}

/* bits = 8 or 16 */
void rand_Color(double *vals, icColorSpaceSignature sig, int bits) {
	int i, nchan = icmCSSig2nchan(sig);

	switch ((int)sig) {
		case icSigXYZData:
			if (bits == 8) {
	    		vals[0] = rand_XYZ8();
	   	 		vals[1] = rand_XYZ8();
   		 		vals[2] = rand_XYZ8();
			} else {
	    		vals[0] = rand_XYZ16();
	   	 		vals[1] = rand_XYZ16();
   		 		vals[2] = rand_XYZ16();
			}
			break;
		case icSigLabData:
			if (bits == 8) {
				vals[0] = rand_L8();
				vals[1] = rand_ab8();
				vals[2] = rand_ab8();
			} else {
				vals[0] = rand_L16();
				vals[1] = rand_ab16();
				vals[2] = rand_ab16();
			}
			break;
		/* Undefined device spaces */
		/* These aren't actually specified by ICC, so we punt. */
		case icSigLuvData:
			vals[0] = rand_rangef(0.0, 100.0, bits);
			vals[1] = rand_rangef(-128, 127.0 + 255.0/256.0, bits);
			vals[2] = rand_rangef(-128, 127.0 + 255.0/256.0, bits);
			break;
		case icSigYCbCrData:
			vals[0] = rand_rangef(0.0, 1.0, bits);
			vals[1] = rand_rangef(-0.5, 0.5, bits);
			vals[2] = rand_rangef(-0.5, 0.5, bits);
			break;
		case icSigYxyData:
			vals[0] = rand_rangef(0.0, 1.0, bits);
			vals[1] = rand_rangef(0.0, 1.0, bits);
			vals[2] = rand_rangef(0.0, 1.0, bits);
			break;
		/* By default assume a device 0..1 */
		default:
			if (bits == 8) {
				for (i = 0; i < nchan; i++)
					vals[i] = rand_8f();
			} else {
				for (i = 0; i < nchan; i++)
					vals[i] = rand_16f();
			}
			break;
	}
}

/* Random selectors for ICC flags and enumerayions */

icScreening rand_ScreenEncodings() {
	icScreening flags = 0;
	
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

void rand_XYZ(icmXYZNumber *pXYZ) {
   	pXYZ->X = rand_XYZ16();
   	pXYZ->Y = rand_XYZ16();
   	pXYZ->Z = rand_XYZ16();
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
    return icMaxEnumColorSpace;
}

icColorSpaceSignature rand_PCS() {
	switch(rand_int(0,1)) {
    	case 0:
			return icSigXYZData;
    	case 1:
			return icSigLabData;
	}
    return icMaxEnumColorSpace;
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

icPhColEncoding rand_ColorantEncoding() {
	switch(rand_int(0,8)) {
		case 0:
		case 7:
		case 8:
			return icPhColUnknown;
		case 1:
			return icPhColITU_R_BT_709;
		case 2:
			return icPhColSMPTE_RP145_1994;
		case 3:
			return icPhColEBU_Tech_3213_E;
		case 4:
			return icPhColP22;
		case 5:
			return icPhColP3;
		case 6:
			return icPhColITU_R_BT2020;
	}
	return icMaxEnumPhCol;
}

icMeasUnitsSig rand_MeasUnitsSig() {
	switch(rand_int(0,8)) {
		case 0:
    		return icSigStatusA;
		case 1:
    		return icSigStatusE;
		case 2:
    		return icSigStatusI;
		case 3:
    		return icSigStatusT;
		case 4:
    		return icSigStatusM;
		case 5:
    		return icSigDN;
		case 6:
    		return icSigDNP;
		case 7:
    		return icSigDNN;
		case 8:
    		return icSigDNN;
	}
	return icMaxEnumMeasUnits;
}

icDevSetMsftIDSignature rand_DevSetMsftIDSignature() {
	switch(rand_int(0,3)) {
    	case 0:
			return icSigMsftResolution;
    	case 1:
			return icSigMsftMedia;
    	case 2:
			return icSigMsftHalftone;
    	case 3:
			return icSigMsftHalftone;
	}
	return icMaxEnumDevSetMsftID;
}

icDevSetMsftMedia rand_DevSetMsftMedia() {
	switch(rand_int(0,3)) {
    	case 0:
			return icMsftMediaStandard;
    	case 1:
			return icMsftMediaTrans;
    	case 2:
			return icMsftMediaGloss;
    	case 3:
			return icMsftMediaUser1 + rand_int(0, 255);
	}
	return icMaxEnumMsftMedia;
}

icDevSetMsftDither rand_DevSetMsftDither() {
	switch(rand_int(0,10)) {
    	case 0:
			return icMsftDitherNone;
    	case 1:
			return icMsftDitherCoarse;
    	case 2:
			return icMsftDitherFine;
    	case 3:
			return icMsftDitherLineArt;
    	case 4:
			return icMsftDitherErrorDiffusion;
    	case 5:
			return icMsftDitherReserved6;
    	case 6:
			return icMsftDitherReserved7;
    	case 7:
			return icMsftDitherReserved8;
    	case 8:
			return icMsftDitherReserved9;
    	case 9:
			return icMsftDitherGrayScale;
    	case 10:
			return icMsftDitherUser1 + rand_int(0, 255);
	}
	return icMsftMaxEnumMsftDither;
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
icmUTF8 *rand_utf8_string(int lenmin, int lenmax) {
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
		if ((out + btw + 1) >= ebuf) {
			size_t off = out - obuf;
			olen += btw + 1;
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
		if ((out + 2 + 1) >= ebuf) {
			size_t off = out - obuf;
			olen += 2 + 1;
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


/* Generate a random id */
void rand_id(unsigned char id[16]) {
	int i;

	for (i = 0; i < 16; i++)
		id[i] = rand_o8();
}

/* Compare to id's */
int idcomp(unsigned char a[16], unsigned char b[16]) {
	int i;

	for (i = 0; i < 16; i++) {
		if (a[i] != b[i])
			return 1;
	}
	return 0;
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
