/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for Soundtracker 2.6/Ice Tracker modules */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


struct ice_ins {
    char name[22];		/* Instrument name */
    uint16 len;			/* Sample length / 2 */
    uint8 finetune;		/* Finetune */
    uint8 volume;		/* Volume (0-63) */
    uint16 loop_start;		/* Sample loop start in file */
    uint16 loop_size;		/* Loop size / 2 */
} PACKED;

struct ice_header {
    char title[20];
    struct ice_ins ins[31];	/* Instruments */
    uint8 len;			/* Size of the pattern list */
    uint8 trk;			/* Number of tracks */
    uint8 ord[128][4];
    uint8 magic[4];		/* 'MTN\0', 'IT10' */
} PACKED;


int ice_load (FILE *f)
{
    int i, j;
    struct xxm_event *event;
    struct ice_header ih;
    uint8 ev[4];
    char *tracker;

    LOAD_INIT ();

    fread (&ih, 1, sizeof (ih), f);

    if (ih.magic[0] == 'I' && ih.magic[1] == 'T' && ih.magic[2] == '1' &&
	ih.magic[3] == '0')
	tracker = "Ice Tracker";
    else if (ih.magic[0] == 'M' && ih.magic[1] == 'T' && ih.magic[2] == 'N' &&
	ih.magic[3] == 0)
	tracker = "Soundtracker";
    else
	return -1;

    xxh->ins = 31;
    xxh->smp = xxh->ins;
    xxh->pat = ih.len;
    xxh->len = ih.len;
    xxh->trk = ih.trk;

    strncpy (xmp_ctl->name, (char *) ih.title, 20);
    strcpy (xmp_ctl->type, "MnemoTroN Soundtracker 2.6");
    strcpy (tracker_name, tracker);
    MODULE_INFO ();

    for (i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (ih.ins[i].len);
	B_ENDIAN16 (ih.ins[i].loop_size);
	B_ENDIAN16 (ih.ins[i].loop_start);
    }

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Instrument name        Len  LBeg LEnd L Vl Ft\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxih[i].nsm = !!(xxs[i].len = 2 * ih.ins[i].len);
	xxs[i].lps = 2 * ih.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * ih.ins[i].loop_size;
	xxs[i].flg = ih.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = ih.ins[i].volume;
	xxi[i][0].fin = ((int16)ih.ins[i].finetune / 0x48) << 4;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	if (V (1) && xxs[i].len > 2)
	    report ("[%2X] %-22.22s %04x %04x %04x %c %02x %+01x\n",
		i, ih.ins[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol,
		xxi[i][0].fin >> 4);
    }


    PATTERN_INIT ();

    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	for (j = 0; j < xxh->chn; j++) {
	    xxp[i]->info[j].index =  ih.ord[i][j];
	}
	xxo[i] = i;

	if (V (0))
	    report (".");
    }

    if (V (0))
	report ("\nStored tracks  : %d ", xxh->trk);

    for (i = 0; i < xxh->trk; i++) {
	xxt[i] = calloc (sizeof (struct xxm_track) + sizeof
		(struct xxm_event) * 64, 1);
	xxt[i]->rows = 64;
	for (j = 0; j < xxt[i]->rows; j++) {
		event = &xxt[i]->event[j];
		fread (ev, 1, 4, f);
		cvt_pt_event (event, ev);
	}

	if (V (0) && !(i % xxh->chn))
	    report (".");
    }

    xxh->flg |= XXM_FLG_MODRNG;

    /* Read samples */

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->ins; i++) {
	if (xxs[i].len <= 4)
	    continue;
	xmp_drv_loadpatch (f, i, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}

