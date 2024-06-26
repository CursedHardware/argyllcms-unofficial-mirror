
/* 
 * International Color Consortium Format Library (icclib)
 * Utility functions used by icclib and other code.
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
 * The distinction between "core" and "utility" functions is somewhat
 * arbitrary. We do this just to make filesize a bit more managable.
 * This file is #included in icc.c.
 */

/* This file is #included in icc.c */

/*
 * TTBD:
 *
 */

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

/* Round a number to the same quantization as a S15Fixed16 */
static double round_S15Fixed16Number(double d) {
	d = floor(d * 65536.0 + 0.5);		/* Beware! (int)(d + 0.5) doesn't work for -ve nummbets ! */
	d = d/65536.0;
	return d;
}

/* Macro version */
#define RND_S15FIXED16(xxx) ((xxx) > 0.0 ? (int)((xxx) * 65536.0 + 0.5)/65536.0			\
                                         : (int)((xxx) * 65536.0 - 0.5)/65536.0)

/* Clamp a 3 vector to be +ve */
void icmClamp3(double out[3], double in[3]) {
	int i;
	for (i = 0; i < 3; i++)
		out[i] = in[i] < 0.0 ? 0.0 : in[i];
}

/* Invert (negate) a 3 vector */
void icmInv3(double out[3], double in[3]) {
	int i;
	for (i = 0; i < 3; i++)
		out[i] = -in[i];
}

/* Add two 3 vectors */
void icmAdd3(double out[3], double in1[3], double in2[3]) {
	out[0] = in1[0] + in2[0];
	out[1] = in1[1] + in2[1];
	out[2] = in1[2] + in2[2];
}

/* Subtract two 3 vectors, out = in1 - in2 */
void icmSub3(double out[3], double in1[3], double in2[3]) {
	out[0] = in1[0] - in2[0];
	out[1] = in1[1] - in2[1];
	out[2] = in1[2] - in2[2];
}

/* Divide two 3 vectors, out = in1/in2 */
void icmDiv3(double out[3], double in1[3], double in2[3]) {
	out[0] = in1[0]/in2[0];
	out[1] = in1[1]/in2[1];
	out[2] = in1[2]/in2[2];
}

/* Multiply two 3 vectors, out = in1 * in2 */
void icmMul3(double out[3], double in1[3], double in2[3]) {
	out[0] = in1[0] * in2[0];
	out[1] = in1[1] * in2[1];
	out[2] = in1[2] * in2[2];
}

/* Take (signed) values to power */
void icmPow3(double out[3], double in[3], double p) {
	int i;

	for (i = 0; i < 3; i++) {
		if (in[i] < 0.0)
			out[i] = -pow(-in[i], p);
		else
			out[i] = pow(in[i], p);
	}
}

/* Square values */
void icmSqr3(double out[3], double in[3]) {
	int i;

	for (i = 0; i < 3; i++)
		out[i] = in[i] * in[i];
}

/* Square root of values */
void icmSqrt3(double out[3], double in[3]) {
	int i;

	for (i = 0; i < 3; i++)
		out[i] = sqrt(in[i]);
}

/* Take absolute of a 3 vector */
void icmAbs3(double out[3], double in[3]) {
	out[0] = fabs(in[0]);
	out[1] = fabs(in[1]);
	out[2] = fabs(in[2]);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Dump a 3x3 matrix to a stream */
void icmDump3x3(FILE *fp, char *id, char *pfx, double a[3][3]) {
	int i, j;
	fprintf(fp, "%s%s[%d][%d]\n",pfx,id,3,3);

	for (j = 0; j < 3; j++) {
		fprintf(fp, "%s ",pfx);
		for (i = 0; i < 3; i++)
			fprintf(fp, "%f%s",a[j][i], i < (3-1) ? ", " : "");
		fprintf(fp, "\n");
	}
}

/* Set a 3x3 matrix to a value */
void icmSetVal3x3(double mat[3][3], double val) {
	int i, j;
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			mat[j][i] = val;
		}
	}
}

/* Set a 3x3 matrix to unity */
void icmSetUnity3x3(double mat[3][3]) {
	int i, j;
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			if (i == j)
				mat[j][i] = 1.0;
			else
				mat[j][i] = 0.0;
		}
	}
}

/* Copy a 3x3 transform matrix */
void icmCpy3x3(double dst[3][3], double src[3][3]) {
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			dst[j][i] = src[j][i];
	}
}

/* Add one 3x3 to another */
/* dst = src1 + src2 */
void icmAdd3x3(double dst[3][3], double src1[3][3], double src2[3][3]) {
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			dst[j][i] = src1[j][i] + src2[j][i];
	}
}

/* Scale each element of a 3x3 transform matrix */
void icmScale3x3(double dst[3][3], double src[3][3], double scale) {
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			dst[j][i] = src[j][i] * scale;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* 

  mat     in    out

[     ]   []    []
[     ] * [] => []
[     ]   []    []

So indexes are mat[out][in]

 */

/* Multiply 3 array by 3x3 transform matrix */
void icmMulBy3x3(double out[3], double mat[3][3], double in[3]) {
	double tt[3];

	tt[0] = mat[0][0] * in[0] + mat[0][1] * in[1] + mat[0][2] * in[2];
	tt[1] = mat[1][0] * in[0] + mat[1][1] * in[1] + mat[1][2] * in[2];
	tt[2] = mat[2][0] * in[0] + mat[2][1] * in[1] + mat[2][2] * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Tensor product. Multiply two 3 vectors to form a 3x3 matrix */
/* src1[] forms the colums, and src2[] forms the rows in the result */
void icmTensMul3(double dst[3][3], double src1[3], double src2[3]) {
	int i, j;
	
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			dst[j][i] = src1[j] * src2[i];
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Multiply one 3x3 with another */
/* dst = src * dst */
void icmMul3x3(double dst[3][3], double src[3][3]) {
	int i, j, k;
	double td[3][3];		/* Temporary dest */

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			double tt = 0.0;
			for (k = 0; k < 3; k++)
				tt += src[j][k] * dst[k][i];
			td[j][i] = tt;
		}
	}

	/* Copy result out */
	for (j = 0; j < 3; j++)
		for (i = 0; i < 3; i++)
			dst[j][i] = td[j][i];
}

/* Multiply one 3x3 with another #2 */
/* dst = src1 * src2 */
void icmMul3x3_2(double dst[3][3], double src1[3][3], double src2[3][3]) {
	int i, j, k;
	double td[3][3];		/* Temporary dest */

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			double tt = 0.0;
			for (k = 0; k < 3; k++)
				tt += src1[j][k] * src2[k][i];
			td[j][i] = tt;
		}
	}

	/* Copy result out */
	for (j = 0; j < 3; j++)
		for (i = 0; i < 3; i++)
			dst[j][i] = td[j][i];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* 
	Matrix Inversion
	by Richard Carling
	from "Graphics Gems", Academic Press, 1990
*/

/* 
 *   adjoint( original_matrix, inverse_matrix )
 * 
 *     calculate the adjoint of a 3x3 matrix
 *
 *      Let  a   denote the minor determinant of matrix A obtained by
 *           ij
 *
 *      deleting the ith row and jth column from A.
 *
 *                    i+j
 *     Let  b   = (-1)    a
 *          ij            ji
 *
 *    The matrix B = (b  ) is the adjoint of A
 *                     ij
 */

#define det2x2(a, b, c, d) (a * d - b * c)

static void adjoint(
double out[3][3],
double in[3][3]
) {
    double a1, a2, a3, b1, b2, b3, c1, c2, c3;

    /* assign to individual variable names to aid  */
    /* selecting correct values  */

	a1 = in[0][0]; b1 = in[0][1]; c1 = in[0][2];
	a2 = in[1][0]; b2 = in[1][1]; c2 = in[1][2];
	a3 = in[2][0]; b3 = in[2][1]; c3 = in[2][2];

    /* row column labeling reversed since we transpose rows & columns */

    out[0][0]  =   det2x2(b2, b3, c2, c3);
    out[1][0]  = - det2x2(a2, a3, c2, c3);
    out[2][0]  =   det2x2(a2, a3, b2, b3);
        
    out[0][1]  = - det2x2(b1, b3, c1, c3);
    out[1][1]  =   det2x2(a1, a3, c1, c3);
    out[2][1]  = - det2x2(a1, a3, b1, b3);
        
    out[0][2]  =   det2x2(b1, b2, c1, c2);
    out[1][2]  = - det2x2(a1, a2, c1, c2);
    out[2][2]  =   det2x2(a1, a2, b1, b2);
}

/*
 * double = icmDet3x3(  a1, a2, a3, b1, b2, b3, c1, c2, c3 )
 * 
 * calculate the determinant of a 3x3 matrix
 * in the form
 *
 *     | a1,  b1,  c1 |
 *     | a2,  b2,  c2 |
 *     | a3,  b3,  c3 |
 */

double icmDet3x3(double in[3][3]) {
    double a1, a2, a3, b1, b2, b3, c1, c2, c3;
    double ans;

	a1 = in[0][0]; b1 = in[0][1]; c1 = in[0][2];
	a2 = in[1][0]; b2 = in[1][1]; c2 = in[1][2];
	a3 = in[2][0]; b3 = in[2][1]; c3 = in[2][2];

    ans = a1 * det2x2(b2, b3, c2, c3)
        - b1 * det2x2(a2, a3, c2, c3)
        + c1 * det2x2(a2, a3, b2, b3);
    return ans;
}

/* 
 *   inverse( original_matrix, inverse_matrix )
 * 
 *    calculate the inverse of a 3x3 matrix
 *
 *     -1     
 *     A  = ___1__ adjoint A
 *         det A
 */

/* Return non-zero if not invertable */
int icmInverse3x3(
double out[3][3],
double in[3][3]
) {
    int i, j;
    double det;

    /*  calculate the 3x3 determinant
     *  if the determinant is zero, 
     *  then the inverse matrix is not unique.
     */
    det = icmDet3x3(in);

    if (fabs(det) < ICM_SMALL_NUMBER)
        return 1;

    /* calculate the adjoint matrix */
    adjoint(out, in);

    /* scale the adjoint matrix to get the inverse */
    for (i = 0; i < 3; i++)
        for(j = 0; j < 3; j++)
		    out[i][j] /= det;
	return 0;
}

/* Set a 2x2 matrix to unity */
void icmSetUnity2x2(double mat[2][2]) {
	int i, j;
	for (j = 0; j < 2; j++) {
		for (i = 0; i < 2; i++) {
			if (i == j)
				mat[j][i] = 1.0;
			else
				mat[j][i] = 0.0;
		}
	}
}

