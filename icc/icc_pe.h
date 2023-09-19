
/* 
 * International Color Consortium Format Library (icclib)
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

/* icmPe transform declarations */
/* This is #included in icc.h */

/* Sig (etype) of processing elements */
/* (Update icmPeSig2str() too) */
typedef enum {
	icmSigPeNone			= icmMakeTag(0,0,0,0),		 	/* Not an icmPe */
	icmSigPeCurveSet 		= icmMakeTag('P','e','c','s'),	/* A set of N x 1d Curves */
	icmSigPeCurve			= icmMakeTag('P','e','c','u'),	/* A linear/gamma/table curve */
	icmSigPeMatrix			= icmMakeTag('P','e','m','a'),	/* An N x M + F matrix */
	icmSigPeClut			= icmMakeTag('P','e','c','l'),	/* An N x M cLUT */

	icmSigPeLut816			= icmMakeTag('P','e','l','8'),

	icmSigPeContainer		= icmMakeTag('P','e','c','r'), 	/* A sequence of other icmPe's */
	icmSigPeInverter		= icmMakeTag('P','e','i','v'), 	/* An inverter */
	icmSigPeMono			= icmMakeTag('P','e','m','o'), 	/* Mono conversion part of ShaperMono */

	icmSigPeShaperMatrix	= icmMakeTag('P','e','s','m'), 	/* A shaper/matrix sequence */
	icmSigPeShaperMono	    = icmMakeTag('P','e','s','o'), 	/* A shaper/mono sequence */

	/* Format conversions */
	icmSigPeXYZ2Lab			= icmMakeTag('P','e','x','l'), 
	icmSigPeAbs2Rel			= icmMakeTag('P','e','a','r'), 
	icmSigPeXYZ2XYZ8		= icmMakeTag('P','e','x','1'), 
	icmSigPeXYZ2XYZ16		= icmMakeTag('P','e','x','2'), 
	icmSigPeLab2Lab8		= icmMakeTag('P','e','l','1'),
	icmSigPeLab2LabV2		= icmMakeTag('P','e','l','2'),
	icmSigPeGeneric2Norm	= icmMakeTag('P','e','G','e'),		/* Generic normalization */
	icmSigPeGridAlign	    = icmMakeTag('P','e','G','a'),		/* Grid align */
	icmSigPeNOP				= icmMakeTag('P','e','N','O'),		/* A NOP */

	icmPeMaxEnum            = icMaxTagVal 
} icmPeSignature;

/* See icmPeSig2str() for text */

/* Lookup return values */
typedef enum {
    icmPe_lurv_OK           = 0x00,	/* Lookup is OK */
    icmPe_lurv_clip         = 0x01,	/* Clipping warning */
    icmPe_lurv_bwclp        = 0x02,	/* lookup_bwd on clipping PeCurveFmlaSeg */
    icmPe_lurv_num          = 0x04,	/* Numerical limit reached warning */
    icmPe_lurv_imp          = 0x08,	/* Not implemented error */
    icmPe_lurv_cfg          = 0x10,	/* Configuration error */

    icmPe_lurv_err          = 0x18	/* Error mask */
} icmPe_lurv;

char *icmPe_lurv2str(icmPe_lurv err);

/* PE operation classification */
typedef enum {				/* Dominant operation type */
	icmPeOp_NOP		= 0x0,	/* No operation */
	icmPeOp_perch	= 0x1,	/* A per-channel operation */
	icmPeOp_matrix	= 0x2,	/* A matrix interchannel operation (maybe abs<->rel or Mono) */
	icmPeOp_cLUT	= 0x3,	/* A cLUT interchannel operation */
	icmPeOp_fmt		= 0x4,	/* PCS format transform (i.e. XYZ <->Lab)  */
	icmPeOp_complex	= 0x5	/* A complex set of interchannel operations */
} icmPeOp;

char *icmPe_Op2str(icmPeOp op);

