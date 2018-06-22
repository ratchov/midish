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

#include <limits.h>
#include "utils.h"
#include "name.h"
#include "song.h"
#include "filt.h"
#include "textio.h"
#include "saveload.h"
#include "frame.h"
#include "conv.h"
#include "version.h"
#include "cons.h"

#define FORMAT_VERSION	1

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
			textout_putstr(f, " ");
			chan_output(e->dev, e->ch, f);
			textout_putstr(f, " ");
			if (e->pc_bank != EV_UNDEF) {
				textout_putlong(f, e->pc_bank);
			} else {
				textout_putstr(f, "nil");
			}
			textout_putstr(f, " ");
			textout_putlong(f, e->pc_prog);
			return;
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
			if (EV_ISSX(e)) {
				textout_putstr(f, evinfo[e->cmd].ev);
				textout_putstr(f, " ");
				textout_putlong(f, e->dev);
				if (evinfo[e->cmd].nparams >= 1) {
					textout_putstr(f, " ");
					textout_putlong(f, e->v0);
				}
				if (evinfo[e->cmd].nparams >= 2) {
					textout_putstr(f, " ");
					textout_putlong(f, e->v1);
				}
			} else {
				textout_putstr(f, "# ignored event\n");
				log_puts("ignoring event: ");
				ev_log(e);
				log_puts("\n");
			}
			break;
		}
	return;
