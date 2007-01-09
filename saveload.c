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

#include "dbg.h"
#include "name.h"
#include "song.h"
#include "filt.h"
#include "parse.h"
#include "textio.h"
#include "saveload.h"
#include "trackop.h"

/* ------------------------------------------------------------------- */

void
chan_output(unsigned dev, unsigned ch, struct textout_s *f) {
	textout_putstr(f, "{");
	textout_putlong(f, dev);
	textout_putstr(f, " ");
	textout_putlong(f, ch % 16);
	textout_putstr(f, "}");
}


void
ev_output(struct ev_s *e, struct textout_s *f) {

	/* XXX: use ev_getstr() */

	textout_indent(f);
	switch(e->cmd) {
		case EV_NOFF:
			textout_putstr(f, "noff");
			goto two;
		case EV_NON:
			textout_putstr(f, "non");
			goto two;
		case EV_CAT:
			textout_putstr(f, "cat");
			goto one;
		case EV_CTL:
			textout_putstr(f, "ctl");
			goto two;
		case EV_PC:
			textout_putstr(f, "pc");
			goto one;
		case EV_KAT:
			textout_putstr(f, "kat");
			goto two;
		case EV_BEND:
			textout_putstr(f, "bend");
			goto two;
		case EV_TEMPO:
			textout_putstr(f, "tempo");
			textout_putstr(f, " ");
			textout_putlong(f, e->data.tempo.usec24);
			textout_putstr(f, "\n");
			break;			
		case EV_TIMESIG:
			textout_putstr(f, "timesig");
			textout_putstr(f, " ");
			textout_putlong(f, e->data.sign.beats);
			textout_putstr(f, " ");
			textout_putlong(f, e->data.sign.tics);
			textout_putstr(f, "\n");
			break;			
		default:
			dbg_puts("ignoring event: ");
			ev_dbg(e);
			dbg_puts("\n");
			break;
		}
	return;	
two:
	textout_putstr(f, " ");
	chan_output(e->data.voice.dev, e->data.voice.ch, f);
	textout_putstr(f, " ");
	textout_putlong(f, e->data.voice.b0);
	textout_putstr(f, " ");
	textout_putlong(f, e->data.voice.b1);
	textout_putstr(f, "\n");
	return;
one:	
	textout_putstr(f, " ");
	chan_output(e->data.voice.dev, e->data.voice.ch, f);
	textout_putstr(f, " ");
	textout_putlong(f, e->data.voice.b0);
	textout_putstr(f, "\n");
	return;
}
	

void
track_output(struct track_s *t, struct textout_s *f) {
	unsigned delta;
	struct ev_s ev;
	struct seqptr_s tp;
	
	textout_putstr(f, "{\n");
	textout_shiftright(f);
	
	track_rew(t, &tp);
	for (;;) {
		delta = track_ticlast(t, &tp);
		if (delta != 0) {
			textout_indent(f);
			textout_putlong(f, delta);
			textout_putstr(f, "\n");		
		}
		if (!track_evavail(t, &tp)) {
			break;
		}
		track_evget(t, &tp, &ev);
		ev_output(&ev, f);
	}
	
	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}


