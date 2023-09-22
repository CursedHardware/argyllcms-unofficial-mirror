
/* 
 * Argyll Color Management System
 *
 * Plot the monochrome axis transfer curve of a given link.
 *
 * Author:  Graeme W. Gill
 * Date:    01/26/8
 * Version: 2.00
 *
 * Copyright 2001 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* TTBD:
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "copyright.h"
#include "aconfig.h"
#include "numsup.h"
#include "icc.h"
#include "plot.h"
#include "ui.h"

#define PRES 100

/* ---------------------------------------- */

void usage(void) {
	fprintf(stderr,"Plot monochrome axis of ICC link file, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"PCS->DEV ->link-> DEV->PCS\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: monoplot inprof linkprof outprof\n");
	fprintf(stderr," -v        verbose\n");
	exit(1);
}

int
main(
	int argc,
	char *argv[]
) {
	int fa, nfa;				/* argument we're looking at */
	int verb = 0;
	char in_name[100];
	char link_name[100];
	char out_name[100];
	icmFile *in_fp, *link_fp, *out_fp;
	icmErr err = { 0, { '\000'} };
	icc *in_icco, *link_icco, *out_icco;
	icmLuSpace *in_lu, *link_lu, *out_lu;
	int rv = 0;

	if (argc < 4)
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

			/* Verbosity */
			if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}
			else if (argv[fa][1] == '?')
				usage();
			else 
				usage();
		}
		else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(in_name,argv[fa++]);

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(link_name,argv[fa++]);

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(out_name,argv[fa++]);

	/* Open up the files for reading */
	if ((in_fp = new_icmFileStd_name(&err,in_name,"r")) == NULL)
		error ("Read: Can't open file '%s' (0x%x, '%s')",in_name,err.c,err.m);

	if ((in_icco = new_icc(&err)) == NULL)
		error ("Read: Creation of ICC object failed (0x%x, '%s')",err.c,err.m);

	if ((rv = in_icco->read(in_icco,in_fp,0)) != 0)
		error ("Read: %d, %s",rv,in_icco->e.m);


	if ((link_fp = new_icmFileStd_name(&err,link_name,"r")) == NULL)
		error ("Read: Can't open file '%s' (0x%x, '%s')",link_name,err.c,err.m);

	if ((link_icco = new_icc(&err)) == NULL)
		error ("Read: Creation of ICC object failed (0x%x, '%s')",err.c,err.m);

	if ((rv = link_icco->read(link_icco,link_fp,0)) != 0)
		error ("Read: %d, %s",rv,link_icco->e.m);


	if ((out_fp = new_icmFileStd_name(&err,out_name,"r")) == NULL)
		error ("Read: Can't open file '%s' (0x%x, '%s')",out_name,err.c,err.m);

	if ((out_icco = new_icc(&err)) == NULL)
		error ("Read: Creation of ICC object failed (0x%x, '%s')",err.c,err.m);

	if ((rv = out_icco->read(out_icco,out_fp,0)) != 0)
		error ("Read: %d, %s",rv,out_icco->e.m);


	/* Get a PCS to Device conversion object */
	if ((in_lu = (icmLuSpace *)in_icco->get_luobj(in_icco, icmBwd, icAbsoluteColorimetric, icSigLabData, icmLuOrdNorm)) == NULL) {
		if ((in_lu = (icmLuSpace *)in_icco->get_luobj(in_icco, icmBwd, icmDefaultIntent, icSigLabData, icmLuOrdNorm)) == NULL)
			error ("%d, %s",in_icco->e.c, in_icco->e.m);
	}

	/* Get a Device to Device conversion object */
	if ((link_lu = (icmLuSpace *)link_icco->get_luobj(link_icco, icmFwd, icmDefaultIntent, icSigLabData, icmLuOrdNorm)) == NULL)
		error ("%d, %s",link_icco->e.c, link_icco->e.m);

	/* Get a Device to PCS conversion object */
	if ((out_lu = (icmLuSpace *)out_icco->get_luobj(out_icco, icmFwd, icAbsoluteColorimetric, icSigLabData, icmLuOrdNorm)) == NULL) {
		if ((out_lu = (icmLuSpace *)out_icco->get_luobj(out_icco, icmFwd, icmDefaultIntent, icSigLabData, icmLuOrdNorm)) == NULL)
			error ("%d, %s",out_icco->e.c, out_icco->e.m);
	}

	{
		double xx[100], yy[PRES];
		int i;
		double tt[10];

		for (i = 0; i < PRES; i++) {

			tt[0] = 100.0 * i/(PRES-1.0);
			tt[1] = tt[2] = 0.0;

			xx[i] = tt[0];

			if (verb)
				printf(" %f %f %f -> ",tt[0],tt[1],tt[2]);

			/* PCS to input device space */
			if ((rv = in_lu->lookup_fwd(in_lu, tt, tt)) & icmPe_lurv_err)
				error ("%d, %s",in_icco->e.c,in_icco->e.m);

			/* input device space to output device space */
			if ((rv = link_lu->lookup_fwd(link_lu, tt, tt)) & icmPe_lurv_err)
				error ("%d, %s",link_icco->e.c,link_icco->e.m);

			/* output device space to PCS */
			if ((rv = out_lu->lookup_fwd(out_lu, tt, tt)) & icmPe_lurv_err)
				error ("%d, %s",out_icco->e.c,out_icco->e.m);

			yy[i] = tt[0];

			if (verb)
				printf("%f %f %f\n",tt[0],tt[1],tt[2]);
		}

		if (do_plot(xx,yy,NULL,NULL,PRES) < 0)
			error("do_plot returned -1!\n");

	}

	/* Done with lookup objects */
	in_lu->del(in_lu);
	link_lu->del(link_lu);
	out_lu->del(out_lu);

	in_icco->del(in_icco);
	in_fp->del(in_fp);
	link_icco->del(link_icco);
	link_fp->del(link_fp);
	out_icco->del(out_icco);
	out_fp->del(out_fp);

	return 0;
}

