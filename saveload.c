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
 * 	  materials  provided with the distribution.
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
ev_output(struct ev_s *e, struct textout_s *f) {
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
			goto one;
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
	textout_putlong(f, e->data.voice.chan);
	textout_putstr(f, " ");
	textout_putlong(f, e->data.voice.b0);
	textout_putstr(f, " ");
	textout_putlong(f, e->data.voice.b1);
	textout_putstr(f, "\n");
	return;
one:	
	textout_putstr(f, " ");
	textout_putlong(f, e->data.voice.chan);
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
	case RULE_CHANMAP:
		textout_putstr(f, "chanmap ");		
		textout_putlong(f, o->ichan);
		textout_putstr(f, " ");
		textout_putlong(f, o->ochan);
		break;
	case RULE_CTRLMAP:
		textout_putstr(f, "ctlmap ");		
		textout_putlong(f, o->ichan);
		textout_putstr(f, " ");
		textout_putlong(f, o->ochan);
		textout_putstr(f, " ");
		textout_putlong(f, o->ictl);
		textout_putstr(f, " ");
		textout_putlong(f, o->octl);
		textout_putstr(f, " ");
		textout_putstr(f, "id");
		break;
	case RULE_KEYMAP:
		textout_putstr(f, "keymap ");		
		textout_putlong(f, o->ichan);
		textout_putstr(f, " ");
		textout_putlong(f, o->ochan);
		textout_putstr(f, " ");
		textout_putlong(f, o->key_start);
		textout_putstr(f, " ");
		textout_putlong(f, o->key_end);
		textout_putstr(f, " ");
		textout_putlong(f, o->key_plus & 0x7f);
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
	
	for (i = o->chan_rules; i != 0; i = i->next) {
		rule_output(i, f);
	}
	for (i = o->voice_rules; i != 0; i = i->next) {
		rule_output(i, f);
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
	textout_putlong(f, o->chan);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "conf ");
	track_output(&o->conf, f);
	textout_putstr(f, "\n");


	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

void
songfilt_output(struct songfilt_s *o, struct textout_s *f) {
	textout_putstr(f, "{\n");
	textout_shiftright(f);

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

	textout_putstr(f, "{\n");
	textout_shiftright(f);
	
	textout_indent(f);
	textout_putstr(f, "meta ");
	track_output(&o->meta, f);
	textout_putstr(f, "\n");

	for (i = o->chanlist; i != 0; i = (struct songchan_s *)i->name.next) {
		textout_indent(f);
		textout_putstr(f, "songchan ");
		textout_putstr(f, i->name.str);
		textout_putstr(f, " ");
		songchan_output(i, f);
		textout_putstr(f, "\n");
	}
	for (g = o->filtlist; g != 0; g = (struct songfilt_s *)g->name.next) {
		textout_indent(f);
		textout_putstr(f, "songfilt ");
		textout_putstr(f, g->name.str);
		textout_putstr(f, " ");
		songfilt_output(g, f);
		textout_putstr(f, "\n");
	}
	for (t = o->trklist; t != 0; t = (struct songtrk_s *)t->name.next) {
		textout_indent(f);
		textout_putstr(f, "songtrk ");
		textout_putstr(f, t->name.str);
		textout_putstr(f, " ");
		songtrk_output(t, f);
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
	textout_indent(f);
	textout_putstr(f, "curpos ");
	textout_putlong(f, o->curpos);
	textout_putstr(f, "\n");
	textout_indent(f);
	
	textout_putstr(f, "curquant ");
	textout_putlong(f, o->curquant);
	textout_putstr(f, "\n");
			
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
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_ENDLINE) {
			break;
		} else if (o->lex.id == TOK_LBRACE) {
			parse_ungetsym(o);
			parse_ukblock(o);
		} else if (o->lex.id == TOK_IDENT || o->lex.id == TOK_NUM || o->lex.id == TOK_STRING) {
			/* nothing */
		} else {
			parse_error(o, "any line should start with ident or number");
			return 0;
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
		parse_error(o, "'{' expected while parsing block\n");
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
			parse_ukline(o);
		}
	}
	return 1;			
}

unsigned
parse_nl(struct parse_s *o) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_ENDLINE) {
		parse_error(o, "new line expected\n");
		return 0;
	}
	return 1;
}

unsigned
parse_long(struct parse_s *o, unsigned long max, unsigned long *data) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_NUM) {
		parse_error(o, "number expected\n");
		return 0;
	}
	if (o->lex.longval > max) {
		parse_error(o, "number to large\n");
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
		parse_error(o, "numeric constant expected in event offset\n");
		return 0;
	}
	*delta = o->lex.longval;
	if (!parse_nl(o)) {
		return 0;
	}
	return 1;
}


