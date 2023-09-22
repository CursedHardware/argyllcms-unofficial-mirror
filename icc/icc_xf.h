/* ================================================= */
/* icmPe transform declarations */
/* This is #included in icc.h */

typedef enum {			/* Lab version encoding */
    icmEncV2 = 0,		/* ICC V2 */
	icmEncProf = 2		/* Same as icc header version */
} icmEncLabVer;

typedef enum {			/* Lab version representation */
    icmEncNbit  = 0,	/* 32 bit float or 16.16 */
    icmEnc8bit  = 1,	/* 8 bit */
    icmEnc16bit = 2		/* 16 bit */
} icmEncBytes;

/* Given a full range colorspace sig, return a default value ranges. */
/* Return NZ if range is not 0.0 - 1.0 */
int icmGetDefaultFullRange(
	struct _icc *icp,
	double *min, double *max,		/* Return range */
	icColorSpaceSignature sig		/* Full range  colorspace sig */
);

/* A structure for returning information about a colorspace */
typedef struct {
	icColorSpaceSignature sig;		/* Signature */
	int nch;						/* Number of channels */
	double min[MAX_CHAN];			/* Nominal range of each channel */
	double max[MAX_CHAN];
} icmCSInfo;

/* Lookup class members common to named and non-named color types */
#define LU4_ICM_BASE_MEMBERS(TCLASS)														\
																							\
	/* Private: */																			\
	icmLu4Type      ttype;		    	/* The type of object */							\
	struct _icc    *icp;				/* Pointer to ICC we're a part of */				\
	icTagSignature  sourceTag;	    	/* The transform source tag */						\
	icRenderingIntent intent;			/* Effective (externaly visible) intent */			\
	icmLookupFunc function;				/* Functionality being used */						\
	icmLookupOrder order;        		/* Conversion representation search Order */ 		\
	icmPeOp op;							/* lookup op */										\
	int can_bwd;						/* nz if lookup_bwd will work */					\
	icmXYZNumber pcswht;				/* PCS White, i.e. D50 (abs. XYZ) */				\
	icmXYZNumber whitePoint, blackPoint;/* Profile White and black point (abs. XYZ) */		\
	int blackisassumed;					/* nz if black point tag is missing from profile */ \
	double toAbs[3][3];					/* Matrix to convert from relative to absolute */ 	\
	double fromAbs[3][3];				/* Matrix to convert from absolute to relative */ 	\
    icmCSInfo ini;						/* Native Clr space of input info. */				\
    icmCSInfo outi;						/* Native Clr space of output info. */				\
	icmCSInfo pcsi;						/* Native PCS info. */								\
    icmCSInfo e_ini;					/* Effective Clr space of input info. */			\
    icmCSInfo e_outi;					/* Effective Clr space of output info. */			\
	icmCSInfo e_pcsi;					/* Effective PCS info. */							\
	/* Hmm. Do we need in/out/e_ abs intent flags ? */										\
																							\
	/* Public: */																			\
																							\
	void       (*del)(TCLASS *p);															\
																							\
	/* Initialise the white and black points from the ICC tags, */							\
	/* and the corresponding absolute<->relative conversion matrices. */					\
	/* (Called on init, and after white & black set during creation.) */					\
	/* Return nz on error */																\
																							\
	int        (*init_wh_bk)(TCLASS *p);													\
																							\
	           /* External effecive colorspaces and other lookup information */				\
	void       (*spaces) (TCLASS *p, icmCSInfo *ini, icmCSInfo *outi, icmCSInfo *pcsi, 		\
	                             icTagSignature *srctag, icRenderingIntent *intt,			\
	                             icmLookupFunc *fnc, icmLookupOrder *ord,					\
	                             icmPeOp *op, int *can_bwd);								\
																							\
	           /* Internal native colorspaces: */	     									\
	void       (*native_spaces) (TCLASS *p, icmCSInfo *ini, icmCSInfo *outi, icmCSInfo *pcsi);\
	                                                                                    	\
	           /* Relative to Absolute for this WP in XYZ */   								\
	void       (*XYZ_Rel2Abs)(TCLASS *p, double *xyzout, double *xyzin);					\
																							\
	           /* Absolute to Relative for this WP in XYZ */   								\
	void       (*XYZ_Abs2Rel)(TCLASS *p, double *xyzout, double *xyzin);					\


/* Base lookup object */
struct _icmLu4Base {
	LU4_ICM_BASE_MEMBERS(struct _icmLu4Base)
}; // typedef struct _icmLu4Base icmLu4Base;


/* Colorspace lookup class (not named color) */
struct _icmLu4Space {
	LU4_ICM_BASE_MEMBERS(struct _icmLu4Space)

	/* Private: */

	/* Note that these icmPeContainers will all be flat - */
	/* i.e. there will be no sub-icmPeSeq's, only icmPeInverter's */

	/* single stage transform */
	icmPeContainer *lookup;			/* Overall transform */

	/* 3 stage transforms */
	icmPeContainer *input;			/* Per channel pre-transforms */
	icmPeContainer *core3;			/* Core transform that may change no. channels */
	icmPeContainer *output;			/* Per channel post-transforms */