two:
	textout_putstr(f, " ");
	chan_output(e->dev, e->ch, f);
	textout_putstr(f, " ");
	textout_putlong(f, e->v0);
	textout_putstr(f, " ");
	textout_putlong(f, e->v1);
	if (e->cmd == EV_XCTL) {
		textout_putstr(f, " # ");
		textout_putlong(f, e->v1 >> 7);
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
		if (o->v0_min == EV_UNDEF) {
			/* XPC is allowed to have v0 = UNDEF */
			textout_putstr(f, "nil");
		} else
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
evpat_output(struct textout *f)
{
	unsigned char *p;
	unsigned i;

	for (i = 0; i < EV_NPAT; i++) {
		if (evinfo[EV_PAT0 + i].ev == NULL)
			continue;
		textout_indent(f);
		textout_putstr(f, "evpat ");
		textout_putstr(f, evinfo[EV_PAT0 + i].ev);
		textout_putstr(f, " {\n");
		textout_shiftright(f);
		textout_indent(f);
		textout_putstr(f, "pattern");
		p = evinfo[EV_PAT0 + i].pattern;
		for (;;) {
			textout_putstr(f, " ");
			switch (*p) {
			case EV_PATV0_HI:
				textout_putstr(f, "v0_hi");
				break;
			case EV_PATV0_LO:
				textout_putstr(f, "v0_lo");
				break;
			case EV_PATV1_HI:
				textout_putstr(f, "v1_hi");
				break;
			case EV_PATV1_LO:
				textout_putstr(f, "v1_lo");
				break;
			default:
				textout_putbyte(f, *p);
				if (*p == 0xf7)
					goto end;
			}
			p++;
		}
	end:
		textout_putstr(f, "\n");
		textout_shiftleft(f);
		textout_indent(f);
		textout_putstr(f, "}\n");
	}
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
	textout_putstr(f, "format ");
	textout_putlong(f, FORMAT_VERSION);
	textout_putstr(f, "\n");

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

	evpat_output(f);

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

	textout_indent(f);
	textout_putstr(f, "tap ");
	textout_putstr(f, song_tap_modestr[o->tap_mode]);
	textout_putstr(f, "\n");

	textout_indent(f);
	textout_putstr(f, "tapev ");
	evspec_output(&o->tap_evspec, f);
	textout_putstr(f, "\n");

	textout_shiftleft(f);
	textout_indent(f);
	textout_putstr(f, "}");
}

/* ---------------------------------------------------------------------- */

enum SYM_ID {
	TOK_EOF = 0, TOK_LBRACE, TOK_RBRACE, TOK_LT, TOK_GT, TOK_NIL,
	TOK_RANGE, TOK_ENDLINE, TOK_WORD, TOK_NUM
};

struct load {
	unsigned id;
#define TOK_MAXLEN	31
	char strval[TOK_MAXLEN + 1];
	unsigned long longval;
	struct textin *in;		/* input file */
	int lookchar;			/* used by ungetchar */
	unsigned line, col;		/* for error reporting */
	unsigned lookavail;
	int format;
};

unsigned      load_getsym(struct load *);
void	      load_ungetsym(struct load *);

/* ----------------------------------------------------- tokdefs --- */

unsigned
load_getchar(struct load *o, int *c)
{
	if (o->lookchar < 0) {
		textin_getpos(o->in, &o->line, &o->col);
		if (!textin_getchar(o->in, c)) {
			return 0;
		}
	} else {
		*c = o->lookchar;
		o->lookchar = -1;
	}
	return 1;
}

void
load_ungetchar(struct load *o, int c)
{
	if (o->lookchar >= 0) {
		log_puts("load_ungetchar: lookchar already set\n");
		panic();
	}
	o->lookchar = c;
}

void
load_err(struct load *o, char *msg)
{
	cons_erruu(o->line + 1, o->col + 1, msg);
}

unsigned
load_scan(struct load *o)
{
	int c, cn;
	unsigned i, dig, base;
	unsigned long val, maxq, maxr;

	for (;;) {
		if (!load_getchar(o, &c))
			return 0;

		if (c == CHAR_EOF) {
			o->id = TOK_EOF;
			return 1;
		}

		/* check if line continues */
		if (c == '\\') {
			do {
				if (!load_getchar(o, &c))
					return 0;
			} while (c == ' ' || c == '\t' || c == '\r');
			if (c == '\n')
				continue;
			if (c == CHAR_EOF) {
				load_ungetchar(o, c);
				continue;
			}
			load_ungetchar(o, c);
			load_err(o, "newline exected after '\\'");
			return 0;
		}

		/* skip comments */
		if (c == '#') {
			do {
				if (!load_getchar(o, &c))
					return 0;
			} while (c != '\n' && c != CHAR_EOF);
			load_ungetchar(o, c);
			continue;
		}

		if (c >= '0' && c <= '9') {
			base = 10;
			if (c == '0') {
				if (!load_getchar(o, &c))
					return 0;
				if (c == 'x' || c == 'X') {
					base = 16;
					if (!load_getchar(o, &c))
						return 0;
					if ((c < '0' || c > '9') &&
					    (c < 'A' || c > 'F') &&
					    (c < 'a' || c > 'f')) {
						load_ungetchar(o, c);
						load_err(o, "bad hex number");
						return 0;
					}
				}
			}

			val = 0;
			maxq = ULONG_MAX / base;
			maxr = ULONG_MAX % base;
			for (;;) {
				if (c >= 'a' && c <= 'z') {
					dig = 10 + c - 'a';
				} else if (c >= 'A' && c <= 'Z') {
					dig = 10 + c - 'A';
				} else if (c >= '0' && c <= '9') {
					dig = c - '0';
				} else {
					load_ungetchar(o, c);
					break;
				}
				if (dig >= base) {
					load_err(o, "bad number");
					return 0;
				}
				if ((val > maxq) ||
				    (val == maxq && dig > maxr)) {
					load_err(o, "number too large");
					return 0;
				}
				val = val * base + dig;
				if (!load_getchar(o, &c))
					return 0;
			}
			o->longval = val;
			o->id = TOK_NUM;
			return 1;
		}

		if ((c >= 'a' && c <= 'z') ||
		    (c >= 'A' && c <= 'Z') ||
		    (c == '_')) {
			i = 0;
			for (;;) {
				if (i >= TOK_MAXLEN) {
					load_err(o, "word too long");
					return 0;
				}
				o->strval[i++] = c;
				if (!load_getchar(o, &c)) {
					return 0;
				}
				if ((c < 'a' || c > 'z') &&
				    (c < 'A' || c > 'Z') &&
				    (c < '0' || c > '9') &&
				    (c != '_')) {
					o->strval[i++] = '\0';
					load_ungetchar(o, c);
					break;
				}
			}
			if (str_eq(o->strval, "nil"))
				o->id = TOK_NIL;
			else
				o->id = TOK_WORD;
			return 1;
		}

		switch (c) {
		case ' ':
		case '\t':
		case '\r':
			continue;
		case '\n':
			o->id = TOK_ENDLINE;
			return 1;
		case '{':
			o->id = TOK_LBRACE;
			return 1;
		case '}':
			o->id = TOK_RBRACE;
			return 1;
		case '<':
			o->id = TOK_LT;
			return 1;
		case '>':
			o->id = TOK_GT;
			return 1;
		case '.':
			if (!load_getchar(o, &cn))
				return 0;
			if (cn == '.') {
				o->id = TOK_RANGE;
				return 1;
			}
			load_ungetchar(o, cn);
		}
		load_err(o, "bad token");
		return 0;
	}
	/* not reached */
}

int
load_init(struct load *o, char *filename)
{
	o->lookchar = -1;
	o->in = textin_new(filename);
	if (!o->in)
		return 0;
	o->lookavail = 0;
	o->format = 0;
	return 1;
}

void
load_done(struct load *o)
{
	textin_delete(o->in);
}

unsigned
load_getsym(struct load *o)
{
	if (o->lookavail) {
		o->lookavail = 0;
		return 1;
	}
	return load_scan(o);
}

void
load_ungetsym(struct load *o)
{
	if (o->lookavail) {
		log_puts("load_ungetsym: looksym already set\n");
		panic();
	}
	o->lookavail = 1;
}

unsigned load_ukline(struct load *o);
unsigned load_ukblock(struct load *o);
unsigned load_delta(struct load *o, unsigned *delta);
unsigned load_ev(struct load *o, struct ev *ev);
unsigned load_track(struct load *o, struct track *t);
unsigned load_songtrk(struct load *o, struct song *s, struct songtrk *t);
unsigned load_song(struct load *o, struct song *s);

unsigned
load_ukline(struct load *o)
{
	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_WORD && o->id != TOK_NUM && o->id != TOK_ENDLINE) {
		load_err(o, "ident, number, newline or '}' expected");
		return 0;
	}
	load_ungetsym(o);
	for (;;) {
		if (!load_getsym(o))
			return 0;
		if (o->id == TOK_RBRACE || o->id == TOK_EOF) {
			load_ungetsym(o);
			break;
		} else if (o->id == TOK_ENDLINE) {
			break;
		} else if (o->id == TOK_LBRACE) {
			load_ungetsym(o);
			if (!load_ukblock(o))
				return 0;
		}
	}
	return 1;
}