void
rule_output(struct rule_s *o, struct textout_s *f) {
	textout_indent(f);
	switch(o->type) {
	case RULE_DEVDROP:
		textout_putstr(f, "devdrop ");		
		textout_putlong(f, o->idev);
		break;
	case RULE_DEVMAP:
		textout_putstr(f, "devmap ");		
		textout_putlong(f, o->idev);
		textout_putstr(f, " ");
		textout_putlong(f, o->odev);
		break;
	case RULE_CHANDROP:
		textout_putstr(f, "chandrop ");		
		chan_output(o->idev, o->ich, f);
		break;
	case RULE_CHANMAP:
		textout_putstr(f, "chanmap ");		
		chan_output(o->idev, o->ich, f);
		textout_putstr(f, " ");
		chan_output(o->odev, o->och, f);
		break;
	case RULE_CTLDROP:
		textout_putstr(f, "ctldrop ");		
		chan_output(o->idev, o->ich, f);
		textout_putstr(f, " ");
		textout_putlong(f, o->ictl);
		break;
	case RULE_CTLMAP:
		textout_putstr(f, "ctlmap ");		
		chan_output(o->idev, o->ich, f);
		textout_putstr(f, " ");
		chan_output(o->odev, o->och, f);
		textout_putstr(f, " ");
		textout_putlong(f, o->ictl);
		textout_putstr(f, " ");
		textout_putlong(f, o->octl);
		textout_putstr(f, " ");
		textout_putstr(f, "id");
		break;
	case RULE_KEYDROP:
		textout_putstr(f, "keydrop ");		
		chan_output(o->idev, o->ich, f);
		textout_putstr(f, " ");
		textout_putlong(f, o->keylo);
		textout_putstr(f, " ");
		textout_putlong(f, o->keyhi);
		break;
	case RULE_KEYMAP:
		textout_putstr(f, "keymap ");		
		chan_output(o->idev, o->ich, f);
		textout_putstr(f, " ");
		chan_output(o->odev, o->och, f);
		textout_putstr(f, " ");
		textout_putlong(f, o->keylo);
		textout_putstr(f, " ");
		textout_putlong(f, o->keyhi);
		textout_putstr(f, " ");
		textout_putlong(f, o->keyplus & 0x7f);
		textout_putstr(f, " ");
		textout_putstr(f, "id");
		break;
	default:
		dbg_puts("rule_output: ignoring unknown rule\n");
	}
	textout_putstr(f, "\n");
}

