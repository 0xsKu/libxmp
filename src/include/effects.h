/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifndef __EFFECTS_H
#define __EFFECTS_H

/* Protracker effects */
#define FX_ARPEGGIO	0x00
#define FX_PORTA_UP	0x01
#define FX_PORTA_DN	0x02
#define FX_TONEPORTA	0x03
#define FX_VIBRATO	0x04
#define FX_TONE_VSLIDE  0x05
#define FX_VIBRA_VSLIDE	0x06
#define FX_TREMOLO	0x07
#define FX_OFFSET	0x09
#define FX_VOLSLIDE	0x0a
#define FX_JUMP		0x0b
#define FX_VOLSET	0x0c
#define FX_BREAK	0x0d
#define FX_EXTENDED	0x0e
#define FX_TEMPO	0x0f

/* Unofficial Protracker effects */
#define FX_SETPAN	0x08

/* Fast Tracker II effects */
#define FX_GLOBALVOL	0x10
#define FX_G_VOLSLIDE	0x11
#define FX_KEYOFF	0x14
#define FX_ENVPOS	0x15
#define FX_MASTER_PAN	0x16
#define FX_PANSLIDE	0x19
#define FX_MULTI_RETRIG	0x1b
#define FX_TREMOR	0x1d
#define FX_XF_PORTA	0x21

/* Protracker extended effects */
#define EX_F_PORTA_UP	0x01
#define EX_F_PORTA_DN	0x02
#define EX_GLISS	0x03
#define EX_VIBRATO_WF	0x04
#define EX_FINETUNE	0x05
#define EX_PATTERN_LOOP	0x06
#define EX_TREMOLO_WF	0x07
#define EX_RETRIG	0x09
#define EX_F_VSLIDE_UP	0x0a
#define EX_F_VSLIDE_DN	0x0b
#define EX_CUT		0x0c
#define EX_DELAY	0x0d
#define EX_PATT_DELAY	0x0e

/* IT effects */
#define FX_TRK_VOL      0x80
#define FX_TRK_VSLIDE   0x81
#define FX_TRK_FVSLIDE  0x82
#define FX_IT_INSTFUNC	0x83
#define FX_FLT_CUTOFF	0x84
#define FX_FLT_RESN	0x85

/* MED effects */
#define FX_HOLD_DECAY	0x90
#define FX_SETPITCH	0x91

/* Extra effects */
#define FX_VOLSLIDE_UP	0xa0	/* SFX */
#define FX_VOLSLIDE_DN	0xa1
#define FX_S3M_TEMPO	0xa3
#define FX_VOLSLIDE_2	0xa4
#define FX_F_VSLIDE	0xa5	/* IMF/MDL */
#define FX_FINETUNE	0xa6
#define FX_NSLIDE_UP	0xa7	/* IMF/PTM */
#define FX_NSLIDE_DN	0xa8	/* IMF/PTM */
#define FX_CHORUS	0xa9	/* IMF */
#define FX_REVERB	0xaa	/* IMF */

#endif /* __EFFECTS_H */
