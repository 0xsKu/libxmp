/* MED4 loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: med4_load.c,v 1.7 2007-09-07 22:03:47 cmatsuoka Exp $
 */

/*
 * MED 2.13 is in Fish disk #424 and has a couple of demo modules, get it
 * from ftp://ftp.funet.fi/pub/amiga/fish/401-500/ff424. Alex Van Starrex's
 * HappySong MED4 is in ff401. MED 3.00 is in ff476.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include "load.h"

#define MAGIC_MED4	MAGIC4('M','E','D',4)


static int read4_ctl;

static void fix_effect(struct xxm_event *event)
{
	switch (event->fxt) {
	case 0x00:	/* arpeggio */
	case 0x01:	/* slide up */
	case 0x02:	/* slide down */
		break;
	case 0x03:	/* vibrato */
		event->fxt = FX_VIBRATO;
		break;
	case 0x0c:	/* set volume (BCD) */
		event->fxp = MSN(event->fxp) * 10 +
					LSN(event->fxp);
		break;
	case 0x0d:	/* volume slides */
		event->fxt = FX_VOLSLIDE;
		break;
	case 0x0f:	/* tempo/break */
		if (event->fxp == 0)
			event->fxt = FX_BREAK;
		if (event->fxp == 0xff) {
			event->fxp = event->fxt = 0;
			event->vol = 1;
		} else if (event->fxp == 0xfe) {
			event->fxp = event->fxt = 0;
		} else if (event->fxp == 0xf1) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_RETRIG << 4) | 3;
		} else if (event->fxp == 0xf2) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_CUT << 4) | 3;
		} else if (event->fxp == 0xf3) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_DELAY << 4) | 3;
		} else if (event->fxp > 10) {
			event->fxt = FX_S3M_BPM;
			event->fxp = event->fxp * 4 - 7;
		}
		break;
	default:
		event->fxp = event->fxt = 0;
	}
}

static inline uint8 read4(FILE *f)
{
	static uint8 b = 0;
	uint8 ret;

	if (read4_ctl & 0x01) {
		ret = b & 0x0f;
	} else {
		b = read8(f);
		ret = b >> 4;
	}

	read4_ctl ^= 0x01;

	return ret;
}

static inline uint16 read12b(FILE *f)
{
	uint32 a, b, c;

	a = read4(f);
	b = read4(f);
	c = read4(f);

	return (a << 8) | (b << 4) | c;
}