/* icmPe attributes. */
/* [ Note that not-limited range is assumed to be >= 0.0 in many situations... ] */
typedef struct {
	char comp;			/* nz if Compound element contains other Pe's (for trace) */
	char inv;			/* nz if element was created with inverted function (for trace) */
	char norm;			/* nz if normalization type operation */			
	icmPeOp op;			/* Operation type */
	char fwd;			/* nz if fwd transform is implemented */
	char bwd;			/* nz if bwd transform is implemented */
} icmPeAttr;

#define ICM_PE_SETATTR(ATTR, COMP, INV, NORM, OP, FWD, BWD) \
	ATTR.comp = COMP;							\
	ATTR.inv = INV;								\
	ATTR.norm = NORM;							\
	ATTR.op = OP;								\
	ATTR.fwd = FWD;								\
	ATTR.bwd = BWD;								\

/* Processing Element base class definition */ 
/* This inherets from icmBase */
#define ICM_PE_MEMBERS(ECLASS)																\
	ICM_BASE_MEMBERS(ECLASS)																\
    unsigned int	inputChan;      /* No. of input channels */								\
    unsigned int   outputChan;		/* No. of output channels */							\
    icmPeAttr      attr;			/* attributes of transforms */							\
	int            isPeSeq;			/* nz if is superclass of icmPeSeq */					\
																							\
	int            trace;			/* Trace level, 1 = top, > 1 = recursion level */		\
																							\
	/* Init anything that depends on the processing (i.e. attr, inverse comp etc.) */		\
	/* Return nz on error */																\
	int (*init) (ECLASS *p);																\
																							\
	/* Translate a value through the Pe, return any warning flags */						\
	/* (Where are warning flags defined ? Clip ?, F.P. error ?) */							\
	icmPe_lurv (*lookup_fwd) (ECLASS *p, double *out, double *in);	/* Forwards */			\
	icmPe_lurv (*lookup_bwd) (ECLASS *p, double *out, double *in);	/* Backwards */			\

/* Processing Element Initialisation */
#define ICM_PE_INIT(ECLASS, ESIG)														\
	p->etype = ESIG;																	\
	p->init = (int (*)(ECLASS *))icmPeDummy_init;										\

/* Processing Element allocation and initialisation */
/* Returns NULL on error */
#define ICM_PE_ALLOCINIT(ECLASS, ESIG, TSIG)											\
	ICM_BASE_ALLOCINIT(ECLASS, TSIG)													\
	ICM_PE_INIT(ECLASS, ESIG)															\

/* Non-serialisable init */
#define ICM_PE_NS_ALLOCINIT(ECLASS, ESIG)												\
	ICM_BASE_NS_ALLOCINIT(ECLASS)														\
	ICM_PE_INIT(ECLASS, ESIG)															\

/* A Processing Element */
struct _icmPe {
	ICM_PE_MEMBERS(struct _icmPe)
}; typedef struct _icmPe icmPe;

/* Structure for managing Pe's */
struct _icmPePrivStr {
	int				  dup;			/* Duplicate Pe flag */
	icmPositionNumber pn;			/* Offset and size of sub-tag */
}; typedef struct _icmPePrivStr icmPePrivStr;

/* ----------------------- */
/* A sequence of icmPe's */

/* Processing Element Sequence base class definition */ 
/* This inherets from icmPe */
#define ICM_PE_SEQ_MEMBERS(ECLASS)															\
	ICM_PE_MEMBERS(ECLASS)																	\
																							\
	/* Private: */																			\
	unsigned int       _count;		/* Count currently allocated */							\
	unsigned int        nncount;	/* Count of non-NULL pe's */							\
																							\
	/* Public: */																			\
	unsigned int	   count;	/* Count of processing elements */							\
    icmPe	  		  **pe;  	/* Array of [count] processing elements */					\


/* Processing Element Sequenece allocation and initialisation. */
/* (It's assumed that the parent class provides all methods) */
/* Returns NULL on error */
#define ICM_PE_SEQ_ALLOCINIT(ECLASS, ESIG, TSIG)											\
	ICM_PE_ALLOCINIT(ECLASS, ESIG, TSIG)													\
	p->isPeSeq = 1;																			\
	p->init = (int (*)(ECLASS *))icmPeSeq_init;												\
	p->lookup_fwd = (icmPe_lurv (*)(ECLASS *, double *, double *))icmPeSeq_lookup_fwd;		\
	p->lookup_bwd = (icmPe_lurv (*)(ECLASS *, double *, double *))icmPeSeq_lookup_bwd;		\

