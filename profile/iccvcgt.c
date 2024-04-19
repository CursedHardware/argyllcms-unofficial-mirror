
/* 
 * Argyll Color Management System
 * ICC Profile extract or insert 'vcgt' display calibration tag.
 *
 * Author:  Graeme W. Gill
 * Date:    2021/6/7
 * Version: 1.0
 *
 * Copyright 1997 - 2021 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Based on iccrw.c 
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
#include "numlib.h"
#include "xicc.h"

void error(char *fmt, ...), warning(char *fmt, ...);

void usage(char *diag, ...) {
	fprintf(stderr,"Extract or insert 'vcgt' calibration tag from/into an ICC profile, V%s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	if (diag != NULL) {
		va_list args;
		fprintf(stderr,"  Diagnostic: ");
		va_start(args, diag);
		vfprintf(stderr, diag, args);
		va_end(args);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"usage: -x profile%s curves.cal\n",ICC_FILE_EXT);
	fprintf(stderr,"usage: -i inprofile%s curves.cal outprofile%s\n",ICC_FILE_EXT, ICC_FILE_EXT);
	fprintf(stderr," -v              Verbose mode\n");
	fprintf(stderr," -x              Extract 'vcgt'\n");
	fprintf(stderr," -i              Insert 'vcgt'\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa,mfa;				/* argument we're looking at */
	char inname[MAXNAMEL+1] = "";
	char calname[MAXNAMEL+1] = "";
	char outname[MAXNAMEL+1] = "";
	icmFile *rd_fp, *wr_fp;
	icmErr err = { 0, { '\000'} };
	icc *icco;
	xcal *cal = NULL;			/* Calibration extracted or inserted */
	int verb = 0;
	int extract = 0;
	int insert = 0;
	int rv = 0;


	error_program = argv[0];

	if (argc < 3)
		usage("Less than three arguments");

	/* Process the arguments */
	mfa = 2;        /* Minimum final arguments */
	for (fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1+mfa) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage(NULL);

			if (argv[fa][1] == 'x') {
				extract = 1;
				mfa = 2;
			}

			if (argv[fa][1] == 'i') {
				insert = 1;
				mfa = 3;
			}

		} else
			break;
	}


	if ((extract && insert)
	 || (!extract && !insert)) {
		usage("Conflicting options");
	}

	if (fa >= argc || argv[fa][0] == '-') usage("No source ICC profile");
	strcpy(inname,argv[fa++]);

	if (fa >= argc || argv[fa][0] == '-') usage("No calibration curve filename");
	strcpy(calname,argv[fa++]);

	if (insert) {
		if (fa >= argc || argv[fa][0] == '-') usage("No destination ICC profile");
		strcpy(outname,argv[fa++]);
	}

	/* Open up the incoming profile for reading */
	if ((rd_fp = new_icmFileStd_name(&err,inname,"r")) == NULL)
		error ("Can't open file '%s' (0x%x, '%s')",inname,err.c,err.m);

	if ((icco = new_icc(&err)) == NULL)
		error ("Creation of ICC object for '%s' failed (0x%x, '%s')",inname,err.c,err.m);

	/* Read header etc. */
	if ((rv = icco->read(icco,rd_fp,0)) != 0)
		error ("%d, %s",rv,icco->e.m);

	if (icco->header->deviceClass != icSigDisplayClass)
		error("'%s' must be a Display profile",inname);


	if (insert) {
		int c, i, j;
		icmVideoCardGamma *wo;
		int ncal;

		/* Read every tag so that we can copy the profile */
		if (icco->read_all_tags(icco) != 0)
			error("Unable to read all tags from '%s': %d, %s",inname,icco->e.c,icco->e.m);

		/* Open up and read the calibration file */
		if ((cal = new_xcal()) == NULL)
			error("new_xcal failed");
		if ((cal->read(cal, calname)) != 0)
			error("%s",cal->e.m);

		if (cal->devchan != 3)
			error("'%s' doesn't contain 3 channels",calname);

		/* Delete the current video card gamma tag */
		if (icco->find_tag(icco, icSigVideoCardGammaTag) == 0)
			if (icco->delete_tag(icco, icSigVideoCardGammaTag) != 0)
				error("Unable to delete videocardgamma tag: %d, %s",icco->e.c,icco->e.m);

		/* Decide on the vcgt resolution */
		ncal = cal->cals[0]->g.res[0];
		if (ncal < 256)
			ncal = 256;

		/* Add new video card gamma table */
		wo = (icmVideoCardGamma *)icco->add_tag(icco, icSigVideoCardGammaTag, icSigVideoCardGammaType);
		if (wo == NULL)
			error ("Unable to add VideoCardGamma tag");

		wo->tagType = icVideoCardGammaTable;
		wo->u.table.channels = 3;						/* rgb */
		wo->u.table.entryCount = ncal;
		wo->u.table.entrySize = 2;						/* 16 bits */
		wo->allocate(wo);
		for (j = 0; j < 3; j++) {
			for (i = 0; i < ncal; i++) {
				double cc, vv = i/(ncal - 1.0);
				cc = cal->interp_ch(cal, j, vv);
				if (cc < 0.0)
					cc = 0.0;
				else if (cc > 1.0)
					cc = 1.0;
				wo->u.table.data[j][i] = cc;
			}
		}

		/* Open up the output profile for writing */
		if ((wr_fp = new_icmFileStd_name(&err,outname,"w")) == NULL)
			error ("Can't open file '%s' (0x%x, '%s')",outname,err.c,err.m);
	
		if ((rv = icco->write(icco, wr_fp, 0)) != 0)
			error ("Write file: %d, %s",rv,icco->e.m);
	
		wr_fp->del(wr_fp);

		cal->del(cal);
	
	} else {	/* Extract */
		
		/* Read the video card gamma tag from the ICC profile */
		if ((cal = new_xcal()) == NULL)
			error("new_xcal failed");
		if ((cal->read_icc(cal, icco)) != 0)
			error("%s",cal->e.m);

		/* Write out the calibration file */
		if (cal->write(cal, calname))
			error("Writing '%s' failed",calname);

		cal->del(cal);
	}

	icco->del(icco);
	rd_fp->del(rd_fp);

	return 0;
}


