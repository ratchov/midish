/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "utils.h"
#include "mididev.h"
#include "sysex.h"
#include "track.h"
#include "song.h"
#include "smf.h"
#include "cons.h"
#include "frame.h"
#include "conv.h"

#define MAXTRACKNAME 100

char smftype_header[4] = { 'M', 'T', 'h', 'd' };
char smftype_track[4]  = { 'M', 'T', 'r', 'k' };

unsigned smf_evlen[] = { 2, 2, 2, 2, 1, 1, 2, 0 };
#define SMF_EVLEN(status) (smf_evlen[((status) >> 4) & 7])

/* --------------------------------------------- chunk read/write --- */

struct smf
{
	FILE *file;
	unsigned length, index;		/* current chunk length/position */
};

/*
 * open a standard midi file and initialize
 * the smf structure
 */
unsigned
smf_open(struct smf *o, char *path, char *mode)
{
	o->file = fopen(path, mode);
	if (!o->file) {
		cons_errs(path, "failed to open file");
		return 0;
	}
	o->length = 0;
	o->index = 0;
	return 1;
}

/*
 * close the file
 */
void
smf_close(struct smf *o)
{
	fclose(o->file);
}

/*
 * read a 32bit fixed-size number, return 0 on error
 */
unsigned
smf_get32(struct smf *o, unsigned *val)
{
	unsigned char buf[4];
	if (o->index + 4 > o->length || fread(buf, 1, 4, o->file) != 4) {
		cons_err("failed to read 32bit number");
		return 0;
	}
	*val = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
	o->index += 4;
	return 1;
}

/*
 * read a 24bit fixed-size number, return 0 on error
 */
unsigned
smf_get24(struct smf *o, unsigned *val)
{
	unsigned char buf[4];
	if (o->index + 3 > o->length || fread(buf, 1, 3, o->file) != 3) {
		cons_err("failed to read 24bit number");
		return 0;
	}
	*val = (buf[0] << 16) + (buf[1] << 8) + buf[2];
	o->index += 3;
	return 1;
}

/*
 * read a 16bit fixed-size number, return 0 on error
 */
unsigned
smf_get16(struct smf *o, unsigned *val)
{
	unsigned char buf[4];
	if (o->index + 2 > o->length || fread(buf, 1, 2, o->file) != 2) {
		cons_err("failed to read 16bit number");
		return 0;
	}
	*val =  (buf[0] << 8) + buf[1];
	o->index += 2;
	return 1;
}

/*
 * read a 8bit fixed-size number, return 0 on error
 */
unsigned
smf_getc(struct smf *o, unsigned *res)
{
	int c;
	if (o->index + 1 > o->length || (c = fgetc(o->file)) < 0) {
		cons_err("failed to read one byte");
		return 0;
	}
	o->index++;
	*res = c & 0xff;
	return 1;
}

/*
 * read a variable length number, return 0 on error
 */
unsigned
smf_getvar(struct smf *o, unsigned *val)
{
	int c;
	unsigned bits;
	*val = 0;
	bits = 0;
	for (;;) {
		if (o->index + 1 > o->length || (c = fgetc(o->file)) == EOF) {
			cons_err("failed to read varlength number");
			return 0;
		}
		o->index++;
		*val += (c & 0x7f);
		if (!(c & 0x80)) {
			break;
		}
		*val <<= 7;
		bits += 7;
		/*
		 * smf spec forbids more than 32bit per integer
		 */
		if (bits > 32) {
			cons_err("overflow while reading varlength number");
			return 0;
		}
	}
	return 1;
}


/*
 * read a chunk header, compare it with ethe given 4-byte header and
 * initialize the smf structure so that other smf_getxxx can work.
 * return 0 on error
 */
unsigned
smf_getheader(struct smf *o, char *hdr)
{
	char buf[4];
	unsigned len;
	if (o->index != o->length) {
		cons_err("chunk not finished");
		return 0;
	}
	if (fread(buf, 1, 4, o->file) != 4) {
		cons_err("failed to read header");
		return 0;
	}
	if (memcmp(buf, hdr, 4) != 0) {
		cons_err("header corrupted");
		return 0;
	}
	o->index = 0;
	o->length = 4;
	if (!smf_get32(o, &len)) {
		return 0;
	}
	o->index = 0;
	o->length = len;
	return 1;
}

