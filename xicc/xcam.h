
#ifndef _XCAM_H_

/* 
 * Abstract interface to color appearance model transforms.
 * 
 * This is to allow the rest of Argyll to use a default CAM.
 * 
 * Author:  Graeme W. Gill
 * Date:    25/7/2004
 * Version: 1.00
 *
 * Copyright 2004 Graeme W. Gill
 * Please refer to COPYRIGHT file for details.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* ---------------------------------- */

/* The range of CAMs supported */
typedef enum {
	cam_default    = 0,		/* Default CAM */
	cam_CIECAM97s3 = 1,		/* CIECAM97, version 3 */
	cam_CIECAM02   = 2		/* CIECAM02 */
} icxCAM;

/* The enumerated viewing conditions: */
typedef enum {
	vc_notset    = -1,
	vc_none      = 0,	/* Figure out from Lv and La */
	vc_dark      = 1,
	vc_dim       = 2,
	vc_average   = 3,
	vc_cut_sheet = 4	/* Transparencies on a Light Box */
} ViewingCondition;

/* Structure to convey viewing conditions */
typedef struct {
	ViewingCondition Ev;/* Enumerated Viewing Condition */
	double Wxyz[3];		/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
	double La;			/* Adapting/Surround Luminance cd/m^2 */
	double Yb;			/* Relative Luminance of Background to reference white */
	double Lv;			/* Luminance of white in the Image/Scene/Viewing field (cd/m^2) */
						/* Ignored if Ev is set to other than vc_none */
	double Yf;			/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
	double Yg;			/* Glare as a fraction of the adapting/surround (Y range 0.0 .. 1.0) */
	double Gxyz[3];		/* The Glare white coordinates (ie the Ambient color) */
						/* will be taken from Wxyz if Gxyz <= 0.0 */
	int hk;				/* Flag, NZ to use Helmholtz-Kohlraush effect */
	double hkscale;		/* [1.0] HK scaling factor */
	double mtaf;		/* [0.0] Mid tone partial adapation factor from Wxyz to Wxyz2 0.0 .. 1.0 */
	double Wxyz2[3];	/* Mid tone Adapted White XYZ (Y range 0.0 .. 1.0) */
	char *desc;			/* Possible description of this VC */
} icxViewCond;

struct _icxcam {
/* Public: */
	void (*del)(struct _icxcam *s);	/* We're done with it */

	/* Always returns 0 */
	int (*set_view_vc)(
		struct _icxcam *s,
		icxViewCond *vc
	);

	/* Always returns 0 */
	int (*set_view)(
		struct _icxcam *s,
		ViewingCondition Ev,	/* Enumerated Viewing Condition */
		double Wxyz[3],	/* Reference/Adapted White XYZ (Y scale 1.0) */
		double La,		/* Adapting/Surround Luminance cd/m^2 */
		double Yb,		/* Luminance of Background relative to reference white (range 0.0 .. 1.0) */
		double Lv,		/* Luminance of white in the Viewing/Scene/Image field (cd/m^2) */
						/* Ignored if Ev is set */
		double Yf,		/* Flare as a fraction of the reference white (range 0.0 .. 1.0) */
		double Yg,		/* Glare as a fraction of the adapting/surround (range 0.0 .. 1.0) */
		double Gxyz[3],	/* The Glare white coordinates (typically the Ambient color) */
		int hk,			/* Flag, NZ to use Helmholtz-Kohlraush effect */
		double hkscale,	/* HK effect scaling factor */

		double mtaf,	/* Mid tone partial adapation factor from Wxyz to Wxyz2, <= 0.0 if none */
		double Wxyz2[3] /* Mid tone Adapted White XYZ (Y range 0.0 .. 1.0) */
	);

	/* Conversions */
	int (*XYZ_to_cam)(struct _icxcam *s, double *out, double *in);
	int (*cam_to_XYZ)(struct _icxcam *s, double *out, double *in);

	/* Debug */
	void (*settrace)(struct _icxcam *s, int tracev);

	/* Dump the viewing conditions to stdout */
	void (*dump)(struct _icxcam *s);

/* Private: */
	icxCAM tag;			/* Type */
	void *p;			/* Pointer to implementation */
	double Wxyz[3];		/* Copy of Wxyz */

}; typedef struct _icxcam icxcam;

/* Create a new CAM conversion object */
icxcam *new_icxcam(icxCAM which);

/* Return the default CAM */
icxCAM icxcam_default();	

/* Return a string describing the given CAM */
char *icxcam_description(icxCAM ct); 

#define _XCAM_H_
#endif /* _XCAM_H_ */