int med4_load(FILE * f)
{
	int i, j, k;
	uint32 m, mask;
	int transp;
	int pos, vermaj, vermin;
	uint8 trkvol[16], buf[1024];
	struct xxm_event *event;

	LOAD_INIT();

	if (read32b(f) !=  MAGIC_MED4)
		return -1;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	fseek(f, -1023, SEEK_CUR);
	fread(buf, 1, 1024, f);
	fseek(f, pos, SEEK_SET);

	vermaj = 2;
	vermin = 10;

	for (i = 0; i < 1012; i++) {
		if (!memcmp(buf + i, "MEDV\000\000\000\004", 8)) {
			vermaj = *(buf + i + 10);
			vermin = *(buf + i + 11);
			break;
		}
	}

	sprintf(xmp_ctl->type, "MED4 (MED %d.%02d)", vermaj, vermin);

	xxh->ins = xxh->smp = 32;
	INSTRUMENT_INIT();

	m = read8(f);
	for (mask = i = 0; i < 8; i++, m <<= 1) {
		if (m & 0x80) {
			mask <<= 8;
			mask |= read8(f);
		}
	}

	/* read instrument names */
	for (i = 0; i < 32; i++, mask <<= 1) {
		uint8 c, size, buf[40];

		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		if (~mask & 0x80000000)
			continue;

		c = read8(f);

		size = read8(f);
		for (j = 0; j < size; j++)
			buf[j] = read8(f);
		buf[j] = 0;

		xxi[i][0].vol = c & 0x20 ? 0x40 : read8(f);

		if (c == 0x4c) {
			read32b(f);
		}
		if (c == 0x4d) {
			read32b(f);
		}
		if (c == 0x6c)
			read32b(f);

		copy_adjust(xxih[i].name, buf, 32);
		//printf("%d = %s\n", i, xxih[i].name);
	}

	xxh->chn = 4;
	xxh->pat = read16b(f);
	xxh->trk = xxh->chn * xxh->pat;

	xxh->len = read16b(f);
	fread(xxo, 1, xxh->len, f);
	xxh->bpm = read16b(f);
        transp = read8s(f);
        read8s(f);
        read8s(f);
	xxh->tpo = read8(f);

	fseek(f, 20, SEEK_CUR);

	fread(trkvol, 1, 16, f);
	read8(f);		/* master vol */

	MODULE_INFO();

	reportv(0, "Play transpose : %d\n", transp);

	for (i = 0; i < 32; i++)
		xxi[i][0].xpo = transp;

	PATTERN_INIT();

	/* Load and convert patterns */
	reportv(0, "Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		int size, plen;
		uint8 ctl, chmsk;
		uint32 linemsk0, fxmsk0, linemsk1, fxmsk1, x;

		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		//printf("\n===== PATTERN %d =====\n", i);
		//printf("offset = %lx\n", ftell(f));

		size = read8(f);	/* pattern control block */
		x = read16b(f);		/* 0x043f */
		plen = read16b(f);
		ctl = read8(f);

		linemsk0 = ctl & 0x80 ? ~0 : ctl & 0x40 ? 0 : read32b(f);
		fxmsk0   = ctl & 0x20 ? ~0 : ctl & 0x10 ? 0 : read32b(f);
		linemsk1 = ctl & 0x08 ? ~0 : ctl & 0x04 ? 0 : read32b(f);
		fxmsk1   = ctl & 0x02 ? ~0 : ctl & 0x01 ? 0 : read32b(f);

		/*printf("size = %02x\n", size);
		printf("x    = %02x\n", x);
		printf("plen = %04x\n", plen);
		printf("ctl  = %02x\n", ctl);
		printf("linemsk0 = %08x\n", linemsk0);
		printf("fxmsk0   = %08x\n", fxmsk0);
		printf("linemsk1 = %08x\n", linemsk1);
		printf("fxmsk1   = %08x\n", fxmsk1);*/

		assert(x == 0x043f);

		x = read8(f);		/* 0xff */
		//printf("blk end  = %02x\n\n", x);

		read4_ctl = 0;

		for (j = 0; j < 32; j++, linemsk0 <<= 1, fxmsk0 <<= 1) {
			if (linemsk0 & 0x80000000) {
				chmsk = read4(f);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, j);

					if (chmsk & 0x08) {
						x = read12b(f);
						event->note = x >> 4;
						if (event->note)
							event->note += 36;
						event->ins  = x & 0x0f;
					}
				}
			}

			if (fxmsk0 & 0x80000000) {
				chmsk = read4(f);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, j);

					if (chmsk & 0x08) {
						x = read12b(f);
						event->fxt = x >> 8;
						event->fxp = x & 0xff;
						fix_effect(event);
					}
				}
			}

			/*printf("%03d ", j);
			for (k = 0; k < 4; k++) {
				event = &EVENT(i, k, j);
				if (event->note)
					printf("%03d", event->note);
				else
					printf("---");
				printf(" %1x%1x%02x ",
					event->ins, event->fxt, event->fxp);
			}
			printf("\n");*/
		}

		for (j = 32; j < 64; j++, linemsk1 <<= 1, fxmsk1 <<= 1) {
			if (linemsk1 & 0x80000000) {
				chmsk = read4(f);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, j);

					if (chmsk & 0x08) {
						x = read12b(f);
						event->note = x >> 4;
						event->ins  = x & 0x0f;
					}
				}
			}

			if (fxmsk1 & 0x80000000) {
				chmsk = read4(f);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, j);

					if (chmsk & 0x08) {
						x = read12b(f);
						event->fxt = x >> 8;
						event->fxp = x & 0xff;
						fix_effect(event);
					}
				}
			}

			/*printf("%03d ", j);
			for (k = 0; k < 4; k++) {
				event = &EVENT(i, k, j);
				if (event->note)
					printf("%03d", event->note);
				else
					printf("---");
				printf(" %1x%1x%02x ",
					event->ins, event->fxt, event->fxp);
			}
			printf("\n");*/
		}

		reportv(0, ".");
	}
	reportv(0, "\n");

	/* Load samples */

	reportv(0, "Instruments    : %d ", xxh->ins);
	reportv(1, "\n     Instrument name                  Len  LBeg LEnd L Vol");

	mask = read32b(f);

	read16b(f);

	//printf("instrument mask: %08x\n", mask);

	mask <<= 1;	/* no instrument #0 */
	for (i = 0; i < 32; i++, mask <<= 1) {
		if (~mask & 0x80000000)
			continue;

		read16b(f);
		read16b(f);
		xxs[i].len = read16b(f);

		xxi[i][0].sid = i;
		xxih[i].nsm = !!(xxs[i].len);

		reportv(1, "\n[%2X] %-32.32s %04x %04x %04x %c V%02x ",
			i, xxih[i].name, xxs[i].len, xxs[i].lps,
			xxs[i].lpe,
			xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			xxi[i][0].vol);

		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
				  &xxs[xxi[i][0].sid], NULL);
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}
