#ifndef ICC_UTIL_H
#define ICC_UTIL_H

/* 
 * International Color Consortium Format Library (icclib)
 * Utility functions used by icclib and other code.
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

/*
 * The distinction between "core" and "utility" functions is somewhat
 * arbitrary.
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

/* Set a 3 vector to the same value */
#define icmSet3(d_ary, s_val) ((d_ary)[0] = (s_val), (d_ary)[1] = (s_val), \
                              (d_ary)[2] = (s_val))

/* Set a 3 vector to 3 values */
#define icmInit3(d_ary, s1, s2, s3) ((d_ary)[0] = (s1), (d_ary)[1] = (s2), \
                              (d_ary)[2] = (s3))

/* Some 2 vector macros: */
#define icmSet2(d_ary, s_val) ((d_ary)[0] = (s_val), (d_ary)[1] = (s_val))

#define icmCpy2(d_ary, s_ary) ((d_ary)[0] = (s_ary)[0], (d_ary)[1] = (s_ary)[1])

#define icmAdd2(d_ary, s1_ary, s2_ary) ((d_ary)[0] = (s1_ary)[0] + (s2_ary)[0], \
                                        (d_ary)[1] = (s1_ary)[1] + (s2_ary)[1])

#define icmSub2(d_ary, s1_ary, s2_ary) ((d_ary)[0] = (s1_ary)[0] - (s2_ary)[0], \
                                        (d_ary)[1] = (s1_ary)[1] - (s2_ary)[1])

/* Copy a 3 vector */
#define icmCpy3(d_ary, s_ary) ((d_ary)[0] = (s_ary)[0], (d_ary)[1] = (s_ary)[1], \
                              (d_ary)[2] = (s_ary)[2])

/* Copy a 4 vector */
#define icmCpy4(d_ary, s_ary) ((d_ary)[0] = (s_ary)[0], (d_ary)[1] = (s_ary)[1], \
                              (d_ary)[2] = (s_ary)[2], (d_ary)[3] = (s_ary)[3])

/* - - - - - - - - - - - - - - */

/* Clamp a 3 vector to be +ve */
void icmClamp3(double out[3], double in[3]);

/* Invert (negate) a 3 vector */
void icmInv3(double out[3], double in[3]);

/* Add two 3 vectors */
void icmAdd3(double out[3], double in1[3], double in2[3]);

#define ICMADD3(o, i, j) ((o)[0] = (i)[0] + (j)[0], (o)[1] = (i)[1] + (j)[1], (o)[2] = (i)[2] + (j)[2])

/* Subtract two 3 vectors, out = in1 - in2 */
void icmSub3(double out[3], double in1[3], double in2[3]);

#define ICMSUB3(o, i, j) ((o)[0] = (i)[0] - (j)[0], (o)[1] = (i)[1] - (j)[1], (o)[2] = (i)[2] - (j)[2])

/* Divide two 3 vectors, out = in1/in2 */
void icmDiv3(double out[3], double in1[3], double in2[3]);

#define ICMDIV3(o, i, j) ((o)[0] = (i)[0]/(j)[0], (o)[1] = (i)[1]/(j)[1], (o)[2] = (i)[2]/(j)[2])

/* Multiply two 3 vectors, out = in1 * in2 */
void icmMul3(double out[3], double in1[3], double in2[3]);

#define ICMMUL3(o, i, j) ((o)[0] = (i)[0] * (j)[0], (o)[1] = (i)[1] * (j)[1], (o)[2] = (i)[2] * (j)[2])

/* Take (signed) values to power */
void icmPow3(double out[3], double in[3], double p);

/* Square values */
void icmSqr3(double out[3], double in[3]);

/* Square root of values */
void icmSqrt3(double out[3], double in[3]);

/* Take absolute of a 3 vector */
void icmAbs3(double out[3], double in[3]);

/* Compute sum of the vectors components */
#define icmSum3(vv) ((vv)[0] + (vv)[1] + (vv)[2]) 

/* Compute the dot product of two 3 vectors */
double icmDot3(double in1[3], double in2[3]);

#define ICMDOT3(o, i, j) ((o) = (i)[0] * (j)[0] + (i)[1] * (j)[1] + (i)[2] * (j)[2])

/* Compute the cross product of two 3D vectors, out = in1 x in2 */
void icmCross3(double out[3], double in1[3], double in2[3]);

/* Compute the norm squared (length squared) of a 3 vector */
double icmNorm3sq(double in[3]);

#define ICMNORM3SQ(i) ((i)[0] * (i)[0] + (i)[1] * (i)[1] + (i)[2] * (i)[2])