unsigned
load_ukblock(struct load *o)
{
	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing block");
		return 0;
	}
	for (;;) {
		if (!load_getsym(o))
			return 0;
		if (o->id == TOK_RBRACE) {
			break;
		} else {
			load_ungetsym(o);
			if (!load_ukline(o))
				return 0;
		}
	}
	return 1;
}

unsigned
load_nl(struct load *o)
{
	if (!load_getsym(o))
		return 0;
	if (o->id == TOK_RBRACE || o->id != TOK_EOF) {
		load_ungetsym(o);
	} else if (o->id != TOK_ENDLINE) {
		load_err(o, "new line expected");
		return 0;
	}
	return 1;
}

unsigned
load_empty(struct load *o)
{
	for (;;) {
		if (!load_getsym(o))
			return 0;
		if (o->id != TOK_ENDLINE) {
			load_ungetsym(o);
			break;
		}
	}
	return 1;
}


unsigned
load_long(struct load *o, unsigned long min, unsigned long max, unsigned long *data)
{
	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_NUM) {
		load_err(o, "number expected");
		return 0;
	}
	if (o->longval < min || o->longval > max) {
		load_err(o, "number out of allowed range");
		return 0;
	}
	*data = (unsigned)o->longval;
	return 1;
}

unsigned
load_delta(struct load *o, unsigned *delta)
{
	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_NUM) {
		load_err(o, "numeric constant expected in event offset");
		return 0;
	}
	*delta = o->longval;
	if (!load_nl(o))
		return 0;
	return 1;
}

unsigned
load_chan(struct load *o, unsigned long *dev, unsigned long *ch)
{
	if (!load_getsym(o))
		return 0;
	if (o->id == TOK_LBRACE) {
		if (!load_long(o, 0, EV_MAXDEV, dev))
			return 0;
		if (!load_long(o, 0, EV_MAXCH, ch))
			return 0;
		if (!load_getsym(o))
			return 0;
		if (o->id != TOK_RBRACE) {
			load_err(o, "'}' expected in channel spec");
			return 0;
		}
		return 1;
	} else if (o->id == TOK_NUM) {
		*dev = o->longval / (EV_MAXCH + 1);
		*ch = o->longval % (EV_MAXCH + 1);
		if (*dev > EV_MAXDEV) {
			load_err(o, "dev/chan out of range");
			return 0;
		}
		return 1;
	} else {
		load_err(o, "bad channel spec");
		return 0;
	}
}

unsigned
load_bank(struct load *o, unsigned long *rbank)
{
	if (!load_getsym(o))
		return 0;
	if (o->id == TOK_NIL) {
		*rbank = EV_UNDEF;
	} else {
		load_ungetsym(o);
		if (!load_long(o, 0, EV_MAXFINE, rbank))
			return 0;
	}
	return 1;
}

unsigned
load_ev(struct load *o, struct ev *ev)
{
	unsigned long val, val2;
	struct evinfo *ei;

	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_WORD) {
		load_err(o, "event name expected");
		return 0;
	}
	if (!ev_str2cmd(ev, o->strval)) {
	ignore:
		load_ungetsym(o);
		if (!load_ukline(o)) {
			return 0;
		}
		load_err(o, "unknown event, ignored");
		ev->cmd = EV_NULL;
		return 1;
	}
	if (EV_ISVOICE(ev)) {
		if (!load_chan(o, &val, &val2)) {
			return 0;
		}
		ev->dev = val;
		ev->ch = val2;
	}
	switch (ev->cmd) {
	case EV_TEMPO:
		if (!load_long(o, TEMPO_MIN, TEMPO_MAX, &val)) {
			return 0;
		}
		ev->tempo_usec24 = val;
		break;
	case EV_TIMESIG:
		if (!load_long(o, 1, TIMESIG_BEATS_MAX, &val)) {
			return 0;
		}
		ev->timesig_beats = val;
		if (!load_long(o, 1, TIMESIG_TICS_MAX, &val)) {
			return 0;
		}
		ev->timesig_tics = val;
		break;
	case EV_NRPN:
	case EV_RPN:
		if (!load_long(o, 0, EV_MAXFINE, &val)) {
			return 0;
		}
		ev->rpn_num = val;
		if (!load_long(o, 0, EV_MAXFINE, &val)) {
			return 0;
		}
		ev->rpn_val = val;
		break;
	case EV_XCTL:
		if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->ctl_num = val;
		if (!load_long(o, 0, EV_MAXFINE, &val)) {
			return 0;
		}
		ev->ctl_val = val;
		break;
	case EV_XPC:
		/* on older versions prog and bank are swapped */
		if (o->format <= 0) {
			if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
				return 0;
			}
			ev->pc_prog = val;
			if (!load_bank(o, &val)) {
				return 0;
			}
			ev->pc_bank = val;
		} else {
			if (!load_bank(o, &val)) {
				return 0;
			}
			ev->pc_bank = val;
			if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
				return 0;
			}
			ev->pc_prog = val;
		}
		break;
	case EV_NON:
	case EV_NOFF:
	case EV_CTL:
		if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v0 = val;
		if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v1 = val;
		break;
	case EV_KAT:
		if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
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
		if (!load_getsym(o))
			return 0;
		if (o->id != TOK_NUM) {
			load_ungetsym(o);
			if (!load_nl(o)) {
				return 0;
			}
			ev->cmd = EV_NULL;
			return 1;
		}
		load_ungetsym(o);
		if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v1 = val;
		break;
	case EV_PC:
		if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->cmd = EV_XPC;
		ev->v0 = EV_UNDEF;
		ev->v1 = val;
		break;
	case EV_CAT:
		if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v0 = val;
		break;
	case EV_BEND:
		if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v0 = val;
		if (!load_long(o, 0, EV_MAXCOARSE, &val)) {
			return 0;
		}
		ev->v0 += (val << 7);
		break;
	default:
		if (EV_ISSX(ev) && evinfo[ev->cmd].ev != NULL) {
			ei = evinfo + ev->cmd;
			if (!load_long(o, 0, EV_MAXDEV, &val))
				return 0;
			ev->dev = val;
			if (ei->nparams >= 1) {
				if (!load_long(o, 0, ei->v0_max, &val))
					return 0;
				ev->v0 = val;
			}
			if (ei->nparams >= 2) {
				if (!load_long(o, 0, ei->v1_max, &val))
					return 0;
				ev->v1 = val;
			}
		} else
			goto ignore;
	}
	if (!load_nl(o)) {
		return 0;
	}
	return 1;
}


