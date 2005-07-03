/*
 * Copyright (c) 2003-2005 Alexandre Ratchov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * 	- Redistributions of source code must retain the above
 * 	  copyright notice, this list of conditions and the
 * 	  following disclaimer.
 *
 * 	- Redistributions in binary form must reproduce the above
 * 	  copyright notice, this list of conditions and the
 * 	  following disclaimer in the documentation and/or other
 * 	  materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "dbg.h"
#include "track.h"
#include "trackop.h"
#include "song.h"
#include "smf.h"
#include "cons.h"

char smftype_header[4] = { 'M', 'T', 'h', 'd' };
char smftype_track[4]  = { 'M', 'T', 'r', 'k' };

unsigned smf_evlen[] = { 2, 2, 2, 2, 1, 1, 2, 0 };
#define SMF_EVLEN(status) (smf_evlen[((status) >> 4) & 7])

/* --------------------------------------------- chunk read/write --- */

struct smf_s {
	FILE *file;
	unsigned length, index;		/* current chunk length/position */
};

	/*
	 * open a standard midi file and initialise
	 * the smf_s structure
	 */

unsigned
smf_open(struct smf_s *o, char *path, char *mode) {
	o->file = fopen(path, mode);
	if (!o->file) {
		cons_errs(path, "failed to open file");
		return 0;
	}
	o->length = 0;
	o->index = 0;
	return 1;	
}

void
smf_close(struct smf_s *o) {
	fclose(o->file);
}

unsigned
smf_get32(struct smf_s *o, unsigned *val) {
	unsigned char buf[4];
	if (o->index + 4 > o->length || fread(buf, 1, 4, o->file) != 4) {
		cons_err("failed to read 32bit number");
		return 0;
	}
	*val = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
	o->index += 4;
	return 1;
}

unsigned
smf_get24(struct smf_s *o, unsigned *val) {
	unsigned char buf[4];
	if (o->index + 3 > o->length || fread(buf, 1, 3, o->file) != 3) {
		cons_err("failed to read 24bit number");
		return 0;
	}
	*val = (buf[0] << 16) + (buf[1] << 8) + buf[2];
	o->index += 3;
	return 1;
}

unsigned
smf_get16(struct smf_s *o, unsigned *val) {
	unsigned char buf[4];
	if (o->index + 2 > o->length || fread(buf, 1, 2, o->file) != 2) {
		cons_err("failed to read 16bit number");
		return 0;
	}
	*val =  (buf[0] << 8) + buf[1];
	o->index += 2;
	return 1;
}

unsigned
smf_getc(struct smf_s *o, unsigned *c) {
	if (o->index + 1 > o->length || (*c = fgetc(o->file) & 0xff) < 0) {
		cons_err("failed to read one byte");
		return 0;
	}
	o->index++;
	return 1;
}