/*
 * in smf_putxxx routines there is a 'unsigned *used' argument if
 * (used == NULL) then data is not written in the smf structure, only
 * *user is incremented by the number of bytes that would be written
 * otherwise.  This is used to determine chunk's length
 */


/*
 * put a fixed-size 32-bit number
 */
void
smf_put32(struct smf *o, unsigned *used, unsigned val)
{
	unsigned char buf[4];
	if (used) {
		(*used) += 4;
	} else {
		if (o->index + 4 > o->length) {
			log_puts("smf_put32: bad chunk length\n");
			panic();
		}
		buf[0] = (val >> 24) & 0xff;
		buf[1] = (val >> 16) & 0xff;
		buf[2] = (val >> 8) & 0xff;
		buf[3] = val & 0xff;
		fwrite(buf, 1, 4, o->file);
		o->index += 4;
	}
}

/*
 * put a fixed-size 24-bit number
 */
void
smf_put24(struct smf *o, unsigned *used, unsigned val)
{
	unsigned char buf[4];
	if (used) {
		(*used) += 3;
	} else {
		if (o->index + 3 > o->length) {
			log_puts("smf_put24: bad chunk length\n");
			panic();
		}
		buf[0] = (val >> 16) & 0xff;
		buf[1] = (val >> 8) & 0xff;
		buf[2] = val & 0xff;
		fwrite(buf, 1, 3, o->file);
		o->index += 3;
	}
}

/*
 * put a fixed-size 16-bit number
 */
void
smf_put16(struct smf *o, unsigned *used, unsigned val)
{
	unsigned char buf[4];
	if (used) {
		(*used) += 2;
	} else {
		if (o->index + 2 > o->length) {
			log_puts("smf_put16: bad chunk length\n");
			panic();
		}
		buf[0] = (val >> 8) & 0xff;
		buf[1] = val & 0xff;
		fwrite(buf, 1, 2, o->file);
		o->index += 2;
	}
}


/*
 * put a fixed-size 8-bit number
 */
void
smf_putc(struct smf *o, unsigned *used, unsigned val)
{
	if (used) {
		(*used) ++;
	} else {
		if (o->index + 1 > o->length) {
			log_puts("smf_putc: bad chunk length\n");
			panic();
		}
		fputc((int)val & 0xff, o->file);
		o->index++;
	}
}


/*
 * put a variable length number
 */
void
smf_putvar(struct smf *o, unsigned *used, unsigned val)
{
#define MAXBYTES 5			/* 32bit / 7bit = 4bytes + 4bit */
	unsigned char buf[MAXBYTES];
	unsigned index = 0, bits;
	for (bits = 7; bits < MAXBYTES * 7; bits += 7) {
		if (val < (1U << bits)) {
			bits -= 7;
			for (; bits != 0; bits -= 7) {
				buf[index++] = ((val >> bits) & 0x7f) | 0x80;
			}
			buf[index++] = val & 0x7f;
			if (used) {
				(*used) += index;
			} else {
				if (o->index + index > o->length) {
					log_puts("smf_putvar: bad chunk length\n");
					panic();
				}
				o->index += index;
				fwrite(buf, 1, index, o->file);
			}
			return;
		}
	}
	log_puts("smf_putvar: bogus value\n");
	panic();
#undef MAXBYTES
}

/*
 * put the given chunk header with the given magic and size field
 */
void
smf_putheader(struct smf *o, char *hdr, unsigned len)
{
	fwrite(hdr, 1, 4, o->file);
	o->length = 4;
	o->index = 0;
	smf_put32(o, NULL, len);
	o->length = len;
	o->index = 0;
}

/*
 * store a track in the smf
 */
