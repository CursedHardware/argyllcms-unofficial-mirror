

/* 
 * International Color Consortium Format Library (icclib)
 * For ICC profile version 4.3
 *
 * Author:  Graeme W. Gill
 * Date:    2022/12/15
 * Version: 3.0.0
 *
 * Copyright 1997 - 2023 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 */

/* icmPe transform implementation */
/* This is #included in icc.c */

static icmPe *icc_new_pe_imp(icc *p, icTagTypeSignature ttype,
                             icTagTypeSignature pttype, int rdfile);

/* ---------------------------------------------------------- */

/* Dummy init() */
static int icmPeDummy_init(icmPe *p) {
	return ICM_ERR_OK;
}

/* Compute attr for a PeSeq */
static int icmPeSeq_init(icmPeSeq *p) {
	int i;
	icmPeOp pop = -1;
	int count[icmPeOp_complex+1] = { 0 };

	/* Make sure all member Pe's are inited */
	p->nncount = 0;
	for (i = 0; i < p->count; i++) {
		if (p->pe[i] != NULL) {
			p->pe[i]->init(p->pe[i]);
			p->nncount++;
		}
	}

	/* Find first to set no chan */
	/* (Note that if icmLut1 is not a NOP and the input */
	/*  channels should be other than 3, then the icmPeSeq */
	/* inputChan will be set wrong by the below...) */ 
	for (i = 0; i < p->count; i++) {
		if (p->pe[i] != NULL && p->pe[i]->attr.op != icmPeOp_NOP) {
			p->inputChan = p->pe[i]->inputChan;
			break;
		}
	}
	/* Find last to set no chan */
	for (i = p->count -1; i >= 0; i--) {
		if (p->pe[i] != NULL && p->pe[i]->attr.op != icmPeOp_NOP) {
			p->outputChan = p->pe[i]->outputChan;
			break;
		}
	}

	/* Compute implementation flags and count distict operation types */
	p->attr.op = icmPeOp_NOP;
	p->attr.fwd = 1;
	p->attr.bwd = 1;
	for (i = 0; i < p->count; i++) {
		if (p->pe[i] != NULL && p->pe[i]->attr.op != icmPeOp_NOP) {
			/* If op is not mergable, count it as distinct */
			if (p->pe[i]->attr.op == icmPeOp_cLUT		/* Not mergable operation */
			 || p->pe[i]->attr.op == icmPeOp_fmt
			 || p->pe[i]->attr.op == icmPeOp_complex
			 || p->pe[i]->attr.op != pop)				/* Not the same mergable op. */
				count[p->pe[i]->attr.op]++;
			pop = p->pe[i]->attr.op;

			p->attr.fwd &= p->pe[i]->attr.fwd;
			p->attr.bwd &= p->pe[i]->attr.bwd;
		}
	}

	/* label the dominant top operation type, i.e. */
	/* perch/NOP -> dom-op -> perch/NOP */

	/* If there is nothing other than NOP, label it as NOP */
	if (count[icmPeOp_perch]  == 0
	 && count[icmPeOp_matrix] == 0 && count[icmPeOp_cLUT]    == 0
	 && count[icmPeOp_fmt]    == 0 && count[icmPeOp_complex] == 0) {
		p->attr.op = icmPeOp_NOP;

	/* If there is nothing other than perch, label it as perch */
	} else if (count[icmPeOp_perch]  > 0
	 && count[icmPeOp_matrix] == 0 && count[icmPeOp_cLUT]    == 0
	 && count[icmPeOp_fmt]    == 0 && count[icmPeOp_complex] == 0) {
		p->attr.op = icmPeOp_perch;

	/* If there is one of matrix/clut/fmt + any number of NOP or per channel, */
	/* label it as that op. */
	} else if (count[icmPeOp_matrix] == 1 && count[icmPeOp_cLUT]    == 0
	 && count[icmPeOp_fmt]    == 0 && count[icmPeOp_complex] == 0) {
		p->attr.op = icmPeOp_matrix;

	} else if (count[icmPeOp_matrix] == 0 && count[icmPeOp_cLUT]    == 1
	        && count[icmPeOp_fmt]    == 0 && count[icmPeOp_complex] == 0) {
		p->attr.op = icmPeOp_cLUT;

	} else if (count[icmPeOp_matrix] == 0 && count[icmPeOp_cLUT]    == 0
	        && count[icmPeOp_fmt]    == 1 && count[icmPeOp_complex] == 0) {
		p->attr.op = icmPeOp_fmt;

	/* else label as complex */
	} else {
		p->attr.op = icmPeOp_complex;
	}

	return ICM_ERR_OK;
}

/* ---------------------------------------------------------- */
/* A set of N x 1d Curves */
/* (icSigLut8Type, icSigLut16Type) */

static void (*icmPeCurveSet_serialise)(icmPeCurveSet *p, icmFBuf *b) = NULL;


/* Serialise this tag type for a Lut8 or Lut 16 */
static void icmPeCurveSet_LUT816_serialise(icmPeCurveSet *p, icmFBuf *b) {
	icmPeCurve **pe = (icmPeCurve **)p->pe;
	unsigned int n;

	for (n = 0; n < p->inputChan; n++) {
		icmSn_PeSubTag(b, NULL, NULL, &p->pe[n], p->ttype, p->rdfile, p->dp);  
	}
	// Can't ICMRDCHECKCONSUMED(icmPeCurveSet) because there is no directory above us
}

static void (*icmPeCurveSet_dump)(icmPeCurveSet *p, icmFile *op, int verb) = NULL;

/* Dump a human readable description */
static void icmPeCurveSet_LUT816_dump(icmPeCurveSet *p, icmFile *op, int verb) {
	int pad = p->dp;

	if (verb <= 0)
		return;

	if (verb >= 1) {
		icmPeCurve **pe = (icmPeCurve **)p->pe;
		unsigned int i, j;

		for (i = 0; i < pe[0]->count; i++) {
			op->printf(op,PAD("  %3u: "),i);
			for (j = 0; j < p->inputChan; j++)
				op->printf(op," %1.10f",pe[j]->data[i]);
			op->printf(op,"\n");
		}
	}
}

static int icmPeCurveSet_check(icmPeCurveSet *p, icTagSignature sig, int rd) {
	icc *icp = p->icp;
	unsigned int n;

	/* Check no. of in/out channels */
	if (p->inputChan != p->outputChan)
		icmFormatWarning(icp, ICM_FMTF_CSET_CHAN, "icmPeCurveSet input/output channels %u %u mismatch",p->inputChan , p->outputChan);	

	/* If Lut8 or Lut16, check that all the sub-tags are icmSig816Curve, that they */
	/* are all icmCurveSpec and that they all have the same table size */
	if (p->ttype == icmSig816Curves) {
		for (n = 0; n < p->inputChan; n++) {
			if (p->pe[n] == NULL)
				continue;			/* Hmm. */
			if (p->pe[n]->ttype != icmSig816Curve)
				icmFormatWarning(icp, ICM_FMT_CSET_SUBT, "icmPeCurveSet sub-tag %u is not icmSig816Curve",n);	
			else {
				icmPeCurve **pe = (icmPeCurve **)p->pe;
				if (pe[n]->ctype != icmCurveSpec)
					icmFormatWarning(icp, ICM_FMT_CURV_CTYPE, "icmPeCurveSet sub-tag %u is not CurveSpec",n);	
				if (pe[n]->count != pe[0]->count)
					icmFormatWarning(icp, ICM_FMT_CURV_POIMATCH, "icmPeCurveSet sub-tag %u count %u doesn't match (should be %u)",n,pe[n]->count, pe[0]->count);	
			}
		}
	}
	/* Check all sub-icmPe's */
	for (n = 0; n < p->inputChan; n++) {
		if (p->pe[n] == NULL)
			continue;			/* Hmm. */
		p->pe[n]->check(p->pe[n], sig, rd); 
		if (p->icp->e.c != ICM_ERR_OK)
			return p->icp->e.c;
	}

	return p->icp->e.c;
}

/* Compare another with this */
static int icmPeCurveSet_cmp(icmPeCurveSet *dst, icmPeCurveSet *src) {
	unsigned int i;

	if (dst->ttype != src->ttype)
		return 1;

	if (dst->inputChan != src->inputChan
	 || dst->outputChan != src->outputChan)
		return 1;

	for (i = 0; i < dst->inputChan; i++) {
		if ((dst->pe[i] == NULL && src->pe[i] != NULL)
		 || (dst->pe[i] != NULL && src->pe[i] == NULL))
			return 1;
		if (dst->pe[i] != NULL && src->pe[i] != NULL) {
			if (dst->pe[i]->cmp == NULL) {
				icm_err(dst->icp,ICM_ERR_UNIMP_TTYPE_CMP,"icmPeCurveSet_cmp: unimplemented for %s",
		                                               icmTypeSig2str(dst->pe[i]->ttype));
				return 1;
			}
		}
		if (dst->pe[i]->cmp(dst->pe[i], src->pe[i]))
			return 1;
	}

	return 0;
}

/* Copy or translate from source to this ttype */
static int icmPeCurveSet_cpy(icmPeCurveSet *dst, icmBase *isrc) {
	icc *p = dst->icp;

	if (dst->etype == icmSigPeCurveSet
	 && isrc->etype == icmSigPeCurveSet) {
		icmPeCurveSet *src = (icmPeCurveSet *)isrc;
		unsigned int i;

		/* Free any existing sub-objects */
		for (i = 0; i < dst->inputChan; i++) {
			if (dst->pe[i] != NULL)
				dst->pe[i]->del(dst->pe[i]);
		}

		dst->inputChan = src->inputChan;
	 	dst->outputChan = src->outputChan;

		for (i = 0; i < dst->inputChan; i++) {
			if ((dst->pe[i] = icc_new_pe_imp(p, src->pe[i]->ttype, dst->ttype, dst->rdfile)) == NULL)
				return p->e.c;  
		 	dst->pe[i]->cpy(dst->pe[i], (icmBase *)src->pe[i]);
		}
		return ICM_ERR_OK;
	}

	return icm_err(dst->icp, ICM_ERR_UNIMP_TTYPE_COPY,"icmPeCurveSet_cpy: unimplemented tagtype");
}

/* Setup attr */
static int icmPeCurveSet_init(icmPeCurveSet *p) {
	unsigned int i;

	p->attr.op = icmPeOp_NOP;
	p->attr.fwd = 1;
	p->attr.bwd = 1;
	for (i = 0; i < p->inputChan; i++) {
		if (p->pe[i] != NULL) {
			p->pe[i]->init(p->pe[i]);

			/* This is a NOP if all channels are NOPs */
			if (p->pe[i]->attr.op != icmPeOp_NOP)
				p->attr.op = icmPeOp_perch;
			p->attr.fwd &= p->pe[i]->attr.fwd;
			p->attr.bwd &= p->pe[i]->attr.bwd;
		}
	}
	return ICM_ERR_OK;
}

/* Polymorphic constructor. */
/* Create an empty object. Return null on error */
/* Set ttype to the serialisation type of this Pe or its */
/* parent ttype if it doesn't use a sig in its serialisation. */
/* Use ttype = icmSigUnknownType if not being serialised. */
static icmBase *new_icmPeCurveSet(icc *icp, icTagTypeSignature ttype) {
	ICM_PE_ALLOCINIT(icmPeCurveSet, icmSigPeCurveSet, ttype)

	if (p->ttype == icmSig816Curves) {	/* icmLut8 and icmLut16 curves */
		ICM_PE_SETATTR(p->attr, 1, 0, 0, icmPeOp_perch, 1, 1)
		p->serialise = icmPeCurveSet_LUT816_serialise;
		p->dump = icmPeCurveSet_LUT816_dump;

	} else {
		icm_err(p->icp, ICM_ERR_PE_UNKNOWN_TTYPE, "new_icmPeCurveSet: Unknown ttype %s",icmtag2str(p->ttype));
		p->icp->al->free(p->icp->al, p);
		return NULL;
	}
	p->init = icmPeCurveSet_init;
	p->lookup_fwd = icmPeCurveSet_lookup_fwd;
	p->lookup_bwd = icmPeCurveSet_lookup_bwd;
	p->cmp = icmPeCurveSet_cmp;
	p->cpy = icmPeCurveSet_cpy;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* A linear/gamma/table/sampled curve */
/* (TRCTag, icSigLut8Type, icSigLut16Type) */

static void (*icmPeCurve_serialise)(icmPeCurve *p, icmFBuf *b) = NULL;


/* Serialise this tag type (embedded within TTYPE) */
static void icmPeCurve_TTYPE_serialise(icmPeCurve *p, icmFBuf *b) {
	unsigned int i;

	if (b->op == icmSnResize)
		p->inited = 0;

	/* Decode flag set by user into count */
	if (b->op == icmSnResize) {
		switch (p->ctype) {
			case icmCurveLin:
				p->count = 0;
				break;
			case icmCurveGamma:
				p->count = 1;
				break;
			case icmCurveSpec:
				/* User sets count for icmCurveSpec */
				break;
			default:
				icm_err(p->icp, ICM_ERR_BADCURVE, "Unknown curve flag %d",p->ctype);
				break;
		}
	}

    icmSn_TagTypeSig32(b, &p->ttype);	/* 0-3:  Curve Tag Type signature */
	icmSn_pad(b, 4);					/* 4-7:  Zero padding */
	icmSn_ui_UInt32(b, &p->count);		/* 8-11: Number of parameters/table values */

	/* decode count read into flag */
	if (b->op == icmSnRead) {
		if (p->count == 0)
			p->ctype = icmCurveLin;
		else if (p->count == 1)
			p->ctype = icmCurveGamma;
		else 
			p->ctype = icmCurveSpec;
	}
		
	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_count, &p->count,
	    (void **)&p->data, sizeof(double), UINT_MAX, 0, 2, "icmCurve"))
		return;

	if (b->op & icmSnSerialise) {				/* (Optimise for speed) */
		if (p->count == 1) {
			icmSn_d_U8Fix8(b, &p->data[0]);		/* 12-13: Gamma value */
		} else {
			for (i = 0; i < p->count; i++)
				icmSn_d_NFix16(b, &p->data[i]);	/* 12 + 2 * n: Curve value */
		}
	}

	ICMSNFREEARRAY(b, p->_count, p->data)
	if (!p->emb) {	/* Can check if first class tag, else not because there is no dir. above us */
		ICMRDCHECKCONSUMED(icmPeCurve)
	}

	if (b->op == icmSnRead)
		icmPeCurve_init(p);
	else if (b->op == icmSnFree)
		icmPeCurve_deinit(p);
}

