
/* 
 * International Color Consortium Format Library (icclib)
 * Profile color lookup utility.
 *
 * Author:  Graeme W. Gill
 * Date:    1999/11/29
 * Version: 2.15
 *
 * Copyright 1998 - 2012 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 */

/* TTBD:
 *
 */

/*

	This file is intended to exercise the ability
	of the icc library to translate colors through a profile.
	It also serves as a concise example of how to do this.

 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "icc.h"

void error(char *fmt, ...), warning(char *fmt, ...);

void usage(void) {
	fprintf(stderr,"Translate colors through an ICC profile, V%s\n",ICCLIB_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: icclu [-v level] [-f func] [-i intent] [-o order] profile\n");
	fprintf(stderr," -v level      Verbosity level 0 - 2 (default = 1)\n");
	fprintf(stderr," -f function   f = forward, b = backwards, g = gamut, p = preview\n");
	fprintf(stderr," -i intent     p = perceptual, r = relative colorimetric,\n");
	fprintf(stderr,"               s = saturation, a = absolute\n");
//	fprintf(stderr,"               P = absolute perceptual, S = absolute saturation\n");
	fprintf(stderr," -p oride      x = XYZ_PCS, l = Lab_PCS, y = Yxy,\n");
	fprintf(stderr," -o order      n = normal (priority: lut > matrix > monochrome)\n");
	fprintf(stderr,"               r = reverse (priority: monochrome > matrix > lut)\n");
	fprintf(stderr," -s scale      Scale device range 0.0 - scale rather than 0.0 - 1.0\n");
	fprintf(stderr," -T            Trace each step of conversions\n");
//	fprintf(stderr," -D            Dump the Pe's of all conversions\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"    The colors to be translated should be fed into standard input,\n");
	fprintf(stderr,"    one input color per line, white space separated.\n");
	fprintf(stderr,"    A line starting with a # will be ignored.\n");
	fprintf(stderr,"    A line not starting with a number will terminate the program.\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	icmErr e = { 0, { '\000'} };
	char prof_name[500];
	icmFile *fp;
	icc *icco;
	int verb = 1;
	double scale = 0.0;		/* Device value scale factor */
	int rv = 0;
	int repYxy = 0;			/* Report Yxy */
	char buf[200];
	double oin[MAX_CHAN], in[MAX_CHAN], out[MAX_CHAN];

	int trace = 0;				/* Trace the conversions */
	icmLuSpace *luo;
	icmCSInfo ini, outi, pcsi;			/* Type of input and output spaces */
	icmCSInfo n_ini, n_outi, n_pcsi;	/* Native type of input and output spaces */
	icTagSignature srctag;				/* Source tag of lookup */
	icRenderingIntent rintent;			/* Rendering intent */
	icmLookupFunc lufunc;				/* Lookup function */
	icmLookupOrder luord;				/* Lookup order */
	icmPeOp luop;						/* Dominant processing element operation */
	int can_bwd;						/* Whether lookup is invertable */

