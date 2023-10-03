/* ============================== */
/* icmPe transform implementation */
/* This is #included in icc.c */

#undef DEBUG_GET_LU	/* [und] debug get_lu function */
#undef ALWAYSLUTRACE	/* [und] create all icmLu with trace set */

// ~8 need to check that _init() functions set & check error code ??

/* ---------------------------------------------------------- */
/* icmPe: A base object. (Not really usable as is) */

static icmPe *icc_new_pe(icc *p, icTagTypeSignature ttype, icTagTypeSignature pttype);


/* ---------------------------------------------------------- */
/* icmPeSeq: A sequence of icmPe's. */
/* All classes derived from icmPeSeq can choose to use these */
/* for their lookup methods. */

static icmPe_lurv icmPeSeq_trace_lookup_fwd(icmPeSeq *p, double *out, double *in);
static icmPe_lurv icmPeSeq_trace_lookup_bwd(icmPeSeq *p, double *out, double *in);

static icmPe_lurv icmPeSeq_lookup_fwd(icmPeSeq *p, double *out, double *in) {
	int n, m;
	icmPe_lurv rv = icmPe_lurv_OK;

	if (p->trace > 0)
		return icmPeSeq_trace_lookup_fwd(p, out, in);

	if (p->nncount == 0) {		/* No usable member Pe's */
		if (out != in) {
			if (p->inputChan == 0 || p->outputChan == 0		/* No default set */
			 || p->inputChan != p->outputChan) {
				rv |= icmPe_lurv_cfg;
			/* Implement a NOP */
			} else {
				for (m = 0; m < p->inputChan; m++)
					out[m] = in[m];
			}
		}
	} else {
		double tmp[MAX_CHAN];

		for (m = 0; m < p->inputChan; m++)
			tmp[m] = in[m];
		for (n = 0; n < p->count; n++) {
			if (p->pe[n] != NULL && p->pe[n]->attr.op != icmPeOp_NOP) {
				if (p->pe[n]->lookup_fwd != NULL && p->pe[n]->attr.fwd)
					rv |= p->pe[n]->lookup_fwd(p->pe[n], tmp, tmp);
				else
					rv |= icmPe_lurv_imp;
			}
		}
		for (m = 0; m < p->outputChan; m++)
			out[m] = tmp[m];
	}
	return rv;
}

static icmPe_lurv icmPeSeq_lookup_bwd(icmPeSeq *p, double *out, double *in) {
	int n, m;
	icmPe_lurv rv = icmPe_lurv_OK;

	if (p->trace > 0)
		return icmPeSeq_trace_lookup_bwd(p, out, in);

	if (p->nncount == 0) {		/* No usable member Pe's */
		if (out != in) {
			if (p->inputChan == 0 || p->outputChan == 0		/* No default set */
			 || p->inputChan != p->outputChan) {
				rv |= icmPe_lurv_cfg;
			/* Implement a NOP */
			} else {
				for (m = 0; m < p->inputChan; m++)
					out[m] = in[m];
			}
		}
	} else {
		double tmp[MAX_CHAN];
		for (m = 0; m < p->outputChan; m++)
			tmp[m] = in[m];
		for (n = p->count-1; n >= 0; n--) {
			if (p->pe[n] != NULL && p->pe[n]->attr.op != icmPeOp_NOP) {
				if (p->pe[n]->lookup_bwd != NULL && p->pe[n]->attr.bwd)
					rv |= p->pe[n]->lookup_bwd(p->pe[n], tmp, tmp);
				else
					rv |= icmPe_lurv_imp;
			}
		}
		for (m = 0; m < p->inputChan; m++)
			out[m] = tmp[m];
	}
	return rv;
}

/* Trace versions of the above */

static icmPe_lurv icmPeSeq_trace_lookup_fwd(icmPeSeq *p, double *out, double *in) {
	int n, m;
	icmPe_lurv rv = icmPe_lurv_OK;
	int pad = p->trace > 0 ? p->trace -1 : 0;

	printf(PAD("PeSeq %s fwd, count %d\n"),icmPeSig2str(p->etype), p->count);
	if (p->trace <= 1)
		printf(PAD("  Input %s\n"),icmPdv(p->inputChan, in));

	if (p->nncount == 0) {		/* No usable member Pe's */
		printf(PAD(" (No Pe's in sequence)\n"));
		if (out != in) {
			if (p->inputChan == 0 || p->outputChan == 0		/* No default set */
			 || p->inputChan != p->outputChan) {
				rv |= icmPe_lurv_cfg;
			/* Implement a NOP */
			} else {
				for (m = 0; m < p->inputChan && m < p->outputChan; m++)
					out[m] = in[m];
			}
		}
	} else {
		double tmp[MAX_CHAN];

		for (m = 0; m < p->inputChan; m++)
			tmp[m] = in[m];
		for (n = 0; n < p->count; n++) {
			if (p->pe[n] != NULL && p->pe[n]->attr.op != icmPeOp_NOP) {
				if (p->pe[n]->lookup_fwd != NULL && p->pe[n]->attr.fwd) {
					int ctr = p->pe[n]->trace;
					if (!p->pe[n]->attr.comp)
						printf(PAD(" Pe %s %s:\n"),icmPeSig2str(p->pe[n]->etype), 
						                      p->pe[n]->attr.inv ? "bwd (ivt)" : "fwd");
					p->pe[n]->trace = p->trace + 1;
					rv |= p->pe[n]->lookup_fwd(p->pe[n], tmp, tmp);
					p->pe[n]->trace = ctr;

				} else {
					printf(PAD(" Pe %s (No %s)\n"),icmPeSig2str(p->pe[n]->etype),
						                      p->pe[n]->attr.inv ? "bwd (ivt)" : "fwd");
					rv |= icmPe_lurv_imp;
				}
				if (!p->pe[n]->attr.comp)
					printf(PAD("  Output %s\n"),icmPdv(p->pe[n]->outputChan, tmp));
			} else {
				printf(PAD(" (Pe[%d] is NULL)\n"),n);
			}
		}
		for (m = 0; m < p->outputChan; m++)
			out[m] = tmp[m];
	}
	if (rv != icmPe_lurv_OK)
		printf(PAD(" returning %s\n"),icmPe_lurv2str(rv));
	return rv;
}

static icmPe_lurv icmPeSeq_trace_lookup_bwd(icmPeSeq *p, double *out, double *in) {
	int n, m;
	icmPe_lurv rv = icmPe_lurv_OK;
	int pad = p->trace > 0 ? p->trace -1 : 0;

	printf(PAD("PeSeq %s bwd, count %d:\n"),icmPeSig2str(p->etype), p->count);
	if (p->trace <= 1)
		printf(PAD("  Input %s\n"),icmPdv(p->outputChan, in));

	if (p->nncount == 0) {		/* No usable member Pe's */
		printf(PAD(" (No Pe's in sequence)\n"));
		if (out != in) {
			if (p->inputChan == 0 || p->outputChan == 0		/* No default set */
			 || p->inputChan != p->outputChan) {
				rv |= icmPe_lurv_cfg;
			/* Implement a NOP */
			} else {
				for (m = 0; m < p->inputChan && m < p->outputChan; m++)
					out[m] = in[m];
			}
		}
	} else {
		double tmp[MAX_CHAN];

		for (m = 0; m < p->outputChan; m++)
			tmp[m] = in[m];
		for (n = p->count-1; n >= 0; n--) {
			if (p->pe[n] != NULL && p->pe[n]->attr.op != icmPeOp_NOP) {
				if (p->pe[n]->lookup_bwd != NULL && p->pe[n]->attr.bwd) {
					int ctr = p->pe[n]->trace;
					if (!p->pe[n]->attr.comp)
						printf(PAD(" Pe %s %s:\n"),icmPeSig2str(p->pe[n]->etype),
						                      p->pe[n]->attr.inv ? "fwd (ivt)" : "bwd");
					p->pe[n]->trace = p->trace + 1;
					rv |= p->pe[n]->lookup_bwd(p->pe[n], tmp, tmp);
					p->pe[n]->trace = ctr;

				} else {
					printf(PAD(" Pe %s (No %s)\n"),icmPeSig2str(p->pe[n]->etype),
						                      p->pe[n]->attr.inv ? "fwd (ivt)" : "bwd");
					rv |= icmPe_lurv_imp;
				}
				if (!p->pe[n]->attr.comp)
					printf(PAD("  Output %s\n"),icmPdv(p->pe[n]->inputChan, tmp));
			} else {
				printf(PAD(" (Pe[%d] is NULL))\n"),n);
			}
		}
		for (m = 0; m < p->inputChan; m++)
			out[m] = tmp[m];
	}
	if (rv != icmPe_lurv_OK)
		printf(PAD(" returning %s\n"),icmPe_lurv2str(rv));
	return rv;
}

/* ---------------------------------------------------------- */
/* icmPeCurveSet: A set of N x 1d Curves */

static icmPe_lurv icmPeCurveSet_trace_lookup_fwd(icmPeCurveSet *p, double *out, double *in);
static icmPe_lurv icmPeCurveSet_trace_lookup_bwd(icmPeCurveSet *p, double *out, double *in);

static icmPe_lurv icmPeCurveSet_lookup_fwd(icmPeCurveSet *p, double *out, double *in) {
	unsigned int n;
	icmPe_lurv rv = icmPe_lurv_OK;

	if (p->trace > 0)
		return icmPeCurveSet_trace_lookup_fwd(p, out, in);

	for (n = 0; n < p->inputChan; n++) {
		if (p->pe[n] != NULL && p->pe[n]->lookup_fwd != NULL)
			rv |= p->pe[n]->lookup_fwd(p->pe[n], &out[n], &in[n]);
		else {
			out[n] = in[n];
			rv |= icmPe_lurv_imp;
		}
	}
	return rv;
}

static icmPe_lurv icmPeCurveSet_lookup_bwd(icmPeCurveSet *p, double *out, double *in) {
	unsigned int n;
	icmPe_lurv rv = icmPe_lurv_OK;

	if (p->trace > 0)
		return icmPeCurveSet_trace_lookup_bwd(p, out, in);

	for (n = 0; n < p->inputChan; n++) {
		if (p->pe[n] != NULL && p->pe[n]->lookup_bwd != NULL)
			rv |= p->pe[n]->lookup_bwd(p->pe[n], &out[n], &in[n]);
		else {
			out[n] = in[n];
			rv |= icmPe_lurv_imp;
		}
	}
	return rv;
}

/* Trace versions of the above */
static icmPe_lurv icmPeCurveSet_trace_lookup_fwd(icmPeCurveSet *p, double *out, double *in) {
	unsigned int n;
	icmPe_lurv rv = icmPe_lurv_OK;
	int pad = p->trace > 0 ? p->trace -1 : 0;

	printf(PAD("PeCurveSet fwd, noChan %d\n"),p->inputChan);
	if (p->trace <= 1)
		printf(PAD("  Input %s\n"),icmPdv(p->inputChan, in));

	for (n = 0; n < p->inputChan; n++) {
		if (p->pe[n] != NULL && p->pe[n]->lookup_fwd != NULL) {
			int ctr = p->pe[n]->trace;
			if (!p->pe[n]->attr.comp)
				printf(PAD(" Pe %s %s chan %d:\n"),icmPeSig2str(p->pe[n]->etype),
						                      p->pe[n]->attr.inv ? "bwd" : "fwd", n);
			p->pe[n]->trace = p->trace + 1;
			rv |= p->pe[n]->lookup_fwd(p->pe[n], &out[n], &in[n]);
			p->pe[n]->trace = ctr;

		} else {
			/* Implement a NOP */
			out[n] = in[n];
			rv |= icmPe_lurv_imp;
		}
	}
	printf(PAD("  Output %s\n"),icmPdv(p->outputChan, out));

	return rv;
}

static icmPe_lurv icmPeCurveSet_trace_lookup_bwd(icmPeCurveSet *p, double *out, double *in) {
	unsigned int n;
	icmPe_lurv rv = icmPe_lurv_OK;
	int pad = p->trace > 0 ? p->trace -1 : 0;

	printf(PAD("PeCurveSet bwd, noChan %d\n"),p->inputChan);
	if (p->trace <= 1)
		printf(PAD("  Input %s\n"),icmPdv(p->inputChan, in));

	for (n = 0; n < p->inputChan; n++) {
		if (p->pe[n] != NULL && p->pe[n]->lookup_bwd != NULL) {

			int ctr = p->pe[n]->trace;
			if (!p->pe[n]->attr.comp)
				printf(PAD(" Pe %s %s chan %d:\n"),icmPeSig2str(p->pe[n]->etype),
						                      p->pe[n]->attr.inv ? "fwd" : "bwd", n);
			p->pe[n]->trace = p->trace + 1;
			rv |= p->pe[n]->lookup_bwd(p->pe[n], &out[n], &in[n]);
			p->pe[n]->trace = ctr;

		} else {
			/* Implement a NOP */
			out[n] = in[n];
			rv |= icmPe_lurv_imp;
		}
	}
	printf(PAD("  Output %s\n"),icmPdv(p->outputChan, out));
	return rv;
}