void
filt_output(struct filt_s *o, struct textout_s *f) {
	struct rule_s *i;
	textout_putstr(f, "{\n");
	textout_shiftright(f);
	
	for (i = o->dev_rules; i != NULL; i = i->next) {
		rule_output(i, f);
	}
	for (i = o->chan_rules; i != NULL; i = i->next) {
		rule_output(i, f);
	}
	for (i = o->voice_rules; i != NULL; i = i->next) {
		rule_output(i, f);
	}
	
	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

void
sysex_output(struct sysex_s *o, struct textout_s *f) {
	struct chunk_s *c;
	unsigned i, col;
	textout_putstr(f, "{\n");
	textout_shiftright(f);
	
	textout_indent(f);	
	textout_putstr(f, "unit ");
	textout_putlong(f, o->unit);
	textout_putstr(f, "\n");
	
	textout_indent(f);	
	textout_putstr(f, "data\t");
	textout_shiftright(f);
	col = 0;
	for (c = o->first; c != NULL; c = c->next) {
		for (i = 0; i < c->used; i++) {
			textout_putbyte(f, c->data[i]);
			if (i + 1 < c->used || c->next != NULL) {
				col++;
				if (col >= 8) {
					col = 0;
					textout_putstr(f, " \\\n");
					textout_indent(f);	
				} else {
					textout_putstr(f, " ");
				}
			}
		}
	}
	textout_putstr(f, "\n");
	textout_shiftleft(f);


	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}


void
songsx_output(struct songsx_s *o, struct textout_s *f) {
	struct sysex_s *i;
	textout_putstr(f, "{\n");
	textout_shiftright(f);
	
	for (i = o->sx.first; i != NULL; i = i->next) {
		textout_indent(f);
		textout_putstr(f, "sysex ");
		sysex_output(i, f);
		textout_putstr(f, "\n");
	}
	
	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}


void
songtrk_output(struct songtrk_s *o, struct textout_s *f) {
	textout_putstr(f, "{\n");
	textout_shiftright(f);

	if (o->curfilt) {
		textout_indent(f);
		textout_putstr(f, "curfilt ");
		textout_putstr(f, o->curfilt->name.str);
		textout_putstr(f, "\n");
	}
	textout_indent(f);
	textout_putstr(f, "mute ");
	textout_putlong(f, o->mute);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "track ");
	track_output(&o->track, f);
	textout_putstr(f, "\n");
	
	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

void
songchan_output(struct songchan_s *o, struct textout_s *f) {
	textout_putstr(f, "{\n");
	textout_shiftright(f);
			
	textout_indent(f);
	textout_putstr(f, "chan ");
	chan_output(o->dev, o->ch, f);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "conf ");
	track_output(&o->conf, f);
	textout_putstr(f, "\n");
	
	textout_indent(f);
	textout_putstr(f, "curinput {");
	textout_putlong(f, o->curinput_dev);
	textout_putstr(f, " ");
	textout_putlong(f, o->curinput_ch);
	textout_putstr(f, "}\n");

	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

void
songfilt_output(struct songfilt_s *o, struct textout_s *f) {
	textout_putstr(f, "{\n");
	textout_shiftright(f);

	if (o->curchan) {
		textout_indent(f);
		textout_putstr(f, "curchan ");
		textout_putstr(f, o->curchan->name.str);
		textout_putstr(f, "\n");
	}

	textout_indent(f);
	textout_putstr(f, "filt ");
	filt_output(&o->filt, f);
	textout_putstr(f, "\n");
	
	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

void
song_output(struct song_s *o, struct textout_s *f) {
	struct songtrk_s *t;
	struct songchan_s *i;
	struct songfilt_s *g;
	struct songsx_s *s;

	textout_putstr(f, "{\n");
	textout_shiftright(f);
	
	textout_indent(f);
	textout_putstr(f, "tics_per_unit ");
	textout_putlong(f, o->tics_per_unit);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "meta ");
	track_output(&o->meta, f);
	textout_putstr(f, "\n");

	for (i = o->chanlist; i != NULL; i = (struct songchan_s *)i->name.next) {
		textout_indent(f);
		textout_putstr(f, "songchan ");
		textout_putstr(f, i->name.str);
		textout_putstr(f, " ");
		songchan_output(i, f);
		textout_putstr(f, "\n");
	}
	for (g = o->filtlist; g != NULL; g = (struct songfilt_s *)g->name.next) {
		textout_indent(f);
		textout_putstr(f, "songfilt ");
		textout_putstr(f, g->name.str);
		textout_putstr(f, " ");
		songfilt_output(g, f);
		textout_putstr(f, "\n");
	}
	for (t = o->trklist; t != NULL; t = (struct songtrk_s *)t->name.next) {
		textout_indent(f);
		textout_putstr(f, "songtrk ");
		textout_putstr(f, t->name.str);
		textout_putstr(f, " ");
		songtrk_output(t, f);
		textout_putstr(f, "\n");
	}
	for (s = o->sxlist; s != NULL; s = (struct songsx_s *)s->name.next) {
		textout_indent(f);
		textout_putstr(f, "songsx ");
		textout_putstr(f, s->name.str);
		textout_putstr(f, " ");
		songsx_output(s, f);
		textout_putstr(f, "\n");
	}
	
	if (o->curtrk) {
		textout_indent(f);
		textout_putstr(f, "curtrk ");
		textout_putstr(f, o->curtrk->name.str);
		textout_putstr(f, "\n");
	}		
	if (o->curfilt) {
		textout_indent(f);
		textout_putstr(f, "curfilt ");
		textout_putstr(f, o->curfilt->name.str);
		textout_putstr(f, "\n");
	}
	if (o->cursx) {
		textout_indent(f);
		textout_putstr(f, "cursx ");
		textout_putstr(f, o->cursx->name.str);
		textout_putstr(f, "\n");
	}
	if (o->curchan) {
		textout_indent(f);
		textout_putstr(f, "curchan ");
		textout_putstr(f, o->curchan->name.str);
		textout_putstr(f, "\n");
	}		
	textout_indent(f);
	textout_putstr(f, "curpos ");
	textout_putlong(f, o->curpos);
	textout_putstr(f, "\n");
	textout_indent(f);
	
	textout_indent(f);
	textout_putstr(f, "curlen ");
	textout_putlong(f, o->curlen);
	textout_putstr(f, "\n");
	textout_indent(f);

	textout_putstr(f, "curquant ");
	textout_putlong(f, o->curquant);
	textout_putstr(f, "\n");
			
	textout_indent(f);
	textout_putstr(f, "curinput {");
	textout_putlong(f, o->curinput_dev);
	textout_putstr(f, " ");
	textout_putlong(f, o->curinput_ch);
	textout_putstr(f, "}\n");

	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

/* ---------------------------------------------------------------------- */

unsigned parse_ukline(struct parse_s *o);
unsigned parse_ukblock(struct parse_s *o);
unsigned parse_delta(struct parse_s *o, unsigned *delta);
unsigned parse_ev(struct parse_s *o, struct ev_s *ev);
unsigned parse_track(struct parse_s *o, struct track_s *t);
unsigned parse_songtrk(struct parse_s *o, struct song_s *s, struct songtrk_s *t);
unsigned parse_song(struct parse_s *o, struct song_s *s);

unsigned
parse_ukline(struct parse_s *o) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT && o->lex.id != TOK_NUM && o->lex.id != TOK_ENDLINE) {
		lex_err(&o->lex, "identifier, number, newline or '}' expected");
		return 0;
	}
	parse_ungetsym(o);
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_RBRACE || o->lex.id == TOK_EOF) {
			parse_ungetsym(o);
			break;
		} else if (o->lex.id == TOK_ENDLINE) {
			break;
		} else if (o->lex.id == TOK_LBRACE) {
			parse_ungetsym(o);
			if (!parse_ukblock(o)) {
				return 0;
			}
		}
	}
	return 1;
}