/* Compute the norm (length) of a 3 vector */
double icmNorm3(double in[3]);

#define ICMNORM3(i) sqrt((i)[0] * (i)[0] + (i)[1] * (i)[1] + (i)[2] * (i)[2])

/* Scale a 3 vector by the given ratio */
void icmScale3(double out[3], double in[3], double rat);

#define ICMSCALE3(o, i, j) ((o)[0] = (i)[0] * (j), (o)[1] = (i)[1] * (j), (o)[2] = (i)[2] * (j))

/* Scale a 3 vector by the given ratio and add it */
void icmScaleAdd3(double out[3], double in2[3], double in1[3], double rat);

/* Compute a blend between in0 and in1 for bl 0..1 */
void icmBlend3(double out[3], double in0[3], double in1[3], double bf);

/* Clip a vector to the range 0.0 .. 1.0 */
void icmClip3(double out[3], double in[3]);

/* Clip a vector to the range 0.0 .. 1.0 */
/* and return nz if clipping occured */
int icmClip3sig(double out[3], double in[3]);

/* Clip a vector to the range 0.0 .. 1.0 */
/* and return any clipping margine over the limits. */
double icmClip3marg(double out[3], double in[3]);

/* Normalise a 3 vector to the given length. Return nz if not normalisable */
int icmNormalize3(double out[3], double in[3], double len);

/* Compute the norm squared (length squared) of a vector defined by two points */
double icmNorm33sq(double in1[3], double in0[3]);

/* Compute the norm (length) of of a vector defined by two points */
double icmNorm33(double in1[3], double in0[3]);

/* Scale a vector from 0->1 by the given ratio. in0[] is the origin. */
void icmScale33(double out[3], double in1[3], double in0[3], double rat);

/* Normalise a vector from 0->1 to the given length. */
/* The new location of in1[] is returned in out[], in0[] is the origin. */
/* Return nz if not normalisable */
int icmNormalize33(double out[3], double in1[3], double in0[3], double len);

/* Dump a 3x3 matrix to a stream */
void icmDump3x3(FILE *fp, char *id, char *pfx, double a[3][3]);

/* Set a 3x3 matrix to a value */
void icmSetVal3x3(double mat[3][3], double val);

/* Set a 3x3 matrix to unity */
void icmSetUnity3x3(double mat[3][3]);

/* Copy a 3x3 transform matrix */
void icmCpy3x3(double out[3][3], double mat[3][3]);

/* Add a 3x3 transform matrix to another */
void icmAdd3x3(double dst[3][3], double src1[3][3], double src2[3][3]);

/* Scale each element of a 3x3 transform matrix */
void icmScale3x3(double dst[3][3], double src[3][3], double scale);

/* Multiply 3 vector by 3x3 transform matrix */
/* Organization is mat[out][in] */
void icmMulBy3x3(double out[3], double mat[3][3], double in[3]);

/* Tensor product. Multiply two 3 vectors to form a 3x3 matrix */
/* src1[] forms the colums, and src2[] forms the rows in the result */
void icmTensMul3(double dst[3][3], double src1[3], double src2[3]);

/* Multiply one 3x3 with another */
/* dst = src * dst */
void icmMul3x3(double dst[3][3], double src[3][3]);

/* Multiply one 3x3 with another #2 */
/* dst = src1 * src2 */
void icmMul3x3_2(double dst[3][3], double src1[3][3], double src2[3][3]);

/* Compute the determinant of a 3x3 matrix */
double icmDet3x3(double in[3][3]);

/* Invert a 3x3 transform matrix. Return 1 if error. */
int icmInverse3x3(double out[3][3], double in[3][3]);

/* Transpose a 3x3 matrix */
void icmTranspose3x3(double out[3][3], double in[3][3]);

/* Given two 3D points, create a matrix that rotates */
/* and scales one onto the other about the origin 0,0,0. */
/* Use icmMulBy3x3() to apply this to other points */
void icmRotMat(double mat[3][3], double src[3], double targ[3]);

/* Copy a 3x4 transform matrix */
void icmCpy3x4(double out[3][4], double mat[3][4]);

/* Multiply 3 array by 3x4 transform matrix */
void icmMul3By3x4(double out[3], double mat[3][4], double in[3]);

/* Given two 3D vectors, create a matrix that translates, */
/* rotates and scales one onto the other. */
/* Use icmMul3By3x4 to apply this to other points */
void icmVecRotMat(double m[3][4], double s1[3], double s0[3], double t1[3], double t0[3]);