unsigned
smf_getvar(struct smf_s *o, unsigned *val) {
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

		
unsigned
smf_getheader(struct smf_s *o, char *hdr) {
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
	 * in smf_put* routines there is a 'unsigned *used' argument
	 * if (used == NULL) then data is not written in the smf_s
	 * structure, only *user is incremented by the number
	 * of bytes that would be written otherwise.
	 * This is used to determine chunk's length
	 */


void
smf_put32(struct smf_s *o, unsigned *used, unsigned val) {
	unsigned char buf[4];
	if (used) {
		(*used) += 4;
	} else {
		if (o->index + 4 > o->length) {
			dbg_puts("smf_put32: bad chunk length\n");
			dbg_panic();
		}
		buf[0] = (val >> 24) & 0xff;
		buf[1] = (val >> 16) & 0xff;
		buf[2] = (val >> 8) & 0xff;
		buf[3] = val & 0xff;
		fwrite(buf, 1, 4, o->file);
		o->index += 4;
	}
}

void
smf_put24(struct smf_s *o, unsigned *used, unsigned val) {
	unsigned char buf[4];
	if (used) {
		(*used) += 3;
	} else {
		if (o->index + 3 > o->length) {
			dbg_puts("smf_put24: bad chunk length\n");
			dbg_panic();
		}
		buf[0] = (val >> 16) & 0xff;
		buf[1] = (val >> 8) & 0xff;
		buf[2] = val & 0xff;
		fwrite(buf, 1, 3, o->file);
		o->index += 3;
	}
}

void
smf_put16(struct smf_s *o, unsigned *used, unsigned val) {
	unsigned char buf[4];
	if (used) {
		(*used) += 2;
	} else {
		if (o->index + 2 > o->length) {
			dbg_puts("smf_put16: bad chunk length\n");
			dbg_panic();
		}
		buf[0] = (val >> 8) & 0xff;
		buf[1] = val & 0xff;
		fwrite(buf, 1, 2, o->file);
		o->index += 2;
	}
}


void
smf_putc(struct smf_s *o, unsigned *used, unsigned val) {
	if (used) {
		(*used) ++;
	} else {
		if (o->index + 1 > o->length) {
			dbg_puts("smf_putc: bad chunk length\n");
			dbg_panic();
		}
		fputc(val & 0xff, o->file);
		o->index++;
	}
}


void
smf_putvar(struct smf_s *o, unsigned *used, unsigned val) {
#define MAXBYTES (5)			/* 32bit / 7bit = 4bytes + 4bit */
	unsigned char buf[MAXBYTES];
	unsigned index = 0, bits;
	for (bits = 7; bits < MAXBYTES * 7; bits += 7) {
		if (val < (1 << bits)) {
			bits -= 7;
			for (; bits != 0; bits -= 7) {
				buf[index++] = ((val >> bits) & 0x7f) | 0x80;
			}
			buf[index++] = val & 0x7f;
			if (used) {
				(*used) += index;
			} else {
				if (o->index + index > o->length) {
					dbg_puts("smf_putvar: bad chunk length\n");
					dbg_panic();
				}
				o->index += index;
				fwrite(buf, 1, index, o->file);
			}
			return;
		}
	}
	dbg_puts("smf_putvar: bogus value\n");
	dbg_panic();
#undef MAXBYTES
}

void
smf_putheader(struct smf_s *o, char *hdr, unsigned len) {
	fwrite(hdr, 1, 4, o->file);
	o->length = 4;
	o->index = 0;
	smf_put32(o, 0, len);
	o->length = len;
	o->index = 0;
}

/* ------------------------------------------------------- export --- */

void
smf_putmeta(struct smf_s *o, unsigned *used, struct song_s *s) {
	struct seqptr_s tp;
	unsigned delta, denom;
	
	delta = s->tics_per_unit;
	track_rew(&s->meta, &tp);
	for (;;) {
		delta += track_ticlast(&s->meta, &tp);
		if (!track_evavail(&s->meta, &tp)) {
			break;
		}
		if ((*tp.pos)->ev.cmd == EV_TEMPO) {
			smf_putvar(o, used, delta);
			delta = 0;
			smf_putc(o, used, 0xff);
			smf_putc(o, used, 0x51);
			smf_putc(o, used, 0x03);
			smf_put24(o, used, (*tp.pos)->ev.data.tempo.usec24 * s->tics_per_unit / 96);
		} else if ((*tp.pos)->ev.cmd == EV_TIMESIG) {
			denom = (*tp.pos)->ev.data.sign.tics / s->tics_per_beat;
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
				/* XXX: should ignore event */
				dbg_puts("bad time signature\n");
				dbg_panic();
			}
			smf_putvar(o, used, delta);
			delta = 0;
			smf_putc(o, used, 0xff);
			smf_putc(o, used, 0x58);
			smf_putc(o, used, 0x04);
			smf_putc(o, used, (*tp.pos)->ev.data.sign.beats);
			smf_putc(o, used, denom);
			/* XXX: do as other sequencers */
			smf_putc(o, used, 24);
			smf_putc(o, used, 8);
		}

		track_evnext(&s->meta, &tp);
	}
	smf_putvar(o, used, delta);
	smf_putc(o, used, 0xff);
	smf_putc(o, used, 0x2f);
	smf_putc(o, used, 0x00);
	return;
}

void
smf_puttrk(struct smf_s *o, unsigned *used, struct song_s *s, struct songtrk_s *t) {
	struct seqptr_s tp;
	unsigned status, newstatus, delta, chan;
	
	status = 0;
	delta = s->tics_per_unit;
	track_rew(&t->track, &tp);
	for (;;) {
		delta += track_ticlast(&t->track, &tp);
		if (!track_evavail(&t->track, &tp)) {
			break;
		}
		if (EV_ISVOICE(&(*tp.pos)->ev)) {
			smf_putvar(o, used, delta);
			delta = 0;
			chan = (*tp.pos)->ev.data.voice.ch;
			newstatus = ((*tp.pos)->ev.cmd << 4) + (chan & 0x0f);
			if (newstatus != status) {
				status = newstatus;
				smf_putc(o, used, status);
			}
			smf_putc(o, used, (*tp.pos)->ev.data.voice.b0);
			if (SMF_EVLEN(status) == 2) {
				smf_putc(o, used, (*tp.pos)->ev.data.voice.b1);
			}
		}
		track_evnext(&t->track, &tp);
	}
	smf_putvar(o, used, delta);
	smf_putc(o, used, 0xff);
	smf_putc(o, used, 0x2f);
	smf_putc(o, used, 0x00);
	return;
}


void
smf_putchan(struct smf_s *o, unsigned *used, struct song_s *s, struct songchan_s *i) {
	struct seqptr_s tp;
	unsigned status, newstatus, delta;
	
	status = 0;
	delta = 0;
	track_rew(&i->conf, &tp);
	for (;;) {
		delta += track_ticlast(&i->conf, &tp);
		if (!track_evavail(&i->conf, &tp)) {
			break;
		}
		if (EV_ISVOICE(&(*tp.pos)->ev)) {
			smf_putvar(o, used, delta);
			delta = 0;
			newstatus = ((*tp.pos)->ev.cmd << 4) + (i->ch & 0x0f);
			if (newstatus != status) {
				status = newstatus;
				smf_putc(o, used, status);
			}
			smf_putc(o, used, (*tp.pos)->ev.data.voice.b0);
			if (SMF_EVLEN(status) == 2) {
				smf_putc(o, used, (*tp.pos)->ev.data.voice.b1);
			}
		}
		track_evnext(&i->conf, &tp);
	}
	smf_putvar(o, used, delta);
	smf_putc(o, used, 0xff);
	smf_putc(o, used, 0x2f);
	smf_putc(o, used, 0x00);
	return;
}


void
smf_putsysex(struct smf_s *o, unsigned *used, struct sysex_s *sx) {
	struct chunk_s *c;
	unsigned i;
		
	for (c = sx->first; c != 0; c = c->next) {
		for (i = 0; i < c->used; i++) {
			smf_putc(o, used, c->data[i]);
		}
	}
}

void
smf_putsx(struct smf_s *o, unsigned *used, struct song_s *s, struct songsx_s *songsx) {
	unsigned sysexused;
	struct sysex_s *sx;
	
	for (sx = songsx->sx.first; sx != 0; sx = sx->next) {
		sysexused = 0;
		smf_putvar(o, used, 0);
		smf_putc(o, used, 0xf7);
		smf_putsysex(o, &sysexused, sx);
		smf_putvar(o, used, sysexused);
		smf_putsysex(o, used, sx);
	}
	smf_putvar(o, used, 0);
	smf_putc(o, used, 0xff);
	smf_putc(o, used, 0x2f);
	smf_putc(o, used, 0x00);		
}

unsigned
song_exportsmf(struct song_s *o, char *filename) {
	struct smf_s f;
	struct songtrk_s *t;
	struct songchan_s *i;
	struct songsx_s *s;
	unsigned ntrks, nchan, nsx, used;

	if (!smf_open(&f, filename, "w")) {
		return 0;
	}
	ntrks = 0;
	for (t = o->trklist; t != 0; t = (struct songtrk_s *)t->name.next) {
		ntrks++;
	}
	nchan = 0;
	for (i = o->chanlist; i != 0; i = (struct songchan_s *)i->name.next) {
		nchan++;
	}
	nsx = 0;
	for (s = o->sxlist; s != 0; s = (struct songsx_s *)s->name.next) {
		nsx++;
	}

	/* write the header */
	smf_putheader(&f, smftype_header, 6);
	smf_put16(&f, 0, 1);				/* format = 1 */
	smf_put16(&f, 0, nsx + ntrks + nchan + 1);	/* +1 -> meta track */
	smf_put16(&f, 0, o->tics_per_unit / 4);		/* tics per quarter */

	/* write the tempo track */
	used = 0;
	smf_putmeta(&f, &used, o);
	smf_putheader(&f, smftype_track, used);
	smf_putmeta(&f, 0, o);
		
	/* write each sx */
	for (s = o->sxlist; s != 0; s = (struct songsx_s *)s->name.next) {
		used = 0;
		smf_putsx(&f, &used, o, s);
		smf_putheader(&f, smftype_track, used);
		smf_putsx(&f, 0, o, s);
	}	
					
	/* write each chan */
	for (i = o->chanlist; i != 0; i = (struct songchan_s *)i->name.next) {
		used = 0;
		smf_putchan(&f, &used, o, i);
		smf_putheader(&f, smftype_track, used);
		smf_putchan(&f, 0, o, i);
	}

	/* write each track */
	for (t = o->trklist; t != 0; t = (struct songtrk_s *)t->name.next) {
		used = 0;
		smf_puttrk(&f, &used, o, t);
		smf_putheader(&f, smftype_track, used);
		smf_puttrk(&f, 0, o, t);
	}
	smf_close(&f);
	return 1;
}

/* ------------------------------------------------------- import --- */


unsigned
smf_gettrack(struct smf_s *o, struct song_s *s, struct songtrk_s *t) {
	unsigned delta, i, status, type, length, abspos;
	unsigned tempo, num, den, dummy;
	struct seqptr_s tp;
	struct ev_s ev;
	unsigned c;
	
	if (!smf_getheader(o, smftype_track)) {
		return 0;
	}
	status = 0;
	abspos = 0;
	track_clear(&t->track, &tp);
	for (;;) {
		if (o->index >= o->length) {
			return 1;
		}
		if (!smf_getvar(o, &delta)) {
			return 0;
		}
		abspos += delta;
		track_seekblank(&t->track, &tp, delta);
		if (!smf_getc(o, &c)) {
			return 0;
		}
		if (c == 0xff) {
			status = 0;
			if (!smf_getc(o, &c)) {
				return 0;
			}
			type = c;
			if (!smf_getvar(o, &length)) {
				return 0;
			}
			if (type == 0x2f && length == 0) {
				/* end of track */
				goto ignoremeta;
			} else if (type == 0x51 && length == 3) {
				/* tempo change */
				if (!smf_get24(o, &tempo)) {
					return 0;
				}
				ev.cmd = EV_TEMPO;
				ev.data.tempo.usec24 = tempo * 96 / s->tics_per_unit;
				goto putev;
			} else if (type == 0x58 && length == 4) {
				/* time signature change */
				if (!smf_getc(o, &num)) {
					return 0;
				}
				if (!smf_getc(o, &den)) {
					return 0;
				}
				ev.cmd = EV_TIMESIG;
				ev.data.sign.beats = num;
				ev.data.sign.tics = s->tics_per_unit / (1 << den);
				if (!smf_getc(o, &dummy)) {
					return 0;
				}
				/*
				dbg_puts("cc=");
				dbg_putu(dummy);
				dbg_puts(", dd=");
				*/
				if (!smf_getc(o, &dummy)) {
					return 0;
				}
				/*
				dbg_putu(dummy);
				dbg_puts("\n");
				*/
				goto putev;
			} else {
			ignoremeta:
				for (i = 0; i < length; i++) {
					if (!smf_getc(o, &c)) {
						return 0;
					}
				}
			}
		} else if (c == 0xf0 || c == 0xf7) {
			/*
			cons_err("0xF0 and 0xF7 event not implemented");
			*/
			status = 0;
			ev.cmd = 0;
			if (!smf_getvar(o, &length)) {
				return 0;
			}
			for (i = 0; i < length; i++) {
				if (!smf_getc(o, &c)) {
					return 0;
				}
			}			
		} else if (c >= 0x80 && c < 0xf0) {
			status = c;
			if (!smf_getc(o, &c)) {
				return 0;
			}
		runningstatus:
			ev.cmd = (status >> 4) & 0xf;
			ev.data.voice.dev = 0;
			ev.data.voice.ch = status & 0xf;
			ev.data.voice.b0 = c & 0x7f;
			if (SMF_EVLEN(status) == 2) {
				if (!smf_getc(o, &c)) {
					return 0;
				}
				ev.data.voice.b1 = c & 0x7f;
			}
		putev:
			if (ev.cmd == EV_NON && ev.data.voice.b1 == 0) {
				ev.cmd = EV_NOFF;
				ev.data.voice.b1 = EV_NOFF_DEFAULTVEL;
			}
			track_evput(&t->track, &tp, &ev);
			/*
			dbg_puts("ev: ");
			dbg_putu(abspos);
			dbg_puts(" ");
			ev_dbg(&ev);
			dbg_puts("\n");			
			*/			
		} else if (c < 0x80) {
			if (status == 0) {
				cons_err("bad status");
				return 0;
			}
			goto runningstatus;
		} else {
			cons_err("bad event");
			return 0;
		}
	}
	return 1;
}

	/*
	 * fix song imported from format 0 smf 
	 */

void
song_fix0(struct song_s *o) {
}

	/*
	 * fix song imported from format 1 smf 
	 */

void
song_fix1(struct song_s *o) {
	struct seqptr_s mp, tp;
	struct seqev_s *se;
	struct songtrk_s *t;
	unsigned delta;
		
	for (t = o->trklist; t != 0; t = (struct songtrk_s *)t->name.next) {
		/* move meta events into meta track */
		delta = 0;
		track_rew(&o->meta, &mp);
		track_rew(&t->track, &tp);
		for (;;) {
			delta = track_ticlast(&t->track, &tp);
			track_seekblank(&o->meta, &mp, delta);
			track_evlast(&o->meta, &mp);
			
			if (!track_evavail(&t->track, &tp)) {
				break;
			}
			if (EV_ISMETA(&(*tp.pos)->ev)) {
				se = track_seqevrm(&t->track, &tp);
				track_seqevins(&o->meta, &mp, se);
				track_seqevnext(&o->meta, &mp);
			} else {
				track_evnext(&t->track, &tp);
			}				
		}
		/* check for inconsistecies */
		track_opcheck(&t->track);
	}
}


struct song_s *
song_importsmf(char *filename) {
	struct song_s *o;
	struct songtrk_s *t;
	unsigned format, ntrks, timecode, i;
	char trackname[100];	
	struct smf_s f;
	
	/*
	cons_err("not implemented");
	return 0;
	*/
	
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
	if (!smf_get16(&f, &timecode)) {
		goto bad2;
	}
	if ((timecode & 0x8000) != 0) {
		cons_err("SMPTE timecode is not supported");
		goto bad2;
	}
	
	o = song_new();
	o->tics_per_unit = timecode * 4;
	
	for (i = 0; i < ntrks; i++) {
		sprintf(trackname, "trk%02u", i);
		t = songtrk_new(trackname);
		song_trkadd(o, t);
		if (!smf_gettrack(&f, o, t)) {
			goto bad3;
		}
	}
	smf_close(&f);
	
	if (format == 0) {
		song_fix0(o);
	} else if (format == 1) {
		song_fix1(o);
	}
	
	return o;
	
bad3:	song_delete(o);
bad2:	smf_close(&f);
bad1:	return 0;
}
