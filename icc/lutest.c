
/* 
 * International Color Consortium Format Library (icclib)
 * Lookup test code, and profile creation examples.
 *
 * Author:  Graeme W. Gill
 * Date:    2000/6/18
 * Version: 2.15
 *
 * Copyright 1998 - 2012 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 */

/* TTBD:
 *
 * Should add test for VideoCardGammaTable to check it works.
 */

/*

	This file is intended to serve two purposes. One is to do some
	basic regression testing of the lookup function of the icc
	library, by creating profiles with known mapping characteristics,
	and then checking that lookup displays those characteristics.
	Given the huge possible number of combinations of color spaces,
	profile variations, number of input and output channels, intents etc.
	the tests done here are far from exaustive.

	The other purpose is as a very basic source code example of how
	to create various styles of ICC profiles from mapping information,
	since extracting this information from the source code can take some
	doing. The example code here is still not perfect, since
	it doesn't cover the finer points of how to handle various intents,
	gamut compression etc. Such things are really the domain of a
	CMS, and would be dependent on the exact nature of the
	profile representation that the ICC file is being created from.
	(See examples in the Argyll CMS for these sorts of details.)

    (Note XYZ scaling to 1.0, not 100.0, as per ICC)
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

/* Some debuging aids */
#undef STOPONERROR	/* [und] stop if errors are excessive */
#undef TEST_APXLS	/* [und] test the cLut ICM_CLUT_SET_APXLS option */
#undef TESTLIN1		/* [und] test linear in curves */
#undef TESTLIN2		/* [und] test linear clut (fails with Lab) */
#undef TESTLIN3		/* [und] test linear out curves */

#undef IGNORE_WRITE_FORMAT_ERRORS	/* [und] warn rather than error on write format check */

/* These two assist the accuracy of our BToA Lut tests, using our simplistic test functions. */
/* They probably shouldn't be used on any real profile. */
#define REVLUTSCALE1		/* [def] Define this for pre-clut gamut bounding box scaling */
#define REVLUTSCALE2		/* [def] Define this for post-clut gamut quantization scaling */

#ifdef TEST_APXLS
# pragma message("######### TEST_APXLS enabled ########")
#endif
#ifdef IGNORE_WRITE_FORMAT_ERRORS
# pragma message("######### IGNORE_WRITE_FORMAT_ERRORS enabled ########")
#endif

/*
 * We start with a mathematically defined transfer characteristic, that
 * we create profiles from, and then check that the lookup function
 * matches the mathematical characteristic we started with.
 *
 * Use a matrix style 3D to 3D XYZ transfer as the core, to be
 * compatible with a matrix and Lut style profile.
 */

/* - - - - - - - - - - - - - */
/* Some support functions */

/* Clip value to range 0.0 to 1.0 */
void clip(double inout[3]) {
	int i;
	for (i = 0; i < 3; i++) {
		if (inout[i] < 0.0)
			inout[i] = 0.0;
		else if (inout[i] > 1.0)
			inout[i] = 1.0;
	}
}

/* Protected power function */
double ppow(double num, double p) {
	if (num < 0.0)
		return -pow(-num, p);
	else
		return pow(num, p);
}

#define D50_X 0.9642
#define D50_Y 1.0000
#define D50_Z 0.8249

#define D50_BX ( 0.8951 * D50_X +  0.2664 * D50_Y + -0.1614 * D50_Z)
#define D50_BY (-0.7502 * D50_X +  1.7135 * D50_Y +  0.0367 * D50_Z)
#define D50_BZ ( 0.0389 * D50_X + -0.0685 * D50_Y +  1.0296 * D50_Z)

#define ABS_X 0.83
#define ABS_Y 0.95
#define ABS_Z 1.05

#define ABS_BX ( 0.8951 * ABS_X +  0.2664 * ABS_Y + -0.1614 * ABS_Z)
#define ABS_BY (-0.7502 * ABS_X +  1.7135 * ABS_Y +  0.0367 * ABS_Z)
#define ABS_BZ ( 0.0389 * ABS_X + -0.0685 * ABS_Y +  1.0296 * ABS_Z)

/* Convert from normalized to relative XYZ */
static void to_rel(double out[3], double in[3]) {
	/* Scale to relative white and black points */
	out[0] = D50_X * in[0];
    out[1] = D50_Y * in[1];
    out[2] = D50_Z * in[2];
}

/* Convert from relative to normalized XYZ */
static void from_rel(double out[3], double in[3]) {
	/* Remove white and black points scale */
	out[0] = in[0]/D50_X;
	out[1] = in[1]/D50_Y;
	out[2] = in[2]/D50_Z;
}

/* Convert from relative to absolute XYZ */
static void rel_to_abs(double out[3], double in[3]) {
	double tt[3];

	/* Multiply by bradford */
	tt[0] =  0.8951 * in[0] +  0.2664 * in[1] + -0.1614 * in[2];
	tt[1] = -0.7502 * in[0] +  1.7135 * in[1] +  0.0367 * in[2];
	tt[2] =  0.0389 * in[0] + -0.0685 * in[1] +  1.0296 * in[2];

	/* Scale from D50 to absolute white point */
	tt[0] = tt[0] * ABS_BX/D50_BX;
    tt[1] = tt[1] * ABS_BY/D50_BY;
    tt[2] = tt[2] * ABS_BZ/D50_BZ;

	/* Inverse bradford */
	out[0] =  0.986993 * tt[0] + -0.147054 * tt[1] + 0.159963 * tt[2];
	out[1] =  0.432305 * tt[0] + 0.518360  * tt[1] + 0.049291 * tt[2];
	out[2] = -0.008529 * tt[0] + 0.040043  * tt[1] + 0.968487 * tt[2];
}

/* Convert from normalized to absolute XYZ */
static void to_abs(double out[3], double in[3]) {

	to_rel(out, in);			/* Convert to relative */
	rel_to_abs(out, out);		/* Convert to absolute */

}

/* Convert from absolute to relative XYZ */
static void abs_to_rel(double out[3], double in[3]) {
	double tt[3];

	/* Multiply by bradford */
	tt[0] =  0.8951 * in[0] +  0.2664 * in[1] + -0.1614 * in[2];
	tt[1] = -0.7502 * in[0] +  1.7135 * in[1] +  0.0367 * in[2];
	tt[2] =  0.0389 * in[0] + -0.0685 * in[1] +  1.0296 * in[2];

	/* Scale from absolute white point to D50 */
	tt[0] = tt[0] * D50_BX/ABS_BX;
    tt[1] = tt[1] * D50_BY/ABS_BY;
    tt[2] = tt[2] * D50_BZ/ABS_BZ;

	/* Inverse bradford */
	out[0] =  0.986993 * tt[0] + -0.147054 * tt[1] + 0.159963 * tt[2];
	out[1] =  0.432305 * tt[0] +  0.518360 * tt[1] + 0.049291 * tt[2];
	out[2] = -0.008529 * tt[0] +  0.040043 * tt[1] + 0.968487 * tt[2];
}

#ifdef NEVER	/* Not currently used */
/* Convert from absolute to normalized XYZ */
static void from_abs(double out[3], double in[3]) {
	
	abs_to_rel(out, in);		/* Convert to relative */
	from_rel(out, out);			/* Convert to normalised */
}
#endif /* NEVER */

/* CIE XYZ to perceptual Lab with ICC D50 white point */
static void
XYZ2Lab(double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double x,y,z,fx,fy,fz;
	double L;

	x = X/D50_X;
	if (x > 0.008856451586)
		fx = pow(x,1.0/3.0);
	else
		fx = 7.787036979 * x + 16.0/116.0;

	y = Y/D50_Y;
	if (y > 0.008856451586) {
		fy = pow(y,1.0/3.0);
		L = 116.0 * fy - 16.0;
	} else {
		fy = 7.787036979 * y + 16.0/116.0;
		L = 903.2963058 * y;
	}

	z = Z/D50_Z;
	if (z > 0.008856451586)
		fz = pow(z,1.0/3.0);
	else
		fz = 7.787036979 * z + 16.0/116.0;

	out[0] = L;
	out[1] = 500.0 * (fx - fy);
	out[2] = 200.0 * (fy - fz);
}

/* Perceptual Lab with ICC D50 white point to CIE XYZ */
static void
Lab2XYZ(double *out, double *in) {
	double L = in[0], a = in[1], b = in[2];
	double x,y,z,fx,fy,fz;

	if (L > 8.0) {
		fy = (L + 16.0)/116.0;
		y = pow(fy,3.0);
	} else {
		y = L/903.2963058;
		fy = 7.787036979 * y + 16.0/116.0;
	}

	fx = a/500.0 + fy;
	if (fx > 24.0/116.0)
		x = pow(fx,3.0);
	else
		x = (fx - 16.0/116.0)/7.787036979;

	fz = fy - b/200.0;
	if (fz > 24.0/116.0)
		z = pow(fz,3.0);
	else
		z = (fz - 16.0/116.0)/7.787036979;

	out[0] = x * D50_X;
	out[1] = y * D50_Y;
	out[2] = z * D50_Z;
}

/* - - - - - - - - - - - - - */

/* Return maximum difference */
static double maxdiff(double in1[3], double in2[3]) {
	double tt, rv = 0.0;
	if ((tt = fabs(in1[0] - in2[0])) > rv)
		rv = tt;
	if ((tt = fabs(in1[1] - in2[1])) > rv)
		rv = tt;
	if ((tt = fabs(in1[2] - in2[2])) > rv)
		rv = tt;
	return rv;
}

/* Return absolute difference */
static double absdiff(double in1[3], double in2[3]) {
	double tt, rv = 0.0;
	tt = in1[0] - in2[0];
	rv += tt * tt;
	tt = in1[1] - in2[1];
	rv += tt * tt;
	tt = in1[2] - in2[2];
	rv += tt * tt;
	return sqrt(rv);
}

/* - - - - - - - - - - - - - - - - - */
# define XARG , int tn
# define XPRM , 0
/* - - - - - - - - - - - - - - - - - */
/* Overall Monochrome XYZ device model is */
/* Gray -> GrayY -> XYZ */
/* Where GrayY is assumed to directly Scale Y */

/* Gray -> GrayY */
static void Gray_GrayY(void *cntx, double *out, double *in XARG) {
#ifdef TESTLIN1
	*out = *in;
#else
	*out = ppow(*in,1.6);
#endif
}

/* GrayY -> Gray */
static double GrayY_Gray(double in) {
#ifdef TESTLIN1
	return in;
#else
	return ppow(in,1.0/1.6);
#endif
}

/* Gray -> XYZ */
static void Gray_XYZ(double out[3], double in) {
	double temp[3];
	Gray_GrayY(NULL, &temp[0], &in XPRM);
	Gray_GrayY(NULL, &temp[1], &in XPRM);
	Gray_GrayY(NULL, &temp[2], &in XPRM);

	/* Scale to relative white and black points */
	to_rel(out, temp);
}

/* XYZ -> Device gray space */
static double XYZ_Gray(double in[3]) {
	double temp[3];

	/* Remove relative white and black points scale */
	from_rel(temp, in);

	/* Do calculation just from Y value */
	return GrayY_Gray(temp[1]);
}

/* Gray -> XYZ absolute */
static void aGray_XYZ(double out[3], double in) {
	double temp[3];
	Gray_GrayY(NULL, &temp[0], &in XPRM);
	Gray_GrayY(NULL, &temp[1], &in XPRM);
	Gray_GrayY(NULL, &temp[2], &in XPRM);

	/* Scale to absolute white and black points */
	to_abs(out, temp);

}

/* XYZ -> Gray absolute */
static double aXYZ_Gray(double in[3]) {
	double tt, temp[3];

	/* Remove absolute white and black points scale */
	abs_to_rel(temp, in);		/* Convert to relative */

	from_rel(temp, temp);			/* Convert to normalised */

	/* Do calculation just from Y value */
	tt = GrayY_Gray(temp[1]);

	return tt;
}

/* - - - - - - - - - - - */
/* Overall Monochrome Lab device model is */
/* Gray -> GrayL -> Lab */
/* Where GrayL is assumed to directly scale L */


/* Gray -> GrayL */
static void Gray_GrayL(void *cntx, double *out, double *in XARG) {
#ifdef TESTLIN1
	*out = in;				/* normalized L */
#else
	*out = ppow(*in,1.6);
#endif
}

/* GrayL -> Gray */
static double GrayL_Gray(double in) {
#ifdef TESTLIN1
	return in;
#else
	return ppow(in,1.0/1.6);	/* Y */
#endif
}

/* Gray -> Lab */
static void Gray_Lab(double out[3], double in) {
	double tt, wp[3];

	wp[0] = D50_X;
	wp[1] = D50_Y;
	wp[2] = D50_Z;
	XYZ2Lab(wp, wp);		/* Lab white point */

	Gray_GrayL(NULL, &tt, &in XPRM);	/* Raw L value */

	/* Scale to relative Lab white point */
	out[0] = wp[0] * tt;
	out[1] = wp[1] * tt;
	out[2] = wp[2] * tt;
}

/* Lab -> Gray */
static double Lab_Gray(double in[3]) {
	double tt, wp[3];

	wp[0] = D50_X;
	wp[1] = D50_Y;
	wp[2] = D50_Z;
	XYZ2Lab(wp, wp);		/* Lab white point */

	/* Scale from relative Lab white point */
	tt = in[0]/wp[0];

	return GrayL_Gray(tt);	/* Raw L value */

}

/* Gray -> Lab absolute */
static void aGray_Lab(double out[3], double in) {

	/* Generate relative Lab */
	Gray_Lab(out, in);

	Lab2XYZ(out, out);
	rel_to_abs(out, out);	
	XYZ2Lab(out, out);
}

