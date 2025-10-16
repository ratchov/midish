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

/*
 * ev is an "extended" midi event
 */
#include <stdio.h>
#include "utils.h"
#include "ev.h"
#include "str.h"
#include "cons.h"

struct evinfo evinfo[EV_NUMCMD] =
{
	{ "nil", "none",
	  0,
	  0, 0,
	  0, 0, 0, 0,
	  NULL
	},
	{ NULL,	"any",
	  EV_HAS_DEV | EV_HAS_CH,
	  0, 0,
	  0, 0, 0, 0,
	  NULL
	},
	{ "tempo", NULL,
	  0,
	  1, 0xdeadbeef,
	  TEMPO_MIN, TEMPO_MAX, 0, 0,
	  NULL
	},
	{ "timesig", NULL,
	  0,
	  2, 0xdeadbeef,
	  1, 16, 1, 32,
	  NULL
	},
	{ "nrpn", "nrpn",
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 2,
	  0, EV_MAXFINE, 0, EV_MAXFINE,
	  NULL
	},
	{ "rpn", "rpn",
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 2,
	  0, EV_MAXFINE, 0, EV_MAXFINE,
	  NULL
	},
	{ "xctl", "xctl",
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 1,
	  0, EV_MAXCOARSE, 0, EV_MAXFINE,
	  NULL
	},
	{ "xpc", "xpc",
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 2,
	  0, EV_MAXFINE, 0, EV_MAXCOARSE,
	  NULL
	},
	{ "noff", NULL,
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 0xdeadbeef,
	  0, EV_MAXCOARSE, 0, EV_MAXCOARSE,
	  NULL
	},
	{ "non", "note",
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 1,
	  0, EV_MAXCOARSE, 0, EV_MAXCOARSE,
	  NULL
	},
	{ "kat", NULL,
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 0xdeadbeef,
	  0, EV_MAXCOARSE, 0, EV_MAXCOARSE,
	  NULL
	},
	{ "ctl", "ctl",
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 1,
	  0, EV_MAXCOARSE, 0, EV_MAXCOARSE,
	  NULL
	},
	{ "pc", "pc",
	  EV_HAS_DEV | EV_HAS_CH,
	  1, 1,
	  0, EV_MAXCOARSE, 0, 0,
	  NULL
	},
	{ "cat", "cat",
	  EV_HAS_DEV | EV_HAS_CH,
	  1, 0,
	  0, EV_MAXCOARSE, 0, 0,
	  NULL
	},
	{ "bend", "bend",
	  EV_HAS_DEV | EV_HAS_CH,
	  1, 0,
	  0, EV_MAXFINE, 0, 0,
	  NULL
	},
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* unused slot */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 0 */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 1 */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 2 */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 3 */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 4 */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 5 */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 6 */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 7 */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 8 */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 9 */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 0xa */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 0xb */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 0xc */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 0xd */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }, /* sysex pattern 0xe */
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL }  /* sysex pattern 0xf */

};

struct evctl evctl_tab[EV_MAXCOARSE + 1];

/*
 * return the 'name' of the given event
 */
char *
ev_getstr(struct ev *ev)
{
	if (ev->cmd >= EV_NUMCMD)
		return NULL;

	return evinfo[ev->cmd].ev;
}

/*
 * find the event EV_XX constant corresponding to the given string
 */
unsigned
ev_str2cmd(struct ev *ev, char *str)
{
	unsigned i;

	for (i = 0; i < EV_NUMCMD; i++) {
		if (evinfo[i].ev && str_eq(evinfo[i].ev, str)) {
			ev->cmd = i;
			return 1;
		}
	}
	return 0;
}

/*
 * return the phase of the event within a frame:
 *
 *    -	EV_PHASE_FIRST is set if the event can be the first event in a
 *	sequence (example: note-on, bender != 0x4000)
 *
 *    -	EV_PHASE_NEXT is set if the given event can be the next event
 *	in a frame after a 'first event' but not the last one
 *	(example: key after-touch, beder != 0x4000)
 *
 *    -	EV_PHASE_LAST is set if the given event can be the last event
 *	in a frame (example: note-off, any unknown controller)
 */