/* Serialise this tag type for a Lut8 or Luut 16 */
static void icmPeCurve_LUT816_serialise(icmPeCurve *p, icmFBuf *b) {

	if (b->op == icmSnResize)
		p->inited = 0;

	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_count, &p->count,
	    (void **)&p->data, sizeof(double),
	    UINT_MAX, 0, p->bpv, "icmLut8/16"))
		return;

	if (b->op & icmSnSerialise) {
		unsigned int i;

		if (p->bpv == 1) {
			for (i = 0; i < p->count; i++)
				icmSn_d_NFix8(b, &p->data[i]);
		} else {
			for (i = 0; i < p->count; i++)
				icmSn_d_NFix16(b, &p->data[i]);
		}
	}

	ICMSNFREEARRAY(b, p->_count, p->data)
	// ICMRDCHECKCONSUMED(icmPeCurve)		can't because there's no directory above us

	if (b->op == icmSnRead)
		icmPeCurve_init(p);
	else if (b->op == icmSnFree)
		icmPeCurve_deinit(p);
}

/* Dump a human readable description */
static void icmPeCurve_dump(icmPeCurve *p, icmFile *op, int verb) {
	unsigned int n;
	int pad = p->dp;

		op->printf(op,PAD("Curve:\n"));

	if (p->ctype == icmCurveLin) {
		op->printf(op,PAD("  Curve is linear\n"));
	} else if (p->ctype == icmCurveGamma) {
		op->printf(op,PAD("  Curve is gamma of %1.10f\n"),p->data[0]);
	} else if (p->ctype == icmCurveSpec) {
		{
			op->printf(op,PAD("  No. elements = %u\n"),p->count);
			if (verb >= 2) {
				for (n = 0; n < p->count; n++)
					op->printf(op,PAD("  %3lu:  %1.10f\n"),n,p->data[n]);
			}
		}
	} else {
		op->printf(op,PAD("  Curve has unknown ctype %u\n"),p->ctype);
	}
}

static int icmPeCurve_check(icmPeCurve *p, icTagSignature sig, int rd) {
	icc *icp = p->icp;
	unsigned int n;

	/* Check no. of in/out channels */
	if (p->inputChan != 1
	 || p->outputChan != 1)
		icmFormatWarning(icp, ICM_FMT_CURV_CHAN, "icmPeCurve input/output channels not = 1 (are %u, %u)",p->inputChan , p->outputChan);	

	if (p->ttype == icSigCurveType) {
		if (p->ctype == icmCurveSpec && p->count < 2) {
			icmFormatWarning(p->icp, ICM_FMT_CURV_POINTS,
			                       "icmCurve count %u < 2",p->count);
		}
	}

	return p->icp->e.c;
}

/* Compare another with this */
static int icmPeCurve_cmp(icmPeCurve *dst, icmPeCurve *src) {
	unsigned int np, i;

	if (dst->ttype != src->ttype)
		return 1;

	if (dst->inputChan != src->inputChan
	 || dst->outputChan != src->outputChan)
		return 1;

	if (dst->ctype != src->ctype)
		return 1;

	if (dst->count != src->count)
		return 1;

	for (i = 0; i < dst->count; i++) {
		if (dst->data[i] != src->data[i])
			return 1;
	}

	return 0;
}

/* Copy or translate from source to this ttype */
static int icmPeCurve_cpy(icmPeCurve *dst, icmBase *isrc) {
	icc *p = dst->icp;

	if (dst->etype == icmSigPeCurve
	 && isrc->etype == icmSigPeCurve) {
		icmPeCurve *src = (icmPeCurve *)isrc;
		unsigned int np, i;

		dst->inputChan = src->inputChan;
	 	dst->outputChan = src->outputChan;
		dst->ctype = src->ctype;

		dst->count = src->count;
		dst->allocate(dst);

		for (i = 0; i < src->count; i++) {
			dst->data[i] = src->data[i];
		}
		return ICM_ERR_OK;
	}

	return icm_err(dst->icp, ICM_ERR_UNIMP_TTYPE_COPY,"icmPeCurve_cpy: unimplemented tagtype");
}