/* ---------------------------------------------------------- */
/* icmPeCurve: A linear/gamma/table curve */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
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

	if (rt->inited) {
		return 0;
	}

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
		goto fail;
	}
	/* Initialize the reverse lookup structures, and get overall min/max */
	if ((rt->rlists = (unsigned int **) icp->al->calloc(icp->al, rt->rsize, sizeof(unsigned int *))) == NULL) {
		goto fail;
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
					goto fail;
				}
				rt->rlists[j][0] = as;
				nf = rt->rlists[j][1] = 2;
			} else {
				as = rt->rlists[j][0];	/* Allocate space for this list */
				nf = rt->rlists[j][1];	/* Next free location in list */
				if (nf >= as) {			/* need to expand space */
					if ((as = sat_mul(as, 2)) == UINT_MAX
					 || ovr_mul(as, sizeof(unsigned int))) {
						goto fail;
					}
					rt->rlists[j] = (unsigned int *) icp->al->realloc(icp->al,rt->rlists[j], as * sizeof(unsigned int));
					if (rt->rlists[j] == NULL) {
						goto fail;
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

  fail:;
	return 2;
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
/* Return 0 on success, 1 if clipping occured */
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

/* - - - - - - - - - - - - - - */
/* Sample curve transform code */

static int icmPeCurve_init(icmPeCurve *p) {

	if (p->inited) {
		return ICM_ERR_OK;
	}
	p->rt.inited = 0;		/* If icmPeCurve not-inited, then rt is not inited */

	/* Set attr. */
	p->attr.op = icmPeOp_perch;
	if (p->ctype == icmCurveLin) {
		p->attr.op = icmPeOp_NOP;

	} else if (p->ctype == icmCurveGamma && p->count == 1
	        && p->data[0] == 1.0) {
		p->attr.op = icmPeOp_NOP;

	} else if ((p->ttype == icSigCurveType || p->ttype == icmSig816Curve)
	        && p->ctype == icmCurveSpec && p->count == 2
	        && p->data[0] == 0.0 && p->data[1] == 1.0) {
		p->attr.op = icmPeOp_NOP;
	}

	/* Setup reverse lookup info */
	if (p->ctype == icmCurveSpec) {
		if (icmTable_setup_bwd(p->icp, &p->rt, p->count, p->data)) {
			return 1;		// ~8
		}
		p->inited = 1;
	}
	return ICM_ERR_OK;
}

static int icmPeCurve_deinit(icmPeCurve *p) {
	if (p->rt.inited) {	
		if (p->ctype == icmCurveSpec)
			icmTable_delete_bwd(p->icp, &p->rt);
	}
	return 0;
}

/* Do a forward lookup through the curve */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static icmPe_lurv icmPeCurve_lookup_fwd(
	icmPeCurve *p,
	double *out,
	double *in
) {
	icmPe_lurv rv = icmPe_lurv_OK;

	if (p->ctype == icmCurveLin) {
		*out = *in;
	} else if (p->ctype == icmCurveGamma) {
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
			rv |= icmPe_lurv_clip;
		} else if (val > inputEnt_1) {
			val = inputEnt_1;
			rv |= icmPe_lurv_clip;
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

/* Do a reverse lookup through the curve */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
/* (Note that clipping means mathematical clipping, and is not */
/*  set just because a device value is out of gamut. */ 
static icmPe_lurv icmPeCurve_lookup_bwd(
	icmPeCurve *p,
	double *out,
	double *in
) {
	icc *icp = p->icp;
	icmPe_lurv rv = icmPe_lurv_OK;

	if (p->ctype == icmCurveLin) {
		*out = *in;
	} else if (p->ctype == icmCurveGamma) {
		double val = *in;
		if (val <= 0.0)
			*out = 0.0;
		else
			*out = pow(val, 1.0/p->data[0]);
	} else { /* Use linear interpolation */
		if (p->inited == 0) {	
			if (icmPeCurve_init(p)) {
				return icmPe_lurv_imp;
			}
		}
		if (icmTable_lookup_bwd(&p->rt, out, in))
			rv |= icmPe_lurv_clip;
	}
	return rv;
}

/* ---------------------------------------------------------- */
/* icmPeMatrix: An N x M + F matrix */

static void mul_matrix(
	double out[MAX_CHAN][MAX_CHAN],	/* Assume no aliases... */
	double in1[MAX_CHAN][MAX_CHAN],
	double in2[MAX_CHAN][MAX_CHAN],
	int n
) {
	int i, j, k;

	for (i = 0;  i < n; i++) {
		for (j = 0;  j < n; j++) {
			out[i][j] = 0.0;
			for (k = 0; k < n; k++) {
				out[i][j] += in1[i][k] * in2[k][j]; 
			}
		}
	}
}

/* Matrix inversion usin Gauss-Jordan elimination with full pivoting. */
/* Return nz on error.  */
static
int invmatrix(
	double out[MAX_CHAN][MAX_CHAN],
	double in[MAX_CHAN][MAX_CHAN],
	int n
) {
	int colixs[MAX_CHAN], rowixs[MAX_CHAN], pivs[MAX_CHAN] = { 0 };
	int i, j, k, kk;
	int cix, rix;
	double lval, ival;

	if (out != in) {
		/* Copy input to output */
		for (i = 0; i < n; i++)
			for (j = 0; j < n; j++)
				out[i][j] = in[i][j];
	}

	/* For each column */
	for (i = 0; i < n; i++) {

		/* Look for the pivot element */
		for (lval = 0.0, j = 0; j < n; j++) {
			if (pivs[j] != 1) {
				for (k = 0; k < n; k++) {
					if (pivs[k] == 0) {
						double aa = fabs(out[j][k]);
						if (aa >= lval) {
							lval = aa;
							rix = j;
							cix = k;
						}
					}
				}
			}
		}
		
		pivs[cix] = 1;

#define DSWAP(a,b) { double tt = (a); (a) = (b); (b) = tt; }

		/* Swap rows to put pivot on the diagonal */
		if (rix != cix) {
			for (k = 0; k < n; k++)
				DSWAP(out[rix][k], out[cix][k])
		}

		/* Divide the pivot row by the pivot element */
		rowixs[i] = rix;
		colixs[i] = cix;
		if (fabs(out[cix][cix]) < ICM_SMALL_NUMBER) {
			return 1;
		}

		ival = 1.0/out[cix][cix];
		out[cix][cix] = 1.0;

		for (k = 0; k < n; k++)
			out[cix][k] *= ival;

		/* Reduce the rows */
		for (kk = 0; kk < n; kk++) {
			if (kk != cix) {
				double val = out[kk][cix];
				out[kk][cix] = 0.0;
				for (k = 0; k < n; k++)
					out[kk][k] -= val * out[cix][k];
			}
		}
	}

	/* Unscramble columns */
	for (kk = n-1; kk >= 0; kk--) {
		if (rowixs[kk] != colixs[kk])
			for (k = 0; k < n; k++)
				DSWAP(out[k][rowixs[kk]], out[k][colixs[kk]])
	}
#undef DSWAP

	/* Polish the inverse slightly */
	{
		double t1[MAX_CHAN][MAX_CHAN];
		double t2[MAX_CHAN][MAX_CHAN];

		for (kk = 0; kk < 2; kk++) {
			mul_matrix(t1, in, out, n);

			for (i = 0; i < n; i++) {
				for (j = 0; j < n; j++) {
					t2[i][j] = out[i][j];
					if (i == j)
						t1[i][j] = 2.0 - t1[i][j];
					else
						t1[i][j] = 0.0 - t1[i][j];
				}
			}
			mul_matrix(out, t2, t1, n);
		}
	}
	return 0;
}

/* Initialise matrix flags and inverse */
static int icmPeMatrix_init(icmPeMatrix *p) {

	if (!p->inited) {
		if (!p->inited) {
			int i, j;

			p->mx_unity = 0;
			p->ct_zero = 0;

			if (p->inputChan == p->outputChan
			 &&	invmatrix(p->imx, p->mx, p->inputChan) == 0) {
				p->ivalid = 1;
				p->attr.bwd = 1;
			} else {
				p->ivalid = 0;
				p->attr.bwd = 0;
			}

			/* Is the matrix unity */
			if (p->outputChan == p->inputChan) {
				for (i = 0; i < p->outputChan; i++) {
					for (j = 0; j < p->inputChan; j++) {
						if (i == j) {
							if (fabs(p->mx[i][j] - 1.0) > ICM_SMALL_NUMBER)
								break;
						} else {
							if (fabs(p->mx[i][j]) > ICM_SMALL_NUMBER)
								break;
						}
					}
					if (j < p->inputChan) {
						break;
					}
				}
				if (i >= p->outputChan)
					p->mx_unity = 1;
			}

			/* Is the constant zero */
			for (i = 0; i < p->outputChan; i++) {
				if (fabs(p->ct[i]) > ICM_SMALL_NUMBER)
					break;
			}
			if (i >= p->outputChan)
				p->ct_zero = 1;

			/* Set attr. */
			if (p->mx_unity && p->ct_zero)
				p->attr.op = icmPeOp_NOP;
			else
				p->attr.op = icmPeOp_matrix;

			p->inited = 1;
		}
	}
	return ICM_ERR_OK;
}


static icmPe_lurv icmPeMatrix_lookup_fwd(
icmPeMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icc *icp = p->icp;
	icmPe_lurv rv = icmPe_lurv_OK;
	unsigned int i, j;
	double tout[MAX_CHAN];

	if (!p->inited) {
		if (icmPeMatrix_init(p))
			return icmPe_lurv_imp;
	}

	for (i = 0; i < p->outputChan; i++) {
		tout[i] = 0.0;
		for (j = 0; j < p->inputChan; j++)
			tout[i] += p->mx[i][j] * in[j];
		tout[i] += p->ct[i];
	}
	for (i = 0; i < p->outputChan; i++)
		out[i] = tout[i];

	return rv;
}

/* Inverse matrix lookup */
static icmPe_lurv icmPeMatrix_lookup_bwd(
icmPeMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icc *icp = p->icp;
	icmPe_lurv rv = icmPe_lurv_OK;

	if (!p->inited) {
		if (icmPeMatrix_init(p)) {
			return icmPe_lurv_imp;
		}
	}

	if (p->ivalid) {
		unsigned int i, j;
		double tin[MAX_CHAN];
	
		for (j = 0; j < p->outputChan; j++)
			tin[j] = in[j] - p->ct[j];

		for (i = 0; i < p->inputChan; i++) {
			out[i] = 0.0;
			for (j = 0; j < p->outputChan; j++)
				out[i] += p->imx[i][j] * tin[j];
		}

	} else {
		rv |= icmPe_lurv_imp;
	}

	return rv;
}

/* ---------------------------------------------------------- */
/* icmPeClut: An N x M cLUT */

/* Initialise dinc and attr. */
static int icmPeClut_init(icmPeClut *p) {

	if (!p->inited) {
		if (!p->inited) {
			int i, j, g;

			/* Compute dimensional increment though clut in doubles. */
			/* Note that first channel varies least rapidly. */
			if (p->inputChan > 0) {
				i = p->inputChan-1;
				p->dinc[i--] = p->outputChan;
				for (; i >= 0; i--)
					p->dinc[i] = p->dinc[i+1] * p->clutPoints[i];
			}

			/* Compute offsets from base of cube to other corners in doubles. */
			for (p->dcube[0] = 0, g = 1, j = 0; j < p->inputChan; j++) {
				for (i = 0; i < g; i++)
					p->dcube[g+i] = p->dcube[i] + p->dinc[j];
				g *= 2;
			}

			/* Setup attr. */
			p->attr.op = icmPeOp_cLUT;

			/* See if the cLUT is a NOP */
			if (p->outputChan == p->inputChan
			&& (p->ttype == icmSig816CLUT
			)) {
				/* Check that all dims are res == 2 */
				for (i = 0; i < p->inputChan; i++) {
					if (p->clutPoints[i] != 2)
						break;
				}
				if (i >= p->inputChan) {	/* Yes */
					/* Check that table values are 0.0 and 1.0 */
					for (i = 0; i < (1 << p->inputChan); i++) {
						double *out = p->clutTable + p->dcube[i];
						for (j = 0; j < p->outputChan; j++) {
							if (out[j] != ((1 << j) & i) ? 1.0 : 0.0)
								break;	/* nope */
						}
						if (j < p->outputChan)
							break;	/* nope */
					}
					if (i  >= (1 << p->inputChan)) {	/* yes, cLUT is a NOP */
						p->attr.op = icmPeOp_NOP;
					}
				}
			}

			p->inited = 1;
		}
	}
	return ICM_ERR_OK;
}

#ifndef NEVER		/* ifdef for DIAGNOSTIC cLut lookup versions */

/* Convert normalized numbers though this Luts multi-dimensional table. */
/* using multi-linear interpolation. */
static icmPe_lurv icmPeClut_lookup_clut_nl(
/* Return 0 on success, 1 if clipping occured, 2 on other error */
icmPeClut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	icc *icp = p->icp;
	icmPe_lurv rv = icmPe_lurv_OK;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	double *gw, GW[1 << 8];		/* weight for each grid cube corner */

	if (p->inputChan <= 8) {
		gw = GW;				/* Use stack allocation */
	} else {
		if ((gw = (double *) icp->al->malloc(icp->al, sat_mul((1 << p->inputChan), sizeof(double)))) == NULL) {
			return icm_err(icp, 2,"icmPeClut_lookup_clut: malloc() failed");
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
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			double clutPoints_1 = (double)(p->clutPoints[e]-1);
			int    clutPoints_2 = p->clutPoints[e]-2;
			unsigned int x;
			double val;

			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= icmPe_lurv_clip;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= icmPe_lurv_clip;
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
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static icmPe_lurv icmPeClut_lookup_clut_sx(
icmPeClut *p,	/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	icmPe_lurv rv = icmPe_lurv_OK;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	int    si[MAX_CHAN];		/* co[] Sort index, [0] = smallest */

	/* We are using a simplex (ie. tetrahedral for 3D input) interpolation. */
	/* This method is more appropriate for XYZ/RGB/CMYK input spaces, */
	/* and uses much fewer node accesses and multiplies than multi-linear interpolation */
	/* as the dimensionality increases. */ 

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int e;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			double clutPoints_1 = (double)(p->clutPoints[e]-1);
			int    clutPoints_2 = p->clutPoints[e]-2;
			unsigned int x;
			double val;
			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= icmPe_lurv_clip;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= icmPe_lurv_clip;
			}
			x = (unsigned int)floor(val);		/* Grid coordinate */
			if (x > clutPoints_2)
				x = clutPoints_2;
			co[e] = val - (double)x;	/* 1.0 - weight */
			gp += x * p->dinc[e];		/* Add index offset for base of cube */
		}
	}
	/* Do insertion sort on coordinates, smallest to largest. */
	/* (Selection sort is generally slower) */
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

#else

/* DIAGNOSTIC VERSION */
/* Convert normalized numbers though this Luts multi-dimensional table. */
/* using multi-linear interpolation. */
static icmPe_lurv icmPeClut_lookup_clut_nl(
/* Return 0 on success, 1 if clipping occured, 2 on other error */
icmPeClut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	icc *icp = p->icp;
	icmPe_lurv rv = icmPe_lurv_OK;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	double *gw, GW[1 << 8];		/* weight for each grid cube corner */

	if (p->inputChan <= 8) {
		gw = GW;				/* Use stack allocation */
	} else {
		if ((gw = (double *) icp->al->malloc(icp->al, sat_mul((1 << p->inputChan), sizeof(double)))) == NULL) {
			return icm_err(icp, 2,"icmPeClut_lookup_clut: malloc() failed");
		}
	}

	printf("lookup_clut_nl input = %s\n",icmPdv(p->inputChan, in));

	/* We are using an multi-linear (ie. Trilinear for 3D input) interpolation. */
	/* The implementation here uses more multiplies that some other schemes, */
	/* (for instance, see "Tri-Linear Interpolation" by Steve Hill, */
	/* Graphics Gems IV, page 521), but has less involved bookeeping, */
	/* needs less local storage for intermediate output values, does fewer */
	/* output and intermediate value reads, and fp multiplies are fast on */
	/* todays processors! */

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int x[MAX_CHAN];
		unsigned int e;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			double clutPoints_1 = (double)(p->clutPoints[e]-1);
			int    clutPoints_2 = p->clutPoints[e]-2;
			double val;

			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= icmPe_lurv_clip;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= icmPe_lurv_clip;
			}
			x[e] = (unsigned int)floor(val);	/* Grid coordinate */
			if (x[e] > clutPoints_2)
				x[e] = clutPoints_2;
			co[e] = val - (double)x[e];	/* 1.0 - weight */
			gp += x[e] * p->dinc[e];		/* Add index offset for base of cube */
		}
		printf(" ix = %s\n",icmPiv(p->inputChan, x));
	}
	printf(" co = %s\n",icmPdv(p->inputChan, in));
	printf(" gp = %ld\n",(long int)(gp - p->clutTable));
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
	printf(" gw = %s\n",icmPdv(1 << p->inputChan, gw));
	/* Now compute the output values */
	{
		int i;
		unsigned int f;
		double w = gw[0];
		double *d = gp + p->dcube[0];
		for (f = 0; f < p->outputChan; f++) {			/* Base of cube */
			out[f] = w * d[f];
			printf("  out[%d] = %f * %f = %f\n",f,w,d[f],out[f]);
		}
		for (i = 1; i < (1 << p->inputChan); i++) {	/* For all other corners of cube */
			w = gw[i];				/* Strength reduce */
			d = gp + p->dcube[i];
			for (f = 0; f < p->outputChan; f++) {
				printf("  out[%d] = %f + %f * %f = %f\n",f,out[f],w,d[f],out[f] + w * d[f]);
				out[f] += w * d[f];
			}
		}
	}
	if (gw != GW)
		icp->al->free(icp->al, (void *)gw);
	return rv;
}

/* DIAGNOSTIC VERSION */
/* Convert normalized numbers though this Luts multi-dimensional table */
/* using simplex interpolation. */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static icmPe_lurv icmPeClut_lookup_clut_sx(
icmPeClut *p,	/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	icmPe_lurv rv = icmPe_lurv_OK;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	int    si[MAX_CHAN];		/* co[] Sort index, [0] = smallest */

	printf("lookup_clut_sx input = %s\n",icmPdv(p->inputChan, in));

	/* We are using a simplex (ie. tetrahedral for 3D input) interpolation. */
	/* This method is more appropriate for XYZ/RGB/CMYK input spaces, */
	/* and uses much fewer node accesses and multiplies than multi-linear interpolation */
	/* as the dimensionality increases. */ 

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int x[MAX_CHAN];
		unsigned int e;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			double clutPoints_1 = (double)(p->clutPoints[e]-1);
			int    clutPoints_2 = p->clutPoints[e]-2;
			double val;
			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= icmPe_lurv_clip;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= icmPe_lurv_clip;
			}
			x[e] = (unsigned int)floor(val);		/* Grid coordinate */
			if (x[e] > clutPoints_2)
				x[e] = clutPoints_2;
			co[e] = val - (double)x[e];	/* 1.0 - weight */
			gp += x[e] * p->dinc[e];		/* Add index offset for base of cube */
		}
		printf(" ix = %s\n",icmPiv(p->inputChan, x));
	}
	printf(" co = %s\n",icmPdv(p->inputChan, in));
	printf(" gp = %ld\n",(long int)(gp - p->clutTable));
	/* Do insertion sort on coordinates, smallest to largest. */
	/* (Selection sort is generally slower) */
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
	printf(" si = %s\n",icmPiv(3, si));
	/* Now compute the weightings, simplex vertices and output values */
	{
		unsigned int e, f;
		double w;		/* Current vertex weight */

		w = 1.0 - co[si[p->inputChan-1]];		/* Vertex at base of cell */
		for (f = 0; f < p->outputChan; f++) {
			out[f] = w * gp[f];
			printf("  out[%d] = %f * %f = %f\n",f,w,gp[f],out[f]);
		}

		for (e = p->inputChan-1; e > 0; e--) {	/* Middle verticies */
			w = co[si[e]] - co[si[e-1]];
			gp += p->dinc[si[e]];				/* Move to top of cell in next largest dimension */
			for (f = 0; f < p->outputChan; f++) {
				printf("  out[%d] = %f + %f * %f = %f\n",f,out[f],w,gp[f],out[f] +  w * gp[f]);
				out[f] += w * gp[f];
			}
		}

		w = co[si[0]];
		gp += p->dinc[si[0]];		/* Far corner from base of cell */
		for (f = 0; f < p->outputChan; f++) {
			printf("  out[%d] = %f + %f * %f = %f\n",f,out[f],w,gp[f],out[f] +  w * gp[f]);
			out[f] += w * gp[f];
		}
	}
	printf("lookup_clut_sx output = %s\n",icmPdv(p->outputChan, out));
	return rv;
}

#endif

/* Clut lookup */
static icmPe_lurv icmPeClut_lookup_fwd(
icmPeClut *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icc *icp = p->icp;

	if (!p->inited) {
		if (icmPeClut_init(p))
			return icmPe_lurv_imp;
	}

	if (p->use_sx)
		return icmPeClut_lookup_clut_sx(p, out, in);
	else
		return icmPeClut_lookup_clut_nl(p, out, in);
}

/* Inverse clut lookup */
static icmPe_lurv icmPeClut_lookup_bwd(
icmPeClut *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	/* No can do... */
	return icmPe_lurv_imp;
}

/* return the locations of the minimum and */
/* maximum values of the given channel, in the clut */
static void icmPeClut_min_max(
	icmPeClut *p,		/* Pointer to Lut object */
	double *minp,		/* Return position of min/max */
	double *maxp,
	int chan			/* Channel, -1 for average of all */
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
				minp[ee] = gc[ee]/(p->clutPoints[ee]-1.0);
		}
		if (v > maxv) {
			maxv = v;
			for (ee = 0; ee < p->inputChan; ee++)
				maxp[ee] = gc[ee]/(p->clutPoints[ee]-1.0);
		}

		/* Increment coord */
		for (e = 0; e < p->inputChan; e++) {
			if (++gc[e] < p->clutPoints[ee])
				break;	/* No carry */
			gc[e] = 0;
		}
	}
}