void
smf_puttrack(struct smf *o, unsigned *used, struct song *s, struct track *t)
{
	struct seqev *pos;
	unsigned status, newstatus, delta, chan, denom;
	struct ev rev[CONV_NUMREV];
	struct statelist slist;
	unsigned i, nev;


	statelist_init(&slist);
	delta = 0;
	status = 0;
	for (pos = t->first; pos != NULL; pos = pos->next) {
		delta += pos->delta;
		if (pos->ev.cmd == EV_NULL) {
			break;
		}
		if (EV_ISVOICE(&pos->ev)) {
			nev = conv_unpackev(&slist, 0U,
			    CONV_XPC | CONV_NRPN | CONV_RPN, &pos->ev, rev);
			for (i = 0; i < nev; i++) {
				smf_putvar(o, used, delta);
				delta = 0;
				chan = rev[i].ch;
				newstatus = (rev[i].cmd << 4) + (chan & 0x0f);
				if (newstatus != status) {
					status = newstatus;
					smf_putc(o, used, status);
				}
				if (rev[i].cmd == EV_BEND) {
					smf_putc(o, used, rev[i].bend_val & 0x7f);
					smf_putc(o, used, rev[i].bend_val >> 7);
				} else {
					smf_putc(o, used, rev[i].v0);
					if (SMF_EVLEN(status) == 2) {
						smf_putc(o, used, rev[i].v1);
					}
				}
			}
		} else if (pos->ev.cmd == EV_TEMPO) {
			smf_putvar(o, used, delta);
			delta = 0;
			smf_putc(o, used, 0xff);
			smf_putc(o, used, 0x51);
			smf_putc(o, used, 0x03);
			smf_put24(o, used, pos->ev.tempo_usec24 * s->tics_per_unit / 96);
		} else if (pos->ev.cmd == EV_TIMESIG) {
			denom = s->tics_per_unit / pos->ev.timesig_tics;
			switch(denom) {
			case 1:
				denom = 0;
				break;
			case 2:
				denom = 1;
				break;
			case 4:
				denom = 2;
				break;
			case 8:
				denom = 3;
				break;
			case 16:
				denom = 4;
				break;
			default:
				log_puts("smf_puttrack: bad time signature\n");
				panic();
			}
			smf_putvar(o, used, delta);
			delta = 0;
			smf_putc(o, used, 0xff);
			smf_putc(o, used, 0x58);
			smf_putc(o, used, 0x04);
			smf_putc(o, used, pos->ev.timesig_beats);
			smf_putc(o, used, denom);
			/* metronome tics per metro beat */
			smf_putc(o, used, pos->ev.timesig_tics);
			/* metronome 1/32 notes per 24 tics */
			smf_putc(o, used, 8 * s->tics_per_unit / 96);
		}

	}
	smf_putvar(o, used, delta);
	smf_putc(o, used, 0xff);
	smf_putc(o, used, 0x2f);
	smf_putc(o, used, 0x00);
	statelist_done(&slist);
}

/*
 * store a sysex in the smf
 */
void
smf_putsysex(struct smf *o, unsigned *used, struct sysex *sx)
{
	struct chunk *c;
	unsigned i, first;

	first = 1;
	for (c = sx->first; c != NULL; c = c->next) {
		for (i = 0; i < c->used; i++) {
			if (first) {
				first = 0;
			} else {
				smf_putc(o, used, c->data[i]);
			}
		}
	}
}

/*
 * store a sysex back in the smf
 */
void
smf_putsx(struct smf *o, unsigned *used, struct song *s, struct songsx *songsx)
{
	unsigned sysexused;
	struct sysex *sx;

	for (sx = songsx->sx.first; sx != NULL; sx = sx->next) {
		sysexused = 0;
		smf_putvar(o, used, 0);
		smf_putc(o, used, 0xf0);
		smf_putsysex(o, &sysexused, sx);
		smf_putvar(o, used, sysexused);
		smf_putsysex(o, used, sx);
	}
	smf_putvar(o, used, 0);
	smf_putc(o, used, 0xff);
	smf_putc(o, used, 0x2f);
	smf_putc(o, used, 0x00);
}

/*
 * open the smf and store the whole song
 */