unsigned
parse_ukblock(struct parse_s *o) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing block");
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_RBRACE) {
			break;
		} else {
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				return 0;
			}
		}
	}
	return 1;			
}

unsigned
parse_nl(struct parse_s *o) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id == TOK_RBRACE || o->lex.id != TOK_EOF) {
		parse_ungetsym(o);
	} else if (o->lex.id != TOK_ENDLINE) {
		lex_err(&o->lex, "new line expected");
		return 0;
	}
	return 1;
}

unsigned
parse_empty(struct parse_s *o) {
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_ENDLINE) {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}


unsigned
parse_long(struct parse_s *o, unsigned long max, unsigned long *data) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_NUM) {
		lex_err(&o->lex, "number expected");
		return 0;
	}
	if (o->lex.longval > max) {
		lex_err(&o->lex, "number to large");
		return 0;
	}
	*data = (unsigned)o->lex.longval;
	return 1;
}

unsigned
parse_delta(struct parse_s *o, unsigned *delta) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_NUM) {
		lex_err(&o->lex, "numeric constant expected in event offset");
		return 0;
	}
	*delta = o->lex.longval;
	if (!parse_nl(o)) {
		return 0;
	}
	return 1;
}

unsigned
parse_chan(struct parse_s *o, unsigned long *dev, unsigned long *ch) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id == TOK_LBRACE) {
		if (!parse_long(o, EV_MAXDEV, dev)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXCH, ch)) {
			return 0;
		}
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_RBRACE) {
			lex_err(&o->lex, "'}' expected in channel spec");
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_NUM) {
		*dev = o->lex.longval / (EV_MAXCH + 1);
		*ch = o->lex.longval % (EV_MAXCH + 1);
		if (*dev > EV_MAXDEV) {
			lex_err(&o->lex, "dev/chan out of range");
			return 0;
		}
		return 1;
	} else {
		lex_err(&o->lex, "bad channel spec");
		return 0;
	}
}