/* Determine appropriate clut lookup algorithm */
static void icmPeClut_choose_alg(icmPeClut *p, icmLu4Space *lu) {
	int use_sx;				/* -1 = undecided, 0 = N-linear, 1 = Simplex lookup */
	icmCSInfo ini, outi;	/* In and out Lut color spaces */

	lu->native_spaces(lu, &ini, &outi, NULL);

	/* Determine if the input space is "Device" like, */
	/* ie. luminance will be expected to vary most strongly */
	/* with the diagonal change in input coordinates. */
	switch(ini.sig) {

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
		switch(outi.sig) {

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
			p->min_max(p, tout1, tout2, lc);
			
			/* Convert to vector and then calculate normalized */
			/* dot product with diagonal vector (1,1,1...) */
			for (tt = 0.0, n = 0; n < ini.nch; n++) {
				tout1[n] = tout2[n] - tout1[n];
				tt += tout1[n] * tout1[n];
			}
			if (tt > 0.0)
				tt = sqrt(tt);			/* normalizing factor for maximum delta */
			else
				tt = 1.0;				/* Hmm. */
			tt *= sqrt((double)ini.nch);	/* Normalizing factor for diagonal vector */
			for (diag = 0.0, n = 0; n < outi.nch; n++)
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

	p->use_sx = use_sx;
}

/* Return total ink limit and channel maximums. */
static double icmPeClut_get_tac(
	struct _icmPeClut *p,
	double *chmax,					/* device return channel sums. May be NULL */
	icmPe *tail,					/* lu tail transform from clut to device output */
	void (*calfunc)(void *cntx, double *out, double *in),	/* Optional calibration func. */
	void *cntx
) {
	double tac = 0.0;
	double max[MAX_CHAN];			/* Channel maximums */
	int outn;
	int f;
	double *tp;						/* Pointer to grid cube base */

	if (tail)
		outn = tail->outputChan;
	else
		outn = p->outputChan;

	/* Search the lut for the largest values */
	for (f = 0; f < outn; f++)
		max[f] = 0.0;

	for (tp = p->clutTable; tp < (p->clutTable + p->_clutsize); tp += p->outputChan) {
		double tot, vv[MAX_CHAN];
		
		icmCpyN(vv, tp, p->outputChan);

		if (tail != NULL)
			tail->lookup_fwd(tail, vv, tp);		/* Lookup though tail of transform */

		if (calfunc != NULL)
			calfunc(cntx, vv, vv);				/* Apply any device calibration */

		for (tot = 0.0, f = 0; f < outn; f++) {
			tot += vv[f];
			if (vv[f] > max[f])
				max[f] = vv[f];
		}
		if (tot > tac) {
			tac = tot;
		}
	}

	if (chmax != NULL) {
		for (f = 0; f < outn; f++)
			chmax[f] = max[f];
	}

	return tac;
}

/* =========================================================================================== */
/* ICC profile lookup support */
/* =========================================================================================== */

/* Methods common to all non-named transforms (icmLu4Base) : */

/* Given a normalized colorspace sig, return a full range colorspace sig if any. */
static icColorSpaceSignature icmNorm2Sig(
	icc *icp,
	icColorSpaceSignature sig		/* Normalized range Colorspace sig */
) {
	switch ((int)sig) {
		case icmSigXYZ8Data:
		case icmSigXYZ16Data:
			return icSigXYZData;

		case icmSigLab8Data:
		case icmSigLabV2Data:
			return icSigLabData;

		case icmSigLuv16Data:
			return icSigLuvData;

		case icmSigYCbCr16Data:
			return icSigYCbCrData;

		case icmSigYxy16Data:
			return icSigYxyData;
	}

	/* Unknown sig, already full range sig or no normalised sig needed */
	return sig;
}

/* Given an full range colorspace sig, return the normalized colorspace sig if any. */
static icColorSpaceSignature icmSig2NormSig(
	icc *icp,
	icColorSpaceSignature sig,		/* Full range Colorspace sig */
	icmEncLabVer ver,				/* 0 = V2, 2 same as profile */
	icmEncBytes bytes				/* 1 = 8 bit int, 2 = 16 bit int, else 32 bit float */
) {
	if (ver == icmEncProf) {
		ver = icmEncV2;
	}
	if (sig == icSigXYZData) {
		if (bytes == icmEnc8bit)
			return icmSigXYZ8Data;
		else if (bytes == icmEnc16bit)
			return icmSigXYZ16Data;

	} else if (sig == icSigLabData) {
		if (bytes == icmEnc8bit)
			return icmSigLab8Data;
		else if (bytes == icmEnc16bit) {
				return icmSigLabV2Data;
		}
	}

	if (sig == icSigLuvData)
		return icmSigLuv16Data;

	else if (sig == icSigYCbCrData)
		return icmSigYCbCr16Data;

	else if (sig == icSigYxyData)
		return icmSigYxy16Data;

	/* Unknown sig, already normalized sig or no normalised sig needed */
	return sig;
}

/* Given a normalized colorspace sig, return a new normalizing conversion. */
/* Return NULL and icm->e.c set on error. */
static icmPe *new_icmNSig2NormPe(
	icc *icp,
	icColorSpaceSignature *tofrom,	/* Return output space or if invert input space */
	icColorSpaceSignature nsig,		/* Normalized colorspace sig */
	int invert,						/* nz to return norm to full transform */
	int nullnop						/* On NOP return null rather than icmPeNOP */
) {

	/* Full scale PCS target needs no normalisation */
	if (nsig == icSigXYZData
	 || nsig == icSigLabData) {
		if (tofrom != NULL)
			*tofrom = nsig;
		if (!nullnop)
			return new_icmPeNOP(icp, 3);
		return NULL;
	}

	/* PCS normalised target */
	if (nsig == icmSigXYZ8Data) {
		if (tofrom != NULL)
			*tofrom = icSigXYZData;
		return new_icmPeXYZ2XYZ8(icp, invert);

	} else if (nsig == icmSigXYZ16Data) {
		if (tofrom != NULL)
			*tofrom = icSigXYZData;
		return new_icmPeXYZ2XYZ16(icp, invert);

	} else if (nsig == icmSigLab8Data) {
		if (tofrom != NULL)
			*tofrom = icSigLabData;
		return new_icmPeLab2Lab8(icp, invert);

	} else if (nsig == icmSigLabV2Data) {
		if (tofrom != NULL)
			*tofrom = icSigLabData;
		return new_icmPeLab2LabV2(icp, invert);

	}

	/* Undefined device spaces */
	/* These aren't actually specified by ICC, so we punt. */
	if (nsig == icmSigLuv16Data) {
		double min[3] = { 0.0, -128.0, -128.0 };
		double max[3] = { 100.0, 127.0 + 255.0/256.0, 127.0 + 255.0/256.0 };
		if (tofrom != NULL)
			*tofrom = icSigLuvData;
		return new_icmPeGeneric2Norm(icp, 3, min, max, "Luv2Norm", invert);

	} else if (nsig == icmSigYCbCr16Data) {
		double min[3] = { 0.0, -0.5, -0.5 };
		double max[3] = { 1.0, 0.5, 0.5 };
		if (tofrom != NULL)
			*tofrom = icSigYCbCrData;
		return new_icmPeGeneric2Norm(icp, 3, min, max, "YCbCr2Norm", invert);

	} else if (nsig == icmSigYxy16Data) {
		double min[3] = { 0.0, 0.0, 0.0 };
		double max[3] = { 1.0, 1.0, 1.0 };
		if (tofrom != NULL)
			*tofrom = icSigYxyData;
		return new_icmPeGeneric2Norm(icp, 3, min, max, "Yxy2Norm", invert);
	}

	/* If a device space not otherwise handled above, */
	/* then range is 0..1 so return a NOP */
	if (icmCSSig2type(nsig) & CSSigType_DEV) {
		if (tofrom != NULL)
			*tofrom = nsig;
		if (!nullnop)
			return new_icmPeNOP(icp, icmCSSig2nchan(nsig));
		return NULL;
	}

	icm_err(icp, ICM_ERR_UNKNOWN_NORMSIG,"new_icmNSig2NormPe: unhandled sig '%s'",icmColorSpaceSig2str(nsig));

	return NULL;
}

/* Given an full range colorspace sig, return a normalizing conversion. */
/* Return NULL and icm->e.c set on error. */
static icmPe *new_icmSig2NormPe(
	icc *icp,
	icmSnPrim *pt,					/* Return matching serializatio enum */
	icColorSpaceSignature sig,		/* Full range Colorspace sig */
	icmEncLabVer ver,				/* 0 = V2, 2 same as profile */
	icmEncBytes bytes				/* 1 = 8 bit int, 2 = 16 bit int, else 32 bit float */
) {
	icColorSpaceSignature nsig;

	if (pt != NULL) {
		if (bytes == icmEnc8bit)
			*pt = icmSnPrim_d_NFix8; 
		else if (bytes == icmEnc16bit)
			*pt = icmSnPrim_d_NFix16; 
		else
			*pt = icmSnPrim_d_Float32; 
	}
	nsig = icmSig2NormSig(icp, sig, ver, bytes);
	return new_icmNSig2NormPe(icp, NULL, nsig, 0, 0);
}

/* Given a normalization icmPe and a icmSnPrim enum, */
/* serialize the given value. */ 
static void icmSn_d_n_prim(icmFBuf *b, icmPe *npe, icmSnPrim pt, double *p) {
	unsigned int i;
	double tt[MAX_CHAN];

	if (b->op & icmSnSerialise) {
		if (b->op == icmSnWrite)
			npe->lookup_fwd(npe, tt, p);

		for (i = 0; i < npe->inputChan; i++) {
			icmSn_primitive(b, (void *)&tt[i], pt, 0);
		}

		if (b->op == icmSnRead)
			npe->lookup_bwd(npe, p, tt);
	}
}

/* Given a full range colorspace sig, return a default value ranges. */
/* (This is not for normalized ranges) */
/* Return NZ if range is not 0.0 - 1.0 */
int icmGetDefaultFullRange(
	icc *icp,
	double *min, double *max,		/* Return range */
	icColorSpaceSignature sig		/* Full range  colorspace sig */
) {
	int nch, i;

	if (sig == icSigXYZData) {
		min[0] = 0.0; max[0] = 1.0 + 32767.0/32768;
		min[1] = 0.0; max[1] = 1.0 + 32767.0/32768;
		min[2] = 0.0; max[2] = 1.0 + 32767.0/32768;
		return 1;
	}
	if (sig == icSigLabData) {
		min[0] = 0.0;    max[0] = 100.0;
		min[1] = -128.0; max[1] = 128.0;
		min[2] = -128.0; max[2] = 128.0;
		return 1;
	}

	if (sig == icSigLuvData) {
		min[0] = 0.0;    max[0] = 100.0;
		min[1] = -128.0; max[1] = 128.0;
		min[2] = -128.0; max[2] = 128.0;
		return 1;

	} else if (sig == icSigYCbCrData) {
		min[0] = 0.0;  max[0] = 10.0;
		min[1] = -0.5; max[1] = 0.5;
		min[2] = -0.5; max[2] = 0.5;
		return 1;

	} else if (sig == icSigYxyData) {
		min[0] = 0.0; max[0] = 1.0;
		min[1] = 0.0; max[1] = 1.0;
		min[2] = 0.0; max[2] = 1.0;
		return 0;
	}

	nch = icmCSSig2nchan(sig); 
	for (i = 0; i < nch; i++)
		min[i] = 0.0, max[i] = 1.0;
	return 0;
}

/* Given a normalized or full range colorspace sig, return a default value ranges of */
/* the equivalent full range after de-normalization */
/* Return NZ if range is not 0.0 - 1.0 */
static int icmGetDefaultRange(
	icc *icp,
	double *min, double *max,		/* Return range */
	icColorSpaceSignature sig		/* Noramlize or full range colorspace sig */
) {
	icmPe *nfunc;
	int i, nch;

	nfunc = new_icmNSig2NormPe(icp, NULL, sig, 0, 1);

	if (nfunc == NULL)
		 return icmGetDefaultFullRange(icp, min, max, sig);

	nch = icmCSSig2nchan(sig); 
	for (i = 0; i < nch; i++)
		min[i] = 0.0, max[i] = 1.0;

	nfunc->lookup_bwd(nfunc, min, min);
	nfunc->lookup_bwd(nfunc, max, max);
	nfunc->del(nfunc);

	return 1;
}

/* - - - - - - - - - */
/* Create a PCS format conversion sequence. */
/* Nothing will be added if there is no conversion needed. */
/* Return nz on error */
static int create_fmt(
	icmPeContainer *p,				/* Pe to append conversion to */
	icmLu4Space *lu, 					/* Lu this is part of */
	icColorSpaceSignature dstsp,	/* destination space */
	int dsta,						/* destination anbsolute colorimetric flag */
	icColorSpaceSignature srcsp,	/* source space */
	int srca						/* source anbsolute colorimetric flag */
) {
	icc *icp = p->icp;
	icmPe *pe;

//printf("\nEntry: srcsp = %s srca = %d, dstsp = %s dsta = %d\n",icmColorSpaceSig2str(srcsp), srca, icmColorSpaceSig2str(dstsp), dsta);

	for (;;) {
//		printf(" srcsp = %s srca = %d, dstsp = %s dsta = %d\n",icmColorSpaceSig2str(srcsp), srca, icmColorSpaceSig2str(dstsp), dsta);

		if (srcsp == dstsp && srca == dsta) {
			p->init(p);
			return ICM_ERR_OK;
		}

		/* If we have a non-PCS full to norm conversion needed */
		if (!(icmCSSig2type(srcsp) & CSSigType_gPCS)
		 && !(icmCSSig2type(dstsp) & CSSigType_gPCS)
		 &&  (icmCSSig2type(dstsp) & CSSigType_NORM)) {
			icColorSpaceSignature tofrom;
			if ((pe = new_icmNSig2NormPe(icp, &tofrom, dstsp, 0, 1)) != NULL) {
				if (p->append(p, pe))
					return icp->e.c;
				pe->del(pe);		/* Container has taken a reference */
			} else if (icp->e.c != ICM_ERR_OK)
				return icp->e.c;
			if (srcsp != tofrom)		/* Ooops. Shouldn't happen though... */
				return icm_err(p->icp, ICM_ERR_PE_FMT_UNH_DEV, "create_fmt: can't deal with src"
				     " '%s' & dst '%s'", icmColorSpaceSig2str(srcsp), icmColorSpaceSig2str(dstsp));
			srcsp = dstsp;
//			printf(" Added non-PCS full to norm\n");
			continue;
		}

		/* If we have a non-PCS norm to full conversion needed */
		if (!(icmCSSig2type(srcsp) & CSSigType_gPCS)
		 &&  (icmCSSig2type(srcsp) & CSSigType_NORM)
		 && !(icmCSSig2type(dstsp) & CSSigType_gPCS)) {
			icColorSpaceSignature tofrom;
			if ((pe = new_icmNSig2NormPe(icp, &tofrom, srcsp, 1, 1)) != NULL) {
				if (p->append(p, pe))
					return icp->e.c;
				pe->del(pe);		/* Container has taken a reference */
			} else if (icp->e.c != ICM_ERR_OK)
				return icp->e.c;
			srcsp = tofrom;
//			printf(" Added non-PCS norm to full\n");
			continue;
		}

		/* If we're not dealing with a PCS conversion now, it's an error */
		if (!(icmCSSig2type(srcsp) & CSSigType_gPCS)
		 || !(icmCSSig2type(dstsp) & CSSigType_gPCS)) {
			return icm_err(p->icp, ICM_ERR_PE_FMT_UNH_DEV, "create_fmt: can't deal with src '%s' "
			             "& dst '%s'", icmColorSpaceSig2str(srcsp), icmColorSpaceSig2str(dstsp));
		}

		/* If there needs to be an intent change */
		if (srca != dsta) {
			if (srcsp != icSigXYZData) {
				/* Recurse: Create a conversion from the source to XYZ */
//				printf(" Recursing\n");
				if (create_fmt(p, lu, icSigXYZData, 0, srcsp, 0))
					return icp->e.c;
				srcsp = icSigXYZData;
//				printf(" Returned\n");
			}
			/* Add XYZ intent change */
			if ((pe = new_icmPeAbs2Rel(icp, lu, dsta)) == NULL)
				return icp->e.c;
			if (p->append(p, pe))
				return icp->e.c;
			pe->del(pe);		/* Container has taken a reference */
			/* Continue from where we are now */
			dsta = srca;
//			printf(" Added intent change\n");
			continue;
		}
		/* Intent is now correct. */

		/* If source is a normalised PCS */
		if (icmCSSig2type(srcsp) & CSSigType_nPCS) {
			icColorSpaceSignature tofrom;
			if ((pe = new_icmNSig2NormPe(icp, &tofrom, srcsp, 1, 1)) != NULL) {
				if (p->append(p, pe))
					return icp->e.c;
				pe->del(pe);		/* Contain has taken a reference */
			} else if (icp->e.c != ICM_ERR_OK)
				return icp->e.c;
			srcsp = tofrom;
//			printf(" Added PCS norm to full\n");
			continue;
		}
		/* Source is now full range. */

		/* If src and destination are XYZ,Lab */
		if (srcsp == icSigXYZData && (icmCSSig2type(dstsp) & CSSigType_gLab)) {
			if ((pe = new_icmPeXYZ2Lab(icp, lu, 0)) == NULL)
				return icp->e.c;
			if (p->append(p, pe))
				return icp->e.c;
			pe->del(pe);		/* Contain has taken a reference */
			srcsp = icSigLabData;
//			printf(" Added XYZ to Lab\n");
			continue;
		}

		/* If src and destination are Lab,XYZ */
		if (srcsp == icSigLabData && (icmCSSig2type(dstsp) & CSSigType_gXYZ)) {
			if ((pe = new_icmPeXYZ2Lab(icp, lu, 1)) == NULL)
				return icp->e.c;
			if (p->append(p, pe))
				return icp->e.c;
			pe->del(pe);		/* Contain has taken a reference */
			srcsp = icSigXYZData;
//			printf(" Added Lab to XYZ\n");
			continue;
		}
		/* src is now full range same PCS type as destination */

		/* If destination is a normalised PCS */
		if (icmCSSig2type(dstsp) & CSSigType_nPCS) {
			icColorSpaceSignature tofrom;
			if ((pe = new_icmNSig2NormPe(icp, &tofrom, dstsp, 0, 1)) != NULL) {
				if (p->append(p, pe))
					return icp->e.c;
				pe->del(pe);		/* Contain has taken a reference */
			} else if (icp->e.c != ICM_ERR_OK)
				return icp->e.c;
			srcsp = dstsp;
//			printf(" Added PCS full to norm\n");
			continue;
		}
		/* Shouldn't get here ? */
		return icm_err(p->icp, ICM_ERR_PE_FMT_UNEXP_CASE, "create_fmt: unexpected case src '%s' "
		             "& dst '%s'", icmColorSpaceSig2str(srcsp), icmColorSpaceSig2str(dstsp));
	}
	p->init(p);
	return ICM_ERR_OK; 
}


/* - - - - - - - - - */

#define LU4INIT_WH_BK(TTYPE) (int (*)(TTYPE *))

/* Initialise the LU pcswht, white and black points from the ICC tags, */
/* and the corresponding absolute<->relative conversion matrices */
/* return nz on error */
static int icmLu4Init_Wh_bk(
icmLu4Base *lup
) {
	icc *p = lup->icp;

	lup->pcswht = p->header->illuminant;

	return p->get_wb_points(p, NULL, &lup->whitePoint, &lup->blackisassumed, &lup->blackPoint, 
	                           lup->toAbs, lup->fromAbs);
}

#define LU4NATIVESPACES(TTYPE) (void (*)(TTYPE *, icmCSInfo *, icmCSInfo *, icmCSInfo *))

/* Return information about the native lut in/out/pcs colorspaces. */
/* Any pointer may be NULL if value is not to be returned */
static void
icmLu4NativeSpaces(
	icmLu4Base *p,					/* This */
	icmCSInfo *ini,
	icmCSInfo *outi,
	icmCSInfo *pcsi
) {
	if (ini != NULL) {
		*ini = p->ini;
	}

	if (outi != NULL) {
		*outi = p->outi;
	}

	if (pcsi != NULL) {
		*pcsi = p->pcsi;
	}
}

#define LU4SPACES(TTYPE) (void (*)(TTYPE *, 					\
	icmCSInfo *, icmCSInfo *, icmCSInfo *,						\
	icTagSignature *, icRenderingIntent *,						\
	icmLookupFunc *, icmLookupOrder *,							\
	icmPeOp *op, int *can_bwd))

/* Return information about the effective lookup in/out colorspaces, */
/* including allowance for PCS override. */
/* Any pointer may be NULL if value is not to be returned */
static void
icmLu4Spaces(
	icmLu4Base *p,
	icmCSInfo *ini, icmCSInfo *outi, icmCSInfo *pcsi,
	icTagSignature *srctag, icRenderingIntent *intt,
	icmLookupFunc *fnc, icmLookupOrder *ord,
	icmPeOp *op, int *can_bwd
) {

	if (ini != NULL) {
		*ini = p->e_ini;
	}

	if (outi != NULL) {
		*outi = p->e_outi;
	}

	if (pcsi != NULL) {
		*pcsi = p->e_pcsi;
	}

	if (srctag != NULL)
		*srctag = p->sourceTag;

    if (intt != NULL)
		*intt = p->intent;

	if (fnc != NULL)
		*fnc = p->function;

	if (ord != NULL)
		*ord = p->order;

	if (op != NULL)
		*op = p->op;

	if (can_bwd != NULL)
		*can_bwd = p->can_bwd;
}

/* Fake up alg from icmLu4Space attributes */
icmLuAlgType icmSynthAlgType(icmLuBase *pp) {
	icmLuSpace *p = (icmLuSpace *)pp;
	icmCSInfo ini, outi;
	icmLookupFunc fnc;
	int can_bwd;
	icmLuAlgType alg;

	if (p->ttype != icmSpaceLu4Type) {	/* Must be named color ? */
		return icmNamedType;
	}		

	p->spaces(p, &ini, &outi, NULL, NULL, NULL, &fnc, NULL, NULL, &can_bwd); 

	alg = icmLutType;
	if (can_bwd) {
		if (fnc == icmMonoFwdType
		 && p->output_pch->attr.op == icmPeOp_NOP) {
			if (ini.sig == icSigGrayData)
				alg = icmMonoFwdType;
			else if (ini.sig == icSigRgbData
			      || ini.sig == icSigCmyData)
				alg = icmMatrixFwdType;
		} else if (p->input_pch->attr.op == icmPeOp_NOP) {
			if (outi.sig == icSigGrayData)
				alg = icmMonoBwdType;
			else if (outi.sig == icSigRgbData
			      || outi.sig == icSigCmyData)
				alg = icmMatrixBwdType;
		}
	}
	return alg;
}

#define LU4XYZ_REL2ABS(TTYPE) (void (*)(TTYPE *, double *, double *))

/* Relative to Absolute for this WP in XYZ */
static void icmLu4XYZ_Rel2Abs(icmLu4Base *p, double *out, double *in) {
	icmMulBy3x3(out, p->toAbs, in);
}

#define LU4XYZ_ABS2REL(TTYPE) (void (*)(TTYPE *, double *, double *))

/* Absolute to Relative for this WP in XYZ */
static void icmLu4XYZ_Abs2Rel(icmLu4Base *p, double *out, double *in) {
	icmMulBy3x3(out, p->fromAbs, in);
}

/* - - - - - - - - - */

#define LU4WH_BK_POINTS(TTYPE) (int (*)(TTYPE *, double *, double *, double *))

