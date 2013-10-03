/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/*
 * OctaMED v1.00b: ftp://ftp.funet.fi/pub/amiga/fish/501-600/ff579
 */

#include "med.h"
#include "loader.h"
#include "med_extras.h"

static int mmd1_test(HIO_HANDLE *, char *, const int);
static int mmd1_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader mmd1_loader = {
	"MED 2.10/OctaMED (MED)",
	mmd1_test,
	mmd1_load
};

static int mmd1_test(HIO_HANDLE *f, char *t, const int start)
{
	char id[4];
	uint32 offset, len;

	if (hio_read(id, 1, 4, f) < 4)
		return -1;

	if (memcmp(id, "MMD0", 4) && memcmp(id, "MMD1", 4))
		return -1;

	hio_seek(f, 28, SEEK_CUR);
	offset = hio_read32b(f);		/* expdata_offset */
	
	if (offset) {
		hio_seek(f, start + offset + 44, SEEK_SET);
		offset = hio_read32b(f);
		len = hio_read32b(f);
		hio_seek(f, start + offset, SEEK_SET);
		read_title(f, t, len);
	} else {
		read_title(f, t, 0);
	}

	return 0;
}


static int mmd1_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k;
	struct MMD0 header;
	struct MMD0song song;
	struct MMD1Block block;
	struct InstrHdr instr;
	struct SynthInstr synth;
	struct InstrExt exp_smp;
	struct MMD0exp expdata;
	struct xmp_event *event;
	int ver = 0;
	int smp_idx = 0;
	uint8 e[4];
	int song_offset;
	int blockarr_offset;
	int smplarr_offset;
	int expdata_offset;
	int expsmp_offset;
	int songname_offset;
	int iinfo_offset;
	int pos;
	int bpm_on, bpmlen, med_8ch;

	LOAD_INIT();

	hio_read(&header.id, 4, 1, f);

	ver = *((char *)&header.id + 3) - '1' + 1;

	D_(D_WARN "load header");
	header.modlen = hio_read32b(f);
	song_offset = hio_read32b(f);
	D_(D_INFO "song_offset = 0x%08x", song_offset);
	hio_read16b(f);
	hio_read16b(f);
	blockarr_offset = hio_read32b(f);
	D_(D_INFO "blockarr_offset = 0x%08x", blockarr_offset);
	hio_read32b(f);
	smplarr_offset = hio_read32b(f);
	D_(D_INFO "smplarr_offset = 0x%08x", smplarr_offset);
	hio_read32b(f);
	expdata_offset = hio_read32b(f);
	D_(D_INFO "expdata_offset = 0x%08x", expdata_offset);
	hio_read32b(f);
	header.pstate = hio_read16b(f);
	header.pblock = hio_read16b(f);
	header.pline = hio_read16b(f);
	header.pseqnum = hio_read16b(f);
	header.actplayline = hio_read16b(f);
	header.counter = hio_read8(f);
	header.extra_songs = hio_read8(f);

	/*
	 * song structure
	 */
	D_(D_WARN "load song");
	hio_seek(f, start + song_offset, SEEK_SET);
	for (i = 0; i < 63; i++) {
		song.sample[i].rep = hio_read16b(f);
		song.sample[i].replen = hio_read16b(f);
		song.sample[i].midich = hio_read8(f);
		song.sample[i].midipreset = hio_read8(f);
		song.sample[i].svol = hio_read8(f);
		song.sample[i].strans = hio_read8s(f);
	}
	song.numblocks = hio_read16b(f);
	song.songlen = hio_read16b(f);
	D_(D_INFO "song.songlen = %d", song.songlen);
	for (i = 0; i < 256; i++)
		song.playseq[i] = hio_read8(f);
	song.deftempo = hio_read16b(f);
	song.playtransp = hio_read8(f);
	song.flags = hio_read8(f);
	song.flags2 = hio_read8(f);
	song.tempo2 = hio_read8(f);
	for (i = 0; i < 16; i++)
		song.trkvol[i] = hio_read8(f);
	song.mastervol = hio_read8(f);
	song.numsamples = hio_read8(f);

	/*
	 * convert header
	 */
	m->c4rate = C4_NTSC_RATE;
	m->quirk |= song.flags & FLAG_STSLIDE ? 0 : QUIRK_VSALL | QUIRK_PBALL;
	med_8ch = song.flags & FLAG_8CHANNEL;
	bpm_on = song.flags2 & FLAG2_BPM;
	bpmlen = 1 + (song.flags2 & FLAG2_BMASK);
	m->time_factor = MED_TIME_FACTOR;

	/* From the OctaMEDv4 documentation:
	 *
	 * In 8-channel mode, you can control the playing speed more
	 * accurately (to techies: by changing the size of the mix buffer).
	 * This can be done with the left tempo gadget (values 1-10; the
	 * lower, the faster). Values 11-240 are equivalent to 10.
	 */

        mod->spd = song.tempo2;
        mod->bpm = med_8ch ?
			mmd_get_8ch_tempo(song.deftempo) :
			(bpm_on ? song.deftempo * bpmlen / 16 : song.deftempo);

	mod->pat = song.numblocks;
	mod->ins = song.numsamples;
	//mod->smp = mod->ins;
	mod->len = song.songlen;
	mod->rst = 0;
	mod->chn = 0;
	memcpy(mod->xxo, song.playseq, mod->len);
	mod->name[0] = 0;

	/*
	 * Obtain number of samples from each instrument
	 */
	mod->smp = 0;
	for (i = 0; i < mod->ins; i++) {
		uint32 smpl_offset;
		int16 type;
		hio_seek(f, start + smplarr_offset + i * 4, SEEK_SET);
		smpl_offset = hio_read32b(f);
		if (smpl_offset == 0)
			continue;
		hio_seek(f, start + smpl_offset, SEEK_SET);
		hio_read32b(f);				/* length */
		type = hio_read16b(f);
		if (type == -1) {			/* type is synth? */
			hio_seek(f, 14, SEEK_CUR);
			mod->smp += hio_read16b(f);		/* wforms */
		} else {
			mod->smp++;
		}
	}

	/*
	 * expdata
	 */
	D_(D_WARN "load expdata");
	expdata.s_ext_entries = 0;
	expdata.s_ext_entrsz = 0;
	expdata.i_ext_entries = 0;
	expdata.i_ext_entrsz = 0;
	expsmp_offset = 0;
	iinfo_offset = 0;
	if (expdata_offset) {
		hio_seek(f, start + expdata_offset, SEEK_SET);
		hio_read32b(f);
		expsmp_offset = hio_read32b(f);
		D_(D_INFO "expsmp_offset = 0x%08x", expsmp_offset);
		expdata.s_ext_entries = hio_read16b(f);
		expdata.s_ext_entrsz = hio_read16b(f);
		hio_read32b(f);
		hio_read32b(f);
		iinfo_offset = hio_read32b(f);
		D_(D_INFO "iinfo_offset = 0x%08x", iinfo_offset);
		expdata.i_ext_entries = hio_read16b(f);
		expdata.i_ext_entrsz = hio_read16b(f);
		hio_read32b(f);
		hio_read32b(f);
		hio_read32b(f);
		hio_read32b(f);
		songname_offset = hio_read32b(f);
		D_(D_INFO "songname_offset = 0x%08x", songname_offset);
		expdata.songnamelen = hio_read32b(f);
		hio_seek(f, start + songname_offset, SEEK_SET);
		D_(D_INFO "expdata.songnamelen = %d", expdata.songnamelen);
		for (i = 0; i < expdata.songnamelen; i++) {
			if (i >= XMP_NAME_SIZE)
				break;
			mod->name[i] = hio_read8(f);
		}
	}

	/*
	 * Quickly scan patterns to check the number of channels
	 */
	D_(D_WARN "find number of channels");

	for (i = 0; i < mod->pat; i++) {
		int block_offset;

		hio_seek(f, start + blockarr_offset + i * 4, SEEK_SET);
		block_offset = hio_read32b(f);
		D_(D_INFO "block %d block_offset = 0x%08x", i, block_offset);
		if (block_offset == 0)
			continue;
		hio_seek(f, start + block_offset, SEEK_SET);

		if (ver > 0) {
			block.numtracks = hio_read16b(f);
			block.lines = hio_read16b(f);
		} else {
			block.numtracks = hio_read8(f);
			block.lines = hio_read8(f);
		}

		if (block.numtracks > mod->chn)
			mod->chn = block.numtracks;
	}

	mod->trk = mod->pat * mod->chn;

	set_type(m, ver == 0 ? mod->chn > 4 ? "OctaMED 2.00 MMD0" :
				"MED 2.10 MMD0" : "OctaMED 4.00 MMD1");
	
	MODULE_INFO();

	D_(D_INFO "BPM mode: %s (length = %d)", bpm_on ? "on" : "off", bpmlen);
	D_(D_INFO "Song transpose: %d", song.playtransp);
	D_(D_INFO "Stored patterns: %d", mod->pat);

	/*
	 * Read and convert patterns
	 */
	D_(D_WARN "read patterns");
	if (pattern_init(mod) < 0)
		return -1;

	for (i = 0; i < mod->pat; i++) {
		int block_offset;

		hio_seek(f, start + blockarr_offset + i * 4, SEEK_SET);
		block_offset = hio_read32b(f);
		if (block_offset == 0)
			continue;
		hio_seek(f, start + block_offset, SEEK_SET);

		if (ver > 0) {
			block.numtracks = hio_read16b(f);
			block.lines = hio_read16b(f);
			hio_read32b(f);
		} else {
			block.numtracks = hio_read8(f);
			block.lines = hio_read8(f);
		}

		if (pattern_tracks_alloc(mod, i, block.lines + 1) < 0)
			return -1;

		if (ver > 0) {		/* MMD1 */
			for (j = 0; j < mod->xxp[i]->rows; j++) {
				for (k = 0; k < block.numtracks; k++) {
					e[0] = hio_read8(f);
					e[1] = hio_read8(f);
					e[2] = hio_read8(f);
					e[3] = hio_read8(f);

					event = &EVENT(i, k, j);
					event->note = e[0] & 0x7f;
					if (event->note)
						event->note +=
						    48 + song.playtransp;
					event->ins = e[1] & 0x3f;

					/* Decay */
					if (event->ins && !event->note) {
						event->f2t = FX_MED_DECAY;
						event->f2p = event->ins;
						event->ins = 0;
					}

					event->fxt = e[2];
					event->fxp = e[3];
					mmd_xlat_fx(event, bpm_on,
							bpmlen, med_8ch);
				}
			}
		} else {		/* MMD0 */
			for (j = 0; j < mod->xxp[i]->rows; j++) {
				for (k = 0; k < block.numtracks; k++) {
					e[0] = hio_read8(f);
					e[1] = hio_read8(f);
					e[2] = hio_read8(f);

					event = &EVENT(i, k, j);
					event->note = e[0] & 0x3f;
					if (event->note)
						event->note += 48;
					event->ins =
					    (e[1] >> 4) | ((e[0] & 0x80) >> 3)
					    | ((e[0] & 0x40) >> 1);

					/* Decay */
					if (event->ins && !event->note) {
						event->f2t = FX_MED_DECAY;
						event->f2p = event->ins;
						event->ins = 0;
					}

					event->fxt = e[1] & 0x0f;
					event->fxp = e[2];
					mmd_xlat_fx(event, bpm_on,
							bpmlen, med_8ch);
				}
			}
		}
	}

	if (med_new_module_extras(m))
		return -1;

	/*
	 * Read and convert instruments and samples
	 */
	D_(D_WARN "read instruments");
	if (instrument_init(mod) < 0)
		return -1;

	D_(D_INFO "Instruments: %d", mod->ins);

	for (smp_idx = i = 0; i < mod->ins; i++) {
		int smpl_offset;
		char name[40] = "";

		hio_seek(f, start + smplarr_offset + i * 4, SEEK_SET);
		smpl_offset = hio_read32b(f);

		D_(D_INFO "sample %d smpl_offset = 0x%08x", i, smpl_offset);

		if (smpl_offset == 0)
			continue;

		hio_seek(f, start + smpl_offset, SEEK_SET);
		instr.length = hio_read32b(f);
		instr.type = hio_read16b(f);

		pos = hio_tell(f);

		if (expdata_offset && i < expdata.i_ext_entries) {
		    hio_seek(f, iinfo_offset + i * expdata.i_ext_entrsz, SEEK_SET);
		    hio_read(name, 40, 1, f);
		}

		D_(D_INFO "\n[%2x] %-40.40s %d", i, name, instr.type);

		exp_smp.finetune = 0;
		if (expdata_offset && i < expdata.s_ext_entries) {
			hio_seek(f, expsmp_offset + i * expdata.s_ext_entrsz,
							SEEK_SET);
			exp_smp.hold = hio_read8(f);
			exp_smp.decay = hio_read8(f);
			exp_smp.suppress_midi_off = hio_read8(f);
			exp_smp.finetune = hio_read8(f);
		}

		hio_seek(f, pos, SEEK_SET);

		if (instr.type == -2) {			/* Hybrid */
			int length, type;
			int pos = hio_tell(f);

			synth.defaultdecay = hio_read8(f);
			hio_seek(f, 3, SEEK_CUR);
			synth.rep = hio_read16b(f);
			synth.replen = hio_read16b(f);
			synth.voltbllen = hio_read16b(f);
			synth.wftbllen = hio_read16b(f);
			synth.volspeed = hio_read8(f);
			synth.wfspeed = hio_read8(f);
			synth.wforms = hio_read16b(f);
			hio_read(synth.voltbl, 1, 128, f);;
			hio_read(synth.wftbl, 1, 128, f);;

			hio_seek(f, pos - 6 + hio_read32b(f), SEEK_SET);
			length = hio_read32b(f);
			type = hio_read16b(f);

			if (med_new_instrument_extras(&mod->xxi[i]) != 0)
				return -1;

			mod->xxi[i].nsm = 1;
			if (subinstrument_alloc(mod, i, 1) < 0)
				return -1;

			MED_INSTRUMENT_EXTRAS(mod->xxi[i])->vts = synth.volspeed;
			MED_INSTRUMENT_EXTRAS(mod->xxi[i])->wts = synth.wfspeed;
			mod->xxi[i].sub[0].pan = 0x80;
			mod->xxi[i].sub[0].vol = song.sample[i].svol;
			mod->xxi[i].sub[0].xpo = song.sample[i].strans;
			mod->xxi[i].sub[0].sid = smp_idx;
			mod->xxi[i].sub[0].fin = exp_smp.finetune;
			mod->xxs[smp_idx].len = length;
			mod->xxs[smp_idx].lps = 2 * song.sample[i].rep;
			mod->xxs[smp_idx].lpe = mod->xxs[smp_idx].lps +
						2 * song.sample[i].replen;
			mod->xxs[smp_idx].flg = song.sample[i].replen > 1 ?
						XMP_SAMPLE_LOOP : 0;

			D_(D_INFO "  %05x %05x %05x %02x %+3d %+1d",
				       mod->xxs[smp_idx].len,
				       mod->xxs[smp_idx].lps,
				       mod->xxs[smp_idx].lpe,
				       mod->xxi[i].sub[0].vol,
				       mod->xxi[i].sub[0].xpo,
				       mod->xxi[i].sub[0].fin >> 4);

			load_sample(m, f, 0, &mod->xxs[smp_idx], NULL);

			smp_idx++;

			if (mmd_alloc_tables(m, i, &synth) != 0)
				return -1;

			continue;
		}

		if (instr.type == -1) {			/* Synthetic */
			int pos = hio_tell(f);

			synth.defaultdecay = hio_read8(f);
			hio_seek(f, 3, SEEK_CUR);
			synth.rep = hio_read16b(f);
			synth.replen = hio_read16b(f);
			synth.voltbllen = hio_read16b(f);
			synth.wftbllen = hio_read16b(f);
			synth.volspeed = hio_read8(f);
			synth.wfspeed = hio_read8(f);
			synth.wforms = hio_read16b(f);
			hio_read(synth.voltbl, 1, 128, f);;
			hio_read(synth.wftbl, 1, 128, f);;
			for (j = 0; j < 64; j++)
				synth.wf[j] = hio_read32b(f);

			D_(D_INFO "  VS:%02x WS:%02x WF:%02x %02x %+3d %+1d",
					synth.volspeed, synth.wfspeed,
					synth.wforms & 0xff,
					song.sample[i].svol,
					song.sample[i].strans,
					exp_smp.finetune);

			if (synth.wforms == 0xffff)	
				continue;

			if (med_new_instrument_extras(&mod->xxi[i]) != 0)
				return -1;

			mod->xxi[i].nsm = synth.wforms;
			if (subinstrument_alloc(mod, i, synth.wforms) < 0)
				return -1;

			MED_INSTRUMENT_EXTRAS(mod->xxi[i])->vts = synth.volspeed;
			MED_INSTRUMENT_EXTRAS(mod->xxi[i])->wts = synth.wfspeed;

			for (j = 0; j < synth.wforms; j++) {
				mod->xxi[i].sub[j].pan = 0x80;
				mod->xxi[i].sub[j].vol = song.sample[i].svol;
				mod->xxi[i].sub[j].xpo = song.sample[i].strans - 24;
				mod->xxi[i].sub[j].sid = smp_idx;
				mod->xxi[i].sub[j].fin = exp_smp.finetune;

				hio_seek(f, pos - 6 + synth.wf[j], SEEK_SET);

				mod->xxs[smp_idx].len = hio_read16b(f) * 2;
				mod->xxs[smp_idx].lps = 0;
				mod->xxs[smp_idx].lpe = mod->xxs[smp_idx].len;
				mod->xxs[smp_idx].flg = XMP_SAMPLE_LOOP;

				load_sample(m, f, 0, &mod->xxs[smp_idx], NULL);

				smp_idx++;
			}

			if (mmd_alloc_tables(m, i, &synth) != 0)
				return -1;

			continue;
		}

		if (instr.type != 0)
			continue;

		/* instr type is sample */
		mod->xxi[i].nsm = 1;
		if (subinstrument_alloc(mod, i, 1) < 0)
			return -1;

		mod->xxi[i].sub[0].vol = song.sample[i].svol;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].xpo = song.sample[i].strans;
		mod->xxi[i].sub[0].sid = smp_idx;
		mod->xxi[i].sub[0].fin = exp_smp.finetune << 4;

		mod->xxs[smp_idx].len = instr.length;
		mod->xxs[smp_idx].lps = 2 * song.sample[i].rep;
		mod->xxs[smp_idx].lpe = mod->xxs[smp_idx].lps + 2 *
						song.sample[i].replen;

		mod->xxs[smp_idx].flg = 0;
		if (song.sample[i].replen > 1)
			mod->xxs[smp_idx].flg |= XMP_SAMPLE_LOOP;

		D_(D_INFO "  %05x %05x %05x %02x %+3d %+1d",
				mod->xxs[smp_idx].len,
				mod->xxs[smp_idx].lps,
				mod->xxs[smp_idx].lpe,
				mod->xxi[i].sub[0].vol,
				mod->xxi[i].sub[0].xpo,
				mod->xxi[i].sub[0].fin >> 4);

		hio_seek(f, start + smpl_offset + 6, SEEK_SET);
		load_sample(m, f, 0, &mod->xxs[smp_idx], NULL);

		smp_idx++;
	}

	/*
	 * Adjust event note data in patterns
	 */

	/* Restrict non-synth instruments to 3 octave range.
	 * Checked in MMD0 with med.egypian/med.medieval from Lemmings 2
	 * and MED.ParasolStars, MMD1 with med.Lemmings2
	 */
	for (i = 0; i < mod->pat; i++) {
		for (j = 0; j < mod->xxp[i]->rows; j++) {
			for (k = 0; k < mod->chn; k++) {
				event = &EVENT(i, k, j);

				if (!event->note || !event->ins)
					continue;

				/* Not a synth instrument */
				if (!MED_MODULE_EXTRAS(*m)->wav_table[event->ins - 1]) {
					while (event->note > (48 + 36))
						event->note -= 12;
				}
			}
		}
	}

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].vol = song.trkvol[i];
		mod->xxc[i].pan = (((i + 1) / 2) % 2) * 0xff;
	}

	return 0;
}