unsigned
parse_ev(struct parse_s *o, struct ev_s *ev) {
	unsigned long val, val2;
	
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT) {
		lex_err(&o->lex, "event name expected");
		return 0;
	}
	if (!ev_str2cmd(ev, o->lex.strval)) {
		goto ignore;
	}
	if (EV_ISVOICE(ev)) {
		if (!parse_chan(o, &val, &val2)) {
			return 0;
		}
		ev->data.voice.dev = val;
		ev->data.voice.ch = val2;
		if (!parse_long(o, EV_MAXB0, &val)) {
			return 0;
		}
		ev->data.voice.b0 = val;
		if (ev->cmd != EV_PC && ev->cmd != EV_CAT) {
			if (ev->cmd == EV_KAT) {
				/*
				 * XXX: midish 0.2.5 used to generate
				 * bogus kat events (without the last
				 * byte.  As workaround, we ignore
				 * such event, in order to allow user
				 * to load its files
				 */
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_NUM) {
					parse_ungetsym(o);
					if (!parse_nl(o)) {
						return 0;
					}
					ev->cmd = EV_NULL;
					return 1;
				}
				parse_ungetsym(o);
			}
			if (!parse_long(o, EV_MAXB1, &val)) {
				return 0;
			}
			ev->data.voice.b1 = val;
		}
	} else if (ev->cmd == EV_TEMPO) {
		if (!parse_long(o, ~1U, &val)) {
			return 0;
		}
		ev->data.tempo.usec24 = val;
	} else if (ev->cmd == EV_TIMESIG) {
		if (!parse_long(o, ~1U, &val)) {
			return 0;
		}
		ev->data.sign.beats = o->lex.longval;
		if (!parse_long(o, ~1U, &val)) {
			return 0;
		}
		ev->data.sign.tics = o->lex.longval;
	} else {
ignore:		parse_ungetsym(o);
		if (!parse_ukline(o)) {
			return 0;
		}
		lex_err(&o->lex, "unknown event, ignored");
		ev->cmd = EV_NULL;
		return 1;
		
	}
	if (!parse_nl(o)) {
		return 0;
	}
	return 1;
}
	


unsigned
parse_track(struct parse_s *o, struct track_s *t) {
	unsigned delta;
	struct ev_s ev;
	struct seqptr_s tp;
	
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing track");
		return 0;
	}
	track_rew(t, &tp);	
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->lex.id == TOK_RBRACE) {
			break;
		} else if (o->lex.id == TOK_NUM) {
			parse_ungetsym(o);
			if (!parse_delta(o, &delta)) {
				return 0;
			}
			delta = o->lex.longval;
			track_seekblank(t, &tp, delta);
		} else {
			parse_ungetsym(o);
			if (!parse_ev(o, &ev)) {
				return 0;
			}
			if (ev.cmd != EV_NULL) {
				track_evlast(t, &tp);
				track_evput(t, &tp, &ev);
			}
		}
	}
	return 1;			
}