/* Return the pcs, media white and black points in absolute XYZ space. */
/* Note that if not in the icc, the black point will be returned as 0, 0, 0, */
/* and the function will return nz. */ 
/* Any pointer may be NULL if value is not to be returned */
static int icmLu4Wh_bk_points(
icmLu4Base *p,
double *pcs,
double *wht,
double *blk
) {
	if (pcs != NULL) {
		icmXYZ2Ary(pcs, p->pcswht);
	}

	if (wht != NULL) {
		icmXYZ2Ary(wht, p->whitePoint);
	}

	if (blk != NULL) {
		icmXYZ2Ary(blk, p->blackPoint);
	}
	if (p->blackisassumed)
		return 1;
	return 0;
}

#define LU4LU_WH_BK_POINTS(TTYPE) (int (*)(TTYPE *, double *, double *, double *))

/* Get the LU white and black points in LU PCS space, converted to XYZ. */
/* (ie. white and black will be relative if LU is relative intent etc.) */
/* Return nz if the black point is being assumed to be 0,0,0 rather */
/* than being from the tag. */															\
static int icmLu4Lu_wh_bk_points(
icmLu4Base *p,
double *pcs,
double *wht,
double *blk
) {
	if (pcs != NULL) {
		icmXYZ2Ary(wht,p->pcswht);
	}

	if (wht != NULL) {
		icmXYZ2Ary(wht,p->whitePoint);
	}

	if (blk != NULL) {
		icmXYZ2Ary(blk,p->blackPoint);
	}
	if (p->intent != icAbsoluteColorimetric
	 && p->intent != icmAbsolutePerceptual
	 && p->intent != icmAbsoluteSaturation) {
		if (pcs != NULL)
			icmMulBy3x3(pcs, p->fromAbs, pcs);
		if (wht != NULL)
			icmMulBy3x3(wht, p->fromAbs, wht);
		if (blk != NULL)
			icmMulBy3x3(blk, p->fromAbs, blk);
	}
	if (p->blackisassumed)
		return 1;
	return 0;
}

/* - - - - - - - - - */
/* Transform methods */

/* Overall transforms */
static icmPe_lurv icmLu4_lookup_fwd (icmLu4Space *p, double *out, double *in) {
#ifndef CHECK_LOOKUP_PARTS 
	return p->lookup->lookup_fwd(p->lookup, out, in);

#else
#  pragma message("######### icmLu4_lookup_fwd CHECK_LOOKUP_PARTS enabled ########")
	double cin[MAX_CHAN], cout3[MAX_CHAN], cout5[MAX_CHAN];
	double c31[MAX_CHAN], c32[MAX_CHAN];
	double c51[MAX_CHAN], c52[MAX_CHAN];
	icmPe_lurv rv, rv1, rv2, rv3, rv5;
	double err;
	int trace = 0, ltrace = p->lookup->trace;
	int pad = 0;

	if (ltrace) trace = 3;
	p->lookup->trace = trace;
	pad = trace-1;

	if (trace) printf("### single part fwd ##\n");
	if (trace) printf(PAD("Input %s\n"),icmPdv(p->lookup->inputChan, in));
	icmCpyN(cin, in, p->lookup->inputChan);
	rv = p->lookup->lookup_fwd(p->lookup, out, in);

	/* 3 part: */
	if (trace) printf("### 3 part fwd ##\n");
	if (trace) printf(PAD("Input %s\n"),icmPdv(p->input->inputChan, cin));
	p->input->trace = p->core3->trace = p->output->trace = trace;

	if (trace) printf(" ## input\n");
	p->input->lookup_fwd(p->input, c31, cin);
	if (trace) printf(" ## core3\n");
	p->core3->lookup_fwd(p->core3, c32, c31);
	if (trace) printf(" ## output\n");
	p->output->lookup_fwd(p->output, cout3, c32);

	p->input->trace = p->core3->trace = p->output->trace = 0;
	if ((err = icmDiffN(cout3, out, p->output->outputChan)) > 1e-6) {
		printf("!!!!! 3 part fwd mismatch 1 part by %f\n",err);
	}

	/* 5 part: */
	if (trace) printf("### 5 part fwd ##\n");
	if (trace) printf(PAD("Input %s\n"),icmPdv(p->input_fmt->inputChan, cin));
	p->input_fmt->trace = p->input_pch->trace = p->core5->trace
	                    = p->output_pch->trace = p->output_fmt->trace = trace;

	if (trace) printf(" ## input_fmt\n");
	p->input_fmt->lookup_fwd(p->input_fmt, cout5, cin);
	if (trace) printf(" ## input_pch\n");
	p->input_pch->lookup_fwd(p->input_pch, c51, cout5);
	if (trace) printf(" ## core5\n");
	p->core5->lookup_fwd(p->core5, c52, c51);
	if (trace) printf(" ## output_pch\n");
	p->output_pch->lookup_fwd(p->output_pch, cout5, c52);
	if (trace) printf(" ## output_fmt\n");
	p->output_fmt->lookup_fwd(p->output_fmt, cout5, cout5);

	p->input_fmt->trace = p->input_pch->trace = p->core5->trace
	                    = p->output_pch->trace = p->output_fmt->trace = 0;

//	Not a true expectation because core3 can include a format change...
//	if ((err = icmDiffN(c31, c51, p->output->outputChan)) > 1e-6)
//		printf("!!!!! 3 part inter 1 fwd mismatch 5 part inter 1 fwd by %f\n",err);
//	if ((err = icmDiffN(c32, c52, p->output->outputChan)) > 1e-6)
//		printf("!!!!! 3 part inter 2 fwd mismatch 5 part inter 2 fwd by %f\n",err);
	if ((err = icmDiffN(cout5, out, p->output_fmt->outputChan)) > 1e-6)
		printf("!!!!! 5 part fwd mismatch 1 part fwd by %f\n",err);

	p->lookup->trace = ltrace;

	return rv;
#endif
}

static icmPe_lurv icmLu4_lookup_bwd (icmLu4Space *p, double *out, double *in) {
#ifndef CHECK_LOOKUP_PARTS 
	return p->lookup->lookup_bwd(p->lookup, out, in);
#else
#  pragma message("######### icmLu4_lookup_bwd CHECK_LOOKUP_PARTS enabled ########")
	double cin[MAX_CHAN], cout3[MAX_CHAN], cout5[MAX_CHAN];
	double c31[MAX_CHAN], c32[MAX_CHAN];
	double c51[MAX_CHAN], c52[MAX_CHAN];
	icmPe_lurv rv, rv1, rv2, rv3, rv5;
	double err;
	int trace, ltrace = p->lookup->trace;

	if (ltrace) trace = 3;
	p->lookup->trace = trace;

	if (trace) printf("## single part bwd ##\n");
	icmCpyN(cin, in, p->lookup->inputChan);
	rv = p->lookup->lookup_bwd(p->lookup, out, in);

	/* 3 part: */
	if (trace) printf("### 3 part bwd ##\n");
	p->input->trace = p->core3->trace = p->output->trace = trace;

	if (trace) printf(" ## output\n");
	p->output->lookup_bwd(p->output, c31, cin);
	if (trace) printf(" ## core3\n");
	p->core3->lookup_bwd(p->core3, c32, c31);
	if (trace) printf(" ## input\n");
	p->input->lookup_bwd(p->input, cout3, c32);

	p->input->trace = p->core3->trace = p->output->trace = 0;
	if ((err = icmDiffN(cout3, out, p->output->outputChan)) > 1e-6) {
		printf("!!!!! 3 part bwd mismatch 1 part by %f\n",err);
	}

	/* 5 part: */
	if (trace) printf("### 5 part bwd ##\n");
	p->input_fmt->trace = p->input_pch->trace = p->core5->trace
	                    = p->output_pch->trace = p->output_fmt->trace = trace;

	if (trace) printf(" ## output_fmt\n");
	p->output_fmt->lookup_bwd(p->output_fmt, cout5, cin);
	if (trace) printf(" ## output_pch\n");
	p->output_pch->lookup_bwd(p->output_pch, c51, cout5);
	if (trace) printf(" ## core5\n");
	p->core5->lookup_bwd(p->core5, c52, c51);
	if (trace) printf(" ## input_pch\n");
	p->input_pch->lookup_bwd(p->input_pch, cout5, c52);
	if (trace) printf(" ## input_fmt\n");
	p->input_fmt->lookup_bwd(p->input_fmt, cout5, cout5);

	p->input_fmt->trace = p->input_pch->trace = p->core5->trace
	                    = p->output_pch->trace = p->output_fmt->trace = 0;

//	Not a true expectation because core3 can include a format change...
//	if ((err = icmDiffN(c31, c51, p->output->outputChan)) > 1e-6)
//		printf("!!!!! 3 part inter 1 bwd mismatch 5 part inter 1 bwd by %f\n",err);
//	if ((err = icmDiffN(c32, c52, p->output->outputChan)) > 1e-6)
//		printf("!!!!! 3 part inter 2 bwd mismatch 5 part inter 2 bwd by %f\n",err);
	if ((err = icmDiffN(cout5, out, p->output_fmt->outputChan)) > 1e-6)
		printf("!!!!! 5 part bwd mismatch 1 part bwd by %f\n",err);

	p->lookup->trace = ltrace;

	return rv;
#endif
}

	
/* 3 stage components of fwd lookup, in order. */
static icmPe_lurv icmLu4_input_fwd	(icmLu4Space *p, double *out, double *in) {
	return p->input->lookup_fwd(p->input, out, in);
}

static icmPe_lurv icmLu4_core3_fwd	(icmLu4Space *p, double *out, double *in) {
	return p->core3->lookup_fwd(p->core3, out, in);
}

static icmPe_lurv icmLu4_output_fwd	(icmLu4Space *p, double *out, double *in) {
	return p->output->lookup_fwd(p->output, out, in);
}


/* 3 stange components of bwd lookup, in order. */
static icmPe_lurv icmLu4_output_bwd	(icmLu4Space *p, double *out, double *in) {
	return p->output->lookup_bwd(p->output, out, in);
}

static icmPe_lurv icmLu4_core3_bwd	(icmLu4Space *p, double *out, double *in) {
	return p->core3->lookup_bwd(p->core3, out, in);
}

static icmPe_lurv icmLu4_input_bwd	(icmLu4Space *p, double *out, double *in) {
	return p->input->lookup_bwd(p->input, out, in);
}


/* 5 stage components of fwd lookup, in order. */
static icmPe_lurv icmLu4_input_fmt_fwd	(icmLu4Space *p, double *out, double *in) {
	return p->input_fmt->lookup_fwd(p->input_fmt, out, in);
}

static icmPe_lurv icmLu4_input_pch_fwd	(icmLu4Space *p, double *out, double *in) {
	return p->input_pch->lookup_fwd(p->input_pch, out, in);
}

static icmPe_lurv icmLu4_core5_fwd	(icmLu4Space *p, double *out, double *in) {
	return p->core5->lookup_fwd(p->core5, out, in);
}

static icmPe_lurv icmLu4_output_pch_fwd	(icmLu4Space *p, double *out, double *in) {
	return p->output_pch->lookup_fwd(p->output_pch, out, in);
}

static icmPe_lurv icmLu4_output_fmt_fwd	(icmLu4Space *p, double *out, double *in) {
	return p->output_fmt->lookup_fwd(p->output_fmt, out, in);
}


/* 5 stange components of bwd lookup, in order. */
static icmPe_lurv icmLu4_output_fmt_bwd	(icmLu4Space *p, double *out, double *in) {
	return p->output_fmt->lookup_bwd(p->output_fmt, out, in);
}

static icmPe_lurv icmLu4_output_pch_bwd	(icmLu4Space *p, double *out, double *in) {
	return p->output_pch->lookup_bwd(p->output_pch, out, in);
}

static icmPe_lurv icmLu4_core5_bwd	(icmLu4Space *p, double *out, double *in) {
	return p->core5->lookup_bwd(p->core5, out, in);
}

static icmPe_lurv icmLu4_input_pch_bwd	(icmLu4Space *p, double *out, double *in) {
	return p->input_pch->lookup_bwd(p->input_pch, out, in);
}

static icmPe_lurv icmLu4_input_fmt_bwd	(icmLu4Space *p, double *out, double *in) {
	return p->input_fmt->lookup_bwd(p->input_fmt, out, in);
}

static unsigned int icmLu4_max_in_res(icmLu4Space *p, int res[MAX_CHAN]) {
	return p->lookup->max_in_res(p->lookup, res);
}

static unsigned int icmLu4_max_clut_res(icmLu4Space *p, int res[MAX_CHAN]) {
	return p->lookup->max_clut_res(p->lookup, res);
}

static unsigned int icmLu4_max_out_res(icmLu4Space *p, int res[MAX_CHAN]) {
	return p->lookup->max_out_res(p->lookup, res);
}

static int icmLu4_linear_light_inout(icmLu4Space *p, int dir) {
	if (p->pcsi.sig != icSigXYZData)
		return 0;

	return p->lookup->linear_light_inout(p->lookup, dir);
}

static icmPeClut *icmLu4_get_lut(struct _icmLu4Space *p, icmPeContainer **ptail) {
	return p->lookup->get_lut(p->lookup, ptail);
}

static double icmLu4_get_tac(icmLu4Space *p, double *chmax,
	void (*calfunc)(void *cntx, double *out, double *in), void *cntx) {
	return p->lookup->get_tac(p->lookup, chmax, calfunc, cntx);
}


static void icmLu4_del(icmLu4Space *p) {
	if (p != NULL) {
		/* Single */
		if (p->lookup != NULL)
			p->lookup->del(p->lookup);
	
		/* 3 stage */
		if (p->input != NULL)
			p->input->del(p->input);
		if (p->core3 != NULL)
			p->core3->del(p->core3);
		if (p->output != NULL)
			p->output->del(p->output);

		/* 5 stage */
		if (p->input_fmt != NULL)
			p->input_fmt->del(p->input_fmt);
		if (p->input_pch != NULL)
			p->input_pch->del(p->input_pch);
		if (p->core5 != NULL)
			p->core5->del(p->core5);
		if (p->output_pch != NULL)
			p->output_pch->del(p->output_pch);
		if (p->output_fmt != NULL)
			p->output_fmt->del(p->output_fmt);

		p->icp->al->free(p->icp->al, p);
	}
}