unsigned
song_exportsmf(struct song *o, char *filename)
{
	struct smf f;
	struct songtrk *t;
	struct songchan *i;
	struct songsx *s;
	unsigned ntrks, nchan, nsx, used;

	if (!smf_open(&f, filename, "w")) {
		return 0;
	}
	ntrks = 0;
	SONG_FOREACH_TRK(o, t) {
		ntrks++;
	}
	nchan = 0;
	SONG_FOREACH_CHAN(o, i) {
		if (i->isinput)
			continue;
		nchan++;
	}
	nsx = 0;
	SONG_FOREACH_SX(o, s) {
		nsx++;
	}

	/*
	 * write the header
	 */
	smf_putheader(&f, smftype_header, 6);
	smf_put16(&f, NULL, 1);				/* format = 1 */
	smf_put16(&f, NULL, nsx + ntrks + nchan + 1);	/* +1 -> meta track */
	smf_put16(&f, NULL, o->tics_per_unit / 4);		/* tics per quarter */

	/*
	 * write the tempo track
	 */
	used = 0;
	smf_puttrack(&f, &used, o, &o->meta);
	smf_putheader(&f, smftype_track, used);
	smf_puttrack(&f, NULL, o, &o->meta);

	/*
	 * write each sx
	 */
	SONG_FOREACH_SX(o, s) {
		used = 0;
		smf_putsx(&f, &used, o, s);
		smf_putheader(&f, smftype_track, used);
		smf_putsx(&f, NULL, o, s);
	}

	/*
	 * write each chan
	 */
	SONG_FOREACH_CHAN(o, i) {
		if (i->isinput)
			continue;
		used = 0;
		smf_puttrack(&f, &used, o, &i->conf);
		smf_putheader(&f, smftype_track, used);
		smf_puttrack(&f, NULL, o, &i->conf);
	}

	/*
	 * write each track
	 */
	SONG_FOREACH_TRK(o, t) {
		used = 0;
		smf_puttrack(&f, &used, o, &t->track);
		smf_putheader(&f, smftype_track, used);
		smf_puttrack(&f, NULL, o, &t->track);
	}
	smf_close(&f);
	return 1;
}

/*
 * parse '0xF0 varlen data data ... data 0xF7'
 */
unsigned
smf_getsysex(struct smf *o, struct sysex *sx)
{
	unsigned i, length, c;

	if (!smf_getvar(o, &length)) {
		return 0;
	}
	for (i = 0; i < length; i++) {
		if (!smf_getc(o, &c)) {
			return 0;
		}
		sysex_add(sx, c);
	}
	return 1;
}

/*
 * parse a track 'varlen event varlen event ... varlen event'
 */
unsigned
smf_gettrack(struct smf *o, struct song *s, struct songtrk *t)
{
	unsigned delta, i, status, type, length;
	unsigned tempo, num, den, dummy;
	struct statelist slist;
	struct songsx *songsx;
	struct seqev *pos, *se;
	struct sysex *sx;
	struct mididev *dev;
	struct ev ev, rev;
	unsigned xctlset, evset;
	unsigned c;

	if (!smf_getheader(o, smftype_track)) {
		return 0;
	}
	status = 0;
	track_clear(&t->track);
	pos = t->track.first;
	songsx = (struct songsx *)s->sxlist;	/* first (and unique) sysex in song */
	if (songsx == NULL) {
		songsx = song_sxnew(s, "smf");
	}
	statelist_init(&slist);
	for (;;) {
		if (o->index >= o->length) {
			statelist_done(&slist);
			return 1;
		}
		if (!smf_getvar(o, &delta)) {
			goto err;
		}
		pos->delta += delta;
		if (!smf_getc(o, &c)) {
			goto err;;
		}
		if (c == 0xff) {
			status = 0;
			if (!smf_getc(o, &c)) {
				goto err;
			}
			type = c;
			if (!smf_getvar(o, &length)) {
				goto err;
			}
			if (type == 0x2f && length == 0) {
				/* end of track */
				goto ignoremeta;
			} else if (type == 0x51 && length == 3) {
				/* tempo change */
				if (!smf_get24(o, &tempo)) {
					goto err;
				}
				ev.cmd = EV_TEMPO;
				ev.tempo_usec24 = tempo * 96 / s->tics_per_unit;
				goto putev;
			} else if (type == 0x58 && length == 4) {
				/* time signature change */
				if (!smf_getc(o, &num)) {
					goto err;
				}
				if (!smf_getc(o, &den)) {
					goto err;
				}
				ev.cmd = EV_TIMESIG;
				ev.timesig_beats = num;
				ev.timesig_tics = s->tics_per_unit / (1 << den);
				if (!smf_getc(o, &dummy)) {
					goto err;
				}
				/*
				log_puts("cc=");
				log_putu(dummy);
				log_puts(", dd=");
				*/
				if (!smf_getc(o, &dummy)) {
					goto err;
				}
				/*
				log_putu(dummy);
				log_puts("\n");
				*/
				goto putev;
			} else {
			ignoremeta:
				for (i = 0; i < length; i++) {
					if (!smf_getc(o, &c)) {
						goto err;
					}
				}
			}
		} else if (c == 0xf7) {
			/* raw data */
			cons_err("raw data (status = 0xF7) not implemented");
			status = 0;
			if (!smf_getvar(o, &length)) {
				goto err;
			}
			for (i = 0; i < length; i++) {
				if (!smf_getc(o, &c)) {
					goto err;
				}
			}
		} else if (c == 0xf0) {
			/* sys ex */
			status = 0;
			sx = sysex_new(0);
			sysex_add(sx, 0xf0);
			if (!smf_getsysex(o, sx)) {
				sysex_del(sx);
				goto err;
			}
			if (sysex_check(sx)) {
				sysexlist_put(&songsx->sx, sx);
			} else {
				cons_err("corrupted sysex message, ignored");
				/*
				sysex_log(sx);
				log_puts("\n");
				*/
				sysex_del(sx);
			}
		} else if (c >= 0x80 && c < 0xf0) {
			status = c;
			if (!smf_getc(o, &c)) {
				goto err;
			}
		runningstatus:
			ev.cmd = (status >> 4) & 0xf;
			ev.dev = 0;
			ev.ch = status & 0xf;
			ev.v0 = c & 0x7f;
			if (SMF_EVLEN(status) == 2) {
				if (!smf_getc(o, &c)) {
					goto err;
				}
				c &= 0x7f;
				if (ev.cmd == EV_BEND) {
					ev.v0 += c << 7;
				} else {
					ev.v1 = c;
				}
			}
		putev:
			if (ev.cmd == EV_NON && ev.note_vel == 0) {
				ev.cmd = EV_NOFF;
				ev.note_vel = EV_NOFF_DEFAULTVEL;
			}

			/*
			 * Pack events according to device setup.
			 *
			 * XXX: this needs to be done in
			 * doevset/doxctl by converting the whole
			 * project whenever events configuration
			 * is changed.
			 */
			if ((evinfo[ev.cmd].flags & EV_HAS_DEV) &&
			    (dev = mididev_byunit[ev.dev]) != NULL) {
				xctlset = dev->oxctlset;
				evset = dev->oevset;
			} else {
				xctlset = 0;
				evset = CONV_XPC | CONV_NRPN | CONV_RPN;
			}
			if (conv_packev(&slist, xctlset, evset,
				&ev, &rev)) {
				se = seqev_new();
				se->ev = rev;
				seqev_ins(pos, se);
			}
		} else if (c < 0x80) {
			if (status == 0) {
				cons_err("bad status");
				goto err;
			}
			goto runningstatus;
		} else {
			cons_err("bad event");
			goto err;
		}
	}
 err:
	statelist_done(&slist);
	return 0;
}