unsigned
load_track(struct load *o, struct track *t)
{
	unsigned delta;
	struct seqev *pos, *se;
	struct statelist slist;
	struct ev ev, rev;

	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing track");
		return 0;
	}
	track_clear(t);
	statelist_init(&slist);
	pos = t->first;
	for (;;) {
		if (!load_getsym(o)) {
			statelist_done(&slist);
			return 0;
		}
		if (o->id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->id == TOK_RBRACE) {
			break;
		} else if (o->id == TOK_NUM) {
			load_ungetsym(o);
			if (!load_delta(o, &delta)) {
				statelist_done(&slist);
				return 0;
			}
			pos->delta += delta;
		} else {
			load_ungetsym(o);
			if (!load_ev(o, &ev)) {
				statelist_done(&slist);
				return 0;
			}
			if (ev.cmd != EV_NULL) {
				if (conv_packev(&slist, 0U,
					CONV_XPC | CONV_NRPN | CONV_RPN,
					&ev, &rev)) {
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
load_range(struct load *o, unsigned min, unsigned max,
    unsigned *rmin, unsigned *rmax)
{
	unsigned long tmin, tmax;

	if (!load_long(o, min, max, &tmin))
		return 0;
	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_RANGE) {
		load_ungetsym(o);
		*rmin = *rmax = tmin;
		return 1;
	}
	if (!load_long(o, min, max, &tmax))
		return 0;
	*rmin = tmin;
	*rmax = tmax;
	return 1;
}

unsigned
load_evspec(struct load *o, struct evspec *es)
{
	struct evinfo *info;

	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_WORD) {
		load_err(o, "event spec name expected");
		return 0;
	}
	if (!evspec_str2cmd(es, o->strval))
		return 0;
	info = &evinfo[es->cmd];
	if ((info->flags & EV_HAS_DEV) && (info->flags & EV_HAS_CH)) {
		if (!load_getsym(o))
			return 0;
		if (o->id != TOK_LBRACE) {
			load_err(o, "'{' expected");
			return 0;
		}
		if (!load_range(o, 0, EV_MAXDEV, &es->dev_min, &es->dev_max) ||
		    !load_range(o, 0, EV_MAXCH, &es->ch_min, &es->ch_max))
			return 0;
		if (!load_getsym(o))
			return 0;
		if (o->id != TOK_RBRACE) {
			load_err(o, "'}' expected");
			return 0;
		}
	}
	if (info->nranges == 0)
		return 1;
	if (!load_getsym(o))
		return 0;
	if (o->id == TOK_NIL) {
		es->v0_min = es->v0_max = EV_UNDEF;
	} else {
		load_ungetsym(o);
		if (!load_range(o, 0, info->v0_max, &es->v0_min, &es->v0_max))
			return 0;
	}
	if (info->nranges == 1)
		return 1;
	if (!load_range(o, 0, info->v1_max, &es->v1_min, &es->v1_max))
		return 0;
	return 1;
}

unsigned
load_rule(struct load *o, struct filt *f)
{
	unsigned long idev, ich, odev, och, ictl, octl, keylo, keyhi, ukeyplus;
	struct evspec from, to;
	int keyplus;

	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_WORD) {
		load_err(o, "filter-type identifier expected");
		return 0;
	}
	if (str_eq(o->strval, "keydrop")) {
		if (!load_chan(o, &idev, &ich))
			return 0;
		if (!load_long(o, 0, EV_MAXCOARSE, &keylo))
			return 0;
		if (!load_long(o, 0, EV_MAXCOARSE, &keyhi))
			return 0;
		evspec_reset(&from);
		from.cmd = EVSPEC_NOTE;
		from.dev_min = from.dev_max = idev;
		from.ch_min = from.ch_max = ich;
		from.v0_min = keylo;
		from.v0_max = keyhi;
		to.cmd = EVSPEC_EMPTY;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->strval, "keymap")) {
		if (!load_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!load_chan(o, &odev, &och)) {
			return 0;
		}
		if (!load_long(o, 0, EV_MAXCOARSE, &keylo)) {
			return 0;
		}
		if (!load_long(o, 0, EV_MAXCOARSE, &keyhi)) {
			return 0;
		}
		if (!load_long(o, 0, EV_MAXCOARSE, &ukeyplus)) {
			return 0;
		}
		if (ukeyplus > 63) {
			keyplus = ukeyplus - 128;
		} else {
			keyplus = ukeyplus;
		}
		if (!load_getsym(o))
			return 0;
		if (o->id != TOK_WORD) {
			load_err(o, "curve identifier expected");
			return 0;
		}
		if ((int)keyhi + keyplus > EV_MAXCOARSE)
			keyhi = EV_MAXCOARSE - keyplus;
		if ((int)keylo + keyplus < 0)
			keylo = -keyplus;
		if (keylo >= keyhi) {
			load_err(o, "bad range in keymap rule");
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
	} else if (str_eq(o->strval, "ctldrop")) {
		if (!load_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!load_long(o, 0, EV_MAXCOARSE, &ictl)) {
			return 0;
		}
		evspec_reset(&from);
		from.cmd = EVSPEC_XCTL;
		from.dev_min = from.dev_max = idev;
		from.ch_min = from.ch_max = ich;
		from.v0_min = from.v0_max = ictl;
		to.cmd = EVSPEC_EMPTY;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->strval, "ctlmap")) {
		if (!load_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!load_chan(o, &odev, &och)) {
			return 0;
		}
		if (!load_long(o, 0, EV_MAXCOARSE, &ictl)) {
			return 0;
		}
		if (!load_long(o, 0, EV_MAXCOARSE, &octl)) {
			return 0;
		}
		if (!load_getsym(o))
			return 0;
		if (o->id != TOK_WORD) {
			load_err(o, "curve identifier expected");
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
	} else if (str_eq(o->strval, "chandrop")) {
		if (!load_chan(o, &idev, &ich)) {
			return 0;
		}
		evspec_reset(&from);
		from.dev_min = from.dev_max = idev;
		from.ch_min = from.ch_max = ich;
		to.cmd = EVSPEC_EMPTY;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->strval, "chanmap")) {
		if (!load_chan(o, &idev, &ich)) {
			return 0;
		}
		if (!load_chan(o, &odev, &och)) {
			return 0;
		}
		evspec_reset(&from);
		evspec_reset(&to);
		from.dev_min = from.dev_max = idev;
		from.ch_min = from.ch_max = ich;
		to.dev_min = to.dev_max = odev;
		to.ch_min = to.ch_max = och;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->strval, "devdrop")) {
		if (!load_long(o, 0, EV_MAXDEV, &idev)) {
			return 0;
		}
		evspec_reset(&from);
		from.dev_min = from.dev_max = idev;
		to.cmd = EVSPEC_EMPTY;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->strval, "devmap")) {
		if (!load_long(o, 0, EV_MAXDEV, &idev)) {
			return 0;
		}
		if (!load_long(o, 0, EV_MAXDEV, &odev)) {
			return 0;
		}
		evspec_reset(&from);
		evspec_reset(&to);
		from.dev_min = from.dev_max = idev;
		to.dev_min = to.dev_max = odev;
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->strval, "evmap")) {
		if (!load_evspec(o, &from)) {
			return 0;
		}
		if (!load_getsym(o))
			return 0;
		if (o->id != TOK_GT) {
			load_err(o, "'>' expected");
			return 0;
		}
		if (!load_evspec(o, &to)) {
			return 0;
		}
		filt_mapnew(f, &from, &to);
	} else if (str_eq(o->strval, "transp")) {
		if (!load_evspec(o, &to)) {
			return 0;
		}
		if (!load_long(o, 0, EV_MAXCOARSE, &ukeyplus)) {
			return 0;
		}
		filt_transp(f, &to, ukeyplus);
	} else if (str_eq(o->strval, "vcurve")) {
		if (!load_evspec(o, &to)) {
			return 0;
		}
		if (!load_long(o, 1, EV_MAXCOARSE, &ukeyplus)) {
			return 0;
		}
		filt_vcurve(f, &to, ukeyplus);
	} else {
		load_ungetsym(o);
		if (!load_ukline(o)) {
			return 0;
		}
		load_err(o, "unknown filter rule, ignored");
		return 1;
	}
	if (!load_nl(o)) {
		return 0;
	}
	return 1;
}