unsigned
ev_phase(struct ev *ev)
{
	unsigned phase;

	switch(ev->cmd) {
	case EV_NOFF:
		phase = EV_PHASE_LAST;
		break;
	case EV_NON:
		phase = EV_PHASE_FIRST;
		break;
	case EV_KAT:
		phase = EV_PHASE_NEXT;
		break;
	case EV_CAT:
		if (ev->cat_val != EV_CAT_DEFAULT) {
			phase = EV_PHASE_FIRST | EV_PHASE_NEXT;
		} else {
			phase = EV_PHASE_LAST;
		}
		break;
	case EV_XCTL:
		if (!EV_CTL_ISFRAME(ev)) {
			phase = EV_PHASE_FIRST | EV_PHASE_LAST;
		} else {
			if (ev->ctl_val != EV_CTL_DEFVAL(ev)) {
				phase = EV_PHASE_FIRST | EV_PHASE_NEXT;
			} else {
				phase = EV_PHASE_LAST;
			}
		}
		break;
	case EV_BEND:
		if (ev->bend_val != EV_BEND_DEFAULT) {
			phase = EV_PHASE_FIRST | EV_PHASE_NEXT;
		} else {
			phase = EV_PHASE_LAST;
		}
		break;
	default:
		phase = EV_PHASE_FIRST | EV_PHASE_LAST;
		break;
	}
	return phase;
}

/*
 * check if the given event matches the given frame (if so, this means
 * that, iether the event is part of the frame, either there is a
 * conflict between the frame and the event)
 */
unsigned
ev_eq(struct ev *e1, struct ev *e2)
{
	unsigned nparams;

	if (e1->cmd != e2->cmd)
		return 0;

	if ((evinfo[e1->cmd].flags & EV_HAS_DEV) && e1->dev != e2->dev)
		return 0;

	if ((evinfo[e1->cmd].flags & EV_HAS_CH) && e1->ch != e2->ch)
		return 0;

	nparams = evinfo[e1->cmd].nparams;
	if (nparams > 0) {
		if (e1->v0 != e2->v0)
			return 0;
		if (nparams > 1) {
			if (e1->v1 != e2->v1)
				return 0;
		}
	}
	return 1;
}

/*
 * check if the given event matches the given frame (if so, this means
 * that, iether the event is part of the frame, either there is a
 * conflict between the frame and the event)
 */
unsigned
ev_match(struct ev *st, struct ev *ev)
{
	switch (st->cmd) {
	case EV_NON:
	case EV_NOFF:
	case EV_KAT:
		if (!EV_ISNOTE(ev) ||
		    st->note_num != ev->note_num ||
		    st->ch != ev->ch ||
		    st->dev != ev->dev) {
			return 0;
		}
		break;
	case EV_XCTL:
	case EV_NRPN:
	case EV_RPN:
		if (st->cmd != ev->cmd ||
		    st->dev != ev->dev ||
		    st->ch != ev->ch ||
		    st->v0 != ev->v0) {
			return 0;
		}
		break;
	case EV_BEND:
	case EV_CAT:
	case EV_XPC:
		if (st->cmd != ev->cmd ||
		    st->dev != ev->dev ||
		    st->ch != ev->ch) {
			return 0;
		}
		break;
	case EV_TEMPO:
	case EV_TIMESIG:
		if (st->cmd != ev->cmd) {
			return 0;
		}
		break;
	default:
		if (EV_ISSX(st)) {
			if (ev->cmd != st->cmd)
				return 0;
			break;
		}
		logx(1, "%s: {ev:%p}: bad event type", __func__, st);
		panic();
		break;
	}
	return 1;
}

/*
 * dump the event structure on stderr, for debug purposes
 */
