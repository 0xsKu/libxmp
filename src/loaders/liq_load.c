/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: liq_load.c,v 1.1 2001-06-02 20:26:30 cmatsuoka Exp $
 */

/* Liquid Tracker module loader based on the format description written
 * by Nir Oren. Tested with Shell.liq sent by Adi Sapir.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include "period.h"
#include "load.h"
#include "liq.h"

#define NONE 0xff

// static uint8 arpeggio_val[64];

static uint8 fx[] = {
    NONE,
    FX_TEMPO,		FX_VIBRATO,
    FX_BREAK,		FX_PORTA_DN,
    FX_PORTA_UP,	NONE /* F: reserved */,
    FX_ARPEGGIO,	NONE /* H: reserved */,
    NONE,		FX_JUMP,
    FX_TREMOLO,		FX_VOLSLIDE,
    FX_EXTENDED,	FX_TONEPORTA,
    FX_OFFSET,		NONE,
    NONE,		NONE,
    NONE,		NONE,
    NONE,		NONE,
    NONE,		NONE,
    NONE,		NONE
};


/* Effect translation */
static void xlat_fx (int c, struct xxm_event *e)
{
#if 0
    uint8 h = MSN (e->fxp), l = LSN (e->fxp);

    e->fxt = e->fxp = 0;
    return;
#endif

    switch (e->fxt = fx[e->fxt]) {
    case FX_ARPEGGIO:			/* Arpeggio */
#if 0
	if (e->fxp)
	    arpeggio_val[c] = e->fxp;
	else
	    e->fxp = arpeggio_val[c];
	break;
    case FX_TEMPO:
	if (e->fxp < 0x20)
	    e->fxp = 0x20;
	break;
    case FX_S3M_TEMPO:
	if (e->fxp < 0x20)
	    e->fxt = FX_TEMPO;
	break;
    case FX_EXTENDED:			/* Extended effects */
	switch (h) {
	case 0x3:			/* Glissando */
	    e->fxp = l | (EX_GLISS << 4);
	    break;
	case 0x4:			/* Vibrato wave */
	    e->fxp = l | (EX_VIBRATO_WF << 4);
	    break;
	case 0x5:			/* Finetune */
	    e->fxp = l | (EX_FINETUNE << 4);
	    break;
	case 0x6:			/* Pattern loop */
	    e->fxp = l | (EX_PATTERN_LOOP << 4);
	    break;
	case 0x7:			/* Tremolo wave */
	    e->fxp = l | (EX_TREMOLO_WF << 4);
	    break;
	case 0xc:			/* Tremolo wave */
	    e->fxp = l | (EX_CUT << 4);
	    break;
	case 0xd:			/* Tremolo wave */
	    e->fxp = l | (EX_DELAY << 4);
	    break;
	case 0xe:			/* Tremolo wave */
	    e->fxp = l | (EX_PATT_DELAY << 4);
	    break;
	default:			/* Ignore */
	    e->fxt = e->fxp = 0;
	    break;
	}
	break;
#endif
    case NONE:				/* No effect */
	e->fxt = e->fxp = 0;
	break;
    }
}


static void decode_event (uint8 x1, struct xxm_event *event, FILE *f)
{
    uint8 x2;

    memset (event, 0, sizeof (struct xxm_event));

    if (x1 & 0x01) {
        fread (&x2, 1, 1, f);
	if (x2 == 0xfe)
	    event->note = XMP_KEY_OFF;
	else
	    event->note = x2 + 1 + 24;
    }
    if (x1 & 0x02) {
        fread (&x2, 1, 1, f);
	event->ins = x2 + 1;
    }
    if (x1 & 0x04) {
        fread (&x2, 1, 1, f);
	event->vol = x2;
    }
    if (x1 & 0x08) {
        fread (&x2, 1, 1, f);
	event->fxt = x2 + 1 - 'A';
    }
    if (x1 & 0x10) {
        fread (&x2, 1, 1, f);
	event->fxp = x2;
    }
    _D(_D_INFO "  event: %02x %02x %02x %02x %02x",
	event->note, event->ins, event->vol, event->fxt, event->fxp);

    assert (event->note <= 107 || event->note == XMP_KEY_OFF);
    assert (event->ins <= 100);
    assert (event->vol <= 64);
    assert (event->fxt <= 26);
}