unsigned
load_filt(struct load *o, struct filt *f)
{
	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing filt");
		return 0;
	}
	for (;;) {
		if (!load_getsym(o))
			return 0;
		if (o->id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->id == TOK_RBRACE) {
			break;
		} else {
			load_ungetsym(o);
			if (!load_rule(o, f))
				return 0;
		}
	}
	return 1;
}

unsigned
load_sysex(struct load *o, struct sysex **res)
{
	struct sysex *sx;
	unsigned long val;

	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing sysex");
		return 0;
	}
	sx = sysex_new(0);
	for (;;) {
		if (!load_getsym(o))
			goto err1;
		if (o->id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->id == TOK_RBRACE) {
			break;
		} else if (o->id == TOK_WORD) {
			if (str_eq(o->strval, "data")) {
				for (;;) {
					if (!load_getsym(o))
						goto err1;
					if (o->id == TOK_ENDLINE) {
						break;
					}
					load_ungetsym(o);
					if (!load_long(o, 0, 0xff, &val)) {
						goto err1;
					}
					sysex_add(sx, val);
				}
			} else if (str_eq(o->strval, "unit")) {
				if (!load_long(o, 0, EV_MAXDEV, &val)) {
					goto err1;
				}
				sx->unit = val;
				if (!load_nl(o)) {
					goto err1;
				}
			} else {
				goto unknown;
			}
		} else {
		unknown:
			load_ungetsym(o);
			if (!load_ukline(o)) {
				goto err1;
			}
			load_err(o, "unknown line format in sysex, ignored");
		}
	}
	*res = sx;
	return 1;