size_t
ev_fmt(char *buf, size_t bufsz, struct ev *ev)
{
	char *cmdstr;

	cmdstr = ev_getstr(ev);
	if (cmdstr == NULL)
		return snprintf(buf, bufsz, "unkw(%d)", ev->cmd);

	switch(ev->cmd) {
	case EV_NON:
	case EV_NOFF:
	case EV_KAT:
	case EV_CTL:
	case EV_NRPN:
	case EV_RPN:
	case EV_XPC:
	case EV_XCTL:
		return snprintf(buf, bufsz, "%s {%x %x} %02x %02x",
		    cmdstr, ev->dev, ev->ch, ev->v0, ev->v1);
	case EV_BEND:
	case EV_CAT:
	case EV_PC:
		return snprintf(buf, bufsz, "%s {%x %x} %02x",
		    cmdstr, ev->dev, ev->ch, ev->v0);
	case EV_TEMPO:
		return snprintf(buf, bufsz, "%s %u", cmdstr, ev->tempo_usec24);
	case EV_TIMESIG:
		return snprintf(buf, bufsz, "%s %u %u", cmdstr, ev->v0, ev->v1);
	default:
		if (!EV_ISSX(ev))
			return snprintf(buf, bufsz, "%s", cmdstr);

		switch (evinfo[ev->cmd].nparams) {
		case 0:
			return snprintf(buf, bufsz,
			    "%s %x", cmdstr, ev->dev);
		case 1:
			return snprintf(buf, bufsz,
			    "%s %x %02x", cmdstr, ev->dev, ev->v0);
		default:
			return snprintf(buf, bufsz,
			    "%s %x %02x %02x", cmdstr, ev->dev, ev->v0, ev->v1);
		}
	}
}

/*
 * transform "in" (matching "from" spec) into "out" so it matches "to"
 * spec. The "from" and "to" specs _must_ have the same dev, ch, v0 and
 * v1 ranges (to ensure the mapping must be bijective).  This routine is
 * supposed to be fast since it's called for each incoming event.
 */
void
ev_map(struct ev *in, struct evspec *from, struct evspec *to, struct ev *out)
{
	if (from->cmd == EVSPEC_ANY) {
		out->cmd = in->cmd;
		out->dev = in->dev - from->dev_min + to->dev_min;
		out->ch = in->ch - from->ch_min + to->ch_min;
		out->v0 = in->v0;
		out->v1 = in->v1;
		return;
	}
	if (from->cmd == EVSPEC_NOTE)
		out->cmd = in->cmd;
	else
		out->cmd = to->cmd;
	if (evinfo[out->cmd].flags & EV_HAS_DEV) {
		out->dev = to->dev_min;
		if (evinfo[from->cmd].flags & EV_HAS_DEV)
			out->dev += in->dev - from->dev_min;
	}
	if (evinfo[out->cmd].flags & EV_HAS_CH) {
		out->ch = to->ch_min;
		if (evinfo[from->cmd].flags & EV_HAS_CH)
			out->ch += in->ch - from->ch_min;
	}
	switch (evinfo[from->cmd].nparams) {
	case 0:
		switch (evinfo[to->cmd].nparams) {
		case 0:
			break;
		case 1:
			out->v0 = to->v0_min;
			break;
		case 2:
			out->v0 = to->v0_min;
			out->v1 = to->v1_min;
			break;
		}
		break;
	case 1:
		switch (evinfo[to->cmd].nparams) {
		case 0:
			break;
		case 1:
			out->v0 = in->v0 - from->v0_min + to->v0_min;
			break;
		case 2:
			out->v0 = to->v0_min;
			out->v1 = in->v0 - from->v0_min + to->v1_min;
			break;
		}
		break;
	case 2:
		switch (evinfo[to->cmd].nparams) {
		case 0:
			break;
		case 1:
			out->v0 = in->v1 - from->v1_min + to->v0_min;
			break;
		case 2:
			out->v0 = in->v0 - from->v0_min + to->v0_min;
			out->v1 = in->v1 - from->v1_min + to->v1_min;
			break;
		}
		break;
	}
}

/*
 * find the EVSPEC_XX constant corresponding to
 * the given string
 */