/* Return an appropriate lookup object */
/* Return NULL on error, and detailed error in icc */
static icmLu4Base *icc_get_lu4obj (
	icc *icp,					/* ICC */
	icmLookupFunc func,			/* Conversion functionality */
	icRenderingIntent intent,	/* Rendering intent, including icmAbsoluteColorimetricXYZ */
	icColorSpaceSignature pcsor,/* PCS override (0 = def) */
	icmLookupOrder order		/* Conversion representation search Order */
) {
	icmLu4Space *p = NULL;				/* Lookup object to return */
	icmPeSeq *tt = NULL;				/* TagType transform */
	icColorSpaceSignature pcs, e_pcs;	/* Native PCS and effective PCS */
	icColorSpaceSignature e_ins, e_outs;/* Effective (i.e. target) in/out spaces */
	int e_ina = 0, e_outa = 0;			/* Effective (i.e. target) in/out abs intent */
	int inch, outch;					/* Number of input and output channels */

	struct {
		icTagSignature sig;				/* Tag to look for. */
		icColorSpaceSignature in;		/* (possibly normalized) Input space */
		icColorSpaceSignature out;		/* (possibly normalized) Output space */
		int ina;						/* Input abs intent flag */
		int outa;						/* Output abs intent flag */
	} sch[10] = { 0 };				/* Tags to search for, in order */
	int ix = 0, nix, ixi;			/* Index into search table */
	
	icColorSpaceSignature tdev, tpcs;	/* Default transform Device and PCS encoding */

	/* Figure out the native and effective PCS */
	e_pcs = pcs = icp->header->pcs;
	if (pcsor != icmSigDefaultData)
		e_pcs = pcsor;			/* Override */

	/* Default in & out encoding for cLUT */
	tdev = icp->header->colorSpace;
	if (tdev == icSigLabData)
		tdev = icmSigLabV2Data;
	else if (tdev == icSigXYZData)
		tdev = icmSigXYZ16Data ;

	tpcs = icp->header->pcs;
	if (tpcs == icSigLabData)
		tpcs = icmSigLabV2Data;
	else if (tpcs == icSigXYZData)
		tpcs = icmSigXYZ16Data ;


#ifdef DEBUG_GET_LU
	printf("icc_get_lu4obj:\n");
	printf(" LookupFunc = %s\n",icm2str(icmTransformLookupFunc, func));
	printf(" RenderingIntent = %s\n",icm2str(icmRenderingIntent, intent));
	printf(" PCS override = %s\n",icm2str(icmColorSpaceSig, pcsor));
	printf(" LookupOrder = %s\n",icm2str(icmTransformLookupOrder, order));
#endif
	/* How we expect to execute the request depends firstly on the type of profile */
	switch (icp->header->deviceClass) {
    	case icSigInputClass:
    	case icSigDisplayClass:
    	case icSigColorSpaceClass:

			/* Look for Intent based AToBX profile + optional BToAX reverse */
			/* or for AToB0 based profile + optional BToA0 reverse */
			/* or three component matrix profile (reversable) */
			/* or momochrome table profile (reversable) */ 
			/* No Lut intent for ICC < V2.4, but possible for >= V2.4, */
			/* so fall back if we can't find the chosen Lut intent. */
			/* Device <-> PCS */

			switch (func) {

		    	case icmFwd:	/* Device to PCS */
					e_ins = icp->header->colorSpace;
					e_outs = e_pcs;
					if (intent == icmDefaultIntent)
						intent = icp->header->renderingIntent;
					switch ((int)intent) {
		    			case icAbsoluteColorimetric:
							e_outa = 1;
							sch[ix].sig = icSigAToB1Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;

							sch[ix].sig = icSigAToB0Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;
							break;
		    			case icRelativeColorimetric:
							sch[ix].sig = icSigAToB1Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;

							sch[ix].sig = icSigAToB0Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;
							break;
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
							e_outa = 1;					/* (fall though) */
		    			case icPerceptual:
							sch[ix].sig = icSigAToB0Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;
							break;
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							e_outa = 1;					/* (fall though) */
		    			case icSaturation:
							sch[ix].sig = icSigAToB2Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;

							sch[ix].sig = icSigAToB0Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;
							break;
						default:
							icm_err(icp, 1,"icc_get_luobj: Unknown intent");
							return NULL;
					}
					break;

		    	case icmBwd:	/* PCS to Device */
					e_ins = e_pcs;
					e_outs = icp->header->colorSpace;
					if (intent == icmDefaultIntent)
						intent = icp->header->renderingIntent;

					switch ((int)intent) {
		    			case icAbsoluteColorimetric:
							e_ina = 1;
							sch[ix].sig = icSigBToA1Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;

							sch[ix].sig = icSigBToA0Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;
							break;
		    			case icRelativeColorimetric:
							sch[ix].sig = icSigBToA1Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;

							sch[ix].sig = icSigBToA0Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;
							break;
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
							e_outa = 1;					/* (fall though) */
		    			case icPerceptual:
							sch[ix].sig = icSigBToA0Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;
							break;
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							e_outa = 1;					/* (fall though) */
		    			case icSaturation:
							sch[ix].sig = icSigBToA2Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;

							sch[ix].sig = icSigBToA0Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;
							break;
						default:
							icm_err(icp, 1,"icc_get_luobj: Unknown intent");
							return NULL;
					}
					break;

				default:
					icm_err(icp, 1,"icc_get_luobj: Inaproptiate function requested");
					return NULL;
				}

			/* Setup for forward and this wll be reverses on creating Pe */
			sch[ix].sig = icmSigShaperMatrix;
			sch[ix].in = icp->header->colorSpace;;
			sch[ix++].out = icSigXYZData;

			sch[ix].sig = icmSigShaperMono;
			sch[ix].in = icp->header->colorSpace;
			sch[ix++].out = icp->header->pcs;
			break;
			
    	case icSigOutputClass:
			/* Expect BToA Lut and optional AToB Lut, All intents, expect gamut */
			/* or momochrome table profile (reversable) */ 
			/* Device <-> PCS */
			/* Gamut Lut - no intent, assume Relative Colorimetric */
			/* Optional preview links PCS <-> PCS */
			
			/* Determine the algorithm and set its parameters */
			switch (func) {
				icTagSignature ttag;
				
		    	case icmFwd:	/* Device to PCS */
					e_ins = icp->header->colorSpace;
					e_outs = e_pcs;
					if (intent == icmDefaultIntent)
						intent = icp->header->renderingIntent;

					switch ((int)intent) {
		    			case icAbsoluteColorimetric:
							e_outa = 1;
							sch[ix].sig = icSigAToB1Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;

							sch[ix].sig = icSigAToB0Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;
							break;
		    			case icRelativeColorimetric:
							sch[ix].sig = icSigAToB1Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;

							sch[ix].sig = icSigAToB0Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;
							break;
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
							e_outa = 1;					/* (fall though) */
		    			case icPerceptual:
							sch[ix].sig = icSigAToB0Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;
							break;
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							e_outa = 1;					/* (fall though) */
		    			case icSaturation:
							sch[ix].sig = icSigAToB2Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;

							sch[ix].sig = icSigAToB0Tag;
							sch[ix].in = tdev;
							sch[ix++].out = tpcs;
							break;
						default:
							icm_err(icp, 1,"icc_get_luobj: Unknown intent");
							return NULL;
					}
					/* Setup for forward and this will be reverses on creating Pe */
					sch[ix].sig = icmSigShaperMono;
					sch[ix].in = icp->header->colorSpace;
					sch[ix++].out = icp->header->pcs;
					break;

		    	case icmBwd:	/* PCS to Device */
					e_ins = e_pcs;
					e_outs = icp->header->colorSpace;
					if (intent == icmDefaultIntent)
						intent = icp->header->renderingIntent;

					switch ((int)intent) {
		    			case icAbsoluteColorimetric:
							e_ina = 1;
							sch[ix].sig = icSigBToA1Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;

							sch[ix].sig = icSigBToA0Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;
							break;
		    			case icRelativeColorimetric:
							sch[ix].sig = icSigBToA1Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;

							sch[ix].sig = icSigBToA0Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;
							break;
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
							e_outa = 1;					/* (fall though) */
		    			case icPerceptual:
							sch[ix].sig = icSigBToA0Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;
							break;
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							e_outa = 1;					/* (fall though) */
		    			case icSaturation:
							sch[ix].sig = icSigBToA2Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;

							sch[ix].sig = icSigBToA0Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tdev;
							break;
						default:
							icm_err(icp, 1,"icc_get_luobj: Unknown intent");
							return NULL;
					}
					/* Setup for forward and this will be reverses on creating Pe */
					sch[ix].sig = icmSigShaperMono;
					sch[ix].in = icp->header->colorSpace;
					sch[ix++].out = icp->header->pcs;
					break;

		    	case icmGamut:	/* PCS to 1D */
					e_ins = e_pcs;
					e_outs = icSigGrayData;

					if ((icp->cflags & icmCFlagAllowQuirks) == 0
					 && (icp->cflags & icmCFlagAllowExtensions) == 0) {

						/* Allow only default and absolute */
						if (intent != icmDefaultIntent
						 && intent != icAbsoluteColorimetric) {
							icm_err(icp, ICM_ERR_GAMUT_INTENT,"icc_get_luobj: Intent (0x%x)is unexpected for Gamut table",intent);
							return NULL;
						}
					} else {
						int warn = 0;

						/* Be more forgiving */
						switch ((int)intent) {
			    			case icAbsoluteColorimetric:
								e_ina = 1;
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
								icm_err(icp, ICM_ERR_GAMUT_INTENT,"icc_get_luobj: Unknown intent (0x%x)",intent);
								return NULL;
						}
						if (warn) { 
							/* (icmQuirkWarning noset doesn't care about icp->op direction) */
							icmQuirkWarning(icp, ICM_ERR_GAMUT_INTENT, 1, "icc_get_luobj: Intent (0x%x)is unexpected for Gamut table",intent);
						}
					}

					sch[ix].sig = icSigGamutTag;
					sch[ix].in = tpcs;
					sch[ix++].out = icSigGrayData;
					break;

		    	case icmPreview:	/* PCS to PCS */
					e_ins = e_pcs;
					e_outs = e_pcs;

					switch ((int)intent) {
		    			case icRelativeColorimetric:
							sch[ix].sig = icSigBToA1Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tpcs; 
							break;
		    			case icPerceptual:
							sch[ix].sig = icSigBToA0Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tpcs;
							break;
		    			case icSaturation:
							sch[ix].sig = icSigBToA2Tag;
							sch[ix].in = tpcs;
							sch[ix++].out = tpcs;
							break;
		    			case icAbsoluteColorimetric:
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							icm_err(icp, 1,"icc_get_luobj: Intent is inappropriate for preview table");
							return NULL;
						default:
							icm_err(icp, 1,"icc_get_luobj: Unknown intent");
							return NULL;
					}
					break;

				default:
					icm_err(icp, 1,"icc_get_luobj: Inaproptiate function requested");
					return NULL;
			}
			break;

    	case icSigLinkClass:
			/* Expect AToB0 Lut and optional BToA0 Lut, One intent in header */
			/* Device <-> Device */

			if (intent != icp->header->renderingIntent
			 && intent != icmDefaultIntent) {
				icm_err(icp, 1,"icc_get_luobj: Intent is inappropriate for Link profile");
				return NULL;
			}
			intent = icp->header->renderingIntent;

			/* Determine the algorithm and set its parameters */
			switch (func) {
		    	case icmFwd:	/* Device to PCS (== Device) */
					e_ins = icp->header->colorSpace;
					e_outs = icp->header->pcs;
					sch[ix].sig = icSigAToB0Tag;
					sch[ix].in = tdev;
					sch[ix++].out = tpcs;
					break;

		    	case icmBwd:	/* PCS (== Device) to Device */
					e_ins = icp->header->pcs;
					e_outs = icp->header->colorSpace;
					sch[ix].sig = icSigBToA0Tag;
					sch[ix].in = tpcs;
					sch[ix++].out = tdev;
					break;

				default:
					icm_err(icp, 1,"icc_get_luobj: Inaproptiate function requested");
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
				icm_err(icp, 1,"icc_get_luobj: Intent is inappropriate for Abstract profile");
				return NULL;
			}

			switch (func) {
		    	case icmFwd:	/* PCS (== Device) to PCS */
					e_ins = e_pcs;
					e_outs = e_pcs;
					if (intent == icAbsoluteColorimetric) {
						e_ina = 1;
						e_outa = 1;
					}
					sch[ix].sig = icSigAToB0Tag;
					sch[ix].in = tdev;
					sch[ix++].out = tpcs;
					break;

		    	case icmBwd:	/* PCS to PCS (== Device) */
					e_ins = e_pcs;
					e_outs = e_pcs;
					if (intent == icAbsoluteColorimetric) {
						e_ina = 1;
						e_outa = 1;
					}
					sch[ix].sig = icSigBToA0Tag;
					sch[ix].in = tpcs;
					sch[ix++].out = tdev;
					break;


				default:
					icm_err(icp, 1,"icc_get_luobj: Inaproptiate function requested");
					return NULL;
			}
			break;

    	case icSigNamedColorClass:
			/* Expect Name -> Device, Optional PCS */
			/* and a reverse lookup would be useful */
			/* (ie. PCS or Device coords to closest named color). */
			/* ~~88 to be implemented ~~ */

			/* ~~ Absolute intent is valid for processing of */
			/* PCS from named Colors. Also allow for e_pcs */
			if (intent != icmDefaultIntent
			 && intent != icRelativeColorimetric
			 && intent != icAbsoluteColorimetric) {
				icm_err(icp, 1,"icc_get_luobj: Intent is inappropriate for Named Color profile");
				return NULL;
			}
			e_ins = icmSigNamedData;
			e_outs = e_pcs;
			if (intent == icAbsoluteColorimetric)
				e_ina = 1;

			icm_err(icp, 1,"icc_get_luobj: Named Colors not implemented");
			return NULL;

    	default:
			icm_err(icp, 1,"icc_get_luobj: Unknown profile class '%s'",icm2str(icmProfileClassSig,icp->header->deviceClass));
			return NULL;
	}

	/* Set search order */
	nix = ix;
	if (order == icmLuOrdRev) {
		ix = nix-1;
		ixi = -1;
	} else {
		ix = 0;
		ixi = 1;
	}

	/* Go through the search list and see if we can create a color conversion */
	for (; ix >= 0 && ix < nix; ix += ixi) {

#ifdef DEBUG_GET_LU
		printf("  Checking sch[%d] sig %s\n",ix,icmLuSrcType2str(sch[ix].sig));
#endif
		/* Mono and Matrix are special case Pe's made from several tags. */
		if (sch[ix].sig == icmSigShaperMono || sch[ix].sig == icmSigShaperMatrix) {
			int invert = 0;
			if (func == icmBwd)
				invert = 1;
			if (sch[ix].sig == icmSigShaperMono)
				tt = new_icmShaperMono(icp, invert);
			else
				tt = new_icmShaperMatrix(icp, invert);

			if (tt != NULL && invert) {
				icColorSpaceSignature t1;
				int t2;
				t1 = sch[ix].in; sch[ix].in = sch[ix].out; sch[ix].out = t1;
				t2 = sch[ix].ina; sch[ix].ina = sch[ix].outa; sch[ix].outa = t2;
			}
		} else {
			/* We know that all tags that are color transforms are icmPeSeq */
			tt = (icmPeSeq *)icp->read_tag(icp, sch[ix].sig);
			if (tt != NULL
			 && (tt->ttype == icmSigUnknownType
			  || tt->etype == icmSigPeNone)) 
				tt = NULL;				/* We can't use this... */
			if (tt != NULL)
				tt->reference(tt);		/* We'll delete this at the end of the function... */
		}
		if (tt != NULL) {
#ifdef DEBUG_GET_LU
			printf("  Choosing sch[%d] sig %s\n",ix,icmLuSrcType2str(sch[ix].sig));
#endif
			break;
		}
	}

	if (tt == NULL) {
		icm_err(icp, 1,"icc_get_luobj: Unable to locate usable conversion");
		return NULL;
	}

	tt->init(tt);			/* Make sure attr is computed */

	/* Correct cLut encoding for Tag Type used */
	if (tt->etype == icmSigPeLut816) {
		icmLut1 *ttt = (icmLut1 *)tt;
		if (ttt->bpv == 1) {
			if (sch[ix].in == icmSigLabV2Data)
				sch[ix].in = icmSigLab8Data;
			else if (sch[ix].in == icmSigXYZ16Data)
				sch[ix].in = icmSigXYZ8Data;
			if (sch[ix].out == icmSigLabV2Data)
				sch[ix].out = icmSigLab8Data;
			else if (sch[ix].out == icmSigXYZ16Data)
				sch[ix].out = icmSigXYZ8Data;
		}
	}

	/* For any non-PCS spaces that need normalisation, set cLut encoded ColorSpace */
	if (tt->etype == icmSigPeLut816) {
		sch[ix].in = icmSig2NormSig(icp, sch[ix].in, icmEncV2, icmEnc16bit);
		sch[ix].out = icmSig2NormSig(icp, sch[ix].out, icmEncV2, icmEnc16bit);
	}

	/* Create and fill in the luobj */
	if ((p = (icmLu4Space *) icp->al->calloc(icp->al, 1, sizeof(icmLu4Space))) == NULL) {
		icm_err(icp, ICM_ERR_MALLOC, "Allocating icmLu failed");
		return NULL;
	}

	p->ttype     = icmSpaceLu4Type;
	p->icp       = icp;
	p->sourceTag = sch[ix].sig;
	p->intent = intent;
	p->function = func;
	p->order = order;
	/* pcswht .. fromAbs are set by ->init_wh_bk() below */

	p->ini.sig  = icmNorm2Sig(icp, sch[ix].in);
	p->ini.nch  = icmCSSig2nchan(p->ini.sig);
	p->outi.sig = icmNorm2Sig(icp, sch[ix].out);
	p->outi.nch  = icmCSSig2nchan(p->outi.sig);
	p->pcsi.sig = pcs; 
	p->pcsi.nch  = icmCSSig2nchan(p->pcsi.sig);
	p->e_ini.sig  = e_ins;
	p->e_ini.nch  = icmCSSig2nchan(p->e_ini.sig);
	p->e_outi.sig = e_outs;
	p->e_outi.nch  = icmCSSig2nchan(p->e_outi.sig);
	p->e_pcsi.sig = e_pcs; 
	p->e_pcsi.nch  = icmCSSig2nchan(p->e_pcsi.sig);

	/* We set all the icmCSInfo range info after determining normalizations.. */

	p->del           = icmLu4_del;
	p->init_wh_bk    = LU4INIT_WH_BK(icmLu4Space) icmLu4Init_Wh_bk;
	p->native_spaces = LU4NATIVESPACES(icmLu4Space) icmLu4NativeSpaces;
	p->spaces        = LU4SPACES(icmLu4Space) icmLu4Spaces;
	p->XYZ_Rel2Abs   = LU4XYZ_REL2ABS(icmLu4Space) icmLu4XYZ_Rel2Abs;
	p->XYZ_Abs2Rel   = LU4XYZ_ABS2REL(icmLu4Space) icmLu4XYZ_Abs2Rel;

	p->wh_bk_points  = LU4WH_BK_POINTS(icmLu4Space) icmLu4Wh_bk_points;
	p->lu_wh_bk_points = LU4LU_WH_BK_POINTS(icmLu4Space) icmLu4Lu_wh_bk_points;

	p->lookup_fwd = icmLu4_lookup_fwd;
	p->lookup_bwd = icmLu4_lookup_bwd;

	p->input_fwd = icmLu4_input_fwd;
	p->core3_fwd = icmLu4_core3_fwd;
	p->output_fwd = icmLu4_output_fwd;

	p->output_bwd = icmLu4_output_bwd;
	p->core3_bwd = icmLu4_core3_bwd;
	p->input_bwd = icmLu4_input_bwd;

	p->input_fmt_fwd = icmLu4_input_fmt_fwd;
	p->input_pch_fwd = icmLu4_input_pch_fwd;
	p->core5_fwd = icmLu4_core5_fwd;
	p->output_pch_fwd = icmLu4_output_pch_fwd;
	p->output_fmt_fwd = icmLu4_output_fmt_fwd;

	p->output_fmt_bwd = icmLu4_output_fmt_bwd;
	p->output_pch_bwd = icmLu4_output_pch_bwd;
	p->core5_bwd = icmLu4_core5_bwd;
	p->input_pch_bwd = icmLu4_input_pch_bwd;
	p->input_fmt_bwd = icmLu4_input_fmt_bwd;

	p->max_in_res = icmLu4_max_in_res;
	p->max_clut_res = icmLu4_max_clut_res;
	p->max_out_res = icmLu4_max_out_res;
	p->linear_light_inout = icmLu4_linear_light_inout;
	p->get_lut = icmLu4_get_lut;
	p->get_tac = icmLu4_get_tac;

	inch = tt->inputChan;
	outch = tt->outputChan;

	/* Create the Pe's for all the transforms */
	if ((p->lookup = new_icmPeContainer(icp, inch, outch)) == NULL

	 || (p->input  = new_icmPeContainer(icp, inch, inch)) == NULL
	 || (p->core3  = new_icmPeContainer(icp, inch, outch)) == NULL
	 || (p->output = new_icmPeContainer(icp, outch, outch)) == NULL

	 || (p->input_fmt  = new_icmPeContainer(icp, inch, inch)) == NULL
	 || (p->input_pch  = new_icmPeContainer(icp, inch, inch)) == NULL
	 || (p->core5      = new_icmPeContainer(icp, inch, outch)) == NULL
	 || (p->output_pch = new_icmPeContainer(icp, outch, outch)) == NULL
	 || (p->output_fmt = new_icmPeContainer(icp, outch, outch)) == NULL) {

		p->del(p);
		return NULL;
	}

	/* -------------------------- */
	/* Create the overall lookup: */
	/* Add any PCS format conversions to the tagtype transform */

	/* Setup any input PCS format converters */
	if (create_fmt(p->lookup, p, sch[ix].in, sch[ix].ina, e_ins, e_ina)) {
		p->del(p);
		return NULL;
	}

	/* Append the tagtype transform */
	if (p->lookup->append_pes(p->lookup, tt, 0, tt->count)) {
		p->del(p);
		return NULL;
	}

	/* Setup any output PCS format converters */
	if (create_fmt(p->lookup, p, e_outs, e_outa, sch[ix].out, sch[ix].outa)) {
		p->del(p);
		return NULL;
	}

	p->lookup->init(p->lookup);

	/* -------------------------- */
	/* Split the overall up into the 3 component lookup: */

	/* Setup components based on scanning the contents of lookup transform. */
	/* NULL or NOP pe's are skipped. */
	/* The in_curve is assumed to end on the first matrix, cLUT or complex Pe, */
	/* as long as there is at least one non-norm per-channel op. */
	/* If there is no matrix, cLUT or complex Pe, the whole transform */
	/* is placed in the core transform. */
	{
		int nnpch = 0;
		int ix1, ix2;

		/* Locate the start of the core transform */
		for (ix1 = 0; ix1 < p->lookup->count; ix1++) {
			if (p->lookup->pe[ix1] == NULL
			 || p->lookup->pe[ix1]->attr.op == icmPeOp_NOP
			 || p->lookup->pe[ix1]->attr.op == icmPeOp_perch) {
				if (p->lookup->pe[ix1] != NULL
				 && p->lookup->pe[ix1]->attr.op == icmPeOp_perch
				 && p->lookup->pe[ix1]->attr.norm == 0)
					nnpch = 1;
				continue;
			}
			break;
		}

		/* If we didn't find an input non-norm per-channel op, then keep all the */
		/* NOP's and normalizing per-channel as part of the core. */
		if (nnpch == 0)
			ix1 = 0;

		/* Whole transform is per channel */
		if (ix1 >= p->lookup->count) {
			if (p->core3->append_pes(p->core3, (icmPeSeq *)p->lookup, 0, p->lookup->count)) {
				p->del(p);
				return NULL;
			}

		/* There are components */
		} else {

			/* Locate the end of the core transform */
			nnpch = 0;
			for (ix2 = p->lookup->count-1; ix2 > ix1; ix2--) {
				if (p->lookup->pe[ix2] == NULL
				 || p->lookup->pe[ix2]->attr.op == icmPeOp_NOP
				 || p->lookup->pe[ix2]->attr.op == icmPeOp_perch) {
					if (p->lookup->pe[ix2] != NULL
					 && p->lookup->pe[ix2]->attr.op == icmPeOp_perch
					 && p->lookup->pe[ix2]->attr.norm == 0)
						nnpch = 1;
					continue;
				}
				break;
			}
			ix2++;

			/* If we didn't find an output non-norm per-channel op, then keep all the */
			/* NOP's and normalizing per-channel as part of the core. */
			if (nnpch == 0)
				ix2 = p->lookup->count;

			/* Add the components we've found */
			if (p->input->append_pes(p->input, (icmPeSeq *)p->lookup, 0, ix1)
			 || p->core3->append_pes(p->core3, (icmPeSeq *)p->lookup, ix1, ix2)
			 || p->output->append_pes(p->output, (icmPeSeq *)p->lookup, ix2, p->lookup->count)) {
				p->del(p);
				return NULL;
			}
		}
	}

	p->input->init(p->input);
	p->core3->init(p->core3);
	p->output->init(p->output);

	/* Add intermediate de-norm/normalization functions to make */
	/* all i/o be full range. */
	/* (Note that the effective and native PCS types will be the same if */
	/* the per channel transform is not a NOP) */
	{
		/* If native input space is a normalized space */
		if (p->input->attr.op != icmPeOp_NOP && (icmCSSig2type(sch[ix].in) & CSSigType_NORM)) {
			icmPe *toNorm, *fromNorm;

			/* Create normalizing/de-normalizing transform */ 
			if ((toNorm = new_icmNSig2NormPe(icp, NULL, sch[ix].in, 0, 1)) == NULL) {
				p->del(p);
				return NULL;
			}
			if ((fromNorm = new_icmNSig2NormPe(icp, NULL, sch[ix].in, 1, 1)) == NULL) {
				toNorm->del(toNorm);
				p->del(p);
				return NULL;
			}

			/* add these in the appropriate places */
			if (p->input->append(p->input, fromNorm)
			 || p->core3->prepend(p->core3, toNorm)) {
				p->del(p);
				return NULL;
			}
			toNorm->del(toNorm);			/* Container has taken a reference */
			fromNorm->del(fromNorm);
		}

		/* If native output space is a normalized space */
		if (p->output->attr.op != icmPeOp_NOP && (icmCSSig2type(sch[ix].out) & CSSigType_NORM)) {
			icmPe *toNorm, *fromNorm;

			/* Create normalizing/de-normalizing transform */ 
			if ((toNorm = new_icmNSig2NormPe(icp, NULL, sch[ix].out, 0, 1)) == NULL) {
				p->del(p);
				return NULL;
			}
			if ((fromNorm = new_icmNSig2NormPe(icp, NULL, sch[ix].out, 1, 1)) == NULL) {
				toNorm->del(toNorm);
				p->del(p);
				return NULL;
			}

			/* add these in the appropriate places */
			if (p->core3->append(p->core3, fromNorm)
			 || p->output->prepend(p->output, toNorm)) {
				p->del(p);
				return NULL;
			}
			toNorm->del(toNorm);			/* Container has taken a reference */
			fromNorm->del(fromNorm);
		}
	}

	p->input->init(p->input);
	p->core3->init(p->core3);
	p->output->init(p->output);

	/* -------------------------- */
	/* Split the overall up into the 5 component lookup: */
	/* This is similar to above except we split the transform before adding */
	/* format conversion. */ 

	/* Setup components based on scanning the contents of tt transform. */
	/* NULL or NOP pe's are skipped. */
	/* The in_curve is assumed to end on the first matrix, cLUT or complex Pe. */
	/* If there is no matrix, cLUT or complex Pe, the whole transform */
	/* is placed in the core transform. */
	/* (We don't have to worry about not splitting a norm only per channel, */
	/*  because we haven't added any.) */
	{
		int ix1, ix2;

		/* Locate the start of the core transform */
		for (ix1 = 0; ix1 < tt->count; ix1++) {
			if (tt->pe[ix1] == NULL
			 || tt->pe[ix1]->attr.op == icmPeOp_NOP
			 || tt->pe[ix1]->attr.op == icmPeOp_perch) {
				continue;
			}
			break;
		}

		/* Whole transform is per channel */
		if (ix1 >= tt->count) {
			if (p->core5->append_pes(p->core5, (icmPeSeq *)tt, 0, tt->count)) {
				p->del(p);
				return NULL;
			}

		/* There are components */
		} else {

			/* Locate the end of the core transform */
			for (ix2 = tt->count-1; ix2 > ix1; ix2--) {
				if (tt->pe[ix2] == NULL
				 || tt->pe[ix2]->attr.op == icmPeOp_NOP
				 || tt->pe[ix2]->attr.op == icmPeOp_perch) {
					continue;
				}
				break;
			}
			ix2++;

			/* Add the components we've found */
			if (p->input_pch->append_pes(p->input_pch, (icmPeSeq *)tt, 0, ix1)
			 || p->core5->append_pes(p->core5, (icmPeSeq *)tt, ix1, ix2)
			 || p->output_pch->append_pes(p->output_pch, (icmPeSeq *)tt, ix2, tt->count)) {
				p->del(p);
				return NULL;
			}
		}
	}

	p->input_pch->init(p->input_pch);
	p->core5->init(p->core5);
	p->output_pch->init(p->output_pch);

	/* Add the final 2 parts of the 5 part conversion: */
	/* Because we want each part to have full range values, make sure the */
	/* format conversion doesn't do the normalization. */

	/* Create any input PCS format converters */
	if (create_fmt(p->input_fmt, p, p->ini.sig, sch[ix].ina, e_ins, e_ina)) {
		p->del(p);
		return NULL;
	}

	/* Create any output PCS format converters */
	if (create_fmt(p->output_fmt, p, e_outs, e_outa, p->outi.sig, sch[ix].outa)) {
		p->del(p);
		return NULL;
	}

	/* Add intermediate de-norm/normalization functions to make */
	/* all i/o be full range. */
	{
		/* If input space is a normalized space */
		if (icmCSSig2type(sch[ix].in) & CSSigType_NORM) {
			icmPe *toNorm, *fromNorm;

			/* Create normalizing/de-normalizing transform */ 
			if ((toNorm = new_icmNSig2NormPe(icp, NULL, sch[ix].in, 0, 1)) == NULL) {
				p->del(p);
				return NULL;
			}
			if ((fromNorm = new_icmNSig2NormPe(icp, NULL, sch[ix].in, 1, 1)) == NULL) {
				toNorm->del(toNorm);
				p->del(p);
				return NULL;
			}

			/* add these in the appropriate places */
			if (p->input_pch->attr.op != icmPeOp_NOP) {
				if (p->input_pch->prepend(p->input_pch, toNorm)
				 || p->input_pch->append(p->input_pch, fromNorm)) {
					p->del(p);
					return NULL;
				}
			}
			if (p->core5->prepend(p->core5, toNorm)) {
				p->del(p);
				return NULL;
			}
			toNorm->del(toNorm);			/* Containers have taken a reference */
			fromNorm->del(fromNorm);
		}

		/* If output space is a normalized space */
		if (icmCSSig2type(sch[ix].out) & CSSigType_NORM) {
			icmPe *toNorm, *fromNorm;

			/* Create normalizing/de-normalizing transform */ 
			if ((toNorm = new_icmNSig2NormPe(icp, NULL, sch[ix].out, 0, 1)) == NULL) {
				p->del(p);
				return NULL;
			}
			if ((fromNorm = new_icmNSig2NormPe(icp, NULL, sch[ix].out, 1, 1)) == NULL) {
				toNorm->del(toNorm);
				p->del(p);
				return NULL;
			}

			/* add these in the appropriate places */
			if (p->core5->append(p->core5, fromNorm)) {
				p->del(p);
				return NULL;
			}
			if (p->output_pch->attr.op != icmPeOp_NOP) {
				if (p->output_pch->prepend(p->output_pch, toNorm)
				 || p->output_pch->append(p->output_pch, fromNorm)) {
					p->del(p);
					return NULL;
				}
			}
			toNorm->del(toNorm);			/* Container has taken a reference */
			fromNorm->del(fromNorm);
		}
	}

	/* Make sure all the attributes are computed */
	p->input_fmt->init(p->input_fmt);
	p->input_pch->init(p->input_pch);
	p->core5->init(p->core5);
	p->output_pch->init(p->output_pch);
	p->output_fmt->init(p->output_fmt);

	p->op = p->lookup->attr.op;
	p->can_bwd = p->lookup->attr.bwd;

	/* set the ranges */
	if (p->ini.min[0] == 0.0 && p->ini.max[0] == 0.0 )
		icmGetDefaultRange(icp, p->ini.min, p->ini.max, sch[ix].in);  
	if (p->outi.min[0] == 0.0 && p->outi.max[0] == 0.0 )
		icmGetDefaultRange(icp, p->outi.min, p->outi.max, sch[ix].out);  

	if (p->ini.sig == pcs) {
		icmCpyN(p->pcsi.min, p->ini.min, p->ini.nch);
		icmCpyN(p->pcsi.max, p->ini.max, p->ini.nch);
	} else if (p->outi.sig == pcs) {
		icmCpyN(p->pcsi.min, p->outi.min, p->outi.nch);
		icmCpyN(p->pcsi.max, p->outi.max, p->outi.nch);
	} else
		icmGetDefaultRange(icp, p->pcsi.min, p->pcsi.max, pcs);

	if (e_ins == p->ini.sig) {
		icmCpyN(p->e_ini.min, p->ini.min, p->ini.nch);
		icmCpyN(p->e_ini.max, p->ini.max, p->ini.nch);
	} else
		icmGetDefaultRange(icp, p->e_ini.min, p->e_ini.max, e_ins);

	if (e_outs == p->outi.sig) {
		icmCpyN(p->e_outi.min, p->outi.min, p->outi.nch);
		icmCpyN(p->e_outi.max, p->outi.max, p->outi.nch);
	} else
		icmGetDefaultRange(icp, p->e_outi.min, p->e_outi.max, e_outs);

	if (p->e_ini.sig == e_pcs) {
		icmCpyN(p->e_pcsi.min, p->e_ini.min, p->e_ini.nch);
		icmCpyN(p->e_pcsi.max, p->e_ini.max, p->e_ini.nch);
	} else if (p->e_outi.sig == e_pcs) {
		icmCpyN(p->e_pcsi.min, p->e_outi.min, p->e_outi.nch);
		icmCpyN(p->e_pcsi.max, p->e_outi.max, p->e_outi.nch);
	} else
		icmGetDefaultRange(icp, p->e_pcsi.min, p->e_pcsi.max, e_pcs);

	if (p->init_wh_bk(p)) {
		p->del(p);
		return NULL;
	}

	/* See if there is a cLut that needs its interpolation algorithm determining */
	/* (We're relying on append_pes() having flattened any hierarchy of icmPeSeq's) */
	for (ix = 0; ix < p->lookup->count; ix++) {
		if (p->lookup->pe[ix] == NULL)
			continue;

		if (p->lookup->pe[ix]->isPeSeq) {
			icm_err(icp, ICM_ERR_SEARCH_PESEQ_INTERNAL, "icc_get_lu4obj found unexpected icmPeSeq inside icmPeContainer");
			p->del(p);
			return NULL;

		/* Is this likely ? Does it make any sense if it existed ?? */
		} else if (p->lookup->pe[ix]->etype == icmSigPeInverter) {
			icmPeInverter *iv = (icmPeInverter *)p->lookup->pe[ix];
			if (iv->pe->etype == icmSigPeClut) {
				icmPeClut *cl = (icmPeClut *)iv->pe;
				cl->choose_alg(cl, p);
				break;
			}
		} else if (p->lookup->pe[ix]->etype == icmSigPeClut) {
			icmPeClut *cl = (icmPeClut *)p->lookup->pe[ix];
			cl->choose_alg(cl, p);
			break;
		}
	}

	/* We're done with source transform */
	tt->del(tt);

#ifdef ALWAYSLUTRACE
#  pragma message("######### icmLu ALWAYSLUTRACE enabled ########")
	p->lookup->trace = 1;
	p->input->trace = 1;
	p->core3->trace = 1;
	p->output->trace = 1;
	p->input_fmt->trace = 1;
	p->input_pch->trace = 1;
	p->core5->trace = 1;
	p->output_pch->trace = 1;
	p->output_fmt->trace = 1;
#endif

#ifdef DEBUG_GET_LU
	printf("done icc_get_lu4obj\n");
#endif
	return (icmLu4Base *)p;
}