/* Compute the intersection of a vector and a plane */
/* return nz if there is no intersection */
int icmVecPlaneIsect(double isect[3], double pl_const, double pl_norm[3], double ve_1[3], double ve_0[3]);

/* Compute the closest point on a line to a point. */
/* Return closest points and parameter values if not NULL. */
int icmLinePointClosest(double ca[3], double *pa,
                       double la0[3], double la1[3], double pp[3]);

/* Compute the closest points between two lines a and b. */
/* Return closest points and parameter values if not NULL. */
/* Return nz if they are parallel. */
int icmLineLineClosest(double ca[3], double cb[3], double *pa, double *pb,
                       double la0[3], double la1[3], double lb0[3], double lb1[3]);

/* Given 3 3D points, compute a plane equation. */
/* The normal will be right handed given the order of the points */
/* The plane equation will be the 3 normal components and the constant. */
/* Return nz if any points are cooincident or co-linear */
int icmPlaneEqn3(double eq[4], double p0[3], double p1[3], double p2[3]);

/* Given a 3D point and a plane equation, return the signed */
/* distance from the plane */
double icmPlaneDist3(double eq[4], double p[3]);

/* - - - - - - - - - - - - - - - - - - - - - - - */

/* Multiply 4 vector by 4x4 transform matrix */
/* Organization is mat[out][in] */
void icmMulBy4x4(double out[4], double mat[4][4], double in[4]);

/* Transpose a 4x4 matrix */
void icmTranspose4x4(double out[4][4], double in[4][4]);

/* Clip a vector to the range 0.0 .. 1.0 */
/* and return any clipping margine over the limit */
double icmClip4marg(double out[4], double in[4]);

/* - - - - - - - - - - - - - - - - - - - - - - - */

#define icmNorm2(i) sqrt((i)[0] * (i)[0] + (i)[1] * (i)[1])

#define icmNorm2sq(i) ((i)[0] * (i)[0] + (i)[1] * (i)[1])

/* Compute the norm (length) of of a vector defined by two points */
double icmNorm22(double in1[2], double in0[2]);

/* Compute the norm (length) squared of of a vector defined by two points */
double icmNorm22sq(double in1[2], double in0[2]);

/* Compute the dot product of two 2 vectors */
double icmDot2(double in1[2], double in2[2]);

#define ICMDOT2(o, i, j) ((o) = (i)[0] * (j)[0] + (i)[1] * (j)[1])

/* Compute the dot product of two 2 vectors defined by 4 points */
/* 1->2 and 3->4 */
double icmDot22(double in1[2], double in2[2], double in3[2], double in4[2]);

/* Normalise a 2 vector to the given length. Return nz if not normalisable */
int icmNormalize2(double out[2], double in[2], double len);

/* Return an orthogonal vector */
void icmOrthog2(double out[2], double in[2]);

/* Given 2 2D points, compute a plane equation. */
/* The normal will be right handed given the order of the points */
/* The plane equation will be the 2 normal components and the constant. */
/* Return nz if any points are cooincident or co-linear */
int icmPlaneEqn2(double eq[3], double p0[2], double p1[2]);

/* Given a 2D point and a plane equation, return the signed */
/* distance from the plane */
double icmPlaneDist2(double eq[3], double p[2]);

/* Return the closest point on an implicit line to a point. */
/* Also return the absolute distance */
double icmImpLinePointClosest2(double cp[2], double eq[3], double pp[2]);

/* Return the point of intersection of two implicit lines . */
/* Return nz if there is no intersection (lines are parallel) */
int icmImpLineIntersect2(double res[2], double eq1[3], double eq2[3]);


/* Compute the closest point on a line to a point. */
/* Return closest point and parameter value if not NULL. */
/* Return nz if the line length is zero */
int icmLinePointClosest2(double cp[2], double *pa,
                       double la0[2], double la1[2], double pp[2]);

/* Given two infinite 2D lines define by two pairs of points, compute the intersection. */
/* Return nz if there is no intersection (lines are parallel) */
int icmLineIntersect2(double res[2], double p1[2], double p2[2], double p3[2], double p4[2]);

/* Given two finite 2D lines define by 4 points, compute their paramaterized intersection. */
/* res[] and/or aprm[] may be NULL. Param is prop. from p1 -> p2, p3 -> p4 */
/* Return 2 if there is no intersection (lines are parallel) */
/* Return 1 lines do not cross within their length */
int icmParmLineIntersect2(double ares[2], double aprm[2], double p1[2], double p2[2], double p3[2], double p4[2]);

/* Set a 2x2 matrix to unity */
void icmSetUnity2x2(double mat[2][2]);