#define ins ini.sig				/* Compatibility until code is switched over */
#define outs outi.sig
#define inn ini.nch
#define outn outi.nch


	/* Lookup parameters */
	icmLookupFunc     func   = icmFwd;				/* Default */
	icRenderingIntent intent = icmDefaultIntent;	/* Default */
	icColorSpaceSignature pcsor = icmSigDefaultData;	/* Default */
	icmLookupOrder    order  = icmLuOrdNorm;		/* Default */
	
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
			else if (argv[fa][1] == 'v') {
				fa = nfa;
				if (na == NULL)
					verb = 2;
				else {
					if (na[0] == '0')
						verb = 0;
					else if (na[0] == '1')
						verb = 1;
					else if (na[0] == '2')
						verb = 2;
					else if (na[0] == '3')
						verb = 3;
					else
						usage();
				}
			}

			/* function */
			else if (argv[fa][1] == 'f') {
				fa = nfa;
				if (na == NULL) usage();
    			switch (na[0]) {
					case 'f':
					case 'F':
						func = icmFwd;
						break;
					case 'b':
					case 'B':
						func = icmBwd;
						break;
					case 'g':
					case 'G':
						func = icmGamut;
						break;
					case 'p':
					case 'P':
						func = icmPreview;
						break;
					default:
						usage();
				}
			}

			/* Intent */
			else if (argv[fa][1] == 'i') {
				fa = nfa;
				if (na == NULL) usage();
    			switch (na[0]) {
					case 'p':
						intent = icPerceptual;
						break;
					case 'r':
						intent = icRelativeColorimetric;
						break;
					case 's':
						intent = icSaturation;
						break;
					case 'a':
						intent = icAbsoluteColorimetric;
						break;
					/* Special function icclib intents */
					case 'P':
						intent = icmAbsolutePerceptual;
						break;
					case 'S':
						intent = icmAbsoluteSaturation;
						break;
					default:
						usage();
				}
			}

			/* Search order */
			else if (argv[fa][1] == 'o') {
				fa = nfa;
				if (na == NULL) usage();
    			switch (na[0]) {
					case 'n':
						order = icmLuOrdNorm;
						break;
					case 'r':
						order = icmLuOrdRev;
						break;
					default:
						usage();
				}
			}

			/* PCS override */
			else if (argv[fa][1] == 'p') {
				fa = nfa;
				if (na == NULL) usage();
    			switch (na[0]) {
					case 'x':
					case 'X':
						pcsor = icSigXYZData;
						repYxy = 0;
						break;
					case 'l':
					case 'L':
						pcsor = icSigLabData;
						repYxy = 0;
						break;
					case 'y':
					case 'Y':
						pcsor = icSigXYZData;
						repYxy = 1;
						break;
					default:
						usage();
				}
			}

			/* Device scale */
			else if (argv[fa][1] == 's') {
				fa = nfa;
				if (na == NULL) usage();
				scale = atof(na);
				if (scale <= 0.0) usage();
			}

			else if (argv[fa][1] == 'T') {
				trace = 1;
			}

			else 
				usage();
		} else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(prof_name,argv[fa]);

	icm_err_clear_e(&e);

	/* Open up the profile for reading */
	if ((fp = new_icmFileStd_name(&e, prof_name,"r")) == NULL)
		error ("Can't open file '%s', failed with 0x%x, '%s'",prof_name, e.c, e.m);

	if ((icco = new_icc(&e)) == NULL)
		error ("Creation of ICC object failed with 0x%x, '%s'", e.c, e.m);

	if ((rv = icco->read(icco,fp,0)) != 0)
		error ("%d, %s",rv,icco->e.m);

	if (icco->header->cmmId == icmstr2tag("argl"))
		icco->allowclutPoints256 = 1;

	/* Dump profile header if verbose */
	if (verb > 1) {
		icmFile *op;
		if ((op = new_icmFileStd_fp(&e, stdout)) == NULL)
			error ("Can't open stdout, failed with 0x%x, '%s'", e.c, e.m);
		icco->header->dump(icco->header, op, 1);
		op->del(op);
	}

	/* Compute a TAC */
	if (verb > 1) {
		double tac;
		double chmax[MAX_CHAN];
		
		tac = icco->get_tac(icco, chmax, NULL, NULL);
	
		if (tac <= -1.0)
			printf("No TAC\n\n");
		else
			printf("TAC = %f, chan max = %s\n\n",tac,
			    icmPdv(icmCSSig2nchan(icco->header->colorSpace), chmax));
	}

	/* Get a conversion object */
	if ((luo = (icmLuSpace *)icco->get_luobj(icco, func, intent, pcsor, order)) == NULL)
		error ("%d, %s",icco->e.c, icco->e.m);

	if (trace) {
		luo->lookup->trace = 1;
	}

	/* Get details of conversion (Arguments may be NULL if info not needed) */
	luo->spaces(luo, &ini, &outi, &pcsi, &srctag, &rintent, &lufunc, &luord, &luop, &can_bwd);
	luo->native_spaces(luo, &n_ini, &n_outi, &n_pcsi);

	if (verb > 1) {
		double pcs[3], white[3], black[3];
		int asblk;

		printf("Lookup details:\n");
		printf(" ini  = %s\n",icmCSInfo2str(&ini));
		printf(" outi = %s\n",icmCSInfo2str(&outi));
		printf(" pcsi = %s\n",icmCSInfo2str(&pcsi));
		printf(" n_ini  = %s\n",icmCSInfo2str(&n_ini));
		printf(" n_outi = %s\n",icmCSInfo2str(&n_outi));
		printf(" n_pcsi = %s\n",icmCSInfo2str(&n_pcsi));
		printf(" src_tag = %s\n",icm2str(icmTagSig_alt, srctag));
		printf(" intent = %s\n",icm2str(icmRenderingIntent, rintent));
		printf(" lu func = %s\n",icm2str(icmTransformLookupFunc, lufunc));
		printf(" lu order = %s\n",icm2str(icmTransformLookupOrder, luord));
		printf(" lu attr.op = %s\n",icm2str(icmProcessingElementOp, luop));
		printf(" can bwd ? = %s\n",can_bwd ? "yes" : "no");

		asblk = luo->wh_bk_points(luo, pcs, white, black);
		printf("Abs. XYZ PCS %s, white %s, black %s%s\n",
		           icmPdv(3, pcs), icmPdv(3, white), icmPdv(3, black), asblk ? " (assumed)" : "");

		asblk = luo->lu_wh_bk_points(luo, pcs, white, black);
		printf("Rel. XYZ PCS %s, white %s, black %s%s\n",
		           icmPdv(3, pcs), icmPdv(3, white), icmPdv(3, black), asblk ? " (assumed)" : "");
	}

	/* Show other derived attributes */
	if (verb > 2) {
		int maxres;
		int chmax[MAX_CHAN];
		
		maxres = luo->max_in_res(luo, chmax);
		printf("Input curve maxres = %d, chan max = %s\n\n",maxres,
		    icmPiv(luo->lookup->inputChan, chmax));

		maxres = luo->max_clut_res(luo, chmax);
		printf("cLut maxres = %d, chan max = %s\n\n",maxres,
		    icmPiv(luo->lookup->inputChan, chmax));

		maxres = luo->max_out_res(luo, chmax);
		printf("Ouput curve maxres = %d, chan max = %s\n\n",maxres,
		    icmPiv(luo->lookup->outputChan, chmax));
	}

	if (repYxy) {	/* report Yxy rather than XYZ */
		if (ini.sig == icSigXYZData)
			ini.sig = icSigYxyData; 
		if (outi.sig == icSigXYZData)
			outi.sig = icSigYxyData; 
	}
		

	/* Process colors to translate */
	for (;;) {
		int i,j;
		char *bp, *nbp;

		/* Init default input values */
		for (i = 0; i < MAX_CHAN; i++) {
			in[i] = oin[i] = 0.0;
		}

		/* Read in the next line */
		if (fgets(buf, 200, stdin) == NULL)
			break;
		if (buf[0] == '#') {
			if (verb > 0)
				fprintf(stdout,"%s\n",buf);
			continue;
		}
		/* For each input number */
		for (nbp = buf, i = 0; i < MAX_CHAN; i++) {
			bp = nbp;
			in[i] = oin[i] = strtod(bp, &nbp);
			if (nbp == bp)
				break;			/* Failed */
		}
		if (i == 0)
			break;

		/* If device data and scale */
		if(scale > 0.0
    	 && ins != icSigXYZData
		 && ins != icSigLabData
		 && ins != icSigLuvData
		 && ins != icSigYCbCrData
		 && ins != icSigYxyData
		 && ins != icSigHsvData
		 && ins != icSigHlsData) {
			for (i = 0; i < MAX_CHAN; i++) {
				in[i] /= scale;
			}
		}

		if (repYxy && ins == icSigYxyData) {
			icmYxy2XYZ(in, in);
		}

		/* Do conversion */
		if ((rv = luo->lookup_fwd(luo, out, in)) & icmPe_lurv_err)
			error("%d, %s",icco->e.c,icco->e.m);

		/* Output the results */
		if (verb > 0) {
			for (j = 0; j < inn; j++) {
				if (j > 0)
					fprintf(stdout," %f",oin[j]);
				else
					fprintf(stdout,"%f",oin[j]);
			}
			printf(" [%s] -> %s -> ", icm2str(icmColorSpaceSig, ins),
		                          icm2str(icmTransformSourceTag, srctag));
		}
		
		if (repYxy && outs == icSigYxyData) {
			icmXYZ2Yxy(out, out);
		}

		/* If device data and scale */
		if(scale > 0.0
    	 && outs != icSigXYZData
		 && outs != icSigLabData
		 && outs != icSigLuvData
		 && outs != icSigYCbCrData
		 && outs != icSigYxyData
		 && outs != icSigHsvData
		 && outs != icSigHlsData) {
			for (i = 0; i < MAX_CHAN; i++) {
				out[i] *= scale;
			}
		}

		for (j = 0; j < outn; j++) {
			if (j > 0)
				fprintf(stdout," %f",out[j]);
			else
				fprintf(stdout,"%f",out[j]);
		}
		if (verb > 0)
			printf(" [%s]", icm2str(icmColorSpaceSig, outs));

		if (rv != icmPe_lurv_OK) 
			printf(" (%s)",icmPe_lurv2str(rv));

		fprintf(stdout,"\n");

	}

	/* Done with lookup object */
	luo->del(luo);

	icco->del(icco);

	fp->del(fp);

	return 0;
}


/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"icclu: Error - ");
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

	fprintf(stderr,"icclu: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}