unsigned
parse_rule(struct parse_s *o, struct filt_s *f) {
	unsigned long idev, ich, odev, och, ictl, octl, keylo, keyhi, ukeyplus;
	int keyplus;
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT) {
		lex_err(&o->lex, "filter-type identifier expected");
		return 0;
	}
	if (str_eq(o->lex.strval, "keydrop")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &keylo)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &keyhi)) {
			return 0;
		}
		filt_conf_keydrop(f, idev, ich, keylo, keyhi);
	} else if (str_eq(o->lex.strval, "keymap")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!parse_chan(o, &odev, &och)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &keylo)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &keyhi)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &ukeyplus)) {
			return 0;
		}
		if (ukeyplus > 63) {
			keyplus = ukeyplus - 128;
		} else {
			keyplus = ukeyplus;
		}
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_IDENT) {
			lex_err(&o->lex, "curve identifier expected");
			return 0;
		}	
		filt_conf_keymap(f, idev, ich, odev, och, keylo, keyhi, keyplus);
	} else if (str_eq(o->lex.strval, "ctldrop")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &ictl)) {
			return 0;
		}
		filt_conf_ctldrop(f, idev, ich, ictl);
	} else if (str_eq(o->lex.strval, "ctlmap")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!parse_chan(o, &odev, &och)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &ictl)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &octl)) {
			return 0;
		}
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_IDENT) {
			lex_err(&o->lex, "curve identifier expected");
			return 0;
		}	
		filt_conf_ctlmap(f, idev, ich, odev, och, ictl, octl);
	} else if (str_eq(o->lex.strval, "chandrop")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		filt_conf_chandrop(f, idev, ich);
	} else if (str_eq(o->lex.strval, "chanmap")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!parse_chan(o, &odev, &och)) {
			return 0;
		}
		filt_conf_chanmap(f, idev, ich, odev, och);
	} else if (str_eq(o->lex.strval, "devdrop")) {
		if (!parse_long(o, EV_MAXDEV, &idev)) {
			return 0;
		}
		filt_conf_devdrop(f, idev);
	} else if (str_eq(o->lex.strval, "devmap")) {
		if (!parse_long(o, EV_MAXDEV, &idev)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXDEV, &odev)) {
			return 0;
		}
		filt_conf_devmap(f, idev, odev);
	} else {
		parse_ungetsym(o);
		if (!parse_ukline(o)) {
			return 0;
		}
		lex_err(&o->lex, "unknown filter rule, ignored");
		return 1;
	}
	if (!parse_nl(o)) {
		return 0;
	}
	return 1;
}

unsigned
parse_filt(struct parse_s *o, struct filt_s *f) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing filt");
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->lex.id == TOK_RBRACE) {
			break;
		} else {
			parse_ungetsym(o);
			if (!parse_rule(o, f)) {
				return 0;
			}
		}
	}
	return 1;
}

unsigned
parse_sysex(struct parse_s *o, struct sysex_s **res) {
	struct sysex_s *sx;
	unsigned long val;
		
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing sysex");
		return 0;
	}	
	sx = sysex_new(0);
	for (;;) {
		if (!parse_getsym(o)) {
			goto err1;
		}
		if (o->lex.id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->lex.id == TOK_RBRACE) {
			break;
		} else if (o->lex.id == TOK_IDENT) {
			if (str_eq(o->lex.strval, "data")) {
				for (;;) {
					if (!parse_getsym(o)) {
						goto err1;
					}
					if (o->lex.id == TOK_ENDLINE) {
						break;
					}
					parse_ungetsym(o);
					if (!parse_long(o, 0xff, &val)) {
						goto err1;
					}
					sysex_add(sx, val);
				}
			} else if (str_eq(o->lex.strval, "unit")) {
				if (!parse_long(o, EV_MAXDEV, &val)) {
					goto err1;
				}
				sx->unit = val;
				if (!parse_nl(o)) {
					goto err1;
				}
			} else {
				goto unknown;
			}
		} else {
		unknown:
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				goto err1;
			}
			lex_err(&o->lex, "unknown line format in sysex, ignored");
		}
	}
	*res = sx;
	return 1;
	
err1:
	sysex_del(sx);
	return 0;
}

unsigned
parse_songchan(struct parse_s *o, struct song_s *s, struct songchan_s *i) {
	unsigned long val, val2;
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing songchan");
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->lex.id == TOK_RBRACE) {
			break;
		} else if (o->lex.id == TOK_IDENT) {
			if (str_eq(o->lex.strval, "conf")) {
				if (!parse_track(o, &i->conf)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				track_opsetchan(&i->conf, i->dev, i->ch);
			} else if (str_eq(o->lex.strval, "chan")) {
				if (!parse_chan(o, &val, &val2)) {
					return 0;
				}
				i->dev = val; 
				i->ch = val2; 
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "curinput")) {
				if (!parse_chan(o, &val, &val2)) {
					return 0;
				}
				i->curinput_dev = val;
				i->curinput_ch = val2;
				if (!parse_nl(o)) {
					return 0;
				}
			} else {
				goto unknown;
			}
		} else {
		unknown:
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				return 0;
			}
			lex_err(&o->lex, "unknown line format in songchan, ignored");
		}
	}
	return 1;
}