/* Invert a 2x2 transform matrix. Return 1 if error. */
int icmInverse2x2(double out[2][2], double in[2][2]);

/* Compute a blend between in0 and in1 for bl 0..1 */
void icmBlend2(double out[2], double in0[2], double in1[2], double bf);

/* Scale a 2 vector by the given ratio */
void icmScale2(double out[2], double in[2], double rat);

/* Scale a 2 vector by the given ratio and add it */
void icmScaleAdd2(double out[2], double in2[3], double in1[2], double rat);


/* Convert orientation 0 = right, 1 = down, 2 = left, 3 = right */
/* into rotation matrix */
static void icmOr2mat(double mat[2][2], int or);

/* Convert an angle in radians into rotation matrix (0 = right) */
void icmRad2mat(double mat[2][2], double rad);

/* Convert an angle in degrees (0 = right) into rotation matrix */
void icmDeg2mat(double mat[2][2], double a);

/* Convert a vector into a rotation matrix */
void icmVec2mat(double mat[2][2], double dx, double dy);

/* Multiply 2 array by 2x2 transform matrix */
void icmMulBy2x2(double out[2], double mat[2][2], double in[2]);

/* - - - - - - - - - - - - - - */

/* Set a vector to a single value */
void icmSetN(double *dst, double src, int len);

/* Copy a vector */
void icmCpyN(double *dst, double *src, int len);

/* Clip a vector to the range 0.0 .. 1.0 */
void icmClipN(double out[3], double in[3], unsigned int len);

/* Clip a vector to the range 0.0 .. 1.0 */
/* and return any clipping margine over the limit */
double icmClipNmarg(double *out, double *in, unsigned int len);

/* Return the magnitude (norm) of the difference between two vectors */
double icmDiffN(double *s1, double *s2, int len);

/* ----------------------------------------------- */

/* Structure to hold pseudo-hilbert counter info */
struct _psh {
	int      di;					/* Dimensionality */
	unsigned int res[MAX_CHAN];		/* Resolution per coordinate */
	unsigned int bits[MAX_CHAN];	/* Bits per coordinate */
	unsigned int xbits;				/* Max of bits[] */
	unsigned int tbits;				/* Total bits */
	unsigned int tmask;				/* Total 2^tbits count mask */
	unsigned int count;				/* Usable count */
	unsigned int ix;				/* Current binary index */
}; typedef struct _psh psh;

/* Initialise a pseudo-hilbert grid counter, return total usable count. */
extern ICCLIB_API unsigned psh_init(psh *p, int di, unsigned int res, int co[]);

/* Same as above but with variable res per axis. */
extern ICCLIB_API unsigned psh_initN(psh *p, int di, unsigned int res[], int co[]);

/* Reset the counter */
extern ICCLIB_API void psh_reset(psh *p);

/* Increment pseudo-hilbert coordinates */
/* Return non-zero if count rolls over to 0 */
extern ICCLIB_API int psh_inc(psh *p, int co[]);

/* - - - - - - - - - - - - - - */

/* Simple macro to transfer an array to an XYZ number */
#define icmAry2XYZ(xyz, ary) ((xyz).X = (ary)[0], (xyz).Y = (ary)[1], (xyz).Z = (ary)[2])

/* And the reverse */
#define icmXYZ2Ary(ary, xyz) ((ary)[0] = (xyz).X, (ary)[1] = (xyz).Y, (ary)[2] = (xyz).Z)

/* Simple macro to transfer an XYZ number to an XYZ number */
#define icmXYZ2XYZ(d_xyz, s_xyz) ((d_xyz).X = (s_xyz).X, (d_xyz).Y = (s_xyz).Y, \
                                  (d_xyz).Z = (s_xyz).Z)

/* Simple macro to transfer an 3array to 3array */
/* Hmm. Same as icmCpy3 */
#define icmAry2Ary(d_ary, s_ary) ((d_ary)[0] = (s_ary)[0], (d_ary)[1] = (s_ary)[1], \
                                  (d_ary)[2] = (s_ary)[2])

/* CIE Y (range 0 .. 1) to perceptual CIE 1976 L* (range 0 .. 100) */
double icmY2L(double val);

/* Perceptual CIE 1976 L* (range 0 .. 100) to CIE Y (range 0 .. 1) */
double icmL2Y(double val);

/* CIE XYZ to perceptual Lab */
extern ICCLIB_API void icmXYZ2Lab(icmXYZNumber *w, double *out, double *in);

/* Perceptual Lab to CIE XYZ */
extern ICCLIB_API void icmLab2XYZ(icmXYZNumber *w, double *out, double *in);

