

/* 
 * Create an ICC V2.4 compatible matrix display profile.
 *
 * Author:  Graeme W. Gill
 * Date:    8/2/2008
 * Version: 1.00
 *
 * Copyright 2006 - 2014 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 *
 * Based on icc/lutest.c
 */


/*
 * TTBD:
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "icc.h"

void error(char *fmt, ...), warning(char *fmt, ...);

void usage(void) {
	fprintf(stderr,"Create a Matrix Display ICC profile\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: mkDispProf [-v level] outfile\n");
	fprintf(stderr," -v                 Verbose\n");
	exit(1);
}

/* sRGB like device gamma encoded value to linear value 0.0 .. 1.0 */
static double gdv2dv(double iv) {
	double ov;

	if (iv < 0.04045)
		ov = iv/12.92;
	else
		ov = pow((iv + 0.055)/1.055, 2.4);
	return ov;
}

int
main(
int argc,
char *argv[]
) {
	int fa,nfa;
	icmErr e = { 0, { '\000'} };
	char out_name[1000];
	icmFile *wr_fp;
	icc *wr_icco;
	int rv = 0;
	int verb = 0;

	if (argc < 2)
		usage();

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

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}

			else 
				usage();
		}
		else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(out_name,argv[fa]);

	/* ---------------------------------------- */
	/* Create a matrix/shaper based XYZ profile */
	/* ---------------------------------------- */

	icm_err_clear_e(&e);

	/* Open up the file for writing */
	if ((wr_fp = new_icmFileStd_name(&e, out_name,"w")) == NULL)
		error ("Write: Can't open file '%s', failed with 0x%x, '%s'",out_name,e.c, e.m);

	if ((wr_icco = new_icc(&e)) == NULL)
		error ("Write: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);

	/* Add all the tags required */

	/* The header: */
	{
		icmHeader *wh = wr_icco->header;

		/* Values that must be set before writing */
		wh->deviceClass     = icSigDisplayClass;
    	wh->colorSpace      = icSigRgbData;			/* It's RGB space */
    	wh->pcs             = icSigXYZData;
    	wh->renderingIntent = icPerceptual;

		/* Values that should be set before writing */
		wh->manufacturer = icmstr2tag("????");
    	wh->model        = icmstr2tag("????");
	}
	/* Profile Description Tag: */
	{
		icmCommonTextDescription *wo;
		char *dst;

		dst = "sRGB like Matrix Display profile";
		if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
		           wr_icco, icSigProfileDescriptionTag,	icmSigCommonTextDescriptionType)) == NULL) 
			error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);

		wo->count = strlen(dst)+1; 	/* Allocated and used size of desc, inc null */
		wo->allocate(wo);/* Allocate space */
		strcpy(wo->desc, dst);		/* Copy the string in */
	}
	/* Copyright Tag: */
	{
		icmCommonTextDescription *wo;
		char *crt = "Copyright tag goes here";
		if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
		           wr_icco, icSigCopyrightTag,	icmSigCommonTextDescriptionType)) == NULL) 
			error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);

		wo->count = strlen(crt)+1; 	/* Allocated and used size of text, inc null */
		wo->allocate(wo);/* Allocate space */
		strcpy(wo->desc, crt);		/* Copy the text in */
	}

	/* Could add other relevant tags here, such as:

	   Device Manufacturers Description Tag
	   Device Model Description Tag
	   Device technology Tag
	   Viewing conditions Description Tag
	   Viewing conditions
	   Display Luminance Tag
	   Measurement information Tag

	   etc.
	 */

	/* Setup the primaries */
	{
		/* Compute primaries as XYZ */
		double wrgb[4][3] = {		/* Primaries in Yxy from the standard */
			{ 1.0, 0.3127, 0.3290 },	/* White */
			{ 1.0, 0.6400, 0.3300 },	/* Red */
			{ 1.0, 0.3000, 0.6000 },	/* Green */
			{ 1.0, 0.1500, 0.0600 }		/* Blue */
		};
		double mat[3][3];
		int i;

		/* Convert yxy to normalised 3x3 matrix */
		icmRGBYxyprim2matrix(wrgb[1], wrgb[2], wrgb[3], wrgb[0], mat, wrgb[0]);
		icmTranspose3x3(mat, mat);

#ifdef NEVER		/* Dump XYZ */
		printf("sRGB: XYZ\n");
		printf("{ %f, %f, %f }, /* Red */\n"
		       "{ %f, %f, %f }, /* Green */\n"
		       "{ %f, %f, %f }, /* Blue */\n"
		       "{ %f, %f, %f }  /* White */\n",
		mat[0][0], mat[0][1], mat[0][2],
		mat[1][0], mat[1][1], mat[1][2],
		mat[2][0], mat[2][1], mat[2][2],
		wrgb[0][0], wrgb[0][1], wrgb[0][2]);
#endif

#ifdef NEVER		/* Dump xyz */
{
	double mat2[4][3];
	double tt;

	for (i = 0; i < 3; i++) {
	
		tt = mat[i][0] + mat[i][1] + mat[i][2];
		mat2[i][0] = mat[i][0] / tt;
		mat2[i][1] = mat[i][1] / tt;
		mat2[i][2] = mat[i][2] / tt;
	}
	tt = wrgb[0][0] + wrgb[0][1] + wrgb[0][2];
	mat2[i][0] = wrgb[0][0] / tt;
	mat2[i][1] = wrgb[0][1] / tt;
	mat2[i][2] = wrgb[0][2] / tt;

	printf("sRGB: xyz\n");
	printf("{ %f, %f, %f },		/* Red */\n"
	       "{ %f, %f, %f },		/* Green */\n"
	       "{ %f, %f, %f },		/* Blue */\n"
	       "{ %f, %f, %f }		/* White */\n",
	mat2[0][0], mat2[0][1], mat2[0][2],
	mat2[1][0], mat2[1][1], mat2[1][2],
	mat2[2][0], mat2[2][1], mat2[2][2],
	mat2[3][0], mat2[3][1], mat2[3][2]);
}
#endif

		/* White Point Tag: */
		{
			icmXYZArray *wo;

			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaWhitePointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);

			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = wrgb[0][0];
			wo->data[0].Y = wrgb[0][1];
			wo->data[0].Z = wrgb[0][2]; 
		}
		/* Black Point Tag: */
		{
			icmXYZArray *wo;

			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaBlackPointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);

			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = 0.00;
			wo->data[0].Y = 0.00;
			wo->data[0].Z = 0.00;
		}
		/* Red, Green and Blue Colorant Tags: */
		{
			icmXYZNumber white;
			icmXYZArray *wor, *wog, *wob;
			double fromAbs[3][3];
			double d50m[3][3];

			/* Convert to D50 adapated */
			icmAry2XYZ(white, wrgb[0]);
			wr_icco->chromAdaptMatrix(wr_icco, ICM_CAM_NONE, NULL, fromAbs, icmD50, white);
			icmMulBy3x3(d50m[0], fromAbs, mat[0]);
			icmMulBy3x3(d50m[1], fromAbs, mat[1]);
			icmMulBy3x3(d50m[2], fromAbs, mat[2]);

			/* Make sure rounding doesn't wreck white point */
			quantizeRGBprimsS15Fixed16(d50m);

			if ((wor = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigRedColorantTag, icSigXYZArrayType)) == NULL) 
					error("add_tag failed: %d, %s",rv,wr_icco->e.m);
			if ((wog = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigGreenColorantTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
			if ((wob = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigBlueColorantTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);

			wor->count = wog->count = wob->count = 1;
			wor->allocate(wor);	/* Allocate space */
			wog->allocate(wog);
			wob->allocate(wob);
			wor->data[0].X = d50m[0][0]; wor->data[0].Y = d50m[0][1]; wor->data[0].Z = d50m[0][2];
			wog->data[0].X = d50m[1][0]; wog->data[0].Y = d50m[1][1]; wog->data[0].Z = d50m[1][2];
			wob->data[0].X = d50m[2][0]; wob->data[0].Y = d50m[2][1]; wob->data[0].Z = d50m[2][2];
		}
	}
	/* Red, Green and Blue Gamma Curve Tags: */
	{
		icmCurve *wor, *wog, *wob;
		int i;

		if ((wor = (icmCurve *)wr_icco->add_tag(
		           wr_icco, icSigRedTRCTag, icSigCurveType)) == NULL) 
			error("add_tag failed: %d, %s",rv,wr_icco->e.m);
		wor->ctype = icmCurveSpec;
		wor->count = 1024;
		wor->allocate(wor);	/* Allocate space */
		for (i = 0; i < wor->count; i++)
			wor->data[i] = gdv2dv(i/(wor->count-1.0));

		/* Link other channels to the red */
		if ((wog = (icmCurve *)wr_icco->link_tag(
		           wr_icco, icSigGreenTRCTag, icSigRedTRCTag)) == NULL) 
			error("link_tag failed: %d, %s",rv,wr_icco->e.m);
		if ((wob = (icmCurve *)wr_icco->link_tag(
		           wr_icco, icSigBlueTRCTag, icSigRedTRCTag)) == NULL) 
			error("link_tag failed: %d, %s",rv,wr_icco->e.m);
	}

	/* Write the file out */
	if ((rv = wr_icco->write(wr_icco,wr_fp,0)) != 0)
		error ("Write file: %d, %s",rv,wr_icco->e.m);
	
	wr_icco->del(wr_icco);
	wr_fp->del(wr_fp);

	return 0;
}

/* ------------------------------------------------ */
/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...) {
	va_list args;

	fprintf(stderr,"mkDispProf: Error - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit (-1);
}

void
warning(char *fmt, ...) {
	va_list args;

	fprintf(stderr,"mkDispProf: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}