/* Invert a 2x2 transform matrix. Return 1 if error. */
int icmInverse2x2(double out[2][2], double in[2][2]) {
	double det = det2x2(in[0][0], in[0][1], in[1][0], in[1][1]);

    if (fabs(det) < ICM_SMALL_NUMBER)
        return 1;

	det = 1.0/det;

	out[0][0] = det * in[1][1];
	out[0][1] = det * -in[0][1];
	out[1][0] = det * -in[1][0];
	out[1][1] = det * in[0][0];

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Transpose a 3x3 matrix */
void icmTranspose3x3(double out[3][3], double in[3][3]) {
	int i, j;
	if (out != in) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				out[i][j] = in[j][i];
	} else {
		double tt[3][3];
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				tt[i][j] = in[j][i];
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				out[i][j] = tt[i][j];
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the dot product of two 3 vectors */
double icmDot3(double in1[3], double in2[3]) {
	return in1[0] * in2[0] + in1[1] * in2[1] + in1[2] * in2[2];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the cross product of two 3D vectors, out = in1 x in2 */
void icmCross3(double out[3], double in1[3], double in2[3]) {
	double tt[3];
    tt[0] = (in1[1] * in2[2]) - (in1[2] * in2[1]);
    tt[1] = (in1[2] * in2[0]) - (in1[0] * in2[2]);
    tt[2] = (in1[0] * in2[1]) - (in1[1] * in2[0]);
	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the norm (length) squared of a 3 vector */
double icmNorm3sq(double in[3]) {
	return in[0] * in[0] + in[1] * in[1] + in[2] * in[2];
}

/* Compute the norm (length) of a 3 vector */
double icmNorm3(double in[3]) {
	return sqrt(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
}

/* Scale a 3 vector by the given ratio */
void icmScale3(double out[3], double in[3], double rat) {
	out[0] = in[0] * rat;
	out[1] = in[1] * rat;
	out[2] = in[2] * rat;
}

/* Scale a 3 vector by the given ratio and add it */
void icmScaleAdd3(double out[3], double in2[3], double in1[3], double rat) {
	out[0] = in2[0] + in1[0] * rat;
	out[1] = in2[1] + in1[1] * rat;
	out[2] = in2[2] + in1[2] * rat;
}

/* Compute a blend between in0 and in1 */
void icmBlend3(double out[3], double in0[3], double in1[3], double bf) {
	out[0] = (1.0 - bf) * in0[0] + bf * in1[0];
	out[1] = (1.0 - bf) * in0[1] + bf * in1[1];
	out[2] = (1.0 - bf) * in0[2] + bf * in1[2];
}

/* Clip a vector to the range 0.0 .. 1.0 */
void icmClip3(double out[3], double in[3]) {
	int j;
	for (j = 0; j < 3; j++) {
		out[j] = in[j];
		if (out[j] < 0.0)
			out[j] = 0.0;
		else if (out[j] > 1.0)
			out[j] = 1.0;
	}
}

/* Clip a vector to the range 0.0 .. 1.0 */
/* and retun nz if clipping occured */
int icmClip3sig(double out[3], double in[3]) {
	int j;
	int clip = 0;
	for (j = 0; j < 3; j++) {
		out[j] = in[j];
		if (out[j] < 0.0) {
			out[j] = 0.0;
			clip = 1;
		} else if (out[j] > 1.0) {
			out[j] = 1.0;
			clip = 1;
		}
	}
	return clip;
}

/* Clip a vector to the range 0.0 .. 1.0 */
/* and return any clipping margine over the limits. */
double icmClip3marg(double out[3], double in[3]) {
	int j;
	double tt, marg = 0.0;
	for (j = 0; j < 3; j++) {
		out[j] = in[j];
		if (out[j] < 0.0) {
			tt = 0.0 - out[j];
			out[j] = 0.0;
			if (tt > marg)
				marg = tt;
		} else if (out[j] > 1.0) {
			tt = out[j] - 1.0;
			out[j] = 1.0;
			if (tt > marg)
				marg = tt;
		}
	}
	return marg;
}

/* Normalise a 3 vector to the given length. Return nz if not normalisable */
int icmNormalize3(double out[3], double in[3], double len) {
	double tt = sqrt(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
	
	if (tt < ICM_SMALL_NUMBER)
		return 1;
	tt = len/tt;
	out[0] = in[0] * tt;
	out[1] = in[1] * tt;
	out[2] = in[2] * tt;
	return 0;
}

/* Compute the norm (length) squared of a vector define by two points */
double icmNorm33sq(double in1[3], double in0[3]) {
	int j;
	double rv;
	for (rv = 0.0, j = 0; j < 3; j++) {
		double tt = in1[j] - in0[j];
		rv += tt * tt;
	}
	return rv;
}

/* Compute the norm (length) of a vector define by two points */
double icmNorm33(double in1[3], double in0[3]) {
	int j;
	double rv;
	for (rv = 0.0, j = 0; j < 3; j++) {
		double tt = in1[j] - in0[j];
		rv += tt * tt;
	}
	return sqrt(rv);
}

/* Scale a two point vector by the given ratio. in0[] is the origin */
void icmScale33(double out[3], double in1[3], double in0[3], double rat) {
	out[0] = in0[0] + (in1[0] - in0[0]) * rat;
	out[1] = in0[1] + (in1[1] - in0[1]) * rat;
	out[2] = in0[2] + (in1[2] - in0[2]) * rat;
}

/* Normalise a vector from 0->1 to the given length. */
/* The new location of in1[] is returned in out[]. */
/* Return nz if not normalisable */
int icmNormalize33(double out[3], double in1[3], double in0[3], double len) {
	int j;
	double vl;

	for (vl = 0.0, j = 0; j < 3; j++) {
		double tt = in1[j] - in0[j];
		vl += tt * tt;
	}
	vl = sqrt(vl);
	if (vl < ICM_SMALL_NUMBER)
		return 1;
	
	vl = len/vl;
	for (j = 0; j < 3; j++) {
		out[j] = in0[j] + (in1[j] - in0[j]) * vl;
	}
	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given two 3D points, create a matrix that rotates */
/* and scales one onto the other about the origin 0,0,0. */
/* The maths is from page 52 of Tomas Moller and Eric Haines "Real-Time Rendering". */
/* s is source vector, t is target vector. */
/* Usage of icmRotMat: */
/*	t[0] == mat[0][0] * s[0] + mat[0][1] * s[1] + mat[0][2] * s[2]; */
/*	t[1] == mat[1][0] * s[0] + mat[1][1] * s[1] + mat[1][2] * s[2]; */
/*	t[2] == mat[2][0] * s[0] + mat[2][1] * s[1] + mat[2][2] * s[2]; */
/* i.e. use icmMulBy3x3 */
void icmRotMat(double m[3][3], double s[3], double t[3]) {
	double sl, tl, sn[3], tn[3];
	double v[3];		/* Cross product */
	double e;			/* Dot product */
	double h;			/* 1-e/Cross product dot squared */

	/* Normalise input vectors */
	/* The rotation will be about 0,0,0 */
	sl = sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
	tl = sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);

	if (sl < 1e-12 || tl < 1e-12) {	/* Hmm. Do nothing */
		m[0][0] = 1.0;
		m[0][1] = 0.0;
		m[0][2] = 0.0;
		m[1][0] = 0.0;
		m[1][1] = 1.0;
		m[1][2] = 0.0;
		m[2][0] = 0.0;
		m[2][1] = 0.0;
		m[2][2] = 1.0;
		return;
	}

	sn[0] = s[0]/sl; sn[1] = s[1]/sl; sn[2] = s[2]/sl;
	tn[0] = t[0]/tl; tn[1] = t[1]/tl; tn[2] = t[2]/tl;

	/* Compute the cross product */
    v[0] = (sn[1] * tn[2]) - (sn[2] * tn[1]);
    v[1] = (sn[2] * tn[0]) - (sn[0] * tn[2]);
    v[2] = (sn[0] * tn[1]) - (sn[1] * tn[0]);

	/* Compute the dot product */
    e = sn[0] * tn[0] + sn[1] * tn[1] + sn[2] * tn[2];

	/* Cross product dot squared */
    h = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];

	/* If the two input vectors are close to being parallel, */
	/* then h will be close to zero. */
	if (fabs(h) < 1e-12) {

		/* Make sure scale is the correct sign */
		if (s[0] * t[0] + s[1] * t[1] + s[2] * t[2] < 0.0)
			tl = -tl;

		m[0][0] = tl/sl;
		m[0][1] = 0.0;
		m[0][2] = 0.0;
		m[1][0] = 0.0;
		m[1][1] = tl/sl;
		m[1][2] = 0.0;
		m[2][0] = 0.0;
		m[2][1] = 0.0;
		m[2][2] = tl/sl;
	} else {
		/* 1-e/Cross product dot squared */
	    h = (1.0 - e) / h;

		m[0][0] = tl/sl * (e + h * v[0] * v[0]);
		m[0][1] = tl/sl * (h * v[0] * v[1] - v[2]);
		m[0][2] = tl/sl * (h * v[0] * v[2] + v[1]);
		m[1][0] = tl/sl * (h * v[0] * v[1] + v[2]);
		m[1][1] = tl/sl * (e + h * v[1] * v[1]);
		m[1][2] = tl/sl * (h * v[1] * v[2] - v[0]);
		m[2][0] = tl/sl * (h * v[0] * v[2] - v[1]);
		m[2][1] = tl/sl * (h * v[1] * v[2] + v[0]);
		m[2][2] = tl/sl * (e + h * v[2] * v[2]);
	}

#ifdef NEVER		/* Check result */
	{
		double tt[3];

		icmMulBy3x3(tt, m, s);

		if (icmLabDEsq(t, tt) > 1e-4) {
			printf("icmRotMat error t, is %f %f %f\n",tt[0],tt[1],tt[2]);
			printf("            should be %f %f %f\n",t[0],t[1],t[2]);
		}
	}
#endif /* NEVER */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* 

  mat     in    out

[     ]   []    []
[     ] * [] => []
[     ]   []    []
[     ]   []    []

 */

/* Multiply 4 array by 4x4 transform matrix */
void icmMulBy4x4(double out[4], double mat[4][4], double in[4]) {
	double tt[4];

	tt[0] = mat[0][0] * in[0] + mat[0][1] * in[1] + mat[0][2] * in[2] + mat[0][3] * in[3];
	tt[1] = mat[1][0] * in[0] + mat[1][1] * in[1] + mat[1][2] * in[2] + mat[1][3] * in[3];
	tt[2] = mat[2][0] * in[0] + mat[2][1] * in[1] + mat[2][2] * in[2] + mat[2][3] * in[3];
	tt[3] = mat[3][0] * in[0] + mat[3][1] * in[1] + mat[3][2] * in[2] + mat[3][3] * in[3];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
	out[3] = tt[3];
}

/* Transpose a 4x4 matrix */
void icmTranspose4x4(double out[4][4], double in[4][4]) {
	int i, j;
	if (out != in) {
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				out[i][j] = in[j][i];
	} else {
		double tt[4][4];
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				tt[i][j] = in[j][i];
		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				out[i][j] = tt[i][j];
	}
}

/* Clip a vector to the range 0.0 .. 1.0 */
/* and return any clipping margine */
double icmClip4marg(double out[4], double in[4]) {
	int j;
	double tt, marg = 0.0;
	for (j = 0; j < 4; j++) {
		out[j] = in[j];
		if (out[j] < 0.0) {
			tt = 0.0 - out[j];
			out[j] = 0.0;
			if (tt > marg)
				marg = tt;
		} else if (out[j] > 1.0) {
			tt = out[j] - 1.0;
			out[j] = 1.0;
			if (tt > marg)
				marg = tt;
		}
	}
	return marg;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Copy a 3x4 transform matrix */
void icmCpy3x4(double dst[3][4], double src[3][4]) {
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 4; i++)
			dst[j][i] = src[j][i];
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Multiply 3 array by 3x4 transform matrix */
void icmMul3By3x4(double out[3], double mat[3][4], double in[3]) {
	double tt[3];

	tt[0] = mat[0][0] * in[0] + mat[0][1] * in[1] + mat[0][2] * in[2] + mat[0][3];
	tt[1] = mat[1][0] * in[0] + mat[1][1] * in[1] + mat[1][2] * in[2] + mat[1][3];
	tt[2] = mat[2][0] * in[0] + mat[2][1] * in[1] + mat[2][2] * in[2] + mat[2][3];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given two 3D vectors, create a matrix that translates, */
/* rotates and scales one onto the other. */
/* The maths is from page 52 of Tomas Moller and Eric Haines */
/* "Real-Time Rendering". */
/* s0 -> s1 is source vector, t0 -> t1 is target vector. */
/* Usage of icmRotMat: */
/*	t[0] = mat[0][0] * s[0] + mat[0][1] * s[1] + mat[0][2] * s[2] + mat[0][3]; */
/*	t[1] = mat[1][0] * s[0] + mat[1][1] * s[1] + mat[1][2] * s[2] + mat[1][3]; */
/*	t[2] = mat[2][0] * s[0] + mat[2][1] * s[1] + mat[2][2] * s[2] + mat[2][3]; */
/* i.e. use icmMul3By3x4 */
void icmVecRotMat(double m[3][4], double s1[3], double s0[3], double t1[3], double t0[3]) {
	int i, j;
	double ss[3], tt[3], rr[3][3];

	/* Create the rotation matrix: */
	for (i = 0; i < 3; i++) {
		ss[i] = s1[i] - s0[i];
		tt[i] = t1[i] - t0[i];
	}
	icmRotMat(rr, ss, tt);
	
	/* Create rotated version of s0: */
	icmMulBy3x3(ss, rr, s0);

	/* Create 4x4 matrix */
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 4; i++) {
			if (i < 3 && j < 3)
				m[j][i] = rr[j][i];
			else if (i == 3 && j < 3)
				m[j][i] = t0[j] - ss[j];
			else if (i == j)
				m[j][i] = 1.0;
			else
				m[j][i] = 0.0;
		}
	}

#ifdef NEVER		/* Check result */
	{
		double tt0[3], tt1[3];

		icmMul3By3x4(tt0, m, s0);

		if (icmLabDEsq(t0, tt0) > 1e-4) {
			printf("icmVecRotMat error t0, is %f %f %f\n",tt0[0],tt0[1],tt0[2]);
			printf("                should be %f %f %f\n",t0[0],t0[1],t0[2]);
		}

		icmMul3By3x4(tt1, m, s1);

		if (icmLabDEsq(t1, tt1) > 1e-4) {
			printf("icmVecRotMat error t1, is %f %f %f\n",tt1[0],tt1[1],tt1[2]);
			printf("                should be %f %f %f\n",t1[0],t1[1],t1[2]);
		}
	}
#endif /* NEVER */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the 3D intersection of a vector and a plane */
/* return nz if there is no intersection */
int icmVecPlaneIsect(
double isect[3],		/* return intersection point */
double pl_const,		/* Plane equation constant */
double pl_norm[3],		/* Plane normal vector */
double ve_1[3],			/* Point on line */
double ve_0[3]			/* Second point on line */
) {
	double den;			/* denominator */
	double rv;			/* Vector parameter value */
	double vvec[3];		/* Vector vector */
	double ival[3];		/* Intersection value */

	/* Compute vector between the two points */
	vvec[0] = ve_1[0] - ve_0[0];
	vvec[1] = ve_1[1] - ve_0[1];
	vvec[2] = ve_1[2] - ve_0[2];

	/* Compute the denominator */
	den = pl_norm[0] * vvec[0] + pl_norm[1] * vvec[1] + pl_norm[2] * vvec[2];

	/* Too small to intersect ? */
	if (fabs(den) < 1e-12) {
		return 1;
	}

	/* Compute the parameterized intersction point */
	rv = -(pl_norm[0] * ve_0[0] + pl_norm[1] * ve_0[1] + pl_norm[2] * ve_0[2] + pl_const)/den;

	/* Compute the actual intersection point */
	isect[0] = ve_0[0] + rv * vvec[0];
	isect[1] = ve_0[1] + rv * vvec[1];
	isect[2] = ve_0[2] + rv * vvec[2];

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the closest point on a line to a point. */
/* Return closest point and parameter value if not NULL. */
/* Return nz if the line length is zero */
int icmLinePointClosest(double cp[3], double *pa,
                       double la0[3], double la1[3], double pp[3]) {
	double va[3], vp[3];
	double val;				/* Vector length squared */
	double a;				/* Parameter value */

	icmSub3(va, la1, la0);	/* Line vector */
	val = icmNorm3sq(va);	/* Vector length squared */

	if (val < 1e-12)
		return 1;

	icmSub3(vp, pp, la0);	/* Point vector to line base */

	a = icmDot3(vp, va) / val;		/* Normalised dist of point projected onto line */

	if (cp != NULL)
		icmBlend3(cp, la0, la1, a);

	if (pa != NULL)
		*pa = a;

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the closest points between two lines a and b. */
/* Return closest points and parameter values if not NULL. */
/* Return nz if they are paralel. */
/* The maths is from page 338 of Tomas Moller and Eric Haines "Real-Time Rendering". */
int icmLineLineClosest(double ca[3], double cb[3], double *pa, double * pb,
                       double la0[3], double la1[3], double lb0[3], double lb1[3]) {
	double va[3], vb[3];
	double vvab[3], vvabns;		/* Cross product of va and vb and its norm squared */
	double vba0[3];				/* lb0 - la0 */
	double tt[3];
	double a, b;				/* Parameter values */

	icmSub3(va, la1, la0);
	icmSub3(vb, lb1, lb0);
	icmCross3(vvab, va, vb);
	vvabns = icmNorm3sq(vvab);

	if (vvabns < 1e-12)
		return 1;

	icmSub3(vba0, lb0, la0);
	icmCross3(tt, vba0, vb);
	a = icmDot3(tt, vvab) / vvabns;

	icmCross3(tt, vba0, va);
	b = icmDot3(tt, vvab) / vvabns;

	if (pa != NULL)
		*pa = a;

	if (pb != NULL)
		*pb = b;

	if (ca != NULL) {
		ca[0] = la0[0] + a * va[0];
		ca[1] = la0[1] + a * va[1];
		ca[2] = la0[2] + a * va[2];
	}

	if (cb != NULL) {
		cb[0] = lb0[0] + b * vb[0];
		cb[1] = lb0[1] + b * vb[1];
		cb[2] = lb0[2] + b * vb[2];
	}

#ifdef NEVER	/* Verify  */
	{
		double vab[3];		/* Vector from ca to cb */

		vab[0] = lb0[0] + b * vb[0] - la0[0] - a * va[0];
		vab[1] = lb0[1] + b * vb[1] - la0[1] - a * va[1];
		vab[2] = lb0[2] + b * vb[2] - la0[2] - a * va[2];
		
		if (icmDot3(va, vab) > 1e-6
		 || icmDot3(vb, vab) > 1e-6)
			warning("icmLineLineClosest verify failed\n");
	}
#endif

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given 3 3D points, compute a plane equation. */
/* The normal will be right handed given the order of the points */
/* The plane equation will be the 3 normal components and the constant. */
/* Return nz if any points are cooincident or co-linear */
int icmPlaneEqn3(double eq[4], double p0[3], double p1[3], double p2[3]) {
	double ll, v1[3], v2[3];

	/* Compute vectors along edges */
	v2[0] = p1[0] - p0[0];
	v2[1] = p1[1] - p0[1];
	v2[2] = p1[2] - p0[2];

	v1[0] = p2[0] - p0[0];
	v1[1] = p2[1] - p0[1];
	v1[2] = p2[2] - p0[2];

	/* Compute cross products v1 x v2, which will be the normal */
	eq[0] = v1[1] * v2[2] - v1[2] * v2[1];
	eq[1] = v1[2] * v2[0] - v1[0] * v2[2];
	eq[2] = v1[0] * v2[1] - v1[1] * v2[0];

	/* Normalise the equation */
	ll = sqrt(eq[0] * eq[0] + eq[1] * eq[1] + eq[2] * eq[2]);
	if (ll < 1e-10) {
		return 1;
	}
	eq[0] /= ll;
	eq[1] /= ll;
	eq[2] /= ll;

	/* Compute the plane equation constant */
	eq[3] = - (eq[0] * p0[0])
	        - (eq[1] * p0[1])
	        - (eq[2] * p0[2]);

	return 0;
}

/* Given a 3D point and a plane equation, return the signed */
/* distance from the plane */
double icmPlaneDist3(double eq[4], double p[3]) {
	double rv;

	rv = eq[0] * p[0]
	   + eq[1] * p[1]
	   + eq[2] * p[2]
	   + eq[3];

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Compute the norm (length) of a vector define by two points */
double icmNorm22(double in1[2], double in0[2]) {
	int j;
	double rv;
	for (rv = 0.0, j = 0; j < 2; j++) {
		double tt = in1[j] - in0[j];
		rv += tt * tt;
	}
	return sqrt(rv);
}

/* Compute the norm (length) squared of of a vector defined by two points */
double icmNorm22sq(double in1[2], double in0[2]) {
	int j;
	double rv;
	for (rv = 0.0, j = 0; j < 2; j++) {
		double tt = in1[j] - in0[j];
		rv += tt * tt;
	}
	return rv;
}

/* Compute the dot product of two 2 vectors */
double icmDot2(double in1[2], double in2[2]) {
	return in1[0] * in2[0] + in1[1] * in2[1];
}

/* Compute the dot product of two 2 vectors defined by 4 points */
/* 1->2 and 3->4 */
double icmDot22(double in1[2], double in2[2], double in3[2], double in4[2]) {
	return (in2[0] - in1[0]) * (in4[0] - in3[0])
	     + (in2[1] - in1[1]) * (in4[1] - in3[1]);
}

/* Normalise a 2 vector to the given length. Return nz if not normalisable */
int icmNormalize2(double out[2], double in[2], double len) {
	double tt = sqrt(in[0] * in[0] + in[1] * in[1]);
	
	if (tt < ICM_SMALL_NUMBER)
		return 1;
	tt = len/tt;
	out[0] = in[0] * tt;
	out[1] = in[1] * tt;
	return 0;
}

/* Return an orthogonal vector */
/* (90 degree rotate = swap and negate X ) */
void icmOrthog2(double out[2], double in[2]) {
	double tt;
	tt = in[0];
	out[0] = -in[1];
	out[1] = tt;
}

/* Given 2 2D points, compute a plane equation (implicit line equation). */
/* The normal will be right handed given the order of the points */
/* The plane equation will be the 2 normal components and the constant. */
/* Return nz if any points are cooincident or co-linear */
int icmPlaneEqn2(double eq[3], double p0[2], double p1[2]) {
	double ll, v1[3];

	/* Compute vectors along edge */
	v1[0] = p1[0] - p0[0];
	v1[1] = p1[1] - p0[1];

	/* Normal to vector */
	eq[0] =  v1[1];
	eq[1] = -v1[0];

	/* Normalise the equation */
	ll = sqrt(eq[0] * eq[0] + eq[1] * eq[1]);
	if (ll < 1e-10) {
		return 1;
	}
	eq[0] /= ll;
	eq[1] /= ll;

	/* Compute the plane equation constant */
	eq[2] = - (eq[0] * p0[0])
	        - (eq[1] * p0[1]);

	return 0;
}

/* Given a 2D point and a plane equation (implicit line), return the signed */
/* distance from the plane. The distance will be +ve if the point */
/* is to the right of the plane formed by two points in order */
double icmPlaneDist2(double eq[3], double p[2]) {
	double rv;

	rv = eq[0] * p[0]
	   + eq[1] * p[1]
	   + eq[2];

	return rv;
}

/* Return the closest point on an implicit line to a point. */
/* Also return the absolute distance */
double icmImpLinePointClosest2(double cp[2], double eq[3], double pp[2]) {
	double q;		/* Closest distance to line */

	q = eq[0] * pp[0]
	  + eq[1] * pp[1]
	  + eq[2];
	
	cp[0] = pp[0] - q * eq[0];
	cp[1] = pp[1] - q * eq[1];

	return fabs(q);
}

/* Return the point of intersection of two implicit lines . */
/* Return nz if there is no intersection (lines are parallel) */
int icmImpLineIntersect2(double res[2], double eq1[3], double eq2[3]) {
	double num;

	num = eq1[0] * eq2[1] - eq2[0] * eq1[1];

	if (fabs(num) < 1e-10)
		return 1;

	res[0] = (eq2[2] * eq1[1] - eq1[2] * eq2[1])/num;
	res[1] = (eq1[2] * eq2[0] - eq2[2] * eq1[0])/num;

	return 0;
}

/* Compute the closest point on a line to a point. */
/* Return closest point and parameter value if not NULL. */
/* Return nz if the line length is zero */
int icmLinePointClosest2(double cp[2], double *pa,
                       double la0[2], double la1[2], double pp[2]) {
	double va[2], vp[2];
	double val;				/* Vector length squared */
	double a;				/* Parameter value */

	va[0] = la1[0] - la0[0];	/* Line vector */
	va[1] = la1[1] - la0[1];

	val = va[0] * va[0] + va[1] * va[1];

	if (val < 1e-12)
		return 1;

	vp[0] = pp[0] - la0[0];		/* Point vector to line base */
	vp[1] = pp[1] - la0[1];

	a = (vp[0] * va[0] + vp[1] * va[1]) / val;	/* Normalised dist of point projected onto line */

	if (cp != NULL) {
		cp[0] = (1.0 - a) * la0[0] + a * la1[0];
		cp[1] = (1.0 - a) * la0[1] + a * la1[1];
	}

	if (pa != NULL)
		*pa = a;

	return 0;
}

/* Given two infinite 2D lines define by 4 points, compute the intersection. */
/* Return nz if there is no intersection (lines are parallel) */
int icmLineIntersect2(double res[2], double p1[2], double p2[2], double p3[2], double p4[2]) {
	/* Compute by determinants */
	double x1y2_y1x2 = p1[0] * p2[1] - p1[1] * p2[0];
	double x3y4_y3x4 = p3[0] * p4[1] - p3[1] * p4[0];
	double x1_x2     = p1[0] - p2[0];
	double y1_y2     = p1[1] - p2[1];
	double x3_x4     = p3[0] - p4[0];
	double y3_y4     = p3[1] - p4[1];
	double num;							/* Numerator */

	num = x1_x2 * y3_y4 - y1_y2 * x3_x4;

	if (fabs(num) < 1e-10)
		return 1;

	res[0] = (x1y2_y1x2 * x3_x4 - x1_x2 * x3y4_y3x4)/num;
	res[1] = (x1y2_y1x2 * y3_y4 - y1_y2 * x3y4_y3x4)/num;

	return 0;
}

/* Given two finite 2D lines define by 4 points, compute their paramaterized intersection. */
/* res[] and/or aprm[] may be NULL. Param is prop. from p1 -> p2, p3 -> p4 */
/* Return 2 if there is no intersection (lines are parallel) */
/* Return 1 lines do not cross within their length */
int icmParmLineIntersect2(double res[2], double aprm[2], double p1[2], double p2[2], double p3[2], double p4[2]) {
	double _prm[2];
	double *prm = aprm != NULL ? aprm : _prm;
	double x21     = p2[0] - p1[0];
	double y21     = p2[1] - p1[1];
	double x31     = p3[0] - p1[0];
	double y31     = p3[1] - p1[1];
	double x43     = p4[0] - p3[0];
	double y43     = p4[1] - p3[1];
	double num;							/* Numerator */

	num = x43 * y21 - x21 * y43;

	if (fabs(num) < 1e-10)
		return 2;

	prm[0] = (x43 * y31 - x31 * y43)/num;		/* Parameter of 1->2 */
	prm[1] = (x21 * y31 - x31 * y21)/num;		/* Parameter of 3->4 */

	if (res != NULL) {
		res[0] = x21 * prm[0] + p1[0];
		res[1] = y21 * prm[0] + p1[1];
	}

	if (prm[0] < -1e-10 || prm[0] > (1.0 + 1e-10)
	 || prm[1] < -1e-10 || prm[1] > (1.0 + 1e-10))
		return 1;

	return 0;
}

/* Compute a blend between in0 and in1 for bl 0..1 */
void icmBlend2(double out[2], double in0[2], double in1[2], double bf) {
	out[0] = (1.0 - bf) * in0[0] + bf * in1[0];
	out[1] = (1.0 - bf) * in0[1] + bf * in1[1];
}

/* Scale a 2 vector by the given ratio */
void icmScale2(double out[2], double in[2], double rat) {
	out[0] = in[0] * rat;
	out[1] = in[1] * rat;
}

/* Scale a 2 vector by the given ratio and add it */
void icmScaleAdd2(double out[3], double in2[2], double in1[2], double rat) {
	out[0] = in2[0] + in1[0] * rat;
	out[1] = in2[1] + in1[1] * rat;
}


/* Convert orientation 0 = right, 1 = down, 2 = left, 3 = right */
/* into rotation matrix */
static void icmOr2mat(double mat[2][2], int or) {
	if (or == 0) {
		mat[0][0] = 1.0; 
		mat[0][1] = 0.0; 
		mat[1][0] = 0.0; 
		mat[1][1] = 1.0; 
	} else if (or == 1) {
		mat[0][0] = 0.0; 
		mat[0][1] = 1.0; 
		mat[1][0] = -1.0; 
		mat[1][1] = 0.0; 
	} else if (or == 2) {
		mat[0][0] = -1.0; 
		mat[0][1] = 0.0; 
		mat[1][0] = 0.0; 
		mat[1][1] = -1.0; 
	} else {
		mat[0][0] = 0.0; 
		mat[0][1] = -1.0; 
		mat[1][0] = 1.0; 
		mat[1][1] = 0.0; 
	}
}

/* Convert an angle in radians into rotation matrix (0 = right) */
void icmRad2mat(double mat[2][2], double rad) {
	double sinv = sin(rad);
	double cosv = cos(rad);
	mat[0][0] = cosv;
	mat[0][1] = -sinv;
	mat[1][0] = sinv;
	mat[1][1] = cosv;
}

/* Convert an angle in degrees (0 = right) into rotation matrix */
void icmDeg2mat(double mat[2][2], double a) {
	double rad = a * 3.1415926535897932384626433832795/180.0;
	icmRad2mat(mat, rad);
}

/* Convert a vector into a rotation matrix */
void icmVec2mat(double mat[2][2], double dx, double dy) {
	double rad = atan2(dy, dx);
	icmRad2mat(mat, rad);
}

/* Multiply 2 array by 2x2 transform matrix */
void icmMulBy2x2(double out[2], double mat[2][2], double in[2]) {
	double tt[2];

	tt[0] = mat[0][0] * in[0] + mat[0][1] * in[1];
	tt[1] = mat[1][0] * in[0] + mat[1][1] * in[1];

	out[0] = tt[0];
	out[1] = tt[1];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Set a vector to a single value */
void icmSetN(double *dst, double src, int len) {
	int j;
	for (j = 0; j < len; j++)
		dst[j] = src;
}

/* Copy a vector */
void icmCpyN(double *dst, double *src, int len) {
	int j;
	if (dst != src)
		for (j = 0; j < len; j++)
			dst[j] = src[j];
}

/* Clip a vector to the range 0.0 .. 1.0 */
void icmClipN(double out[3], double in[3], unsigned int len) {
	int j;
	for (j = 0; j < len; j++) {
		out[j] = in[j];
		if (out[j] < 0.0)
			out[j] = 0.0;
		else if (out[j] > 1.0)
			out[j] = 1.0;
	}
}

/* Clip a vector to the range 0.0 .. 1.0 */
/* and return any clipping margine over the limit */
double icmClipNmarg(double *out, double *in, unsigned int len) {
	int j;
	double tt, marg = 0.0;
	for (j = 0; j < len; j++) {
		out[j] = in[j];
		if (out[j] < 0.0) {
			tt = 0.0 - out[j];
			out[j] = 0.0;
			if (tt > marg)
				marg = tt;
		} else if (out[j] > 1.0) {
			tt = out[j] - 1.0;
			out[j] = 1.0;
			if (tt > marg)
				marg = tt;
		}
	}
	return marg;
}

/* Return the magnitude (norm) of the difference between two vectors */
double icmDiffN(double *s1, double *s2, int len) {
	int i;
	double rv = 0.0;

	for (i = 0; i < len; i++) {
		double tt = s1[i] - s2[i];
		rv += tt * tt;
	}

	return sqrt(rv);
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
psh_initN(
psh *p,				/* Pointer to structure to initialise */
int di,				/* Dimensionality */
unsigned int res[],	/* Size per coordinate */
int co[]			/* Coordinates to initialise (May be NULL) */
) {
	int e;

	memset(p, 0, sizeof(psh));

	p->di = di;
	for (e = 0; e < di; e++)
		p->res[e] = res[e];
	p->xbits = 0;

	/* Compute bits per axis and total bits */
	for (p->tbits = e = 0; e < di; e++) {
		for (p->bits[e] = 0; (1u << p->bits[e]) < p->res[e]; p->bits[e]++)
			;
		p->tbits += p->bits[e];
		if (p->bits[e] > p->xbits)
			p->xbits = p->bits[e];
	}

	if (p->tbits > 32)		/* Hmm */
		return 0;

	/* Compute the total count mask */
	p->tmask = ((((unsigned)1) << p->tbits)-1);

	/* Compute usable count */
	p->count = 1;
	for (e = 0; e < di; e++)
		p->count *= p->res[e];

	p->ix = 0;		/* Initial binary index */

	if (co != NULL) {
		for (e = 0; e < di; e++)
			co[e] = 0;
	}

	return p->count;
}

/* Backwards compatible same res per axis init */
unsigned
psh_init(
psh *p,			/* Pointer to structure to initialise */
int di,				/* Dimensionality */
unsigned int res,	/* Size */
int co[]			/* Coordinates to initialise (May be NULL) */
) {
	int e;
	unsigned int mres[MAX_CHAN];

	for (e = 0; e < di; e++)
		mres[e] = res;

	return psh_initN(p, di, mres, co);
}

/* Diagnostic init - psh returns the value in co[] and then terminates */
unsigned
psh_init_diag(
psh *p,				/* Pointer to structure to initialise */
int di,				/* Dimensionality */
int co[]			/* Coordinates to start on */
) {
	int e;

	memset(p, 0, sizeof(psh));

	p->di = di;
	p->diag = 1;

	return 1;
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
psh *p,		/* Pointer to structure */
int co[]	/* Coordinates to return */
) {
	int di = p->di;
	int e;

	if (p->diag)
		return 1;

	do {
		unsigned int b;
		int gix;	/* Gray code index */
		
		p->ix = (p->ix + 1) & p->tmask;

		gix = p->ix ^ (p->ix >> 1);		/* Convert to gray code index */
	
		for (e = 0; e < di; e++) 
			co[e] = 0;
		
		for (b = 0; b < p->xbits; b++) {			/* Distribute bits */
			if (b & 1) {
				for (e = di-1; e >= 0; e--)  {		/* In reverse order */
					if (b < p->bits[e]) {
						co[e] |= (gix & 1) << b;
						gix >>= 1;
					}
				}
			} else {
				for (e = 0; e < di; e++)  {			/* In normal order */
					if (b < p->bits[e]) {
						co[e] |= (gix & 1) << b;
						gix >>= 1;
					}
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
			if (tv >= p->res[e])	/* Dumbo filter - increment again if outside cube range */
				break;
			co[e] = tv;
		}

	} while (e < di);
	
	return (p->ix == 0);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* CIE Y (range 0 .. 1) to perceptual CIE 1976 L* (range 0 .. 100) */
double
icmY2L(double val) {
	if (val > 0.008856451586)
		val = pow(val,1.0/3.0);
	else
		val = 7.787036979 * val + 16.0/116.0;

	val = (116.0 * val - 16.0);
	return val;
}

/* Perceptual CIE 1976 L* (range 0 .. 100) to CIE Y (range 0 .. 1) */
double
icmL2Y(double val) {
	val = (val + 16.0)/116.0;

	if (val > 24.0/116.0)
		val = pow(val,3.0);
	else
		val = (val - 16.0/116.0)/7.787036979;
	return val;
}

/* CIE XYZ (range 0 .. 1) to perceptual CIE 1976 L*a*b* */
void
icmXYZ2Lab(icmXYZNumber *w, double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double x,y,z,fx,fy,fz;

	x = X/w->X;
	y = Y/w->Y;
	z = Z/w->Z;

	if (x > 0.008856451586)
		fx = pow(x,1.0/3.0);
	else
		fx = 7.787036979 * x + 16.0/116.0;

	if (y > 0.008856451586)
		fy = pow(y,1.0/3.0);
	else
		fy = 7.787036979 * y + 16.0/116.0;

	if (z > 0.008856451586)
		fz = pow(z,1.0/3.0);
	else
		fz = 7.787036979 * z + 16.0/116.0;

	out[0] = 116.0 * fy - 16.0;
	out[1] = 500.0 * (fx - fy);
	out[2] = 200.0 * (fy - fz);
}

/* Perceptual CIE 1976 L*a*b* to CIE XYZ (range 0 .. 1) */
void
icmLab2XYZ(icmXYZNumber *w, double *out, double *in) {
	double L = in[0], a = in[1], b = in[2];
	double x,y,z,fx,fy,fz;

	fy = (L + 16.0)/116.0;
	fx = a/500.0 + fy;
	fz = fy - b/200.0;

	if (fy > 24.0/116.0)
		y = pow(fy,3.0);
	else
		y = (fy - 16.0/116.0)/7.787036979;

	if (fx > 24.0/116.0)
		x = pow(fx,3.0);
	else
		x = (fx - 16.0/116.0)/7.787036979;

	if (fz > 24.0/116.0)
		z = pow(fz,3.0);
	else
		z = (fz - 16.0/116.0)/7.787036979;

	out[0] = x * w->X;
	out[1] = y * w->Y;
	out[2] = z * w->Z;
}

/*
 * This is a modern update to L*a*b*, based on IPT space.
 *
 * Differences to L*a*b* and IPT:
 *	Using inverse CIE 2012 2 degree LMS to XYZ matrix instead of Hunt-Pointer-Estevez.
 *  Von Kries chromatic adapation in LMS cone space.
 *  Using L* compression rather than IPT pure 0.43 power.
 *  Tweaked LMS' to IPT matrix to account for change in XYZ to LMS matrix.
 *  Output scaled to L*a*b* type ranges, to maintain 1 JND scale.
 *  (Watch out - L* value is not a non-linear Y value though!
 *   - Interesting that Dolby force L to be just dependent on Y
 *     by making L = 0.5 L | 0.5 M in ICtCp space).
 */

/* CIE XYZ to perceptual Lpt */
void
icmXYZ2Lpt(icmXYZNumber *w, double *out, double *in) {
	double wxyz[3];
	double wlms[3];
	double lms[3];
	double xyz2lms[3][3] = {
        {  0.2052445519046028, 0.8334486497310412, -0.0386932016356441 },
        { -0.4972221301804286, 1.4034846060306130,  0.0937375241498157 },
        {  0.0000000000000000, 0.0000000000000000,  1.0000000000000000 }
	};
	double lms2ipt[3][3] = {
        {  0.6585034777870502,  0.1424555300344579,  0.1990409921784920 },
        {  5.6413505933276049, -6.1697985811414187,  0.5284479878138138 },
        {  1.6370552576322106,  0.0192823194340315, -1.6563375770662419 }
	};
	int j;

	/* White point in Cone space */
	wxyz[0] = w->X;
	wxyz[1] = w->Y;
	wxyz[2] = w->Z;
	icmMulBy3x3(wlms, xyz2lms, wxyz);

	/* Incoming XYZ to Cone space */
	icmMulBy3x3(lms, xyz2lms, in);

	for (j = 0; j < 3; j++) {
		/* Von Kries chromatic adapation */
		lms[j] /= wlms[j];

		/* Non-linearity */
		if (lms[j] > 0.008856451586)
			lms[j] = pow(lms[j],1.0/3.0);
		else
			lms[j] = 7.787036979 * lms[j] + 16.0/116.0;
		lms[j] = 116.0 * lms[j] - 16.0;
	}
	/* IPT */
	icmMulBy3x3(out, lms2ipt, lms);
}

void
icmLpt2XYZ(icmXYZNumber *w, double *out, double *in) {
	double wxyz[3];
	double wlms[3];
	double lms[3];
	double xyz2lms[3][3] = {
        {  0.2052445519046028, 0.8334486497310412, -0.0386932016356441 },
        { -0.4972221301804286, 1.4034846060306130,  0.0937375241498157 },
        {  0.0000000000000000, 0.0000000000000000,  1.0000000000000000 }
	};
	double ipt2lms[3][3] = {
        {  1.0000000000000000,  0.0234881527511557,  0.1276631419615779 },
        {  1.0000000000000000, -0.1387534648407132,  0.0759005921388901 },
        {  1.0000000000000000,  0.0215994105411036, -0.4766811148374502 }
	};
	double lms2xyz[3][3] = {
        {  1.9979376130193824, -1.1864600428553205,  0.1885224298359384 },
        {  0.7078230795296872,  0.2921769204703129, -0.0000000000000000 },
        {  0.0000000000000000,  0.0000000000000000,  1.0000000000000000 }
	};
	int j;

	wxyz[0] = w->X;
	wxyz[1] = w->Y;
	wxyz[2] = w->Z;
	icmMulBy3x3(wlms, xyz2lms, wxyz);

	icmMulBy3x3(lms, ipt2lms, in);

	for (j = 0; j < 3; j++) {
		lms[j] = (lms[j] + 16.0)/116.0;

		if (lms[j] > 24.0/116.0)
			lms[j] = pow(lms[j], 3.0);
		else
			lms[j] = (lms[j] - 16.0/116.0)/7.787036979;

		lms[j] *= wlms[j];
	}

	icmMulBy3x3(out, lms2xyz, lms);
}

/* LCh to Lab (general to polar, works with Lpt, Luv too) */
void icmLCh2Lab(double *out, double *in) {
	double C, h;

	C = in[1];
	h = M_PI/180.0 * in[2];

	out[0] = in[0];
	out[1] = C * cos(h);
	out[2] = C * sin(h);
}

/* Lab to LCh (general to polar, works with Lpt, Luv too) */
void icmLab2LCh(double *out, double *in) {
	double C, h;

	C = sqrt(in[1] * in[1] + in[2] * in[2]);

    h = (180.0/M_PI) * atan2(in[2], in[1]);
	h = (h < 0.0) ? h + 360.0 : h;

	out[0] = in[0];
	out[1] = C;
	out[2] = h;
}

/* CIE XYZ to perceptual CIE 1976 L*u*v* */
extern ICCLIB_API void icmXYZ2Luv(icmXYZNumber *w, double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double un, vn, u, v, fl, fu, fv;

	un = (4.0 * w->X) / (w->X + 15.0 * w->Y + 3.0 * w->Z);
	vn = (9.0 * w->Y) / (w->X + 15.0 * w->Y + 3.0 * w->Z);
	u = (4.0 * X) / (X + 15.0 * Y + 3.0 * Z);
	v = (9.0 * Y) / (X + 15.0 * Y + 3.0 * Z);
	
	Y /= w->Y;

	if (Y > 0.008856451586)
		fl = pow(Y,1.0/3.0);
	else
		fl = 7.787036979 * Y + 16.0/116.0;

	fu = u - un;
	fv = v - vn;

	out[0] = 116.0 * fl - 16.0;
	out[1] = 13.0 * out[0] * fu;
	out[2] = 13.0 * out[0] * fv;
}

/* Perceptual CIE 1976 L*u*v* to CIE XYZ */
extern ICCLIB_API void icmLuv2XYZ(icmXYZNumber *w, double *out, double *in) {
	double un, vn, u, v, fl, fu, fv, sum, X, Y, Z;

	fl = (in[0] + 16.0)/116.0;
	fu = in[1] / (13.0 * in[0]);
	fv = in[2] / (13.0 * in[0]);

	un = (4.0 * w->X) / (w->X + 15.0 * w->Y + 3.0 * w->Z);
	vn = (9.0 * w->Y) / (w->X + 15.0 * w->Y + 3.0 * w->Z);

	u = fu + un;
	v = fv + vn;

	if (fl > 24.0/116.0)
		Y = pow(fl,3.0);
	else
		Y = (fl - 16.0/116.0)/7.787036979;
	Y *= w->Y;

	sum = (9.0 * Y)/v;
	X = (u * sum)/4.0;
	Z = (sum - X - 15.0 * Y)/3.0;

	out[0] = X;
	out[1] = Y;
	out[2] = Z;
}

/* XYZ to Yxy */
extern ICCLIB_API void icmXYZ2Yxy(double *out, double *in) {
	double sum = in[0] + in[1] + in[2];
	double Y, x, y;

	if (sum < 1e-9) {
		Y = 0.0;
		y = 1.0/3.0;
		x = 1.0/3.0;
	} else {
		Y = in[1];
		x = in[0]/sum;
		y = in[1]/sum;
	}
	out[0] = Y;
	out[1] = x;
	out[2] = y;
}

/* XYZ to xy */
extern ICCLIB_API void icmXYZ2xy(double *out, double *in) {
	double sum = in[0] + in[1] + in[2];
	double x, y;

	if (sum < 1e-9) {
		y = 1.0/3.0;
		x = 1.0/3.0;
	} else {
		x = in[0]/sum;
		y = in[1]/sum;
	}
	out[0] = x;
	out[1] = y;
}

/* Yxy to XYZ */
extern ICCLIB_API void icmYxy2XYZ(double *out, double *in) {
	double Y = in[0];
	double x = in[1];
	double y = in[2];
	double z = 1.0 - x - y;
	double sum;
	if (y < 1e-9) {
		out[0] = out[1] = out[2] = 0.0;
	} else {
		sum = Y/y;
		out[0] = x * sum;
		out[1] = Y;
		out[2] = z * sum;
	}
}

/* Y & xy to XYZ */
extern ICCLIB_API void icmY_xy2XYZ(double *out, double *xy, double Y) {
	double x = xy[0];
	double y = xy[1];
	double z = 1.0 - x - y;
	double sum;
	if (y < 1e-9) {
		out[0] = out[1] = out[2] = 0.0;
	} else {
		sum = Y/y;
		out[0] = x * sum;
		out[1] = Y;
		out[2] = z * sum;
	}
}

/* CIE XYZ to perceptual CIE 1976 UCS diagram Yu'v'*/
/* (Yu'v' is a better linear chromaticity space than Yxy) */
extern ICCLIB_API void icmXYZ21976UCS(double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double den, u, v;

	den = (X + 15.0 * Y + 3.0 * Z);

	if (den < 1e-9) {
		Y = 0.0;
		u = 4.0/19.0;
		v = 9.0/19.0;
	} else {
		u = (4.0 * X) / den;
		v = (9.0 * Y) / den;
	}
	
	out[0] = Y;
	out[1] = u;
	out[2] = v;
}

/* Perceptual CIE 1976 UCS diagram Yu'v' to CIE XYZ */
extern ICCLIB_API void icm1976UCS2XYZ(double *out, double *in) {
	double u, v, fl, fu, fv, sum, X, Y, Z;

	Y = in[0];
	u = in[1];
	v = in[2];

	if (v < 1e-9) {
		X = Y = Z = 0.0;
	} else {
		X = ((9.0 * u * Y)/(4.0 * v));
		Z = -(((20.0 * v + 3.0 * u - 12.0) * Y)/(4.0 * v));
	}

	out[0] = X;
	out[1] = Y;
	out[2] = Z;
}

/* CIE 1976 UCS diagram Y & u'v' to XYZ */
extern ICCLIB_API void icm1976UCSY_uv2XYZ(double *out, double *uv, double Y) {
	double u, v, fl, fu, fv, sum, X, Z;

	u = uv[0];
	v = uv[1];

	if (v < 1e-9) {
		X = Y = Z = 0.0;
	} else {
		X = ((9.0 * u * Y)/(4.0 * v));
		Z = -(((20.0 * v + 3.0 * u - 12.0) * Y)/(4.0 * v));
	}

	out[0] = X;
	out[1] = Y;
	out[2] = Z;
}

/* CIE XYZ to perceptual CIE 1976 UCS diagram u'v'*/
/* (u'v' is a better linear chromaticity space than xy) */
extern ICCLIB_API void icmXYZ21976UCSuv(double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double den, u, v;

	den = (X + 15.0 * Y + 3.0 * Z);

	if (den < 1e-9) {
		u = 4.0/19.0;
		v = 9.0/19.0;
	} else {
		u = (4.0 * X) / den;
		v = (9.0 * Y) / den;
	}
	
	out[0] = u;
	out[1] = v;
}


/* CIE XYZ to perceptual CIE 1960 UCS */
/* (This was obsoleted by the 1976UCS, but is still used */
/*  in computing color temperatures.) */
extern ICCLIB_API void icmXYZ21960UCS(double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double den, u, v;

	den = (X + 15.0 * Y + 3.0 * Z);

	if (den < 1e-9) {
		Y = 0.0;
		u = 4.0/19.0;
		v = 6.0/19.0;
	} else {
		u = (4.0 * X) / den; 
		v = (6.0 * Y) / den;
	}
	
	out[0] = Y;
	out[1] = u;
	out[2] = v;
}

/* Perceptual CIE 1960 UCS to CIE XYZ */
extern ICCLIB_API void icm1960UCS2XYZ(double *out, double *in) {
	double u, v, fl, fu, fv, sum, X, Y, Z;

	Y = in[0];
	u = in[1];
	v = in[2];

	if (v < 1e-9) {
		X = Y = Z = 0.0;
	} else {
		X = ((3.0 * u * Y)/(2.0 * v));
		Z = -(((10.0 * v + u - 4.0) * Y)/(2.0 * v));
	}

	out[0] = X;
	out[1] = Y;
	out[2] = Z;
}

/* CIE XYZ to perceptual CIE 1964 WUV (U*V*W*) */
/* (This is obsolete but still used in computing CRI) */
extern ICCLIB_API void icmXYZ21964WUV(icmXYZNumber *w, double *out, double *in) {
	double W, U, V;
	double wucs[3];
	double iucs[3];

	icmXYZ2Ary(wucs, *w);
	icmXYZ21960UCS(wucs, wucs);
	icmXYZ21960UCS(iucs, in);

	W = 25.0 * pow(iucs[0] * 100.0/wucs[0], 1.0/3.0) - 17.0;
	U = 13.0 * W * (iucs[1] - wucs[1]);
	V = 13.0 * W * (iucs[2] - wucs[2]);

	out[0] = W;
	out[1] = U;
	out[2] = V;
}

/* Perceptual CIE 1964 WUV (U*V*W*) to CIE XYZ */
extern ICCLIB_API void icm1964WUV2XYZ(icmXYZNumber *w, double *out, double *in) {
	double W, U, V;
	double wucs[3];
	double iucs[3];

	icmXYZ2Ary(wucs, *w);
	icmXYZ21960UCS(wucs, wucs);

	W = in[0];
	U = in[1];
	V = in[2];

	iucs[0] = pow((W + 17.0)/25.0, 3.0) * wucs[0]/100.0;
	iucs[1] = U / (13.0 * W) + wucs[1];
	iucs[2] = V / (13.0 * W) + wucs[2];

	icm1960UCS2XYZ(out, iucs);
}

/* CIE CIE1960 UCS to perceptual CIE 1964 WUV (U*V*W*) */
/* (This is used in computing CRI) */
extern ICCLIB_API void icm1960UCS21964WUV(icmXYZNumber *w, double *out, double *in) {
	double W, U, V;
	double wucs[3];

	icmXYZ2Ary(wucs, *w);
	icmXYZ21960UCS(wucs, wucs);

	W = 25.0 * pow(in[0] * 100.0/wucs[0], 1.0/3.0) - 17.0;
	U = 13.0 * W * (in[1] - wucs[1]);
	V = 13.0 * W * (in[2] - wucs[2]);

	out[0] = W;
	out[1] = U;
	out[2] = V;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* NOTE :- that these values are for the 1931 standard observer. */
/* Since they are an arbitrary 4 decimal place accuracy, we round */
/* them to be exactly the same as ICC header encoded values, */
/* to avoid any slight discrepancy with PCS white from profiles. */

/* available D50 Illuminant */
icmXYZNumber icmD50 = { 		/* Profile illuminant - D50 */
    RND_S15FIXED16(0.9642),
	RND_S15FIXED16(1.0000),
	RND_S15FIXED16(0.8249)
};

icmXYZNumber icmD50_100 = {		/* Profile illuminant - D50, scaled to 100 */
    RND_S15FIXED16(0.9642) * 100.0,
	RND_S15FIXED16(1.0000) * 100.0,
	RND_S15FIXED16(0.8249) * 100.0
};

double icmD50_ary3[3] = { 		/* Profile illuminant - D50 */
    RND_S15FIXED16(0.9642),
	RND_S15FIXED16(1.0000),
	RND_S15FIXED16(0.8249)
};

double icmD50_100_ary3[3] = {	/* Profile illuminant - D50, scaled to 100 */
    RND_S15FIXED16(0.9642) * 100.0,
	RND_S15FIXED16(1.0000) * 100.0,
	RND_S15FIXED16(0.8249) * 100.0
};

/* available D65 Illuminant */
icmXYZNumber icmD65 = { 		/* Profile illuminant - D65 */
    RND_S15FIXED16(0.9505),
	RND_S15FIXED16(1.0000),
	RND_S15FIXED16(1.0890)
};

icmXYZNumber icmD65_100 = { 	/* Profile illuminant - D65, scaled to 100 */
    RND_S15FIXED16(0.9505) * 100.0,
	RND_S15FIXED16(1.0000) * 100.0,
	RND_S15FIXED16(1.0890) * 100.0
};

double icmD65_ary3[3] = { 		/* Profile illuminant - D65 */
    RND_S15FIXED16(0.9505),
	RND_S15FIXED16(1.0000),
	RND_S15FIXED16(1.0890)
};

double icmD65_100_ary3[3] = { 	/* Profile illuminant - D65, scaled to 100 */
    RND_S15FIXED16(0.9505) * 100.0,
	RND_S15FIXED16(1.0000) * 100.0,
	RND_S15FIXED16(1.0890) * 100.0
};


/* Default black point */
icmXYZNumber icmBlack = {
    0.0000, 0.0000, 0.0000
};

/* The Standard ("wrong Von-Kries") chromatic transform matrix */
double icmWrongVonKries[3][3] = {
	{ 1.0000, 0.0000, 0.0000 },
	{ 0.0000, 1.0000, 0.0000 },
	{ 0.0000, 0.0000, 1.0000 }
};

/* The Bradford chromatic transform matrix */
double icmBradford[3][3] = {
	{ RND_S15FIXED16( 0.8951), RND_S15FIXED16( 0.2664), RND_S15FIXED16(-0.1614) },
	{ RND_S15FIXED16(-0.7502), RND_S15FIXED16( 1.7135), RND_S15FIXED16( 0.0367) },
	{ RND_S15FIXED16( 0.0389), RND_S15FIXED16(-0.0685), RND_S15FIXED16( 1.0296) }
};

/* Return the normal Delta E given two Lab values */
double icmLabDE(double *Lab0, double *Lab1) {
	double rv = 0.0, tt;

	tt = Lab0[0] - Lab1[0];
	rv += tt * tt;
	tt = Lab0[1] - Lab1[1];
	rv += tt * tt;
	tt = Lab0[2] - Lab1[2];
	rv += tt * tt;
	
	return sqrt(rv);
}

/* Return the normal Delta E squared, given two Lab values */
double icmLabDEsq(double *Lab0, double *Lab1) {
	double rv = 0.0, tt;

	tt = Lab0[0] - Lab1[0];
	rv += tt * tt;
	tt = Lab0[1] - Lab1[1];
	rv += tt * tt;
	tt = Lab0[2] - Lab1[2];
	rv += tt * tt;
	
	return rv;
}

/* Return the normal Delta E squared given two XYZ values */
extern ICCLIB_API double icmXYZLabDEsq(icmXYZNumber *w, double *in0, double *in1) {
	double lab0[3], lab1[3], rv;

	icmXYZ2Lab(w, lab0, in0);
	icmXYZ2Lab(w, lab1, in1);
	rv = icmLabDEsq(lab0, lab1);
	return rv;
}

/* Return the normal Delta E given two XYZ values */
extern ICCLIB_API double icmXYZLabDE(icmXYZNumber *w, double *in0, double *in1) {
	double lab0[3], lab1[3], rv;

	icmXYZ2Lab(w, lab0, in0);
	icmXYZ2Lab(w, lab1, in1);
	rv = icmLabDE(lab0, lab1);
	return rv;
}

/* Return the normal Delta E squared given two XYZ values */
extern ICCLIB_API double icmXYZLptDEsq(icmXYZNumber *w, double *in0, double *in1) {
	double lab0[3], lab1[3], rv;

	icmXYZ2Lpt(w, lab0, in0);
	icmXYZ2Lpt(w, lab1, in1);
	rv = icmLabDEsq(lab0, lab1);
	return rv;
}

/* Return the normal Delta E given two XYZ values */
extern ICCLIB_API double icmXYZLptDE(icmXYZNumber *w, double *in0, double *in1) {
	double lab0[3], lab1[3], rv;

	icmXYZ2Lpt(w, lab0, in0);
	icmXYZ2Lpt(w, lab1, in1);
	rv = icmLabDE(lab0, lab1);
	return rv;
}

/* (Note that CIE94 can give odd results for very large delta E's, */
/* when one of the two points is near the neutral axis: */
/* ie DE(A,B + del) != DE(A,B) + DE(del) */
#ifdef NEVER
{
	double w1[3] = { 99.996101, -0.058417, -0.010557 };
	double c1[3] = { 60.267956, 98.845863, -61.163277 };
	double w2[3] = { 100.014977, -0.138339, 0.089744 };
	double c2[3] = { 60.294464, 98.117104, -60.843159 };
	
	printf("DE   1 = %f, 2 = %f\n", icmLabDE(c1, w1), icmLabDE(c2, w2));
	printf("DE94 1 = %f, 2 = %f\n", icmCIE94(c1, w1), icmCIE94(c2, w2));
}
#endif


/* Return the CIE94 Delta E color difference measure, squared */
double icmCIE94sq(double Lab0[3], double Lab1[3]) {
	double desq, dhsq;
	double dlsq, dcsq;
	double c12;

	{
		double dl, da, db;
		dl = Lab0[0] - Lab1[0];
		dlsq = dl * dl;		/* dl squared */
		da = Lab0[1] - Lab1[1];
		db = Lab0[2] - Lab1[2];

		/* Compute normal Lab delta E squared */
		desq = dlsq + da * da + db * db;
	}

	{
		double c1, c2, dc;

		/* Compute chromanance for the two colors */
		c1 = sqrt(Lab0[1] * Lab0[1] + Lab0[2] * Lab0[2]);
		c2 = sqrt(Lab1[1] * Lab1[1] + Lab1[2] * Lab1[2]);
		c12 = sqrt(c1 * c2);	/* Symetric chromanance */

		/* delta chromanance squared */
		dc = c1 - c2;
		dcsq = dc * dc;
	}

	/* Compute delta hue squared */
	if ((dhsq = desq - dlsq - dcsq) < 0.0)
		dhsq = 0.0;
	{
		double sc, sh;

		/* Weighting factors for delta chromanance & delta hue */
		sc = 1.0 + 0.045 * c12;
		sh = 1.0 + 0.015 * c12;
		return dlsq + dcsq/(sc * sc) + dhsq/(sh * sh);
	}
}

/* Return the CIE94 Delta E color difference measure */
double icmCIE94(double Lab0[3], double Lab1[3]) {
	return sqrt(icmCIE94sq(Lab0, Lab1));
}

/* Return the CIE94 Delta E color difference measure for two XYZ values */
extern ICCLIB_API double icmXYZCIE94(icmXYZNumber *w, double *in0, double *in1) {
	double lab0[3], lab1[3];

	icmXYZ2Lab(w, lab0, in0);
	icmXYZ2Lab(w, lab1, in1);
	return sqrt(icmCIE94sq(lab0, lab1));
}


/* From the paper "The CIEDE2000 Color-Difference Formula: Implementation Notes, */
/* Supplementary Test Data, and Mathematical Observations", by */
/* Gaurav Sharma, Wencheng Wu and Edul N. Dalal, */
/* Color Res. Appl., vol. 30, no. 1, pp. 21-30, Feb. 2005. */

/* Return the CIEDE2000 Delta E color difference measure squared, for two Lab values */
double icmCIE2Ksq(double *Lab0, double *Lab1) {
	double C1, C2;
	double h1, h2;
	double dL, dC, dH;
	double dsq;

	/* The trucated value of PI is needed to ensure that the */
	/* test cases pass, as one of them lies on the edge of */
	/* a mathematical discontinuity. The precision is still */
	/* enough for any practical use. */
#define RAD2DEG(xx) (180.0/M_PI * (xx))
#define DEG2RAD(xx) (M_PI/180.0 * (xx))

	/* Compute Cromanance and Hue angles */
	{
		double C1ab, C2ab;
		double Cab, Cab7, G;
		double a1, a2;

		C1ab = sqrt(Lab0[1] * Lab0[1] + Lab0[2] * Lab0[2]);
		C2ab = sqrt(Lab1[1] * Lab1[1] + Lab1[2] * Lab1[2]);
		Cab = 0.5 * (C1ab + C2ab);
		Cab7 = pow(Cab,7.0);
		G = 0.5 * (1.0 - sqrt(Cab7/(Cab7 + 6103515625.0)));
		a1 = (1.0 + G) * Lab0[1];
		a2 = (1.0 + G) * Lab1[1];
		C1 = sqrt(a1 * a1 + Lab0[2] * Lab0[2]);
		C2 = sqrt(a2 * a2 + Lab1[2] * Lab1[2]);

		if (C1 < 1e-9)
			h1 = 0.0;
		else {
			h1 = RAD2DEG(atan2(Lab0[2], a1));
			if (h1 < 0.0)
				h1 += 360.0;
		}

		if (C2 < 1e-9)
			h2 = 0.0;
		else {
			h2 = RAD2DEG(atan2(Lab1[2], a2));
			if (h2 < 0.0)
				h2 += 360.0;
		}
	}

	/* Compute delta L, C and H */
	{
		double dh;

		dL = Lab1[0] - Lab0[0];
		dC = C2 - C1;
		if (C1 < 1e-9 || C2 < 1e-9) {
			dh = 0.0;
		} else {
			dh = h2 - h1;
			if (dh > 180.0)
				dh -= 360.0;
			else if (dh < -180.0)
				dh += 360.0;
		}

		dH = 2.0 * sqrt(C1 * C2) * sin(DEG2RAD(0.5 * dh));
	}

	{
		double L, C, h, T;
		double hh, ddeg;
		double C7, RC, L50sq, SL, SC, SH, RT;
		double dLsq, dCsq, dHsq, RCH;

		L = 0.5 * (Lab0[0]  + Lab1[0]);
		C = 0.5 * (C1 + C2);

		if (C1 < 1e-9 || C2 < 1e-9) {
			h = h1 + h2;
		} else {
			h = h1 + h2;
			if (fabs(h1 - h2) > 180.0) {
				if (h < 360.0)
					h += 360.0;
				else if (h >= 360.0)
					h -= 360.0;
			}
			h *= 0.5;
		}

		T = 1.0 - 0.17 * cos(DEG2RAD(h-30.0)) + 0.24 * cos(DEG2RAD(2.0 * h))
		  + 0.32 * cos(DEG2RAD(3.0 * h + 6.0)) - 0.2 * cos(DEG2RAD(4.0 * h - 63.0));
		L50sq = (L - 50.0) * (L - 50.0);

		SL = 1.0 + (0.015 * L50sq)/sqrt(20.0 + L50sq);
		SC = 1.0 + 0.045 * C;
		SH = 1.0 + 0.015 * C * T;

		dLsq = dL/SL;
		dCsq = dC/SC;
		dHsq = dH/SH;

		hh = (h - 275.0)/25.0;
		ddeg = 30.0 * exp(-hh * hh);
		C7 = pow(C, 7.0);
		RC = 2.0 * sqrt(C7/(C7 + 6103515625.0));
		RT = -sin(DEG2RAD(2 * ddeg)) * RC;

		RCH = RT * dCsq * dHsq;

		dLsq *= dLsq;
		dCsq *= dCsq;
		dHsq *= dHsq;

		dsq = dLsq + dCsq + dHsq + RCH;
	}

	return dsq;

#undef RAD2DEG
#undef DEG2RAD
}

/* Return the CIE2DE000 Delta E color difference measure for two Lab values */
double icmCIE2K(double *Lab0, double *Lab1) {
	return sqrt(icmCIE2Ksq(Lab0, Lab1));
}

/* Return the CIEDE2000 Delta E color difference measure for two XYZ values */
ICCLIB_API double icmXYZCIE2K(icmXYZNumber *w, double *in0, double *in1) {
	double lab0[3], lab1[3];

	icmXYZ2Lab(w, lab0, in0);
	icmXYZ2Lab(w, lab1, in1);
	return sqrt(icmCIE2Ksq(lab0, lab1));
}



/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Independent chromatic adaptation transform utility. */
/* Return a 3x3 chromatic adaptation matrix */
/* Use icmMulBy3x3(dst, mat, src) */
/* NOTE that to transform primaries they */
/* must be mat[XYZ][RGB] format! */
void icmChromAdaptMatrix(
	int flags,				/* Use Bradford flag, Transform given matrix flag */
	icmXYZNumber d_wp,		/* Destination white point */
	icmXYZNumber s_wp,		/* Source white point */
	double mat[3][3]		/* Destination matrix */
) {
	double dst[3], src[3];			/* Source & destination white points */
	double vkmat[3][3];				/* Von Kries matrix */
	static int inited = 0;			/* Compute inverse bradford once */
	static double ibradford[3][3];	/* Inverse Bradford */

	/* Set initial matrix to unity if creating from scratch */
	if (!(flags & ICM_CAM_MULMATRIX)) {
		icmSetUnity3x3(mat);
	}

	icmXYZ2Ary(src, s_wp);
	icmXYZ2Ary(dst, d_wp);

	if (flags & ICM_CAM_BRADFORD) {
		icmMulBy3x3(src, icmBradford, src);
		icmMulBy3x3(dst, icmBradford, dst);
	}

	/* Setup the Von Kries white point adaptation matrix */
	vkmat[0][0] = dst[0]/src[0];
	vkmat[1][1] = dst[1]/src[1];
	vkmat[2][2] = dst[2]/src[2];
	vkmat[0][1] = vkmat[0][2] = 0.0;
	vkmat[1][0] = vkmat[1][2] = 0.0;
	vkmat[2][0] = vkmat[2][1] = 0.0;

	/* Transform to Bradford space if requested */
	if (flags & ICM_CAM_BRADFORD) {
		icmMul3x3(mat, icmBradford);
	}

	/* Apply chromatic adaptation */
	icmMul3x3(mat, vkmat);

	/* Transform from Bradford space */
	if (flags & ICM_CAM_BRADFORD) {
		if (inited == 0) {
			icmInverse3x3(ibradford, icmBradford);
			inited = 1;
		}
		icmMul3x3(mat, ibradford);
	}

	/* We're done */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* RGB XYZ primaries device to RGB->XYZ transform matrix. */
/* We assume that the device is perfectly additive, but that */
/* there may be a scale factor applied to the channels to */
/* match the white point at RGB = 1. */
/* Use icmMulBy3x3(dst, mat, src) */
/* Return non-zero if matrix would be singular */
int icmRGBXYZprim2matrix(
	double red[3],		/* Red colorant */
	double green[3],	/* Green colorant */
	double blue[3],		/* Blue colorant */
	double white[3],	/* White point */
	double mat[3][3]	/* Destination matrix[RGB][XYZ] */
) {
	double tmat[3][3];
	double t[3];

	/* Assemble the colorants into a matrix */
	tmat[0][0] = red[0];
	tmat[0][1] = green[0];
	tmat[0][2] = blue[0];
	tmat[1][0] = red[1];
	tmat[1][1] = green[1];
	tmat[1][2] = blue[1];
	tmat[2][0] = red[2];
	tmat[2][1] = green[2];
	tmat[2][2] = blue[2];

	/* Compute the inverse */
	if (icmInverse3x3(mat, tmat))
		return 1;

	/* Compute scale vector that maps colorants to white point */
	t[0] = mat[0][0] * white[0]
	     + mat[0][1] * white[1] 
	     + mat[0][2] * white[2]; 
	t[1] = mat[1][0] * white[0]
	     + mat[1][1] * white[1] 
	     + mat[1][2] * white[2]; 
	t[2] = mat[2][0] * white[0]
	     + mat[2][1] * white[1] 
	     + mat[2][2] * white[2]; 

	/* Now formulate the transform matrix */
	mat[0][0] = red[0]   * t[0];
	mat[0][1] = green[0] * t[1];
	mat[0][2] = blue[0]  * t[2];
	mat[1][0] = red[1]   * t[0];
	mat[1][1] = green[1] * t[1];
	mat[1][2] = blue[1]  * t[2];
	mat[2][0] = red[2]   * t[0];
	mat[2][1] = green[2] * t[1];
	mat[2][2] = blue[2]  * t[2];

	return 0;
}

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
) {
	double r[3], g[3], b[3];
	
	icmYxy2XYZ(r, red);
	icmYxy2XYZ(g, green);
	icmYxy2XYZ(b, blue);
	icmYxy2XYZ(wXYZ, white);

	return icmRGBXYZprim2matrix(r, g, b, wXYZ, mat);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Pre-round a 3x3 matrix to ensure that the product of */
/* the matrix and the input value is the same as */
/* the quantized matrix product. This is used to improve accuracy */
/* of 'chad' tag in computing absolute white point. */ 
void icmQuantize3x3S15Fixed16(
	double targ[3],			/* Target of product */
	double mat[3][3],		/* matrix[][] to be quantized */
	double in[3]			/* Input of product - must not be 0.0! */
) {
	int i, j;
	double sum[3];			/* == target */
	double tmp[3];			/* Uncorrected sum */

//	printf("In     = %.8f %.8f %.8f\n",in[0], in[1], in[2]);
//	printf("Target = %.8f %.8f %.8f\n",targ[0], targ[1], targ[2]);

	for (j = 0; j < 3; j++)
		sum[j] = targ[j];

	/* Pre-quantize the matrix, and then ensure that the */
	/* sum of the product of the quantized values is the quantized */
	/* sum by assigning the rounding error to the largest component. */
	for (i = 0; i < 3; i++) {
		int bix = 0;
		double bval = -1e9;

		/* locate the largest and quantize each matrix component */
		for (j = 0; j < 3; j++) {
			if (fabs(mat[i][j]) > bval) {	/* Locate largest */
				bix = j;
				bval = fabs(mat[i][j]);
			} 
			mat[i][j] = round_S15Fixed16Number(mat[i][j]);
		}

		/* Check the product of the uncorrected quantized values */
		tmp[i] = 0.0;
		for (j = 0; j < 3; j++)
			tmp[i] += mat[i][j] * in[j];

		/* Compute the value the largest has to be */
		/* to ensure that sum of the quantized mat[][] times in[] is */
		/* equal to the quantized sum. */
		for (j = 0; j < 3; j++) {
			if (j == bix)
				continue;
			sum[i] -= mat[i][j] * in[j];
		}
		mat[i][bix] = round_S15Fixed16Number(sum[i]/in[i]);

		/* Check the product of the corrected quantized values */
		sum[i] = 0.0;
		for (j = 0; j < 3; j++)
			sum[i] += mat[i][j] * in[j];
	}
//	printf("Q Sum     = %.8f %.8f %.8f\n",tmp[0], tmp[1], tmp[2]);
//	printf("Q cor Sum = %.8f %.8f %.8f\n",sum[0], sum[1], sum[2]);
}

/* Pre-round RGB device primary values to ensure that */
/* the sum of the quantized primaries is the same as */
/* the quantized sum. */
/* [Note matrix is transposed compared to quantize3x3S15Fixed16() ] */  
void quantizeRGBprimsS15Fixed16(
	double mat[3][3]		/* matrix[RGB][XYZ] */
) {
	int i, j;
	double sum[3];

//	printf("D50 = %f %f %f\n",icmD50.X, icmD50.Y, icmD50.Z);

	/* Compute target sum of primary XYZ */
	for (i = 0; i < 3; i++) {
		sum[i] = 0.0;
		for (j = 0; j < 3; j++)
			sum[i] += mat[j][i];
	}
//	printf("Sum = %f %f %f\n",sum[0], sum[1], sum[2]);

	/* Pre-quantize the primary XYZ's, and then ensure that the */
	/* sum of the quantized values is the quantized sum by assigning */
	/* the rounding error to the largest component. */
	for (i = 0; i < 3; i++) {
		int bix = 0;
		double bval = -1e9;

		/* locate the largest and quantize each component */
		for (j = 0; j < 3; j++) {
			if (fabs(mat[j][i]) > bval) {	/* Locate largest */
				bix = j;
				bval = fabs(mat[j][i]);
			} 
			mat[j][i] = round_S15Fixed16Number(mat[j][i]);
		}

		/* Compute the value the largest has to be */
		/* to ensure that sum of the quantized values is */
		/* equal to the quantized sum */
		for (j = 0; j < 3; j++) {
			if (j == bix)
				continue;
			sum[i] -= mat[j][i];
		}
		mat[bix][i] = round_S15Fixed16Number(sum[i]);

		/* Check the sum of the quantized values */
//		sum[i] = 0.0;
//		for (j = 0; j < 3; j++)
//			sum[i] += mat[j][i];
	}
//	printf("Q cor Sum = %f %f %f\n",sum[0], sum[1], sum[2]);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Some PCS utility functions */

/* Clip Lab, while maintaining hue angle. */
/* Return nz if clipping occured */
int icmClipLab(double out[3], double in[3]) {
	double C;

	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];

	if (out[0] >= 0.0 && out[0] <= 100.0
	 && out[1] >= -128.0 && out[1] <= 127.0
	 && out[2] >= -128.0 && out[2] <= 127.0)
		return 0;

	/* Clip L */
	if (out[0] < 0.0)
		out[0] = 0.0;
	else if (out[0] > 100.0)
		out[0] = 100.0;
	
	C = out[1];
	if (fabs(out[2]) > fabs(C))
		C = out[2];
	if (C < -128.0 || C > 127.0) {
		if (C < 0.0)
			C = -128.0/C;
		else
			C = 127.0/C;
		out[1] *= C;
		out[2] *= C;
	}

	return 1;
}

/* Clip XYZ, while maintaining hue angle */
/* Return nz if clipping occured */
int icmClipXYZ(double out[3], double in[3]) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];

	if (out[0] >= 0.0 && out[0] <= 1.9999
	 && out[1] >= 0.0 && out[1] <= 1.9999
	 && out[2] >= 0.0 && out[2] <= 1.9999)
		return 0;

	/* Clip Y and scale X and Z similarly */

	if (out[1] > 1.9999) {
		out[0] *= 1.9999/out[1];
		out[2] *= 1.9999/out[1];
		out[1] = 1.9999;
	} else if (out[1] < 0.0) {
		out[0] = 0.0;
		out[1] = 0.0;
		out[2] = 0.0;
	}

	if (out[0] < 0.0 || out[0] > 1.9999
	 || out[2] < 0.0 || out[2] > 1.9999) {
		double D50[3] = { 0.9642, 1.0000, 0.8249 };
		double bb = 0.0;

		/* Scale the D50 so that it has the same Y value as our color */
		D50[0] *= out[1];
		D50[1] *= out[1];
		D50[2] *= out[1];

		/* Figure out what blend factor with Y scaled D50, brings our */
		/* color X and Z values into range */

		if (out[0] < 0.0) {
			double b;
			b = (0.0 - out[0])/(D50[0] - out[0]);
			if (b > bb)
				bb = b;
		} else if (out[0] > 1.9999) {
			double b;
			b = (1.9999 - out[0])/(D50[0] - out[0]);
			if (b > bb)
				bb = b;
		}
		if (out[2] < 0.0) {
			double b;
			b = (0.0 - out[2])/(D50[2] - out[2]);
			if (b > bb)
				bb = b;
		} else if (out[2] > 1.9999) {
			double b;
			b = (1.9999 - out[2])/(D50[2] - out[2]);
			if (b > bb)
				bb = b;
		}
		/* Do the desaturation */
		out[0] = D50[0] * bb + (1.0 - bb) * out[0];
		out[2] = D50[2] * bb + (1.0 - bb) * out[2];
	}
	return 1;
}

/* --------------------------------------------------------------- */
/* Some video specific functions */

/* Should add ST.2048 log functions */

/* Convert Lut table index/value to YPbPr */
/* (Same as Lut_Lut2YPbPr() ) */ 
void icmLut2YPbPr(double *out, double *in) {
	out[0] = in[0];			/* Y */
	out[1] = in[1] - 0.5;	/* Cb */
	out[2] = in[2] - 0.5;	/* Cr */
}

/* Convert YPbPr to Lut table index/value */
/* (Same as Lut_YPbPr2Lut() ) */ 
void icmYPbPr2Lut(double *out, double *in) {
	out[0] = in[0];			/* Y */
	out[1] = in[1] + 0.5;	/* Cb */
	out[2] = in[2] + 0.5;	/* Cr */
}

/* Convert Rec601 RGB' into YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
/* [From the Rec601 spec. ] */
void icmRec601_RGBd_2_YPbPr(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 0.299 * in[0] + 0.587 * in[1] + 0.114 * in[2];

	tt[1] =     -0.299 /1.772 * in[0]
	      +     -0.587 /1.772 * in[1]
          + (1.0-0.114)/1.772 * in[2];

	tt[2] = (1.0-0.299)/1.402 * in[0]
	      +     -0.587 /1.402 * in[1]
          +     -0.114 /1.402 * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec601 YPbPr to RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
/* [Inverse of above] */
void icmRec601_YPbPr_2_RGBd(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 1.000000000 * in[0] +  0.000000000 * in[1] +  1.402000000 * in[2];
	tt[1] = 1.000000000 * in[0] + -0.344136286 * in[1] + -0.714136286 * in[2];
	tt[2] = 1.000000000 * in[0] +  1.772000000 * in[1] +  0.000000000 * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}


/* Convert Rec709 1150/60/2:1 RGB' into YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
/* (This is for digital Rec709 and is very close to analog  Rec709 60Hz.) */
/* [From the Rec709 specification] */
void icmRec709_RGBd_2_YPbPr(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 0.2126 * in[0] + 0.7152 * in[1] + 0.0722 * in[2];

	tt[1] = 1.0/1.8556 * -0.2126      * in[0]
	      + 1.0/1.8556 * -0.7152      * in[1]
          + 1.0/1.8556 * (1.0-0.0722) * in[2];

	tt[2] = 1.0/1.5748 * (1.0-0.2126) * in[0]
	      + 1.0/1.5748 * -0.7152      * in[1]
          + 1.0/1.5748 * -0.0722      * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec709 1150/60/2:1 YPbPr to RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
/* (This is for digital Rec709 and is very close to analog  Rec709 60Hz.) */
/* [Inverse of above] */
void icmRec709_YPbPr_2_RGBd(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 1.000000000 * in[0] +  0.000000000 * in[1] +  1.574800000 * in[2];
	tt[1] = 1.000000000 * in[0] + -0.187324273 * in[1] + -0.468124273 * in[2];
	tt[2] = 1.000000000 * in[0] +  1.855600000 * in[1] +  0.000000000 * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec709 1250/50/2:1 RGB' into YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
/* (This is for analog Rec709 50Hz) */
/* [From the Rec709 specification] */
void icmRec709_50_RGBd_2_YPbPr(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 0.299 * in[0] + 0.587 * in[1] + 0.114 * in[2];

	tt[1] = 0.564 * -0.299      * in[0]
	      + 0.564 * -0.587      * in[1]
          + 0.564 * (1.0-0.114) * in[2];

	tt[2] = 0.713 * (1.0-0.299) * in[0]
	      + 0.713 * -0.587      * in[1]
          + 0.713 * -0.114      * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec709 1250/50/2:1 YPbPr to RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
/* (This is for analog Rec709 50Hz) */
/* [Inverse of above] */
void icmRec709_50_YPbPr_2_RGBd(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 1.000000000 * in[0] +  0.000000000 * in[1] +  1.402524544 * in[2];
	tt[1] = 1.000000000 * in[0] + -0.344340136 * in[1] + -0.714403473 * in[2];
	tt[2] = 1.000000000 * in[0] +  1.773049645 * in[1] +  0.000000000 * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}


/* Convert Rec2020 RGB' into Non-constant luminance YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
/* [From the Rec2020 specification] */
void icmRec2020_NCL_RGBd_2_YPbPr(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 0.2627 * in[0] + 0.6780 * in[1] + 0.0593 * in[2];

	tt[1] = 1/1.8814 * -0.2627      * in[0]
	      + 1/1.8814 * -0.6780      * in[1]
          + 1/1.8814 * (1.0-0.0593) * in[2];

	tt[2] = 1/1.4746 * (1.0-0.2627) * in[0]
	      + 1/1.4746 * -0.6780      * in[1]
          + 1/1.4746 * -0.0593      * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec2020 Non-constant luminance YPbPr into RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
/* [Inverse of above] */
void icmRec2020_NCL_YPbPr_2_RGBd(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 1.000000000 * in[0] +  0.000000000 * in[1] +  1.474600000 * in[2];
	tt[1] = 1.000000000 * in[0] + -0.164553127 * in[1] + -0.571353127 * in[2];
	tt[2] = 1.000000000 * in[0] +  1.881400000 * in[1] +  0.000000000 * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec2020 RGB' into Constant luminance YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
/* [From the Rec2020 specification] */
void icmRec2020_CL_RGBd_2_YPbPr(double out[3], double in[3]) {
	int i;
	double tt[3];

	/* Convert RGB' to RGB */
	for (i = 0; i < 3; i++) {
		if (in[i] < (4.5 * 0.0181))
			tt[i] = in[i]/4.5;
		else
			tt[i] = pow((in[i] + 0.0993)/1.0993, 1.0/0.45);
	}

	/* Y value */
	tt[0] = 0.2627 * tt[0] + 0.6780 * tt[1] + 0.0593 * tt[2];

	/* Y' value */
	if (tt[0] < 0.0181)
		tt[0] = tt[0] * 4.5;
	else
		tt[0] = 1.0993 * pow(tt[0], 0.45) - 0.0993;

	tt[1] = in[2] - tt[0];
	if (tt[1] <= 0.0)
		tt[1] /= 1.9404;
	else
		tt[1] /= 1.5816;

	tt[2] = in[0] - tt[0];
	if (tt[2] <= 0.0)
		tt[2] /= 1.7184;
	else
		tt[2] /= 0.9936;

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec2020 Constant luminance YPbPr into RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
/* [Inverse of above] */
void icmRec2020_CL_YPbPr_2_RGBd(double out[3], double in[3]) {
	int i;
	double tin[3], tt[3];

	/* Y' */
	tin[0] = in[0];

	/* B' - Y' */
	if (in[1] <= 0.0)
		tin[1] = 1.9404 * in[1];
	else
		tin[1] = 1.5816 * in[1];

	/* R' - Y' */
	if (in[2] <= 0.0)
		tin[2] = 1.7184 * in[2];
	else
		tin[2] = 0.9936 * in[2];


	/* R' */
	tt[0] = tin[2] + tin[0];

	/* Y' */
	tt[1] = tin[0];

	/* B' */
	tt[2] = tin[1] + tin[0];

	/* Convert RYB' to RYB */
	for (i = 0; i < 3; i++) {
		if (tt[i] < (4.5 * 0.0181))
			tin[i] = tt[i]/4.5;
		else
			tin[i] = pow((tt[i] + 0.0993)/1.0993, 1.0/0.45);
	}
	
	/* G */
	tt[1] = (tin[1] - 0.2627 * tin[0] - 0.0593 * tin[2])/0.6780;

	/* G' */
	if (tt[1] < 0.0181)
		tt[1] = tt[1] * 4.5;
	else
		tt[1] = 1.0993 * pow(tt[1], 0.45) - 0.0993;
	
	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}


/* Convert Rec601/Rec709/Rec2020 YPbPr to YCbCr Video range. */
/* input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, */
/* output 16/255 .. 235/255, 16/255 .. 240/255, 16/255 .. 240/255 */ 
void icmRecXXX_YPbPr_2_YCbCr(double out[3], double in[3]) {
	out[0] = ((235.0 - 16.0) * in[0] + 16.0)/255.0;
	out[1] = ((128.0 - 16.0) * 2.0 * in[1] + 128.0)/255.0;
	out[2] = ((128.0 - 16.0) * 2.0 * in[2] + 128.0)/255.0;
}

/* Convert Rec601/Rec709/Rec2020 Video YCbCr to YPbPr range. */
/* input 16/255 .. 235/255, 16/255 .. 240/255, 16/255 .. 240/255 */ 
/* output 0..1, -0.5 .. 0.5, -0.5 .. 0.5, */
void icmRecXXX_YCbCr_2_YPbPr(double out[3], double in[3]) {
	out[0] = (255.0 * in[0] - 16.0)/(235.0 - 16.0);
	out[1] = (255.0 * in[1] - 128.0)/(2.0 * (128.0 - 16.0));
	out[2] = (255.0 * in[2] - 128.0)/(2.0 * (128.0 - 16.0));
}

/* Convert full range RGB to Video range 16..235 RGB */
void icmRGB_2_VidRGB(double out[3], double in[3]) {
	out[0] = ((235.0 - 16.0) * in[0] + 16.0)/255.0;
	out[1] = ((235.0 - 16.0) * in[1] + 16.0)/255.0;
	out[2] = ((235.0 - 16.0) * in[2] + 16.0)/255.0;
}

/* Convert Video range 16..235 RGB to full range RGB */
/* Return nz if outside RGB range */
void icmVidRGB_2_RGB(double out[3], double in[3]) {
	out[0] = (255.0 * in[0] - 16.0)/(235.0 - 16.0);
	out[1] = (255.0 * in[1] - 16.0)/(235.0 - 16.0);
	out[2] = (255.0 * in[2] - 16.0)/(235.0 - 16.0);
}

/* =============================================================== */
/* PS 3.14-2009, Digital Imaging and Communications in Medicine */
/* (DICOM) Part 14: Grayscale Standard Display Function */

/* JND index value 1..1023 to L 0.05 .. 3993.404 cd/m^2 */
static double icmDICOM_fwd_nl(double jnd) {
	double a = -1.3011877;
	double b = -2.5840191e-2;
	double c =  8.0242636e-2;
	double d = -1.0320229e-1;
	double e =  1.3646699e-1;
	double f =  2.8745620e-2;
	double g = -2.5468404e-2;
	double h = -3.1978977e-3;
	double k =  1.2992634e-4;
	double m =  1.3635334e-3;
	double jj, num, den, rv;

	jj = jnd = log(jnd);

	num = a;
	den = 1.0;
	num += c * jj;
	den += b * jj;
	jj *= jnd;
	num += e * jj;
	den += d * jj;
	jj *= jnd;
	num += g * jj;
	den += f * jj;
	jj *= jnd;
	num += m * jj;
	den += h * jj;
	jj *= jnd;
	den += k * jj;
	
	rv = pow(10.0, num/den);

	return rv;
}

/* JND index value 1..1023 to L 0.05 .. 3993.404 cd/m^2 */
double icmDICOM_fwd(double jnd) {

	if (jnd < 0.5)
		jnd = 0.5;
	if (jnd > 1024.0)
		jnd = 1024.0;

	return icmDICOM_fwd_nl(jnd);
}

/* L 0.05 .. 3993.404 cd/m^2 to JND index value 1..1023 */
/* This is not super accurate -  typically to 0.03 .. 0.1 jne. */
static double icmDICOM_bwd_apx(double L) {
	double A = 71.498068;
	double B = 94.593053;
	double C = 41.912053;
	double D =  9.8247004;
	double E =  0.28175407;
	double F = -1.1878455;
	double G = -0.18014349;
	double H =  0.14710899;
	double I = -0.017046845;
	double rv, LL;

	if (L < 0.049982) {			/* == jnd 0.5 */
		return 0.5;
	}
	if (L > 4019.354716)		/* == jnd 1024 */
		L = 4019.354716;

	LL = L = log10(L);
	rv = A;
	rv += B * LL;
	LL *= L;
	rv += C * LL;
	LL *= L;
	rv += D * LL;
	LL *= L;
	rv += E * LL;
	LL *= L;
	rv += F * LL;
	LL *= L;
	rv += G * LL;
	LL *= L;
	rv += H * LL;
	LL *= L;
	rv += I * LL;

	return rv;
}

/* L 0.05 .. 3993.404 cd/m^2 to JND index value 1..1023 */
/* Polish the aproximate solution twice using Newton's itteration */
double icmDICOM_bwd(double L) {
	double rv, Lc, prv, pLc, de;
	int i;

	if (L < 0.045848) 			/* == jnd 0.5 */
		L = 0.045848;
	if (L > 4019.354716)		/* == jnd 1024 */
		L = 4019.354716;

	/* Approx solution */
	rv = icmDICOM_bwd_apx(L);

	/* Compute aprox derivative */
	Lc = icmDICOM_fwd_nl(rv);

	prv = rv + 0.01;
	pLc = icmDICOM_fwd_nl(prv);

	do {
		de = (rv - prv)/(Lc - pLc);
		prv = rv;
		rv -= (Lc - L) * de;
		pLc = Lc;
		Lc = icmDICOM_fwd_nl(rv);
	} while (fabs(Lc - L) > 1e-8);

	return rv;
}


/* ---------------------------------------------------------- */

/* Convert an angle in radians into chromatic RGB values */
/* in a simple geometric fashion, with 0 = Red. */
void icmRad2RGB(double rgb[3], double ang) {
	double th1 = 1.0/3.0 * 2.0 * M_PI;
	double th2 = 2.0/3.0 * 2.0 * M_PI;
	double bl;

	while (ang < 0.0)
		ang += 2.0 * M_PI; 
	while (ang >= (2.0 * M_PI))
		ang -= 2.0 * M_PI; 

	if (ang < th1) {
		bl = ang/th1;
		rgb[0] = (1.0 - bl);
		rgb[1] = bl;
		rgb[2] = 0.0;

	}  else if (ang < th2) {
		bl = (ang - th1)/th1;
		rgb[0] = 0.0;
		rgb[1] = (1.0 - bl);
		rgb[2] = bl;

	} else {
		bl = (ang - th2)/th1;
		rgb[0] = bl;
		rgb[1] = 0.0;
		rgb[2] = (1.0 - bl);
	}
}

/* ---------------------------------------------------------- */
/* Print an int vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPiv(int di, int *p) {
#   define BUFSZ (MAX_CHAN * 8 * 16)
	char *fmt = "%d";
	static char buf[5][BUFSZ];
	static int ix = 0;
	int brem = BUFSZ;
	int e;
	char *bp;

	if (p == NULL)
		return "(null)";

	if (++ix >= 5)
		ix = 0;
	bp = buf[ix];

	for (e = 0; e < di && brem > 10; e++) {
		int tt;
		if (e > 0)
			*bp++ = ' ', brem--;
		tt = snprintf(bp, brem, fmt, p[e]);
		if (tt < 0 || tt >= brem)
			break;			/* Run out of room... */
		bp += tt;
		brem -= tt;
	}
	return buf[ix];
#   undef BUFSZ
}

/* Print a double color vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPdv(int di, double *p) {
	return icmPdvf(di, NULL, p);
}

/* Print a double color vector to a string with format. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPdvf(int di, char *fmt, double *p) {
#   define NBUF 10
#   define BUFSZ (MAX_CHAN * 16)
	static char buf[NBUF][BUFSZ];
	static int ix = 0;
	int brem = BUFSZ;
	int e;
	char *bp;

	if (p == NULL)
		return "(null)";

	if (fmt == NULL)
		fmt = "%.8f";

	if (++ix >= NBUF)
		ix = 0;
	bp = buf[ix];

	for (e = 0; e < di && brem > 10; e++) {
		int tt;
		if (e > 0)
			*bp++ = ' ', brem--;
		tt = snprintf(bp, brem, fmt, p[e]);
		if (tt < 0 || tt >= brem)
			break;			/* Run out of room... */
		bp += tt;
		brem -= tt;
	}
	return buf[ix];
#   undef BUFSZ
#   undef NBUF
}

/* Print a float color vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPfv(int di, float *p) {
#   define BUFSZ (MAX_CHAN * 8 * 16)
	char *fmt = "%.8f";
	static char buf[5][BUFSZ];
	static int ix = 0;
	int brem = BUFSZ;
	int e;
	char *bp;

	if (p == NULL)
		return "(null)";

	if (++ix >= 5)
		ix = 0;
	bp = buf[ix];

	for (e = 0; e < di && brem > 10; e++) {
		int tt;
		if (e > 0)
			*bp++ = ' ', brem--;
		tt = snprintf(bp, brem, fmt, p[e]);
		if (tt < 0 || tt >= brem)
			break;			/* Run out of room... */
		bp += tt;
		brem -= tt;
	}
	return buf[ix];
#   undef BUFSZ
}

/* Print an XYZ */
/* Returned static buffer is re-used every 5 calls. */
char *icmPXYZ(icmXYZNumber *p) {
	double xyz[3];

	icmXYZ2Ary(xyz, *p);
	return icmPdv(3, xyz);
}

/* Print an 0..1 range XYZ as a D50 Lab string */
/* Returned static buffer is re-used every 5 calls. */
char *icmPLab(double *p) {
	double lab[3];

	icmXYZ2Lab(&icmD50, lab, p);
	return icmPdv(3, lab);
}

/* ---------------------------------------------------------- */
/* Unicode conversions */

/*
 * Copyright 2001-2004 Unicode, Inc.
 * 
 * Disclaimer
 * 
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 * 
 * Limitations on Rights to Redistribute This Code
 * 
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

	Conversions between UTF32, UTF-16, and UTF-8. Source code file.
	Author: Mark E. Davis, 1994.
	Rev History: Rick McGowan, fixes & updates May 2001.
	Sept 2001: fixed const & error conditions per
		mods suggested by S. Parent & A. Lillich.
	June 2002: Tim Dodd added detection and handling of incomplete
		source sequences, enhanced error detection, added casts
		to eliminate compiler warnings.
	July 2003: slight mods to back out aggressive FFFE detection.
	Jan 2004: updated switches in from-UTF8 conversions.
	Oct 2004: updated to use UNI_MAX_LEGAL_UTF32 in UTF-32 conversions.
	Dec 2022 Modified for use in icclib by Graeme W. Gill. 

------------------------------------------------------------------------ */

/* Some fundamental constants */
#define UNI_REPLACEMENT_CHAR (icmUTF32)0x0000FFFD
#define UNI_BOM_CHAR (icmUTF32)0x0000FEFF
#define UNI_MAX_BMP (icmUTF32)0x0000FFFF
#define UNI_MAX_UTF16 (icmUTF32)0x0010FFFF
#define UNI_MAX_UTF32 (icmUTF32)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32 (icmUTF32)0x0010FFFF

static const int halfShift  = 10; /* used for shifting by 10 bits */

static const icmUTF32 halfBase = 0x0010000;
static const icmUTF32 halfMask = 0x3FF;

#define UNI_SUR_HIGH_START  (icmUTF32)0xD800
#define UNI_SUR_HIGH_END	(icmUTF32)0xDBFF
#define UNI_SUR_LOW_START   (icmUTF32)0xDC00
#define UNI_SUR_LOW_END	 	(icmUTF32)0xDFFF

/* --------------------------------------------------------------------- */

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is
 * left as-is for anyone who may want to do such conversion, which was
 * allowed in earlier algorithms.
 */
static const char trailingBytesForUTF8[256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

/* Mask of bits that the extra bytes is needed for */
static const icmUTF32 msbitsTrailingBytesForUTF8[6] = {
	0x0000007f,		/* Bits 0..6 */
	0x00000780,		/* Bits 7..10 */
	0x0000f800,		/* Bits 11..15 */
	0x001f0000,		/* Bits 16..20 */
	0x03e00000,		/* Bits 21..25 */
	0x7c000000		/* Bits 26..30 */
};

/* Mask to leave just payload bits given a UTF-8 byte */
static const ORD8 paylloadMaskForUTF8[256] = {
	0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,
	0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,
	0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,
	0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,
	0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,
	0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,
	0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,
	0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,0x7f,

	0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,
	0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,
	0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,
	0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,

	0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,
	0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,
	0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
	0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x03,0x03,0x03,0x03,0x01,0x01,0x01,0x01
};

/*
 * Magic values subtracted from a buffer value during UTF8 conversion.
 * This table contains as many values as there might be trailing bytes
 * in a UTF-8 sequence.
 */
static const icmUTF32 offsetsFromUTF8[6] = {
	0x00000000UL, 0x00003080UL, 0x000E2080UL, 
	0x03C82080UL, 0xFA082080UL, 0x82082080UL
};

/*
 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
 * into the first byte, depending on how many bytes follow.  There are
 * as many entries in this table as there are UTF-8 sequence types.
 * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
 * for *legal* UTF-8 will be 4 or fewer bytes total.
 */
static const icmUTF8 firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

/* --------------------------------------------------------------------- */

/* Convert ICC buffer of UTF16 to UTF8 using icmSn serialisation. */
/* Input is expected to be nul terminated on the last character. */
/* Checks and repairs UTF16 string to be legal UTF-8. */
/* Output will be nul terminated. */
/* len is number of UTF16 bytes including nul. */
/* Return resulting size in bytes. */
/* out pointer can be NULL. */
/* pillegal can be NULL. */
/* If nonul is nz then UTF16 is not expected to have a nul terminator */
size_t icmUTF16SntoUTF8(icmUTFerr *pillegal, icmUTF8 *out, icmFBuf *bin, size_t len, int nonul) {
	int fix = 1;			/* We always fix */
	icmUTFerr illegal = icmUTF_ok;
	size_t ilen = len;
	icmUTF8 *iout = out;

//printf("\n~1 icmUTF16SntoUTF8: len %d bytes\n",len);

	if (len & 1)
		illegal |= icmUTF_odd_bytes;

	for (;;) {
		unsigned short bytesToWrite = 0;
		icmUTF32 ch;

		if (len < 2) {
			if (!nonul) {
				illegal |= icmUTF_no_nul;		/* No nul terminator before end */
//printf(" ~1 icmUTF_no_nul\n");
			}
			break;
		}

		icmSn_ui_UInt16(bin, &ch);	
		len -= 2;

//printf(" ~1 ch = 0x%x remaining len = %d bytes\n",ch, len);

		if (ch == 0) {
			if (len >= 2) {		/* nul terminator prior to end */
				illegal |= icmUTF_prem_nul;
//printf(" ~1 icmUTF_prem_nul\n");
			}
			if (nonul) {
				illegal |= icmUTF_unex_nul;
			}
			break;
		}

		/* ICC UTF-16 should not have a Byte Order Mark, */
		/* and resulting UTF-8 should not reproduce it. */
		if (ch == UNI_BOM_CHAR && (ilen == (len+2))) {
			illegal |= icmUTF_unn_bom;
//printf(" ~1 icmUTF_unn_bom\n");
			continue;
		}

		/* If we have a surrogate pair, decode them into UTF32 */
		if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
			icmUTF32 ch2;	/* Next UTF16 */

//printf(" ~1 found high surrogate\n");
			if (len < 2) {
				illegal |= icmUTF_bad_surr;
				if (fix) ch = UNI_REPLACEMENT_CHAR;
//printf(" ~1 icmUTF_bad_surr 1\n");
				break;
			}

			icmSn_ui_UInt16(bin, &ch2);	
//printf(" ~1 next ch = 0x%x\n",ch2);

			/* If the next input is the low pair, convert to UTF32. */
			if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
//printf(" ~1 found low surrogate\n");
				ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
					+ (ch2 - UNI_SUR_LOW_START) + halfBase;
				len -= 2;

			/* A stranded surrogate high */
			} else {
				icmSn_rseek(bin, -2);		/* Put back the character */
				illegal |= icmUTF_bad_surr;
				if (fix) ch = UNI_REPLACEMENT_CHAR;
//printf(" ~1 icmUTF_bad_surr 2\n");
			}

		/* An unexpected surrogate low */
		} else if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
//printf(" ~1 found low surrogate\n");
			illegal |= icmUTF_bad_surr;
			if (fix) ch = UNI_REPLACEMENT_CHAR;
//printf(" ~1 icmUTF_bad_surr 3\n");
		}

		/* We have a UTF32 value. Convert to UTF8 */

		if (ch == 0) {
			illegal |= icmUTF_emb_nul;		/* Embedded nul */
			ch = UNI_REPLACEMENT_CHAR;
//printf(" ~1 icmUTF_emb_nul\n");
		}

		/* Figure out how many bytes the result will require */
		if (ch < (icmUTF32)0x80) {			 	bytesToWrite = 1;
		} else if (ch < (icmUTF32)0x800) {	 	bytesToWrite = 2;
		} else if (ch < (icmUTF32)0x10000) {   bytesToWrite = 3;
		} else if (ch < (icmUTF32)0x110000) {  bytesToWrite = 4;
		} else {							bytesToWrite = 3;
			/* Shouldn't happen ? */
			illegal |= icmUTF_bad_surr;
			ch = UNI_REPLACEMENT_CHAR;
//printf(" ~1 icmUTF_bad_surr 4\n");
		}

		if (iout != NULL) {
			const icmUTF32 byteMask = 0xBF;
			const icmUTF32 byteMark = 0x80; 

			out += bytesToWrite;
			switch (bytesToWrite) { /* note: everything falls through. */
				case 4:
					*--out = (icmUTF8)((ch | byteMark) & byteMask); ch >>= 6;
				case 3:
					*--out = (icmUTF8)((ch | byteMark) & byteMask); ch >>= 6;
				case 2:
					*--out = (icmUTF8)((ch | byteMark) & byteMask); ch >>= 6;
				case 1:
					*--out =  (icmUTF8)(ch | firstByteMark[bytesToWrite]);
			}
		}
		out += bytesToWrite;
	}

	if (iout != NULL)
		*out = 0;
	out++;

	if (pillegal != NULL)
		*pillegal = illegal;

	return (out - iout) * sizeof(icmUTF8);
}

/* General convert UTF16 to UTF8. */
/* Repairs UTF16 string to be legal UTF-8. */
/* Input is expected to be nul terminated. */
/* Output will be nul terminated. */
/* Return resulting size in bytes. */
/* out pointer can be NULL to size buffer. */
/* pillegal can be NULL. */
size_t icmUTF16toUTF8(icmUTFerr *pillegal, icmUTF8 *out, icmUTF16 *in) {
	int fix = 1;			/* Always fix */
	icmUTFerr illegal = icmUTF_ok;
	ORD16 *iin = in;
	icmUTF8 *iout = out;

	for (;;) {
		unsigned short bytesToWrite = 0;
		icmUTF32 ch;

		ch = *in++;;
		if (ch == 0) {
			break;
		}

		/* Once in machine representation, a Byte Order Mark is not needed, */
		/* and resulting UTF-8 should not reproduce it. */
		if (ch == UNI_BOM_CHAR && iin == (in-2)) {
			illegal |= icmUTF_unn_bom;
			continue;
		}

		/* If we have a surrogate pair, decode them int UTF32 */
		if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
			icmUTF32 ch2 = 0;	/* Next UTF16 */

			ch2 = *in;

			/* If the next input is the low pair, convert to UTF32. */
			if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
				ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
					+ (ch2 - UNI_SUR_LOW_START) + halfBase;
				in++;

			/* A stranded surrogate high */
			} else {
				illegal |= icmUTF_bad_surr;
				if (fix) ch = UNI_REPLACEMENT_CHAR;
			}

		/* An unexpected surrogate low */
		} else if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
			illegal |= icmUTF_bad_surr;
			if (fix) ch = UNI_REPLACEMENT_CHAR;
		}

		/* We have a UTF32 value. Convert to UTF8 */

		if (ch == 0) {
			illegal |= icmUTF_emb_nul;		/* Embedded nul */
			ch = UNI_REPLACEMENT_CHAR;
		}

		/* Figure out how many bytes the result will require */
		if (ch < (icmUTF32)0x80) {			 	bytesToWrite = 1;
		} else if (ch < (icmUTF32)0x800) {	 	bytesToWrite = 2;
		} else if (ch < (icmUTF32)0x10000) {   bytesToWrite = 3;
		} else if (ch < (icmUTF32)0x110000) {  bytesToWrite = 4;
		} else {							bytesToWrite = 3;
				/* Shouldn't happen ? */
				illegal |= icmUTF_bad_surr;
				ch = UNI_REPLACEMENT_CHAR;
		}

		if (iout != NULL) {
			const icmUTF32 byteMask = 0xBF;
			const icmUTF32 byteMark = 0x80; 

			out += bytesToWrite;
			switch (bytesToWrite) { /* note: everything falls through. */
				case 4:
					*--out = (icmUTF8)((ch | byteMark) & byteMask); ch >>= 6;
				case 3:
					*--out = (icmUTF8)((ch | byteMark) & byteMask); ch >>= 6;
				case 2:
					*--out = (icmUTF8)((ch | byteMark) & byteMask); ch >>= 6;
				case 1:
					*--out =  (icmUTF8)(ch | firstByteMark[bytesToWrite]);
			}
		}
		out += bytesToWrite;
	}

	if (iout != NULL)
		*out = 0;
	out++;

	if (pillegal != NULL)
		*pillegal = illegal;

	return (out - iout) * sizeof(icmUTF8);
}

/* --------------------------------------------------------------------- */

/* Convert UTF-8 to ICC buffer of UTF-16 using icmSn serialisation. */
/* Input must be nul terminated, and so will output. */
/* len is number of UTF8 characters including nul. */
/* Return resulting size in bytes. */
/* bout may be NULL to size buffer. */
/* If nonul is nz then UTF16 is not expected to have a nul terminator */
size_t icmUTF8toUTF16Sn(icmUTFerr *pillegal, icmFBuf *bout, icmUTF8 *in, size_t len, int nonul) {
	icmUTFerr illegal = icmUTF_ok;
	icmUTF16 val;
	size_t osize = 0;

//printf("\n~1 icmUTF8toUTF16Sn:\n");

	if (in == NULL) {
//printf("~1 in == NULL, setting len = 0\n");
		len = 0;
	}

	for (;;) {
		unsigned short extraBytesToRead;
		icmUTF32 ch;

//printf("~1 len %zu\n",len);
		if (len == 0) {
			illegal |= icmUTF_no_nul;		/* No nul terminator before end */
			break;
		}

		ch = *in++;
		len--;

//printf(" ~1 raw ch = 0x%x\n",ch);
		if (ch == 0) {
			break;				/* We're done */
		}

		if ((ch & 0xc0) == 0x80) { 
			illegal |= icmUTF_unex_cont;		/* Unexpected continuation byte */
			ch = UNI_REPLACEMENT_CHAR;

		} else {
			int i;

			extraBytesToRead = trailingBytesForUTF8[ch];

			ch &= paylloadMaskForUTF8[ch];		/* Just the payload bits */

			if (extraBytesToRead > 3)
				illegal |= icmUTF_tmany_cont;		/* Too many continuation bytes */
													/* (we'll decode them anyway) */

			/* Read trailing bytes while checking for nul terminator and bad encoding */
			for (i = 0; i < extraBytesToRead; i++) {
				icmUTF32 tch;

				if (len == 0) {
					illegal |= icmUTF_no_nul;		/* No nul terminator before end */
					break;
				}

				tch = *in++;

				if (tch == 0) {
					illegal |= icmUTF_short_cont;	/* Cut short by end of string */ 
					ch = UNI_REPLACEMENT_CHAR;
					break;
				}

				/* If byte hasn't got continuation code */
				if ((tch & 0xc0) != 0x80) {
					illegal |= icmUTF_short_cont; 	/* Cut short by next non-continuation */
					ch = UNI_REPLACEMENT_CHAR;
					break;
				}

				tch &= 0x3f;		/* Just payload bits */

				ch = (ch << 6) + tch;
			}

			/* See if the encoding is over long */
			if (extraBytesToRead > 0
			 && (ch & msbitsTrailingBytesForUTF8[extraBytesToRead]) == 0) {
				illegal |= icmUTF_overlong;			/* overlong (doesn't affect decoding) */
			}
		}

		if (len == 0
		 && (illegal & icmUTF_no_nul))	/* No nul terminator before end */
			break;

		/* We now have UTF32 values, so convert to UTF16 */

//printf(" ~1 cooked ch = 0x%x\n",ch);

		if (ch == 0) {
			illegal |= icmUTF_emb_nul;		/* Embedded nul */
			ch = UNI_REPLACEMENT_CHAR;
		}

		/* Is a character <= 0xFFFF */
		if (ch <= UNI_MAX_BMP) {

			/* UTF-16 surrogate values are illegal in UTF-32 */
			if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
				illegal |= icmUTF_inv_cdpnt;			/* Bad code point - surrogate */
				if (bout != NULL) {
					val = UNI_REPLACEMENT_CHAR;
					icmSn_us_UInt16(bout, &val);
//printf("  ~1 output 0x%x\n",val);
				}
				osize += 2;

			/* Normal UTF-16 character */
			} else {
				if (bout != NULL) {
					val = (icmUTF16)ch;
					icmSn_us_UInt16(bout, &val);
//printf("  ~1 output 0x%x\n",val);
				}
				osize += 2;
			}

		/* Outside unicode range */
		} else if (ch > UNI_MAX_UTF16) {
			illegal |= icmUTF_ovr_cdpnt;			/* Bad code point - beyond unicode */
			if (bout != NULL) {
				val = UNI_REPLACEMENT_CHAR;
				icmSn_us_UInt16(bout, &val);
//printf("  ~1 output 0x%x\n",val);
			}
			osize += 2;

		/* Is a character in range 0x10000 - 0x10FFFF, */
		/* so output a surrogate pair */
		} else {
			ch -= halfBase;
			if (bout != NULL) {
				val = (icmUTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
				icmSn_us_UInt16(bout, &val);
//printf("  ~1 output 0x%x\n",val);
				val = (icmUTF16)((ch  & halfMask)  + UNI_SUR_LOW_START);
				icmSn_us_UInt16(bout, &val);
//printf("  ~1 output 0x%x\n",val);
			}
			osize += 4;
		}
	}
	if (!nonul) {
		if (bout != NULL) {
			val = 0;
			icmSn_us_UInt16(bout, &val);
//printf("  ~1 output 0x%x\n",val);
		}
		osize += 2;
	}

	if (pillegal != NULL)
		*pillegal = illegal;

//printf("  ~1 returning osize %zu with illegal = 0x%X\n",osize * sizeof(ORD8),illegal);
	return osize * sizeof(ORD8);
}


/* General convert UTF-8 to UTF-16. */
/* Input must be nul terminated, and so will output. */
/* Return resulting size in bytes. */
/* out can be NULL. */
size_t icmUTF8toUTF16(icmUTFerr *pillegal, icmUTF16 *out, icmUTF8 *in) {
	icmUTFerr illegal = icmUTF_ok;
	icmUTF16 *iout = out;

	for (;;) {
		unsigned short extraBytesToRead;
		icmUTF32 ch;

		ch = *in++;
		if (ch == 0)
			break;				/* We're done */

		if ((ch & 0xc0) == 0x80) { 
			illegal |= icmUTF_unex_cont;		/* Unexpected continuation byte */
			ch = UNI_REPLACEMENT_CHAR;

		} else {
			int i;

			extraBytesToRead = trailingBytesForUTF8[ch];
			ch &= paylloadMaskForUTF8[ch];		/* Just the payload bits */

			if (extraBytesToRead > 3)
				illegal |= icmUTF_tmany_cont;		/* Too many extra bytes */
													/* (we'll decode them anyway) */
			/* Read trailing bytes while checking for nul terminator and bad encoding */
			for (i = 0; i < extraBytesToRead; i++) {
				icmUTF32 tch;

				tch = *in++;
				if (tch == 0) {
					illegal |= icmUTF_short_cont;	/* Cut short by end of string */ 
					ch = UNI_REPLACEMENT_CHAR;
					break;
				}

				/* If byte hasn't got continuation code */
				if ((tch & 0xc0) != 0x80) {
					illegal |= icmUTF_short_cont; 	/* Cut short by next non-continuation */
					ch = UNI_REPLACEMENT_CHAR;
					break;
				}

				tch &= 0x3f;		/* Just payload bits */

				ch = (ch << 6) + tch;
			}

			/* See if the encoding is over long */
			if (extraBytesToRead > 0
			 && (ch & msbitsTrailingBytesForUTF8[extraBytesToRead]) == 0) {
				illegal |= icmUTF_overlong;			/* overlong (doesn't affect decoding) */
			}
		}

		/* We now have UTF32 values, so convert to UTF16 */

		if (ch == 0) {
			illegal |= icmUTF_emb_nul;		/* Embedded nul */
			ch = UNI_REPLACEMENT_CHAR;
		}

		/* Is a character <= 0xFFFF */
		if (ch <= UNI_MAX_BMP) {

			/* UTF-16 surrogate values are illegal in UTF-32 */
			if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
				illegal |= icmUTF_inv_cdpnt;			/* Bad code point - surrogate */
				if (iout != NULL)
					*out = UNI_REPLACEMENT_CHAR;
				out++;

			/* Normal UTF-16 character */
			} else {
				if (iout != NULL)
					*out = (icmUTF16)ch;
				out++;
			}

		/* Outside unicode range */
		} else if (ch > UNI_MAX_UTF16) {
			illegal |= icmUTF_ovr_cdpnt;			/* Bad code point - beyond unicode */
			if (iout != NULL)
				*out = UNI_REPLACEMENT_CHAR;
			out++;

		/* Is a character in range 0x10000 - 0x10FFFF, */
		/* so output a surrogate pair */
		} else {
			ch -= halfBase;
			if (iout != NULL) {
				out[0] = (icmUTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
				out[1] = (icmUTF16)((ch  & halfMask)  + UNI_SUR_LOW_START);
			}
			out += 2;
		}
	}
	if (iout != NULL)
		*out = 0;
	out++;

	if (pillegal != NULL)
		*pillegal = illegal;

	return (out - iout) * sizeof(icmUTF16);
}

/* --------------------------------------------------------------------- */

/* Convert UTF-8 to printable ASCII + HTML escapes. */
/* Input must be nul terminated, and so will output. */
/* Return resulting size in bytes (including terminating nul). */
/* *pillegal will have icmUTF_non_ascii set if any non-ASCII found. */
/* All output pointers may be NULL */
size_t icmUTF8toHTMLESC(icmUTFerr *pillegal, char *out, icmUTF8 *in) {
	icmUTFerr illegal = icmUTF_ok;
	int u8 = 0; 
	char *iout = out;

	for (;;) {
		unsigned short extraBytesToRead;
		icmUTF32 ch;

		ch = *in++;

		if (ch == 0)
			break;				/* We're done */

		if ((ch & 0xc0) == 0x80) { 
			illegal |= icmUTF_unex_cont;		/* Unexpected continuation byte */
			ch = UNI_REPLACEMENT_CHAR;

		} else {
			int i;

			extraBytesToRead = trailingBytesForUTF8[ch];
			ch &= paylloadMaskForUTF8[ch];		/* Just the payload bits */

			if (extraBytesToRead > 3)
				illegal |= icmUTF_tmany_cont;		/* Too many extra bytes */
													/* (we'll decode them anyway) */

			/* Read trailing bytes while checking for nul terminator and bad encoding */
			for (i = 0; i < extraBytesToRead; i++) {
				icmUTF32 tch;

				tch = *in++;
				if (tch == 0) {
					illegal |= icmUTF_short_cont;	/* Cut short by end of string */ 
					ch = UNI_REPLACEMENT_CHAR;
					break;
				}

				/* If byte hasn't got continuation code */
				if ((tch & 0xc0) != 0x80) {
					illegal |= icmUTF_short_cont; 	/* Cut short by next non-continuation */
					ch = UNI_REPLACEMENT_CHAR;
					break;
				}

				tch &= 0x3f;		/* Just payload bits */

				ch = (ch << 6) + tch;
			}

			/* See if the encoding is over long */
			if (extraBytesToRead > 0
			 && (ch & msbitsTrailingBytesForUTF8[extraBytesToRead]) == 0) {
				illegal |= icmUTF_overlong;			/* overlong (doesn't affect decoding) */
			}
		}

		/* We now have UTF32 values */

		if (ch > 0x7f)
			illegal |= icmUTF_non_ascii;		/* non-ASCII found */
		
		/* HTML special characters */
		if (ch == '&') {
			if (iout != NULL)
				memmove(out, "&amp", 4);
			out += 4;

		} else if (ch == '<') {
			if (iout != NULL)
				memmove(out, "&lt", 3);
			out += 3;

		} else if (ch == '>') {
			if (iout != NULL)
				memmove(out, "&gt", 3);
			out += 3;

		} else if (ch == '"') {
			if (iout != NULL)
				memmove(out, "&quot", 5);
			out += 5;

		} else if (ch == '\'') { 
			if (iout != NULL)
				memmove(out, "&#39", 4);
			out += 4;

		/* Printable ASCII */
		} else if (ch >= 0x20 && ch < 0x80) {
			if (iout != NULL) *out = ch;
			out++;
			
		/* HTML Excape form "&#DDDD;" */
		} else {
			char buf[17], *bp;

			if (iout != NULL) *out = '&';
			out++;
			if (iout != NULL) *out = '#';
			out++;

			/* Do a simple utoa */
			bp = &buf[17];
			*--bp = '\000';
			if (ch == 0)
    			*--bp = '0';
		    for (; ch; ch /= 10)
				*--bp = '0' + ch % 10;

			/* Copy it out */
			for ( ;*bp != '\000'; bp++) {
				if (iout != NULL) *out = *bp;
				out++;
			}

			if (iout != NULL) *out = ';';
			out++;
		}
	}

	if (iout != NULL)
		*out = 0;
	out++;

	if (pillegal != NULL)
		*pillegal = illegal;

	return (out - iout) * sizeof(char);
}

/* --------------------------------------------------------------------- */

/* LEGACY ?? ~8 */
/* Convert UTF-8 to ASCII. Out of range characters are replaced by '?'. */
/* Input must be nul terminated, and so will output. */
/* Return resulting size in bytes (including terminating nul). */
/* *pillegal will have icmUTF_non_ascii set if any non-ASCII found. */
/* All output pointers may be NULL */
size_t icmUTF8toASCII(icmUTFerr *pillegal, char *out, icmUTF8 *in) {
	icmUTFerr illegal = icmUTF_ok;
	int u8 = 0; 
	char *iout = out;
	char replacement_char = '?';

	for (;;) {
		unsigned short extraBytesToRead;
		icmUTF32 ch;

		ch = *in++;

		if (ch == 0)
			break;				/* We're done */

		if ((ch & 0xc0) == 0x80) { 
			illegal |= icmUTF_unex_cont;		/* Unexpected continuation byte */
			ch = replacement_char;

		} else {
			int i;

			extraBytesToRead = trailingBytesForUTF8[ch];
			ch &= paylloadMaskForUTF8[ch];		/* Just the payload bits */

			if (extraBytesToRead > 3)
				illegal |= icmUTF_tmany_cont;		/* Too many extra bytes */
													/* (we'll decode them anyway) */

			/* Read trailing bytes while checking for nul terminator and bad encoding */
			for (i = 0; i < extraBytesToRead; i++) {
				icmUTF32 tch;

				tch = *in++;
				if (tch == 0) {
					illegal |= icmUTF_short_cont;	/* Cut short by end of string */ 
					ch = replacement_char;
					break;
				}

				/* If byte hasn't got continuation code */
				if ((tch & 0xc0) != 0x80) {
					illegal |= icmUTF_short_cont; 	/* Cut short by next non-continuation */
					ch = replacement_char;
					break;
				}

				tch &= 0x3f;		/* Just payload bits */

				ch = (ch << 6) + tch;
			}

			/* See if the encoding is over long */
			if (extraBytesToRead > 0
			 && (ch & msbitsTrailingBytesForUTF8[extraBytesToRead]) == 0) {
				illegal |= icmUTF_overlong;			/* overlong (doesn't affect decoding) */
			}
		}

		/* We now have UTF32 values */

		if (ch > 0x7f) {
			ch = replacement_char;
			illegal |= icmUTF_non_ascii;		/* non-ASCII found */
		}

		if (iout != NULL) *out = ch;
		out++;
	}

	if (iout != NULL)
		*out = 0;
	out++;

	if (pillegal != NULL)
		*pillegal = illegal;

	return (out - iout) * sizeof(char);
}

/* Convert ICC buffer of ASCIIZ to UTF8 using icmSn serialisation. */
/* Input is expected to be nul terminated on the last character. */
/* ilen is the expected number of ASCIIZ input bytes including nul. */
/* If fxlen is > 0, then the input is from a fixed length buffer of fxlen. */
/* If fxlen is < 0, then the input has a maximum length of -fxlen */
/* (If fxlen != 0 then usually ilen = abs(fxlen)) */
/* Output will be nul terminated. */
/* Return resulting size in bytes. */
/* out pointer can be NULL. */
/* pillegal can be NULL. */
size_t icmASCIIZSntoUTF8(icmUTFerr *pillegal, icmUTF8 *out, icmFBuf *bin, size_t ilen, int fxlen) {
	icmUTFerr illegal = icmUTF_ok;
	char replacement_char = '?';
	icmUTF8 *iout = out;
	size_t remin = ilen;	/* fixed/max length buffer bytes left */
	icmUTF32 ch;

//printf("~1 icmASCIIZSntoUTF8 got ilen %zu fxlen %zu\n",ilen,fxlen);

	if (fxlen > 0) {
		remin = fxlen;
	} else if (fxlen < 0) {
		remin = -fxlen;
	}

	if (fxlen != 0 && ilen > remin) {
		illegal |= icmUTF_ascii_toolong;	/* Can't be longer than fixed/max length */
		ilen = remin;
	}

	for (;;) {

		if (ilen < 1) {
			illegal |= icmUTF_no_nul;		/* No nul terminator before end */
			break;
		}

		icmSn_ui_UInt8(bin, &ch);	
		ilen --;
		if (fxlen != 0)
			remin--;

		if (ch == 0) {		/* Got to the end */
			if (fxlen == 0 && ilen > 0)
				illegal |= icmUTF_prem_nul;
			break;
		}

		/* Make sure this is legal ASCII */
		if (ch > 0x7f) {
			ch = replacement_char;
			illegal |= icmUTF_non_ascii;		/* non-ASCII found */
		}

		/* Since this is ASCII, we don't need any conversion to UTF-8 */
		if (iout != NULL)
			*out = (icmUTF8)ch; 
		out++;
	}

	/* Use up the rest of a fixed buffer */
	while (fxlen > 0 && remin > 0) {
		icmSn_ui_UInt8(bin, &ch);	
		remin--;
	}

	if (iout != NULL)	/* Add nul terminator */
		*out = 0;
	out++;

	if (pillegal != NULL)
		*pillegal = illegal;

//printf("~1 returning %zu\n",(out - iout) * sizeof(icmUTF8));
	return (out - iout) * sizeof(icmUTF8);
}

/* Convert UTF-8 to ICC buffer of ASCIIZ using icmSn serialisation. */
/* Input is expected to be nul terminated, and so will output. */
/* ilen is number of UTF8 characters including nul of the input. */
/* If fxlen is > 0, then the output is to a fixed length buffer of that length, */
/* and will be padded with 0 up to that length. */
/* If fxlen < 0, then the otput has a maxumum length of -fxlen. */
/* Return resulting size in bytes. */
/* bout may be NULL. */
size_t icmUTF8toASCIIZSn(icmUTFerr *pillegal, icmFBuf *bout, icmUTF8 *in, size_t ilen, int fxlen) {
	icmUTFerr illegal = icmUTF_ok;
	icmUTF32 replacement_char = '?';
	ORD8 val;
	size_t osize = 0;
	size_t remout;		/* fixed/max length buffer bytes left */
	icmUTF32 ch;

	if (fxlen > 0) {
		remout = fxlen;
	} else if (fxlen < 0) {
		remout = -fxlen;
	}

	if (in == NULL)
		ilen = 0;

	for (;;) {
		unsigned short extraBytesToRead;

		if (ilen == 0) {
			illegal |= icmUTF_no_nul;		/* No nul terminator before end */
			break;
		}

		ch = *in++;
		ilen--;

		if (ch == 0) {
			if (ilen > 0)
				illegal |= icmUTF_prem_nul;
			break;				/* We're done */
		}

		if ((ch & 0xc0) == 0x80) { 
			illegal |= icmUTF_unex_cont;		/* Unexpected continuation byte */
			ch = replacement_char;

		} else {
			int i;

			extraBytesToRead = trailingBytesForUTF8[ch];

			ch &= paylloadMaskForUTF8[ch];		/* Just the payload bits */

			if (extraBytesToRead > 3)
				illegal |= icmUTF_tmany_cont;		/* Too many continuation bytes */
													/* (we'll decode them anyway) */

			/* Read trailing bytes while checking for nul terminator and bad encoding */
			for (i = 0; i < extraBytesToRead; i++) {
				icmUTF32 tch;

				if (ilen == 0) {
					illegal |= icmUTF_no_nul;		/* No nul terminator before end */
					break;
				}

				tch = *in++;

				if (tch == 0) {
					illegal |= icmUTF_short_cont;	/* Cut short by end of string */ 
					ch = replacement_char;
					break;
				}

				/* If byte hasn't got continuation code */
				if ((tch & 0xc0) != 0x80) {
					illegal |= icmUTF_short_cont; 	/* Cut short by next non-continuation */
					ch = replacement_char;
					break;
				}

				tch &= 0x3f;		/* Just payload bits */

				ch = (ch << 6) + tch;
			}

			/* See if the encoding is over long */
			if (extraBytesToRead > 0
			 && (ch & msbitsTrailingBytesForUTF8[extraBytesToRead]) == 0) {
				illegal |= icmUTF_overlong;			/* overlong (doesn't affect decoding) */
			}
		}

		if (ilen == 0
		 && (illegal & icmUTF_no_nul))	/* No nul terminator before end */
			break;

		/* We now have UTF32 values, so convert to UTF16 */

		if (ch > 0x7f) {
			ch = replacement_char;
			illegal |= icmUTF_non_ascii;		/* non-ASCII found */
		}

		if (fxlen != 0 && remout <= 1) {		/* Won't be room for final nul */
			illegal |= icmUTF_ascii_toolong;	/* Can't be longer than fixed/max buffer */
			break;
		}

		if (bout != NULL) {
			val = (ORD8) ch;
			icmSn_uc_UInt8(bout, &val);
		}
		osize++;
		if (fxlen != 0)
			remout--;
	}

	/* add nul terminator */
	if (bout != NULL) {
		val = 0;
		icmSn_uc_UInt8(bout, &val);
	}
	osize++;
	if (fxlen != 0)
		remout--;

	/* if fixed length, pad */
	while (fxlen > 0 && remout > 0) {
		ch = 0;
		if (bout != NULL) {
			icmSn_ui_UInt8(bout, &ch);
		}
		remout--;
	}

	if (pillegal != NULL)
		*pillegal = illegal;

	return osize * sizeof(ORD8);
}

/* --------------------------------------------------------------------- */
/* ICC ScriptCode support */

/* Convert ICC buffer of ScriptCode to UTF8 using icmSn serialisation. */
/* Input is expected to be nul terminated on the last character. */
/* len is number of ASCIIZ bytes including nul. */
/* len may be 0, in which case there is no ScriptCode */
/* Output will be nul terminated. */
/* Return resulting size in bytes. */
/* out pointer can be NULL. */
/* pillegal can be NULL. */
size_t icmSntoScriptCode(icmUTFerr *pillegal, icmUTF8 *out, icmFBuf *bin, size_t len) {
	icmUTFerr illegal = icmUTF_ok;
	char replacement_char = '?';
	size_t ilen = len;
	icmUTF8 *iout = out;
	icmUTF32 ch;
	size_t remin = 67;

	if (len > remin) {
		illegal |= icmUTF_sc_toolong;		/* Can't be longer than 67 */
		len = remin;
	}

	for (;;) {

		if (len < 1) {
			if (ilen > 0)
				illegal |= icmUTF_no_nul;		/* No nul terminator before end */
			break;
		}

		icmSn_ui_UInt8(bin, &ch);	
		len--;
		remin--;

		if (ch == 0) {
			if (len > 0)
				illegal |= icmUTF_no_nul;
			break;
		}

		if (iout != NULL)
			*out = (icmUTF8)ch; 
		out++;
	}

	/* Use up the rest of the fixed ScriptCode buffer length */
	while (remin > 0) {
		/* Note this won't pick up an un-padded script code when it is */
		/* simply concatenated within a larger tag such as ProfileSequenceDescType */
		if (bin->get_space(bin) == 0) {			/* Wasn't padded */
			illegal |= icmUTF_sc_tooshort;
			break;
		}
		icmSn_ui_UInt8(bin, &ch);		/* Could check if ch == 0 ? */
		remin--;
	}

	if (ilen > 0) {
		/* Add nul */
		if (iout != NULL)
			*out = 0;
		out++;
	}

	if (pillegal != NULL)
		*pillegal = illegal;

	return (out - iout) * sizeof(icmUTF8);
}

/* Convert ScriptCode to ICC buffer of ScriptCode using icmSn serialisation. */
/* Input must be nul terminated, and so will output. */
/* len is number of UTF8 characters including nul. */
/* Input may be NULL and len == 0 in which case there is no ScriptCode */
/* Buffer output will be nul terminated and always 67 bytes */
/* If buffer is NULL or len is 0, the output will be 67 bytes of 0 */ 
/* Return resulting string size in bytes. (file may be larger with padding) */
/* bout may be NULL. */
size_t icmScriptCodetoSn(icmUTFerr *pillegal, icmFBuf *bout, ORD8 *in, size_t len) {
	icmUTFerr illegal = icmUTF_ok;
	icmUTF32 replacement_char = '?';
	ORD8 ch;
	size_t osize = 0;
	size_t remout = 67;		/* Remaining output space */

	if (in == NULL)
		len = 0;
	
	for (;;) {

		if (len == 0) {
			if (in != NULL)
				illegal |= icmUTF_no_nul;		/* No nul terminator before end */
			break;
		}

		ch = *in++;
		len--;

		if (ch == 0)
			break;				/* We're done */

		if (remout <= 1) {	/* No room for final nul */
			illegal |= icmUTF_sc_toolong;		/* Can't be longer than 67 */
			break;
		}

		if (bout != NULL) {
			icmSn_uc_UInt8(bout, &ch);
		}
		osize++;
		remout--;
	}

	if (in != NULL) {
		/* nul terminator */
		if (bout != NULL) {
			ch = 0;
			icmSn_uc_UInt8(bout, &ch);
		}
		osize++;
		remout--;
	}

	/* pad until remout== 0 */
	while (remout > 0) {
		if (bout != NULL) {
			ch = 0;
			icmSn_uc_UInt8(bout, &ch);
		}
		remout--;
	}

	if (pillegal != NULL)
		*pillegal = illegal;

	return osize * sizeof(ORD8);
}

/* --------------------------------------------------------------------- */
/* Convert icmUTFerr to a comma separated list of error descriptions */

struct {
	icmUTFerr mask;
	char *string;

} UTFerrStrings[] = {
	{ icmUTF_emb_nul, 	   "embedded nul" },
	{ icmUTF_no_nul,	   "no nul terminator" },
	{ icmUTF_unex_nul,	   "unexpected nul terminator" },
	{ icmUTF_prem_nul,	   "premature nul terminator" },
	{ icmUTF_bad_surr,	   "bad surrogate" },
	{ icmUTF_unn_bom,	   "unnecessary BOM" },
	{ icmUTF_unex_cont,	   "unexpected continuation byte" },
	{ icmUTF_tmany_cont,   "to many continuation bytes" },
	{ icmUTF_short_cont,   "not enough continuation bytes" },
	{ icmUTF_overlong, 	   "overlong encoding" },
	{ icmUTF_inv_cdpnt,    "invalid codepoint" },
	{ icmUTF_ovr_cdpnt,    "over-range codepoint" },
	{ icmUTF_non_ascii,    "non-ASCII characters" },
	{ icmUTF_ascii_toolong,"ASCII longer than fixed sized buffer" },
	{ icmUTF_sc_tooshort,  "ScriptCode occupies less than 67 bytes" },
	{ icmUTF_sc_toolong,   "ScriptCode longer than 67 bytes" },
	{ 0, NULL },

};

char *icmUTFerr2str(icmUTFerr err) {
	static char buf[400];
	char *bp = buf;
	int ne = 0;
	int i;

	if (err == icmUTF_ok)
		return "ok";

	for (i = 0; UTFerrStrings[i].string != NULL; i++) {
		if (err & UTFerrStrings[i].mask) {
			if (ne)
				bp += sprintf(bp, ", ");
			bp += sprintf(bp, "%s",UTFerrStrings[i].string);
			ne = 1;
		}
	}
	return buf;
}