/* Polymorphic constructor. */
/* Create an empty PeCurve object. Return null on error */
static icmBase *new_icmPeCurve(icc *icp, icTagTypeSignature ttype) {
	ICM_PE_ALLOCINIT(icmPeCurve, icmSigPeCurve, ttype)

	p->inputChan = p->outputChan = 1;

	if (p->ttype == icSigCurveType) {
		ICM_PE_SETATTR(p->attr, 0, 0, 0, icmPeOp_perch, 1, 1)
		p->ctype = icmCurveUndef;
		p->serialise = icmPeCurve_TTYPE_serialise;
	} else if (p->ttype == icmSig816Curve) {
		ICM_PE_SETATTR(p->attr, 0, 0, 0, icmPeOp_perch, 1, 1)
		p->ctype = icmCurveSpec;
		p->serialise = icmPeCurve_LUT816_serialise;
	} else {
		icm_err(p->icp, ICM_ERR_PE_UNKNOWN_TTYPE, "new_icmPeCurve: Unknown ttype %s",icmtag2str(p->ttype));
		p->icp->al->free(p->icp->al, p);
		return NULL;
	}

	p->init = icmPeCurve_init;
	p->cmp = icmPeCurve_cmp;
	p->cpy = icmPeCurve_cpy;
	p->lookup_fwd = icmPeCurve_lookup_fwd;
	p->lookup_bwd = icmPeCurve_lookup_bwd;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* An N x M + F matrix */
/* (icSigLut8Type, icSigLut16Type) */

static void (*icmPeMatrix_serialise)(icmPeMatrix *p, icmFBuf *b) = NULL;


/* Serialise this from/to a Lut8 or Lut16 Matrix */
void icmPeMatrix_LUT816_serialise(icmPeMatrix *p, icmFBuf *b) {
	unsigned int m, n;

	if (b->op == icmSnResize)
		p->inited = 0;

	if (b->op & icmSnSerialise) {
		for (m = 0; m < 3; m++) {			/* Rows */
			for (n = 0; n < 3; n++)			/* Columns */
				icmSn_d_S15Fix16(b, &p->mx[m][n]);
		}
	}

	/* Create inverse and flags */
	if (b->op == icmSnRead) {
		for (m = 0; m < p->outputChan; m++)
			p->ct[m] = 0.0;

		icmPeMatrix_init(p);
	}
	// Can't ICMRDCHECKCONSUMED(icmPeMatrix) because there is no directory above us
}

/* Dump a human readable description */
static void icmPeMatrix_dump(icmPeMatrix *p, icmFile *op, int verb) {
	int pad = p->dp;
	unsigned int m, n;

	if (verb <= 0)
		return;


	if (verb >= 1) {
		{
			for (m = 0; m < 3; m++) {
				op->printf(op,PAD("  "));
				for (n = 0; n < 3; n++)
					op->printf(op,"%1.10f%s",p->mx[m][n], n < (p->inputChan-1) ? ", " : "");
				op->printf(op,"\n");
			}
		}
	}
}

static int icmPeMatrix_check(icmPeMatrix *p, icTagSignature sig, int rd) {
	icc *icp = p->icp;
	unsigned int n;

	/* Check inChan == outChan == 3 and ct[] == 0.0 */
	if (p->ttype == icmSig816Matrix) {
		unsigned int n;

		if (p->inputChan != 3 || p->outputChan != 3)
			icmFormatWarning(icp, ICM_FMT_MATX_CHAN, "icmSig816Matrix input/output channels not = 3 (are %u, %u)",p->inputChan , p->outputChan);	
		for (n = 0; n < p->outputChan; n++) {
			if (p->ct[n] != 0.0)
				icmFormatWarning(icp, ICM_FMT_MATX_CNST, "icmSig816Matrix constant %u is not 0.0 (is %f)",n , p->ct[n]);	
		}
	}
	return p->icp->e.c;
}

/* Compare another with this */
static int icmPeMatrix_cmp(icmPeMatrix *dst, icmPeMatrix *src) {
	unsigned int m, n;

	if (dst->ttype != src->ttype)
		return 1;

	if (dst->inputChan != src->inputChan
	 || dst->outputChan != src->outputChan)
		return 1;

	for (m = 0; m < dst->outputChan; m++) {
		for (n = 0; n < dst->inputChan; n++) {
			if (dst->mx[m][n] != src->mx[m][n])
				return 1;
		}
	}

	for (m = 0; m < dst->outputChan; m++) {
		if (dst->ct[m] != src->ct[m])
			return 1;
	}

	return 0;
}

/* Copy or translate from source to this ttype */
static int icmPeMatrix_cpy(icmPeMatrix *dst, icmBase *isrc) {
	icc *p = dst->icp;

	if (dst->etype == icmSigPeMatrix
	 && isrc->etype == icmSigPeMatrix) {
		icmPeMatrix *src = (icmPeMatrix *)isrc;
		unsigned int m, n;

		dst->inputChan = src->inputChan;
	 	dst->outputChan = src->outputChan;

		for (m = 0; m < src->outputChan; m++) {
			for (n = 0; n < src->inputChan; n++)
				dst->mx[m][n] = src->mx[m][n];
		}

		for (m = 0; m < src->outputChan; m++)
			dst->ct[m] = src->ct[m];

		return ICM_ERR_OK;
	}

	return icm_err(dst->icp, ICM_ERR_UNIMP_TTYPE_COPY,"icmPeMatrix_cpy: unimplemented tagtype");
}


/* Polymorphic constructor. */
/* Create an empty object. Return null on error */
static icmBase *new_icmPeMatrix(icc *icp, icTagTypeSignature ttype) {
	unsigned int m;
	ICM_PE_ALLOCINIT(icmPeMatrix, icmSigPeMatrix, ttype)


	/* Assume NOP until icmPeMatrix_init is called */
	if (p->ttype == icmSig816Matrix) {
		ICM_PE_SETATTR(p->attr, 0, 0, 0, icmPeOp_NOP, 1, 1)
		p->inputChan = p->outputChan = 3;
		p->serialise = icmPeMatrix_LUT816_serialise;

	} else {
		icm_err(p->icp, ICM_ERR_PE_UNKNOWN_TTYPE, "new_icmPeMatrix: Unknown ttype %s",icmtag2str(p->ttype));
		p->icp->al->free(p->icp->al, p);
		return NULL;
	}

	/* Default to a NOP matrix */
	for (m = 0; m < (p->outputChan < p->inputChan ? p->outputChan: p->inputChan); m++)
		p->mx[m][m] = 1.0;
	for (m = 0; m < p->outputChan; m++)
		p->ct[m] = 0.0;

	p->init = icmPeMatrix_init;
	p->cmp = icmPeMatrix_cmp;
	p->cpy = icmPeMatrix_cpy;
	p->lookup_fwd = icmPeMatrix_lookup_fwd;
	p->lookup_bwd = icmPeMatrix_lookup_bwd;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* An N x M cLUT */
/* (icSigLut8Type, icSigLut16Type) */

static void (*icmPeClut_serialise)(icmPeClut *p, icmFBuf *b) = NULL;


/* Serialise this from/to a Lut8 or Lut16 */
void icmPeClut_LUT816_serialise(icmPeClut *p, icmFBuf *b) {
	int ind;
	unsigned int i;
	unsigned int clutsize;

	if (b->op == icmSnResize)
		p->inited = 0;

	/* Clut table */
	/* Grid resolution table */
	ind = 0;
	clutsize = 1;
	for (i = 0; i < p->inputChan; i++)
		clutsize = sati_mul(&ind, p->clutPoints[i], clutsize);
	clutsize = sati_mul(&ind, p->outputChan, clutsize);

	if (ind != 0) {
		icmFmtWarn(b, ICM_FMTF_CLUT_LUTSIZE, "icmPeClut table size overflow");	
		return;
	}

	if (icmArrayRdAllocResize(b, icmAResizeByCount, &p->_clutsize, &clutsize,
	    (void **)&p->clutTable, sizeof(double),
	    UINT_MAX, 0, p->bpv, "icmLut8/16"))
		return;
	if (b->op & icmSnSerialise) {
		if (p->bpv == 1) {
			for (i = 0; i < clutsize; i++)
				icmSn_d_NFix8(b, &p->clutTable[i]);
		} else {
			for (i = 0; i < clutsize; i++)
				icmSn_d_NFix16(b, &p->clutTable[i]);
		}
	}
	ICMSNFREEARRAY(p, p->_clutsize, p->clutTable)
	// Can't ICMRDCHECKCONSUMED(icmPeClut) because there is no directory above us

	if (b->op & icmSnAlloc) {		/* After alloc or read */
		icmPeClut_init(p);
	}
}

/* Dump a human readable description */
static void icmPeClut_dump(icmPeClut *p, icmFile *op, int verb) {
	int pad = p->dp;
	unsigned int i, j;

	if (verb <= 0)
		return;


	if (verb >= 2) {
		unsigned int ii[MAX_CHAN];	/* Input channel index */

		for (j = 0; j < p->inputChan; j++)
			ii[j] = 0;
		for (i = 0; i < p->_clutsize;) {
			unsigned int k;
			/* Print table entry index */
			op->printf(op,PAD(" "));
			for (j = p->inputChan-1; j < p->inputChan; j--)
				op->printf(op," %2u",ii[j]);
			op->printf(op,":");
			/* Print table entry contents */
			for (k = 0; k < p->outputChan; k++, i++)
				op->printf(op," %1.10f",p->clutTable[i]);
			op->printf(op,"\n");
		
			/* Increment index */
			for (j = 0; j < p->inputChan; j++) {
				ii[j]++;
				if (ii[j] < p->clutPoints[j])
					break;	/* No carry */
				ii[j] = 0;
			}
		}
	}
}

static int icmPeClut_check(icmPeClut *p, icTagSignature sig, int rd) {
	icc *icp = p->icp;
	unsigned int n;

	/* Check all clutPoints[] are >= 2 */
	for (n = 0; n < p->inputChan; n++) {
		if (p->clutPoints[n] < 2)
			icmFormatWarning(icp, ICM_FMT_CLUT_RES, "icmPeClut constant %u resolution < 2 (is %u)",n , p->clutPoints[n]);	
	}
	return p->icp->e.c;
}

/* Compare another with this */
static int icmPeClut_cmp(icmPeClut *dst, icmPeClut *src) {
	unsigned int np, i;

	if (dst->ttype != src->ttype)
		return 1;

	if (dst->inputChan != src->inputChan
	 || dst->outputChan != src->outputChan)
		return 1;

	for (i = 0; i < dst->inputChan; i++) {
		if (dst->clutPoints[i] != src->clutPoints[i])
			return 1;
	}

	if (dst->_clutsize != src->_clutsize)
		return 1;

	for (i = 0; i < dst->_clutsize; i++) {
		if (dst->clutTable[i] != src->clutTable[i])
			return 1;
	}

	return 0;
}

/* Copy or translate from source to this ttype */
static int icmPeClut_cpy(icmPeClut *dst, icmBase *isrc) {
	icc *p = dst->icp;

	if (dst->etype == icmSigPeClut
	 && isrc->etype == icmSigPeClut) {
		icmPeClut *src = (icmPeClut *)isrc;
		unsigned int i;

		dst->inputChan = src->inputChan;
	 	dst->outputChan = src->outputChan;

		for (i = 0; i < src->inputChan; i++)
			dst->clutPoints[i] = src->clutPoints[i];
		dst->allocate(dst);

		for (i = 0; i < dst->_clutsize; i++)
			dst->clutTable[i] = src->clutTable[i];

		return ICM_ERR_OK;
	}

	return icm_err(dst->icp, ICM_ERR_UNIMP_TTYPE_COPY,"icmPeClut_cpy: unimplemented tagtype");
}

/* Polymorphic constructor. */
/* Create an empty object. Return null on error */
static icmBase *new_icmPeClut(icc *icp, icTagTypeSignature ttype) {
	ICM_PE_ALLOCINIT(icmPeClut, icmSigPeClut, ttype)


	if (p->ttype == icmSig816CLUT) {
		ICM_PE_SETATTR(p->attr, 0, 0, 0, icmPeOp_cLUT, 1, 0)
		p->serialise = icmPeClut_LUT816_serialise;

	} else {
		icm_err(p->icp, ICM_ERR_PE_UNKNOWN_TTYPE, "new_icmPeClut: Unknown ttype %s",icmtag2str(p->ttype));
		p->icp->al->free(p->icp->al, p);
		return NULL;
	}

	p->use_sx = 1;			/* Default */

	p->init = icmPeClut_init;
	p->cmp = icmPeClut_cmp;
	p->cpy = icmPeClut_cpy;
	p->lookup_fwd = icmPeClut_lookup_fwd;
	p->lookup_bwd = icmPeClut_lookup_bwd;

	p->min_max    = icmPeClut_min_max;
	p->choose_alg = icmPeClut_choose_alg;
	p->get_tac = icmPeClut_get_tac;

	return (icmBase *)p;
}

/* ============================================================ */

/* Table of icmPe Types and constructor, plus */
/* valid ttypes that they can be serialised as. */
static struct {
	icmPeSignature  pesig;			/* The icmPe signature */
	struct _icmBase *(*new_obj)(struct _icc *icp, icTagTypeSignature ttype);
	icTagTypeSignature ttypes[6];	/* Known ttypes */
} icmPeConstrTable[] = {
	{ icmSigPeCurve,			new_icmPeCurve,
								{icSigCurveType,
	                             icMaxEnumTagType} },

	{ icmSigPeMatrix,			new_icmPeMatrix,
								{
	                             icSigLut8Type, icSigLut16Type, icMaxEnumTagType} },

	{ icmSigPeClut,				new_icmPeClut,
								{
	                             icSigLut8Type, icSigLut16Type, icMaxEnumTagType} },

	{ icmPeMaxEnum }
};

/* Create an empty icmPe object. Return null on error */
/* Set ttype to the serialisation type of this Pe or its */
/* parent ttype if it doesn't use a sig in its serialisation. */
/* Use ttype = icmSigUnknownType if not being serialised. */
/* (Used just to construct transforms ?) */
static icmPe *new_icmPe(
	icc *p,
	icTagTypeSignature ttype,		/* Determines serialisation & dump type. */
	icmPeSignature pesig			/* Type of icmPe */
) {
	icmPe *nob;
	unsigned int j, k;

	/* Find the table entry for this pesig */
	for (j = 0; icmPeConstrTable[j].pesig != pesig
	         && icmPeConstrTable[j].pesig != icmPeMaxEnum; j++)
		;
	
	if (icmPeConstrTable[j].pesig == icmPeMaxEnum) {	/* Unknown pesig */
		icm_err(p, ICM_ERR_PE_NOT_KNOWN, "new_icmPe: sig '%s' is not known", icmtag2str(pesig));
		return NULL;
	}
	
	/* Check that the ttype is known */
	if (ttype != icmSigUnknownType) {
		for (k = 0; icmPeConstrTable[j].ttypes[k] != icMaxEnumTagType; k++) {
			if (icmPeConstrTable[j].ttypes[k] == ttype)
				break;
		}
		if (icmPeConstrTable[j].ttypes[k] == icMaxEnumTagType) {
			icmFormatWarning(p, ICM_ERR_PE_UNKNOWN_TTYPE,
			          "new_icmPe: icmPe %s has unknown ttype %s\n",
			               icmPeSig2str(pesig), icmTypeSig2str(ttype));
		}
	}

	/* Allocate the empty object */
	if ((nob = (icmPe *)icmPeConstrTable[j].new_obj(p, ttype)) == NULL)
		return NULL;

	return nob;
}

/* ------------------------------------------------------------ */
/* Lut8Type, Lut16type */

/* Serialise this tag type */
static void icmLut1_serialise(icmLut1 *p, icmFBuf *b) {
	static icTagTypeSignature pettypes[4] = {
		icmSig816Matrix, icmSig816Curves, icmSig816CLUT, icmSig816Curves
	};
	icc *icp = b->icp;
	unsigned int n;

	if (b->op & icmSnSerialise) {
		unsigned int clutpoints;

		/* Special case */
		if (b->op == icmSnWrite) {
			if (icp->allowclutPoints256 && p->clutPoints == 256)
				clutpoints = 0;
			else
				clutpoints = p->clutPoints;
		}

		/* Setup tag header */
	    icmSn_TagTypeSig32(b, &p->ttype);		/* 0-3:  Lut Tag Type signature */
		icmSn_pad(b, 4);						/* 4-7:  Zero padding */

		icmSn_check_ui_UInt8(b, &p->inputChan, MAX_CHAN);	/* 8:    Number of input channels */
		icmSn_check_ui_UInt8(b, &p->outputChan, MAX_CHAN);	/* 9:    Number of output channels */
		icmSn_ui_UInt8(b, &clutpoints);			/* 10:   Clut resolution */
		icmSn_pad(b, 1);						/* 11:   Padding, must be 0 */

		if (b->op == icmSnRead) {
			p->bpv = (p->ttype == icSigLut8Type) ? 1 : 2;	/* Recompute after read*/

			/* Special case */
			if (icp->allowclutPoints256 && clutpoints == 0)
				p->clutPoints = 256;
			else
				p->clutPoints = clutpoints;
		}
	}

	/* Serialise the Elements */
	for (n = 0; n < 4; n++) {

		if (icp->e.c != ICM_ERR_OK) {
			return;
		}

		/* Before input table */
		if (n == icmLut816_ix_in) {
			if (p->bpv == 2) {
				icmSn_ui_UInt16(b, &p->inputEnt);		/* 48-49:	Input table entries */
				icmSn_ui_UInt16(b, &p->outputEnt);		/* 50-51:	Output table entries */
			}
		}

		if (b->op & icmSnAlloc) {
			icTagTypeSignature tsig;

			if (p->pe[n] == NULL
			 && (p->pe[n] = icc_new_pe_imp(icp, pettypes[n], p->ttype, p->rdfile)) == NULL) {
				return;
			}

			/* Set element in/out channels - changeover from in to out at icmLut816_ix_CLUT */
			/* Leave matrix as hard coded 3x3 */
			if (pettypes[n] != icmSig816Matrix) {
				p->pe[n]->inputChan = n <= 2 ? p->inputChan : p->outputChan; 
				p->pe[n]->outputChan = n <= 1 ? p->inputChan : p->outputChan; 
			}

			/* We can't use sub-elements to allocation, we have to */
			/* pre-allocate here... */
			tsig = pettypes[n];

			/* If we can, add curves to CurveSet */
			if (tsig == icmSig816Curves && p->pe[n]->inputChan > 0) {
				icmPeCurveSet *pe = (icmPeCurveSet *)p->pe[n];
				unsigned int m;

				pe->bpv = p->bpv;
				for (m = 0; m < pe->inputChan; m++) {
					icmPeCurve *cpe;
					if (pe->pe[m] == NULL
					 && (pe->pe[m] = icc_new_pe_imp(icp, icmSig816Curve, pe->ttype, p->rdfile))
						                                                           == NULL) {
						icm_err(icp, ICM_ERR_NEW_PE_FAILED,"icmLut1_serialise: icc_new_pe_imp()"
						                       " for %s failed",icmtag2str(icSigCurveType));
						return;
					}
					cpe = (icmPeCurve *)pe->pe[m];
					cpe->bpv = p->bpv;
					cpe->ctype = icmCurveSpec;
					cpe->count = (n == icmLut816_ix_in) ? p->inputEnt : p->outputEnt;
					cpe->allocate(cpe);
				}

			/* We don't have to allocate matrix contents.. */
			} else if (pettypes[n] == icmSig816Matrix && p->pe[n]->inputChan > 0) {

			/* Allocate cLUT array */
			} else if (pettypes[n] == icmSig816CLUT
			        && p->pe[n]->inputChan > 0 && p->pe[n]->outputChan > 0) {
				icmPeClut *pe = (icmPeClut *)p->pe[n];
				unsigned int m;

				pe->bpv = p->bpv;
				for (m = 0; m < pe->inputChan; m++) {
					pe->clutPoints[m] = p->clutPoints;
					if (pe->clutPoints[m] == 0)
						break;
				}
				if (m >= pe->inputChan) {
					pe->allocate(pe);
				}
			}

			/* Setup typed aliases to Processing Elements */
			p->pe_mx = (icmPeMatrix *)p->pe[icmLut816_ix_Matrix];
			if (p->pe[icmLut816_ix_in] != NULL)
				p->pe_ic = (icmPeCurve **)((icmPeCurveSet *)p->pe[icmLut816_ix_in])->pe;
			else
				p->pe_ic = NULL;
			p->pe_cl = (icmPeClut *)p->pe[icmLut816_ix_CLUT];
			if (p->pe[icmLut816_ix_out] != NULL)
				p->pe_oc = (icmPeCurve **)((icmPeCurveSet *)p->pe[icmLut816_ix_out])->pe;
			else
				p->pe_oc = NULL;
		}

		/* Serialize a sub-tag */
		icmSn_PeSubTag(b, NULL, NULL, &p->pe[n], p->ttype, p->rdfile, p->dp);  
	}
	ICMSNFREEARRAY(b, p->_count, p->pe)
	ICMRDCHECKCONSUMED(icmLut1)
}

/* Dump a text description of the object */
static void icmLut1_dump(
	icmLut1 *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	int pad = p->dp;

	if (verb <= 0)
		return;

	if (p->ttype == icSigLut8Type) {
		op->printf(op,"Lut8:\n");
	} else {
		op->printf(op,"Lut16:\n");
	}
	op->printf(op,"  Input Channels = %u\n",p->inputChan);
	op->printf(op,"  Output Channels = %u\n",p->outputChan);
	op->printf(op,"  CLUT resolution = %u\n",p->clutPoints);
	op->printf(op,"  Input Table entries = %u\n",p->inputEnt);
	op->printf(op,"  Output Table entries = %u\n",p->outputEnt);

	if (verb >= 2) {
		op->printf(op,"  XYZ matrix:\n");
		if (p->pe[icmLut816_ix_Matrix] != NULL)
			p->pe[icmLut816_ix_Matrix]->dump(p->pe[icmLut816_ix_Matrix], op, verb);
		op->printf(op,"  Input table:\n");
		if (p->pe[icmLut816_ix_in] != NULL)
			p->pe[icmLut816_ix_in]->dump(p->pe[icmLut816_ix_in], op, verb);
		op->printf(op,"  CLUT table:\n");
		if (p->pe[icmLut816_ix_CLUT] != NULL)
			p->pe[icmLut816_ix_CLUT]->dump(p->pe[icmLut816_ix_CLUT], op, verb);
		op->printf(op,"  Output table:\n");
		if (p->pe[icmLut816_ix_out] != NULL)
			p->pe[icmLut816_ix_out]->dump(p->pe[icmLut816_ix_out], op, verb);
	}
}

/* Check Lut1 */
static int icmLut1_check(icmLut1 *p, icTagSignature sig, int rd) {
	icc *icp = p->icp;
	icColorSpaceSignature inputSpace, outputSpace;
	unsigned int ichan, ochan;
	unsigned int n;
	int vpurp = 1;

	switch (icp->get_tag_lut_purpose(icp, p->creatorsig)) {
		case icmTPLutFwd:        /* AtoBn:    Device to PCS */
			inputSpace  = icp->header->colorSpace;
			outputSpace = icp->header->pcs;
			break;
		case icmTPLutBwd:			/* BtoAn:    PCS to Device */
			inputSpace  = icp->header->pcs;
			outputSpace = icp->header->colorSpace;
			break;
		case icmTPLutGamut:		/* Gamut:    PCS to Gray */
			inputSpace  = icp->header->pcs;
			outputSpace = icSigGrayData;
			break;
		case icmTPLutPreview:		/* Preview: PCS to PCS */
			inputSpace  = icp->header->pcs;
			outputSpace = icp->header->pcs;
			break;
		default:
			vpurp = 0;
			icmFormatWarning(icp, ICM_FMT_LUPURP,
			                 "icmLut1 Unknown LUT purpose");	
	}

	if (vpurp) {
		/* Does number of channels match header device space encoding ? */
		ichan = icmCSSig2nchan(inputSpace);
		if (p->inputChan != ichan)  {
			icmFormatWarning(icp, ICM_FMT_LUICHAN,
			                 "icmLut1 no. input channels %d doesn't match colorspace %d",
			                  p->inputChan,ichan);	
		}
		ochan = icmCSSig2nchan(outputSpace);
		if (p->outputChan != ochan) {
			icmFormatWarning(icp, ICM_FMT_LUOCHAN,
			                 "icmLut1 no. output channels %d doesn't match colorspace %d",
			                 p->outputChan,ochan);	
		}
	}

	if (p->ttype == icSigLut8Type) {
		if (p->inputEnt != 256 || p->outputEnt != 256) {
			icmFormatWarning(icp, ICM_FMT_LU8IOENT,
		                 "icmLut8 1D input or output tables don't have 256 entries");
		}
	} else if (p->inputEnt > 4096 || p->outputEnt > 4096) {
		icmFormatWarning(icp, ICM_FMT_LUIOENT,
		                 "icmLut8 1D input or output have no. entries > 4096");
	}

	/* Check all sub-icmPe's */
	for (n = 0; n < 4; n++) {
		if (p->pe[n] == NULL)
			continue;			/* Hmm. */
		p->pe[n]->check(p->pe[n], sig, rd); 
		if (p->icp->e.c != ICM_ERR_OK)
			return p->icp->e.c;
	}

	/* ~8 add check that input must be 3 channel if matrix is non-unity */

	return p->icp->e.c;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmLut1(icc *icp, icTagTypeSignature ttype
) {
	ICM_PE_SEQ_ALLOCINIT(icmLut1, icmSigPeLut816, ttype)

	p->dp = 2;		/* Dump padding */

	p->count = 4;	/* Pre-allocate pe[] */
	if (icmArrayResize(icp, &p->_count, &p->count,
	    (void **)&p->pe, sizeof(icmPe *), "icmLut1 array")) {
		return NULL;
	}

	if (ttype == icSigLut8Type) {
		p->bpv = 1;
		p->inputEnt  = 256;	/* By definition */
		p->outputEnt = 256;	/* By definition */
	} else {
		p->bpv = 2;
	}

	return (icmBase *)p;
}

/* ------------------------------------------------------------ */
/* RGB matrix-shaper transform (Not serialisable) */
/* Output is relative XYZ */

/* Dump a human readable description */
static void icmShaperMatrix_dump(icmShaperMatrix *p, icmFile *op, int verb) {
	int pad = p->dp;
	unsigned int i;

	op->printf(op,PAD("ShaperMatrix:\n"));
	op->printf(op,PAD("  Input Channels = %u\n"),p->inputChan);
	op->printf(op,PAD("  Output Channels = %u\n"),p->outputChan);
	op->printf(op,PAD("  No. elements = %u\n"),p->count);

	for (i = 0; i < p->count; i++) {
		op->printf(op,PAD("    PeType = %s\n"),icmPeSig2str(p->pe[i]->etype));
	}
}

/* Delete the container and all its contents */
static void icmShaperMatrix_del(icmShaperMatrix *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		unsigned int i;

		/* TagTypes are owned by icc. */

		/* We took a reference to Pe's, so release them... */
		for (i = 0; i < p->count; i++) {
			if (p->pe[i] != NULL) 
				p->pe[i]->del(p->pe[i]);			/* Deletes if last reference */
		}
		ICMFREEARRAY(p->icp, p->_count, p->pe)
		p->icp->al->free(p->icp->al, p);
	}
}

/* Create an empty object. Return null on error. */
/* Doesn't set icc error if tags not present. */
static icmPeSeq *new_icmShaperMatrix(
	icc *icp,
	int invert			/* If nz, setup inverse of shape-matrix */
) {
	icmPeCurveSet *cs;
	icmPeMatrix *mx;

	ICM_PE_SEQ_NS_ALLOCINIT(icmShaperMatrix, icmSigPeShaperMatrix)

	p->dp = 2;		/* Dump padding */

	p->count = 2;	/* Pre-allocate pe[] */
	if (icmArrayResize(icp, &p->_count, &p->count,
	    (void **)&p->pe, sizeof(icmPe *), "icmShaperMatrix array")) {
		return NULL;
	}
	p->outputChan = p->inputChan = 3;

	/* See if the color spaces are appropriate for the matrix type */
	if (icmCSSig2nchan(icp->header->colorSpace) != 3
	 || (icmCSSig2type(icp->header->colorSpace) & CSSigType_DEV) == 0
	 || (icmCSSig2type(icp->header->pcs) & CSSigType_PCS) == 0) {
		p->del(p);
		return NULL;
	}

	/* Find the appropriate tags */
	if ((p->redCurve = icp->read_tag(icp, icSigRedTRCTag)) == NULL
     || (  p->redCurve->ttype != icSigCurveType
	    )
	 || (p->greenCurve = icp->read_tag(icp, icSigGreenTRCTag)) == NULL
     || (  p->greenCurve->ttype != icSigCurveType
	    )

	 || (p->blueCurve = icp->read_tag(icp, icSigBlueTRCTag)) == NULL
     || (  p->blueCurve->ttype != icSigCurveType
	    )

	 || (p->redColrnt = (icmXYZArray *)icp->read_tag(icp, icSigRedColorantTag)) == NULL
     || p->redColrnt->ttype != icSigXYZType || p->redColrnt->count < 1

	 || (p->greenColrnt = (icmXYZArray *)icp->read_tag(icp, icSigGreenColorantTag)) == NULL
     || p->greenColrnt->ttype != icSigXYZType || p->greenColrnt->count < 1

	 || (p->blueColrnt = (icmXYZArray *)icp->read_tag(icp, icSigBlueColorantTag)) == NULL
     || p->blueColrnt->ttype != icSigXYZType || p->blueColrnt->count < 1) {

		p->del(p);
		return NULL;
	}

	/* Create and add Pe's */
	if ((p->pe[0] = icp->new_pe(icp, icmSig816Curves, icSigLut16Type)) == NULL) {
		p->del(p);
		return NULL;
	}
	cs = (icmPeCurveSet *)p->pe[0]; 

	cs->outputChan = cs->inputChan = 3;
	if (cs->allocate(cs)) { 	/* Allocate variable elements */
		p->del(p);
		return NULL;
	}
	cs->pe[0] = (icmPe *)p->redCurve->reference(p->redCurve);
	cs->pe[1] = (icmPe *)p->greenCurve->reference(p->greenCurve);
	cs->pe[2] = (icmPe *)p->blueCurve->reference(p->blueCurve);

	/* Matrix */
	if ((p->pe[1] = icp->new_pe(icp, icmSig816Matrix, icSigLut16Type)) == NULL) {
		p->del(p);
		return NULL;
	}
	mx = (icmPeMatrix *)p->pe[1]; 

	/* Copy the matrix */
	mx->outputChan = mx->inputChan = 3;
	mx->mx[0][0] = p->redColrnt->data[0].X;
	mx->mx[0][1] = p->greenColrnt->data[0].X;
	mx->mx[0][2] = p->blueColrnt->data[0].X;
	mx->mx[1][1] = p->greenColrnt->data[0].Y;
	mx->mx[1][0] = p->redColrnt->data[0].Y;
	mx->mx[1][2] = p->blueColrnt->data[0].Y;
	mx->mx[2][1] = p->greenColrnt->data[0].Z;
	mx->mx[2][0] = p->redColrnt->data[0].Z;
	mx->mx[2][2] = p->blueColrnt->data[0].Z;
	mx->ct[0] = mx->ct[1] = mx->ct[2] = 0.0;

	/* Workaround for buggy Kodak RGB profiles. Their matrix values */
	/* may be scaled to 100 rather than 1.0, and the colorant curves */
	/* may be scaled by 0.5 */
	if (icp->header->cmmId == icmstr2tag("KCMS")) {
		int i, j, oc = 0;
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				if (mx->mx[i][j] > 5.0)
					oc++;

		if (oc > 4) {		/* Looks like it */
			if ((p->icp->cflags & icmCFlagAllowQuirks) != 0) {
				// Fix it
				for (i = 0; i < 3; i++)
					for (j = 0; j < 3; j++)
						mx->mx[i][j] /= 100.0;
				p->icp->op = icmSnRead;			/* Let icmQuirkWarning know direction */
				icmQuirkWarning(icp, ICM_FMT_MATRIX_SCALE, 0, "Matrix profile values have wrong scale");
			} else {
				p->del(p);
				icmFormatWarning(icp, ICM_FMT_MATRIX_SCALE, "Matrix profile values have wrong scale");
				return NULL;
			}
		}
	}

	if (invert) {	/* Make this the inverse transform */
		/* inputChan & outputChan are the same, so don't have to swap... */
		p->pe[0] = new_icmPeInverter(icp, (icmPe *)mx);
		p->pe[1] = new_icmPeInverter(icp, (icmPe *)cs);
		mx->del(mx);		/* icmPeInverter takes a reference, so remove ours.. */
		cs->del(cs);
	}

	return (icmPeSeq *)p;
}

/* ------------------------------------------------------------ */
/* Monochrome matrix-mono transform (Not serialisable) */

/* Dump a human readable description */
static void icmShaperMono_dump(icmShaperMono *p, icmFile *op, int verb) {
	int pad = p->dp;
	unsigned int i;

	op->printf(op,PAD("ShaperMono:\n"));
	op->printf(op,PAD("  Input Channels = %u\n"),p->inputChan);
	op->printf(op,PAD("  Output Channels = %u\n"),p->outputChan);
	op->printf(op,PAD("  No. elements = %u\n"),p->count);

	for (i = 0; i < p->count; i++) {
		op->printf(op,PAD("    PeType = %s\n"),icmPeSig2str(p->pe[i]->etype));
	}
}

/* Delete the container and all its contents */
static void icmShaperMono_del(icmShaperMono *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		unsigned int i;

		/* TagTypes are owned by icc. */

		/* We took a reference to Pe's, so release them... */
		for (i = 0; i < p->count; i++) {
			if (p->pe[i] != NULL)
				p->pe[i]->del(p->pe[i]);			/* Deletes if last reference */
		}
		ICMFREEARRAY(p->icp, p->_count, p->pe)
		p->icp->al->free(p->icp->al, p);
	}
}

/* Create an empty object. Return null on error. */
/* Doesn't set icc error if tags not present. */
static icmPeSeq *new_icmShaperMono(
	icc *icp,
	int invert			/* If nz, setup inverse of shape-mono */
) {
	icmPe *cu, *mo;

	ICM_PE_SEQ_NS_ALLOCINIT(icmShaperMono, icmSigPeShaperMono)

	p->dp = 2;		/* Dump padding */

	p->count = 2;	/* Pre-allocate pe[] */
	if (icmArrayResize(icp, &p->_count, &p->count,
	    (void **)&p->pe, sizeof(icmPe *), "icmShaperMono array")) {
		return NULL;
	}
	p->inputChan = 1;
	p->outputChan = 3;

	/* See if the color spaces are appropriate for the mono type */
	if (icmCSSig2nchan(icp->header->colorSpace) != 1
	 || (icmCSSig2type(icp->header->colorSpace) & CSSigType_DEV) == 0
	 || (icmCSSig2type(icp->header->pcs) & CSSigType_PCS) == 0) {
		p->del(p);
		return NULL;
	}

	/* Find the appropriate tags */
	if ((p->grayCurve = icp->read_tag(icp, icSigGrayTRCTag)) == NULL
     || (  p->grayCurve->ttype != icSigCurveType
	    )) {
		p->del(p);
		return NULL;
	}

	/* Curve */
	p->pe[0] = cu = (icmPe *)p->grayCurve->reference(p->grayCurve);

	/* Mono conversion to PCS */
	if ((p->pe[1] = mo = (icmPe *)new_icmPeMono(icp)) == NULL) {
		p->del(p);
		return NULL;
	}

	if (invert) {	/* Make this the inverse transform */
		p->inputChan = 3;
		p->outputChan = 1;
		p->pe[0] = new_icmPeInverter(icp, (icmPe *)mo);
		p->pe[1] = new_icmPeInverter(icp, (icmPe *)cu);
		mo->del(mo);		/* icmPeInverter takes a reference, so remove ours.. */
		cu->del(cu);
	}

	return (icmPeSeq *)p;
}

/* ------------------------------------------------------------------------------ */
/* A Pe container that implements the monochrome to PCS conversion (Not serialisable) */ 
/* Output is relative PCS, i.e. XYZ or Lab */

/* Dump a human readable description */
static void icmPeMono_dump(icmPeMono *p, icmFile *op, int verb) {
	int pad = p->dp;

	op->printf(op,PAD("Mono:\n"));
	// ~8 isLab, white point...
}

static icmPe_lurv icmPeMono_lookup_fwd(icmPeMono *p, double *out, double *in) {
	double inv = in[0];		/* In case in & out are aliases */

	if (p->icp->header->pcs == icSigLabData) {
		out[0] = 100.0 * inv;
		out[1] = 0.0   * inv;
		out[2] = 0.0   * inv;
	} else {
		out[0] = inv * p->icp->header->illuminant.X;
		out[1] = inv * p->icp->header->illuminant.Y;
		out[2] = inv * p->icp->header->illuminant.Z;
	}

	return icmPe_lurv_OK;
}

static icmPe_lurv icmPeMono_lookup_bwd(icmPeMono *p, double *out, double *in) {
	if (p->icp->header->pcs == icSigLabData) {
		out[0] = in[0]/100.0;	/* Prop of PCS L* */
	} else {
		out[0] = in[1]/p->icp->header->illuminant.Y;	/* Prop of PCS Y */
	}

	return icmPe_lurv_OK;
}

/* Delete the container and all its contents */
static void icmPeMono_del(icmPeMono *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->icp->al->free(p->icp->al, p);
	}
}

