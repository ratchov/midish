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

#include "dbg.h"
#include "name.h"
#include "song.h"
#include "filt.h"
#include "parse.h"
#include "textio.h"
#include "saveload.h"
#include "frame.h"
#include "conv.h"
#include "version.h"

/* ------------------------------------------------------------------- */

void
chan_output(unsigned dev, unsigned ch, struct textout *f)
{
	textout_putstr(f, "{");
	textout_putlong(f, dev);
	textout_putstr(f, " ");
	textout_putlong(f, ch % 16);
	textout_putstr(f, "}");
}


void
ev_output(struct ev *e, struct textout *f)
{

	/* XXX: use ev_getstr() */

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
		case EV_XCTL:
			if (e->ctl_num < 32) {
				textout_putstr(f, "xctl");
				goto two;
			} else {
				textout_putstr(f, "ctl");
				textout_putstr(f, " ");
				chan_output(e->dev, e->ch, f);
				textout_putstr(f, " ");
				textout_putlong(f, e->ctl_num);
				textout_putstr(f, " ");
				textout_putlong(f, e->ctl_val >> 7);
			}
			break;
		case EV_XPC:
			textout_putstr(f, "xpc");
			goto two;
		case EV_RPN:
			textout_putstr(f, "rpn");
			goto two;
		case EV_NRPN:
			textout_putstr(f, "nrpn");
			goto two;
		case EV_KAT:
			textout_putstr(f, "kat");
			goto two;
		case EV_BEND:
			textout_putstr(f, "bend");
			textout_putstr(f, " ");
			chan_output(e->dev, e->ch, f);
			textout_putstr(f, " ");
			textout_putlong(f, e->bend_val & 0x7f);
			textout_putstr(f, " ");
			textout_putlong(f, e->bend_val >> 7);
			break;
		case EV_TEMPO:
			textout_putstr(f, "tempo");
			textout_putstr(f, " ");
			textout_putlong(f, e->tempo_usec24);
			break;
		case EV_TIMESIG:
			textout_putstr(f, "timesig");
			textout_putstr(f, " ");
			textout_putlong(f, e->timesig_beats);
			textout_putstr(f, " ");
			textout_putlong(f, e->timesig_tics);
			break;
		default:
			textout_putstr(f, "# ignored event\n");
			dbg_puts("ignoring event: ");
			ev_dbg(e);
			dbg_puts("\n");
			break;
		}
	return;
two:
	textout_putstr(f, " ");
	chan_output(e->dev, e->ch, f);
	textout_putstr(f, " ");
	textout_putlong(f, e->v0);
	textout_putstr(f, " ");
	if (e->v1 != EV_UNDEF) {
		textout_putlong(f, e->v1);
	} else {
		textout_putstr(f, "nil");
	}
	return;
one:
	textout_putstr(f, " ");
	chan_output(e->dev, e->ch, f);
	textout_putstr(f, " ");
	textout_putlong(f, e->v0);
	return;
}

void
range_output(unsigned min, unsigned max, struct textout *f)
{
	textout_putlong(f, min);
	if (min != max) {
		textout_putstr(f, "..");
		textout_putlong(f, max);
	}
}

void
evspec_output(struct evspec *o, struct textout *f)
{
	textout_putstr(f, evinfo[o->cmd].spec);
	if ((evinfo[o->cmd].flags & EV_HAS_DEV) &&
	    (evinfo[o->cmd].flags & EV_HAS_CH)) {
		textout_putstr(f, " {");
		range_output(o->dev_min, o->dev_max, f);
		textout_putstr(f, " ");
		range_output(o->ch_min, o->ch_max, f);
		textout_putstr(f, "}");
	}
	if (evinfo[o->cmd].nranges >= 1) {
		textout_putstr(f, " ");
		range_output(o->v0_min, o->v0_max, f);
	}
	if (evinfo[o->cmd].nranges >= 2) {
		textout_putstr(f, " ");
		range_output(o->v1_min, o->v1_max, f);
	}
}