unsigned
evspec_str2cmd(struct evspec *ev, char *str)
{
	unsigned i;

	for (i = 0; i < EV_NUMCMD; i++) {
		if (evinfo[i].spec != NULL && str_eq(evinfo[i].spec, str)) {
			ev->cmd = i;
			return 1;
		}
	}
	return 0;
}

/*
 * reset the evspec structure with "select any event"
 */
void
evspec_reset(struct evspec *o)
{
	o->cmd = EVSPEC_ANY;
	o->dev_min = 0;
	o->dev_max = EV_MAXDEV;
	o->ch_min  = 0;
	o->ch_max  = EV_MAXCH;
	o->v0_min  = evinfo[o->cmd].v0_min;
	o->v0_max  = evinfo[o->cmd].v0_max;
	o->v1_min  = evinfo[o->cmd].v1_min;
	o->v1_max  = evinfo[o->cmd].v1_max;
}

size_t
evspec_fmt(char *buf, size_t bufsz, struct evspec *o)
{
	char *p = buf, *end = buf + bufsz;
	unsigned i;

	i = 0;
	for (;;) {
		if (i == EV_NUMCMD) {
			p += snprintf(p, p < end ? end - p : 0,
			    "unk(%d)", o->cmd);
			break;
		}
		if (o->cmd == i) {
			if (evinfo[i].spec == NULL) {
				p += snprintf(p, p < end ? end - p : 0,
				    "bad(%d)", o->cmd);
			} else {
				p += snprintf(p, p < end ? end - p : 0,
				    "%s", evinfo[i].spec);
			}
			break;
		}
		i++;
	}
	if (evinfo[o->cmd].flags & EV_HAS_DEV) {
		p += snprintf(p, p < end ? end - p : 0,
		    " %u:%u", o->dev_min, o->dev_max);
	}
	if (evinfo[o->cmd].flags & EV_HAS_CH) {
		p += snprintf(p, p < end ? end - p : 0,
		    " %u:%u", o->ch_min, o->ch_max);
	}
	if (evinfo[o->cmd].nparams >= 1) {
		p += snprintf(p, p < end ? end - p : 0,
		    " %u:%u", o->v0_min, o->v0_max);
	}
	if (evinfo[o->cmd].nparams >= 2) {
		p += snprintf(p, p < end ? end - p : 0,
		    " %u:%u", o->v1_min, o->v1_max);
	}
	return p - buf;
}

/*
 * check if the given state belongs to the event spec
 */
unsigned
evspec_matchev(struct evspec *es, struct ev *ev)
{
	if (es->cmd == EVSPEC_EMPTY)
		return 0;
	if (es->cmd == EVSPEC_NOTE && !EV_ISNOTE(ev))
		return 0;
	if (es->cmd != EVSPEC_ANY) {
		if (es->cmd == EVSPEC_NOTE) {
			if (!EV_ISNOTE(ev))
				return 0;
		} else {
			if (es->cmd != ev->cmd)
				return 0;
		}
	}
	if ((evinfo[es->cmd].flags & EV_HAS_DEV) &&
	    (evinfo[ev->cmd].flags & EV_HAS_DEV)) {
		if (ev->dev < es->dev_min ||
		    ev->dev > es->dev_max)
			return 0;
	}
	if ((evinfo[es->cmd].flags & EV_HAS_CH) &&
	    (evinfo[ev->cmd].flags & EV_HAS_CH)) {
		if (ev->ch < es->ch_min ||
		    ev->ch > es->ch_max)
			return 0;
	}
	if (evinfo[es->cmd].nparams > 0 &&
	    evinfo[ev->cmd].nparams > 0) {
		if (ev->v0 < es->v0_min ||
		    ev->v0 > es->v0_max)
			return 0;
	}
	if (evinfo[es->cmd].nparams > 1 &&
	    evinfo[ev->cmd].nparams > 1) {
		if (ev->v1 < es->v1_min ||
		    ev->v1 > es->v1_max)
			return 0;
	}
	return 1;
}

/*
 * check if both sets are the same
 */