/*
 * fix song imported from format 1 smf
 */
void
song_fix1(struct song *o)
{
	struct track copy;
	struct seqptr *tp, *cp;
	struct state *st;
	struct statelist slist;
	struct songtrk *t, *tnext;
	unsigned delta;

	SONG_FOREACH_TRK(o, t) {
		/*
		 * move meta events into meta track
		 */
		delta = 0;
		track_init(&copy);
		cp = seqptr_new(&copy);
		tp = seqptr_new(&t->track);
		statelist_init(&slist);
		for (;;) {
			delta = seqptr_ticdel(tp, ~0U, &slist);
			seqptr_ticput(tp, delta);
			seqptr_ticput(cp, delta);
			st = seqptr_evdel(tp, &slist);
			if (st == NULL) {
				break;
			}
			if (st->phase & EV_PHASE_FIRST) {
				st->tag = EV_ISMETA(&st->ev) ? 1 : 0;
			}
			if (st->tag) {
				seqptr_evput(cp, &st->ev);
			} else {
				seqptr_evput(tp, &st->ev);
			}
		}
		statelist_done(&slist);
		seqptr_del(tp);
		seqptr_del(cp);
		track_merge(&o->meta, &copy);
		track_done(&copy);
	}

	/*
	 * remove the first track, if there are not events
	 */
	for (t = (struct songtrk *)o->trklist; t != NULL; t = tnext) {
		tnext = (struct songtrk *)t->name.next;
		if (t->track.first->ev.cmd == EV_NULL) {
			song_trkdel(o, t);
		}
	}
}

/*
 * fix song imported from format 0 SMFs: call fix1 to move meta events
 * to the meta-track then split it creating one track per channel
 */