/* Lab -> Gray  absolute */
static double aLab_Gray(double in[3]) {
	double tt[3];

	Lab2XYZ(tt, in);
	abs_to_rel(tt, tt);	
	XYZ2Lab(tt, tt);
	
	return Lab_Gray(tt);
}

/* - - - - - - - - - - - - - - - - - */
/* RGB -> XYZ test transfer curves */
/* Note that overall model is: */
/* RBG -> RGB' -> XYZ' -> XYZ */

/* Device space linearization */
/* RGB -> RGB' */
static void RGB_RGBp(void *cntx, double out[3], double in[3] XARG) {
#ifdef TESTLIN1
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
#else
	out[0] = ppow(in[0],1.6);
	out[1] = ppow(in[1],1.5);
	out[2] = ppow(in[2],1.4);
#endif
}

/* RGB' -> RGB */
static void RGBp_RGB(void *cntx, double out[3], double in[3] XARG) {
#ifdef TESTLIN1
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
#else
	out[0] = ppow(in[0],1.0/1.6);
	out[1] = ppow(in[1],1.0/1.5);
	out[2] = ppow(in[2],1.0/1.4);
#endif
}

#ifdef TESTLIN2
static double matrix[3][3] = {
	{ 1.0, 0.0, 0.0 },
	{ 0.0, 1.0, 0.0 },
	{ 0.0, 0.0, 1.0 },
};
#else
static double matrix[3][3] = {
	{ 0.4361, 0.3851, 0.1431 },
	{ 0.2225, 0.7169, 0.0606 },
	{ 0.0139, 0.0971, 0.7141 },
};
#endif

/* 3x3 matrix conversion */
/* RGB' -> XYZ' */
static void RGBp_XYZp(void *cntx, double out[3], double in[3] XARG) {
	double o0,o1,o2;

#ifdef TESTLIN2
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
#else
	o0 = matrix[0][0] * in[0] + matrix[0][1] * in[1] + matrix[0][2] * in[2];
	o1 = matrix[1][0] * in[0] + matrix[1][1] * in[1] + matrix[1][2] * in[2];
	o2 = matrix[2][0] * in[0] + matrix[2][1] * in[1] + matrix[2][2] * in[2];

	out[0] = o0;
	out[1] = o1;
	out[2] = o2;
#endif
}

/* XYZ' -> RGB' */
static void XYZp_RGBp(void *cntx, double out[3], double in[3] XARG) {
	double o0,o1,o2;

#ifdef TESTLIN2
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
#else
	o0 =  3.13360257102309000 * in[0] + -1.61682140135654430 * in[1] + -0.49074240441282400 * in[2];
	o1 = -0.97865031588250000 * in[0] +  1.91606100412532800 * in[1] +  0.03351290204844009 * in[2];
	o2 =  0.07207655781398956 * in[0] + -0.22906554547222160 * in[1] +  1.40535949675456500 * in[2];

	out[0] = o0;
	out[1] = o1;
	out[2] = o2;
#endif
}

/* Output linearization curves */
/* XYZ' -> XYZ */
static void XYZp_XYZ(void *cntx, double out[3], double in[3] XARG) {
#ifdef TESTLIN3
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
#else
	out[0] = ppow(in[0],0.9);
	out[1] = ppow(in[1],0.8);
	out[2] = ppow(in[2],1.1);
#endif
}

/* XYZ -> XYZ' */
static void XYZ_XYZp(void *cntx, double out[3], double in[3] XARG) {
#ifdef TESTLIN3
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
#else
	out[0] = ppow(in[0],1.0/0.9);
	out[1] = ppow(in[1],1.0/0.8);
	out[2] = ppow(in[2],1.0/1.1);
#endif
}

/* RGB -> XYZ' */
static void RGB_XYZp(void *cntx, double out[3], double in[3]) {
	RGB_RGBp(cntx, out, in XPRM);
	RGBp_XYZp(cntx, out, out XPRM);
}

/* RGB -> XYZ', absolute (for matrix profile test) */
static void aRGB_XYZp(void *cntx, double out[3], double in[3]) {
	RGB_RGBp(cntx, out, in XPRM);
	RGBp_XYZp(cntx, out, out XPRM);
	from_rel(out, out);
	to_abs(out, out);
}

/* RGB -> XYZ */
static void RGB_XYZ(void *cntx, double out[3], double in[3]) {
	RGB_RGBp(cntx, out, in XPRM);
	RGBp_XYZp(cntx, out, out XPRM);
	XYZp_XYZ(cntx, out, out XPRM);
}

/* XYZ -> RGB */
static void XYZ_RGB(void *cntx, double out[3], double in[3]) {
	XYZ_XYZp(cntx, out, in XPRM);
	XYZp_RGBp(cntx, out, out XPRM);
	RGBp_RGB(cntx, out, out XPRM);
}

/* RGB -> XYZ, absolute */
static void aRGB_XYZ(void *cntx, double out[3], double in[3]) {
	RGB_XYZ(cntx, out, in);
	from_rel(out, out);
	to_abs(out, out);
}

#ifdef NEVER	/* Not currently used */

/* XYZ -> RGB, absolute */
static void aXYZ_RGB(void *cntx, double out[3], double in[3]) {
	from_abs(out, in);
	to_rel(out, out);
	XYZ_RGB(cntx, out, out);
}

/* XYZ -> RGB, gamut constrained */
static void cXYZ_RGB(void *cntx, double out[3], double in[3]) {
	XYZ_RGB(cntx, out, in);
	clip(out);
}

#endif /* NEVER */

/* XYZ' -> distance to gamut boundary */
static void XYZp_BDIST(void *cntx, double out[1], double in[3] XARG) {
	double gdst;				/* Gamut error */
	int m, mini = 0, outg;
	double tt, mind;
	double pcs[3];				/* PCS value of input */
	double dev[3];				/* Device value */
	double pgb[3];				/* PCS gamut boundary point */
	double dgb[3];				/* device gamut boundary point */

	/* Do XYZ' -> XYZ */
	XYZp_XYZ(cntx, pcs, in XPRM);

	/* Do XYZ -> RGB transform */
	XYZ_RGB(NULL, dev, pcs);

	/* Compute nearest point on gamut boundary, */
	/* and whether it is in or out of gamut. */
	/* This should be nearest in PCS space, but */
	/* we'll cheat and find the nearest point in */
	/* device space, and then compute the distance in PCS space. */

	for (m = 0; m < 3; m++)
		dgb[m] = dev[m];

	for (mind = 10000.0, outg = 0, m = 0; m < 3; m++) {
		if (dev[m] < 0.0) {		/* Clip any coordinates outside device limits */
			dgb[m] = 0.0;
			outg = 1;			/* Out of gamut */
		} else if (dev[m] > 1.0) {
			dgb[m] = 1.0;
			outg = 1;			/* Out of gamut */
		} else {				/* Note closest cood to boundary if within limits */
			if (dev[m] < 0.5)
				tt = 0.5 - dev[m];
			else 	/* >= 0.5 */
				tt = dev[m] - 0.5;
			if (tt < mind) {
				mind = tt;
				mini = m;
			}
		}
	}
	if (!outg) {				/* If point is within gamut, set to closest point */
		if (dev[mini] < 0.5)
			dgb[mini] = 0.0;
		else
			dgb[mini] = 1.0;
	}
	
	/* Do RGB -> XYZ transform on nearest gamut boundary point */
	RGB_XYZ(NULL, pgb, dgb);

	/* Distance to nearest gamut point in PCS (XYZ) space */
	gdst = absdiff(pcs, pgb);
	if (!outg)	/* If within gamut */
		gdst = -gdst;

	/* Distance in PCS space will be roughly -0.866 -> 0.866 */
	/* Convert so that 0.5 is on boundary, and then clip. */
	gdst += 0.5;
	if (gdst < 0.0)
		gdst = 0.0;
	else if (gdst > 1.0)
		gdst = 1.0;

	out[0] = gdst;
}

/* The output table is usually a special for the gamut table, returning */
/* a value of 0 for all inputs <= 0.5, and then outputing between */
/* 0.0 and 1.0 for the input range 0.5 to 1.0. This is so a graduated */
/* "gamut boundary distance" number from the multi-d lut can be */
/* translated into the ICC "0.0 if in gamut, > 0.0 if not" number. */
static void BDIST_GAMMUT(void *cntx, double out[1], double in[1] XARG) {
	double iv, ov;
	iv = in[0];
	if (iv <= 0.5)
		ov = 0.0;
	else
		ov = (iv - 0.5) * 2.0;
	out[0] = ov;
}

/* - - - - - - - - - - - - - */
/* Lab versions for Lut profile, built on top of XYZ model */
/* The overall model is: */
/* RBG -> RGB' -> Lab' -> Lab */

/* 3x3 matrix conversion */
/* RGB' -> Lab' */
static void RGBp_Labp(void *cntx, double out[3], double in[3] XARG) {
	RGBp_XYZp(cntx, out, in XPRM);
	XYZ2Lab(out, out);
}

/* Lab' -> RGB' */
static void Labp_RGBp(void *cntx, double out[3], double in[3] XARG) {
	Lab2XYZ(out, in);
	XYZp_RGBp(cntx, out, out XPRM);
}

/* Lab' -> Lab */
/* (We are using linear) */
static void Labp_Lab(void *cntx, double out[3], double in[3] XARG) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

/* Lab -> Lab' */
static void Lab_Labp(void *cntx, double out[3], double in[3] XARG) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

/* RGB -> Lab */
static void RGB_Lab(void *cntx, double out[3], double in[3]) {
	RGB_RGBp(cntx, out, in XPRM);
	RGBp_Labp(cntx, out, out XPRM);
	Labp_Lab(cntx, out, out XPRM);
}

/* Lab -> RGB */
static void Lab_RGB(void *cntx, double out[3], double in[3]) {
	Lab_Labp(cntx, out, in XPRM);
	Labp_RGBp(cntx, out, out XPRM);
	RGBp_RGB(cntx, out, out XPRM);
}

/* RGB -> Lab, absolute */
static void aRGB_Lab(void *cntx, double out[3], double in[3]) {
	RGB_Lab(cntx, out, in);
	Lab2XYZ(out, out);
	from_rel(out, out);
	to_abs(out, out);
	XYZ2Lab(out, out);
}

#ifdef NEVER	/* Not currently used */

/* Lab -> RGB, absolute */
static void aLab_RGB(void *cntx, double out[3], double in[3]) {
	Lab2XYZ(out, in);
	from_abs(out, out);
	to_rel(out, out);
	XYZ2Lab(out, out);
	Lab_RGB(cntx, out, out);
}

/* Lab -> RGB, gamut constrained */
static void cLab_RGB(void *cntx, double out[3], double in[3]) {
	Lab_RGB(cntx, out, in);
	clip(out);
}

#endif /* NEVER */

/* Lab' -> distance to gamut boundary */
static void Labp_BDIST(void *cntx, double out[1], double in[3] XARG) {
	double gdst;				/* Gamut error */
	int m, mini = 0, outg;
	double tt, mind;
	double pcs[3];				/* PCS value of input */
	double dev[3];				/* Device value */
	double pgb[3];				/* PCS gamut boundary point */
	double dgb[3];				/* device gamut boundary point */

	/* Do Lab' -> Lab */
	Labp_Lab(cntx, pcs, in XPRM);

	/* Do Lab -> RGB transform */
	Lab_RGB(cntx, dev, pcs);

	/* Compute nearest point on gamut boundary, */
	/* and whether it is in or out of gamut. */
	/* This should be nearest in PCS space, but */
	/* we'll cheat and find the nearest point in */
	/* device space, and then compute the distance in PCS space. */

	for (m = 0; m < 3; m++)
		dgb[m] = dev[m];

	for (mind = 10000.0, outg = 0, m = 0; m < 3; m++) {
		if (dev[m] < 0.0) {		/* Clip any coordinates outside device limits */
			dgb[m] = 0.0;
			outg = 1;			/* Out of gamut */
		} else if (dev[m] > 1.0) {
			dgb[m] = 1.0;
			outg = 1;			/* Out of gamut */
		} else {				/* Note closest cood to boundary if within limits */
			if (dev[m] < 0.5)
				tt = 0.5 - dev[m];
			else 	/* >= 0.5 */
				tt = dev[m] - 0.5;
			if (tt < mind) {
				mind = tt;
				mini = m;
			}
		}
	}
	if (!outg) {				/* If point is within gamut, set to closest point */
		if (dev[mini] < 0.5)
			dgb[mini] = 0.0;
		else
			dgb[mini] = 1.0;
	}
	
	/* Do RGB -> Lab transform on nearest gamut boundary point */
	RGB_Lab(NULL, pgb, dgb);

	/* Distance to nearest gamut point in PCS (Lab) space */
	gdst = absdiff(pcs, pgb);
	if (!outg)	/* If within gamut */
		gdst = -gdst;

	/* Distance in PCS space will be roughly -86 -> 86 */
	/* Convert so that 0.5 is on boundary, and then clip. */
	gdst /= 100.0;
	gdst += 0.5;
	if (gdst < 0.0)
		gdst = 0.0;
	else if (gdst > 1.0)
		gdst = 1.0;

	out[0] = gdst;
}

/* - - - - - - - - - - - - - */

static int check_parts(char *name, icc *icco);

#define TRES 10
#define MON_POINTS 8101		/* Number of test points in monochrome tests */