/* =========================================================================================== */
/* ICC profile transform creation support */
/* =========================================================================================== */

#define CLIP_MARGIN 0.005       /* Margine to allow before reporting clipping = 0.5% */


/* Helper function to set multiple Monochrome tags simultaneously. */
/* Note that these tables and matrix value all have to be */
/* compatible in having the same configuration and resolutions. */
/* Set errc and return error number in underlying icc */
/* Note that clutfunc in[] value has "index under". */
/* Returns ec */
int icc_create_mono_xforms(
	struct _icc *icp,
	int     flags,					/* Setting flags */
	void   *cbctx,					/* Opaque callback context pointer value */

	int ntables,					/* Number of tables to be set, 1..n */
	icmXformSigs *sigs,				/* signatures and tag types for each table */
									/* ICCV2 Matrix/gamma/shaper uses icmSigShaperMatrix */
									/* Valid combinations are:
									  icmSigShaperMono + icmSigShaperMatrixType 
									  icSigAToB0Tag + icSigLut16Type
									  	(icSigBToA0Tag + icSigLutB16Type automatically added) */

	unsigned int	inputEnt,       /* Num of in-table entries */
	unsigned int	inv_inputEnt,   /* Num of inverse in-table entries */

	icColorSpaceSignature insig, 	/* Input color space */
	icColorSpaceSignature outsig, 	/* Output color space */

	void (*infunc)(void *cbctx, double *out, double *in, int tn)
						/* Input transfer function, inspace->inspace' (NULL = default) */
						/* Will be called ntables times each input grid value */
) {
	/* Pointers to elements to set */
	struct {
		icmBase      *wo_fwd;				/* TagType */
		icmPeCurve   *in_fwd;				/* Input curve */
		icmPeMatrix  *matrix_fwd;			/* Either matrix or clut set */
		icmPeClut    *clut_fwd;
		icmPeCurve   *out_fwd[3];			/* Output curve */

		icmBase      *wo_bwd;				/* bwd TagType */
		icmPeCurve   *in_bwd[3];			/* bwd Input curve */
		icmPeMatrix  *matrix_bwd;			/* Either inverse matrix or bwd clut set */
		icmPeClut    *clut_bwd;
		icmPeCurve   *out_bwd;				/* bwd output curve */


		icmPe *pcs_nf;				/* PCS normalization function */
		int clip_ent;				/* nz to clip entry values of tables */
	} *tables = NULL;
	int inputChan, outputChan;
	int ii[MAX_CHAN];		/* Index value */
	double _iv[2 * MAX_CHAN], *iv = &_iv[MAX_CHAN], *ivn;	/* Real index value/table value */
	int tn, i, e, f;
	int clip = 0;

	inputChan = icmCSSig2nchan(insig);
	outputChan = icmCSSig2nchan(outsig);

	if (inputChan != 1 || outputChan != 3)
		return icm_err(icp, ICM_ERR_CRM_XFORMS_BADNCHAN, "icc_create_mono_xforms got unexpected no. inChan %d and outChan %d",inputChan, outputChan);

	/* Allocate the per table info */
	if ((tables = icp->al->calloc(icp->al, ntables, sizeof(*tables))) == NULL) {
		return icm_err(icp, ICM_ERR_MALLOC, "Allocating icc_create_mono_xforms tables failed");
	}

	/* Create tags and tagtypes for each table. */
	for (tn = 0; tn < ntables; tn++) {
		if (sigs[tn].sig == icmSigShaperMono
		 && sigs[tn].ttype == icmSigShaperMatrixType) {
			/* ---------------------------------------------------------- */
			icmCurve *wo;

			tables[tn].clip_ent = 1;

			/* Delete any existing tag */
			if (icp->delete_tag_quiet(icp, icSigGrayTRCTag) != ICM_ERR_OK)
				return icp->e.c;

			if ((tables[tn].wo_fwd = icp->add_tag(icp, sigs[tn].sig, sigs[tn].ttype)) == NULL) 
				return icp->e.c;

			wo = (icmCurve *)tables[tn].wo_fwd;
			tables[tn].in_fwd = (icmPeCurve *)wo;

			/* Allocate the fwd curve contents */
			wo->outputChan = wo->inputChan = 1;
			wo->ctype = icmCurveSpec;
			wo->count = inputEnt; 
			if (wo->allocate(wo))
				return icp->e.c;

		}
		else if (sigs[tn].sig == icSigAToB0Tag) {
			if (sigs[tn].ttype == icSigLut16Type) {
				/* ---------------------------------------------------------- */
				icColorSpaceSignature pcsnsig; 
				icmLut1 *wo;

				tables[tn].clip_ent = 1;

				/* Setup normalization Pe */
				pcsnsig = icmSig2NormSig(icp, outsig, icmEncV2, icmEnc16bit);
				tables[tn].pcs_nf = new_icmNSig2NormPe(icp, NULL, pcsnsig, 0, 1);

				/* - - - - - - fwd transform - - - - - - */
				/* Delete any existing tag and add one */
				if (icp->delete_tag_quiet(icp, sigs[tn].sig) != ICM_ERR_OK)
					return icp->e.c;

				if ((tables[tn].wo_fwd = icp->add_tag(icp, sigs[tn].sig, sigs[tn].ttype)) == NULL) 
					return icp->e.c;

				wo = (icmLut1 *)tables[tn].wo_fwd;
			
				/* Allocate space in tags */
				wo->inputChan = 1;
				wo->outputChan = 3;
				wo->inputEnt = inputEnt; 
				wo->clutPoints = 2;
				wo->outputEnt = 2;

				if (wo->allocate(wo))
					return icp->e.c;

				/* Get pointers to tables to be filled */
				tables[tn].in_fwd = wo->pe_ic[0];
				tables[tn].clut_fwd = wo->pe_cl;
				tables[tn].out_fwd[0] = wo->pe_oc[0];
				tables[tn].out_fwd[1] = wo->pe_oc[1];
				tables[tn].out_fwd[2] = wo->pe_oc[2];

				if (icp->e.c != ICM_ERR_OK)
					return icp->e.c;

				/* - - - - - - bwd transform - - - - - - */
				/* Delete any existing tag and add one */
				if (icp->delete_tag_quiet(icp, icSigBToA0Tag) != ICM_ERR_OK)
					return icp->e.c;

				if ((tables[tn].wo_bwd = icp->add_tag(icp, icSigBToA0Tag, icSigLut16Type)) == NULL) 
					return icp->e.c;

				wo = (icmLut1 *)tables[tn].wo_bwd;
			
				/* Allocate space in tags */
				wo->inputChan = 3;
				wo->outputChan = 1;
				wo->inputEnt = 1024;
				wo->clutPoints = 2;
				wo->outputEnt = inv_inputEnt; 

				if (wo->allocate(wo))
					return icp->e.c;

				/* Get pointers to tables to be filled */
				tables[tn].in_bwd[0] = wo->pe_ic[0];
				tables[tn].in_bwd[1] = wo->pe_ic[1];
				tables[tn].in_bwd[2] = wo->pe_ic[2];
				tables[tn].clut_bwd = wo->pe_cl;
				tables[tn].out_bwd = wo->pe_oc[0];

				if (icp->e.c != ICM_ERR_OK)
					return icp->e.c;
			}
			else {
				return icm_err(icp, ICM_ERR_CRM_XFORMS_BADTTYPE, "icc_create_mono_xforms got unexpected tag type '%s'", icmTypeSig2str(sigs[tn].ttype));
			}
		}
		else {
				return icm_err(icp, ICM_ERR_CRM_XFORMS_BADSIG, "icc_create_mono_xforms got unexpected sig '%s'", icmTagSig2str(sigs[tn].sig));
		}
	}	/* Next table */

	/* Fill in the transform values */
	for (tn = 0; tn < ntables; tn++) {
		double nwp[3];			/* (possibly) Normalized white point */
		double nbp[3];			/* (possibly) Normalized black point */
		int wplix;				/* wp luminance index */
	
		/* Compute the normalized PCS white */
		if (outsig == icSigLabData) {
			nwp[0] = 100.0, nwp[1] = 0.0, nwp[2] = 0.0;
			nbp[0] = 0.0, nbp[1] = 0.0, nbp[2] = 0.0;
			wplix = 0;
		} else {
			icmXYZ2Ary(nwp, icp->header->illuminant);
			nbp[0] = 0.0, nbp[1] = 0.0, nbp[2] = 0.0;
			wplix = 1;
		}
	
		if (tables[tn].pcs_nf != NULL) {
			tables[tn].pcs_nf->lookup_fwd(tables[tn].pcs_nf, nwp, nwp);
			tables[tn].pcs_nf->lookup_fwd(tables[tn].pcs_nf, nbp, nbp);
		}
		
		/* - - - - - fwd transform - - - - - - - */
		/* Fill in the input table */
		if (tables[tn].in_fwd != NULL) {
	
			/* Create the input table entry values */
			for (i = 0; i < tables[tn].in_fwd->count; i++) {
				double fv = i/(tables[tn].in_fwd->count-1.0);
		
				*((int *)&iv[-((int)0)-1]) = i;			/* Trick to supply grid index in iv[] */
		
				icmSetN(iv, fv, inputChan);
		
				if (infunc != NULL)
					infunc(cbctx, iv, iv, tn);	/* inspace -> input function -> inspace' */
		
				if (tables[tn].clip_ent) {
					if (icmClipNmarg(iv, iv, inputChan) > CLIP_MARGIN)
						clip |= 1;
				}
		
				tables[tn].in_fwd->data[i] = iv[0];
			}
		}
	
		/* Create a matrix mono->PCS */
		if (tables[tn].matrix_fwd != NULL) {
			for (i = 0; i < 3; i++)
				tables[tn].matrix_fwd->mx[i][0] = nwp[i];
		}
	
		/* or create a cLut mono->PCS */
		if (tables[tn].clut_fwd != NULL) {
	
			/* Just two entries to interp between 0 and wp */
			icmCpy3(tables[tn].clut_fwd->clutTable, nbp);
			icmCpy3(tables[tn].clut_fwd->clutTable + 3, nwp);
	
		}
	
		/* Create a NOP output table */
		if (tables[tn].out_fwd[0] != 0) {

			for (e = 0; e < 3; e++) {
				tables[tn].out_fwd[e]->data[0] = 0.0;
				tables[tn].out_fwd[e]->data[1] = 1.0;
			}
		}
	

		/* - - - - - bwd transform - - - - - - - */
		/* Create a NOP/divide by wp inverse matrix mono->PCS */
		if (tables[tn].in_bwd[0] != NULL) {
	
			/* Create the input table entry values */
			for (i = 0; i < tables[tn].in_bwd[0]->count; i++) {
				double fv = i/(tables[tn].in_bwd[0]->count-1.0);
		
				if (outsig == icSigLabData) {
					iv[0] = fv/nwp[0];
					iv[1] = fv;
					iv[2] = fv;

				} else {
					for (f = 0; f < 3; f++)
						iv[f] = fv/nwp[f]; 
				}

				icmClip3(iv, iv);

				for (f = 0; f < 3; f++)
					tables[tn].in_bwd[f]->data[i] = iv[f];
			}
		}

		/* Create an inverse matrix */
		if (tables[tn].matrix_bwd != NULL) {

			for (i = 0; i < 3; i++) {
				if (i == wplix)
					tables[tn].matrix_bwd->mx[0][i] = 1.0/nwp[i];
				else
					tables[tn].matrix_bwd->mx[0][i] = 0.0;
			}
	
		}
	
		/* else create PCS->mono cLut */
		if (tables[tn].clut_bwd != NULL) {
			for (e = 0; e < 8 ; e++) {				/* Cube index */
				if ((e & (4 >> wplix)) != 0) 		/* Luminance coord max */
					tables[tn].clut_bwd->clutTable[e] = 1.0; 
				else
					tables[tn].clut_bwd->clutTable[e] = 0.0;
			}
		}
	
		/* Create output table */
		if (tables[tn].out_bwd != NULL) {
			if (tables[tn].in_fwd == NULL)
				return icm_err(icp, ICM_ERR_CRM_XFORMS_NOFWD, "icc_create_mono_xforms got no fwd table to create bwd");

			for (i = 0; i < tables[tn].out_bwd->count; i++) {
				double fv = i/(tables[tn].out_bwd->count-1.0);
				iv[0] = fv;
		
				tables[tn].in_fwd->lookup_bwd(tables[tn].in_fwd, iv, iv);
		
				if (tables[tn].clip_ent) {
					if (icmClipNmarg(iv, iv, inputChan) > CLIP_MARGIN)
						clip |= 1;
				}
				tables[tn].out_bwd->data[i] = iv[0];
			}
		}

		/* Free up transforms */
		if (tables[tn].pcs_nf != NULL)
			tables[tn].pcs_nf->del(tables[tn].pcs_nf);
	}

	icp->al->free(icp->al, tables);

	return ICM_ERR_OK;
}