int liq_load (FILE * f)
{
    int i;
    struct xxm_event *event = NULL;
    struct liq_header lh;
    struct liq_instrument li;
    struct liq_pattern lp;
    uint8 x1, x2, pmag[4];

    LOAD_INIT ();

    fread (&lh, 1, sizeof (lh), f);
    if (strncmp ((char *) lh.magic, "Liquid Module:", 14))
	return -1;

    L_ENDIAN16 (lh.version);
    L_ENDIAN16 (lh.speed);
    L_ENDIAN16 (lh.bpm);
    L_ENDIAN16 (lh.low);
    L_ENDIAN16 (lh.high);
    L_ENDIAN16 (lh.chn);
    L_ENDIAN32 (lh.flags);
    L_ENDIAN16 (lh.pat);
    L_ENDIAN16 (lh.ins);
    L_ENDIAN16 (lh.len);
    L_ENDIAN16 (lh.hdrsz);

    if ((lh.version >> 8) == 0) {
	lh.hdrsz = lh.len;
	lh.len = 0;
	fseek (f, -2, SEEK_CUR);
    }

    xxh->tpo = lh.speed;
    xxh->bpm = lh.bpm;
    xxh->chn = lh.chn;
    xxh->pat = lh.pat;
    xxh->ins = xxh->smp = lh.ins;
    xxh->len = lh.len;
    xxh->trk = xxh->chn * xxh->pat;
    xxh->flg = XXM_FLG_INSVOL;

    strncpy (xmp_ctl->name, lh.name, 30);
    strncpy (tracker_name, lh.tracker, 20);
    strncpy (author_name, lh.author, 20);
    sprintf (xmp_ctl->type, "Liquid module %d.%02d",
	lh.version >> 8, lh.version & 0x00ff);

if (lh.version > 0) {
    for (i = 0; i < xxh->chn; i++) {
	fread (&x1, 1, 1, f);
	xxc[i].pan = x1 << 2;
    }

    for (i = 0; i < xxh->chn; i++) {
	fread (&x1, 1, 1, f);
	xxc[i].vol = x1;
    }

    fread (xxo, 1, xxh->len, f);

    /* Skip 1.01 echo pools */
    fseek (f, lh.hdrsz - (0x6d + xxh->chn * 2 + xxh->len), SEEK_CUR);
} else {
    fseek (f, 0xf0, SEEK_SET);
    fread (xxo, 1, 256, f);
    fseek (f, lh.hdrsz, SEEK_SET);

    for (i = 0; i < 256; i++) {
	if (xxo[i] == 0xff)
	    break;
    }
    xxh->len = i;
}

    MODULE_INFO ();


    PATTERN_INIT ();

    /* Read and convert patterns */

    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    x2 = 0;
    for (i = 0; i < xxh->pat; i++) {
	int row, channel, count;

	PATTERN_ALLOC (i);
	fread (pmag, 1, 4, f);
	if (pmag[0] == '!' && pmag[1] == '!' && pmag[2] == '!' && pmag[3] == '!')
	    continue;
	assert (pmag[0] == 'L' && pmag[1] == 'P' && !pmag[2] && !pmag[3]);
	
	_D(_D_WARN "\n\nPATTERN %d: %c %c %02x %02x",
		i, pmag[0], pmag[1], pmag[2], pmag[3]);
	fread (&lp, sizeof (struct liq_pattern), 1, f);
	L_ENDIAN16 (lp.rows);
	L_ENDIAN32 (lp.size);
	_D(_D_INFO "rows: %d  size: %d\n", lp.rows, lp.size);
	xxp[i]->rows = lp.rows;
	TRACK_ALLOC (i);

	row = 0;
	channel = 0;
	count = ftell (f);

/*
 * Packed pattern data is stored full Track after full Track from the left to
 * the right (all Intervals in Track and then going Track right). You should
 * expect 0C0h on any pattern end, and then your Unpacked Patterndata Pointer
 * should be equal to the value in offset [24h]; if it's not, you should exit
 * with an error.
 */

read_event:
	event = &EVENT (i, channel, row);

	if (x2) {
	    decode_event (x1, event, f);
	    xlat_fx (channel, event); 
	    x2--;
	    goto next_row;	
	}

	fread (&x1, 1, 1, f);

test_event:
	event = &EVENT (i, channel, row);
	_D(_D_INFO "* count=%ld chan=%d row=%d event=%02x",
		ftell(f) - count, channel, row, x1);

	switch (x1) {
	case 0xc0:
	    _D(_D_WARN "- end of pattern");
	    assert (ftell (f) - count == lp.size);
	    goto next_pattern;
	case 0xe1:
	    fread (&x1, 1, 1, f);
	    channel += x1;
	    _D(_D_INFO "  [skip %d channels]", x1);
	    /* fall thru */
	case 0xa0:
	    _D(_D_INFO "  [next channel]");
	    channel++;
	    if (channel >= xxh->chn) {
		_D(_D_CRIT "uh-oh! bad channel number!");
		channel--;
	    }
	    row = -1;
	    goto next_row;
	case 0xe0:
	    fread (&x1, 1, 1, f);
	    _D(_D_INFO "  [skip %d rows]", x1);
	    row += x1;
	    /* fall thru */
	case 0x80:
	    _D(_D_INFO "  [next row]");
	    goto next_row;
	}

	if (x1 > 0xc0 && x1 < 0xe0) {
	    _D(_D_INFO "  [packed data]");
	    decode_event (x1, event, f);
	    xlat_fx (channel, event); 
	    goto next_row;
	}

	if (x1 > 0xa0 && x1 < 0xc0) {
	    fread (&x2, 1, 1, f);
	    _D(_D_INFO "  [packed data - repeat %d times]", x2);
	    decode_event (x1, event, f);
	    xlat_fx (channel, event); 
	    goto next_row;
	}

	if (x1 > 0x80 && x1 < 0xa0) {
	    fread (&x2, 1, 1, f);
	    _D(_D_INFO "  [packed data - repeat %d times, keep note]", x2);
	    decode_event (x1, event, f);
	    xlat_fx (channel, event); 
	    while (x2) {
	        row ++;
		memcpy (&EVENT (i, channel, row), event, sizeof (struct xxm_event));
		x2--;
	    }
	    goto next_row;
	}

	_D (_D_INFO "  [unpacked data]");
	if (++x1)
	    event->note = x1 + 24;
	else if (x1 == 0xff)
	    event->note = XMP_KEY_OFF;

	fread (&x1, 1, 1, f);
	if (x1 > 100) {
	    row++;
	    goto test_event;
	}
	if (++x1)
	    event->ins = x1;

	fread (&x1, 1, 1, f);
	if (++x1)
	    event->vol = x1;

	fread (&x1, 1, 1, f);
	if (++x1)
	    event->fxt = x1 - 'A';

	fread (&x1, 1, 1, f);
	event->fxp = x1;

	xlat_fx (channel, event); 

	_D(_D_INFO "  event: %02x %02x %02x %02x %02x\n",
	    event->note, event->ins, event->vol, event->fxt, event->fxp);

	assert (event->note <= 107 || event->note == XMP_KEY_OFF);
	assert (event->ins <= 100);
	assert (event->vol <= 65);
	assert (event->fxt <= 26);

next_row:
	row++;
	if (row >= xxp[i]->rows) {
	    row = 0;
	    x2 = 0;
	    channel++;
	}

	if (channel >= xxh->chn) {
	    _D(_D_CRIT "bad channel number!");
	    fread (&x1, 1, 1, f);
	    goto test_event;
	}

	goto read_event;

next_pattern:
	if (V (0))
	    report (".");
    }

    /* Read and convert instruments */

    INSTRUMENT_INIT ();

    if (V (0))
	report ("\nInstruments    : %d ", xxh->ins);

    if (V(1)) {
	report ("\n"
"     Instrument name                Size  Start End Loop Vol   Ver  C2Spd");
    }

    for (i = 0; i < xxh->ins; i++) {
	unsigned char b[4];

	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	fread (&b, 1, 4, f);

	if (b[0] == '?' && b[1] == '?' && b[2] == '?' && b[3] == '?')
	    continue;
	assert (b[0] == 'L' && b[1] == 'D' && b[2] == 'S' && b[3] == 'S');
	_D(_D_WARN "INS %d: %c %c %c %c", i, b[0], b[1], b[2], b[3]);

	fread (&li, 1, sizeof (struct liq_instrument), f);
	L_ENDIAN32 (li.length);
	L_ENDIAN32 (li.loopstart);
	L_ENDIAN32 (li.loopend);
	L_ENDIAN32 (li.c2spd);
	L_ENDIAN16 (li.hdrsz);
	L_ENDIAN16 (li.comp);
	xxih[i].nsm = !!(li.length);
	xxih[i].vol = 0x40;
	xxs[i].len = li.length;
	xxs[i].lps = li.loopstart;
	xxs[i].lpe = li.loopend;

	if (li.flags & 0x01) {
	    xxs[i].flg = WAVE_16_BITS;
	    xxs[i].len <<= 1;
	}

	if (li.loopend > 0)
	    xxs[i].flg = WAVE_LOOPING;

	/* FIXME: LDSS 1.0 have global vol == 0 ? */
	/* if (li.gvl == 0) */
	    li.gvl = 0x40;

	xxi[i][0].vol = li.vol;
	xxi[i][0].gvl = li.gvl;
	xxi[i][0].pan = li.pan;
	xxi[i][0].sid = i;
	strncpy ((char *) xxih[i].name, li.name, 24);
	str_adj ((char *) li.name);
	if ((V (1)) && (strlen ((char *) li.name) || xxs[i].len)) {
	    report ("\n[%2X] %-30.30s %05x%c%05x %05x %c %02x %02x %2d.%02d %5d ",
		i, li.name, xxs[i].len,
		xxs[i].flg & WAVE_16_BITS ? '+' : ' ',
		xxs[i].lps, xxs[i].lpe,
		xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
		xxi[i][0].vol, xxi[i][0].gvl,
		li.version >> 8, li.version & 0xff, li.c2spd);
	}

	c2spd_to_note (li.c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
	fseek (f, li.hdrsz - 0x90, SEEK_CUR);

	if (!xxs[i].len)
	    continue;
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}