void usage(void) {
	printf("ICC library lu test, V%s\n",ICCLIB_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: lutest [-v level] [-w] [-r]\n");
	fprintf(stderr," -v level      Verbosity level\n");
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
	int verb = 0;
	int wonly = 0;
	int ronly = 0;
	int warn = 0;
	int fail = 0;

	icmErr e = { 0, { '\000'} };
	char *file_name;
	icmFile *wr_fp, *rd_fp;
	icc *wr_icco, *rd_icco;		/* Keep object separate */
	char *tpfx = "";
	double revthr;
	int rv = 0;

	/* Check variables */
	int co[3];
	double in[3], out[3], check[3];

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
			else if (argv[fa][1] == 'v') {
				fa = nfa;
				if (na == NULL)
					verb = 1;
				else
					verb = na[0] - '1';
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

	{

		printf("Starting lookup function test - V%s\n",ICCLIB_VERSION_STR);

		/* Do a check that our reference function is reversable */
		for (co[0] = 0; co[0] < TRES; co[0]++) {
			in[0] = co[0]/(TRES-1.0);
			for (co[1] = 0; co[1] < TRES; co[1]++) {
				in[1] = co[1]/(TRES-1.0);
				for (co[2] = 0; co[2] < TRES; co[2]++) {
					double mxd;
					in[2] = co[2]/(TRES-1.0);

					/* Do device -> XYZ transform */
					RGB_XYZ(NULL, out, in);
			
					/* Do XYZ -> Device transform */
					XYZ_RGB(NULL, check, out);

					/* Check the result */
					mxd = maxdiff(in, check);
					if (mxd > 0.00001) {
						warning ("######## Excessive error %f > 0.00001 ########",mxd);
						fail = 1;
#ifdef STOPONERROR
						goto done1;
#endif /* STOPONERROR */
					}
				}
			}
		}
	  done1:;
		printf("Self check complete\n");
	}


	/* ---------------------------------------- */
	/* Create a monochrome XYZ profile to test      */
	/* ---------------------------------------- */

	/* Open up the file for writing */
	file_name = "xxxx_mono_XYZ.icm";
	tpfx = "";
	revthr = 0.0002;

	if (!ronly) {
		icm_err_clear_e(&e);

		if ((wr_fp = new_icmFileStd_name(&e, file_name,"w")) == NULL)
			error ("Write: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((wr_icco = new_icc(&e)) == NULL)
			error ("Write: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			wr_icco->warning = Warning;

#ifdef IGNORE_WRITE_FORMAT_ERRORS
		wr_icco->set_cflag(wr_icco, icmCFlagWrFormatWarn);
#endif

		/* The header: */
		{
			icmHeader *wh = wr_icco->header;
	
			/* Values that must be set before writing */
			wh->deviceClass     = icSigDisplayClass;	/* Could use Output or Input too */
	    	wh->colorSpace      = icSigGrayData;		/* It's a gray space */
	    	wh->pcs             = icSigXYZData;			/* Test XYZ monochrome profile */
	    	wh->renderingIntent = icRelativeColorimetric;
	
			/* Values that should be set before writing */
			wh->manufacturer = icmstr2tag("tst2");
	    	wh->model        = icmstr2tag("test");
		}
		/* Profile Description Tag: */
		{
			icmCommonTextDescription *wo;
			char *dst = "This is a test monochrome XYZ style Display Profile";
			if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigProfileDescriptionTag,	icmSigCommonTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = strlen(dst)+1; 	/* Allocated and used size of desc, inc null */
			wo->allocate(wo);/* Allocate space */
			strcpy(wo->desc, dst);		/* Copy the string in */
		}
		/* Copyright Tag: */
		{
			icmCommonTextDescription *wo;
			char *crt = "Copyright 2023 Graeme Gill";
			if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigCopyrightTag,	icmSigCommonTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = strlen(crt)+1; 	/* Allocated and used size of text, inc null */
			wo->allocate(wo);/* Allocate space */
			strcpy(wo->desc, crt);		/* Copy the text in */
		}
		/* White Point Tag: */
		{
			icmXYZArray *wo;
			/* Note that tag types icSigXYZType and icSigXYZArrayType are identical */
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaWhitePointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = ABS_X;			/* Set some silly numbers */
			wo->data[0].Y = ABS_Y;
			wo->data[0].Z = ABS_Z;
		}
		/* Black Point Tag: */
		{
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaBlackPointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = 0.02 * ABS_X;
			wo->data[0].Y = 0.02 * ABS_Y;
			wo->data[0].Z = 0.02 * ABS_Z;
		}
		{
			/* Intent 1 = relative colorimetric */
			int nsigs = 1;
			icmXformSigs sigs[2] = { { icmSigShaperMono, icmSigShaperMatrixType} };

			if (wr_icco->create_mono_xforms(wr_icco, ICM_CREATE_FLAG_NONE, NULL,
					nsigs, sigs,		/* Tables to be set */
					256, 256, 			/* Table resolution */
					icSigGrayData, 		/* Input color space */
					icSigXYZData, 		/* Output color space */
					Gray_GrayY			/* Monochrome table function */
			) != 0)
				error("Setting 16 bit RGB->XYZ Lut failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
	
		/* Write the file out */
		if ((rv = wr_icco->write(wr_icco,wr_fp,0)) != 0)
			error ("Write file: %d, %s",rv,wr_icco->e.m);
		
		wr_icco->del(wr_icco);
		wr_fp->del(wr_fp);
	}

	/* - - - - - - - - - - - - - - */
	/* Deal with reading and verifying the monochrome XYZ profile */

	if (!wonly) {
		icm_err_clear_e(&e);

		/* Open up the file for reading */
		if ((rd_fp = new_icmFileStd_name(&e, file_name,"r")) == NULL)
			error ("Read: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((rd_icco = new_icc(&e)) == NULL)
			error ("Read: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			rd_icco->warning = Warning;

		/* Read the header and tag list */
		if ((rv = rd_icco->read(rd_icco,rd_fp,0)) != 0)
			error ("Read: %d, %s",rv,rd_icco->e.m);
	
		if (check_parts(file_name, rd_icco)) {
			fail = 1;
		}

		/* Check the lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;

			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icmDefaultIntent,
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < MON_POINTS; co[0]++) {
				double mxd;
				in[0] = co[0]/(MON_POINTS-1.0);
	
				/* Do reference conversion of device -> XYZ transform */
				Gray_XYZ(check,in[0]);
		
				/* Do lookup of device -> XYZ transform */
				if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
					error ("Lookup error %s",icmPe_lurv2str(rv));
		
				/* Check the result */
				mxd = maxdiff(out, check);
				if (mxd > 0.00005) {
					warning ("######## Excessive error in Monochrome%s XYZ Fwd %f > 0.00005 ########",tpfx,mxd);
					fail = 1;
#ifdef STOPONERROR
					break;
#endif /* STOPONERROR */
				}
				if (mxd > merr)
					merr = mxd;
			}
			printf("Monochrome%s XYZ fwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the reverse lookup function */
		{
			double min[3], range[3];
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Establish the range */
			Gray_XYZ(min,0.0);
			Gray_XYZ(range,1.0);
			range[0] -= min[0];
			range[1] -= min[1];
			range[2] -= min[2];
		
			/* Get a bwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("%d, %s",rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < MON_POINTS; co[0]++) {
				double mxd;
				in[0] = range[0] * co[0]/(MON_POINTS-1.0) + min[0];
				in[1] = range[1] * co[0]/(MON_POINTS-1.0) + min[1];
				in[2] = range[2] * co[0]/(MON_POINTS-1.0) + min[2];
	
				/* Do reference conversion of XYZ -> device transform */
				check[0] = XYZ_Gray(in);
	
				/* Do reverse lookup of XYZ -> device transform */
				if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
					error ("Lookup error %s",icmPe_lurv2str(rv));
	
				/* Check the result */
				mxd = fabs(check[0] - out[0]);
				if (mxd > revthr) {
					warning ("######## Excessive error in Monochrome%s XYZ Bwd %f > %f ########",tpfx,mxd,revthr);
					fail = 1;
#ifdef STOPONERROR
					break;
#endif /* STOPONERROR */
				}
				if (mxd > merr)
					merr = mxd;
			}
			printf("Monochrome%s XYZ bwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the lookup function, absolute colorimetric */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < MON_POINTS; co[0]++) {
				double mxd;
				in[0] = co[0]/(MON_POINTS-1.0);
	
				/* Do reference conversion of device -> XYZ transform */
				aGray_XYZ(check,in[0]);
		
				/* Do lookup of device -> XYZ transform */
				if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
					error ("Lookup error %s",icmPe_lurv2str(rv));
		
				/* Check the result */
				mxd = maxdiff(out, check);
				if (mxd > revthr) {
					warning ("######## Excessive error in Monochrome%s XYZ Abs Fwd\n    %s -> %s, should be %s, diff %f > %f ########",tpfx,icmPdv(1, in),icmPdv(3, out),icmPdv(3, check),mxd,revthr);
					fail = 1;
#ifdef STOPONERROR
					break;
#endif /* STOPONERROR */
				}
				if (mxd > merr)
					merr = mxd;
			}
			printf("Monochrome%s XYZ fwd absolute intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the reverse lookup function, absolute colorimetric */
		{
			double min[3], range[3];
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Establish the range */
			/* Establish the range */
			aGray_XYZ(min,0.0);
			aGray_XYZ(range,1.0);
			range[0] -= min[0];
			range[1] -= min[1];
			range[2] -= min[2];
		
			/* Get a bwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < MON_POINTS; co[0]++) {
				double mxd;
				in[0] = range[0] * co[0]/(MON_POINTS-1.0) + min[0];
				in[1] = range[1] * co[0]/(MON_POINTS-1.0) + min[1];
				in[2] = range[2] * co[0]/(MON_POINTS-1.0) + min[2];
	
				/* Do reference conversion of XYZ -> device transform */
				check[0] = aXYZ_Gray(in);
	
				/* Do reverse lookup of device -> XYZ transform */
				if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
					error ("Lookup error %s",icmPe_lurv2str(rv));
	
				/* Check the result */
				mxd = fabs(check[0] - out[0]);
				if (mxd > revthr) {
					warning ("######## Excessive error in Monochrome%s XYZ Abs Bwd %f > %f ########",tpfx,mxd,revthr);
					fail = 1;
#ifdef STOPONERROR
					break;
#endif /* STOPONERROR */
				}
				if (mxd > merr)
					merr = mxd;
			}
			printf("Monochrome%s XYZ bwd absolute intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		rd_icco->del(rd_icco);
		rd_fp->del(rd_fp);
	}

	/* ---------------------------------------- */
	/* Create a monochrome Lab profile to test      */
	/* ---------------------------------------- */

	/* Open up the file for writing */
	file_name = "xxxx_mono_Lab.icm";
	tpfx = "";
	revthr = 0.005;

	if (!ronly) {
		icm_err_clear_e(&e);

		if ((wr_fp = new_icmFileStd_name(&e, file_name,"w")) == NULL)
			error ("Write: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((wr_icco = new_icc(&e)) == NULL)
			error ("Write: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			wr_icco->warning = Warning;

#ifdef IGNORE_WRITE_FORMAT_ERRORS
		wr_icco->set_cflag(wr_icco, icmCFlagWrFormatWarn);
#endif

		/* Add all the tags required */
	
		/* The header: */
		{
			icmHeader *wh = wr_icco->header;
	
			/* Values that must be set before writing */
			wh->deviceClass     = icSigDisplayClass;	/* Could use Output or Input too */
	    	wh->colorSpace      = icSigGrayData;		/* It's a gray space */
	    	wh->pcs             = icSigLabData;			/* Use Lab for this monochrome profile */
	    	wh->renderingIntent = icRelativeColorimetric;
	
			/* Values that should be set before writing */
			wh->manufacturer = icmstr2tag("tst2");
	    	wh->model        = icmstr2tag("test");
		}
		/* Profile Description Tag: */
		{
			icmCommonTextDescription *wo;
			char *dst = "This is a test monochrome Lab style Display Profile";
			if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigProfileDescriptionTag,	icmSigCommonTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = strlen(dst)+1; 	/* Allocated and used size of desc, inc null */
			wo->allocate(wo);/* Allocate space */
			strcpy(wo->desc, dst);		/* Copy the string in */
		}
		/* Copyright Tag: */
		{
			icmCommonTextDescription *wo;
			char *crt = "Copyright 1998 Graeme Gill";
			if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigCopyrightTag,	icmSigCommonTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = strlen(crt)+1; 	/* Allocated and used size of text, inc null */
			wo->allocate(wo);/* Allocate space */
			strcpy(wo->desc, crt);		/* Copy the text in */
		}
		/* White Point Tag: */
		{
			icmXYZArray *wo;
			/* Note that tag types icSigXYZType and icSigXYZArrayType are identical */
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaWhitePointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = ABS_X;	/* Set some silly numbers */
			wo->data[0].Y = ABS_Y;
			wo->data[0].Z = ABS_Z;
		}
		/* Black Point Tag: */
		{
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaBlackPointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = 0.02;			/* Doesn't take part in Absolute anymore */
			wo->data[0].Y = 0.04;
			wo->data[0].Z = 0.03;
		}
		{
			/* Intent 1 = relative colorimetric */
			int nsigs = 1;
			icmXformSigs sigs[2] = { { icmSigShaperMono, icmSigShaperMatrixType} };

			if (wr_icco->create_mono_xforms(wr_icco, ICM_CREATE_FLAG_NONE, NULL,
					nsigs, sigs,		/* Tables to be set */
					256, 256, 			/* Table resolution */
					icSigGrayData, 		/* Input color space */
					icSigLabData, 		/* Output color space */
					Gray_GrayL			/* Monochrome table function */
			) != 0)
				error("Setting 16 bit grey->Lab failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
	
		/* Write the file out */
		if ((rv = wr_icco->write(wr_icco,wr_fp,0)) != 0)
			error ("Write file: %d, %s",rv,wr_icco->e.m);
		
		wr_icco->del(wr_icco);
		wr_fp->del(wr_fp);
	}

	/* - - - - - - - - - - - - - - */
	/* Deal with reading and verifying the monochrome Lab profile */

	if (!wonly) {
		icm_err_clear_e(&e);

		/* Open up the file for reading */
		if ((rd_fp = new_icmFileStd_name(&e, file_name,"r")) == NULL)
			error ("Read: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((rd_icco = new_icc(&e)) == NULL)
			error ("Read: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			rd_icco->warning = Warning;

		/* Read the header and tag list */
		if ((rv = rd_icco->read(rd_icco,rd_fp,0)) != 0)
			error ("Read: %d, %s",rv,rd_icco->e.m);
	
		if (check_parts(file_name, rd_icco)) {
			fail = 1;
		}

		/* Check the lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < MON_POINTS; co[0]++) {
				double mxd;
				in[0] = co[0]/(MON_POINTS-1.0);
	
				/* Do reference conversion of device -> Lab transform */
				Gray_Lab(check,in[0]);
		
				/* Do lookup of device -> Lab transform */
				if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
					error ("Lookup error %s",icmPe_lurv2str(rv));
		
				/* Check the result */
				mxd = maxdiff(out, check);
				if (mxd > 0.0025) {
					warning ("######## Excessive error in Monochrome%s Lab Fwd %f > 0.0025 ########",tpfx,mxd);
					fail = 1;
#ifdef STOPONERROR
					break;
#endif /* STOPONERROR */
				}
				if (mxd > merr)
					merr = mxd;
			}
			printf("Monochrome%s Lab fwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the reverse lookup function */
		{
			double min, range;
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Establish the range */
			Gray_Lab(out,0.0);
			min = out[0];
			Gray_Lab(out,1.0);
			range = out[0] - min;
		
			/* Get a bwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < MON_POINTS; co[0]++) {
				double mxd;
				in[0] = range * co[0]/(MON_POINTS-1.0) + min;
	
				/* Do reference conversion of Lab -> device transform */
				check[0] = Lab_Gray(in);
	
				/* Do reverse lookup of Lab -> device transform */
				if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
					error ("Lookup error %s",icmPe_lurv2str(rv));
	
				/* Check the result */
				mxd = fabs(check[0] - out[0]);
				if (mxd > revthr) {
					warning ("######## Excessive error in Monochrome %s Lab Bwd %f > %f ########",tpfx,mxd,revthr);
					fail = 1;
#ifdef STOPONERROR
					break;
#endif /* STOPONERROR */
				}
				if (mxd > merr)
					merr = mxd;
			}
			printf("Monochrome%s Lab bwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
#ifdef NEVER
		/* Check the fwd/bwd accuracy */
		{
			double merr = 0.0;
			icmLuSpace *luof, *luob;
	
			/* Get a fwd conversion object */
			if ((luof = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			/* Get a bwd conversion object */
			if ((luob = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			/* Check it out */
			for (co[0] = 0; co[0] < MON_POINTS; co[0]++) {
				double mxd;
				in[0] = co[0]/(MON_POINTS-1.0);
	
				/* Do lookup of device -> Lab transform */
				if ((rv = luof->lookup_fwd(luof, out, in)) & icmPe_lurv_err)
					error ("Lookup error %s",icmPe_lurv2str(rv));
		
				/* Do reverse lookup of device -> Lab transform */
				if ((rv = luob->lookup_fwd(luob, out, in)) & icmPe_lurv_err)
					error ("Lookup error %s",icmPe_lurv2str(rv));
	
				mxd = fabs(in[0] - check[0]);
				if (mxd > 1e-6) {
					printf("co %d, in %f, out %f, check %f, err %f\n",
					        co[0],in[0],out[0],check[0], fabs(in[0] - check[0]));
				}
	
				if (mxd > merr)
					merr = mxd;
			}
			printf("Monochrome%s Lab fwd/bwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup objects */
			luof->del(luof);
			luob->del(luob);
		}
	
		/* Benchmark the routines */
		{
			int ii;
			icmLuSpace *luo;
			double no_pixels = 0.0;
			clock_t stime,ttime;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			stime = clock();
			no_pixels = 1000.0 * 2048.0;
	
			for (ii = 0; ii < 1000; ii++) {
				for (co[0] = 0; co[0] < 2048; co[0]++) {
					double mxd;
					in[0] = co[0]/(2048-1.0);
		
					/* Do lookup of device -> Lab transform */
					if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
						error ("Lookup error %s",icmPe_lurv2str(rv));
				}
			}
			ttime = clock() - stime;
			printf("Done - %f seconds, rate = %f Mpix/sec\n",
		       (double)ttime/CLOCKS_PER_SEC,no_pixels * CLOCKS_PER_SEC/ttime);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		{
			int ii;
			icmLuSpace *luo;
			double no_pixels = 0.0;
			clock_t stime,ttime;
	
			/* Get a bwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			stime = clock();
			no_pixels = 1000.0 * 2048.0;
	
			for (ii = 0; ii < 1000; ii++) {
				for (co[0] = 0; co[0] < 2048; co[0]++) {
					double mxd;
					in[0] = 100.0 * co[0]/(2048-1.0);
		
					/* Do lookup of device -> Lab transform */
					if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
						error ("Lookup error %s",icmPe_lurv2str(rv));
				}
			}
			ttime = clock() - stime;
			printf("Done - %f seconds, rate = %f Mpix/sec\n",
		       (double)ttime/CLOCKS_PER_SEC,no_pixels * CLOCKS_PER_SEC/ttime);
	
			/* Done with lookup object */
			luo->del(luo);
		}
#endif	/* NEVER */
	
		/* Check the lookup function, absolute colorimetric */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < MON_POINTS; co[0]++) {
				double mxd;
				in[0] = co[0]/(MON_POINTS-1.0);
	
				/* Do reference conversion of device -> Lab transform */
				aGray_Lab(check,in[0]);
		
				/* Do lookup of device -> Lab transform */
				if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
					error ("Lookup error %s",icmPe_lurv2str(rv));
		
				/* Check the result */
				mxd = maxdiff(out, check);
				if (mxd > 0.003) {
					warning ("######## Excessive error in Monochrome%s Lab Fwd Abs %f > 0.003 ########",tpfx,mxd);
					fail = 1;
#ifdef STOPONERROR
					break;
#endif /* STOPONERROR */
				}
				if (mxd > merr)
					merr = mxd;
			}
			printf("Monochrome%s Lab fwd absolute intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the reverse lookup function, absolute colorimetric*/
		{
			double min, range;
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Establish the range */
			aGray_Lab(out,0.0);
			min = out[0];
			aGray_Lab(out,1.0);
			range = out[0] - min;
		
			/* Get a bwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < MON_POINTS; co[0]++) {
				double mxd;
				in[0] = range * co[0]/(MON_POINTS-1.0) + min;
				in[1] = in[2] = 0.0;
	
				/* Do reference conversion of Lab -> device transform */
				check[0] = aLab_Gray(in);
	
				/* Do reverse lookup of Lab -> device transform */
				if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
					error ("Lookup error %s",icmPe_lurv2str(rv));
	
				/* Check the result */
				mxd = fabs(check[0] - out[0]);
				if (mxd > revthr) {
					warning ("######## Excessive error in Monochrome%s Lab Bwd Abs %f > %f ########",tpfx,mxd,revthr);
					fail = 1;
#ifdef STOPONERROR
					break;
#endif /* STOPONERROR */
				}
				if (mxd > merr)
					merr = mxd;
			}
			printf("Monochrome%s Lab bwd absolute intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		rd_icco->del(rd_icco);
		rd_fp->del(rd_fp);
	}



	/* ---------------------------------------- */
	/* Create a matrix based profile to test    */
	/* ---------------------------------------- */

	/* Open up the file for writing */
	file_name = "xxxx_matrix.icm";
	tpfx = "";

	if (!ronly) {
		icm_err_clear_e(&e);

		if ((wr_fp = new_icmFileStd_name(&e, file_name,"w")) == NULL)
			error ("Write: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((wr_icco = new_icc(&e)) == NULL)
			error ("Write: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			wr_icco->warning = Warning;

#ifdef IGNORE_WRITE_FORMAT_ERRORS
		wr_icco->set_cflag(wr_icco, icmCFlagWrFormatWarn);
#endif

		/* Add all the tags required */
	
		/* The header: */
		{
			icmHeader *wh = wr_icco->header;
	
			/* Values that must be set before writing */
			wh->deviceClass     = icSigDisplayClass;	/* Could use Output or Input too */
	    	wh->colorSpace      = icSigRgbData;			/* It's and RGBish space */
	    	wh->pcs             = icSigXYZData;			/* Must be XYZ for matrix based profile */
	    	wh->renderingIntent = icRelativeColorimetric;
	
			/* Values that should be set before writing */
			wh->manufacturer = icmstr2tag("tst2");
	    	wh->model        = icmstr2tag("test");
		}
		/* Profile Description Tag: */
		{
			icmCommonTextDescription *wo;
			char *dst = "This is a test matrix style Display Profile";
			if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigProfileDescriptionTag,	icmSigCommonTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = strlen(dst)+1; 	/* Allocated and used size of desc, inc null */
			wo->allocate(wo);/* Allocate space */
			strcpy(wo->desc, dst);		/* Copy the string in */
		}
		/* Copyright Tag: */
		{
			icmCommonTextDescription *wo;
			char *crt = "Copyright 1998 Graeme Gill";
			if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigCopyrightTag,	icmSigCommonTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = strlen(crt)+1; 	/* Allocated and used size of text, inc null */
			wo->allocate(wo);/* Allocate space */
			strcpy(wo->desc, crt);		/* Copy the text in */
		}
		/* White Point Tag: */
		{
			icmXYZArray *wo;
			/* Note that tag types icSigXYZType and icSigXYZArrayType are identical */
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaWhitePointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = ABS_X;	/* Set some silly numbers */
			wo->data[0].Y = ABS_Y;
			wo->data[0].Z = ABS_Z;
		}
		/* Black Point Tag: */
		{
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaBlackPointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = 0.02;			/* Doesn't take part in Absolute anymore */
			wo->data[0].Y = 0.04;
			wo->data[0].Z = 0.03;
		}
		{
			/* Intent 1 = relative colorimetric */
			int nsigs = 1;
			icmXformSigs sigs[2] = { { icmSigShaperMatrix, icmSigShaperMatrixType} };

			if (wr_icco->create_matrix_xforms(wr_icco, ICM_CREATE_FLAG_NONE, NULL,
					nsigs, sigs,		/* Tables to be set */
					256, 256, 			/* Table resolution */
					icSigRgbData, 		/* Input color space */
					icSigXYZData, 		/* Output color space */
					RGB_RGBp,			/* RGB table function */
					matrix,				/* Matrix */
					NULL,				/* No matrix constant */
					0, NULL, 0			/* No special TRC flags */
			) != 0)
				error("Setting matrix failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
	
		/* Write the file out */
		if ((rv = wr_icco->write(wr_icco,wr_fp,0)) != 0)
			error ("Write file: %d, %s",rv,wr_icco->e.m);
		
		wr_icco->del(wr_icco);
		wr_fp->del(wr_fp);
	}

	/* - - - - - - - - - - - - - - */
	/* Deal with reading and verifying the Matrix based profile */

	if (!wonly) {
		icm_err_clear_e(&e);

		/* Open up the file for reading */
		if ((rd_fp = new_icmFileStd_name(&e, file_name,"r")) == NULL)
			error ("Read: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((rd_icco = new_icc(&e)) == NULL)
			error ("Read: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			rd_icco->warning = Warning;

		/* Read the header and tag list */
		if ((rv = rd_icco->read(rd_icco,rd_fp,0)) != 0)
			error ("Read: %d, %s",rv,rd_icco->e.m);
	
		if (check_parts(file_name, rd_icco)) {
			fail = 1;
		}

		/* Check the forward lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> XYZ transform */
						RGB_XYZp(NULL, check, in);
		
						/* Do lookup of device -> XYZ transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(out, check);
						if (mxd > 0.00005) {
							warning ("######## Excessive error in Matrix%s Fwd %f > 0.00005 ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							break;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
			printf("Matrix%s fwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the reverse lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> XYZ */
						RGB_XYZp(NULL, check, in);
		
						/* Do reverse lookup of device -> XYZ transform */
						if ((rv = luo->lookup_fwd(luo, out, check)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(in, out);
						if (mxd > 0.0002) {
							warning ("######## Excessive error in Matrix%s Bwd %f > 0.0002 ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							break;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
			printf("Matrix%s bwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the forward absolute lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> abs XYZ transform */
						aRGB_XYZp(NULL, check, in);
		
						/* Do lookup of device -> XYZ transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(out, check);
						if (mxd > 0.00005) {
							warning ("######## Excessive error in Abs Matrix%s Fwd %f > 0.00005 ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							break;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
			printf("Matrix%s fwd absolute intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the reverse absolute lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> abs XYZ */
						aRGB_XYZp(NULL, check, in);
		
						/* Do reverse lookup of device -> XYZ transform */
						if ((rv = luo->lookup_fwd(luo, out, check)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(in, out);
						if (mxd > 0.001) {
							warning ("######## Excessive error in Abs Matrix%s Bwd %f > 0.001 ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							break;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
			printf("Matrix%s bwd absolute intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		rd_icco->del(rd_icco);
		rd_fp->del(rd_fp);
	}



	/* ---------------------------------------- */
	/* Create a Lut16 based XYZ profile to test    */
	/* ---------------------------------------- */

	/* Open up the file for writing */
	file_name = "xxxx_lut16_XYZ.icm";
	tpfx = "";

	if (!ronly) {
		icm_err_clear_e(&e);

		if ((wr_fp = new_icmFileStd_name(&e, file_name,"w")) == NULL)
			error ("Write: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((wr_icco = new_icc(&e)) == NULL)
			error ("Write: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);

		if (warn)
			wr_icco->warning = Warning;

#ifdef IGNORE_WRITE_FORMAT_ERRORS
		wr_icco->set_cflag(wr_icco, icmCFlagWrFormatWarn);
#endif

		/* Add all the tags required */
	
		/* The header: */
		{
			icmHeader *wh = wr_icco->header;
	
			/* Values that must be set before writing */
			wh->deviceClass     = icSigOutputClass;
	    	wh->colorSpace      = icSigRgbData;			/* It's and RGBish space */
	    	wh->pcs             = icSigXYZData;
	    	wh->renderingIntent = icRelativeColorimetric;	/* For want of something */
	
			/* Values that should be set before writing */
			wh->manufacturer = icmstr2tag("tst2");
	    	wh->model        = icmstr2tag("test");
		}
		/* Profile Description Tag: */
		{
			icmCommonTextDescription *wo;
			char *dst = "This is a test Lut XYZ style Output Profile";
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
			char *crt = "Copyright 1998 Graeme Gill";
			if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigCopyrightTag,	icmSigCommonTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
	
			wo->count = strlen(crt)+1; 	/* Allocated and used size of text, inc null */
			wo->allocate(wo);/* Allocate space */
			strcpy(wo->desc, crt);		/* Copy the text in */
		}
		/* White Point Tag: */
		{
			icmXYZArray *wo;
			/* Note that tag types icSigXYZType and icSigXYZArrayType are identical */
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaWhitePointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = ABS_X;	/* Set some silly numbers */
			wo->data[0].Y = ABS_Y;
			wo->data[0].Z = ABS_Z;
		}
		/* Black Point Tag: */
		{
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaBlackPointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = 0.02;			/* Doesn't take part in Absolute anymore */
			wo->data[0].Y = 0.04;
			wo->data[0].Z = 0.03;
		}
	
		/* 16 bit dev -> pcs lut: */
		{
			double xyzmin[3] = {0.0, 0.0, 0.0};
			double xyzmax[3] = {1.0, 1.0, 1.0};			/* Override default XYZ max of 1.999969482422 */

			/* Intent 1 = relative colorimetric */
			int nsigs = 1;
			icmXformSigs sigs[2] = { { icSigAToB1Tag, icSigLut16Type} };
			unsigned int clutres[3] = { 33, 33, 33 };

			/* Create A2B table */
			if (wr_icco->create_lut_xforms(wr_icco,
#ifdef TEST_APXLS
					ICM_CLUT_SET_APXLS,
#else
					ICM_CLUT_SET_EXACT,
#endif
					NULL,
					nsigs, sigs,		/* Tables to be set */
					2, 256, clutres, 4096, 	/* Table resolutions */
					icSigRgbData, 		/* Input color space */
					icSigXYZData, 		/* Output color space */
					NULL, NULL,			/* input range not applicable */
					RGB_RGBp,			/* Input transfer function, RGB->RGB' (NULL = default) */
					NULL, NULL,			/* Use default Maximum range of RGB' values */
					RGBp_XYZp,			/* RGB' -> XYZ' transfer function */
					xyzmin, xyzmax,		/* Make XYZ' range 0.0 - 1.0 for better precision */
					XYZp_XYZ,			/* Output transfer function, XYZ'->XYZ (NULL = deflt) */
					NULL, NULL			/* No apxls range */
			) != 0)
				error("Setting 16 bit RGB->XYZ Lut%s failed: %d, %s",tpfx,wr_icco->e.c,wr_icco->e.m);

		}
		/* 16 bit dev -> pcs lut - link intent 0 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 0 = perceptual */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigAToB0Tag,	icSigAToB1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 16 dev -> pcs bit lut - link intent 2 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 2 = saturation */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigAToB2Tag,	icSigAToB1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 16 bit pcs -> dev lut: */
		{
			double xyzmin[3] = {0.0, 0.0, 0.0};			/* XYZ' range */
			double xyzmax[3] = {1.1, 1.1, 1.1};			/* Override default XYZ max of 1.999969482422 */
			double rgbmin[3] = {0.0, 0.0, 0.0};			/* RGB' range */
			double rgbmax[3] = {1.0, 1.0, 1.0};
			/* Intent 1 = relative colorimetric */
			int nsigs = 1;
			icmXformSigs sigs[2] = { { icSigBToA1Tag, icSigLut16Type} };
			unsigned int clutres[3] = { 33, 33, 33 };

#ifdef REVLUTSCALE1
			{
			/* In any any real profile, you will probably be providing a clut    */
			/* function that carefully maps out of gamut PCS values to in-gamut  */
			/* device values, so the scaling done here won't be appropriate.     */ 
			/*                                                                   */
			/* For this regresion test, we are interested in maximizing accuracy */
			/* over the known gamut of the device. It is an advantage therefore  */
			/* to scale the internal lut values to this end.                     */
	
			/* We'll do a really simple sampling search of the device gamut to   */
			/* establish the XYZ' bounding box.                                  */
	
			int co[3];
			double in[3], out[3];
	
			xyzmin[0] = xyzmin[1] = xyzmin[2] = 1.0;
			xyzmax[0] = xyzmax[1] = xyzmax[2] = 0.0;
			for (co[0] = 0; co[0] < 11; co[0]++) {
				in[0] = co[0]/(11-1.0);
				for (co[1] = 0; co[1] < 11; co[1]++) {
					in[1] = co[1]/(11-1.0);
					for (co[2] = 0; co[2] < 11; co[2]++) {
						in[2] = co[2]/(11-1.0);
		
						/* Do RGB -> XYZ' transform */
						RGB_XYZp(NULL, out, in);
						if (out[0] < xyzmin[0])
							xyzmin[0] = out[0];
						if (out[0] > xyzmax[0])
							xyzmax[0] = out[0];
						if (out[1] < xyzmin[1])
							xyzmin[1] = out[1];
						if (out[1] > xyzmax[1])
							xyzmax[1] = out[1];
						if (out[2] < xyzmin[2])
							xyzmin[2] = out[2];
						if (out[2] > xyzmax[2])
							xyzmax[2] = out[2];
					}
				}
			}
			xyzmax[0] *= 1.1;		/* Allow a slight margin */
			xyzmax[1] *= 1.1;
			xyzmax[2] *= 1.1;
			}
#endif	/* REVLUTSCALE1 */
	
#ifdef REVLUTSCALE2
			{
			/* In any any real profile, you will probably be providing a clut    */
			/* function that carefully maps out of gamut PCS values to in-gamut  */
			/* device values, so the scaling done here won't be appropriate.     */ 
			/*                                                                   */
			/* For this regresion test, we are interested in maximizing accuracy */
			/* over the known gamut of the device.                               */
			/* By setting the min/max to a larger range than will actually be    */
			/* used, we can make sure that the extreme table values of the       */
			/* clut are not actually used, and therefore we won't see the        */
			/* rounding effects of these extreme values being clipped            */
			/* by the numerical limits of the ICC representation.                */
			/* Instead the extreme values will be clipped by the the             */
			/* higher resolution output table.                                   */
			/*                                                                   */
			/* This all assumes that the multi-d reverse transform we are trying */
			/* to represent in the profile extrapolates beyond the legal device  */
			/* value range.                                                      */
			/*                                                                   */
			/* The scaling was chosen by experiment to make sure that the full   */
			/* gamut is surrounded by one row of extrapolated, unclipped clut    */
			/* table entries.                                                    */
		
			int i;
			for (i = 0; i < 3; i++) {
				rgbmin[i] = -0.1667;	/* Magic numbers */
				rgbmax[i] =  1.1667;
			}
			}
#endif	/* REVLUTSCALE2 */

			/* (We're not testing the matrix here...) */

			/* Create B2A table */
			if (wr_icco->create_lut_xforms(wr_icco,
#ifdef TEST_APXLS
					ICM_CLUT_SET_APXLS,
#else
					ICM_CLUT_SET_EXACT,
#endif
					NULL,
					nsigs, sigs,		/* Table to be set */
					2, 1024, clutres, 4096, 	/* Table resolutions */
					icSigXYZData, 		/* Input color space */
					icSigRgbData, 		/* Output color space */
					NULL, NULL,			/* input range not applicable */
					XYZ_XYZp, 			/* Input transfer function, XYZ->XYZ' (NULL = default) */
					xyzmin, xyzmax,		/* Make XYZ' range 0.0 - 1.0 for better precision */
					XYZp_RGBp,			/* XYZ' -> RGB' transfer function */
					rgbmin, rgbmax,		/* Make RGB' range 0.0 - 1.333 for less clip rounding */
					RGBp_RGB,			/* Output transfer function, RGB'->RGB (NULL = deflt) */
					NULL, NULL			/* No apxls range */
			) != 0)
				error("Setting 16 bit XYZ->RGB Lut%s failed: %d, %s",tpfx,wr_icco->e.c,wr_icco->e.m);

		}
		/* 16 bit pcs -> dev lut - link intent 0 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 0 = perceptual */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigBToA0Tag,	icSigBToA1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 16 pcs -> dev bit lut - link intent 2 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 2 = saturation */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigBToA2Tag,	icSigBToA1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
	
		/* 16 bit XYZ pcs -> gamut lut: */
		{
			double xyzmin[3] = {0.0, 0.0, 0.0};			/* XYZ' range */
			double xyzmax[3] = {1.1, 1.1, 1.1};			/* Override default XYZ max of 1.999969482422 */
			icmXformSigs sigs[1] = { { icSigGamutTag, icSigLut16Type} };
			unsigned int clutres[3] = { 33, 33, 33 };

			/* (We're not testing the matrix here...) */

			/* Create Gamut table */
			if (wr_icco->create_lut_xforms(wr_icco,
					ICM_CLUT_SET_EXACT,
					NULL,
					1, sigs,			/* Table to be set */
					2, 256, clutres, 256, 	/* Table resolutions */
					icSigXYZData, 		/* Input color space */
					icSigGrayData, 		/* Output color space */
					NULL, NULL,			/* input range not applicable */
					XYZ_XYZp,			/* Input transfer function, XYZ->XYZ' (NULL = default) */
					xyzmin, xyzmax,		/* Make XYZ' range 0.0 - 1.0 for better precision */
					XYZp_BDIST,		/* XYZ' -> Boundary Distance transfer function */
					NULL, NULL,			/* Default range from clut to output table */
					BDIST_GAMMUT,		/* Boundary Distance -> Out of gamut distance */
					NULL, NULL			/* No apxls range */
			) != 0)
				error("Setting 16 bit XYZ->Gammut Lut%sLut failed: %d, %s",tpfx,wr_icco->e.c,wr_icco->e.m);

		}
	
		/* Write the file out */
		if ((rv = wr_icco->write(wr_icco,wr_fp,0)) != 0)
			error ("Write file: %d, %s",rv,wr_icco->e.m);
		
		wr_icco->del(wr_icco);
		wr_fp->del(wr_fp);
	}

	/* - - - - - - - - - - - - - - */
	/* Deal with reading and verifying the Lut XYZ style profile */

	if (!wonly) {
		icm_err_clear_e(&e);

		/* Open up the file for reading */
		if ((rd_fp = new_icmFileStd_name(&e, file_name,"r")) == NULL)
			error ("Read: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((rd_icco = new_icc(&e)) == NULL)
			error ("Read: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			rd_icco->warning = Warning;

		/* Read the header and tag list */
		if ((rv = rd_icco->read(rd_icco,rd_fp,0)) != 0)
			error ("Read: %d, %s",rv,rd_icco->e.m);
	
		if (check_parts(file_name, rd_icco)) {
			fail = 1;
		}

		/* Check the Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icRelativeColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> XYZ transform */
						RGB_XYZ(NULL, check, in);
		
						/* Do lookup of device -> XYZ transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(out, check);
						if (mxd > 0.00005) {
							warning ("######## Excessive error in XYZ Lut%sLut Fwd %f > 0.00005 ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							goto done2;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
		  done2:;
			printf("Lut%s XYZ fwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the reverse Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
			int co[3];
			double in[3], out[3], check[3];
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icRelativeColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of XYZ -> device transform */
						RGB_XYZ(NULL, check, in);
	
						/* Do reverse lookup of device -> XYZ transform */
						if ((rv = luo->lookup_fwd(luo, out, check)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
	
						/* Check the result */
						mxd = maxdiff(in, out);
						if (mxd > 0.002) {
							warning ("######## Excessive error in XYZ Lut%sLut Bwd %f > 0.002 ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							goto done3;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
		  done3:;
			printf("Lut%s XYZ bwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the Absolute Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> XYZ transform */
						aRGB_XYZ(NULL, check,in);
		
						/* Do lookup of device -> XYZ transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(out, check);
						if (mxd > 0.00005) {
							warning ("######## Excessive error in XYZ Abs Lut%sLut Fwd %f > 0.00005 ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							goto done8;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
		  done8:;
			printf("Lut%s XYZ fwd absolute intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the Absolute reverse Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of XYZ -> device transform */
						aRGB_XYZ(NULL, check,in);
		
						/* Do reverse lookup of device -> XYZ transform */
						if ((rv = luo->lookup_fwd(luo, out, check)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(in, out);
						if (mxd > 0.002) {
							warning ("######## Excessive error in XYZ Abs Lut%sLut Bwd %f > 0.002 ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							goto done9;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
		  done9:;
			printf("Lut%s XYZ bwd absolute intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the XYZ gamut function */
		{
			int ino,ono,iok,ook;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmGamut, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			ino = ono = iok = ook = 0;
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						int outgamut;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do gamut lookup of XYZ transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Do reference conversion of XYZ -> RGB */
						XYZ_RGB(NULL, check,in);
		
						/* Check the result */
						outgamut = 1;	/* assume on edge */
						if (check[0] < -0.01 || check[0] > 1.01
						 || check[1] < -0.01 || check[1] > 1.01
						 || check[2] < -0.01 || check[2] > 1.01)
							outgamut = 2;	/* Definitely out of gamut */
						if (check[0] > 0.01 && check[0] < 0.99
						 && check[1] > 0.01 && check[1] < 0.99
						 && check[2] > 0.01 && check[2] < 0.99)
							outgamut = 0;	/* Definitely in gamut */
	
						/* Keep record of agree/disagree */
						if (outgamut <= 1) {
							ino++;
							if (out[0] <= 0.01)
								iok++;
						} else {
							ono++;
							if (out[0] > 0.01)
								ook++;
						}
					}
				}
			}
			printf("Lut%s XYZ gamut check inside  correct = %f%%\n",tpfx,100.0 * iok/ino);
			printf("Lut%s XYZ gamut check outside correct = %f%%\n",tpfx,100.0 * ook/ono);
			printf("Lut%s XYZ gamut check total   correct = %f%%\n",tpfx,100.0 * (iok+ook)/(ino+ono));
			if (((double)iok/ino) < 0.99 || ((double)ook/ono) < 0.98) {
				warning ("######## Gamut XYZ lookup has excessive error ########");
				fail = 1;
			}
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		rd_icco->del(rd_icco);
		rd_fp->del(rd_fp);
	}

	/* ---------------------------------------- */
	/* Create a Lut16 based Lab profile to test    */
	/* ---------------------------------------- */

	/* Open up the file for writing */
	file_name = "xxxx_lut16_Lab.icm";
	tpfx = "";

	if (!ronly) {
		icm_err_clear_e(&e);

		if ((wr_fp = new_icmFileStd_name(&e, file_name,"w")) == NULL)
			error ("Write: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((wr_icco = new_icc(&e)) == NULL)
			error ("Write: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			wr_icco->warning = Warning;

#ifdef IGNORE_WRITE_FORMAT_ERRORS
		wr_icco->set_cflag(wr_icco, icmCFlagWrFormatWarn);
#endif

		/* Add all the tags required */
	
		/* The header: */
		{
			icmHeader *wh = wr_icco->header;
	
			/* Values that must be set before writing */
			wh->deviceClass     = icSigOutputClass;
	    	wh->colorSpace      = icSigRgbData;			/* It's and RGBish space */
	    	wh->pcs             = icSigLabData;
	    	wh->renderingIntent = icRelativeColorimetric;	/* For want of something */
	
			/* Values that should be set before writing */
			wh->manufacturer = icmstr2tag("tst2");
	    	wh->model        = icmstr2tag("test");
		}
		/* Profile Description Tag: */
		{
			icmCommonTextDescription *wo;
			char *dst = "This is a test Lut style Lab Output Profile";
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
			char *crt = "Copyright 1998 Graeme Gill";
			if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigCopyrightTag,	icmSigCommonTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
	
			wo->count = strlen(crt)+1; 	/* Allocated and used size of text, inc null */
			wo->allocate(wo);/* Allocate space */
			strcpy(wo->desc, crt);		/* Copy the text in */
		}
		/* White Point Tag: */
		{
			icmXYZArray *wo;
			/* Note that tag types icSigXYZType and icSigXYZArrayType are identical */
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaWhitePointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = ABS_X;	/* Set some silly numbers */
			wo->data[0].Y = ABS_Y;
			wo->data[0].Z = ABS_Z;
		}
		/* Black Point Tag: */
		{
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaBlackPointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = 0.02;			/* Doesn't take part in Absolute anymore */
			wo->data[0].Y = 0.04;
			wo->data[0].Z = 0.03;
		}
		/* 16 bit dev -> pcs lut: */
		{
			/* Intent 1 = relative colorimetric */
			int nsigs = 1;
			icmXformSigs sigs[2] = { { icSigAToB1Tag, icSigLut16Type} };
			unsigned int clutres[3] = { 33, 33, 33 };

			/* Create A2B table */
			if (wr_icco->create_lut_xforms(wr_icco, ICM_CLUT_SET_EXACT, NULL,
					nsigs, sigs,		/* Table to be set */
					2, 256, clutres, 256, 	/* Table resolutions */
					icSigRgbData, 		/* Input color space */
					icSigLabData, 		/* Output color space */
					NULL, NULL,			/* input range not applicable */
					RGB_RGBp,			/* Input transfer function, RGB->RGB' (NULL = default) */
					NULL, NULL,			/* Use default Maximum range of RGB' values */
					RGBp_Labp,			/* RGB' -> XYZ' transfer function */
					NULL, NULL,			/* Use default Maximum range of Lab' values */
					Labp_Lab,			/* Linear output transform Lab'->Lab */
					NULL, NULL			/* No apxls range */
			) != 0)
				error("Setting 16 bit RGB->Lab Lut%sLut failed: %d, %s",tpfx,wr_icco->e.c,wr_icco->e.m);
		}
		/* 16 bit dev -> pcs lut - link intent 0 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 0 = perceptual */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigAToB0Tag,	icSigAToB1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 16 dev -> pcs bit lut - link intent 2 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 2 = saturation */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigAToB2Tag,	icSigAToB1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 16 bit pcs -> dev lut: */
		{
			double rgbmin[3] = {0.0, 0.0, 0.0};								/* RGB' range */
			double rgbmax[3] = {1.0, 1.0, 1.0};
			/* Intent 1 = relative colorimetric */
			int nsigs = 1;
			icmXformSigs sigs[2] = { { icSigBToA1Tag, icSigLut16Type} };
			unsigned int clutres[3] = { 33, 33, 33 };
			/* REVLUTSCALE1 could be used here, but in this case it hardly */
			/* makes any difference.                                       */

#ifdef REVLUTSCALE2
			{
			/* In any any real profile, you will probably be providing a clut    */
			/* function that carefully maps out of gamut PCS values to in-gamut  */
			/* device values, so the scaling done here won't be appropriate.     */ 
			/*                                                                   */
			/* For this regresion test, we are interested in maximizing accuracy */
			/* over the known gamut of the device.                               */
			/* By setting the min/max to a larger range than will actually be    */
			/* used, we can make sure that the extreme table values of the       */
			/* clut are not actually used, and therefore we won't see the        */
			/* rounding effects of these extreme values being clipped to         */
			/* by the numerical limits of the ICC representation.                */
			/* Instead the extreme values will be clipped by the the higher      */
			/* higher resolution output table.                                   */
			/*                                                                   */
			/* This all assumes that the multi-d reverse transform we are trying */
			/* to represent in the profile extrapolates beyond the legal device  */
			/* value range.                                                      */
			/*                                                                   */
			/* The scaling was chosen by experiment to make sure that the full   */
			/* gamut is surrounded by one row of extrapolated, unclipped clut    */
			/* table entries.                                                    */
		
			int i;
			for (i = 0; i < 3; i++) {
				rgbmin[i] = -0.1667;	/* Magic numbers */
				rgbmax[i] =  1.1667;
			}
			}
#endif	/* REVLUTSCALE2 */


			/* Create B2A table */
			if (wr_icco->create_lut_xforms(wr_icco, ICM_CLUT_SET_EXACT, NULL,
					nsigs, sigs,		/* Table to be set */
					2, 256, clutres, 4096, 	/* Table resolutions */
					icSigLabData, 		/* Input color space */
					icSigRgbData, 		/* Output color space */
					NULL, NULL,			/* input range not applicable */
					Lab_Labp,			/* Linear input transform Lab->Lab' */
					NULL, NULL,			/* Use default Lab' range */
					Labp_RGBp,			/* Lab' -> RGB' transfer function */
					rgbmin, rgbmax,		/* Make RGB' range 0.0 - 1.333 for less clip rounding */
					RGBp_RGB,			/* Output transfer function, RGB'->RGB (NULL = deflt) */
					NULL, NULL
			) != 0)
				error("Setting 16 bit Lab->RGB Lut%sLut failed: %d, %s",tpfx,wr_icco->e.c,wr_icco->e.m);

		}
		/* 16 bit pcs -> dev lut - link intent 0 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 0 = perceptual */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigBToA0Tag,	icSigBToA1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 16 pcs -> dev bit lut - link intent 2 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 2 = saturation */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigBToA2Tag,	icSigBToA1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
	
		/* 16 bit pcs -> gamut lut: */
		{
			icmXformSigs sigs[1] = { { icSigGamutTag, icSigLut16Type} };
			unsigned int clutres[3] = { 33, 33, 33 };

			/* Create Gamut table */
			if (wr_icco->create_lut_xforms(wr_icco, ICM_CLUT_SET_EXACT, NULL,
					1, sigs,			/* One table to be set */
					2, 256, clutres, 256, 	/* Table resolutions */
					icSigLabData, 		/* Input color space */
					icSigGrayData, 		/* Output color space */
					NULL, NULL,			/* input range not applicable */
					Lab_Labp,			/* Linear input transform Lab->Lab' */
					NULL, NULL,			/* Default Lab' range */
					Labp_BDIST,		/* Lab' -> Boundary Distance transfer function */
					NULL, NULL,			/* Default range from clut to output table */
					BDIST_GAMMUT,		/* Boundary Distance -> Out of gamut distance */
					NULL, NULL
			) != 0)
				error("Setting 16 bit Lab->Gammut Lut%sLut failed: %d, %s",tpfx,wr_icco->e.c,wr_icco->e.m);
		}
	
		/* Write the file out */
		if ((rv = wr_icco->write(wr_icco,wr_fp,0)) != 0)
			error ("Write file: %d, %s",rv,wr_icco->e.m);
		
		wr_icco->del(wr_icco);
		wr_fp->del(wr_fp);
	}

	/* - - - - - - - - - - - - - - */
	/* Deal with reading and verifying the Lut Lab 16bit style profile */

	if (!wonly) {
		icm_err_clear_e(&e);

		/* Open up the file for reading */
		if ((rd_fp = new_icmFileStd_name(&e, file_name,"r")) == NULL)
			error ("Read: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((rd_icco = new_icc(&e)) == NULL)
			error ("Read: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			rd_icco->warning = Warning;

		/* Read the header and tag list */
		if ((rv = rd_icco->read(rd_icco,rd_fp,0)) != 0)
			error ("Read: %d, %s",rv,rd_icco->e.m);
	
		if (check_parts(file_name, rd_icco)) {
			fail = 1;
		}

		/* Check the Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icRelativeColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> Lab transform */
						RGB_Lab(NULL, check,in);
		
						/* Do lookup of device -> Lab transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(out, check);
						if (mxd > 1.0) {
							warning ("######## Excessive error in Lab16 Lut%sLut Fwd %f ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							break;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
			printf("Lut%s Lab16 fwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the reverse Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icRelativeColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> Lab */
						RGB_Lab(NULL, check, in);
		
						/* Do reverse lookup of device -> Lab transform */
						if ((rv = luo->lookup_fwd(luo, out, check)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(in, out);
						if (mxd > 0.02) {
							warning ("######## Excessive error in Lab16 Lut%sLut Bwd in %s -> %f > 0.02 ########",tpfx,icmPdv(3, check), mxd);
							fail = 1;
#ifdef STOPONERROR
							goto done4;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
		  done4:;
			printf("Lut%s Lab16 bwd default intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the Absolute Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> Lab transform */
						aRGB_Lab(NULL, check,in);
		
						/* Do lookup of device -> Lab transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(out, check);
						if (mxd > 1.0) {
							warning ("######## Excessive error in Abs Lab16 Lut%sLut Fwd %f > 1.0 ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							break;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
			printf("Lut%s Lab16 fwd absolute intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the Absolute reverse Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> Lab transform */
						aRGB_Lab(NULL, check,in);
		
						/* Do reverse lookup of device -> Lab transform */
						if ((rv = luo->lookup_fwd(luo, out, check)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(in, out);
						if (mxd > 0.02) {
							warning ("######## Excessive error in Abs Lab16 Lut%sLut Bwd %f > 0.02 ########",tpfx,mxd);
							fail = 1;
#ifdef STOPONERROR
							goto done5;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
		  done5:;
			printf("Lut%s Lab16 bwd absolute intent check complete, peak error = %f\n",tpfx,merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the Lab gamut function */
		{
			int ino,ono,iok,ook;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmGamut, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			ino = ono = iok = ook = 0;
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = (co[0]/(TRES-1.0)) * 100.0;					/* L */
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = ((co[1]/(TRES-1.0)) - 0.5) * 256.0;		/* a */
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						int outgamut;
						in[2] = ((co[2]/(TRES-1.0)) - 0.5) * 256.0;	/* b */
		
						/* Do gamut lookup of Lab transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Do reference conversion of Lab -> RGB */
						Lab_RGB(NULL, check,in);
		
						/* Check the result */
						outgamut = 1;	/* assume on edge */
						if (check[0] < -0.01 || check[0] > 1.01
						 || check[1] < -0.01 || check[1] > 1.01
						 || check[2] < -0.01 || check[2] > 1.01)
							outgamut = 2;	/* Definitely out of gamut */
						if (check[0] > 0.01 && check[0] < 0.99
						 && check[1] > 0.01 && check[1] < 0.99
						 && check[2] > 0.01 && check[2] < 0.99)
							outgamut = 0;	/* Definitely in gamut */
	
						/* Keep record of agree/disagree */
						if (outgamut <= 1) {
							ino++;
							if (out[0] <= 0.01)
								iok++;
						} else {
							ono++;
							if (out[0] > 0.01)
								ook++;
						}
					}
				}
			}
			printf("Lut%s Lab16 gamut check inside  correct = %f%%\n",tpfx,100.0 * iok/ino);
			printf("Lut%s Lab16 gamut check outside correct = %f%%\n",tpfx,100.0 * ook/ono);
			printf("Lut%s Lab16 gamut check total   correct = %f%%\n",tpfx,100.0 * (iok+ook)/(ino+ono));
			if (((double)iok/ino) < 0.98 || ((double)ook/ono) < 0.98) {
				warning ("######## Gamut Lab16 lookup has excessive error ########");
				fail = 1;
			}
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		rd_icco->del(rd_icco);
		rd_fp->del(rd_fp);
	}

	/* ---------------------------------------- */
	/* Create a Lut8 based Lab profile to test    */
	/* ---------------------------------------- */

	/* Open up the file for writing */
	file_name = "xxxx_lut8_Lab.icm";

	if (!ronly) {
		icm_err_clear_e(&e);

		if ((wr_fp = new_icmFileStd_name(&e, file_name,"w")) == NULL)
			error ("Write: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((wr_icco = new_icc(&e)) == NULL)
			error ("Write: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			wr_icco->warning = Warning;

#ifdef IGNORE_WRITE_FORMAT_ERRORS
		wr_icco->set_cflag(wr_icco, icmCFlagWrFormatWarn);
#endif

		/* Add all the tags required */
	
		/* The header: */
		{
			icmHeader *wh = wr_icco->header;
	
			/* Values that must be set before writing */
			wh->deviceClass     = icSigOutputClass;
	    	wh->colorSpace      = icSigRgbData;			/* It's and RGBish space */
	    	wh->pcs             = icSigLabData;
	    	wh->renderingIntent = icRelativeColorimetric;	/* For want of something */
	
			/* Values that should be set before writing */
			wh->manufacturer = icmstr2tag("tst2");
	    	wh->model        = icmstr2tag("test");
		}
		/* Profile Description Tag: */
		{
			icmCommonTextDescription *wo;
			char *dst = "This is a test Lut style Lab Output Profile";
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
			char *crt = "Copyright 1998 Graeme Gill";
			if ((wo = (icmCommonTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigCopyrightTag,	icmSigCommonTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
	
			wo->count = strlen(crt)+1; 	/* Allocated and used size of text, inc null */
			wo->allocate(wo);/* Allocate space */
			strcpy(wo->desc, crt);		/* Copy the text in */
		}
		/* White Point Tag: */
		{
			icmXYZArray *wo;
			/* Note that tag types icSigXYZType and icSigXYZArrayType are identical */
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaWhitePointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = ABS_X;	/* Set some silly numbers */
			wo->data[0].Y = ABS_Y;
			wo->data[0].Z = ABS_Z;
		}
		/* Black Point Tag: */
		{
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaBlackPointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
	
			wo->count = 1;
			wo->allocate(wo);	/* Allocate space */
			wo->data[0].X = 0.02;			/* Doesn't take part in Absolute anymore */
			wo->data[0].Y = 0.04;
			wo->data[0].Z = 0.03;
		}
		/* 8 bit dev -> pcs lut: */
		{
			icmXformSigs sigs[1] = { { icSigAToB1Tag, icSigLut8Type} };
			unsigned int clutres[3] = { 33, 33, 33 };

			/* Create A2B table */
			if (wr_icco->create_lut_xforms(wr_icco, ICM_CLUT_SET_EXACT, NULL,
					1, sigs,			/* One table to be set */
					2, 256, clutres, 256, 	/* Table resolutions */
					icSigRgbData, 		/* Input color space */
					icSigLabData, 		/* Output color space */
					NULL, NULL,			/* input range not applicable */
					RGB_RGBp,			/* Input transfer function, RGB->RGB' (NULL = default) */
					NULL, NULL,			/* Use default Maximum range of RGB' values */
					RGBp_Labp,			/* RGB' -> Lab' transfer function */
					NULL, NULL,			/* Use default Maximum range of Lab' values */
					Labp_Lab,			/* Linear output transform Lab'->Lab */
					NULL, NULL
			) != 0)
				error("Setting 8 bit RGB->Lab Lut failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 8 bit dev -> pcs lut - link intent 0 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 0 = perceptual */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigAToB0Tag,	icSigAToB1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 8 dev -> pcs bit lut - link intent 2 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 2 = saturation */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigAToB2Tag,	icSigAToB1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 8 bit pcs -> dev lut: */
		{
			double rgbmin[3] = {0.0, 0.0, 0.0};								/* RGB' range */
			double rgbmax[3] = {1.0, 1.0, 1.0};
			/* Intent 1 = relative colorimetric */
			icmXformSigs sigs[1] = { { icSigBToA1Tag, icSigLut8Type} };
			unsigned int clutres[3] = { 33, 33, 33 };

			/* REVLUTSCALE1 could be used here, but in this case it hardly */
			/* makes any difference.                                       */
	
#ifdef REVLUTSCALE2
			{
			/* In any any real profile, you will probably be providing a clut    */
			/* function that carefully maps out of gamut PCS values to in-gamut  */
			/* device values, so the scaling done here won't be appropriate.     */ 
			/*                                                                   */
			/* For this regresion test, we are interested in maximizing accuracy */
			/* over the known gamut of the device.                               */
			/* By setting the min/max to a larger range than will actually be    */
			/* used, we can make sure that the extreme table values of the       */
			/* clut are not actually used, and therefore we won't see the        */
			/* rounding effects of these extreme values being clipped to         */
			/* by the numerical limits of the ICC representation.                */
			/* Instead the extreme values will be clipped by the the higher      */
			/* higher resolution output table.                                   */
			/*                                                                   */
			/* This all assumes that the multi-d reverse transform we are trying */
			/* to represent in the profile extrapolates beyond the legal device  */
			/* value range.                                                      */
			/*                                                                   */
			/* The scaling was chosen by experiment to make sure that the full   */
			/* gamut is surrounded by one row of extrapolated, unclipped clut    */
			/* table entries.                                                    */
		
			int i;
			for (i = 0; i < 3; i++) {
				rgbmin[i] = -0.1667;	/* Magic numbers */
				rgbmax[i] =  1.1667;
			}
			}
#endif /* REVLUTSCALE2 */

			/* Create B2A table */
			if (wr_icco->create_lut_xforms(wr_icco, ICM_CLUT_SET_EXACT, NULL,
					1, sigs,			/* One table to be set */
					2, 256, clutres, 256, 	/* Table resolutions */
					icSigLabData, 		/* Input color space */
					icSigRgbData, 		/* Output color space */
					NULL, NULL,			/* input range not applicable */
					Lab_Labp,			/* Linear input transform Lab->Lab' */
					NULL, NULL,			/* Use default Lab' range */
					Labp_RGBp,			/* Lab' -> RGB' transfer function */
					rgbmin, rgbmax,		/* Make RGB' range 0.0 - 1.333 for less clip rounding */
					RGBp_RGB,			/* Output transfer function, RGB'->RGB (NULL = deflt) */
					NULL, NULL
			) != 0)
				error("Setting 8 bit Lab->RGB Lut failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 8 bit pcs -> dev lut - link intent 0 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 0 = perceptual */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigBToA0Tag,	icSigBToA1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
		/* 8 pcs -> dev bit lut - link intent 2 to intent 1 */
		{
			icmLut1 *wo;
			/* Intent 2 = saturation */
			if ((wo = (icmLut1 *)wr_icco->link_tag(
			           wr_icco, icSigBToA2Tag,	icSigBToA1Tag)) == NULL) 
				error("link_tag failed: %d, %s",wr_icco->e.c,wr_icco->e.m);
		}
	
		/* 8 bit pcs -> gamut lut: */
		{
			icmXformSigs sigs[1] = { { icSigGamutTag, icSigLut8Type} };
			unsigned int clutres[3] = { 33, 33, 33 };

			/* Create Gamut table */
			if (wr_icco->create_lut_xforms(wr_icco, ICM_CLUT_SET_EXACT, NULL,
					1, sigs,			/* One table to be set */
					2, 256, clutres, 256, 	/* Table resolutions */
					icSigLabData, 		/* Input color space */
					icSigGrayData, 		/* Output color space */
					NULL, NULL,			/* input range not applicable */
					Lab_Labp,			/* Linear input transform Lab->Lab' */
					NULL, NULL	,		/* Default Lab' range */
					Labp_BDIST,		/* Lab' -> Boundary Distance transfer function */
					NULL, NULL,			/* Default range from clut to output table */
					BDIST_GAMMUT,		/* Boundary Distance -> Out of gamut distance */
					NULL, NULL
			) != 0)
				error("Setting 8 bit Lab->Gammut Lut failed: %d, %s",wr_icco->e.c,wr_icco->e.m);

		}
	
		/* Write the file out */
		if ((rv = wr_icco->write(wr_icco,wr_fp,0)) != 0)
			error ("Write file: %d, %s",rv,wr_icco->e.m);
		
		wr_icco->del(wr_icco);
		wr_fp->del(wr_fp);
	}

	/* - - - - - - - - - - - - - - */
	/* Deal with reading and verifying the Lut Lab 8bit style profile */

	if (!wonly) {
		icm_err_clear_e(&e);

		/* Open up the file for reading */
		if ((rd_fp = new_icmFileStd_name(&e, file_name,"r")) == NULL)
			error ("Read: Open file '%s' failed with 0x%x, '%s'",file_name, e.c, e.m);
	
		if ((rd_icco = new_icc(&e)) == NULL)
			error ("Read: Creation of ICC object failed with 0x%x, '%s'",e.c, e.m);
	
		if (warn)
			rd_icco->warning = Warning;

		/* Read the header and tag list */
		if ((rv = rd_icco->read(rd_icco,rd_fp,0)) != 0)
			error ("Read: %d, %s",rv,rd_icco->e.m);
	
		if (check_parts(file_name, rd_icco)) {
			fail = 1;
		}

		/* Check the Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icRelativeColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> Lab transform */
						RGB_Lab(NULL, check,in);
		
						/* Do lookup of device -> Lab transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(out, check);
						if (mxd > 2.1) {
							warning ("######## Excessive error in Lab8 Lut Fwd %f > 2.1 ########",mxd);
							fail = 1;
#ifdef STOPONERROR
							break;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
			printf("Lut Lab8 fwd default intent check complete, peak error = %f\n",merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the reverse Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icRelativeColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> Lab */
						RGB_Lab(NULL, check,in);
		
						/* Do reverse lookup of device -> Lab transform */
						if ((rv = luo->lookup_fwd(luo, out, check)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(in, out);
						if (mxd > 0.03) {
							warning ("######## Excessive error in Lab8 Lut Bwd %f > 0.03 ########",mxd);
							fail = 1;
#ifdef STOPONERROR
							goto done6;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
		  done6:;
			printf("Lut Lab8 bwd default intent check complete, peak error = %f\n",merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the Absolute Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmFwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> Lab transform */
						aRGB_Lab(NULL, check,in);
		
						/* Do lookup of device -> Lab transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(out, check);
						if (mxd > 2.3) {
							warning ("######## Excessive error in Abs Lab8 Lut Fwd %f > 2.3 ########",mxd);
							fail = 1;
#ifdef STOPONERROR
							break;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
			printf("Lut Lab8 fwd absolute intent check complete, peak error = %f\n",merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the Absolute reverse Lut lookup function */
		{
			double merr = 0.0;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmBwd, icAbsoluteColorimetric, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = co[0]/(TRES-1.0);
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = co[1]/(TRES-1.0);
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						double mxd;
						in[2] = co[2]/(TRES-1.0);
		
						/* Do reference conversion of device -> Lab transform */
						aRGB_Lab(NULL, check,in);
		
						/* Do reverse lookup of device -> Lab transform */
						if ((rv = luo->lookup_fwd(luo, out, check)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Check the result */
						mxd = maxdiff(in, out);
						if (mxd > 0.03) {
							warning ("######## Excessive error in Abs Lab8 Lut Bwd %f > 0.03 ########",mxd);
							fail = 1;
#ifdef STOPONERROR
							goto done7;
#endif /* STOPONERROR */
						}
						if (mxd > merr)
							merr = mxd;
					}
				}
			}
		  done7:;
			printf("Lut Lab8 bwd absolute intent check complete, peak error = %f\n",merr);
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		/* Check the Lab gamut function */
		{
			int ino,ono,iok,ook;
			icmLuSpace *luo;
	
			/* Get a fwd conversion object */
			if ((luo = (icmLuSpace *)rd_icco->get_luobj(rd_icco, icmGamut, icmDefaultIntent, 
			                              icmSigDefaultData, icmLuOrdNorm)) == NULL)
				error ("line %d, %d, %s",__LINE__,rd_icco->e.c, rd_icco->e.m);
	
			ino = ono = iok = ook = 0;
			for (co[0] = 0; co[0] < TRES; co[0]++) {
				in[0] = (co[0]/(TRES-1.0)) * 100.0;					/* L */
				for (co[1] = 0; co[1] < TRES; co[1]++) {
					in[1] = ((co[1]/(TRES-1.0)) - 0.5) * 256.0;		/* a */
					for (co[2] = 0; co[2] < TRES; co[2]++) {
						int outgamut;
						in[2] = ((co[2]/(TRES-1.0)) - 0.5) * 256.0;	/* b */
		
						/* Do gamut lookup of Lab transform */
						if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
							error ("Lookup error %s",icmPe_lurv2str(rv));
		
						/* Do reference conversion of Lab -> RGB */
						Lab_RGB(NULL, check,in);
		
						/* Check the result */
						outgamut = 1;	/* assume on edge */
						if (check[0] < -0.01 || check[0] > 1.01
						 || check[1] < -0.01 || check[1] > 1.01
						 || check[2] < -0.01 || check[2] > 1.01)
							outgamut = 2;	/* Definitely out of gamut */
						if (check[0] > 0.01 && check[0] < 0.99
						 && check[1] > 0.01 && check[1] < 0.99
						 && check[2] > 0.01 && check[2] < 0.99)
							outgamut = 0;	/* Definitely in gamut */
	
						/* Keep record of agree/disagree */
						if (outgamut <= 1) {
							ino++;
							if (out[0] <= 0.01)
								iok++;
						} else {
							ono++;
							if (out[0] > 0.01)
								ook++;
						}
					}
				}
			}
			printf("Lut Lab8 gamut check inside  correct = %f%%\n",100.0 * iok/ino);
			printf("Lut Lab8 gamut check outside correct = %f%%\n",100.0 * ook/ono);
			printf("Lut Lab8 gamut check total   correct = %f%%\n",100.0 * (iok+ook)/(ino+ono));
			if (((double)iok/ino) < 0.98 || ((double)ook/ono) < 0.98) {
				warning ("######## Gamut Lab8 lookup has excessive error ########");
				fail = 1;
			}
	
			/* Done with lookup object */
			luo->del(luo);
		}
	
		rd_icco->del(rd_icco);
		rd_fp->del(rd_fp);
	}


	/* ---------------------------------------- */

	if (wonly)
		printf("Lookup test write OK\n");
	else {
		if (fail)
			printf("Lookup test completed but FAILED\n");
		else
			printf("Lookup test completed OK\n");
	}
	return 0;
}

/* ------------------------------------------------ */

/* Return nz if Pe contains anything other than per channel operations */
static int check_Pe_pch(icmPeContainer *p) {
	int i;

	for (i = 0; i < p->count; i++) {
		if (p->pe[i] == NULL
		 || p->pe[i]->attr.op == icmPeOp_NOP
		 || p->pe[i]->attr.op == icmPeOp_perch) {
			continue;
		}
		return 1;
	}
	return 0;
}

/* Return nz if Pe contains a non-normalizaing per channel operations */
/* before any non-per channel operations */
static int check_Pe_in_notpch(icmPeContainer *p) {
	int i;

//printf("~1 check in, count %d\n",p->count);
	for (i = 0; i < p->count; i++) {
		/* Ignore any NOPs or normalizaing per channel */
		if (p->pe[i] == NULL
		 || p->pe[i]->attr.op == icmPeOp_NOP
		 || (p->pe[i]->attr.op == icmPeOp_perch && p->pe[i]->attr.norm)) {
//printf("~1 Skipping %d\n",i);
			continue;
		}

		/* Error if we find a non-normalizing per channel */
		if (p->pe[i]->attr.op == icmPeOp_perch
		 && p->pe[i]->attr.norm == 0) {
//printf("~1 ix %d is '%s' and has op = %s and norm %d\n",i,icm2str(icmProcessingElementTag,p->pe[i]->etype),icmPe_Op2str(p->pe[i]->attr.op),p->pe[i]->attr.norm);
//printf("~1 found error %d\n",i);
			return 1;
		}

		/* Must be other than a per channel op, so stop looking */
//printf("~1 found non-perch %d\n",i);
		return 0;
	}
//printf("~1 found nothing %d\n",i);
	return 0;
}

/* Return nz if Pe contains a non-normalizaing per channel operations */
/* after any non-per channel operations */
static int check_Pe_out_notpch(icmPeContainer *p) {
	int i;

	for (i = p->count-1; i >= 0; i--) {
		/* Ignore any NOPs or normalizaing per channel */
		if (p->pe[i] == NULL
		 || p->pe[i]->attr.op == icmPeOp_NOP
		 || (p->pe[i]->attr.op == icmPeOp_perch && p->pe[i]->attr.norm))
			continue;

		/* Error if we find a non-normalizing per channel */
		if (p->pe[i]->attr.op == icmPeOp_perch
		 && p->pe[i]->attr.norm == 0) 
			return 1;

		/* Must be other than a per channel op, so stop looking */
		return 0;
	}
	return 0;
}

/* Check 3 and 5 part lookups */
/* Return nz if error */

static int check_parts(char *name, icc *icco) {
	icmErr e = { 0, { '\000'} };
	icmLuSpace *luo;
	int cfg;
	int fail = 0;

	/* For each lookup configuration */
	for (cfg = 0; cfg < 8; cfg++) {
		icRenderingIntent intent;
		icColorSpaceSignature pcsor;
		icmLookupFunc func;
		icmCSInfo ini, outi;
		icmCSInfo eini, eouti;
		double emin[MAX_CHAN], emax[MAX_CHAN];
		double in[MAX_CHAN], out1[MAX_CHAN], out3[MAX_CHAN], out5[MAX_CHAN];
		double inmin[MAX_CHAN], inmax[MAX_CHAN];
		double outmin[MAX_CHAN], outmax[MAX_CHAN];
		double min[MAX_CHAN], max[MAX_CHAN];
		char cfgstr[100];
		double err;
		int i;

		if (cfg & 1) {
			strcpy(cfgstr,"-ia");
			intent = icAbsoluteColorimetric;
		} else {
			strcpy(cfgstr,"-ir");
			intent = icRelativeColorimetric;
		}

		if (cfg & 2) {
			strcat(cfgstr," -px");
			pcsor = icSigXYZData;
		} else {
			strcat(cfgstr," -pl");
			pcsor = icSigLabData;
		}

		if (cfg & 4) {
			strcat(cfgstr," -fb");
			func = icmBwd;
		} else {
			strcat(cfgstr," -ff");
			func = icmFwd;
		}

		if ((luo = (icmLuSpace *)icco->get_luobj(icco, func, intent, pcsor, icmLuOrdNorm)) == NULL)
			error ("File '%s' '%s' get_luobj failed, %d, %s",name,cfgstr,icco->e.c, icco->e.m);

		luo->spaces(luo, &eini, &eouti, NULL, NULL, NULL, NULL, NULL, NULL, NULL); 
		luo->native_spaces(luo, &ini, &outi, NULL);

		/* Figure out a notional effective input range */
		if (eini.sig == icSigLabData) {
			emin[0] = 0.0, emax[0] = 100.0;
			emin[1] = -127.0, emax[1] = 127.0;
			emin[2] = -127.0, emax[2] = 127.0;
		} else if (eini.sig == icSigXYZData) {
			emin[0] = 0.0, emax[0] = 1.0;
			emin[1] = 0.0, emax[1] = 1.0;
			emin[2] = 0.0, emax[2] = 1.0;
		} else {
			for (i = 0; i < eini.nch; i++)
				emin[i] = 0.0, emax[i] = 1.0;
		}

		/* 1 - Check that the conversions agree with a simple smoke test */
		for (i = 0; i < eini.nch; i++)
			in[i] = 0.5 * (emin[i] + emax[i]);

		luo->lookup_fwd(luo, out1, in);

		luo->input_fwd(luo, out3, in);
		luo->core3_fwd(luo, out3, out3);
		luo->output_fwd(luo, out3, out3);
	
		if ((err = icmDiffN(out1, out3, eouti.nch)) > 1e-6) {
			warning("################# file '%s' '%s': 3 part fwd mismatch 1 part by %f\n",name,cfgstr,err);
			fail = 1;
		}
	
		luo->input_fmt_fwd(luo, out5, in);
		luo->input_pch_fwd(luo, out5, out5);
		luo->core5_fwd(luo, out5, out5);
		luo->output_pch_fwd(luo, out5, out5);
		luo->output_fmt_fwd(luo, out5, out5);
		if ((err = icmDiffN(out1, out5, eouti.nch)) > 1e-6) {
			warning("################# file '%s' '%s': 5 part fwd mismatch 1 part by %f\n",name,cfgstr,err);
			fail = 1;
		}

		/* 2 - check that the per channel conversions contain only per channel Pe's */
		if (check_Pe_pch(luo->input)) {
			warning("################# file '%s' '%s': input Pe is not per-channel\n",name,cfgstr);
			fail = 1;
		}
		if (check_Pe_pch(luo->output)) {
			warning("################# file '%s' '%s': output Pe is not per-channel\n",name,cfgstr);
			fail = 1;
		}
		if (check_Pe_pch(luo->input_pch)) {
			warning("################# file '%s' '%s': input_pch Pe is not per-channel\n",name,cfgstr);
			fail = 1;
		}
		if (check_Pe_pch(luo->output_pch)) {
			warning("################# file '%s' '%s': output_pch Pe is not per-channel\n",name,cfgstr);
			fail = 1;
		}

		/* check that the non-per channel conversions don't contain any non-norm per channel ops */
		if (check_Pe_in_notpch(luo->core3)) {
			warning("################# file '%s' '%s': core3 Pe has in per-channel\n",name,cfgstr);
			fail = 1;
		}
		if (check_Pe_out_notpch(luo->core3)) {
			warning("################# file '%s' '%s': core3 Pe has out per-channel\n",name,cfgstr);
			fail = 1;
		}

		if (check_Pe_out_notpch(luo->input_fmt)) {
			warning("################# file '%s' '%s': input_fmt Pe has out per-channel\n",name,cfgstr);
			fail = 1;
		}
		if (check_Pe_in_notpch(luo->core5)) {
			warning("################# file '%s' '%s': core5 Pe has in per-channel\n",name,cfgstr);
			fail = 1;
		}
		if (check_Pe_out_notpch(luo->core5)) {
			warning("################# file '%s' '%s': core5 Pe has out per-channel\n",name,cfgstr);
			fail = 1;
		}
		if (check_Pe_in_notpch(luo->output_fmt)) {
			warning("################# file '%s' '%s': output_fmt Pe has in per-channel\n",name,cfgstr);
			fail = 1;
		}

		/* 3 - check that per channel transform range seems sane */

		/* Figure out a notional native input range */
		if (ini.sig == icSigLabData) {
			inmin[0] = 0.0,    inmax[0] = 100.0;
			inmin[1] = -127.0, inmax[1] = 127.0;
			inmin[2] = -127.0, inmax[2] = 127.0;
		} else if (ini.sig == icSigXYZData) {
			inmin[0] = 0.0, inmax[0] = 1.0;
			inmin[1] = 0.0, inmax[1] = 1.0;
			inmin[2] = 0.0, inmax[2] = 1.0;
		} else {
			for (i = 0; i < eini.nch; i++)
				inmin[i] = 0.0, inmax[i] = 1.0;
		}

		/* Figure out a notional native output range */
		if (outi.sig == icSigLabData) {
			outmin[0] = 0.0,    outmax[0] = 100.0;
			outmin[1] = -127.0, outmax[1] = 127.0;
			outmin[2] = -127.0, outmax[2] = 127.0;
		} else if (outi.sig == icSigXYZData) {
			outmin[0] = 0.0, outmax[0] = 1.0;
			outmin[1] = 0.0, outmax[1] = 1.0;
			outmin[2] = 0.0, outmax[2] = 1.0;
		} else {
			for (i = 0; i < eouti.nch; i++)
				outmin[i] = 0.0, outmax[i] = 1.0;
		}

		/* check 3 part pch in */
		luo->input_fwd(luo, min, inmin);
		luo->input_fwd(luo, max, inmax);
		for (i = 0; i < eini.nch; i++) {
			double rat = (max[i] - min[i])/(inmax[i] - inmin[i]);
			if (rat > 2.0 || rat < 0.45) {
				warning("################# file '%s' '%s': 3 part input %d range ratio %f\n",name,cfgstr,i,rat);
				fail = 1;
			}
		}

		/* check 3 part pch out */
		luo->output_fwd(luo, min, outmin);
		luo->output_fwd(luo, max, outmax);
		for (i = 0; i < eouti.nch; i++) {
			double rat = (max[i] - min[i])/(outmax[i] - outmin[i]);
			if (rat > 2.0 || rat < 0.45) {
				warning("################# file '%s' '%s': 3 part output %d range ratio %f\n",name,cfgstr,i,rat);
				fail = 1;
			}
		}

		/* check 5 part pch in */
		luo->input_pch_fwd(luo, min, inmin);
		luo->input_pch_fwd(luo, max, inmax);
		for (i = 0; i < eini.nch; i++) {
			double rat = (max[i] - min[i])/(inmax[i] - inmin[i]);
			if (rat > 2.0 || rat < 0.45) {
				warning("################# file '%s' '%s': 5 part input %d range ratio %f\n",name,cfgstr,i,rat);
				fail = 1;
			}
		}

		/* check 5 part pch out */
		luo->output_pch_fwd(luo, min, outmin);
		luo->output_pch_fwd(luo, max, outmax);
		for (i = 0; i < eouti.nch; i++) {
			double rat = (max[i] - min[i])/(outmax[i] - outmin[i]);
			if (rat > 2.0 || rat < 0.45) {
				warning("################# file '%s' '%s': 5 part output %d range ratio %f\n",name,cfgstr,i,rat);
				fail = 1;
			}
		}

		luo->del(luo);
	}

	return fail;
}


/* ------------------------------------------------ */
/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"lutest: Error - ");
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

	fprintf(stderr,"lutest: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}
