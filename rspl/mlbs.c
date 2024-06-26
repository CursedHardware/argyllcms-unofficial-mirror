
/* 
 * Argyll Color Management System
 *
 * Scattered Data Interpolation with multilevel B-splines library.
 * This can be used by rspl, or independently by any other routine.
 *
 * Author: Graeme W. Gill
 * Date:   2001/1/1
 *
 * Copyright 2000 - 2001 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
 * This is from the paper
 * "Scattered Data Interpolation with Multilevel B-Splines"
 * by Seungyong Lee, George Wolberg and Sung Yong Shin,
 * IEEE Transactions on Visualisation and Computer Graphics
 * Vol. 3, No. 3, July-September 1997, pp 228.
 */

/* TTBD:
 *
 * Figure out why the results are rubbish ?
 *
 * Can this be adapted to be adaptive in it smoothness,
 * like the non-linear regularized spline stuff that Don Bone used ?
 *
 * Get rid of error() calls - return status instead
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "numlib.h"
#include "mlbs.h"

#ifndef NUMSUP_H
void error(char *fmt, ...), warning(char *fmt, ...);
#endif

static void delete_mlbs(mlbs *p);
static int lookup_mlbs(mlbs *p, co *c);

/* Allocate a new empty mlbs */
mlbs *alloc_mlbs(
int di,		/* Input dimensionality */
int fdi,	/* Output dimesionality */
int res,	/* Target resolution */
double smf	/* Smoothing factor */
) {
	mlbs *p;
	if ((p = (mlbs *)malloc(sizeof(mlbs))) == NULL)
		error("Malloc mlbs failed");

	p->di = di;
	p->fdi = fdi;
	p->tres = res;
	p->smf = smf;
	p->s = NULL;

	p->lookup = lookup_mlbs;
	p->del =    delete_mlbs;

	return p;
}

static void delete_slbs(slbs *s);

static void delete_mlbs(mlbs *p) {

	if (p != NULL) {
		delete_slbs(p->s);
		free(p);
	}
}

/* Create a new empty slbs */
static slbs *new_slbs(
mlbs *p,	/* Parent mlbs */
int res		/* Resolution of this slbs */
) {
	slbs *s;
	int e, f;
	double *_lat, *lat;	/* Latice base address */
	int ix, oe, oo[MXDI];	/* Neighborhood offset index, counter */

	if ((s = (slbs *)malloc(sizeof(slbs))) == NULL)
		error("Malloc slbs failed");

	s->p = p;
	s->res = res;

	for (s->lsize = p->fdi, s->nsize = 1, e = 0; e < p->di; e++) {
		s->coi[e] = s->lsize;		/* (double) increment in this input dimension */
		s->lsize *= (res + 2);		/* Latice in 1D +/- 1 */
		s->nsize *= 4;				/* Neighborhood of 4 */
	} 

	if ((s->_lat = (double *)malloc(s->lsize * sizeof(double))) == NULL)
		error("Malloc slbs latice failed");

	/* Compute the base address */
	for (s->loff = 0, e = 0; e < p->di; e++) {
		s->loff += s->coi[e];		/* Offset by 1 in each input dimension */
	}
	s->lat = s->_lat + s->loff;

	/* Figure the cell width */
	for (e = 0; e < p->di; e++)
		s->w[e] = (p->h[e] - p->l[e])/(res-1.0);
	
	/* Setup neighborhood cache info */
	if ((s->n = (neigh *)malloc(s->nsize * sizeof(neigh))) == NULL)
		error("Malloc slbs neighborhood failed");

	for (oe = 0; oe < p->di; oe++)
		oo[oe] = 0;

	for(ix = oe = 0; oe < p->di; ix++) {
		int xo;
		for (xo = e = 0; e < p->di; e++) {
			s->n[ix].c[e] = oo[e];
			xo += s->coi[e] * oo[e];	/* Accumulate latice offset */
		}
		s->n[ix].xo = xo;
		s->n[ix].w = 0.0;

		/* Increment destination offset counter */
		for (oe = 0; oe < p->di; oe++) {
			if (++oo[oe] <= 3)		/* Counting from 0 ... 3 */
				break;
			oo[oe] = 0;
		}
	}

	return s;
}