/* CIE XYZ to perceptual Lpt */
extern ICCLIB_API void icmXYZ2Lpt(icmXYZNumber *w, double *out, double *in);

/* Perceptual Lpt to CIE XYZ */
extern ICCLIB_API void icmLpt2XYZ(icmXYZNumber *w, double *out, double *in);

/* LCh to Lab (general) */
extern ICCLIB_API void icmLCh2Lab(double *out, double *in);

/* Lab to LCh (general) */
extern ICCLIB_API void icmLab2LCh(double *out, double *in);


/* CIE XYZ to perceptual Luv */
extern ICCLIB_API void icmXYZ2Luv(icmXYZNumber *w, double *out, double *in);

/* Perceptual Luv to CIE XYZ */
extern ICCLIB_API void icmLuv2XYZ(icmXYZNumber *w, double *out, double *in);


/* XYZ to Yxy */
extern ICCLIB_API void icmXYZ2Yxy(double *out, double *in);

/* Yxy to XYZ */
extern ICCLIB_API void icmYxy2XYZ(double *out, double *in);

/* XYZ to xy */
extern ICCLIB_API void icmXYZ2xy(double *out, double *in);

/* Y & xy to XYZ */
extern ICCLIB_API void icmY_xy2XYZ(double *out, double *xy, double Y);


/* CIE XYZ to perceptual CIE 1976 UCS diagram Yu'v'*/
/* (Yu'v' is a better linear chromaticity space than Yxy) */
extern ICCLIB_API void icmXYZ21976UCS(double *out, double *in);

/* Perceptual CIE 1976 UCS diagram Yu'v' to CIE XYZ */
extern ICCLIB_API void icm1976UCS2XYZ(double *out, double *in);

/* CIE XYZ to perceptual CIE 1976 UCS diagram u'v'*/
/* (u'v' is a better linear chromaticity space than xy) */
extern ICCLIB_API void icmXYZ21976UCSuv(double *out, double *in);

/* CIE 1976 UCS diagram Y & u'v' to XYZ */
extern ICCLIB_API void icm1976UCSY_uv2XYZ(double *out, double *uv, double Y);

/* Aliases for above */
#define icmXYZ2Yuv   icmXYZ21976UCS
#define icmYuv2XYZ   icm1976UCS2XYZ
#define icmXYZ2uv    icmXYZ21976UCSuv
#define icmY_uv2XYZ  icm1976UCSY_uv2XYZ


/* CIE XYZ to perceptual CIE 1960 UCS */
/* (This was obsoleted by the 1976UCS, but is still used */
/*  in computing color temperatures.) */
extern ICCLIB_API void icmXYZ21960UCS(double *out, double *in);

/* Perceptual CIE 1960 UCS to CIE XYZ */
extern ICCLIB_API void icm1960UCS2XYZ(double *out, double *in);


/* CIE XYZ to perceptual CIE 1964 WUV (U*V*W*) */
/* (This is obsolete but still used in computing CRI) */
extern ICCLIB_API void icmXYZ21964WUV(icmXYZNumber *w, double *out, double *in);

/* Perceptual CIE 1964 WUV (U*V*W*) to CIE XYZ */
extern ICCLIB_API void icm1964WUV2XYZ(icmXYZNumber *w, double *out, double *in);

/* CIE CIE1960 UCS to perceptual CIE 1964 WUV (U*V*W*) */
extern ICCLIB_API void icm1960UCS21964WUV(icmXYZNumber *w, double *out, double *in);


/* NOTE :- that these values are for the 1931 standard observer */

/* The standard D50 illuminant value */
extern icmXYZNumber icmD50;
extern icmXYZNumber icmD50_100;		/* Scaled to 100 */
extern double icmD50_ary3[3];		/* As an array */
extern double icmD50_100_ary3[3];	/* Scaled to 100 as an array */

/* The standard D65 illuminant value */
extern icmXYZNumber icmD65;
extern icmXYZNumber icmD65_100;		/* Scaled to 100 */
extern double icmD65_ary3[3];		/* As an array */
extern double icmD65_100_ary3[3];	/* Scaled to 100 as an array */


/* The default black value */
extern icmXYZNumber icmBlack;

/* The Standard ("wrong Von-Kries") chromatic transform matrix */
extern double icmWrongVonKries[3][3];

/* The Bradford chromatic transform matrix */
extern double icmBradford[3][3];

/* ----------------------------------------------- */

/* - - - - - - - - - - - - - - - - - - - - - - - */

/* Return the normal Delta E given two Lab values */
extern ICCLIB_API double icmLabDE(double *in0, double *in1);