void
song_fix0(struct song *o)
{
	char trackname[MAXTRACKNAME];
	struct seqptr *tp, *cp;
	struct state *st;
	struct statelist slist;
	struct songtrk *t, *smf;
	unsigned delta;
	unsigned i;

	song_fix1(o);
	smf = (struct songtrk *)o->trklist;
	if (smf == NULL) {
		return;
	}
	for (i = 1; i < 16; i++) {
		snprintf(trackname, MAXTRACKNAME, "trk%02u", i);
		t = song_trknew(o, trackname);
		delta = 0;
		cp = seqptr_new(&t->track);
		tp = seqptr_new(&smf->track);
		statelist_init(&slist);
		for (;;) {
			delta = seqptr_ticdel(tp, ~0U, &slist);
			seqptr_ticput(tp, delta);
			seqptr_ticput(cp, delta);
			st = seqptr_evdel(tp, &slist);
			if (st == NULL) {
				break;
			}
			if (st->phase & EV_PHASE_FIRST) {
				if (EV_ISVOICE(&st->ev) &&
				    st->ev.ch == i) {
					st->tag = 1;
				} else {
					st->tag = 0;
				}
			}
			if (st->tag) {
				seqptr_evput(cp, &st->ev);
			} else {
				seqptr_evput(tp, &st->ev);
			}
		}
		statelist_done(&slist);
		seqptr_del(tp);
		seqptr_del(cp);
	}
}

/*
 * open the smf and load the whole file
 */
struct song *
song_importsmf(char *filename)
{
	struct song *o;
	struct songtrk *t;
	unsigned format, ntrks, timecode, i;
	char trackname[MAXTRACKNAME];
	struct smf f;

	if (!smf_open(&f, filename, "r")) {
		goto bad1;
	}
	if (!smf_getheader(&f, smftype_header)) {
		goto bad2;
	}
	if (!smf_get16(&f, &format)) {
		goto bad2;
	}
	if (format != 1 && format != 0) {
		cons_err("only smf format 0 or 1 can be imported");
		goto bad2;
	}
	if (!smf_get16(&f, &ntrks)) {
		goto bad2;
	}
	if (ntrks >= 256) {
		cons_err("too many tracks in midi file");
		goto bad2;
	}
	if (!smf_get16(&f, &timecode)) {
		goto bad2;
	}
	if ((timecode & 0x8000) != 0) {
		cons_err("SMPTE timecode is not supported");
		goto bad2;
	}

	o = song_new();
	o->tics_per_unit = timecode * 4;   /* timecode = tics per quarter */

	for (i = 0; i < ntrks; i++) {
		snprintf(trackname, MAXTRACKNAME, "trk%02u", i);
		t = song_trknew(o, trackname);
		if (!smf_gettrack(&f, o, t)) {
			goto bad3;
		}
		track_check(&t->track);
	}
	smf_close(&f);

	if (format == 0) {
		song_fix0(o);
	} else if (format == 1) {
		song_fix1(o);
	}


	/*
	 * TODO: move sysex messages into separate songsx
	 */
	return o;

bad3:	song_delete(o);
bad2:	smf_close(&f);
bad1:	return 0;
}

int
syx_import(char *path, struct sysexlist *l, int unit)
{
	FILE *f;
	struct sysex *sx = NULL;
	int c;

	f = fopen(path, "r");
	if (f == NULL) {
		cons_errs(path, "failed to open file");
		return 0;
	}
	sysexlist_clear(l);
	for (;;) {
		c = fgetc(f);
		if (c == EOF)
			break;
		if (c == 0xf0)
			sx = sysex_new(unit);
		if (sx == NULL) {
			cons_errs(path, "corrupted .syx file");
			goto err;
		}
		sysex_add(sx, c);
		if (c == 0xf7) {
			if (sysex_check(sx)) {
				sysexlist_put(l, sx);
				sx = NULL;
				continue;
			} else {
				cons_err("corrupted sysex message, ignored");
				sysex_del(sx);
				goto err;
			}
		}
	}
	fclose(f);
	return 1;
err:
	sysexlist_clear(l);
	fclose(f);
	return 0;
}

int
syx_export(char *path, struct sysexlist *l)
{
	FILE *f;
	struct sysex *x;
	struct chunk *c;
	ssize_t n;

	f = fopen(path, "w");
	if (f == NULL) {
		cons_errs(path, "failed to open file");
		return 0;
	}
	for (x = l->first; x != NULL; x = x->next) {
		for (c = x->first; c != NULL; c = c->next) {
			n = fwrite(c->data, 1, c->used, f);
			if (n != c->used) {
				cons_errs(path, "write failed");
				fclose(f);
				return 0;
			}

		}
	}
	fclose(f);
	return 1;
}