	/* 5 stage transforms */
	icmPeContainer *input_fmt;			/* PCS and abs. conversion */
	icmPeContainer *input_pch;			/* Per channel pre-transforms */
	icmPeContainer *core5;				/* Core transform that may change no. channels */
	icmPeContainer *output_pch;			/* Per channel post-transforms */
	icmPeContainer *output_fmt;			/* PCS and abs. conversion */
	/* Public: */

	/* Get the LU PCS white, white and black points in absolute XYZ space. */
	/* PCS will be the profile header PCS white point. */
	/* Return nz if the black point is being assumed to be 0,0,0 rather */
	/* than being from the tag. */
	int (*wh_bk_points)(struct _icmLu4Space *p, double *pcs, double *wht, double *blk);

	/* Get the LU PCS white, white and black points in LU PCS space, converted to XYZ. */
	/* (ie. white and black will be relative if LU is relative intent etc.) */
	/* Return nz if the black point is being assumed to be 0,0,0 rather */
	/* than being from the tag. */
	int (*lu_wh_bk_points)(struct _icmLu4Space *p, double *pcs, double *wht, double *blk);

	/* Overall transforms */
	icmPe_lurv (*lookup_fwd) (struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*lookup_bwd) (struct _icmLu4Space *p, double *out, double *in);

	/* 3 stage transforms are fmt&per-channel + core + per-channel&fmt: */

	/* 3 stage components of fwd lookup */
	icmPe_lurv (*input_fwd)	(struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*core3_fwd)	(struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*output_fwd)	(struct _icmLu4Space *p, double *out, double *in);

	/* 3 stage cComponents of bwd lookup */
	icmPe_lurv (*output_bwd)	(struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*core3_bwd)	(struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*input_bwd)	(struct _icmLu4Space *p, double *out, double *in);


	/* 5 stage transforms are fmt + per-channel + core + per-channel + fmt: */

	/* 5 stage sub-components of input_fwd and output_fwd lookup */
	icmPe_lurv (*input_fmt_fwd)	 (struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*input_pch_fwd)	 (struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*core5_fwd)	     (struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*output_pch_fwd) (struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*output_fmt_fwd) (struct _icmLu4Space *p, double *out, double *in);

	/* 5 stage sub-components of input_bwd and output_bwd lookup */
	icmPe_lurv (*output_fmt_bwd) (struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*output_pch_bwd) (struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*core5_bwd)	     (struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*input_pch_bwd)	 (struct _icmLu4Space *p, double *out, double *in);
	icmPe_lurv (*input_fmt_bwd)	 (struct _icmLu4Space *p, double *out, double *in);

	/* Lookup information */

	/* Find the largest per channel input lut resolution. */
	/* res may be NULL */
	/* Returns 0 if there is no per channel lut in the input sequence. */ 
	unsigned int (*max_in_res)(struct _icmLu4Space *p, int res[MAX_CHAN]);

	/* Find the largest cLUT clutPoints[] value. */
	/* res may be NULL */
	/* Returns 0 if there is no cLut transform in the sequence. */ 
	unsigned int (*max_clut_res)(struct _icmLu4Space *p, int res[MAX_CHAN]);

	/* Find the largest per channel output lut resolution. */
	/* res may be NULL */
	/* Returns 0 if there is no per channel lut in the output sequence. */ 
	unsigned int (*max_out_res)(struct _icmLu4Space *p, int res[MAX_CHAN]);

	/* Determine if the per channel input or output is likely to be */
	/* in a linear light space. We do this by seeing if the first */
	/* non-per channel or format element is a matrix or matrix like */
	/* element, and if the PCS is XYZ. This is not totally reliable, but */
	/* should work for simple matrix device profiles. */
	/* Return nz if true */
	/* dir should be 0 if for if we are interested in the input characteristic, */
	/* and nz if we are interested in the output characteristic. */
	/* (We assume that the icmPeContainer is flattened) */
	int (*linear_light_inout)(struct _icmLu4Space *p, int dir);

	/* Return a pointer to the last icmPeLut in the lookup_fwd and */
	/* a Seq containing the transforms from the output of the Lut */
	/* Return NULL if no icmPeLut */
	/* Delete both objects when done */
	icmPeClut *(*get_lut)(
		struct _icmLu4Space *p,
		icmPeContainer **ptail		/* If not NULL return tail process, NULL if none */
	);

	/* Return total ink limit and channel maximums. */
	/* return -1.0 if this lu doesn't have an icmPeClut in it. */
	double (*get_tac)(
		struct _icmLu4Space *p,
		double *chmax,					/* device return channel maximums. May be NULL */
		void (*calfunc)(void *cntx, double *out, double *in),	/* Optional calibration func. */
		void *cntx
	);

}; // typedef struct _icmLu4Space icmLu4Space;

/* Fake up alg from icmLu4 attributes */
icmLuAlgType icmSynthAlgType(icmLuBase *p);