/* ============================================================ */
/* Helper function to set multiple Matrix tags simultaneously. */
/* Note that these tables and matrix value all have to be */
/* compatible in having the same configuration and resolutions. */
/* Set errc and return error number in underlying icc */
/* Note that clutfunc in[] value has "index under". */
/* Returns ec */
int icc_create_matrix_xforms(
	struct _icc *icp,
	int     flags,					/* Setting flags */
	void   *cbctx,					/* Opaque callback context pointer value */

	int ntables,					/* Number of tables to be set, 1..n */
	icmXformSigs *sigs,				/* signatures and tag types for each table */
									/* ICCV2 Matrix/gamma/shaper uses icmSigShaperMatrix */
									/* Valid combinations are:
									  icmSigShaperMatrix + icmSigShaperMatrixType */ 

	unsigned int	inputEnt,       /* Num of in-table entries */
	unsigned int	inv_inputEnt,   /* Num of inverse in-table entries */

	icColorSpaceSignature insig, 	/* Input color space */
	icColorSpaceSignature outsig, 	/* Output color space */

	void (*infunc)(void *cbctx, double *out, double *in, int tn),
						/* Input transfer function, inspace->inspace' (NULL = default) */
						/* Will be called ntables times each input grid value */

	double mx[3][3],	/* [outn][inn] Matrix values */
	double ct[3],		/* [outn] Constant values added after mx[][] multiply */
						/* Must be NULL or 0.0 if a table uses icmSigShaperMatrix */
	int isShTRC,		/* NZ if shared TRCs */
	double *gamma,		/* != NULL if gamma rather than shaper. Only one gamma if isShTRC */
	int isLinear		/* NZ if pure linear, gamma = 1.0 */
) {
	/* Pointers to elements to set */
	struct {
		icmBase *wo_fwd;					/* TagType */
		icmPeCurve   *in_fwd[3];			/* Input curve */
		icmXYZArray  *chmx_fwd[3];			/* Matrix as RGB channels */
		icmPeMatrix  *matrix_fwd;			/* Matrix */

		icmBase *wo_bwd;					/* bwd TagType */
		icmPeMatrix  *matrix_bwd;			/* Inverse matrix */
		icmPeCurve   *out_bwd[3];			/* bwd output curve */

		icmPe *pcs_nf;				/* PCS normalization function */
		int clip_ent;				/* nz to clip entry values of tables */
	} *tables = NULL;
	int inputChan, outputChan;
	int ii[MAX_CHAN];		/* Index value */
	double _iv[2 * MAX_CHAN], *iv = &_iv[MAX_CHAN], *ivn;	/* Real index value/table value */
	int tn, i, e, f;
	int clip = 0;

	if (isLinear)			/* isLinear takes priority */
		gamma = NULL;

	inputChan = icmCSSig2nchan(insig);
	outputChan = icmCSSig2nchan(outsig);

	if (inputChan != 3 || outputChan != 3 || outsig != icSigXYZData)
		return icm_err(icp, ICM_ERR_CRX_XFORMS_BADNCHAN, "icc_create_matrix_xforms got unexpected no. inChan %d and outChan %d or output space %s",inputChan, outputChan,icmColorSpaceSig2str(outsig));

	/* Allocate the per table info */
	if ((tables = icp->al->calloc(icp->al, ntables, sizeof(*tables))) == NULL) {
		return icm_err(icp, ICM_ERR_MALLOC, "Allocating icc_create_matrix_xforms tables failed");
	}

	/* Create tags and tagtypes for each table. */
	for (tn = 0; tn < ntables; tn++) {
		if (sigs[tn].sig == icmSigShaperMatrix
		 && sigs[tn].ttype == icmSigShaperMatrixType) {
			/* ---------------------------------------------------------- */
			icmCurve *wcr, *wcg, *wcb;
			icmXYZArray *wmr, *wmg, *wmb;

			tables[tn].clip_ent = 1;

			if (ct != NULL) {
				for (i = 0; i < 3; i++) {
					if (ct[i] != 0.0)
						break;
				}
				if (i < 3)
					return icm_err(icp, ICM_ERR_CRX_XFORMS_NZCONST, "icc_create_matrix_xforms got non-zero constant %s",icmPdv(3,ct));
			}

			/* Delete any existing input curve tags */
			if (icp->delete_tag_quiet(icp, icSigRedTRCTag) != ICM_ERR_OK
			 || icp->delete_tag_quiet(icp, icSigGreenTRCTag) != ICM_ERR_OK
			 || icp->delete_tag_quiet(icp, icSigBlueTRCTag) != ICM_ERR_OK)
				return icp->e.c;

			if (isShTRC) {  /* Make TRCs shared and the same value(s) */
				if ((wcr = (icmCurve *)icp->add_tag(icp, icSigRedTRCTag, icSigCurveType)) == NULL 
				 || (wcg = (icmCurve *)icp->link_tag(icp, icSigGreenTRCTag, icSigRedTRCTag))
				                                                                          == NULL 
				 || (wcb = (icmCurve *)icp->link_tag(icp, icSigBlueTRCTag, icSigRedTRCTag))
				                                                                          == NULL) 
					return icp->e.c;
			} else {
				if ((wcr = (icmCurve *)icp->add_tag(icp, icSigRedTRCTag, icSigCurveType)) == NULL 
				 || (wcg = (icmCurve *)icp->add_tag(icp, icSigGreenTRCTag, icSigCurveType)) == NULL 
				 || (wcb = (icmCurve *)icp->add_tag(icp, icSigBlueTRCTag, icSigCurveType)) == NULL) 
					return icp->e.c;
			}

			/* Set gamma or linear */
			if (gamma != NULL) {

				if (isShTRC) {
					wcr->ctype = icmCurveGamma; 
					if (wcr->allocate(wcr))
						return icp->e.c;
					wcr->data[0] = gamma[0];				
				} else {
					wcr->ctype = wcg->ctype = wcb->ctype = icmCurveGamma; 
					if (wcr->allocate(wcr) || wcg->allocate(wcg) || wcb->allocate(wcb))
						return icp->e.c;
					wcr->data[0] = gamma[0];				
					wcg->data[0] = gamma[1];				
					wcb->data[0] = gamma[2];				
				}

			} else if (isLinear) {
				if (isShTRC) {
					wcr->ctype = icmCurveLin; 
					if (wcr->allocate(wcr))
						return icp->e.c;
				} else {
					wcr->ctype = wcg->ctype = wcb->ctype = icmCurveLin; 
					if (wcr->allocate(wcr) || wcg->allocate(wcg) || wcb->allocate(wcb))
						return icp->e.c;
				}

			/* Prep table for callback */
			} else {
				wcr->ctype = wcg->ctype = wcb->ctype = icmCurveSpec; 
				wcr->count = wcg->count = wcb->count = inputEnt;
				if (wcr->allocate(wcr) || wcg->allocate(wcg) || wcb->allocate(wcb))
					return icp->e.c;

				tables[tn].in_fwd[0] = wcr;
				tables[tn].in_fwd[1] = wcg;
				tables[tn].in_fwd[2] = wcb;
			}

			/* Delete any existing colorant tags */
			if (icp->delete_tag_quiet(icp, icSigRedColorantTag) != ICM_ERR_OK
			 || icp->delete_tag_quiet(icp, icSigGreenColorantTag) != ICM_ERR_OK
			 || icp->delete_tag_quiet(icp, icSigBlueColorantTag) != ICM_ERR_OK)
				return icp->e.c;

			if ((wmr = (icmXYZArray *)icp->add_tag(icp, icSigRedColorantTag, icSigXYZArrayType))
			                                                                             == NULL 
			 || (wmg = (icmXYZArray *)icp->add_tag(icp, icSigGreenColorantTag, icSigXYZArrayType))
			                                                                             == NULL 
			 || (wmb = (icmXYZArray *)icp->add_tag(icp, icSigBlueColorantTag, icSigXYZArrayType))
			                                                                             == NULL)
				return icp->e.c;

			wmr->count = wmg->count = wmb->count = 1;
			if (wmr->allocate(wmr) || wmg->allocate(wmg) || wmb->allocate(wmb))
				return icp->e.c;
			tables[tn].chmx_fwd[0] = wmr;
			tables[tn].chmx_fwd[1] = wmg;
			tables[tn].chmx_fwd[2] = wmb;
		}
		else {
			return icm_err(icp, ICM_ERR_CRX_XFORMS_BADSIG, "icc_create_matrix_xforms got unexpected sig '%s' or type '%s'", icmTagSig2str(sigs[tn].sig), icmTypeSig2str(sigs[tn].ttype));
		}
	}	/* Next table */

	/* Fill in the transform values */
	for (tn = 0; tn < ntables; tn++) {
		double tmx[3][3];
		double tct[3];
	
		/* - - - - - fwd transform - - - - - - - */
		/* Fill in the input table */
		if (tables[tn].in_fwd[0] != NULL) {
	
			/* Create the input table entry values */
			for (i = 0; i < inputEnt; i++) {
				double fv = i/(inputEnt-1.0);
		
				*((int *)&iv[-((int)0)-1]) = i;			/* Trick to supply grid index in iv[] */
		
				icmSetN(iv, fv, inputChan);
		
				if (infunc != NULL)
					infunc(cbctx, iv, iv, tn);	/* inspace -> input function -> inspace' */

				if (isShTRC) {
					if (tables[tn].clip_ent) {
						if (icmClipNmarg(iv, iv, 1) > CLIP_MARGIN)
							clip |= 1;
					}
					for (e = 0; e < inputChan; e++)
						tables[tn].in_fwd[e]->data[i] = iv[0];
				} else {
					if (tables[tn].clip_ent) {
						if (icmClipNmarg(iv, iv, inputChan) > CLIP_MARGIN)
							clip |= 1;
					}
					for (e = 0; e < inputChan; e++)
						tables[tn].in_fwd[e]->data[i] = iv[e];
				}
			}
		}

	
		if (tables[tn].chmx_fwd[0] != NULL) {
			tables[tn].chmx_fwd[0]->data[0].X = mx[0][0];
			tables[tn].chmx_fwd[0]->data[0].Y = mx[1][0];
			tables[tn].chmx_fwd[0]->data[0].Z = mx[2][0];
			tables[tn].chmx_fwd[1]->data[0].X = mx[0][1];
			tables[tn].chmx_fwd[1]->data[0].Y = mx[1][1];
			tables[tn].chmx_fwd[1]->data[0].Z = mx[2][1];
			tables[tn].chmx_fwd[2]->data[0].X = mx[0][2];
			tables[tn].chmx_fwd[2]->data[0].Y = mx[1][2];
			tables[tn].chmx_fwd[2]->data[0].Z = mx[2][2];
		}

		/* Copy and scale matrix */
		if (tables[tn].matrix_fwd != NULL) {

			icmCpy3x3(tmx, mx);
			if (ct != NULL)
				icmCpy3(tct, ct);
			else
				icmSet3(tct, 0.0);

			if (tables[tn].pcs_nf != NULL) {
				icmTranspose3x3(tmx, tmx);
				for (f = 0; f < 3; f++) {
					tables[tn].pcs_nf->lookup_fwd(tables[tn].pcs_nf, tmx[f], tmx[f]);
				}
				tables[tn].pcs_nf->lookup_fwd(tables[tn].pcs_nf, tct, tct);
				icmTranspose3x3(tmx, tmx);
			}

			for (f = 0; f < 3; f++) {
				for (e = 0; e < 3; e++)
					tables[tn].matrix_fwd->mx[f][e] = tmx[f][e];
				tables[tn].matrix_fwd->ct[f] = tct[f];
			}
		}
	
		/* - - - - - bwd transform - - - - - - - */

		/* Create an inverse matrix */
		if (tables[tn].matrix_bwd != NULL) {
			double imx[3][3];
			double ict[3];
	
			if (icmInverse3x3(imx, tmx))
				return icm_err(icp, ICM_ERR_CRX_XFORMS_NOIMX, "icc_create_matrix_xforms unable to invert fwd matrix");

			icmMulBy3x3(ict, imx, tct);
			icmScale3(ict, ict, -1.0);
			
			for (f = 0; f < 3; f++) {
				for (e = 0; e < 3; e++)
					tables[tn].matrix_bwd->mx[f][e] = imx[f][e];
				tables[tn].matrix_bwd->ct[f] = ict[f];
			}
		}
	
		/* Create output table */
		if (tables[tn].out_bwd[0] != NULL) {
			if (tables[tn].in_fwd[0] == NULL)
				return icm_err(icp, ICM_ERR_CRX_XFORMS_NOFWD, "icc_create_matrix_xforms got no fwd table to create bwd");

			for (i = 0; i < inv_inputEnt; i++) {
				double fv = i/(inv_inputEnt-1.0);
		
				for (f = 0; f < 3; f++) {
					iv[0] = fv;
					tables[tn].in_fwd[f]->lookup_bwd(tables[tn].in_fwd[f], iv, iv);
		
					if (tables[tn].clip_ent) {
						if (icmClipNmarg(iv, iv, 1) > CLIP_MARGIN)
							clip |= 1;
					}
					tables[tn].out_bwd[f]->data[i] = iv[0];
				}
			}
		}


		/* Free up transforms */
		if (tables[tn].pcs_nf != NULL)
			tables[tn].pcs_nf->del(tables[tn].pcs_nf);
	}

	icp->al->free(icp->al, tables);

	return ICM_ERR_OK;
}