/* Return the normal Delta E squared, given two Lab values */
extern ICCLIB_API double icmLabDEsq(double *in0, double *in1);

/* Return the normal Delta E squared given two XYZ values */
extern ICCLIB_API double icmXYZLabDEsq(icmXYZNumber *w, double *in0, double *in1);

/* Return the normal Delta E given two XYZ values */
extern ICCLIB_API double icmXYZLabDE(icmXYZNumber *w, double *in0, double *in1);

/* Return the normal Delta E squared given two XYZ values */
extern ICCLIB_API double icmXYZLptDEsq(icmXYZNumber *w, double *in0, double *in1);

/* Return the normal Delta E given two XYZ values */
extern ICCLIB_API double icmXYZLptDE(icmXYZNumber *w, double *in0, double *in1);


/* Return the CIE94 Delta E color difference measure for two Lab values */
extern ICCLIB_API double icmCIE94(double *in0, double *in1);

/* Return the CIE94 Delta E color difference measure squared, for two Lab values */
extern ICCLIB_API double icmCIE94sq(double *in0, double *in1);

/* Return the CIE94 Delta E color difference measure for two XYZ values */
extern ICCLIB_API double icmXYZCIE94(icmXYZNumber *w, double *in0, double *in1);


/* Return the CIEDE2000 Delta E color difference measure for two Lab values */
extern ICCLIB_API double icmCIE2K(double *in0, double *in1);

/* Return the CIEDE2000 Delta E color difference measure squared, for two Lab values */
extern ICCLIB_API double icmCIE2Ksq(double *in0, double *in1);

/* Return the CIEDE2000 Delta E color difference measure for two XYZ values */
extern ICCLIB_API double icmXYZCIE2K(icmXYZNumber *w, double *in0, double *in1);


/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Clip Lab, while maintaining hue angle. */
/* Return nz if clipping occured */
int icmClipLab(double out[3], double in[3]);

/* Clip XYZ, while maintaining hue angle */
/* Return nz if clipping occured */
int icmClipXYZ(double out[3], double in[3]);

/* - - - - - - - - - - - - - - - - - - - - - - - */

/* RGB XYZ primaries to device to RGB->XYZ transform matrix */
/* Return non-zero if matrix would be singular */
/* Use icmMulBy3x3(dst, mat, src) */
int icmRGBXYZprim2matrix(
	double red[3],		/* Red colorant */
	double green[3],	/* Green colorant */
	double blue[3],		/* Blue colorant */
	double white[3],	/* White point */
	double mat[3][3]	/* Destination matrix[RGB][XYZ] */
);

/* RGB Yxy primaries to device to RGB->XYZ transform matrix */
/* Return non-zero if matrix would be singular */
/* Use icmMulBy3x3(dst, mat, src) */
int icmRGBYxyprim2matrix(
	double red[3],		/* Red colorant */
	double green[3],	/* Green colorant */
	double blue[3],		/* Blue colorant */
	double white[3],	/* White point */
	double mat[3][3],	/* Return matrix[RGB][XYZ] */
	double wXYZ[3]		/* Return white XYZ */
);

/* Chromatic Adaption transform utility */
/* Return a 3x3 chromatic adaption matrix */
/* Use icmMulBy3x3(dst, mat, src) */
/* [ use icc->chromAdaptMatrix() to use the profiles cone space matrix rather */
/*   than specify a CAM ] */

#define ICM_CAM_NONE	    0x0000	/* No flags */
#define ICM_CAM_BRADFORD	0x0001	/* Use Bradford sharpened response space */
#define ICM_CAM_MULMATRIX	0x0002	/* Transform the given matrix rather than */
									/* create a transform from scratch. */
									/* NOTE that to transform primaries they */
									/* must be mat[XYZ][RGB] format! */

void icmChromAdaptMatrix(
	int flags,				/* Flags as defined below */
	icmXYZNumber d_wp,		/* Destination white point */
	icmXYZNumber s_wp,		/* Source white point */
	double mat[3][3]		/* Destination matrix */
);

/* Pre-round RGB device primary values to ensure that */
/* the sum of the quantized primaries is the same as */
/* the quantized sum. */
void quantizeRGBprimsS15Fixed16(
	double mat[3][3]		/* matrix[RGB][XYZ] */
);

/* Pre-round a 3x3 matrix to ensure that the product of */
/* the matrix and the input value is the same as */
/* the quantized matrix product. This is used to improve accuracy */
/* of 'chad' tag in computing absolute white point. */ 
void icmQuantize3x3S15Fixed16(
	double targ[3],			/* Target of product */
	double mat[3][3],		/* matrix[][] to be quantized */
	double in[3]			/* Input of product - must not be 0.0! */
);