/* Non-serialisable init */
#define ICM_PE_SEQ_NS_ALLOCINIT(ECLASS, ESIG)												\
	ICM_PE_NS_ALLOCINIT(ECLASS, ESIG)														\
	p->isPeSeq = 1;																			\
	p->init = (int (*)(ECLASS *))icmPeSeq_init;												\
	p->lookup_fwd = (icmPe_lurv (*)(ECLASS *, double *, double *))icmPeSeq_lookup_fwd;		\
	p->lookup_bwd = (icmPe_lurv (*)(ECLASS *, double *, double *))icmPeSeq_lookup_bwd;		\

/* A Processing Element Sequence */
struct _icmPeSeq {
	ICM_PE_SEQ_MEMBERS(struct _icmPeSeq)
}; typedef struct _icmPeSeq icmPeSeq;

/* ================================================================================== */

/* ----------------------- */
/* A set of N x 1d Curves */

/* etype icmSigPeCurveSet */
/* ttype icmSig816Curves */

struct _icmPeCurveSet {
	ICM_PE_MEMBERS(struct _icmPeCurveSet)

	/* Private: */
	icmPePrivStr    pei[MAX_CHAN];		/* Management info. */
	unsigned int bpv;					/* Bytes per value (Lut1 only) */

	/* Public: */
	icmPe           *pe[MAX_CHAN];		/* 1D curves */

}; typedef struct _icmPeCurveSet icmPeCurveSet;


/* A fake internal Sig 'l1vs' for icmLut8 and icmLut16 curves */ 
#define icmSig816Curves ((icTagTypeSignature)icmMakeTag('l','1','v','s'))

/* ----------------------- */
/* A linear/gamma/table (sample) curve */

/* etype icmSigPeCurve */
/* ttype icSigCurveType */
/* ttype icmSig816Curve */

/* icmPeCurve reverse lookup information */
typedef struct {
	int inited;				/* Flag */
	double rmin, rmax;		/* Range of reverse grid */
	double qscale;			/* Quantising scale factor */
	int rsize;				/* Number of reverse lists */
	unsigned int **rlists;	/* Array of list of fwd values that may contain output value */
							/* Offset 0 = allocated size */
							/* Offset 1 = next free index */
							/* Offset 2 = first fwd index */
	unsigned int count;		/* Copy of forward table size */
	double       *data;		/* Copy of forward table data */
} icmRevTable;

typedef enum {
    icmCurveUndef           = -1, /* Undefined curve */
    icmCurveLin             = 0,  /* Linear transfer curve */
    icmCurveGamma           = 1,  /* Gamma power transfer curve */
    icmCurveSpec            = 2   /* Specified curve */
} icmCurveType;

struct _icmPeCurve {
	ICM_PE_MEMBERS(struct _icmPeCurve)

	/* Private: */
	int inited;					/* Flag */
	unsigned int   _count;		/* Size currently allocated */
	unsigned int bpv;			/* Bytes per value (Lut1 only) */
	icmRevTable  rt;			/* Reverse table information */

	/* Public: */
    icmCurveType   ctype;		/* Type of curve */
	unsigned int	count;		/* Allocated and used size of the array */
    double         *data;  		/* Curve data scaled to range 0.0 - 1.0 */
								/* or data[0] = gamma value */
								/* or count == 0 for linear. */

}; typedef struct _icmPeCurve icmPeCurve;

typedef icmPeCurve icmCurve;

/* A fake internal Sig 'l1cv' for a icmLut8 and icmLut16 curve */ 
#define icmSig816Curve ((icTagTypeSignature)icmMakeTag('l','1','c','v'))

/* ----------------------- */
/* An N x M + F matrix */

/* etype icmSigPeMatrix */
/* ttype icmSig816Matrix */

struct _icmPeMatrix {
	ICM_PE_MEMBERS(struct _icmPeMatrix)