/* Destroy a slbs */
static void delete_slbs(slbs *s) {
	if (s != NULL) {
		free(s->_lat);
		free(s->n);
		free(s);
	}
}

/* Dump the 2D -> 1D contents of an slbs */
static void dump_slbs(slbs *s) {
	int e, f;
	int ce, co[MXDI];	/* latice counter */
	mlbs *p = s->p;		/* Parent object */

	/* Init the counter */
	for (ce = 0; ce < p->di; ce++)
		co[ce] = -1;
	ce = 0;

	f = 0;
	while(ce < p->di) {
		double v;
		int off = 0;				/* Latice offset */
		for (e = 0; e < p->di; e++) {
			off += co[e] * s->coi[e];		/* Accumulate latice offset */
		}
		v = s->lat[off + f];				/* Value of this latice point */

		printf("Latice at [%d][%d] = %f\n",co[1],co[0],v);

		/* Increment the latice counter */
		for (ce = 0; ce < p->di; ce++) {
			if (++co[ce] <= s->res)		/* Counting from -1 ... s->res */
				break;
			co[ce] = -1;
		}
	}
}

/* Initialise an slbs with a linear approximation to the scattered data */
static void linear_slbs(
slbs *s
) {
	int i, e, f;
	mlbs *p = s->p;		/* Parent object */
	double **A;			/* A matrix holding scattered data points */
	double *B;			/* B matrix holding RHS & solution */

	/* Allocate the matricies */
	B = dvector(0, p->npts-1);
	A = dmatrix(0, p->npts-1, 0, p->di);

	/* For each output dimension, solve the linear equation coeficients */
	for (f = 0; f < p->fdi; f++) {
		int ce, co[MXDI];	/* latice counter */

		/* Init A[][] with the scattered data points positions */
		/* Also init B[] with the value for this output dimension */
		for (i = 0; i < p->npts; i++) {
			for (e = 0; e < p->di; e++)
				A[i][e] = p->pts[i].p[e];
			A[i][e] = 1.0;
			B[i] = p->pts[i].v[f];
		}

		/* Solve the equation A.x = b using SVD */
		/* (The w[] values are thresholded for best accuracy) */
		/* Return non-zero if no solution found */
		if (svdsolve(A, B, p->npts, p->di+1) != 0)
			error("SVD least squares failed");
		/* A[][] will have been changed, and B[] holds the p->di+1 coefficients */

		/* Use the coefficients to initialise the slbs values */
		for (ce = 0; ce < p->di; ce++)
			co[ce] = -1;
		ce = 0;

		while(ce < p->di) {
			double v = B[p->di];		/* Constant */
			int off = 0;				/* Latice offset */
			for (e = 0; e < p->di; e++) {
				double lv;
				lv = p->l[e] + s->w[e] * co[e];		/* Input value for this latice location */
				v += B[e] * lv;
				off += co[e] * s->coi[e];			/* Accumulate latice offset */
			}
			s->lat[off + f] = v;					/* Value of this latice point */

			/* Increment the latice counter */
			for (ce = 0; ce < p->di; ce++) {
				if (++co[ce] <= s->res)		/* Counting from -1 ... s->res */
					break;
				co[ce] = -1;
			}
		}
	}
	free_dmatrix(A, 0, p->npts-1, 0, p->di);
	free_dvector(B, 0, p->npts-1);
}