/* - - - - - - - - - - - - - - */
/* Video functions */

/* Convert Lut table index/value to YCbCr */
void icmLut2YCbCr(double *out, double *in);

/* Convert YCbCr to Lut table index/value */
void icmYCbCr2Lut(double *out, double *in);


/* Convert Rec601 RGB' into YPbPr, (== "full range YCbCr") */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
void icmRec601_RGBd_2_YPbPr(double out[3], double in[3]);

/* Convert Rec601 YPbPr to RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
void icmRec601_YPbPr_2_RGBd(double out[3], double in[3]);


/* Convert Rec709 1150/60/2:1 RGB' into YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
void icmRec709_RGBd_2_YPbPr(double out[3], double in[3]);

/* Convert Rec709 1150/60/2:1 YPbPr to RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
void icmRec709_YPbPr_2_RGBd(double out[3], double in[3]);

/* Convert Rec709 1250/50/2:1 RGB' into YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
void icmRec709_50_RGBd_2_YPbPr(double out[3], double in[3]);

/* Convert Rec709 1250/50/2:1 YPbPr to RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
void icmRec709_50_YPbPr_2_RGBd(double out[3], double in[3]);


/* Convert Rec2020 RGB' into Non-constant liminance YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
void icmRec2020_NCL_RGBd_2_YPbPr(double out[3], double in[3]);

/* Convert Rec2020 Non-constant liminance YPbPr into RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
void icmRec2020_NCL_YPbPr_2_RGBd(double out[3], double in[3]);

/* Convert Rec2020 RGB' into Constant liminance YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
void icmRec2020_CL_RGBd_2_YPbPr(double out[3], double in[3]);

/* Convert Rec2020 Constant liminance YPbPr into RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
void icmRec2020_CL_YPbPr_2_RGBd(double out[3], double in[3]);


/* Convert Rec601/709/2020 YPbPr to YCbCr range. */
/* input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, */
/* output 16/255 .. 235/255, 16/255 .. 240/255, 16/255 .. 240/255 */ 
void icmRecXXX_YPbPr_2_YCbCr(double out[3], double in[3]);

/* Convert Rec601/709/2020 YCbCr to YPbPr range. */
/* input 16/255 .. 235/255, 16/255 .. 240/255, 16/255 .. 240/255 */ 
/* output 0..1, -0.5 .. 0.5, -0.5 .. 0.5, */
void icmRecXXX_YCbCr_2_YPbPr(double out[3], double in[3]);


/* Convert full range RGB to Video range 16..235 RGB */
void icmRGB_2_VidRGB(double out[3], double in[3]);

/* Convert Video range 16..235 RGB to full range RGB */
void icmVidRGB_2_RGB(double out[3], double in[3]);

/* ---------------------------------------------------------- */
/* PS 3.14-2009, Digital Imaging and Communications in Medicine */
/* (DICOM) Part 14: Grayscale Standard Display Function */

/* JND index value 1..1023 to L 0.05 .. 3993.404 cd/m^2 */
double icmDICOM_fwd(double jnd);

/* L 0.05 .. 3993.404 cd/m^2 to JND index value 1..1023 */
double icmDICOM_bwd(double L);


/* ---------------------------------------------------------- */
/* Some utility functions */

/* Convert an angle in radians into chromatic RGB values */
/* in a simple geometric fashion with 0 = Red */
void icmRad2RGB(double rgb[3], double ang);

/* ---------------------------------------------------------- */
/* UTF conversion functions. */
/* Derived from Unicode, Inc. code, Author: Mark E. Davis, 1994. */
/* see corresponding icc.c for full credits and copyright */