unsigned
evspec_eq(struct evspec *es1, struct evspec *es2)
{
	if (es1->cmd != es2->cmd) {
		return 0;
	}
	if (evinfo[es1->cmd].flags & EV_HAS_DEV) {
		if (es1->dev_min != es2->dev_min ||
		    es1->dev_max != es2->dev_max) {
			return 0;
		}
	}
	if (evinfo[es1->cmd].flags & EV_HAS_CH) {
		if (es1->ch_min != es2->ch_min ||
		    es1->ch_max != es2->ch_max) {
			return 0;
		}
	}
	if (evinfo[es1->cmd].nparams > 0) {
		if (es1->v0_min != es2->v0_min ||
		    es1->v0_max != es2->v0_max) {
			return 0;
		}
	}
	if (evinfo[es1->cmd].nparams > 1) {
		if (es1->v1_min != es2->v1_min ||
		    es1->v1_max != es2->v1_max) {
			return 0;
		}
	}
	return 1;
}

/*
 * check if there is intersection between two evspecs
 */
unsigned
evspec_isec(struct evspec *es1, struct evspec *es2)
{
	if (es1->cmd == EVSPEC_EMPTY || es2->cmd == EVSPEC_EMPTY) {
		return 0;
	}
	if (es1->cmd != EVSPEC_ANY &&
	    es2->cmd != EVSPEC_ANY &&
	    es1->cmd != es2->cmd) {
		return 0;
	}
	if ((evinfo[es1->cmd].flags & EV_HAS_DEV) &&
	    (evinfo[es2->cmd].flags & EV_HAS_DEV)) {
		if (es1->dev_min > es2->dev_max ||
		    es1->dev_max < es2->dev_min) {
			return 0;
		}
	}
	if ((evinfo[es1->cmd].flags & EV_HAS_CH) &&
	    (evinfo[es2->cmd].flags & EV_HAS_CH)) {
		if (es1->ch_min > es2->ch_max ||
		    es1->ch_max < es2->ch_min) {
			return 0;
		}
	}
	if (evinfo[es1->cmd].nparams > 0 &&
	    evinfo[es2->cmd].nparams > 0) {
		if (es1->v0_min > es2->v0_max ||
		    es1->v0_max < es2->v0_min) {
			return 0;
		}
	}
	if (evinfo[es1->cmd].nparams > 1 &&
	    evinfo[es2->cmd].nparams > 1) {
		if (es1->v1_min > es2->v1_max ||
		    es1->v1_max < es2->v1_min) {
			return 0;
		}
	}
	return 1;
}

/*
 * check if the first evspec is included in the second one
 * note: any evspec includes itself
 */
unsigned
evspec_in(struct evspec *es1, struct evspec *es2)
{
	if (es1->cmd == EVSPEC_EMPTY) {
		return 1;
	}
	if (es2->cmd == EVSPEC_EMPTY) {
		return 0;
	}
	if (es1->cmd == EVSPEC_ANY && es2->cmd != EVSPEC_ANY) {
		return 0;
	}
	if (es2->cmd != EVSPEC_ANY && es2->cmd != es1->cmd) {
		return 0;
	}
	if ((evinfo[es1->cmd].flags & EV_HAS_DEV) &&
	    (evinfo[es2->cmd].flags & EV_HAS_DEV)) {
		if (es1->dev_min < es2->dev_min ||
		    es1->dev_max > es2->dev_max) {
			return 0;
		}
	}
	if ((evinfo[es1->cmd].flags & EV_HAS_CH) &&
	    (evinfo[es2->cmd].flags & EV_HAS_CH)) {
		if (es1->ch_min < es2->ch_min ||
		    es1->ch_max > es2->ch_max) {
			return 0;
		}
	}
	if (evinfo[es1->cmd].nparams > 0 &&
	    evinfo[es2->cmd].nparams > 0) {
		if (es1->v0_min < es2->v0_min ||
		    es1->v0_max > es2->v0_max) {
			return 0;
		}
	}
	if (evinfo[es1->cmd].nparams > 1 &&
	    evinfo[es2->cmd].nparams > 1) {
		if (es1->v1_min < es2->v1_min ||
		    es1->v1_max > es2->v1_max) {
			return 0;
		}
	}
	return 1;
}