/* Do a latice refinement - upsample the current */
/* source latice to the destination latice. */
static void refine_slbs(
slbs *ds,		/* Destination slbs */
slbs *ss		/* Source slbs */
) {
	mlbs *p = ss->p;	/* Parent object */
	int ce, co[MXDI];	/* Source coordinate counter */
	int six;			/* Source index */
	int dix;			/* destination index */
	static double _wt[5] = { 1.0/8.0, 4.0/8.0, 6.0/8.0, 4.0/8.0, 1.0/8.0 };	
	static double *wt = &_wt[2];	/* 1D Distribution weighting */

	/* Zero the destination latice before accumulating values */
	for (dix = 0; dix < ds->lsize; dix++)
		ds->_lat[dix] = 0.0;

	/* Now for each source latice entry, add weighted portions */
	/* to the associated destination points */

	/* Init the source coordinate counter */
	for (ce = 0; ce < p->di; ce++)
		co[ce] = -1;
	ce = 0;
	six = -ss->loff;

	while(ce < p->di) {
		int oe, oo[MXDI];	/* Destination offset counter */

//printf("Source coord %d %d, offset %d, value %f\n",co[0], co[1], six, ss->lat[six]);
		/* calc destination index, and init offest counter */
		for (dix = oe = 0; oe < p->di; oe++) {
			oo[oe] = -2;
			dix += co[oe] * 2 * ds->coi[oe];	/* Accumulate dest offset */
		}
		oe = 0;

//printf("Dest coord %d %d\n",co[0] * 2, co[1] * 2);
		/* For all the offsets from the destination point */
		while(oe < p->di) {
			int e, f, dixo;		/* Destination index offset */
			double w = 1.0;		/* Weighting */

//printf("dest offset %d %d\n",oo[0], oo[1]);
			/* Compute dest index offset, and check that we are not outside the destination */
			for (dixo = e = 0; e < p->di; e++) {
				int x = co[e] * 2 + oo[e];		/* dest coord */
				dixo += oo[e] * ds->coi[e];		/* Accumulate dest offset */
//printf("x[%d] = %d\n",e, x);
				w *= wt[oo[e]];					/* Compute distribution weighting */
				if (x < -1 || x > ds->res)
					break;						/* No good */
			}
			if (e >= p->di) {		/* We are within the destination latice */
//if ((co[0] * 2 + oo[0]) == 0 && (co[1] * 2 + oo[1]) == 0) {
//printf("Source coord %d %d, offset %d, value %f\n",co[0], co[1], six, ss->lat[six]);
//printf("Dest coord %d %d ix %d, weight %f\n",co[0] * 2 + oo[0], co[1] * 2 + oo[1], dix+dixo, w);
//}
			
				for (f = 0; f < p->fdi; f++) {		/* Distribute weighted values */
					double v = ss->lat[six + f];
//if ((co[0] * 2 + oo[0]) == 0 && (co[1] * 2 + oo[1]) == 0)
//printf("Value being dist %f, weighted value %f\n", v, v * w);
					ds->lat[dix + dixo + f] += v * w;
				}
			}

			/* Increment destination offset counter */
			for (oe = 0; oe < p->di; oe++) {
				if (++oo[oe] <= 2)		/* Counting from -2 ... +2 */
					break;
				oo[oe] = -2;
			}
		}

		/* Increment the source index and coordinat counter */
		six += p->fdi;
		for (ce = 0; ce < p->di; ce++) {
			if (++co[ce] <= ss->res)		/* Counting from -1 ... ss->res */
				break;
			co[ce] = -1;
		}
	}
}

/* Compute the Cubic B-spline weightings for a given t */
void basis(double b[4], double t) {
	double _t3, _t2, _t1, _3t3, _3t2, _3t1, _6t2;

	_t1  = t/6.0;
	_t2  = _t1 * _t1;
	_t3  = _t2 * _t1;
	_3t1 = 3.0 * _t1;
	_3t2 = 3.0 * _t2;
	_3t3 = 3.0 * _t3; 
	_6t2 = 6.0 * _t2;

	b[0] = - _t3 + _3t2 - _3t1 + 1.0/6.0;
	b[1] =  _3t3 - _6t2        + 4.0/6.0;
	b[2] = -_3t3 + _3t2 + _3t1 + 1.0/6.0;
	b[3] =   _t3;
}