/* Error and warning codes from conversions */
typedef enum {
    icmUTF_ok            	= 0x00000,	/* ok */
    icmUTF_emb_nul			= 0x00001,	/* embedded nul */

									/* UTF-16 errors */
    icmUTF_no_nul          	= 0x00002,	/* no nul terminator */
    icmUTF_unex_nul 	    = 0x00004,	/* unexpected nul terminator */	
    icmUTF_prem_nul 	    = 0x00008,	/* nul before end */	
    icmUTF_bad_surr         = 0x00010,	/* badly formed surrogate pair */
    icmUTF_unn_bom          = 0x00020,	/* unecessary Byte Order Mark */
    icmUTF_odd_bytes        = 0x00040,	/* length is odd number of bytes */

									/* UTF-8 errors */
    icmUTF_unex_cont		= 0x00080,	/* unexpected continuation byte */
    icmUTF_tmany_cont		= 0x00100,	/* too many continuation bytes */
    icmUTF_short_cont		= 0x00200,	/* continuation bytes are cut short */
    icmUTF_overlong			= 0x00400,	/* overlong encoding */
    icmUTF_inv_cdpnt		= 0x00800,	/* invalid code point - UTF-16 surrogate */
    icmUTF_ovr_cdpnt		= 0x01000,	/* invalid code point - greater than unicode maximum */

									/* ASCII errors */
    icmUTF_non_ascii		= 0x02000,	/* found non-ascii character in UTF-8 */
	icmUTF_ascii_toolong	= 0x04000,	/* ASCII longer than fixed sized buffer */

									/* ScriptCode errors */
    icmUTF_sc_tooshort		= 0x08000,	/* ScriptCode size < 67 */
    icmUTF_sc_toolong		= 0x10000	/* ScriptCode size > 67 */
} icmUTFerr;


/* LEGACY serialisation */
size_t icmUTF16BEtoUTF8(icmUTFerr *pillegal, icmUTF8 *out, ORD8 *in, size_t len);
size_t icmUTF8toUTF16BE(icmUTFerr *pillegal, ORD8 *out, icmUTF8 *in, size_t len);

/* NEW serialisation */
size_t icmUTF16SntoUTF8(icmUTFerr *pillegal, icmUTF8 *out, icmFBuf *bin, size_t len, int nonul);
size_t icmUTF8toUTF16Sn(icmUTFerr *pillegal, icmFBuf *bout, icmUTF8 *in, size_t len, int nonul);

size_t icmASCIIZSntoUTF8(icmUTFerr *pillegal, icmUTF8 *out, icmFBuf *bin, size_t len, int fxlen);
size_t icmUTF8toASCIIZSn(icmUTFerr *pillegal, icmFBuf *bout, icmUTF8 *in, size_t len, int fxlen);

size_t icmSntoScriptCode(icmUTFerr *pillegal, icmUTF8 *out, icmFBuf *bin, size_t len);
size_t icmScriptCodetoSn(icmUTFerr *pillegal, icmFBuf *bout, ORD8 *in, size_t len);

/* General use */
size_t icmUTF16toUTF8(icmUTFerr *pillegal, icmUTF8 *out, icmUTF16 *in);
size_t icmUTF8toUTF16(icmUTFerr *pillegal, icmUTF16 *out, icmUTF8 *in);

/* Convert utf-8 to HTML escapes. illegal will have icmUTF_non_ascii set if any non-ASCII found. */
/* All output pointers may be NULL */
size_t icmUTF8toHTMLESC(icmUTFerr *pillegal, char *out, icmUTF8 *in);

/* Convert UTF-8 to ASCII. Out of range characters are replaced by '?'. */
/* illegal will have icmUTF_non_ascii set if any non-ASCII found. */
/* All output pointers may be NULL */
size_t icmUTF8toASCII(icmUTFerr *pillegal, char *out, icmUTF8 *in);

/* Convert icmUTFerr to a comma separated list of error descriptions. */
char *icmUTFerr2str(icmUTFerr err);

/* ---------------------------------------------------------- */
/* Print an int vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPiv(int di, int *p);

/* Print a double color vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPdv(int di, double *p);

/* Print a double color vector to a string with format. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPdvf(int di, char *fmt, double *p);

/* Print a float color vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPfv(int di, float *p);

/* Print an XYZ */
/* Returned static buffer is re-used every 5 calls. */
char *icmPXYZ(icmXYZNumber *p);

/* Print an 0..1 range XYZ as a D50 Lab string */
/* Returned static buffer is re-used every 5 calls. */
char *icmPLab(double *p);

/* ---------------------------------------------------------- */

/* Cause a debug break or stack trace */
#ifndef DEBUG_BREAK
# ifdef NT
#  define DEBUG_BREAK DebugBreak() 
# else
#  if defined(__APPLE__)
//    extern void Debugger();
//#   define DEBUG_BREAK Debugger() 	// deprecated ?
    //extern void __debugbreak();
//#  define DEBUG_BREAK __debugbreak()
#   define DEBUG_BREAK *((volatile char *)0) = 0x55;
#  else
#   include <signal.h>
#   if defined(SIGTRAP)
#    define DEBUG_BREAK raise(SIGTRAP)
#   else
#    define DEBUG_BREAK raise(SIGABRT)
#   endif
#  endif
# endif
#endif

#ifdef __cplusplus
	}
#endif

#endif /* ICC_UTIL_H */

