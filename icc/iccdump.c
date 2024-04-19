
/* 
 * International Color Consortium Format Library (icclib)
 * File dump utility.
 *
 * Author:  Graeme W. Gill
 * Date:    1999/11/29
 * Version: 2.15
 *
 * Copyright 1997 - 2012 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 */

/*
 * TTBD:
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include "icc.h"

void error(char *fmt, ...), warning(char *fmt, ...);

void usage(void) {
	fprintf(stderr,"Dump an ICC file in human readable form, V%s\n",ICCLIB_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: iccdump [-v level] [-t tagname] [-s] infile\n");
	fprintf(stderr," -v level                 Verbose level 1-3 (default 2)\n");
	fprintf(stderr," -t tag                   Dump this tag only (can be used multiple times)\n");
	fprintf(stderr," -s                       Search for embedded profile\n");
	fprintf(stderr," -i                       Check V4 ID value\n");
	fprintf(stderr," -w                       Warn of ICC format problems to stdout\n");
	fprintf(stderr," -W                       Warn of ICC format problems to stderr\n");
	fprintf(stderr," -S                       Be strict about ICC format\n");
	exit(1);
}

FILE *warnout;

/* Warning callback function */
void dump_warning(icc *p, int err, const char *format, va_list vp) {
	fprintf(warnout, "Warning (0x%x): ",err);
	vfprintf(warnout, format, vp);
	fprintf(warnout, "\n");
}