	/* Private: */
	int       inited;		/* flag - if initiliase (imx, flags) */
	int       ivalid;		/* flag - if imx is valid */
	int       mx_unity;		/* flag - if mx[] is unity */
	int       ct_zero;		/* flag - if ct[] is zero */

	double imx[MAX_CHAN][MAX_CHAN];	/* [inn][outn] Inverse matrix values */

	/* Public: */

	double mx[MAX_CHAN][MAX_CHAN];	/* [outn][inn] Matrix values */
	double ct[MAX_CHAN];			/* [outn] Constant values added after mx[][] multiply */

}; typedef struct _icmPeMatrix icmPeMatrix;


/* A fake internal Sig 'l1mx' for icmLut8 and icmLut16 Matrix */ 
#define icmSig816Matrix ((icTagTypeSignature)icmMakeTag('l','1','m','x'))

/* ----------------------- */
/* An N x M cLUT */

/* etype icmSigPeClut */
/* ttype icmSig816CLUT */

struct _icmLu4Base;
struct _icmLu4Space;

struct _icmPeClut {
	ICM_PE_MEMBERS(struct _icmPeClut)

	/* Private: */
    unsigned int _clutsize;     	/* Current clut size in doubles */
	int       inited;				/* flag - if initialised (dinc[]) */
	unsigned int dinc[MAX_CHAN];	/* [inn] grid resolution values */
	int dcube[1 << MAX_CHAN];		/* Hyper cube offsets (in doubles) */
	int       use_sx;				/* flag - use simplex interpolation */

	/* Public: */
	unsigned int bpv;			/* Bytes per value */
	unsigned int clutPoints[MAX_CHAN];	/* [inn] grid resolution values */
								/* (Must be all the same for icmLut) */
	double *clutTable;	 		/* clut values organized as: */
								/* [inputChan 0: 0..clutPoints[0]-1].. */
								/* [inputChan inn-1: 0..clutPoints[inn-1]-1] */
								/*                   [outputChan: 0..outn-1] */

	/* return the minimum and maximum values of the given channel in the clut */
	void (*min_max) (struct _icmPeClut *p, double *minv, double *maxv, int chan);

	/* Determine appropriate clut lookup algorithm */
	void (*choose_alg) (struct _icmPeClut *p, struct _icmLu4Space *lu);

	/* Return total ink limit and channel maximums. */
	double (*get_tac) (
		struct _icmPeClut *p,
		double *chmax,					/* device return channel sums. May be NULL */
		icmPe *tail,					/* lu tail transform from clut to device output */
		void (*calfunc)(void *cntx, double *out, double *in),	/* Optional calibration func. */
		void *cntx
	);

	/* ~8 missing API's:

		tune_value()	cLut single value fine tune)

	*/

}; typedef struct _icmPeClut icmPeClut;


/* A fake internal Sig 'l1L1' for icmLut8 and icmLut16  */ 
#define icmSig816CLUT ((icTagTypeSignature)icmMakeTag('l','1','L','U'))

/* ----------------------- */

/* Create an empty icmPe object. Return null on error */
/* Set ttype to the serialisation type of this Pe or its */
/* parent ttype if it doesn't use a sig in its serialisation. */
/* Use ttype = icmSigUnknownType if not a serialisable object. */
static icmPe *new_icmPe(struct _icc *icp, icTagTypeSignature ttype, icmPeSignature pesig);

/* =========================================================================== */
/* Client code populates data[i].pe etc using icc->new_pe().  */

/* ------------------------------------------------------------ */
/* Lut8Type and Lut16Type */

/* etype icmSigPeLut816 */
/* ttype icSigLut8Type */
/* ttype icSigLut16Type */

/* Indexes into underlying Pe's */
typedef enum {
    icmLut816_ix_Matrix	= 0,
    icmLut816_ix_in		= 1,
    icmLut816_ix_CLUT	= 2,
    icmLut816_ix_out	= 3
} icmLut1_ix;

struct _icmLut1 {
	ICM_PE_SEQ_MEMBERS(struct _icmLut1)

	/* Private: */
	unsigned int bpv;				/* Bytes per value for curves and cLUT */