err1:
	sysex_del(sx);
	return 0;
}

unsigned
load_evpat(struct load *o, char *ref)
{
	unsigned long val;
	unsigned size = 0;
	unsigned char *pattern;
	char *name;
	unsigned cmd;

	/*
	 * find a free slot
	 */
	if (evpat_lookup(ref, &cmd))
		evpat_unconf(cmd);
	for (cmd = EV_PAT0;; cmd++) {
		if (cmd == EV_PAT0 + EV_NPAT) {
			load_err(o, "too many sysex patterns");
			return 0;
		}
		if (evinfo[cmd].ev == NULL)
			break;
	}
	name = str_new(ref);
	pattern = xmalloc(EV_PATSIZE, "evpat");

	if (!load_getsym(o))
		goto err1;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing evpat");
		goto err1;
	}

	for (;;) {
		if (!load_getsym(o))
			goto err1;
		if (o->id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->id == TOK_RBRACE) {
			break;
		} else if (o->id == TOK_WORD) {
			if (str_eq(o->strval, "pattern")) {
				for (;;) {
					if (!load_getsym(o))
						goto err1;
					if (o->id == TOK_ENDLINE) {
						break;
					}
					if (size == EV_PATSIZE) {
						load_err(o, "pattern too long");
						goto err1;
					}
					if (o->id == TOK_NUM) {
						load_ungetsym(o);
						if (!load_long(o, 0, 0xff, &val)) {
							goto err1;
						}
						pattern[size++] = val;
					} else if (o->id == TOK_WORD) {
						if (str_eq(o->strval, "v0_hi"))
							val = EV_PATV0_HI;
						else if (str_eq(o->strval, "v0_lo"))
							val = EV_PATV0_LO;
						else if (str_eq(o->strval, "v1_hi"))
							val = EV_PATV1_HI;
						else if (str_eq(o->strval, "v1_lo"))
							val = EV_PATV1_LO;
						else {
							load_err(o, "unexpected atom in pattern");
							return 0;
						}
						pattern[size++] = val;
					}
				}
			} else {
				goto unknown;
			}
		} else {
		unknown:
			load_ungetsym(o);
			if (!load_ukline(o)) {
				goto err1;
			}
			load_err(o, "unknown line format in sysex, ignored");
		}
	}
	if (!evpat_set(cmd, name, pattern, size))
		goto err1;
	return 1;
err1:
	str_delete(name);
	xfree(pattern);
	return 0;
}

unsigned
load_songchan(struct load *o, struct song *s, struct songchan *i)
{
	unsigned long val, val2;

	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing songchan");
		return 0;
	}
	for (;;) {
		if (!load_getsym(o))
			return 0;
		if (o->id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->id == TOK_RBRACE) {
			break;
		} else if (o->id == TOK_WORD) {
			if (str_eq(o->strval, "conf")) {
				if (!load_track(o, &i->conf))
					return 0;
				if (!load_nl(o))
					return 0;
				track_setchan(&i->conf, i->dev, i->ch);
			} else if (str_eq(o->strval, "chan")) {
				if (!load_chan(o, &val, &val2))
					return 0;
				i->dev = val;
				i->ch = val2;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "curinput")) {
				if (!load_chan(o, &val, &val2))
					return 0;
				if (!load_nl(o))
					return 0;
				load_err(o,
				    "ignored obsolete 'curinput' line");
			} else {
				goto unknown;
			}
		} else {
		unknown:
			load_ungetsym(o);
			if (!load_ukline(o)) {
				return 0;
			}
			load_err(o, "unknown line format, ignored");
		}
	}
	return 1;
}