/*
 * check it the given map can work with ev_map()
 */
int
evspec_isamap(struct evspec *from, struct evspec *to)
{
	if ((from->cmd == EVSPEC_NOTE && to->cmd != EVSPEC_NOTE) ||
	    (from->cmd != EVSPEC_NOTE && to->cmd == EVSPEC_NOTE)) {
		logx(1, "note may only be used in both map args");
		return 0;
	}
	if ((from->cmd == EVSPEC_ANY && to->cmd != EVSPEC_ANY) ||
	    (from->cmd != EVSPEC_ANY && to->cmd == EVSPEC_ANY)) {
		logx(1, "any may only be used in both map args");
		return 0;
	}
	if (evinfo[from->cmd].flags & EV_HAS_DEV &&
	    from->dev_max - from->dev_min != to->dev_max - to->dev_min) {
		logx(1, "dev ranges must have the same size");
		return 0;
	}
	if (evinfo[from->cmd].flags & EV_HAS_CH &&
	    from->ch_max - from->ch_min != to->ch_max - to->ch_min) {
		logx(1, "chan ranges must have the same size");
		return 0;
	}
	switch (evinfo[from->cmd].nparams) {
	case 0:
		switch (evinfo[to->cmd].nparams) {
		case 0:
			break;
		case 1:
			if (to->v0_max != to->v0_min) {
				logx(1, "v0 ranges not empty");
				return 0;
			}
			break;
		case 2:
			if (to->v0_max != to->v0_min ||
			    to->v1_max != to->v1_min) {
				logx(1, "v0/v1 ranges not empty");
				return 0;
			}
			break;
		}
		break;
	case 1:
		switch (evinfo[to->cmd].nparams) {
		case 0:
			if (from->v0_max != from->v0_min) {
				logx(1, "v0 ranges not empty");
				return 0;
			}
			break;
		case 1:
			if (from->v0_max - from->v0_min !=
				to->v0_max - to->v0_min) {
				logx(1, "v0 ranges not of the same sizes");
				return 0;
			}
			break;
		case 2:
			if (to->v0_max != to->v0_min) {
				logx(1, "v0 range not empty");
				return 0;
			}
			if (from->v0_max - from->v0_min !=
				to->v1_max - to->v1_min) {
				logx(1, "v0/v1 ranges not of the same sizes");
				return 0;
			}
			break;
		}
		break;
	case 2:
		switch (evinfo[to->cmd].nparams) {
		case 0:
			if (from->v0_max != from->v0_min ||
			    from->v1_max != from->v1_min) {
				logx(1, "v0/v1 ranges not empty");
				return 0;
			}
			break;
		case 1:
			if (from->v0_max != from->v0_min) {
				logx(1, "v0 range not empty");
				return 0;
			}
			if (from->v1_max - from->v1_min !=
				to->v0_max - to->v0_min) {
				logx(1, "v1/v0 ranges not of the same sizes");
				return 0;
			}
			break;
		case 2:
			if (from->v0_max - from->v0_min !=
				to->v0_max - to->v0_min ||
			    from->v1_max - from->v1_min !=
				to->v1_max - to->v1_min) {
				logx(1, "x0/v1 ranges not of the same sizes");
				return 0;
			}
			break;
		}
		break;
	}
	return 1;
}

/*
 * transform "in" spec (included in "from" spec) into "out" spec
 * (included in "to" spec). This routine works in exactly the same way
 * as ev_map() but for specs instead of events; so it has the same
 * semantics and constraints.
 */
void
evspec_map(struct evspec *in,
    struct evspec *from, struct evspec *to, struct evspec *out)
{
	int offs;