	/* Public: */
	/* unsigned int	inputChan;      	No. of input channels (in icpPeSeq) */
	/* unsigned int outputChan;			No. of output channels (in icpPeSeq) */
	unsigned int	inputEnt;       /* Num of in-table entries (must be 256 for Lut8) */
	unsigned int	clutPoints;     /* Num of grid points */
	unsigned int	outputEnt;      /* Num of out-table entries (must be 256 for Lut8) */

	/* Processing Element aliases: */
	icmPeMatrix  *pe_mx;
	icmPeCurve  **pe_ic;
	icmPeClut    *pe_cl;
	icmPeCurve  **pe_oc;

}; typedef struct _icmLut1 icmLut1;

typedef struct _icmLut1 icmLut8;
typedef struct _icmLut1 icmLut16;

/* ------------------------------------------------------------ */
/* A Pe that encodes an RGB shaper-matrix transform. */
/* This transform is always XYZ PCS. */
/* (This is not a serialisable tagtype) */

struct _icmShaperMatrix {
	ICM_PE_SEQ_MEMBERS(struct _icmShaperMatrix)
	
	/* icSigCurveType */
	icmBase    *redCurve, *greenCurve, *blueCurve;
	icmXYZArray *redColrnt, *greenColrnt, *blueColrnt;

}; typedef struct _icmShaperMatrix icmShaperMatrix;

/* Create an empty object. Return null on error. */
/* Doesn't set icc error if tags not present. */
static icmPeSeq *new_icmShaperMatrix(
	struct _icc *icp,
	int invert			/* If nz, setup inverse of shape-matrix */
);

/* ------------------------------------------------------------ */
/* A Pe that encodes an Monochrome shaper-mono transform. */
/* (This is not a serialisable tagtype) */

struct _icmShaperMono {
	ICM_PE_SEQ_MEMBERS(struct _icmShaperMono)
	
	/* icSigCurveType */
	icmBase    *grayCurve;

}; typedef struct _icmShaperMono icmShaperMono;

/* Create an empty object. Return null on error. */
/* Doesn't set icc error if tags not present. */
static icmPeSeq *new_icmShaperMono(
	struct _icc *icp,
	int invert			/* If nz, setup inverse of shape-mono */
);

/* ------------------------------------------------------------ */
/* A Pe that implements the mono<->PCS conversion. */
/* (This is not a serialisable tagtype) */

struct _icmPeMono {
	ICM_PE_MEMBERS(struct _icmPeMono)
}; typedef struct _icmPeMono icmPeMono;

/* Create the Pe */
static icmPe *new_icmPeMono(
	struct _icc *icp
);

/* =========================================================== */
/* A Pe that inverts another Pe. */
/* (This is not a serialisable tagtype) */

struct _icmPeInverter {
	ICM_PE_MEMBERS(struct _icmPeInverter)
	
	icmPe *pe;		/* Pe that's being inverted */

}; typedef struct _icmPeInverter icmPeInverter;

/* Make Pe appear to be its inverse. */
static icmPe *new_icmPeInverter(
	struct _icc *icp,
	icmPe *pe
);

/* ------------------------------------------------------------ */
/* XYZ <-> Lab Pe (This is not a serialisable tagtype) */

struct _icmPeXYZ2Lab {
	ICM_PE_MEMBERS(struct _icmPeXYZ2Lab)
	struct _icmLu4Space *lu;
	
}; typedef struct _icmPeXYZ2Lab icmPeXYZ2Lab;

static icmPe *new_icmPeXYZ2Lab(
	struct _icc *icp,
	struct _icmLu4Space *lu,		/* pcswht */
	int invert				/* Lab->XYZ */
);

/* ------------------------------------------------------------ */
/* XYZ abs <-> XYZ rel Pe (This is not a serialisable tagtype) */

struct _icmPeAbs2Rel {
	ICM_PE_MEMBERS(struct _icmPeAbs2Rel)
	struct _icmLu4Space *lu;
}; typedef struct _icmPeAbs2Rel icmPeAbs2Rel;