unsigned
load_songtrk(struct load *o, struct song *s, struct songtrk *t)
{
	struct songfilt *f;
	unsigned long val;

	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing songtrk");
		return 0;
	}
	for (;;) {
		if (!load_getsym(o))
			return 0;
		if (o->id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->id == TOK_RBRACE) {
			break;
		} else if (o->id == TOK_WORD) {
			if (str_eq(o->strval, "curfilt")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				f = song_filtlookup(s, o->strval);
				if (!f) {
					f = song_filtnew(s, o->strval);
					song_setcurfilt(s, NULL);
				}
				t->curfilt = f;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "track")) {
				if (!load_track(o, &t->track))
					return 0;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "mute")) {
				if (!load_long(o, 0, 1, &val))
					return 0;
				t->mute = val;
				if (!load_nl(o))
					return 0;
			} else
				goto unknown;
		} else {
		unknown:
			load_ungetsym(o);
			if (!load_ukline(o))
				return 0;
			load_err(o, "unknown line format, ignored");
		}
	}

	return 1;
}

unsigned
load_songfilt(struct load *o, struct song *s, struct songfilt *g)
{
	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing songfilt");
		return 0;
	}
	for (;;) {
		if (!load_getsym(o))
			return 0;
		if (o->id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->id == TOK_RBRACE) {
			break;
		} else if (o->id == TOK_WORD) {
			if (str_eq(o->strval, "curchan")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				if (!load_nl(o))
					return 0;
				load_err(o,
				    "ignored obsolete 'curchan' line");
			} else if (str_eq(o->strval, "filt")) {
				if (!load_filt(o, &g->filt))
					return 0;
			} else
				goto unknown;
		} else {
		unknown:
			load_ungetsym(o);
			if (!load_ukline(o))
				return 0;
			load_err(o, "unknown line format, ignored");
		}
	}
	return 1;
}

unsigned
load_songsx(struct load *o, struct song *s, struct songsx *g)
{
	struct sysex *sx;

	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing songsx");
		return 0;
	}
	for (;;) {
		if (!load_getsym(o))
			return 0;
		if (o->id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->id == TOK_RBRACE) {
			break;
		} else if (o->id == TOK_WORD) {
			if (str_eq(o->strval, "sysex")) {
				if (!load_sysex(o, &sx))
					return 0;
				sysexlist_put(&g->sx, sx);
			} else {
				goto unknown;
			}
		} else {
		unknown:
			load_ungetsym(o);
			if (!load_ukline(o))
				return 0;
			load_err(o, "unknown line format, ignored");
		}
	}
	return 1;
}

unsigned
load_metro(struct load *o, struct metro *m)
{
	struct ev ev;
	unsigned mask;

	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing metro");
		return 0;
	}
	for (;;) {
		if (!load_getsym(o))
			return 0;
		if (o->id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->id == TOK_RBRACE) {
			break;
		} else if (o->id == TOK_WORD) {
			if (str_eq(o->strval, "enabled")) {
				if (!load_getsym(o))
					return 0;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "mask")) {
				if (!load_getsym(o))
					return 0;
				if (!metro_str2mask(m, o->strval, &mask)) {
					load_err(o, "skipped unknown "
					    "metronome mask");
				}
				metro_setmask(m, mask);
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "lo")) {
				if (!load_ev(o, &ev))
					return 0;
				if (ev.cmd != EV_NON) {
					load_err(o, "'lo' must be followed "
					  "by a 'non' event");
				} else {
					m->lo = ev;
				}
			} else if (str_eq(o->strval, "hi")) {
				if (!load_ev(o, &ev))
					return 0;
				if (ev.cmd != EV_NON) {
					load_err(o, "'hi' must be followed "
					    "by a 'non' event\n");
				} else
					m->hi = ev;
			} else {
				goto unknown;
			}
		} else {
		unknown:
			load_ungetsym(o);
			if (!load_ukline(o))
				return 0;
			load_err(o, "unknown line format, ignored");
		}
	}
	return 1;
}