void
track_output(struct track *t, struct textout *f)
{
	struct seqev *i;

	textout_putstr(f, "{\n");
	textout_shiftright(f);

	for (i = t->first; i != NULL; i = i->next) {
		if (i->delta != 0) {
			textout_indent(f);
			textout_putlong(f, i->delta);
			textout_putstr(f, "\n");
		}
		if (i->ev.cmd == EV_NULL) {
			break;
		}
		textout_indent(f);
		ev_output(&i->ev, f);
		textout_putstr(f, "\n");
	}

	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

void
filt_output(struct filt *o, struct textout *f)
{
	struct filtnode *s, *snext;
	struct filtnode *d;
	textout_putstr(f, "{\n");
	textout_shiftright(f);

	snext = 0;
	for (;;) {
		if (snext == o->map)
			break;
		for (s = o->map; s->next != snext; s = s->next) {
			/* nothing */
		}
		for (d = s->dstlist; d != NULL; d = d->next) {
			textout_indent(f);
			textout_putstr(f, "evmap ");
			evspec_output(&s->es, f);
			textout_putstr(f, " > ");
			evspec_output(&d->es, f);
			textout_putstr(f, "\n");
		}
		snext = s;
	}
	for (d = o->transp; d != NULL; d = d->next) {
		textout_indent(f);
		textout_putstr(f, "transp ");
		evspec_output(&d->es, f);
		textout_putstr(f, " ");
		textout_putlong(f, d->u.transp.plus & 0x7f);
		textout_putstr(f, "\n");
	}
	for (d = o->vcurve; d != NULL; d = d->next) {
		textout_indent(f);
		textout_putstr(f, "vcurve ");
		evspec_output(&d->es, f);
		textout_putstr(f, " ");
		textout_putlong(f, (64 - d->u.vel.nweight) & 0x7f);
		textout_putstr(f, "\n");
	}
	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

void
sysex_output(struct sysex *o, struct textout *f)
{
	struct chunk *c;
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
songsx_output(struct songsx *o, struct textout *f)
{
	struct sysex *i;
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
songtrk_output(struct songtrk *o, struct textout *f)
{
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
songchan_output(struct songchan *o, struct textout *f)
{
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

	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

void
songfilt_output(struct songfilt *o, struct textout *f)
{
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
metro_output(struct metro *o, struct textout *f)
{
	char *mstr;

	if (o->mask & (1 << SONG_PLAY)) {
		mstr = "on";
	} else if (o->mask & (1 << SONG_REC)) {
		mstr = "rec";
	} else {
		mstr = "off";
	}

	textout_putstr(f, "{\n");
	textout_shiftright(f);

	textout_indent(f);
	textout_putstr(f, "mask\t");
	textout_putstr(f, mstr);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "lo\t");
	ev_output(&o->lo, f);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "hi\t");
	ev_output(&o->hi, f);
	textout_putstr(f, "\n");

	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

void
evctltab_output(struct evctl *tab, struct textout *f)
{
	unsigned i;
	struct evctl *ctl;

	textout_putstr(f, "ctltab {\n");
	textout_shiftright(f);
	textout_indent(f);
	textout_putstr(f, "#\n");
	textout_indent(f);
	textout_putstr(f, "# name\tnumber\tdefval\n");
	textout_indent(f);
	textout_putstr(f, "#\n");
	for (i = 0; i < EV_MAXCOARSE + 1; i++) {
		ctl = &tab[i];
		if (ctl->name) {
			textout_indent(f);
			textout_putstr(f, ctl->name);
			textout_putstr(f, "\t");
			textout_putlong(f, i);
			textout_putstr(f, "\t");
			if (ctl->defval == EV_UNDEF) {
				textout_putstr(f, "nil");
			} else {
				textout_putlong(f, ctl->defval);

			}
    			textout_putstr(f, "\n");
		}
	}
	textout_shiftleft(f);
	textout_putstr(f, "}\n");
}

void
song_output(struct song *o, struct textout *f)
{
	struct songtrk *t;
	struct songchan *i;
	struct songfilt *g;
	struct songsx *s;

	textout_putstr(f, "{\n");
	textout_shiftright(f);

	textout_indent(f);
	textout_putstr(f, "tics_per_unit ");
	textout_putlong(f, o->tics_per_unit);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "tempo_factor ");
	textout_putlong(f, o->tempo_factor);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "meta ");
	track_output(&o->meta, f);
	textout_putstr(f, "\n");

	SONG_FOREACH_IN(o, i) {
		textout_indent(f);
		textout_putstr(f, "songin ");
		textout_putstr(f, i->name.str);
		textout_putstr(f, " ");
		songchan_output(i, f);
		textout_putstr(f, "\n");
	}
	SONG_FOREACH_OUT(o, i) {
		textout_indent(f);
		textout_putstr(f, "songout ");
		textout_putstr(f, i->name.str);
		textout_putstr(f, " ");
		songchan_output(i, f);
		textout_putstr(f, "\n");
	}
	SONG_FOREACH_FILT(o, g) {
		textout_indent(f);
		textout_putstr(f, "songfilt ");
		textout_putstr(f, g->name.str);
		textout_putstr(f, " ");
		songfilt_output(g, f);
		textout_putstr(f, "\n");
	}
	SONG_FOREACH_TRK(o, t) {
		textout_indent(f);
		textout_putstr(f, "songtrk ");
		textout_putstr(f, t->name.str);
		textout_putstr(f, " ");
		songtrk_output(t, f);
		textout_putstr(f, "\n");
	}
	SONG_FOREACH_SX(o, s) {
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
	if (o->curin) {
		textout_indent(f);
		textout_putstr(f, "curin ");
		textout_putstr(f, o->curin->name.str);
		textout_putstr(f, "\n");
	}
	if (o->curout) {
		textout_indent(f);
		textout_putstr(f, "curout ");
		textout_putstr(f, o->curout->name.str);
		textout_putstr(f, "\n");
	}
	textout_indent(f);
	textout_putstr(f, "curpos ");
	textout_putlong(f, o->curpos);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "curlen ");
	textout_putlong(f, o->curlen);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "curquant ");
	textout_putlong(f, o->curquant);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "curev ");
	evspec_output(&o->curev, f);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "metro ");
	metro_output(&o->metro, f);
	textout_putstr(f, "\n");

	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

/* ---------------------------------------------------------------------- */

unsigned parse_ukline(struct parse *o);
unsigned parse_ukblock(struct parse *o);
unsigned parse_delta(struct parse *o, unsigned *delta);
unsigned parse_ev(struct parse *o, struct ev *ev);
unsigned parse_track(struct parse *o, struct track *t);
unsigned parse_songtrk(struct parse *o, struct song *s, struct songtrk *t);
unsigned parse_song(struct parse *o, struct song *s);

unsigned
parse_ukline(struct parse *o)
{
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT && o->lex.id != TOK_NUM &&
	    o->lex.id != TOK_ENDLINE) {
		lex_err(&o->lex, "ident, number, newline or '}' expected");
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
parse_ukblock(struct parse *o)
{
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
parse_nl(struct parse *o)
{
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
parse_empty(struct parse *o)
{
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
parse_long(struct parse *o, unsigned long min, unsigned long max, unsigned long *data)
{
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_NUM) {
		lex_err(&o->lex, "number expected");
		return 0;
	}
	if (o->lex.longval < min || o->lex.longval > max) {
		lex_err(&o->lex, "number out of allowed range");
		return 0;
	}
	*data = (unsigned)o->lex.longval;
	return 1;
}

unsigned
parse_delta(struct parse *o, unsigned *delta)
{
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
parse_chan(struct parse *o, unsigned long *dev, unsigned long *ch)
{
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id == TOK_LBRACE) {
		if (!parse_long(o, 0, EV_MAXDEV, dev)) {
			return 0;
		}
		if (!parse_long(o, 0, EV_MAXCH, ch)) {
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
parse_ev(struct parse *o, struct ev *ev)
{
	unsigned long val, val2;

	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT) {
		lex_err(&o->lex, "event name expected");
		return 0;
	}
	if (!ev_str2cmd(ev, o->lex.strval)) {
	ignore:
		parse_ungetsym(o);
		if (!parse_ukline(o)) {
			return 0;
		}
		lex_err(&o->lex, "unknown event, ignored");
		ev->cmd = EV_NULL;
		return 1;
	}
	if (EV_ISVOICE(ev)) {
		if (!parse_chan(o, &val, &val2)) {
			return 0;
		}
		ev->dev = val;
		ev->ch = val2;
	}
	switch (ev->cmd) {
	case EV_TEMPO:
		if (!parse_long(o, TEMPO_MIN, TEMPO_MAX, &val)) {
			return 0;
		}
		ev->tempo_usec24 = val;
		break;
	case EV_TIMESIG:
		if (!parse_long(o, 1, TIMESIG_BEATS_MAX, &val)) {
			return 0;
		}
		ev->timesig_beats = val;
		if (!parse_long(o, 1, TIMESIG_TICS_MAX, &val)) {
			return 0;
		}
		ev->timesig_tics = val;
		break;
	case EV_NRPN:
	case EV_RPN:
		if (!parse_long(o, 0, EV_MAXFINE, &val)) {
			return 0;
		}
		ev->rpn_num = val;
		if (!parse_long(o, 0, EV_MAXFINE, &val)) {
			return 0;
		}
		ev->rpn_val = val;
		break;
	case EV_XCTL:
		if (!parse_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->ctl_num = val;
		if (!parse_long(o, 0, EV_MAXFINE, &val)) {
			return 0;
		}
		ev->ctl_val = val;
		break;
	case EV_XPC:
		if (!parse_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->pc_prog = val;
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_NIL) {
			ev->pc_bank = EV_UNDEF;
			break;
		}
		parse_ungetsym(o);
		if (!parse_long(o, 0, EV_MAXFINE, &val)) {
			return 0;
		}
		ev->pc_bank = val;
		break;
	case EV_NON:
	case EV_NOFF:
	case EV_CTL:
		if (!parse_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v0 = val;
		if (!parse_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v1 = val;
		break;
	case EV_KAT:
		if (!parse_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v0 = val;
		/*
		 * XXX: midish version < 0.2.6 used to
		 * generate bogus kat events (without the last
		 * byte). As workaround, we ignore such
		 * events, in order to allow user to load its
		 * files. Remove this code when no more needed
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
		if (!parse_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v1 = val;
		break;
	case EV_PC:
	case EV_CAT:
		if (!parse_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v0 = val;
		break;
	case EV_BEND:
		if (!parse_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v0 = val;
		if (!parse_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v0 += (val << 7);
		break;
	default:
		goto ignore;
	}
	if (!parse_nl(o)) {
		return 0;
	}
	return 1;
}


unsigned
parse_track(struct parse *o, struct track *t)
{
	unsigned delta;
	struct seqev *pos, *se;
	struct statelist slist;
	struct ev ev, rev;

	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing track");
		return 0;
	}
	track_clear(t);
	statelist_init(&slist);
	pos = t->first;
	for (;;) {
		if (!parse_getsym(o)) {
			statelist_done(&slist);
			return 0;
		}
		if (o->lex.id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->lex.id == TOK_RBRACE) {
			break;
		} else if (o->lex.id == TOK_NUM) {
			parse_ungetsym(o);
			if (!parse_delta(o, &delta)) {
				statelist_done(&slist);
				return 0;
			}
			pos->delta += delta;
		} else {
			parse_ungetsym(o);
			if (!parse_ev(o, &ev)) {
				statelist_done(&slist);
				return 0;
			}
			if (ev.cmd != EV_NULL) {
				if (conv_packev(&slist, 0U, &ev, &rev)) {
					se = seqev_new();
					se->ev = rev;
					seqev_ins(pos, se);
				}
			}
		}
	}
	statelist_done(&slist);
	return 1;
}

unsigned
parse_range(struct parse *o, unsigned min, unsigned max,
    unsigned *rmin, unsigned *rmax)
{
	unsigned long tmin, tmax;

	if (!parse_long(o, min, max, &tmin))
		return 0;
	if (!parse_getsym(o))
		return 0;
	if (o->lex.id != TOK_RANGE) {
		parse_ungetsym(o);
		*rmin = *rmax = tmin;
		return 1;
	}
	if (!parse_long(o, min, max, &tmax))
		return 0;
	*rmin = tmin;
	*rmax = tmax;
	return 1;
}

unsigned
parse_evspec(struct parse *o, struct evspec *es)
{
	struct evinfo *info;

	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT) {
		lex_err(&o->lex, "event spec name expected");
		return 0;
	}
	if (!evspec_str2cmd(es, o->lex.strval)) {
		return 0;
	}
	info = &evinfo[es->cmd];
	if ((info->flags & EV_HAS_DEV) && (info->flags & EV_HAS_CH)) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_LBRACE) {
			lex_err(&o->lex, "'{' expected");
			return 0;
		}
		if (!parse_range(o, 0, EV_MAXDEV, &es->dev_min, &es->dev_max) ||
		    !parse_range(o, 0, EV_MAXCH, &es->ch_min, &es->ch_max))
			return 0;
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_RBRACE) {
			lex_err(&o->lex, "'}' expected");
			return 0;
		}
	}
	if (info->nranges == 0) {
		return 1;
	}
	if (!parse_range(o, 0, info->v0_max, &es->v0_min, &es->v0_max)) {
		return 0;
	}
	if (info->nranges == 1) {
		return 1;
	}
	if (!parse_range(o, 0, info->v1_max, &es->v1_min, &es->v1_max)) {
		return 0;
	}
	return 1;
}

unsigned
parse_rule(struct parse *o, struct filt *f)
{
	unsigned long idev, ich, odev, och, ictl, octl, keylo, keyhi, ukeyplus;
	struct evspec from, to;
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
		if (!parse_long(o, 0, EV_MAXCOARSE, &keylo)) {
			return 0;
		}
		if (!parse_long(o, 0, EV_MAXCOARSE, &keyhi)) {
			return 0;
		}
		evspec_reset(&from);
		from.cmd = EVSPEC_NOTE;
		from.dev_min = from.dev_max = idev;
		from.ch_min = from.ch_max = ich;
		from.v0_min = keylo;
		from.v0_max = keyhi;
		to.cmd = EVSPEC_EMPTY;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->lex.strval, "keymap")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!parse_chan(o, &odev, &och)) {
			return 0;
		}
		if (!parse_long(o, 0, EV_MAXCOARSE, &keylo)) {
			return 0;
		}
		if (!parse_long(o, 0, EV_MAXCOARSE, &keyhi)) {
			return 0;
		}
		if (!parse_long(o, 0, EV_MAXCOARSE, &ukeyplus)) {
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
		if ((int)keyhi + keyplus > EV_MAXCOARSE)
			keyhi = EV_MAXCOARSE - keyplus;
		if ((int)keylo + keyplus < 0)
			keylo = -keyplus;
		if (keylo >= keyhi) {
			lex_err(&o->lex, "bad range in keymap rule");
			return 0;
		}
		evspec_reset(&from);
		evspec_reset(&to);
		from.cmd = EVSPEC_NOTE;
		from.dev_min = from.dev_max = idev;
		from.ch_min = from.ch_max = ich;
		from.v0_min = keylo;
		from.v0_max = keyhi;
		to.cmd = EVSPEC_NOTE;
		to.dev_min = to.dev_max = odev;
		to.ch_min = to.ch_max = och;
		to.v0_min = keylo + keyplus;
		to.v0_max = keyhi + keyplus;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->lex.strval, "ctldrop")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!parse_long(o, 0, EV_MAXCOARSE, &ictl)) {
			return 0;
		}
		evspec_reset(&from);
		from.cmd = EVSPEC_XCTL;
		from.dev_min = from.dev_max = idev;
		from.ch_min = from.ch_max = ich;
		from.v0_min = from.v0_max = ictl;
		to.cmd = EVSPEC_EMPTY;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->lex.strval, "ctlmap")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!parse_chan(o, &odev, &och)) {
			return 0;
		}
		if (!parse_long(o, 0, EV_MAXCOARSE, &ictl)) {
			return 0;
		}
		if (!parse_long(o, 0, EV_MAXCOARSE, &octl)) {
			return 0;
		}
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_IDENT) {
			lex_err(&o->lex, "curve identifier expected");
			return 0;
		}
		evspec_reset(&from);
		evspec_reset(&to);
		from.cmd = EVSPEC_CTL;
		from.dev_min = from.dev_max = idev;
		from.ch_min = from.ch_max = ich;
		from.v0_min = from.v0_max = ictl;
		to.cmd = EVSPEC_CTL;
		to.dev_min = to.dev_max = odev;
		to.ch_min = to.ch_max = och;
		to.v0_min = to.v0_max = octl;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->lex.strval, "chandrop")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		evspec_reset(&from);
		from.dev_min = from.dev_max = idev;
		from.ch_min = from.ch_max = ich;
		to.cmd = EVSPEC_EMPTY;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->lex.strval, "chanmap")) {
		if (!parse_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!parse_chan(o, &odev, &och)) {
			return 0;
		}
		evspec_reset(&from);
		evspec_reset(&to);
		from.dev_min = from.dev_max = idev;
		from.ch_min = from.ch_max = ich;
		to.dev_min = to.dev_max = odev;
		to.ch_min = to.ch_max = och;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->lex.strval, "devdrop")) {
		if (!parse_long(o, 0, EV_MAXDEV, &idev)) {
			return 0;
		}
		evspec_reset(&from);
		from.dev_min = from.dev_max = idev;
		to.cmd = EVSPEC_EMPTY;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->lex.strval, "devmap")) {
		if (!parse_long(o, 0, EV_MAXDEV, &idev)) {
			return 0;
		}
		if (!parse_long(o, 0, EV_MAXDEV, &odev)) {
			return 0;
		}
		evspec_reset(&from);
		evspec_reset(&to);
		from.dev_min = from.dev_max = idev;
		to.dev_min = to.dev_max = odev;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->lex.strval, "evmap")) {
		if (!parse_evspec(o, &from)) {
			return 0;
		}
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_GT) {
			lex_err(&o->lex, "'>' expected");
			return 0;
		}
		if (!parse_evspec(o, &to)) {
			return 0;
		}
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->lex.strval, "transp")) {
		if (!parse_evspec(o, &to)) {
			return 0;
		}
		if (!parse_long(o, 0, EV_MAXCOARSE, &ukeyplus)) {
			return 0;
		}
		filt_transp(f, &to, ukeyplus);
	} else if (str_eq(o->lex.strval, "vcurve")) {
		if (!parse_evspec(o, &to)) {
			return 0;
		}
		if (!parse_long(o, 1, EV_MAXCOARSE, &ukeyplus)) {
			return 0;
		}
		filt_vcurve(f, &to, ukeyplus);
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
parse_filt(struct parse *o, struct filt *f)
{
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
parse_sysex(struct parse *o, struct sysex **res)
{
	struct sysex *sx;
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
					if (!parse_long(o, 0, 0xff, &val)) {
						goto err1;
					}
					sysex_add(sx, val);
				}
			} else if (str_eq(o->lex.strval, "unit")) {
				if (!parse_long(o, 0, EV_MAXDEV, &val)) {
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
parse_songchan(struct parse *o, struct song *s, struct songchan *i)
{
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
				track_setchan(&i->conf, i->dev, i->ch);
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
				if (!parse_nl(o)) {
					return 0;
				}
				lex_err(&o->lex, 
				    "ignored obsolete 'curinput' line");
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
parse_songtrk(struct parse *o, struct song *s, struct songtrk *t)
{
	struct songfilt *f;
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
					f = song_filtnew(s, o->lex.strval);
					song_setcurfilt(s, NULL);
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
				if (!parse_long(o, 0, 1, &val)) {
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
parse_songfilt(struct parse *o, struct song *s, struct songfilt *g)
{
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
					lex_err(&o->lex, 
					    "identifier expected "
					    "after 'curchan' in songfilt");
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				lex_err(&o->lex, 
				    "ignored obsolete 'curchan' line");
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
parse_songsx(struct parse *o, struct song *s, struct songsx *g)
{
	struct sysex *sx;

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
parse_metro(struct parse *o, struct metro *m)
{
	struct ev ev;
	unsigned mask;

	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		lex_err(&o->lex, "'{' expected while parsing metro");
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
			if (str_eq(o->lex.strval, "enabled")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "mask")) {
				if (!parse_getsym(o)) {
					return 0;
				}
				if (!metro_str2mask(m, o->lex.strval, &mask)) {
					lex_err(&o->lex, "skipped unknown metronome mask");
				}
				metro_setmask(m, mask);
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "lo")) {
				if (!parse_ev(o, &ev)) {
					return 0;
				}
				if (ev.cmd != EV_NON) {
					lex_err(&o->lex, "'lo' must be followed by a 'non' event\n");
				} else {
					m->lo = ev;
				}
			} else if (str_eq(o->lex.strval, "hi")) {
				if (!parse_ev(o, &ev)) {
					return 0;
				}
				if (ev.cmd != EV_NON) {
					lex_err(&o->lex, "'hi' must be followed by a 'non' event\n");
				} else {
					m->hi = ev;
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

unsigned
parse_song(struct parse *o, struct song *s)
{
	struct songtrk *t;
	struct songchan *i;
	struct songfilt *g;
	struct songsx *l;
	struct evspec es;
	unsigned long num, num2;
	int input;

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
					t = song_trknew(s, o->lex.strval);
					song_setcurtrk(s, NULL);
				}
				if (!parse_songtrk(o, s, t)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "songin")) {
				input = 1;
				goto chan_do;
			} else if (str_eq(o->lex.strval, "songout") ||
				str_eq(o->lex.strval, "songchan")) {
				input = 0;
			chan_do:
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected after 'songchan' in song");
					return 0;
				}
				i = song_chanlookup(s, o->lex.strval, input);
				if (i == NULL) {
					i = song_channew(s, o->lex.strval,
					    0, 0, input);
					song_setcurchan(s, NULL, input);
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
					g = song_filtnew(s, o->lex.strval);
					song_setcurfilt(s, NULL);
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
					l = song_sxnew(s, o->lex.strval);
					song_setcursx(s, NULL);
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
				if (!parse_long(o, 0, ~1U, &num)) {
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
			} else if (str_eq(o->lex.strval, "tempo_factor")) {
				if (!parse_long(o, 0, ~1U, &num)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				if (num < 0x80 || num > 0x200) {
					lex_err(&o->lex, "warning, tempo factor out of bounds\n");
				} else {
					s->tempo_factor = num;
				}
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
			} else if (str_eq(o->lex.strval, "curin")) {
				input = 1;
				goto curchan_do;
			} else if (str_eq(o->lex.strval, "curout") ||
			    str_eq(o->lex.strval, "curchan")) {
				input = 0;
			curchan_do:
				if (!parse_getsym(o)) {
					return 0;
				}
				if (o->lex.id != TOK_IDENT) {
					lex_err(&o->lex, "identifier expected afer 'curchan' in song");
					return 0;
				}
				i = song_chanlookup(s, o->lex.strval, input);
				if (i) {
					song_setcurchan(s, i, input);
				} else {
					lex_err(&o->lex, "warning, cant set current chan, not such chan");
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "curpos")) {
				if (!parse_long(o, 0, ~1U, &num)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				s->curpos = num;
			} else if (str_eq(o->lex.strval, "curlen")) {
				if (!parse_long(o, 0, ~1U, &num)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				s->curlen = num;
			} else if (str_eq(o->lex.strval, "curquant")) {
				if (!parse_long(o, 0, ~1U, &num)) {
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
			} else if (str_eq(o->lex.strval, "curev")) {
				if (!parse_evspec(o, &es)) {
					return 0;
				}
				if (!parse_nl(o)) {
					return 0;
				}
				s->curev = es;
			} else if (str_eq(o->lex.strval, "curinput")) {
				if (!parse_chan(o, &num, &num2)) {
					return 0;
				}
				i = song_chanlookup_bynum(s, num, num2, 1);
				if (i == NULL) {
					i = song_channew(s, "old_style_curin",
					    num, num2, 1);
					song_setcurchan(s, NULL, 1);
				} else {
					i->dev = num;
					i->ch = num2;
				}
				if (!parse_nl(o)) {
					return 0;
				}
			} else if (str_eq(o->lex.strval, "metro")) {
				if (!parse_metro(o, &s->metro)) {
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
song_save(struct song *o, char *name)
{
	struct textout *f;

	f = textout_new(name);
	if (f == NULL) {
		return;
	}
	textout_putstr(f,
	    "#\n"
	    "# " VERSION "\n"
	    "#\n"
	    );
	song_output(o, f);
	textout_putstr(f, "\n");
	textout_delete(f);
}

unsigned
song_load(struct song *o, char *filename)
{
	struct parse *parse;
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