static icmPe *new_icmPeMono(
	icc *icp
) {
	ICM_PE_NS_ALLOCINIT(icmPeMono, icmSigPeMono)
	p->inputChan = 1;
	p->outputChan = 3;
	ICM_PE_SETATTR(p->attr, 0, 0, 0, icmPeOp_matrix, 1, 1)
	p->lookup_fwd = icmPeMono_lookup_fwd;
	p->lookup_bwd = icmPeMono_lookup_bwd;

	return (icmPe *)p;
}

/* ------------------------------------------------------------------------------ */
/* A Pe container that inverts another Pe (Not serialisable) */ 
/* [ Note that we can't directly modify a Pe to be its inverse */
/*   because it may be shared via a reference. A way of getting rid */
/*   of icmPeInverter would be to add a clone_invert() method to */
/*   icSigCurveType, icSigParametricCurveType, icmPeCurveSet, icmPeMatrix &  icmPeMono. ] */

/* Dump a human readable description */
static void icmPeInverter_dump(icmPeInverter *p, icmFile *op, int verb) {
	int pad = p->dp;

	op->printf(op,PAD("Inverter:\n"));
	p->pe->dp = pad + 2;
	p->pe->dump(p->pe, op, verb);
}

/* Delete the container and all its contents */
static void icmPeInverter_del(icmPeInverter *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->pe->del(p->pe);				/* Deletes if last reference */
		p->icp->al->free(p->icp->al, p);
	}
}