#define MXTGNMS 30

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	char in_name[500];
	int ntag_names = 0;
	char tag_names[MXTGNMS][5];
	int verb = 2;		/* Default = moderate (some from individual tags) */
	int search = 0;
	int chid = 0;		/* Check V4 ID */
	int strict = 0;		/* Be strict about ICC format */
	int warnings = 0;	/* Warn of format problems */
	int ecount = 1;		/* Embedded count */
	int offset = 0;		/* Offset to read profile from */
	int found;
	icmErr err = { 0, { '\000' }};
	icmFile *fp, *op;
	icc *icco;
	int rv = 0;
	

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
				if (na == NULL) usage();
				verb = atoi(na);
			}
			/* Tag name */
			else if (argv[fa][1] == 't') {
				fa = nfa;
				if (na == NULL) usage();
				if (ntag_names >= MXTGNMS)
					usage();
				strncpy(tag_names[ntag_names],na,4);
				tag_names[ntag_names++][4] = '\000';
			}
			/* Search */
			else if (argv[fa][1] == 's') {
				search = 1;
			}
			/* Check ID */
			else if (argv[fa][1] == 'i') {
				chid = 1;
			}
			/* Warnings */
			else if (argv[fa][1] == 'w') {
				warnings = 1;
				warnout = stdout;
			}
			else if (argv[fa][1] == 'W') {
				warnings = 1;
				warnout = stderr;
			}
			/* Strict */
			else if (argv[fa][1] == 'S') {
				strict = 1;
			}


			else 
				usage();
		}
		else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(in_name,argv[fa]);


	/* Open up the file for reading */
	if ((fp = new_icmFileStd_name(&err, in_name,"r")) == NULL)
		error ("Can't open file '%s' failed with 0x%x, '%s'",in_name, err.c, err.m);

	if ((icco = new_icc(&err)) == NULL)
		error ("Creation of ICC object failed with 0x%x, '%s'", err.c, err.m);

	if (strict)
		icco->unset_cflag(icco, icmCFlagRdFormatWarn
		                      | icmCFlagRdVersionWarn
		                      | icmCFlagRdAllowUnknown
		                      | icmCFlagAllowExtensions);

	/* Install warning message handler */
	if (warnings)
		icco->warning = dump_warning; 

	/* open output stream */
	if ((op = new_icmFileStd_fp(&err, stdout)) == NULL)
		error ("Can't open stdout stream, failed with 0x%x, '%s'",err.c, err.m);


	do {
		found = 0;

		/* Dumb search for magic number */
		if (search) {
			int fc = 0;
			char c;
		
			if (fp->seek(fp, offset) != 0)
				break;

			while(found == 0) {
				if (fp->read(fp, &c, 1, 1) != 1) {
					break;
				}
				offset++;
				
				switch (fc) {
					case 0:
						if (c == 'a')
							fc++;
						else
							fc = 0;
						break;
					case 1:
						if (c == 'c')
							fc++;
						else
							fc = 0;
						break;
					case 2:
						if (c == 's')
							fc++;
						else
							fc = 0;
						break;
					case 3:
						if (c == 'p') {
							found = 1;
							offset -= 40;
						}
						else
							fc = 0;
						break;
				}
			}
		}

		if (search == 0 || found != 0) {
			ecount++;
	
			if (search)
				printf("Embedded profile found at file offset %d (0x%x)\n",offset,offset);

			if ((rv = icco->read(icco,fp,offset)) != 0)
				error ("%d, %s",rv,icco->e.m);
		
			if (icco->header->cmmId == icmstr2tag("argl"))
				icco->allowclutPoints256 = 1;

			if (ntag_names > 0) {
				int i;
				for (i = 0; i < ntag_names; i++) {

					icTagSignature sig = icmstr2tag(tag_names[i]);
			
					/* Try and locate that particular tag */
					if ((rv = icco->find_tag(icco,sig)) != 0 && rv != 1) {
						warning ("icc->find_tag() can't find tag '%s' in file", icmtag2str(sig));
					} else {
						icmBase *ob;
			
						if (rv == 1)
							warning ("icc->find_tag() tag '%s' found but unknown type", tag_names[i]);
						if ((ob = icco->read_tag_any(icco, sig)) == NULL) {
							warning("Failed to read tag '%s': %d, %s",tag_names[i], icco->e.c,icco->e.m);
						} else {
							ob->dump(ob, op, verb-1);
						}
					}
				}
			} else {
				icco->dump(icco, op, verb);
				if (chid && icco->header->vers.majv >= 4) {
					unsigned char id[16];
					rv = icco->check_id(icco, id);
					if (rv == 0) {
						icmHeader *p = icco->header;
						printf("Id check succeeds\n");
						op->printf(op," ID is        = %02x%02x%02x%02x%02x%02x%02x%02x"
						                              "%02x%02x%02x%02x%02x%02x%02x%02x\n",
							p->id[0], p->id[1], p->id[2], p->id[3],
							p->id[4], p->id[5], p->id[6], p->id[7],
							p->id[8], p->id[9], p->id[10], p->id[11],
							p->id[12], p->id[13], p->id[14], p->id[15]);
						op->printf(op," ID should be = %02x%02x%02x%02x%02x%02x%02x%02x"
						                               "%02x%02x%02x%02x%02x%02x%02x%02x\n",
							id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7],
							id[8], id[9], id[10], id[11], id[12], id[13], id[14], id[15]);
					} else if (rv == 1) {
						printf("Id can't be checked because it's not set\n");
						op->printf(op," ID should be = %02x%02x%02x%02x%02x%02x%02x%02x"
						                               "%02x%02x%02x%02x%02x%02x%02x%02x\n",
							id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7],
							id[8], id[9], id[10], id[11], id[12], id[13], id[14], id[15]);
					} else if (rv == 2) {
						icmHeader *p = icco->header;
						printf("Id check fails:\n");
						op->printf(op," ID is        = %02x%02x%02x%02x%02x%02x%02x%02x"
						                              "%02x%02x%02x%02x%02x%02x%02x%02x\n",
							p->id[0], p->id[1], p->id[2], p->id[3],
							p->id[4], p->id[5], p->id[6], p->id[7],
							p->id[8], p->id[9], p->id[10], p->id[11],
							p->id[12], p->id[13], p->id[14], p->id[15]);
						op->printf(op," ID should be = %02x%02x%02x%02x%02x%02x%02x%02x"
						                               "%02x%02x%02x%02x%02x%02x%02x%02x\n",
							id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7],
							id[8], id[9], id[10], id[11], id[12], id[13], id[14], id[15]);
					} else
						warning("Failed to do ID check %d, %s",icco->e.c,icco->e.m);
				}
			}
			offset += 128;
		}
	} while (found != 0);

	icco->del(icco);
	op->del(op);
	fp->del(fp);

	return 0;
}


/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"iccdump: Error - ");
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

	fprintf(stderr,"iccdump: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}