/* ============================================================ */
/* Function to set multiple Lut tables simultaneously. */
/* Note that these tables all have to be compatible in */
/* having the same configuration and resolutions, and the */
/* same per channel input and output curves. */
/* Set errc and return error number in underlying icc */
/* Returns ec */
int icc_create_lut_xforms(
	icc    *icp,
	int     flags,					/* Setting flags */
	void   *cbctx,					/* Opaque callback context pointer value */

	int ntables,					/* Number of tables to be set, 1..n */
	icmXformSigs *sigs,				/* signatures and tag types for each table */

	unsigned int	bpv,			/* Bytes per value of AToB or BToA CLUT, 1 or 2 */
	unsigned int	inputEnt,       /* Num of in-table entries (must be 256 for Lut8 */
	unsigned int	clutPoints[MAX_CHAN],    /* Num of grid points (must be same for Lut8/16) */
	unsigned int	outputEnt,      /* Num of out-table entries (must be 256 for Lut8) */

	icColorSpaceSignature insig, 	/* Input color space */
	icColorSpaceSignature outsig, 	/* Output color space */

	double *inmin, double *inmax,	/* Maximum range of inspace values (Not used)  */
									/* (NULL = default) */
	void (*infunc)(void *cbctx, double *out, double *in, int tn),
						/* Input transfer function, inspace->inspace' (NULL = default) */
						/* Will be called ntables times each input grid value */
	double *indmin, double *indmax,	/* Maximum range of inspace' values */
									/* (NULL = same as inmin/max or default) */
	void (*clutfunc)(void *cbntx, double *out, double *in, int tn),
						/* inspace' -> outspace[ntables]' transfer function */
						/* will be called once for each input' grid value, and ntable times */
						/* Note that in[] value has "index under". */
	double *clutmin, double *clutmax,	/* Maximum range of outspace' values */
										/* (NULL = default) */
	void (*outfunc)(void *cbntx, double *out, double *in, int tn),
							/* Output transfer function, outspace'->outspace (NULL = deflt) */
							/* Will be called ntables times on each output value */

	int *apxls_gmin, int *apxls_gmax/* If not NULL, the grid indexes not to be affected */
									/* by ICM_CLUT_SET_APXLS, defaulting to 0..>clutPoints-1 */
) {
	/* Pointers to elements to set */
	struct {
		icmBase *wo;					/* TagType */
		icmPeCurve *in[MAX_CHAN];		/* Associated Pe's */
		icmPeClut  *clut;
		icmPeCurve *out[MAX_CHAN];
		icmPe *in_nf;				/* input normalization transform (NULL if NOP) */
		icmPe *ind_nf;				/* input' normalization transform (NULL if NOP) */
		icmPe *outd_nf;				/* output' normalization transform (NULL if NOP) */
		icmPe *out_nf;				/* output normalization transform (NULL if NOP) */

		int clip_ent;				/* nz to clip entry values of tables */
	} *tables = NULL;
	int inputChan, outputChan;
	int ii[MAX_CHAN];		/* Index value */
	psh counter;			/* Pseudo-Hilbert counter */
	double _iv[2 * MAX_CHAN], *iv = &_iv[MAX_CHAN], *ivn;	/* Real index value/table value */
	double ivc[MAX_CHAN];									/* Copy of iv */
	double **clutTable2 = NULL;		/* Cell center values for ICM_CLUT_SET_APXLS */ 
	int def_apxls_gmin[MAX_CHAN], def_apxls_gmax[MAX_CHAN];
	int tn, i, e, f;
	int clip = 0;

	inputChan = icmCSSig2nchan(insig);
	outputChan = icmCSSig2nchan(outsig);

	/* Allocate the per table info */
	if ((tables = icp->al->calloc(icp->al, ntables, sizeof(*tables))) == NULL) {
		return icm_err(icp, ICM_ERR_MALLOC, "Allocating icc_create_lut_xforms tables failed");
	}

	/* Create tags and tagtypes for each table. */
	for (tn = 0; tn < ntables; tn++) {
		if (sigs[tn].sig == icSigAToB0Tag
		 || sigs[tn].sig == icSigAToB1Tag
		 || sigs[tn].sig == icSigAToB2Tag
		 || sigs[tn].sig == icSigBToA0Tag
		 || sigs[tn].sig == icSigBToA1Tag
		 || sigs[tn].sig == icSigBToA2Tag
		 || sigs[tn].sig == icSigGamutTag) {

			if (sigs[tn].ttype == icSigLut16Type
			 || sigs[tn].ttype == icSigLut8Type) {
				/* ---------------------------------------------------------- */
				icColorSpaceSignature ninsig; 		/* Normalized input space */
				icColorSpaceSignature noutsig;		/* Normalized output space */ 
				icmLut1 *wo;

				ninsig = icmSig2NormSig(icp, insig, icmEncV2,
				                   sigs[tn].ttype == icSigLut16Type ? icmEnc16bit : icmEnc8bit);  
				noutsig = icmSig2NormSig(icp, outsig, icmEncV2,
				                    sigs[tn].ttype == icSigLut16Type ? icmEnc16bit : icmEnc8bit);  

				tables[tn].clip_ent = 1;

				/* Delete any existing tag */
				if (icp->delete_tag_quiet(icp, sigs[tn].sig) != ICM_ERR_OK)
					return icp->e.c;

				if ((tables[tn].wo = icp->add_tag(icp, sigs[tn].sig, sigs[tn].ttype)) == NULL) 
					return icp->e.c;
				wo = (icmLut1 *)tables[tn].wo;

				/* Allocate space in tags */
				wo->inputChan = inputChan;
				wo->outputChan = outputChan;
				if (sigs[tn].ttype == icSigLut8Type)
					wo->outputEnt = wo->inputEnt = 256; 
				else {
					wo->inputEnt = inputEnt; 
					wo->outputEnt = outputEnt;
				}
				/* Check all clutPoints[] are the same */
				for (i = 1; i < inputChan; i++) { 
					if (clutPoints[i] != clutPoints[0])
						return icm_err(icp, ICM_ERR_CRL_XFORMS_BAD_CLUTRES, "icc_create_lut_xforms got non-equal clutPoints[%d] %d != clutPoints[0] %d",i,clutPoints[i],clutPoints[0]);
				}

				wo->clutPoints = clutPoints[0];
				if (wo->allocate(wo))
					return icp->e.c;

				/* Get pointers to tables to be filled */
				for (i = 0; i < inputChan; i++) 
					tables[tn].in[i] = wo->pe_ic[i];
				tables[tn].clut = wo->pe_cl;
				for (i = 0; i < outputChan; i++) 
					tables[tn].out[i] = wo->pe_oc[i];

				/* Setup basic normalization Pe's */
				tables[tn].in_nf = new_icmNSig2NormPe(icp, NULL, ninsig, 0, 1);
				if (indmin == NULL || indmax == NULL) {
					tables[tn].ind_nf = new_icmNSig2NormPe(icp, NULL, ninsig, 0, 1);   
				} else {
					tables[tn].ind_nf = new_icmPeGeneric2Norm(icp, inputChan, indmin, indmax, "cluti", 0); 
				}
				if (clutmin == NULL || clutmax == NULL) {
					tables[tn].outd_nf = new_icmNSig2NormPe(icp, NULL, noutsig, 0, 1);   
				} else {
					tables[tn].outd_nf = new_icmPeGeneric2Norm(icp, outputChan, clutmin, clutmax, "cluto", 0); 
				}

				tables[tn].out_nf = new_icmNSig2NormPe(icp, NULL, noutsig, 0, 1);   

				if (icp->e.c != ICM_ERR_OK)
					return icp->e.c;
			}
			else {
				return icm_err(icp, ICM_ERR_CRL_XFORMS_BADTTYPE, "icc_create_lut_xforms got unexpected tag type '%s'", icmTypeSig2str(sigs[tn].ttype));
			}
		}
		  else {
				return icm_err(icp, ICM_ERR_CRL_XFORMS_BADSIG, "icc_create_lut_xforms got unexpected sig '%s'", icmTagSig2str(sigs[tn].sig));
		}

#ifdef PUT_PCS_WHITE_ON_GRIDPOINT
		/* If Lab or XYZ input, map the PCS white to a cLUT grid */
		/* Only do this if input table res. is sufficient to represent break accurately. */
		if ((icmCSSig2type(insig) & CSSigType_PCS)
		 && inputEnt >= 256
		 && sigs[tn].ttype != icSigLut8Type) {
			double wp[3];
			icmPe *ga;
	
			if (insig == icSigLabData)
				wp[0] = 100.0, wp[1] = 0.0, wp[2] = 0.0;
			else
				icmXYZ2Ary(wp, icp->header->illuminant);
	
//printf("~1 grid align res = %s\n",icmPiv(3,clutPoints));
//printf("~1 grid point align input src = %s\n",icmPdv(3,wp));
			/* Transform white point through input table function */ 
			if (infunc != NULL) {
				infunc(cbctx, wp, wp, tn);	/* inspace -> input function -> inspace' */
			}
	
//printf("~1 grid point align src' = %s\n",icmPdv(3,wp));

			/* And in' normalization function */
			if (tables[tn].ind_nf != NULL) /* Normalized to full range */
				tables[tn].ind_nf->lookup_fwd(tables[tn].ind_nf, wp, wp);
	
//printf("~1 grid point align norm src' = %s\n",icmPdv(3,wp));

			/* Create grid point adjustment */
			if ((ga = new_icmPeGridAlign(icp, inputChan, wp, clutPoints, 0)) == NULL)
				return icp->e.c;

//printf("~1 grid point align dst = %s\n",icmPdv(3,((icmPeGridAlign *)ga)->dst));
	
			/* Append the ga after any current ind_nf */
			if (tables[tn].ind_nf == NULL) {
//printf("~1 added as ind_nf\n");
				tables[tn].ind_nf = ga;
			} else {
				icmPeContainer *seq;
	
				if ((seq = new_icmPeContainer(icp, inputChan, inputChan)) == NULL)
					return icp->e.c;
				seq->append(seq, tables[tn].ind_nf); 
				tables[tn].ind_nf->del(tables[tn].ind_nf);		/* Container has taken ref. */
				seq->append(seq, ga); 
				ga->del(ga);									/* Container has taken ref. */
				seq->init(seq);
				tables[tn].ind_nf = (icmPe *)seq;
//printf("~1 appended to ind_nf\n");
			}
		}
#endif /* PUT_PCS_WHITE_ON_GRIDPOINT */
	}		/* Next tn */

	/* Allocate space for cell center value lookup */
	if (flags & ICM_CLUT_SET_APXLS) {
		int clutsize = 1;

		for (i = 0; i < inputChan; i++)
			clutsize = sat_mul(clutPoints[i], clutsize);
		clutsize = sat_mul(outputChan, clutsize);

		if (apxls_gmin == NULL) {
			apxls_gmin = def_apxls_gmin;
			for (e = 0; e < inputChan; e++)
				apxls_gmin[e] = 0;
		}
		if (apxls_gmax == NULL) {
			apxls_gmax = def_apxls_gmax;
			for (e = 0; e < inputChan; e++)
				apxls_gmax[e] = clutPoints[e]-1;
		}

		if ((clutTable2 = (double **) icp->al->calloc(icp->al,sizeof(double *), ntables)) == NULL) {
			icp->al->free(icp->al, _iv);
			return icm_err(icp, 1,"icmLut_set_tables malloc of cube center array failed");
		}
		for (tn = 0; tn < ntables; tn++) {
			if ((clutTable2[tn] = (double *) icp->al->calloc(icp->al,sizeof(double),
			                                               clutsize)) == NULL) {
				for (--tn; tn >= 0; tn--)
					icp->al->free(icp->al, clutTable2[tn]);
				icp->al->free(icp->al, _iv);
				icp->al->free(icp->al, clutTable2);
				return icm_err(icp, ICM_ERR_CRL_XFORMS_C2MALLOC,"icmLut_set_tables malloc of cube center array failed");
			}
		}
	}

	/* Now fill in the table values: */

	/* Create the input table entry values */
	for (tn = 0; tn < ntables; tn++) {
		for (i = 0; i < inputEnt; i++) {
			double fv = i/(inputEnt-1.0);

			*((int *)&iv[-((int)0)-1]) = i;			/* Trick to supply grid index in iv[] */

			icmSetN(iv, fv, inputChan);

			/* Normalized to full range */
			if (tables[tn].in_nf != NULL)
				tables[tn].in_nf->lookup_bwd(tables[tn].in_nf, iv, iv);

			if (infunc != NULL)
				infunc(cbctx, iv, iv, tn);	/* inspace -> input function -> inspace' */

			/* Full range to normalized */
			if (tables[tn].ind_nf != NULL)
				tables[tn].ind_nf->lookup_fwd(tables[tn].ind_nf, iv, iv);

			/* Note that if the range is reduced and clipping occurs, */
			/* then the resolution within the input table should be made */
			/* large enough to be able to represent the sharp edges of the */
			/* clipping. */
			if (tables[tn].clip_ent) {
				if (icmClipNmarg(iv, iv, inputChan) > CLIP_MARGIN)
					clip |= 1;
			}

			for (e = 0; e < inputChan; e++)
				tables[tn].in[e]->data[i] = iv[e];
		}
	}

	/* Create the multi-dimensional lookup table values */

	/* To make this clut function cache friendly, we use the pseudo-hilbert */
	/* count sequence. This keeps each point close to the last in the */
	/* multi-dimensional space. This is the point of setting multiple Luts at */ 
	/* once too - the assumption is that these tables are all related (different */
	/* gamut compressions for instance), and hence calling the clutfunc() with */
	/* close values will maximise reverse lookup cache hit rate. */
	psh_initN(&counter, inputChan, clutPoints, ii);	/* Initialise counter */

	/* Itterate through all verticies in the grid */
	for (;;) {
		int ti;		/* Table index */
	
		/* (Note that we assume all the cLuts have the same res[] and dinc) */
		for (ti = e = 0; e < inputChan; e++) { 			/* Input tables */
			ti += ii[e] * tables[0].clut->dinc[e];		/* Clut index */
			ivc[e] = iv[e] = ii[e]/(clutPoints[e]-1.0);	/* Vertex value */
			*((int *)&iv[-((int)e)-1]) = ii[e];			/* Trick to supply grid index in iv[] */
		}

//printf("~1 clut coord %s\n",icmPiv(inputChan, ii));
//printf("~1 clut index %d\n",ti);
//printf("~1 clut in value %s\n",icmPdv(inputChan, iv));

		for (tn = 0; tn < ntables; tn++) {

			if (tn > 0)
				for (e = 0; e < inputChan; e++)		/* Restore iv */
					iv[e] = ivc[e];
	
			/* Normalized to full range */
			if (tables[tn].ind_nf != NULL)
				tables[tn].ind_nf->lookup_bwd(tables[tn].ind_nf, iv, iv);
//printf("~1 tn %d clut normed in value %s\n",tn,icmPdv(inputChan, iv));

			/* Lookup our cLut function */
			if (clutfunc != NULL)
				clutfunc(cbctx, iv, iv, tn);
//printf("~1 tn %d clut normed out value %s\n",tn,icmPdv(inputChan, iv));

			/* Full range to normalized */
			if (tables[tn].outd_nf != NULL)
				tables[tn].outd_nf->lookup_fwd(tables[tn].outd_nf, iv, iv);
//printf("~1 tn %d clut out value %s\n",tn,icmPdv(inputChan, iv));

			/* Clip */
			if (tables[tn].clip_ent) {
				if (icmClipNmarg(iv, iv, outputChan) > CLIP_MARGIN)
					clip |= 2;
			}
//printf("~1 tn %d clut clipped value %s\n",tn,icmPdv(inputChan, iv));
	
			for (f = 0; f < outputChan; f++)
				tables[tn].clut->clutTable[ti + f] = iv[f];

			/* Lookup cell center value if ICM_CLUT_SET_APXLS */
			if (clutTable2 != NULL) {

				for (e = 0; e < inputChan; e++) {
					if (ii[e] < apxls_gmin[e]
					 || ii[e] >= apxls_gmax[e])
						break;							/* Don't lookup outside least squares area */
					iv[e] = (ii[e] + 0.5)/(clutPoints[e]-1.0);		/* Vertex coordinates + 0.5 */
					*((int *)&iv[-((int)e)-1]) = -ii[e]-1;	/* Trick to supply -ve grid index in iv[] */
												    /* (Not this is only the base for +0.5 center) */
				}

				if (e >= inputChan) {	/* We're not on the last row */
			
					/* Normalized to full range */
					if (tables[tn].ind_nf != NULL)
						tables[tn].ind_nf->lookup_bwd(tables[tn].ind_nf, iv, iv);
		
					/* Lookup our cLut function */
					if (clutfunc != NULL)
						clutfunc(cbctx, iv, iv, tn);
		
					/* Full range to normalized */
					if (tables[tn].outd_nf != NULL)
						tables[tn].outd_nf->lookup_fwd(tables[tn].outd_nf, iv, iv);
		
					/* Clip */
					if (tables[tn].clip_ent) {
						if (icmClipNmarg(iv, iv, outputChan) > CLIP_MARGIN)
							clip |= 4;
					}
		
					for (f = 0; f < outputChan; f++) 	/* Output chans */
						clutTable2[tn][ti + f] = iv[f];
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
		double cw = 1.0/(double)(1 << inputChan);		/* Weight for each cube corner */
		icmPeClut *clut = tables[0].clut;				/* Use first cLut offset info */

		/* For each cell center point except last row because we access ii[e]+1 */  
		for (e = 0; e < inputChan; e++)
			ii[e] = apxls_gmin[e];	/* init coords */

		/* Compute linear interpolated value from center values */
		for (ee = 0; ee < inputChan;) {

			/* Compute base index for table2 */
			for (ti2 = e = 0; e < inputChan; e++)  	/* Input tables */
				ti2 += ii[e] * clut->dinc[e];				/* Clut index */

			ti = ti2 + clut->dcube[(1 << inputChan)-1];	/* +1 to each coord for vertex index */

			for (tn = 0; tn < ntables; tn++) {
				double mval[MAX_CHAN], vv;
				double maxd = 0.0;

				clut = tables[tn].clut;

				/* Compute mean of center values */
				for (f = 0; f < outputChan; f++) { 	/* Output chans */

					mval[f] = 0.0;
					for (i = 0; i < (1 << inputChan); i++) { /* For surrounding center values */
						mval[f] += clutTable2[tn][ti2 + clut->dcube[i] + f];
					}
					mval[f] = clut->clutTable[ti + f] - mval[f] * cw;		/* Diff to mean */
					vv = fabs(mval[f]);
					if (vv > maxd)
						maxd = vv;
				}
			
				if (outputChan <= 3 || maxd < APXLS_DIFF_THRHESH) {
					for (f = 0; f < outputChan; f++) { 	/* Output chans */
				
						vv = clut->clutTable[ti + f] + APXLS_WHT * mval[f];
	
						if (tables[tn].clip_ent) {
							/* Hmm. This is a bit crude. How do we know valid range is 0-1 ? */
							/* What about an ink limit ? */
							if (vv < 0.0) {
								vv = 0.0;
							} else if (vv > 1.0) {
								vv = 1.0;
							}
						}
//						printf("~1 delta[%d] = %e\n",f,fabs(clut->clutTable[ti + f] - vv));
						clut->clutTable[ti + f] = vv;
					}
//					printf("~1 nix %s apxls ov %s\n",icmPiv(inputChan, ii), icmPdv(outputChan, clut->clutTable + ti));
				}
			}

			/* Increment coord */
			for (ee = 0; ee < inputChan; ee++) {
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

	/* Create the output table entry values */
	for (tn = 0; tn < ntables; tn++) {

		for (i = 0; i < outputEnt; i++) {
			double fv = i/(outputEnt-1.0);

			*((int *)&iv[-((int)0)-1]) = i;			/* Trick to supply grid index in iv[] */
			icmSetN(iv, fv, outputChan);

			/* Normalized to full range */
			if (tables[tn].outd_nf != NULL)
				tables[tn].outd_nf->lookup_bwd(tables[tn].outd_nf, iv, iv);

			if (outfunc != NULL)
				outfunc(cbctx, iv, iv, tn);	/* outspace' -> output function -> outspace */

			/* Full range to normalized */
			if (tables[tn].out_nf != NULL)
				tables[tn].out_nf->lookup_fwd(tables[tn].out_nf, iv, iv);

			/* Clip */
			if (tables[tn].clip_ent) {
				if (icmClipNmarg(iv, iv, inputChan) > CLIP_MARGIN)
					clip |= 8;
			}

			for (f = 0; f < outputChan; f++)
				tables[tn].out[f]->data[i] = iv[f];
		}

	}

	/* Free up transforms */
	for (tn = 0; tn < ntables; tn++) {
		if (tables[tn].in_nf != NULL)
			tables[tn].in_nf->del(tables[tn].in_nf);
		if (tables[tn].ind_nf != NULL)
			tables[tn].ind_nf->del(tables[tn].ind_nf);
		if (tables[tn].outd_nf != NULL)
			tables[tn].outd_nf->del(tables[tn].outd_nf);
		if (tables[tn].out_nf != NULL)
			tables[tn].out_nf->del(tables[tn].out_nf);
	}
	icp->al->free(icp->al, tables);

	return ICM_ERR_OK;
}