static icmPe *new_icmPeAbs2Rel(
	struct _icc *icp,
	struct _icmLu4Space *lu,		/* wp, toAbs, fromAbs */
	int invert				/* Lab->XYZ */
);

/* ------------------------------------------------------------ */
/* XYZ <-> XYZ8 Pe (This is not a serialisable tagtype) */

struct _icmPeXYZ2XYZ8 {
	ICM_PE_MEMBERS(struct _icmPeXYZ2XYZ8)
}; typedef struct _icmPeXYZ2XYZ8 icmPeXYZ2XYZ8;

static icmPe *new_icmPeXYZ2XYZ8(
	struct _icc *icp,
	int invert
);

/* ------------------------------------------------------------ */
/* XYZ <-> XYZ16 Pe (This is not a serialisable tagtype) */

struct _icmPeXYZ2XYZ16 {
	ICM_PE_MEMBERS(struct _icmPeXYZ2XYZ16)
}; typedef struct _icmPeXYZ2XYZ16 icmPeXYZ2XYZ16;

static icmPe *new_icmPeXYZ2XYZ16(
	struct _icc *icp,
	int invert
);

/* ------------------------------------------------------------ */
/* Lab <-> Lab8 Pe (This is not a serialisable tagtype) */

struct _icmPeLab2Lab8 {
	ICM_PE_MEMBERS(struct _icmPeLab2Lab8)
}; typedef struct _icmPeLab2Lab8 icmPeLab2Lab8;

static icmPe *new_icmPeLab2Lab8(
	struct _icc *icp,
	int invert
);

/* ------------------------------------------------------------ */
/* Lab <-> LabV2 Pe (This is not a serialisable tagtype) */

struct _icmPeLab2LabV2 {
	ICM_PE_MEMBERS(struct _icmPeLab2LabV2)
}; typedef struct _icmPeLab2LabV2 icmPeLab2LabV2;

static icmPe *new_icmPeLab2LabV2(
	struct _icc *icp,
	int invert
);

/* ------------------------------------------------------------ */
/* Generic <-> Normalized Pe (This is not a serialisable tagtype) */
/* min/max difference is sanity limited to be at least 1e-4. */
/* NOTE that these conversions don't clip. */

struct _icmPeGeneric2Norm {
	ICM_PE_MEMBERS(struct _icmPeGeneric2Norm)
	double min[MAX_CHAN];		/* Full range min & max */
	double max[MAX_CHAN];
	double nmin[MAX_CHAN];		/* Normalize range min & max */
	double nmax[MAX_CHAN];
	char ident[50];
}; typedef struct _icmPeGeneric2Norm icmPeGeneric2Norm;

static icmPe *new_icmPeFullyGeneric2Norm(
	struct _icc *icp,
	unsigned int nchan,
	double *min, double *max,		/* Per channel range to be normalised */
	double *nmin, double *nmax,		/* Per channel range to normalize to (NULL = 0..1) */
	char *ident,					/* Encoding identifier */
	int invert
);

static icmPe *new_icmPeGeneric2Norm(
	struct _icc *icp,
	unsigned int nchan,
	double *min, double *max,		/* Per channel range to be normalised */
	char *ident,					/* Encoding identifier */
	int invert
);

/* ------------------------------------------------------------ */
/* A point to grid alignment transform. */ 
/* This creates a per channel transform that piecwize bends */
/* the input so as to land the given point on a grid point. */
/* The values are assumed in the range 0..1 */

struct _icmPeGridAlign {
	ICM_PE_MEMBERS(struct _icmPeGridAlign)
	double src[MAX_CHAN];
	double dst[MAX_CHAN];
	double fwdlow[MAX_CHAN];
	double fwdhigh[MAX_CHAN];
	double bwdlow[MAX_CHAN];
	double bwdlowoff[MAX_CHAN];
	double bwdhigh[MAX_CHAN];
	double bwdhighoff[MAX_CHAN];

}; typedef struct _icmPeGridAlign icmPeGridAlign;

static icmPe *new_icmPeGridAlign(
	struct _icc *icp,
	unsigned int nchan,
	double *point,					/* Point to aim for, must be 0..1 */
	unsigned int *clutPoints,		/* Grid resolution for each channel */
	int invert
);