static icmPe_lurv icmPeInverter_trace_lookup_fwd(icmPeInverter *p, double *out, double *in) {
	icmPe_lurv rv = icmPe_lurv_OK;
	int pad = p->trace > 0 ? p->trace -1 : 0;
	int ctr = p->pe->trace;

	printf(PAD("PeInverter fwd:\n"));
	if (p->trace <= 1)
		printf(PAD("  Input %s\n"),icmPdv(p->inputChan, in));

	if (!p->pe->attr.comp)
		printf(PAD(" Pe %s bwd:\n"),icmPeSig2str(p->pe->etype));
	p->pe->trace = p->trace + 1;
	rv = p->pe->lookup_bwd(p->pe, out, in);
	p->pe->trace = ctr;
	if (!p->pe->attr.comp)
		printf(PAD("  Output %s\n"),icmPdv(p->outputChan, out));

	return rv;
}

static icmPe_lurv icmPeInverter_trace_lookup_bwd(icmPeInverter *p, double *out, double *in) {
	icmPe_lurv rv = icmPe_lurv_OK;
	int pad = p->trace > 0 ? p->trace -1 : 0;
	int ctr = p->pe->trace;

	printf(PAD("PeInverter bwd:\n"));
	if (p->trace <= 1)
		printf(PAD("  Input %s\n"),icmPdv(p->outputChan, in));

	if (!p->pe->attr.comp)
		printf(PAD(" Pe %s fwd:\n"),icmPeSig2str(p->pe->etype));
	p->pe->trace = p->trace + 1;
	rv = p->pe->lookup_fwd(p->pe, out, in);
	p->pe->trace = ctr;
	if (!p->pe->attr.comp)
		printf(PAD("  Output %s\n"),icmPdv(p->inputChan, out));

	return rv;
}

static icmPe_lurv icmPeInverter_lookup_fwd(icmPeInverter *p, double *out, double *in) {
	if (p->trace)
		return icmPeInverter_trace_lookup_fwd(p, out, in);
	return p->pe->lookup_bwd(p->pe, out, in);
}

static icmPe_lurv icmPeInverter_lookup_bwd(icmPeInverter *p, double *out, double *in) {
	if (p->trace)
		return icmPeInverter_trace_lookup_bwd(p, out, in);
	return p->pe->lookup_fwd(p->pe, out, in);
}

static int icmPeInverter_init(icmPeInverter *p) {
	int rv;

	if ((rv = p->pe->init(p->pe)) != ICM_ERR_OK)
		return rv;

	/* Simply reverse the attr's of what's being inverted */
	/* and make our op be the Pe op */
	ICM_PE_SETATTR(p->attr, 1, 0, p->pe->attr.norm, p->pe->attr.op, p->pe->attr.bwd,
	          p->pe->attr.fwd)

	return ICM_ERR_OK;
}
/* Make Pe appear to be its inverse. */
/* icmPeInverter takes a reference. */
static icmPe *new_icmPeInverter(
	icc *icp,
	icmPe *pe
) {
	ICM_PE_NS_ALLOCINIT(icmPeInverter, icmSigPeInverter)

	p->init = icmPeInverter_init;
	p->inputChan  = pe->outputChan;
	p->outputChan = pe->inputChan;

	p->pe = pe->reference(pe);
	p->lookup_fwd = icmPeInverter_lookup_fwd;
	p->lookup_bwd = icmPeInverter_lookup_bwd;

	return (icmPe *)p;
}

/* ------------------------------------------------------------------------------ */
/* XYZ abs <-> XYZ rel Pe (This is not a serialisable tagtype) */

static void icmPeAbs2Rel_dump(icmPeAbs2Rel *p, icmFile *op, int verb) {
	int pad = p->dp;

	if (p->attr.inv)
		op->printf(op,PAD("PeRel2Abs:\n"));
	else
		op->printf(op,PAD("PeAbs2Rel:\n"));
}

static void icmPeAbs2Rel_del(icmPeAbs2Rel *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->icp->al->free(p->icp->al, p);
	}
}

static icmPe_lurv icmPeAbs2Rel_lookup_fwd(icmPeAbs2Rel *p, double *out, double *in) {
	icmMulBy3x3(out, p->lu->fromAbs, in);
	return icmPe_lurv_OK;
}

static icmPe_lurv icmPeAbs2Rel_lookup_bwd(icmPeAbs2Rel *p, double *out, double *in) {
	icmMulBy3x3(out, p->lu->toAbs, in);
	return icmPe_lurv_OK;
}

static icmPe *new_icmPeAbs2Rel(
	struct _icc *icp,
	struct _icmLu4Space *lu,		/* wp, toAbs, fromAbs */
	int invert				/* Rel->Abs */
) {
	ICM_PE_NS_ALLOCINIT(icmPeAbs2Rel, icmSigPeAbs2Rel)

	p->inputChan = p->outputChan = 3;
	p->lu = lu;

	ICM_PE_SETATTR(p->attr, 0, invert, 0, icmPeOp_matrix, 1, 1)
	if (invert) {
		p->lookup_fwd = icmPeAbs2Rel_lookup_bwd;
		p->lookup_bwd = icmPeAbs2Rel_lookup_fwd;
	} else {
		p->lookup_fwd = icmPeAbs2Rel_lookup_fwd;
		p->lookup_bwd = icmPeAbs2Rel_lookup_bwd;
	}

	return (icmPe *)p;
}

/* ------------------------------------------------------------------------------ */
/* XYZ <-> Lab Pe (This is not a serialisable tagtype) */

static void icmPeXYZ2Lab_dump(icmPeXYZ2Lab *p, icmFile *op, int verb) {
	int pad = p->dp;

	if (p->attr.inv)
		op->printf(op,PAD("PeLab2XYZ:\n"));
	else
		op->printf(op,PAD("PeXYZ2Lab:\n"));
}

static void icmPeXYZ2Lab_del(icmPeXYZ2Lab *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->icp->al->free(p->icp->al, p);
	}
}

static icmPe_lurv icmPeXYZ2Lab_lookup_fwd(icmPeXYZ2Lab *p, double *out, double *in) {
	icmXYZ2Lab(&p->lu->pcswht, out, in);
	return icmPe_lurv_OK;
}

static icmPe_lurv icmPeXYZ2Lab_lookup_bwd(icmPeXYZ2Lab *p, double *out, double *in) {
	icmLab2XYZ(&p->lu->pcswht, out, in);
	return icmPe_lurv_OK;
}

static icmPe *new_icmPeXYZ2Lab(
	struct _icc *icp,
	struct _icmLu4Space *lu,		/* pcswht */
	int invert				/* Lab->XYZ */
) {
	ICM_PE_NS_ALLOCINIT(icmPeXYZ2Lab, icmSigPeXYZ2Lab)

	p->inputChan = p->outputChan = 3;
	p->lu = lu;

	ICM_PE_SETATTR(p->attr, 0, invert, 0, icmPeOp_matrix, 1, 1)
	if (invert) {
		p->lookup_fwd = icmPeXYZ2Lab_lookup_bwd;
		p->lookup_bwd = icmPeXYZ2Lab_lookup_fwd;
	} else {
		p->lookup_fwd = icmPeXYZ2Lab_lookup_fwd;
		p->lookup_bwd = icmPeXYZ2Lab_lookup_bwd;
	}

	return (icmPe *)p;
}