unsigned
parse_ev(struct parse_s *o, struct ev_s *ev) {
	unsigned long val;
	
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT) {
		parse_error(o, "event name expected\n");
		return 0;
	}
	if (!ev_str2cmd(ev, o->lex.strval)) {
		goto ignore;
	}
	if (EV_ISVOICE(ev)) {
		if (!parse_long(o, EV_MAXCHAN, &val)) {
			return 0;
		}
		ev->data.voice.chan = val;
		if (!parse_long(o, EV_MAXB0, &val)) {
			return 0;
		}
		ev->data.voice.b0 = val;
		if (ev->cmd != EV_PC && ev->cmd != EV_CAT) {
			if (!parse_long(o, EV_MAXB1, &val)) {
				return 0;
			}
			ev->data.voice.b1 = val;
		}
	} else if (ev->cmd == EV_TEMPO) {
		if (!parse_long(o, ~1, &val)) {
			return 0;
		}
		ev->data.tempo.usec24 = val;
	} else if (ev->cmd == EV_TIMESIG) {
		if (!parse_long(o, ~1, &val)) {
			return 0;
		}
		ev->data.sign.beats = o->lex.longval;
		if (!parse_long(o, ~1, &val)) {
			return 0;
		}
		ev->data.sign.tics = o->lex.longval;
	} else {
ignore:		parse_error(o, "unknown event, ignored\n");
		parse_ungetsym(o);
		return parse_ukline(o);
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
		parse_error(o, "'{' expected while parsing track\n");
		return 0;
	}
	track_clear(t, &tp);	
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
			track_evlast(t, &tp);
			track_evput(t, &tp, &ev);
		}
	}
	return 1;			
}


unsigned
parse_rule(struct parse_s *o, struct filt_s *f) {
	unsigned long ichan, ochan, ictl, octl, key_start, key_end, ukey_plus;
	int key_plus;
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT) {
		parse_error(o, "filter-type identifier expected\n");
		return 0;
	}
	if (str_eq(o->lex.strval, "keymap")) {
		if (!parse_long(o, EV_MAXCHAN, &ichan)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXCHAN, &ochan)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &key_start)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &key_end)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXB0, &ukey_plus)) {
			return 0;
		}
		if (ukey_plus > 63) {
			key_plus = ukey_plus - 128;
		} else {
			key_plus = ukey_plus;
		}
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_IDENT) {
			parse_error(o, "curve identifier expected\n");
			return 0;
		}	
		filt_new_keymap(f, ichan, ochan, key_start, key_end, key_plus);
	} else if (str_eq(o->lex.strval, "ctlmap")) {
		if (!parse_long(o, EV_MAXCHAN, &ichan)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXCHAN, &ochan)) {
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
			parse_error(o, "curve identifier expected\n");
			return 0;
		}	
		filt_new_ctlmap(f, ichan, ochan, ictl, octl);
	} else if (str_eq(o->lex.strval, "chanmap")) {
		if (!parse_long(o, EV_MAXCHAN, &ichan)) {
			return 0;
		}
		if (!parse_long(o, EV_MAXCHAN, &ochan)) {
			return 0;
		}
		filt_new_chanmap(f, ichan, ochan);
	} else {
		parse_error(o, "unknown filter rule\n");
		return 0;
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
		parse_error(o, "'{' expected while parsing filt\n");
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
parse_songchan(struct parse_s *o, struct song_s *s, struct songchan_s *i) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		parse_error(o, "'{' expected while parsing songchan\n");
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
				track_opsetchan(&i->conf, 0);
			} else if (str_eq(o->lex.strval, "chan")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_NUM) {
					parse_error(o, "number expected afer 'chan' in songchan\n");
					return 0;
				}
				i->chan = o->lex.longval; 
				if (!parse_nl(o)) {
					return 0;
				}
			} else {
				goto unknown;
			}
		} else {
		unknown:
			parse_error(o, "unknown line format in songchan, ignored\n");
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				return 0;
			}
		}
	}
	return 1;
}