unsigned
load_song(struct load *o, struct song *s)
{
	struct songtrk *t;
	struct songchan *i;
	struct songfilt *g;
	struct songsx *l;
	struct evspec es;
	unsigned long num, num2;
	int input;

	if (!load_getsym(o))
		return 0;
	if (o->id != TOK_LBRACE) {
		load_err(o, "'{' expected while parsing song");
		return 0;
	}
	for (;;) {
		if (!load_getsym(o)) {
			return 0;
		}
		if (o->id == TOK_ENDLINE) {
			/* nothing */
		} else if (o->id == TOK_RBRACE) {
			break;
		} else if (o->id == TOK_WORD) {
			if (str_eq(o->strval, "format")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_NUM) {
					load_err(o, "version number expected");
					return 0;
				}
				if (o->longval > FORMAT_VERSION) {
					cons_err("Warning: midish version "
					    "too old to read this file.");
				}
				o->format = o->longval;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "evpat")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				if (!load_evpat(o, o->strval))
					return 0;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "songtrk")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				t = song_trklookup(s, o->strval);
				if (t == NULL) {
					t = song_trknew(s, o->strval);
					song_setcurtrk(s, NULL);
				}
				if (!load_songtrk(o, s, t))
					return 0;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "songin")) {
				input = 1;
				goto chan_do;
			} else if (str_eq(o->strval, "songout") ||
				str_eq(o->strval, "songchan")) {
				input = 0;
			chan_do:
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				i = song_chanlookup(s, o->strval, input);
				if (i == NULL) {
					i = song_channew(s, o->strval,
					    0, 0, input);
					song_setcurchan(s, NULL, input);
				}
				if (!load_songchan(o, s, i))
					return 0;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "songfilt")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				g = song_filtlookup(s, o->strval);
				if (!g) {
					g = song_filtnew(s, o->strval);
					song_setcurfilt(s, NULL);
				}
				if (!load_songfilt(o, s, g))
					return 0;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "songsx")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				l = song_sxlookup(s, o->strval);
				if (!l) {
					l = song_sxnew(s, o->strval);
					song_setcursx(s, NULL);
				}
				if (!load_songsx(o, s, l))
					return 0;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "meta")) {
				if (!load_track(o, &s->meta))
					return 0;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "tics_per_unit")) {
				if (!load_long(o, 0, ~1U, &num))
					return 0;
				if ((num % 96) != 0) {
					load_err(o, "warning, rounding "
					    "tic_per_unit to a multiple "
					    "of 96");
					num = 96 * (num / 96);
					if (num < 96) {
						num = 96;
					}
				}
				if (!load_nl(o))
					return 0;
				s->tics_per_unit = num;
			} else if (str_eq(o->strval, "tempo_factor")) {
				if (!load_long(o, 0, ~1U, &num))
					return 0;
				if (!load_nl(o))
					return 0;
				if (num < 0x80 || num > 0x200) {
					load_err(o, "warning, tempo factor "
					    "out of bounds\n");
				} else
					s->tempo_factor = num;
			} else if (str_eq(o->strval, "curtrk")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				t = song_trklookup(s, o->strval);
				if (t) {
					s->curtrk = t;
				} else {
					load_err(o, "warning, can't set "
					  "current track, not such track");
				}
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "curfilt")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				g = song_filtlookup(s, o->strval);
				if (g) {
					s->curfilt = g;
				} else {
					load_err(o, "warning, can't set "
					    "current filt, not such filt");
				}
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "cursx")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				l = song_sxlookup(s, o->strval);
				if (l) {
					s->cursx = l;
				} else {
					load_err(o, "warning, cant set "
					    "current sysex, not such sysex");
				}
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "curin")) {
				input = 1;
				goto curchan_do;
			} else if (str_eq(o->strval, "curout") ||
			    str_eq(o->strval, "curchan")) {
				input = 0;
			curchan_do:
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "word expected");
					return 0;
				}
				i = song_chanlookup(s, o->strval, input);
				if (i) {
					song_setcurchan(s, i, input);
				} else {
					load_err(o, "warning, cant set "
					    "current chan, not such chan");
				}
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "curpos")) {
				if (!load_long(o, 0, ~1U, &num))
					return 0;
				if (!load_nl(o))
					return 0;
				s->curpos = num;
			} else if (str_eq(o->strval, "curlen")) {
				if (!load_long(o, 0, ~1U, &num))
					return 0;
				if (!load_nl(o))
					return 0;
				s->curlen = num;
			} else if (str_eq(o->strval, "curquant")) {
				if (!load_long(o, 0, ~1U, &num))
					return 0;
				if (num > s->tics_per_unit) {
					load_err(o, "warning, truncated "
					    "curquant because it was "
					    "too large");
					num = s->tics_per_unit;
				}
				if (!load_nl(o))
					return 0;
				s->curquant = num;
			} else if (str_eq(o->strval, "curev")) {
				if (!load_evspec(o, &es))
					return 0;
				if (!load_nl(o))
					return 0;
				s->curev = es;
			} else if (str_eq(o->strval, "curinput")) {
				if (!load_chan(o, &num, &num2))
					return 0;
				i = song_chanlookup_bynum(s, num, num2, 1);
				if (i == NULL) {
					i = song_channew(s, "old_style_curin",
					    num, num2, 1);
					song_setcurchan(s, NULL, 1);
				} else {
					i->dev = num;
					i->ch = num2;
				}
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "metro")) {
				if (!load_metro(o, &s->metro))
					return 0;
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "tap")) {
				if (!load_getsym(o))
					return 0;
				if (o->id != TOK_WORD) {
					load_err(o, "expected tap mode\n");
					goto unknown;
				}
				if (str_eq(o->strval, "off"))
					s->tap_mode = SONG_TAP_OFF;
				else if (str_eq(o->strval, "start"))
					s->tap_mode = SONG_TAP_START;
				else if (str_eq(o->strval, "tempo"))
					s->tap_mode = SONG_TAP_TEMPO;
				else {
					load_err(o, "unknown tap mode\n");
					goto unknown;
				}
				if (!load_nl(o))
					return 0;
			} else if (str_eq(o->strval, "tapev")) {
				if (!load_evspec(o, &es))
					return 0;
				if (!load_nl(o))
					return 0;
				s->tap_evspec = es;
			} else
				goto unknown;
		} else {
		unknown:
			load_ungetsym(o);
			if (!load_ukline(o))
				return 0;
			load_err(o, "unknown line format, ignored");
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
	struct load p;
	unsigned res;

	if (!load_init(&p, filename))
		return 0;
	res = load_empty(&p);
	if (res != 0) {
		res = load_song(&p, o);
	}
	load_done(&p);
	return res;
}
