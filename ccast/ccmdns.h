#ifndef CCMDNST_H

/* 
 * Argyll Color Correction System
 * ChromCast mDNS support
 *
 * Author: Graeme W. Gill
 * Date:   28/8/2014
 *
 * Copyright 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License2.txt file for licencing details.
 *
 */

/*
 * This class provides simple access to the Google ChromeCast
 * for the purposes of generating Video Test patches.
 */

/* Chrome cast type */
typedef enum {
	cctyp_unkn = 0,			/* Unknown */
	cctyp_1,				/* Chromecast 1 or General Video */
	cctyp_2,				/* Chromecast 2 (Can't be detected though - id will be cctyp_1) */
	cctyp_Audio,			/* Chromecast Audio or General Audio */
	cctyp_Ultra,			/* Chromecast Ultra */
	cctyp_Other				/* Non Video/Audio output device */
} cctype;

/* Use cctype2str() to dump type */

/* A record of a Chromecast that may be accessed */
struct _ccast_id {
	char *name;		/* Chromecast friendly name */
	char *ip;		/* IP address as string (ie. "10.0.0.128") */
	cctype typ;		/* Chromecast type (If known) */
}; typedef struct _ccast_id ccast_id;

/* Get a list of Video out capable Chromecasts. Return NULL on error */
/* Last pointer in array is NULL */ 
/* Takes 0.5 second to return */
ccast_id **get_ccids(
void
);

/* Free up what get_ccids returned */
void free_ccids(ccast_id **ids);

void ccast_id_copy(ccast_id *dst, ccast_id *src);
ccast_id *ccast_id_clone(ccast_id *src);
void ccast_id_del(ccast_id *id);

#define CCMDNST_H
#endif /* CCMDNST_H */