/* Improve an slbs to make it closer to the scattered data */
static void improve_slbs(
slbs *s
) {
	int i, e, f;
	mlbs *p = s->p;		/* Parent object */
	double *delta;		/* Delta accumulation */
	double *omega;		/* Omega accumulation */

	/* Allocate temporary accumulation arrays */
	if ((delta = (double *)calloc(sizeof(double), s->lsize)) == NULL)
		error("Malloc slbs temp latice failed");
	delta += s->loff;
	if ((omega = (double *)calloc(sizeof(double), s->lsize)) == NULL)
		error("Malloc slbs temp latice failed");
	omega += s->loff;

	/* For each scattered data point */
	for (i = 0; i < p->npts; i++) {
		int    ix;			/* Latice index of base of neighborhood */
		double b[MXDI][4];	/* B-spline basis factors for each dimension */
		double sws;			/* Sum of all the basis factors squared */
		double ve[MXDO];	/* Current output value error */
		int    nn;			/* Neighbor counter */

		/* Figure out our neighborhood */
		for (ix = e = 0; e < p->di; e++) {
			int x;
			double t, sp, fp;
			sp = (p->pts[i].p[e] - p->l[e])/s->w[e];	/* Scaled position */
			fp = floor(sp);
			x = (int)(fp - 1.0);		/* Grid coordinate */
			ix += s->coi[e] * x;		/* Accume latice offset */
			t = sp - fp;				/* Spline parameter */
			basis(b[e], t);				/* Compute basis function values */
		}

		/* Compute the grid basis weight functions, */
		/* the sum of the weights squared, and the current */
		/* output value estimate. */
		for (f = 0; f < p->fdi; f++)
			ve[f] = p->pts[i].v[f];			/* Target output value */
		for (sws = 0.0, nn = 0; nn < s->nsize; nn++) {
			double w;
			for (w = 1.0, e = 0; e < p->di; e++)
				w *= b[e][s->n[nn].c[e]];
			s->n[nn].w = w;		/* cache weighting */
			sws += w * w;
			for (f = 0; f < p->fdi; f++)
				ve[f] -= w * s->lat[ix + s->n[nn].xo + f];	/* Subtract current aprox value */
		}
//printf("Error at point %d = %f\n",i,ve[0]);
		
		/* Accumulate the delta and omega factors */
		/* for this resolutions improvement. */
		for (nn = 0; nn < s->nsize; nn++) {
			double ws, ww, w = s->n[nn].w;
			int xo = ix + s->n[nn].xo;		/* Latice offset */
			ww = w * w;
			ws = ww * w/sws;				/* Scale factor for delta */
			omega[xo] += ww;				/* Accumulate omega */
			for (f = 0; f < p->fdi; f++)
				delta[xo + f] += ws * ve[f];	/* Accumulate delta */
//printf("Distributing omega %f to %d %d\n",ww,s->n[nn].c[0],s->n[nn].c[1]);
//printf("Distributing delta %f to %d %d\n",ws * ve[0],s->n[nn].c[0],s->n[nn].c[1]);
		}
	}

	omega -= s->loff;	/* Base them back to -1 corner */
	delta -= s->loff;

	/* Go through the delta and omega arrays, */
	/* compute and add the refinements to the current */
	/* B-spline control latice. */
	for (i = 0; i < s->lsize; i++) {
		double om = omega[i];
		if (om != 0.0) {
			for (f = 0; f < p->fdi; f++)
				s->_lat[i] += delta[i + f]/om;
//printf("Adjusting latice index %d by %f to give %f\n",i, delta[i]/om, s->_lat[i]);
		}
	}

	/* Done with temporary arrays */
	free(omega);
	free(delta);
}