/* ------------------------------------------------------------------------------ */
/* XYZ <-> XYZ8 Pe (This is not a serialisable tagtype) */

static void icmPeXYZ2XYZ8_dump(icmPeXYZ2XYZ8 *p, icmFile *op, int verb) {
	int pad = p->dp;

	if (p->attr.inv)
		op->printf(op,PAD("PeXYZ82XYZ:\n"));
	else
		op->printf(op,PAD("PeXYZ2XYZ8:\n"));
}

static void icmPeXYZ2XYZ8_del(icmPeXYZ2XYZ8 *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->icp->al->free(p->icp->al, p);
	}
}

/* Convert XYZ to 8 bit Normalised */ 
static icmPe_lurv icmPeXYZ2XYZ8_lookup_fwd(icmPeXYZ2XYZ8 *p, double *out, double *in) {
	out[0] = in[0] * (1.0/(1.0 + 127.0/128));
	out[1] = in[1] * (1.0/(1.0 + 127.0/128));
	out[2] = in[2] * (1.0/(1.0 + 127.0/128));
	return icmPe_lurv_OK;
}

/* Convert 8 bit Normalised value XYZ */
static icmPe_lurv icmPeXYZ2XYZ8_lookup_bwd(icmPeXYZ2XYZ8 *p, double *out, double *in) {
	out[0] = in[0] * (1.0 + 127.0/128); /* X */
	out[1] = in[1] * (1.0 + 127.0/128); /* Y */
	out[2] = in[2] * (1.0 + 127.0/128); /* Z */
	return icmPe_lurv_OK;
}