/* ------------------------------------------------------------ */
/* A NOP (This is not a serialisable tagtype) */

struct _icmPeNOP {
	ICM_PE_MEMBERS(struct _icmPeNOP)
}; typedef struct _icmPeNOP icmPeNOP;

static icmPe *new_icmPeNOP(
	struct _icc *icp,
	unsigned int nchan
);

/* ========================================================= */
/* A Processing Element Container (This is not a serialisable tagtype) */

struct _icmPeContainer {
	ICM_PE_SEQ_MEMBERS(struct _icmPeContainer)

	/* Added Pe's will have their reference counts incremented, */
	/* so if we adding a new_PeXXX, we need to delete it after adding. */
	/* Removed Pe's will have their reference counts decremented. */
	/* All these will return an error code. */

	/* Add a Pe to the end of the sequence */
	int (*append) (struct _icmPeContainer *p, icmPe *pe);

	/* Add a Pe to the start of the sequence */
	int (*prepend) (struct _icmPeContainer *p, icmPe *pe);

	/* Insert a Pe before the given index */
	int (*insert) (struct _icmPeContainer *p, unsigned int ix, icmPe *pe);

	/* Replace the Pe at the given index */
	int (*replace) (struct _icmPeContainer *p, unsigned int ix, icmPe *pe);

	/* Remove the Pe at the given index */
	int (*remove) (struct _icmPeContainer *p, unsigned int ix);

	/* Append Pe's from another icmPeContainer, */
	/* starting at six and ending before eix. */
	/* Any empty or attr == NOP are skipped. */
	/* This will flatten any sub-icmPeSeq, resulting in a sequence */
	/* of direct Pe's or direct Pe's inside an Inverter. */
	int (*append_pes) (struct _icmPeContainer *p, struct _icmPeSeq *src, int six, int eix);

	/* Various icmPeContainer/icmPeSeq characteristic analysis methods: */
	/* These all assume that the icmPeContainer is in a flattened state. */

	/* Find the largest per channel input lut resolution. */
	/* res may be NULL */
	/* Returns 0 if there is no per channel lut in the input sequence. */ 
	unsigned int (*max_in_res)(struct _icmPeContainer *p, int res[MAX_CHAN]);

	/* Find the largest cLUT clutPoints[] value. */
	/* res may be NULL */
	/* Returns 0 if there is no cLut transform in the sequence. */ 
	unsigned int (*max_clut_res)(struct _icmPeContainer *p, int res[MAX_CHAN]);

	/* Find the largest per channel output lut resolution. */
	/* res may be NULL */
	/* Returns 0 if there is no per channel lut in the output sequence. */ 
	unsigned int (*max_out_res)(struct _icmPeContainer *p, int res[MAX_CHAN]);

	/* Determine if the per channel input or output is likely to be */
	/* in a linear light space. We do this by seeing if the first */
	/* non-per channel or format element is a matrix or matrix like element. */
	int (*linear_light_inout)(struct _icmPeContainer *p, int dir);

	/* Return a pointer to the last icmPeLut and a Seq containing */
	/* the transforms from the output of the Lut */
	/* Return NULL if no icmPeLut */
	/* Delete both objects when done */
	icmPeClut *(*get_lut)(
		struct _icmPeContainer *p,
		struct _icmPeContainer **ptail		/* If not NULL return tail process, NULL if none */
	);

	/* Return total ink limit and channel maximums. */
	/* return -1.0 if this lu doesn't have an icmPeClut in it */
	double (*get_tac) (
		struct _icmPeContainer *p,
		double *chmax,					/* device return channel maximums. May be NULL */
		void (*calfunc)(void *cntx, double *out, double *in),	/* Optional calibration func. */
		void *cntx
	);

}; typedef struct _icmPeContainer icmPeContainer;

/* Create an empty object. Return null on error */
static icmPeContainer *new_icmPeContainer(
	struct _icc *icp,
	int dinch,			/* Default number of input channels, 0 OK if will add non-NULL memb. Pe. */
	int doutch			/* Default number of output channels, 0 OK if will add non-NULL memb. Pe. */
);