/* Return the interpolated value for a given point */
/* Return NZ if input point is out of range */
static int lookup_mlbs(
mlbs *p,
co *c		/* Point to interpolate */
) {
	slbs   *s = p->s;
	int    e, f;
	int    ix;			/* Latice index of base of neighborhood */
	double b[MXDI][4];	/* B-spline basis factors for each dimension */
	int    nn;			/* Neighbor counter */

	/* Figure out our neighborhood */
	for (ix = e = 0; e < p->di; e++) {
		int x;
		double t, sp, fp;
		sp = c->p[e];
		if (sp < p->l[e] || sp > p->h[e])
			return 1;
		sp = (sp - p->l[e])/s->w[e];	/* Scaled position */
		fp = floor(sp);
		x = (int)(fp - 1.0);		/* Grid coordinate */
		ix += s->coi[e] * x;		/* Accume latice offset */
		t = sp - fp;				/* Spline parameter */
		basis(b[e], t);				/* Compute basis function values */
	}

	/* Compute the the current output value. */
	for (f = 0; f < p->fdi; f++)
		c->v[f] = 0.0;
	for (nn = 0; nn < s->nsize; nn++) {
		double w;
		for (w = 1.0, e = 0; e < p->di; e++)
			w *= b[e][s->n[nn].c[e]];
		for (f = 0; f < p->fdi; f++)
			c->v[f] += w * s->lat[ix + s->n[nn].xo + f];	/* Accume spline value */
	}
		
	return 0;
}

/* Take a list of scattered data points, */
/* and setup the mlbs. */
static void set_mlbs(
mlbs *p,		/* mlbs to set up */
dpnts  *pts,		/* scattered data points and weights */
int  npts,		/* number of scattered data points */
double *l,		/* Input data range, low  (May be NULL) */
double *h		/* Input data range, high (May be NULL) */
) {
	int res;
	int i, e, f;
	slbs *s0 = NULL, *s1;

	/* Establish the input data range */
	for (e = 0; e < p->di; e++) {
		if (l == NULL)
			p->l[e] = 1e60;
		else
			p->l[e] = l[e];
		if (h == NULL)
			p->h[e] = -1e60;
		else
			p->h[e] = h[e];
	}
	for (i = 0; i < npts; i++) {
		for (e = 0; e < p->di; e++) {
			if (pts[i].p[e] < p->l[e])
				p->l[e] = pts[i].p[e];
			if (pts[i].p[e] > p->h[e])
				p->h[e] = pts[i].p[e];
		}
	}

	/* Make point data available during init */
	p->pts  = pts;
	p->npts = npts;

	/* Create an initial slbs */
	res = 2;
	if ((s1 = new_slbs(p, 2)) == NULL) 
		error("new_slbs failed");

	/* Set it up with a linear first approximation */
	linear_slbs(s1);
//dump_slbs(s1);

	/* Build up the resolution */
	for (; res < p->tres;) {  

		res = 2 * res -1;

printf("~1 doing resolution %d\n",res);
		delete_slbs(s0);
		s0 = s1;
		if ((s1 = new_slbs(p, res)) == NULL) 
			error("new_slbs failed");

		refine_slbs(s1, s0);
//dump_slbs(s1);
		improve_slbs(s1);
	}

	delete_slbs(s0);
	p->s = s1;		/* Final resolution */

	/* We can't assume point data will stick around */
	p->pts  = NULL;
	p->npts = 0;
}


/* Create a new empty mlbs */
mlbs *new_mlbs(
int di,		/* Input dimensionality */
int fdi,	/* Output dimesionality */
int res,	/* Minimum final resolution */
dpnts  *pts,	/* scattered data points and weights */
int  npts,	/* number of scattered data points */
double *l,	/* Input data range, low  (May be NULL) */
double *h,	/* Input data range, high (May be NULL) */
double smf	/* Smoothing factor */
)  {
	mlbs *p;

	if ((p = alloc_mlbs(di, fdi, res, smf)) == NULL)
		return p;

	set_mlbs(p, pts, npts, l, h);

	return p;
}

#ifndef NUMSUP_H
/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"stest: Error - ");
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

	fprintf(stderr,"stest: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

#endif /* NUMSUP_H */