static icmPe *new_icmPeXYZ2XYZ8(
	struct _icc *icp,
	int invert				/* Lab->XYZ */
) {
	ICM_PE_NS_ALLOCINIT(icmPeXYZ2XYZ8, icmSigPeXYZ2XYZ8)

	p->inputChan = p->outputChan = 3;

	if (invert) {
		ICM_PE_SETATTR(p->attr, 0, invert, 1, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeXYZ2XYZ8_lookup_bwd;
		p->lookup_bwd = icmPeXYZ2XYZ8_lookup_fwd;
	} else {
		ICM_PE_SETATTR(p->attr, 0, invert, 1, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeXYZ2XYZ8_lookup_fwd;
		p->lookup_bwd = icmPeXYZ2XYZ8_lookup_bwd;
	}

	return (icmPe *)p;
}

/* ------------------------------------------------------------------------------ */
/* XYZ <-> XYZ16 Pe (This is not a serialisable tagtype) */

static void icmPeXYZ2XYZ16_dump(icmPeXYZ2XYZ16 *p, icmFile *op, int verb) {
	int pad = p->dp;

	if (p->attr.inv)
		op->printf(op,PAD("PeXYZ162XYZ:\n"));
	else
		op->printf(op,PAD("PeXYZ2XYZ16:\n"));
}

static void icmPeXYZ2XYZ16_del(icmPeXYZ2XYZ16 *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->icp->al->free(p->icp->al, p);
	}
}

/* Convert XYZ to 16 bit Normalised */
static icmPe_lurv icmPeXYZ2XYZ16_lookup_fwd(icmPeXYZ2XYZ16 *p, double *out, double *in) {
	out[0] = in[0] * (1.0/(1.0 + 32767.0/32768));
	out[1] = in[1] * (1.0/(1.0 + 32767.0/32768));
	out[2] = in[2] * (1.0/(1.0 + 32767.0/32768));
	return icmPe_lurv_OK;
}

/* Convert 16 bit Normalised value XYZ */
static icmPe_lurv icmPeXYZ2XYZ16_lookup_bwd(icmPeXYZ2XYZ16 *p, double *out, double *in) {
	out[0] = in[0] * (1.0 + 32767.0/32768); /* X */
	out[1] = in[1] * (1.0 + 32767.0/32768); /* Y */
	out[2] = in[2] * (1.0 + 32767.0/32768); /* Z */
	return icmPe_lurv_OK;
}

static icmPe *new_icmPeXYZ2XYZ16(
	struct _icc *icp,
	int invert				/* Lab->XYZ */
) {
	ICM_PE_NS_ALLOCINIT(icmPeXYZ2XYZ16, icmSigPeXYZ2XYZ16)

	p->inputChan = p->outputChan = 3;

	if (invert) {
		ICM_PE_SETATTR(p->attr, 0, invert, 1, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeXYZ2XYZ16_lookup_bwd;
		p->lookup_bwd = icmPeXYZ2XYZ16_lookup_fwd;
	} else {
		ICM_PE_SETATTR(p->attr, 0, invert, 1, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeXYZ2XYZ16_lookup_fwd;
		p->lookup_bwd = icmPeXYZ2XYZ16_lookup_bwd;
	}

	return (icmPe *)p;
}

/* ------------------------------------------------------------------------------ */
/* Lab <-> Lab8 Pe (This is not a serialisable tagtype) */

static void icmPeLab2Lab8_dump(icmPeLab2Lab8 *p, icmFile *op, int verb) {
	int pad = p->dp;

	if (p->attr.inv)
		op->printf(op,PAD("PeLab82Lab:\n"));
	else
		op->printf(op,PAD("PeLab2Lab8:\n"));
}

static void icmPeLab2Lab8_del(icmPeLab2Lab8 *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->icp->al->free(p->icp->al, p);
	}
}

/* Convert Lab to 8 bit Normalised */ 
static icmPe_lurv icmPeLab2Lab8_lookup_fwd(icmPeLab2Lab8 *p, double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;				/* L */
	out[1] = (in[1] + 128.0) * 1.0/255.0;	/* a */
	out[2] = (in[2] + 128.0) * 1.0/255.0;	/* b */
	return icmPe_lurv_OK;
}

/* Convert 8 bit Normalised value to Lab */
static icmPe_lurv icmPeLab2Lab8_lookup_bwd(icmPeLab2Lab8 *p, double *out, double *in) {
	out[0] = in[0] * 100.0;				/* L */
	out[1] = (in[1] * 255.0) - 128.0;	/* a */
	out[2] = (in[2] * 255.0) - 128.0;	/* b */
	return icmPe_lurv_OK;
}

static icmPe *new_icmPeLab2Lab8(
	struct _icc *icp,
	int invert				/* Lab->Lab */
) {
	ICM_PE_NS_ALLOCINIT(icmPeLab2Lab8, icmSigPeLab2Lab8)

	p->inputChan = p->outputChan = 3;

	if (invert) {
		ICM_PE_SETATTR(p->attr, 0, invert, 1, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeLab2Lab8_lookup_bwd;
		p->lookup_bwd = icmPeLab2Lab8_lookup_fwd;
	} else {
		ICM_PE_SETATTR(p->attr, 0, invert, 1, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeLab2Lab8_lookup_fwd;
		p->lookup_bwd = icmPeLab2Lab8_lookup_bwd;
	}

	return (icmPe *)p;
}

/* ------------------------------------------------------------------------------ */
/* Lab <-> LabV2 Pe (This is not a serialisable tagtype) */

static void icmPeLab2LabV2_dump(icmPeLab2LabV2 *p, icmFile *op, int verb) {
	int pad = p->dp;

	if (p->attr.inv)
		op->printf(op,PAD("PeLabV22Lab:\n"));
	else
		op->printf(op,PAD("PeLab2LabV2:\n"));
}

static void icmPeLab2LabV2_del(icmPeLab2LabV2 *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->icp->al->free(p->icp->al, p);
	}
}

/* Convert Lab to 16 bit V2 Normalised */ 
static icmPe_lurv icmPeLab2LabV2_lookup_fwd(icmPeLab2LabV2 *p, double *out, double *in) {
	out[0] = in[0] * 65280.0/(100.0 * 65535.0);				/* L */
	out[1] = (in[1] + 128.0) * 65280.0/(255.0 * 65535.0);	/* a */
	out[2] = (in[2] + 128.0) * 65280.0/(255.0 * 65535.0);	/* b */
	return icmPe_lurv_OK;
}

/* Convert 16 bit V2 Normalised value to Lab */
static icmPe_lurv icmPeLab2LabV2_lookup_bwd(icmPeLab2LabV2 *p, double *out, double *in) {
	out[0] = in[0] * (100.0 * 65535.0)/65280.0;			/* L */
	out[1] = (in[1] * (255.0 * 65535.0)/65280) - 128.0;	/* a */
	out[2] = (in[2] * (255.0 * 65535.0)/65280) - 128.0;	/* b */
	return icmPe_lurv_OK;
}

static icmPe *new_icmPeLab2LabV2(
	struct _icc *icp,
	int invert				/* Lab->Lab */
) {
	ICM_PE_NS_ALLOCINIT(icmPeLab2LabV2, icmSigPeLab2LabV2)

	p->inputChan = p->outputChan = 3;

	if (invert) {
		ICM_PE_SETATTR(p->attr, 0, invert, 1, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeLab2LabV2_lookup_bwd;
		p->lookup_bwd = icmPeLab2LabV2_lookup_fwd;
	} else {
		ICM_PE_SETATTR(p->attr, 0, invert, 1, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeLab2LabV2_lookup_fwd;
		p->lookup_bwd = icmPeLab2LabV2_lookup_bwd;
	}

	return (icmPe *)p;
}

/* ------------------------------------------------------------------------------ */
/* Generic <-> Normalized Pe (This is not a serialisable tagtype) */
/* min/max difference is sanity limited to be at least 1e-4. */
/* (The normalized range is 0.0 - 1.0) */

static void icmPeGeneric2Norm_dump(icmPeGeneric2Norm *p, icmFile *op, int verb) {
	int pad = p->dp;

	if (p->attr.inv) {
		op->printf(op,PAD("Norm2Generic (%s):\n"),p->ident);
	} else {
		op->printf(op,PAD("PeGeneric2Norm (%s):\n"), p->ident);
	}
	if (verb >= 1) {
		op->printf(op,PAD("  full  min %s, max %s\n"),icmPdv(p->inputChan, p->min),icmPdv(p->inputChan, p->max));
		op->printf(op,PAD("  norm min %s, max %s\n"),icmPdv(p->inputChan, p->nmin),icmPdv(p->inputChan, p->nmax));
	}
}

static void icmPeGeneric2Norm_del(icmPeGeneric2Norm *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->icp->al->free(p->icp->al, p);
	}
}

/* Convert full range to normalised */ 
static icmPe_lurv icmPeGeneric2Norm_lookup_fwd(icmPeGeneric2Norm *p, double *out, double *in) {
	unsigned int i;

	for (i = 0; i < p->inputChan; i++) {
		double val = (in[i] - p->min[i])/(p->max[i] - p->min[i]);
		out[i] = val * (p->nmax[i] - p->nmin[i]) + p->nmin[i];
	}

	return icmPe_lurv_OK;
}

/* Convert from normalised to full range */
static icmPe_lurv icmPeGeneric2Norm_lookup_bwd(icmPeGeneric2Norm *p, double *out, double *in) {
	unsigned int i;

	for (i = 0; i < p->inputChan; i++) {
		double val = (in[i] - p->nmin[i])/(p->nmax[i] - p->nmin[i]);
		out[i] = val * (p->max[i] - p->min[i]) + p->min[i];
	}

	return icmPe_lurv_OK;
}

static icmPe *new_icmPeFullyGeneric2Norm(
	struct _icc *icp,
	unsigned int nchan,
	double *min, double *max,		/* Per channel range to be normalised */
	double *nmin, double *nmax,		/* Per channel range to normalize to (NULL = 0..1) */
	char *ident,					/* Encoding identifier */
	int invert
) {
	unsigned int i;
	ICM_PE_NS_ALLOCINIT(icmPeGeneric2Norm, icmSigPeGeneric2Norm)

	p->inputChan = p->outputChan = nchan;

	for (i = 0; i < nchan; i++) {

		if (min[i] > max[i]) {		/* Hmm */
			double tt = min[i];
			min[i] = max[i];
			max[i] = tt;
		}
		p->min[i] = min[i];
		p->max[i] = max[i];
		if ((p->max[i] - p->min[i]) < 1e-4) {
			p->max[i] += 0.5e-4;
			p->min[i] -= 0.5e-4;
		}

		if (nmin == NULL) 
			p->nmin[i] = 0.0;
		else
			p->nmin[i] = nmin[i];
		if (nmax == NULL) 
			p->nmax[i] = 1.0;
		else
			p->nmax[i] = nmax[i];

		if (p->nmin[i] > p->nmax[i]) {		/* Hmm */
			double tt = p->nmin[i];
			p->nmin[i] = p->nmax[i];
			p->nmax[i] = tt;
		}

		if ((p->nmax[i] - p->nmin[i]) < 1e-4) {
			p->nmax[i] += 0.5e-4;
			p->nmin[i] -= 0.5e-4;
		}
	}

	strncpy(p->ident, ident, 50);  p->ident[49] = '\000';

	if (invert) {
		ICM_PE_SETATTR(p->attr, 0, invert, 1, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeGeneric2Norm_lookup_bwd;
		p->lookup_bwd = icmPeGeneric2Norm_lookup_fwd;
	} else {
		ICM_PE_SETATTR(p->attr, 0, invert, 1, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeGeneric2Norm_lookup_fwd;
		p->lookup_bwd = icmPeGeneric2Norm_lookup_bwd;
	}

	return (icmPe *)p;
}

static icmPe *new_icmPeGeneric2Norm(
	struct _icc *icp,
	unsigned int nchan,
	double *min, double *max,		/* Per channel range to be normalised */
	char *ident,					/* Encoding identifier */
	int invert
) {

	return new_icmPeFullyGeneric2Norm(icp, nchan, min, max, NULL, NULL, ident, invert);
}

/* ------------------------------------------------------------------------------ */
/* A point to grid alignment transform. */ 
/* This creates a per channel transform that piecwize bends */
/* the input so as to land the given point on a grid point. */
/* The values are assumed in the range 0..1 */

static void icmPeGridAlign_dump(icmPeGridAlign *p, icmFile *op, int verb) {
	int pad = p->dp;

	op->printf(op,PAD("PeGridAlign:\n"));
	op->printf(op,PAD(" src %s\n"),icmPdv(p->inputChan, p->src));
	op->printf(op,PAD(" dst %s\n"),icmPdv(p->inputChan, p->dst));
}

static void icmPeGridAlign_del(icmPeGridAlign *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->icp->al->free(p->icp->al, p);
	}
}

/* Align the src point to the dst */
static icmPe_lurv icmPeGridAlign_lookup_fwd(icmPeGridAlign *p, double *out, double *in) {
	unsigned int i;

	for (i = 0; i < p->inputChan; i++) {
		if (in[i] <= p->src[i])
			out[i] = in[i] * p->fwdlow[i];
		else
			out[i] = 1.0 - (1.0 - in[i]) * p->fwdhigh[i];
	}

	return icmPe_lurv_OK;
}

/* Align the dst point to the src */
static icmPe_lurv icmPeGridAlign_lookup_bwd(icmPeGridAlign *p, double *out, double *in) {
	unsigned int i;

	for (i = 0; i < p->inputChan; i++) {
		if (in[i] < p->dst[i])
			out[i] = p->bwdlowoff[i] + in[i] * p->bwdlow[i];
		else
			out[i] = p->bwdhighoff[i] + 1.0 - (1.0 - in[i]) * p->bwdhigh[i];
	}

	return icmPe_lurv_OK;
}

static icmPe *new_icmPeGridAlign(
	struct _icc *icp,
	unsigned int nchan,
	double *point,				/* Point to aim for, must be 0..1 */
	unsigned int *gridres,		/* Grid resolution for each channel */
	int invert
) {
	unsigned int i;
	ICM_PE_NS_ALLOCINIT(icmPeGridAlign, icmSigPeGridAlign)

	p->inputChan = p->outputChan = nchan;

	icmClipN(p->src, point, nchan);		/* Just to be sure... */

	for (i = 0; i < nchan; i++) {
		unsigned int ix;

		/* Skip any grid with a low res */
		if (gridres[i] < 9) {
			/* Make this a NOP */
			p->bwdlowoff[i] = p->bwdhighoff[i] = 0.0;
			p->src[i] = p->dst[i] = 0.5;
			p->fwdlow[i] = 1.0;
			p->fwdhigh[i] = 1.0;
			p->bwdlow[i] = 1.0;
			p->bwdhigh[i] = 1.0;
			continue;
		}

		/* Closest index to src point. This sets our raw destination. */
		ix = (unsigned int)floor(p->src[i] * (gridres[i]-1.0) + 0.5);

		p->dst[i] = ix/(gridres[i]-1.0); 

		if (fabs(p->src[i]) > 1e-9)
			p->fwdlow[i] = p->dst[i]/p->src[i];
		else
			p->fwdlow[i] = 1.0;			/* dst must be zero too... */

		if (fabs(p->src[i] - 1.0) > 1e-9)
			p->fwdhigh[i] = (1.0 - p->dst[i])/(1.0 - p->src[i]);
		else
			p->fwdhigh[i] = 1.0;		/* dst must be zero too... */

		p->bwdlowoff[i] = p->bwdhighoff[i] = 0.0;

		/* For completeness we handle the case where src != edge and dst == edge: */
		if (fabs(p->dst[i]) > 1e-9)
			p->bwdlow[i] = p->src[i]/p->dst[i];
		else {
			p->bwdlow[i] = 0.0;			/* clip to src */
			p->bwdlowoff[i] = p->src[i];
		}

		if (fabs(p->dst[i] - 1.0) > 1e-9)
			p->bwdhigh[i] = (1.0 - p->src[i])/(1.0 - p->dst[i]);
		else {
			p->bwdhigh[i] = 0.0;		/* clip to src */
			p->bwdhighoff[i] = p->src[i] - 1.0;
		}
	}

	if (invert) {
		ICM_PE_SETATTR(p->attr, 0, invert, 0, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeGridAlign_lookup_bwd;
		p->lookup_bwd = icmPeGridAlign_lookup_fwd;
	} else {
		ICM_PE_SETATTR(p->attr, 0, invert, 0, icmPeOp_perch, 1, 1)
		p->lookup_fwd = icmPeGridAlign_lookup_fwd;
		p->lookup_bwd = icmPeGridAlign_lookup_bwd;
	}

#ifdef NEVER
{
double tin[MAX_CHAN], tout[MAX_CHAN];
printf("~1 src %s\n",icmPdv(p->inputChan, p->src));
printf("~1 dst %s\n",icmPdv(p->inputChan, p->dst));
printf("~1 fwdlow %s\n",icmPdv(p->inputChan, p->fwdlow));
printf("~1 fwdhigh %s\n",icmPdv(p->inputChan, p->fwdhigh));
printf("~1 bwdlow %s\n",icmPdv(p->inputChan, p->bwdlow));
printf("~1 bwdlowoff %s\n",icmPdv(p->inputChan, p->bwdlowoff));
printf("~1 bwdhigh %s\n",icmPdv(p->inputChan, p->bwdhigh));
printf("~1 bwdhighoff %s\n",icmPdv(p->inputChan, p->bwdhighoff));

icmCpyN(tin, point, p->inputChan);
p->lookup_fwd(p, tout, tin);
printf("~1 in  %s\n",icmPdv(p->inputChan, tin));
printf("~1 out %s\n",icmPdv(p->inputChan, tout));
p->lookup_bwd(p, tin, tout);
printf("~1 in  %s\n\n",icmPdv(p->inputChan, tin));

icmSetN(tin, 0.0, p->inputChan);
p->lookup_fwd(p, tout, tin);
printf("~1 in  %s\n",icmPdv(p->inputChan, tin));
printf("~1 out %s\n",icmPdv(p->inputChan, tout));
p->lookup_bwd(p, tin, tout);
printf("~1 in  %s\n\n",icmPdv(p->inputChan, tin));

icmSetN(tin, 1.0, p->inputChan);
p->lookup_fwd(p, tout, tin);
printf("~1 in  %s\n",icmPdv(p->inputChan, tin));
printf("~1 out %s\n",icmPdv(p->inputChan, tout));
p->lookup_bwd(p, tin, tout);
printf("~1 in  %s\n\n",icmPdv(p->inputChan, tin));
}
#endif /* NEVER */

	return (icmPe *)p;
}

/* ------------------------------------------------------------------------------ */
/* A NOP (This is not a serialisable tagtype) */

static void icmPeNOP_dump(icmPeNOP *p, icmFile *op, int verb) {
	int pad = p->dp;

	op->printf(op,PAD("PeNOP:\n"));
}

static void icmPeNOP_del(icmPeNOP *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		p->icp->al->free(p->icp->al, p);
	}
}

static icmPe_lurv icmPeNOP_lookup(icmPeNOP *p, double *out, double *in) {
	if (out != in) {
		unsigned int i;

		for (i = 0; i < p->inputChan; i++)
			out[i] = in[i];
	}

	return icmPe_lurv_OK;
}


static icmPe *new_icmPeNOP(
	struct _icc *icp,
	unsigned int nchan
) {
	ICM_PE_NS_ALLOCINIT(icmPeNOP, icmSigPeNOP)

	p->inputChan = p->outputChan = nchan;

	ICM_PE_SETATTR(p->attr, 0, 0, 0, icmPeOp_perch, 1, 1)
	p->lookup_fwd = icmPeNOP_lookup;
	p->lookup_bwd = icmPeNOP_lookup;

	return (icmPe *)p;
}

/* ====================================================================== */
/* Processing Element Container (Not serialisable) */

/* Dump a human readable description */
static void icmPeContainer_dump(icmPeContainer *p, icmFile *op, int verb) {
	int pad = p->dp;
	unsigned int i;

	op->printf(op,PAD("PeContainer:\n"));
	op->printf(op,PAD("  Attributes = %s\n"),icmPe_Attr2Str(&p->attr));
	op->printf(op,PAD("  Input Channels = %u\n"),p->inputChan);
	op->printf(op,PAD("  Output Channels = %u\n"),p->outputChan);
	op->printf(op,PAD("  No. elements = %u\n"),p->count);

	for (i = 0; i < p->count; i++) {
		op->printf(op,PAD("  Element %u:\n"),i);
		if (p->pe[i]->etype == icmSigPeContainer) {
			p->pe[i]->dp = pad + 2;
			p->pe[i]->dump(p->pe[i], op, verb);
		} else {
			op->printf(op,PAD("    PeType = %s\n"),icmPeSig2str(p->pe[i]->etype));
		}
	}
}

/* Add a Pe to the end of the sequence. */
/* Any empty or attr == NOP are skipped. */
static int icmPeContainer_append(icmPeContainer *p, icmPe *pe) {

	if (pe == NULL || pe->attr.op == icmPeOp_NOP)
		return ICM_ERR_OK;

	p->count++;
	if (icmArrayResize(p->icp, &p->_count, &p->count,
	    (void **)&p->pe, sizeof(icmPe *), "icmPeContainer array")) {
		return ICM_ERR_MALLOC;
	}

	pe->reference(pe);
	p->pe[p->count-1] = pe;

	return ICM_ERR_OK;
}

/* Add a Pe to the end of the sequence */
static int icmPeContainer_prepend(icmPeContainer *p, icmPe *pe) {
	return p->insert(p, 0, pe);
}

/* Insert a Pe before the given index */
static int icmPeContainer_insert(icmPeContainer *p, unsigned int ix, icmPe *pe) {
	unsigned int i;

	if (ix >= p->count)
		return icm_err(p->icp, ICM_ERR_PECONT_BOUND, "icmPeContainer_insert ix bounds");

	p->count++;
	if (icmArrayResize(p->icp, &p->_count, &p->count,
	    (void **)&p->pe, sizeof(icmPe *), "icmPeContainer array")) {
		return ICM_ERR_MALLOC;
	}
	for (i = p->count-1; i > ix; i--)
		p->pe[i] = p->pe[i-1];

	pe->reference(pe);
	p->pe[ix] = pe;

	return ICM_ERR_OK;
}

/* Replace the Pe at the given index */
static int icmPeContainer_replace(icmPeContainer *p, unsigned int ix, icmPe *pe) {
	if (ix >= p->count)
		return icm_err(p->icp, ICM_ERR_PECONT_BOUND, "icmPeContainer_replace ix bounds");

	p->pe[ix]->del(p->pe[ix]);
	pe->reference(pe);
	p->pe[ix] = pe;

	return ICM_ERR_OK;
}

/* Remove the Pe at the given index. */
static int icmPeContainer_remove(icmPeContainer *p, unsigned int ix) {
	unsigned int i;

	if (ix >= p->count)
		return icm_err(p->icp, ICM_ERR_PECONT_BOUND, "icmPeContainer_remove ix bounds");

	p->pe[ix]->del(p->pe[ix]);

	for (i = ix; i < (p->count-1); i++)
		p->pe[i] = p->pe[i+1];

	p->count--;
	if (icmArrayResize(p->icp, &p->_count, &p->count,
	    (void **)&p->pe, sizeof(icmPe *), "icmPeContainer array")) {
		return ICM_ERR_MALLOC;
	}

	return ICM_ERR_OK;
}

/* Append Pe's from another icmPeContainer, */
/* starting at six and ending before eix. */
/* Any empty or attr == NOP are skipped. */
/* This will flatten any sub-icmPeSeq, resulting in a sequence */
/* of direct Pe's or direct Pe's inside an Inverter. */
static int icmPeContainer_append_pes(icmPeContainer *p, icmPeSeq *src, int six, int eix) {
	int ix;

	for (ix = six; ix < src->count && ix < eix; ix++) {
		if (src->pe[ix] != NULL && src->pe[ix]->attr.op != icmPeOp_NOP) {
			int rv;

			/* We need to recurse */
			if (src->pe[ix]->isPeSeq) {
				icmPeSeq *rsrc = (icmPeSeq *)src->pe[ix];

				if ((rv = p->append_pes(p, rsrc, 0, rsrc->count)) != ICM_ERR_OK)
					return rv;

			} else {
				if (src->pe[ix]->etype == icmSigPeInverter
				 && ((icmPeInverter *)src->pe[ix])->pe->isPeSeq) {
					/* We're not currently handling an icmPeSeq inside an inverter. */
					/* We could add support if needed by adding each element in */
					/* reverse order each inside its own inverter. Inverting an */
					/* inverter would remove the need for an Inverter, etc. */
					return icm_err(p->icp, ICM_ERR_APPEND_PES_INTERNAL,"icmPeContainer_append_pes found icmPeSeq within inverter - we don't handle that at the moment!");
				} else {
					if ((rv = p->append(p, src->pe[ix])) != 0)
					return rv;
				}
			}
		}
	}
	return ICM_ERR_OK;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Various icmPeContainer/icmPeSeq characteristic analysis methods: */
/* These all assume that the icmPeContainer is in a flattened state. */

/* Find the largest per channel input lut resolution. */
/* res may be NULL */
/* Returns 0 if there is no per channel lut in the input sequence. */
static unsigned int icmPeContainer_max_in_res(icmPeContainer *p, int res[MAX_CHAN]) {
	unsigned int ix, e;
	unsigned int maxres = 0;

	if (res != NULL) {
		for (e = 0; e < MAX_CHAN; e++)
			res[e] = 0;
	}

	/* Search forwards */
	for (ix = 0; ix < p->count; ix++) {
		icmPe *pe = p->pe[ix];

		if (pe == NULL)
			continue;

		if (pe->isPeSeq) {
			icm_err(p->icp, ICM_ERR_SEARCH_PESEQ_INTERNAL, "icmPeContainer_max_in_res found unexpected icmPeSeq inside icmPeContainer");
			return 0;

		} else if (pe->etype == icmSigPeInverter) {
			pe = ((icmPeInverter *)pe)->pe;
		}

		if ((pe->etype == icmSigPeMatrix
		  || pe->etype == icmSigPeClut
		  || pe->etype == icmSigPeMono)
		 && pe->attr.op != icmPeOp_NOP) { 
			break;		/* Assume end of input sequence */
		}

		if (pe->etype == icmSigPeCurve) {
			icmPeCurve *cv = (icmPeCurve *)pe;

			if (cv->count > maxres)
				maxres = cv->count;
			if (res != NULL && cv->count > res[0])
				res[0] = cv->count;

		} else if (pe->etype == icmSigPeCurveSet) {
			icmPeCurveSet *cs = (icmPeCurveSet *)pe;
			for (e = 0; e < cs->inputChan; e++) {
				       if (cs->pe[e]->etype == icmSigPeCurve) {
					icmPeCurve *cv = (icmPeCurve *)cs->pe[e];

					if (cv->count > maxres)
						maxres = cv->count;
					if (res != NULL && cv->count > res[e])
						res[e] = cv->count;
				}
			}
		}
	}

	return maxres;
}

/* Find the largest cLUT clutPoints[] value. */
/* Returns 0 if there is no cLut transform in the sequence. */ 
static unsigned int icmPeContainer_max_clut_res(icmPeContainer *p, int res[MAX_CHAN]) {
	unsigned int ix, e;
	unsigned int maxres = 0;

	if (res != NULL) {
		for (e = 0; e < MAX_CHAN; e++)
			res[e] = 0;
	}

	for (ix = 0; ix < p->count; ix++) {
		icmPe *pe = p->pe[ix];

		if (pe == NULL)
			continue;

		if (pe->isPeSeq) {
			icm_err(p->icp, ICM_ERR_SEARCH_PESEQ_INTERNAL, "icmPeContainer_max_clut_res found unexpected icmPeSeq inside icmPeContainer");
			return 0;

		/* Is this likely ? Does it make any sense if it existed ?? */
		} else if (pe->etype == icmSigPeInverter) {
			pe = ((icmPeInverter *)pe)->pe;
		}

		if (pe->etype == icmSigPeClut) {
			icmPeClut *cl = (icmPeClut *)pe;
			for (e = 0; e < cl->inputChan; e++) {
				if (cl->clutPoints[e] > maxres)
					maxres = cl->clutPoints[e];
				if (res != NULL && cl->clutPoints[e] > res[e]) {
					res[e] = cl->clutPoints[e];
				}
			}
		}
	}

	return maxres;
}

/* Find the largest per channel output lut resolution. */
/* res may be NULL */
/* Returns 0 if there is no per channel lut out the output sequence. */
static unsigned int icmPeContainer_max_out_res(icmPeContainer *p, int res[MAX_CHAN]) {
	int ix;
	unsigned int e;
	unsigned int maxres = 0;

	if (res != NULL) {
		for (e = 0; e < MAX_CHAN; e++)
			res[e] = 0;
	}

	/* Search backwards */
	for (ix = p->count-1; ix >= 0; ix--) {
		icmPe *pe = p->pe[ix];

		if (pe == NULL)
			continue;

		if (pe->isPeSeq) {
			icm_err(p->icp, ICM_ERR_SEARCH_PESEQ_INTERNAL, "icmPeContainer_max_in_res found unexpected icmPeSeq inside icmPeContainer");
			return 0;

		} else if (pe->etype == icmSigPeInverter) {
			pe = ((icmPeInverter *)pe)->pe;
		}

		if ((pe->etype == icmSigPeMatrix
		  || pe->etype == icmSigPeClut
		  || pe->etype == icmSigPeMono)
		 && pe->attr.op != icmPeOp_NOP) { 
			break;		/* Assume end of input sequence */
		}

		if (pe->etype == icmSigPeCurve) {
			icmPeCurve *cv = (icmPeCurve *)pe;

			if (cv->count > maxres)
				maxres = cv->count;
			if (res != NULL && cv->count > res[0])
				res[0] = cv->count;

		} else if (pe->etype == icmSigPeCurveSet) {
			icmPeCurveSet *cs = (icmPeCurveSet *)pe;
			for (e = 0; e < cs->inputChan; e++) {
				       if (cs->pe[e]->etype == icmSigPeCurve) {
					icmPeCurve *cv = (icmPeCurve *)cs->pe[e];

					if (cv->count > maxres)
						maxres = cv->count;
					if (res != NULL && cv->count > res[e])
						res[e] = cv->count;
				}
			}
		}
	}

	return maxres;
}

/* Determine if the per channel input or output is likely to be */
/* in a linear light space. We do this by seeing if the first */
/* non-per channel or format element is a matrix or matrix like */
/* element. */
/* dir should be 0 if for if we are interested in the input characteristic, */
/* and nz if we are interested in the output characteristic. */
/* (We assume that the icmPeContainer is flattened) */
static int icmPeContainer_linear_light_inout(icmPeContainer *p, int dir) {
	int ix, ix_s, ix_i, ix_e;
	unsigned int e;

	p->init(p);			/* Don't wonder if it is inited */

	if (dir) {
		ix_s = p->count-1;
		ix_i = -1;
		ix_e = -1;
	} else {	
		ix_s = 0;
		ix_i = 1;
		ix_e = p->count;
	}

	for (ix = ix_s; ix != ix_e; ix += ix_i) {
		icmPe *pe = p->pe[ix];

		if (pe == NULL)
			continue;

		if (pe->isPeSeq) {
			icm_err(p->icp, ICM_ERR_SEARCH_PESEQ_INTERNAL, "icmPeContainer_linear_light_inout found unexpected icmPeSeq inside icmPeContainer");
			return 0;

		} else if (pe->etype == icmSigPeInverter) {
			pe = ((icmPeInverter *)pe)->pe;
		}

		if (pe->attr.op == icmPeOp_complex) {		/* Hmm. Shouldn't happen if flattened ? */
			icm_err(p->icp, ICM_ERR_SEARCH_PESEQ_INTERNAL, "icmPeContainer_linear_light_inout found unexpected icmPeSeq op = icmPeOp_complex");
			return 0;
		}
			
		/* Ignore these ones... */
		if (pe->attr.op == icmPeOp_NOP
		 || pe->attr.op == icmPeOp_perch
		 || pe->attr.op == icmPeOp_fmt) {
			continue;
		}

		if (pe->etype == icmSigPeMatrix) {
			return 1;

		/* We're assuming that a cLut with a res. of 2 is really a type of matrix... */
		} else if (pe->etype == icmSigPeClut) {
			icmPeClut *cl = (icmPeClut *)p->pe[ix];
			for (e = 0; e < cl->inputChan; e++) {
				if (cl->clutPoints[e] > 2)
					return 0;
			}
			return 1;

		} else {
			icm_err(p->icp, ICM_ERR_SEARCH_PESEQ_INTERNAL, "icmPeContainer_linear_light_inout found unexpected icmPeSeq op = %s, etype = %s",icmPe_Op2str(pe->attr.op), icmPeSig2str(pe->etype));
			return 0;
		}
	}
	return 0;
}

/* Return a pointer to the last icmPeLut  */
/* and a Seq containing the transforms from the output of the Lut. */
/* Return NULL if no icmPeLut */
/* Delete both objects when done */
static icmPeClut *icmPeContainer_get_lut(
	icmPeContainer *p,			/* full transform sequence */
	icmPeContainer **ptail		/* If not NULL return tail process, NULL if none */
) {
	int ix, e;

	for (ix = p->count-1; ix >= 0; ix--) {
		icmPe *pe = p->pe[ix];

		if (pe == NULL)
			continue;

		if (pe->isPeSeq) {
			icm_err(p->icp, ICM_ERR_SEARCH_PESEQ_INTERNAL, "icmPeContainer_get_lut found unexpected icmPeSeq inside icmPeContainer");
			return NULL;
		}

		/* Ignore any inverter - cLut will never be inside an inverter... */
		if (pe->etype == icmSigPeClut) {
			icmPeClut *cl = (icmPeClut *)pe;
			icmPeContainer *tail = NULL;
			double sum;

			tail = new_icmPeContainer(p->icp, 0, 0);
			tail->append_pes(tail, (icmPeSeq *)p, ix+1, p->count);
			tail->init(tail);

			if (tail->attr.op == icmPeOp_NOP) {
				tail->del(tail);
				tail = NULL;
			}

			if (ptail != NULL)
				*ptail = tail;

			return cl->reference(cl);
		}
	}

	return NULL;
}

/* Return total ink limit and channel maximums. */
/* return -1.0 if this lu doesn't have an icmPeClut in it. */
static double icmPeContainer_get_tac(
	icmPeContainer *p,
	double *chmax,					/* device return channel maximums. May be NULL */
	void (*calfunc)(void *cntx, double *out, double *in),	/* Optional calibration func. */
	void *cntx
) {
	icmPeClut *cl;
	icmPeContainer *tail;		/* If not NULL return tail process, NULL if none */
	double sum;

	if ((cl = icmPeContainer_get_lut(p, &tail)) == NULL) {
		return 1;
	}

	sum = cl->get_tac(cl, chmax, (icmPe *)tail, calfunc, cntx); 

	cl->del(cl);
	if (tail != NULL)
		tail->del(tail);

	return sum;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Delete the container and all its contents */
static void icmPeContainer_del(icmPeContainer *p) {
	if (p->refcount > 0 && --p->refcount == 0) {
		unsigned int i;

		for (i = 0; i < p->count; i++)
			p->pe[i]->del(p->pe[i]);				/* Deletes if last reference */
		ICMFREEARRAY(p->icp, p->_count, p->pe)
		p->icp->al->free(p->icp->al, p);
	}
}

/* Create an empty object. Return null on error */
static icmPeContainer *new_icmPeContainer(
	icc *icp,
	int dinch,			/* Default number of input channels, 0 OK if will add non-NULL memb. Pe. */
	int doutch			/* Default number of output channels, 0 OK if will add non-NULL memb. Pe. */
) {
	ICM_PE_SEQ_NS_ALLOCINIT(icmPeContainer, icmSigPeContainer)

	p->inputChan = dinch;
	p->outputChan = doutch;
	ICM_PE_SETATTR(p->attr, 1, 0, 0, icmPeOp_NOP, 1, 1)

	p->append = icmPeContainer_append;
	p->prepend = icmPeContainer_prepend;
	p->insert = icmPeContainer_insert;
	p->replace = icmPeContainer_replace;
	p->remove = icmPeContainer_remove;
	p->append_pes = icmPeContainer_append_pes;

	p->max_in_res = icmPeContainer_max_in_res;
	p->max_clut_res = icmPeContainer_max_clut_res;
	p->max_out_res = icmPeContainer_max_out_res;
	p->linear_light_inout = icmPeContainer_linear_light_inout;
	p->get_lut = icmPeContainer_get_lut;
	p->get_tac = icmPeContainer_get_tac;

	return p;
}