	/*
	 * any -> any is special
	 */
	if (from->cmd == EVSPEC_ANY) {
		out->cmd = in->cmd;
		offs = to->dev_min - from->dev_min;
		out->dev_min = in->dev_min + offs;
		out->dev_max = in->dev_max + offs;
		offs = to->ch_min - from->ch_min;
		out->ch_min = in->ch_min + offs;
		out->ch_max = in->ch_max + offs;
		out->v0_min = in->v0_min;
		out->v0_max = in->v0_max;
		out->v1_min = in->v1_min;
		out->v1_max = in->v1_max;
		return;
	}

	/*
	 * note are always mapped to notes
	 */
	if (from->cmd == EVSPEC_NOTE)
		out->cmd = in->cmd;
	else
		out->cmd = to->cmd;

	if (evinfo[out->cmd].flags & EV_HAS_DEV) {
		out->dev_min = to->dev_min;
		out->dev_max = to->dev_max;
		if (evinfo[from->cmd].flags & EV_HAS_DEV) {
			out->dev_min += in->dev_min - from->dev_min;
			out->dev_max += in->dev_max - from->dev_min;
		}
	}
	if (evinfo[out->cmd].flags & EV_HAS_CH) {
		out->ch_min = to->ch_min;
		out->ch_max = to->ch_max;
		if (evinfo[from->cmd].flags & EV_HAS_CH) {
			out->ch_min += in->ch_min - from->ch_min;
			out->ch_max += in->ch_max - from->ch_min;
		}
	}
	switch (evinfo[from->cmd].nparams) {
	case 0:
		switch (evinfo[to->cmd].nparams) {
		case 0:
			break;
		case 1:
			out->v0_min = to->v0_min;
			out->v0_max = to->v0_max;
			break;
		case 2:
			out->v0_min = to->v0_min;
			out->v0_max = to->v0_max;
			out->v1_min = to->v1_min;
			out->v1_max = to->v1_max;
			break;
		}
		break;
	case 1:
		switch (evinfo[to->cmd].nparams) {
		case 0:
			break;
		case 1:
			offs = to->v0_min - from->v0_min;
			out->v0_min = in->v0_min + offs;
			out->v0_max = in->v0_max + offs;
			break;
		case 2:
			out->v0_min = to->v0_min;
			out->v0_max = to->v0_max;
			offs = to->v1_min - from->v0_min;
			out->v1_min = in->v0_min + offs;
			out->v1_max = in->v0_max + offs;
			break;
		}
		break;
	case 2:
		switch (evinfo[to->cmd].nparams) {
		case 0:
			break;
		case 1:
			offs = to->v0_min - from->v1_min;
			out->v0_min = in->v1_min + offs;
			out->v0_max = in->v1_max + offs;
			break;
		case 2:
			offs = to->v0_min - from->v0_min;
			out->v0_min = in->v0_min + offs;
			out->v0_max = in->v0_max + offs;
			offs = to->v1_min - from->v1_min;
			out->v1_min = in->v1_min - offs;
			out->v1_max = in->v1_max - offs;
			break;
		}
		break;
	}
}

/*
 * configure a controller (set the name and default value)
 */
void
evctl_conf(unsigned num, char *name, unsigned defval)
{
	struct evctl *ctl = &evctl_tab[num];

	if (name) {
		ctl->name = str_new(name);
	}
	ctl->defval = defval;
}

/*
 * unconfigure a controller (clear its name unset set its default
 * value tu "unknown")
 */
void
evctl_unconf(unsigned i)
{
	struct evctl *ctl = &evctl_tab[i];

	if (ctl->name != NULL) {
		str_delete(ctl->name);
		ctl->name = NULL;
	}
	ctl->defval = EV_UNDEF;
}

/*
 * find the controller number corresponding the given controller
 * name. Return 1 if found, 0 if not
 */
unsigned
evctl_lookup(char *name, unsigned *ret)
{
	unsigned i;
	struct evctl *ctl;

	for (i = 0; i < EV_MAXCOARSE + 1; i++) {
		ctl = &evctl_tab[i];
		if (ctl->name != NULL && str_eq(ctl->name, name)) {
			*ret = i;
			return 1;
		}
	}
	return 0;
}

/*
 * initialize the controller table
 */