unsigned
parse_songtrk(struct parse_s *o, struct song_s *s, struct songtrk_s *t) {
	struct songfilt_s *f;
	unsigned long val;
	
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing songtrk");
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->lex.id == TOK_RBRACE) {
			break;
		} else if (o->lex.id == TOK_IDENT) {
			if (str_eq(o->lex.strval, "curfilt")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected after 'curfilt' in songtrk");
					return 0;
				}
				f = song_filtlookup(s, o->lex.strval);
				if (!f) {
					f = songfilt_new(o->lex.strval);
					song_filtadd(s, f);
				}				
				t->curfilt = f; 
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "track")) {
				if (!parse_track(o, &t->track)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "mute")) {
				if (!parse_long(o, 1, &val)) {
					return 0;
				}
				t->mute = val;
				if (!parse_nl(o)) {
					return 0;
				}
			} else {
				goto unknown;
			}
		} else {
		unknown:
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				return 0;
			}
			lex_err(&o->lex, "unknown line format in songtrk, ignored");
		}
	}
	
	return 1;
}


unsigned
parse_songfilt(struct parse_s *o, struct song_s *s, struct songfilt_s *g) {
	struct songchan_s *c;
	
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing songfilt");
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->lex.id == TOK_RBRACE) {
			break;
		} else if (o->lex.id == TOK_IDENT) {
			if (str_eq(o->lex.strval, "curchan")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected after 'curchan' in songfilt");
					return 0;
				}
				c = song_chanlookup(s, o->lex.strval);
				if (!c) {
					c = songchan_new(o->lex.strval);
					song_chanadd(s, c);
				}
				g->curchan = c; 
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "filt")) {
				if (!parse_filt(o, &g->filt)) {
					return 0;
				}
			} else {
				goto unknown;
			}
		} else {
		unknown:
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				return 0;
			}
			lex_err(&o->lex, "unknown line format in songfilt, ignored");
		}
	}
	return 1;
}



unsigned
parse_songsx(struct parse_s *o, struct song_s *s, struct songsx_s *g) {
	struct sysex_s *sx;
	
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing songsx");
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->lex.id == TOK_RBRACE) {
			break;
		} else if (o->lex.id == TOK_IDENT) {
			if (str_eq(o->lex.strval, "sysex")) {
				if (!parse_sysex(o, &sx)) {
					return 0;
				}
				sysexlist_put(&g->sx, sx);
			} else {
				goto unknown;
			}
		} else {
		unknown:
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				return 0;
			}
			lex_err(&o->lex, "unknown line format in songsx, ignored");
		}
	}
	return 1;
}