unsigned
parse_songtrk(struct parse_s *o, struct song_s *s, struct songtrk_s *t) {
	struct songchan_s *c = 0;
	struct songfilt_s *f;
	
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		parse_error(o, "'{' expected while parsing songtrk\n");
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
			if (str_eq(o->lex.strval, "ochan")) {
				/* XXX remove this */
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					parse_error(o, "identifier expected after 'ochan' in songtrk\n");
					return 0;
				}
				c = song_chanlookup(s, o->lex.strval);
				if (!c) {
					parse_error(o, "not such channel in songtrk\n");
					return 0;
				}
				f = song_filtlookup(s, c->name.str);
				if (!f) {
					f = songfilt_new(c->name.str);
					song_filtadd(s, f);
					filt_new_chanmap(&f->filt, 0, c->chan);
				}
				t->curfilt = f;
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "curfilt")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					parse_error(o, "identifier expected after 'curfilt' in songtrk\n");
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
			} else {
				goto unknown;
			}
		} else {
		unknown:
			parse_error(o, "unknown line format in songtrk, ignored\n");
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				return 0;
			}
		}
	}
	
	if (c) {
		track_opsetchan(&t->track, c->chan);
	}
	return 1;
}


unsigned
parse_songfilt(struct parse_s *o, struct song_s *s, struct songfilt_s *g) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		parse_error(o, "'{' expected while parsing songfilt\n");
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
			if (str_eq(o->lex.strval, "filt")) {
				if (!parse_filt(o, &g->filt)) {
					return 0;
				}
			} else {
				goto unknown;
			}
		} else {
		unknown:
			parse_error(o, "unknown line format in songfilt, ignored\n");
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				return 0;
			}
		}
	}
	return 1;
}



unsigned
parse_song(struct parse_s *o, struct song_s *s) {
	struct songtrk_s *t;
	struct songchan_s *i;
	struct songfilt_s *g;
	long num;

	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		parse_error(o, "'{' expected while parsing song\n");
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
					parse_error(o, "identifier expected after 'songtrk' in song\n");
					return 0;
				}
				t = songtrk_new(o->lex.strval);
				if (!parse_songtrk(o, s, t)) {
					songtrk_delete(t);
					return 0;
				}
				song_trkadd(s, t);
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "songchan")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					parse_error(o, "identifier expected after 'songchan' in song\n");
					return 0;
				}
				i = songchan_new(o->lex.strval);
				if (!parse_songchan(o, s, i)) {
					songchan_delete(i);
					return 0;
				}
				song_chanadd(s, i);
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "songfilt")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					parse_error(o, "identifier expected after 'songfilt' in song\n");
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
			} else if (str_eq(o->lex.strval, "meta")) {
				if (!parse_track(o, &s->meta)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "tics_per_unit")) {
				if (!parse_long(o, ~1, &num)) {
					return 0;
				}
				if ((num % 96) != 0) {
					parse_error(o, "warning, rounded tic_per_unit to a multiple of 96\n");
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
					parse_error(o, "identifier expected afer 'curtrk' in song\n");
					return 0;
				}
				t = song_trklookup(s, o->lex.strval);
				if (t) {
					s->curtrk = t;
				} else {
					parse_error(o, "warning, cant set current track, not such track\n");
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "curfilt")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					parse_error(o, "identifier expected afer 'curfilt' in song\n");
					return 0;
				}
				g = song_filtlookup(s, o->lex.strval);
				if (t) {
					s->curfilt = g;
				} else {
					parse_error(o, "warning, cant set current filt, not such filt\n");
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "curpos")) {
				if (!parse_long(o, ~1, &num)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				s->curpos = num;
			} else if (str_eq(o->lex.strval, "curquant")) {
				if (!parse_long(o, ~1, &num)) {
					return 0;
				}
				if (num > s->tics_per_unit) {
					parse_error(o, "warning, truncated curquant because it was too large\n");
					num = s->tics_per_unit;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				s->curquant = num;
			} else {
				goto unknown;
			}
		} else {
		unknown:
			parse_error(o, "unknown line format in song, ignored\n");
			parse_ungetsym(o);
			if (!parse_ukline(o)) {
				return 0;
			}
		}
	}
	return 1;
}

unsigned
track_load(struct track_s *o, char *filename) {
	struct parse_s *parse;
	unsigned res;
	
	parse = parse_new(filename);
	if (!parse) {
		return 0;
	}
	res = parse_track(parse, o);
	parse_delete(parse);
	return res;
}

void
track_save(struct track_s *o, char *name) {
	struct textout_s *f;
	
	f = textout_new(name);
	if (f == 0) {
		return;
	}
	track_output(o, f);
	textout_putstr(f, "\n");
	textout_delete(f);
}


void
song_save(struct song_s *o, char *name) {
	struct textout_s *f;
	
	f = textout_new(name);
	if (f == 0) {
		return;
	}
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
	res = parse_song(parse, o);
	parse_delete(parse);
	return res;
}

