/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "mod.h"
#include "period.h"


int flt_load (FILE *f)
{
    int i, j;
    struct xxm_event *event;
    struct mod_header mh;
    uint8 mod_event[4];
    char *tracker;

    LOAD_INIT ();

    fread (&mh, 1, sizeof (struct mod_header), f);

    if (mh.magic[0] == 'F' && mh.magic[1] == 'L' && mh.magic[2] == 'T')
	tracker = "Startrekker";
    else if (mh.magic[0] == 'E' && mh.magic[1] == 'X' && mh.magic[2] == 'O')
	tracker = "Startrekker/Audio Sculpture";
    else
	return -1;

    if (mh.magic[3] == '4')
	xxh->chn = 4;
    else if (mh.magic[3] == '8') 
	xxh->chn = 8;
    else return -1;

    xxh->len = mh.len;
    xxh->rst = mh.restart;
    memcpy (xxo, mh.order, 128);

    for (i = 0; i < 128; i++) {
	if (xxh->chn > 4)
	    xxo[i] >>= 1;
	if (xxo[i] > xxh->pat)
	    xxh->pat = xxo[i];
    }

    xxh->pat++;

    xxh->trk = xxh->chn * xxh->pat;

    strncpy (xmp_ctl->name, (char *) mh.name, 20);
    sprintf (xmp_ctl->type, "%4.4s (%d channel MOD)", mh.magic, xxh->chn);
    sprintf (tracker_name, tracker);
    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Instrument name        Len  LBeg LEnd L Vol Fin\n");

    for (i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (mh.ins[i].size);
	B_ENDIAN16 (mh.ins[i].loop_start);
	B_ENDIAN16 (mh.ins[i].loop_size);

	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = 2 * mh.ins[i].size;
	xxs[i].lps = 2 * mh.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * mh.ins[i].loop_size;
	xxs[i].flg = mh.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = (int8)(mh.ins[i].finetune << 4);
	xxi[i][0].vol = mh.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	xxih[i].rls = 0xfff;
	strncpy (xxih[i].name, mh.ins[i].name, 22);
	str_adj (xxih[i].name);

	if ((V (1)) && (strlen ((char *) xxih[i].name) ||
	    xxs[i].len > 2))
	    report ("[%2X] %-22.22s %04x %04x %04x %c V%02x %+d\n",
		i, xxih[i].name, xxs[i].len, xxs[i].lps,
		xxs[i].lpe, mh.ins[i].loop_size > 1 ? 'L' : ' ',
		xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
    }

    PATTERN_INIT ();

    /* Load and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < (64 * 4); j++) {
	    event = &EVENT (i, j % 4, j / 4);
	    fread (mod_event, 1, 4, f);
	    cvt_pt_event (event, mod_event);
	}
	if (xxh->chn > 4) {
	    for (j = 0; j < (64 * 4); j++) {
		event = &EVENT (i, (j % 4) + 4, j / 4);
		fread (mod_event, 1, 4, f);
		cvt_pt_event (event, mod_event);
	    }
	}

	if (V (0))
	    report (".");
    }

    xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);
    for (i = 0; i < xxh->smp; i++) {
	if (!xxs[i].len)
	    continue;
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
	    &xxs[xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