unsigned
parse_song(struct parse_s *o, struct song_s *s) {
	struct songtrk_s *t;
	struct songchan_s *i;
	struct songfilt_s *g;
	struct songsx_s *l;
	unsigned long num, num2;

	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing song");
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->lex.id == TOK_RBRACE) {
			break;
		} else if (o->lex.id == TOK_IDENT) {
			if (str_eq(o->lex.strval, "songtrk")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected after 'songtrk' in song");
					return 0;
				}
				t = song_trklookup(s, o->lex.strval);
				if (t == NULL) {
					t = songtrk_new(o->lex.strval);
					song_trkadd(s, t);
				}
				if (!parse_songtrk(o, s, t)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "songchan")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected after 'songchan' in song");
					return 0;
				}
				i = song_chanlookup(s, o->lex.strval);
				if (i == NULL) {
					i = songchan_new(o->lex.strval);
					song_chanadd(s, i);
				}
				if (!parse_songchan(o, s, i)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "songfilt")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected after 'songfilt' in song");
					return 0;
				}
				g = song_filtlookup(s, o->lex.strval);
				if (!g) {
					g = songfilt_new(o->lex.strval);
					song_filtadd(s, g);
				}
				if (!parse_songfilt(o, s, g)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "songsx")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected after 'songsx' in song");
					return 0;
				}
				l = song_sxlookup(s, o->lex.strval);
				if (!l) {
					l = songsx_new(o->lex.strval);
					song_sxadd(s, l);
				}
				if (!parse_songsx(o, s, l)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "meta")) {
				if (!parse_track(o, &s->meta)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "tics_per_unit")) {
				if (!parse_long(o, ~1U, &num)) {
					return 0;
				}
				if ((num % 96) != 0) {
					lex_err(&o->lex, "warning, rounded tic_per_unit to a multiple of 96");
					num = 96 * (num / 96);
					if (num < 96) {
						num = 96;
					}
				}
				if (!parse_nl(o)) {
					return 0;
				}
				s->tics_per_unit = num;
			} else if (str_eq(o->lex.strval, "curtrk")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected afer 'curtrk' in song");
					return 0;
				}
				t = song_trklookup(s, o->lex.strval);
				if (t) {
					s->curtrk = t;
				} else {
					lex_err(&o->lex, "warning, cant set current track, not such track");
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "curfilt")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected afer 'curfilt' in song");
					return 0;
				}
				g = song_filtlookup(s, o->lex.strval);
				if (g) {
					s->curfilt = g;
				} else {
					lex_err(&o->lex, "warning, cant set current filt, not such filt");
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "cursx")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected afer 'cursx' in song");
					return 0;
				}
				l = song_sxlookup(s, o->lex.strval);
				if (l) {
					s->cursx = l;
				} else {
					lex_err(&o->lex, "warning, cant set current sysex, not such sysex");
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "curchan")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected afer 'curchan' in song");
					return 0;
				}
				i = song_chanlookup(s, o->lex.strval);
				if (i) {
					s->curchan = i;
				} else {
					lex_err(&o->lex, "warning, cant set current chan, not such chan");
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "curpos")) {
				if (!parse_long(o, ~1U, &num)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				s->curpos = num;
			} else if (str_eq(o->lex.strval, "curlen")) {
				if (!parse_long(o, ~1U, &num)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				s->curlen = num;
			} else if (str_eq(o->lex.strval, "curquant")) {
				if (!parse_long(o, ~1U, &num)) {
					return 0;
				}
				if (num > s->tics_per_unit) {
					lex_err(&o->lex, "warning, truncated curquant because it was too large");
					num = s->tics_per_unit;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				s->curquant = num;
			} else if (str_eq(o->lex.strval, "curinput")) {
				if (!parse_chan(o, &num, &num2)) {
					return 0;
				}
				s->curinput_dev = num;
				s->curinput_ch = num2;
				if (!parse_nl(o)) {
					return 0;
				}
			} else {
				goto unknown;
			}
		} else {
		unknown:
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				return 0;
			}
			lex_err(&o->lex, "unknown line format in song, ignored");
		}
	}
	return 1;
}

void
song_save(struct song_s *o, char *name) {
	struct textout_s *f;
	
	f = textout_new(name);
	if (f == NULL) {
		return;
	}
	textout_putstr(f, 
	    "#\n"
	    "# song file of " VERSION "\n"
	    "#\n"
	    );
	song_output(o, f);
	textout_putstr(f, "\n");
	textout_delete(f);	
}

unsigned
song_load(struct song_s *o, char *filename) {
	struct parse_s *parse;
	unsigned res;
	
	parse = parse_new(filename);
	if (!parse) {
		return 0;
	}
	res = parse_empty(parse);
	if (res != 0) {
		res = parse_song(parse, o);
	}
	parse_delete(parse);
	return res;
}