void
evctl_init(void)
{
	unsigned i;

	for (i = 0; i < EV_MAXCOARSE + 1; i++) {
		evctl_tab[i].name = NULL;
		evctl_tab[i].defval = EV_UNDEF;
	}

	/*
	 * some defaults, for testing ...
	 */
	evctl_conf(1,   "mod", 0);
	evctl_conf(7,   "vol", EV_UNDEF);
	evctl_conf(11,  "expr", EV_MAXCOARSE << 7);
	evctl_conf(64,  "sustain", 0);
}

/*
 * free the controller table
 */
void
evctl_done(void)
{
	unsigned i;

	for (i = 0; i < EV_MAXCOARSE + 1; i++) {
		if (evctl_tab[i].name != NULL) {
			str_delete(evctl_tab[i].name);
		}
	}
}

/*
 * find the sysex pattern corresponding to the given name
 */
unsigned
evpat_lookup(char *name, unsigned *ret)
{
	int cmd;

	for (cmd = EV_PAT0; cmd < EV_PAT0 + EV_NPAT; cmd++) {
		if (evinfo[cmd].ev != NULL &&
		    str_eq(evinfo[cmd].ev, name)) {
			*ret = cmd;
			return 1;
		}
	}
	return 0;
}

/*
 * free the given sysex pattern
 */
void
evpat_unconf(unsigned cmd)
{
	str_delete(evinfo[cmd].ev);
	xfree(evinfo[cmd].pattern);
	evinfo[cmd].ev = NULL;
	evinfo[cmd].spec = NULL;
	evinfo[cmd].pattern = NULL;
}

void
evpat_reset(void)
{
	unsigned cmd;

	for (cmd = EV_PAT0; cmd < EV_PAT0 + EV_NPAT; cmd++) {
		if (evinfo[cmd].ev != NULL)
			evpat_unconf(cmd);
	}
}

unsigned
evpat_set(unsigned cmd, char *name, unsigned char *pattern, unsigned size)
{
	unsigned i;
	int has_v0_hi, has_v0_lo, has_v1_hi, has_v1_lo;

	/*
	 * check pattern
	 */
	if (size < 2 || pattern[0] != 0xf0 || pattern[size - 1] != 0xf7) {
		logx(1, "sysex pattern must be in the 0xf0 ... 0xf7 format");
		return 0;
	}
	has_v0_hi = has_v0_lo = has_v1_hi = has_v1_lo = 0;
	for (i = 1; i < size - 1; i++) {
		switch (pattern[i]) {
		case EV_PATV0_HI:
			has_v0_hi++;
			break;
		case EV_PATV0_LO:
			has_v0_lo++;
			break;
		case EV_PATV1_HI:
			has_v1_hi++;
			break;
		case EV_PATV1_LO:
			has_v1_lo++;
			break;
		default:
			if (pattern[i] > 0x7f) {
				logx(1, "sysex pattern data out of range");
				return 0;
			}
		}
	}
	if (has_v0_hi > 1 || has_v0_lo > 1 || has_v1_hi > 1 || has_v1_lo > 1) {
		logx(1, "duplicate atom in sysex pattern");
		return 0;
	}
	if (has_v0_lo && !has_v0_hi) {
		logx(1, "v0_lo but no v0_hi in sysex pattern");
		return 0;
	}
	if (has_v1_lo && !has_v1_hi) {
		logx(1, "v1_lo but no v1_hi in sysex pattern");
		return 0;
	}
	evinfo[cmd].pattern = pattern;
	evinfo[cmd].ev = evinfo[cmd].spec = name;
	evinfo[cmd].flags = EV_HAS_DEV;
	evinfo[cmd].nparams = has_v0_hi + has_v1_hi;
	evinfo[cmd].nranges = evinfo[cmd].nparams;
	evinfo[cmd].v0_min = 0;
	evinfo[cmd].v0_max = EV_MAXFINE;
	evinfo[cmd].v1_min = 0;
	evinfo[cmd].v1_max = EV_MAXFINE;

	return 1;
}
