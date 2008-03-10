/*
 * Copyright (c) 2003-2007 Alexandre Ratchov <alex@caoua.org>
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

/*
 * ev is an "extended" midi event
 */

#include "dbg.h"
#include "ev.h"
#include "str.h"

struct evinfo evinfo[EV_NUMCMD] = {
	{ "nil", "none",	
	  0, 0,
	  0, 0, 0, 0
	},
	{ NULL,	"any",	
	  EV_HAS_DEV | EV_HAS_CH,
	  0, 0,
	  0, 0, 0, 0
	},
	{ "tempo", NULL,	
	  0,
	  1, 0xdeadbeef,
	  TEMPO_MIN, TEMPO_MAX, 0, 0 
	},
	{ "timesig", NULL,	
	  0,
	  2, 0xdeadbeef,
	  1, 16, 1, 32
	},
	{ "nrpn", "nrpn",  
	  EV_HAS_DEV | EV_HAS_CH, 
	  2, 2, 
	  0, EV_MAXFINE, 0, EV_MAXFINE 
	},
	{ "rpn", "rpn", 
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 2,
	  0, EV_MAXFINE, 0, EV_MAXFINE 
	},
	{ "xctl", "xctl",
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 1,
	  0, EV_MAXCOARSE, 0, EV_MAXFINE 
	},
	{ "xpc", "xpc",
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 2,
	  0, EV_MAXCOARSE, 0, EV_MAXFINE 
	},
	{ "noff", NULL,
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 0xdeadbeef,
	  0, EV_MAXCOARSE, 0, EV_MAXCOARSE 
	},
	{ "non", "note",
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 1,
	  0, EV_MAXCOARSE, 0, EV_MAXCOARSE 
	},
	{ "kat", NULL,
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 0xdeadbeef,
	  0, EV_MAXCOARSE, 0, EV_MAXCOARSE 
	},
	{ "ctl", "ctl",
	  EV_HAS_DEV | EV_HAS_CH,
	  2, 1,
	  0, EV_MAXCOARSE, 0, EV_MAXCOARSE 
	},
	{ "pc", "pc",
	  EV_HAS_DEV | EV_HAS_CH,
	  1, 1,
	  0, EV_MAXCOARSE, 0, 0
	},
	{ "cat", "cat",
	  EV_HAS_DEV | EV_HAS_CH,
	  1, 0,
	  0, EV_MAXCOARSE, 0, 0
	},
	{ "bend", "bend",
	  EV_HAS_DEV | EV_HAS_CH,
	  1, 0,
	  0, EV_MAXFINE, 0, 0
	}
};

struct evctl evctl_tab[128];

/*
 * return the 'name' of the given event
 */
char *
ev_getstr(struct ev *ev) {
	if (ev->cmd >= EV_NUMCMD) {
		return NULL;
		/*
		dbg_puts("ev_getstr: invalid cmd\n");
		dbg_panic();
		*/
	}
	return evinfo[ev->cmd].ev;
}

/*
 * find the event EV_XXX constant corresponding to the given string
 */
unsigned
ev_str2cmd(struct ev *ev, char *str) {
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
ev_phase(struct ev *ev) {
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
 * dump the event structure on stderr, for debug purposes
 */
void
ev_dbg(struct ev *ev) {
	char *cmdstr;
	cmdstr = ev_getstr(ev);
	if (cmdstr == NULL) {
		dbg_puts("unkw(");
		dbg_putu(ev->cmd);
		dbg_puts(")");
	} else {
		dbg_puts(cmdstr);
		switch(ev->cmd) {
		case EV_NON:
		case EV_NOFF:
		case EV_KAT:
		case EV_CTL:
		case EV_NRPN:
		case EV_RPN:
		case EV_XPC:
		case EV_XCTL:
			dbg_puts(" {");
			dbg_putx(ev->dev);
			dbg_puts(" ");
			dbg_putx(ev->ch);
			dbg_puts("} ");
			dbg_putx(ev->v0);
			dbg_puts(" ");
			dbg_putx(ev->v1);
			break;
		case EV_BEND:
		case EV_CAT:
		case EV_PC:
			dbg_puts(" {");
			dbg_putx(ev->dev);
			dbg_puts(" ");
			dbg_putx(ev->ch);
			dbg_puts("} ");
			dbg_putx(ev->v0);
			break;
		case EV_TEMPO:
			dbg_puts(" ");
			dbg_putu((unsigned)ev->tempo_usec24);
			break;
		case EV_TIMESIG:
			dbg_puts(" ");
			dbg_putx(ev->timesig_beats);
			dbg_puts(" ");
			dbg_putx(ev->timesig_tics);
			break;
		}
	}
}

/*
 * find the EVSPEC_XXX constant corresponding to
 * the given string
 */
unsigned
evspec_str2cmd(struct evspec *ev, char *str) {
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
evspec_reset(struct evspec *o) {
	o->cmd = EVSPEC_ANY;
	o->dev_min = 0;
	o->dev_max = EV_MAXDEV;
	o->ch_min  = 0;
	o->ch_max  = EV_MAXCH;
	o->v0_min  = 0xdeadbeef;
	o->v0_max  = 0xdeadbeef;
	o->v1_min  = 0xdeadbeef;
	o->v1_max  = 0xdeadbeef;
}

/*
 * dump the event structure on stderr (debug purposes)
 */
void
evspec_dbg(struct evspec *o) {
	unsigned i;
	
	i = 0;
	for (;;) {
		if (i == EV_NUMCMD) {
			dbg_puts("unk(");
			dbg_putu(o->cmd);
			dbg_puts(")");
			break;
		}
		if (o->cmd == i) {
			if (evinfo[i].spec == NULL) {
				dbg_puts("bad(");
				dbg_putu(o->cmd);
				dbg_puts(")");
			} else {
				dbg_puts(evinfo[i].spec);
			}
			break;
		}
		i++;
	}
	if (evinfo[o->cmd].flags & EV_HAS_DEV) {
		dbg_puts(" ");
		dbg_putu(o->dev_min);
		dbg_puts(":");
		dbg_putu(o->dev_max);
	}
	if (evinfo[o->cmd].flags & EV_HAS_CH) {
		dbg_puts(" ");
		dbg_putu(o->ch_min);
		dbg_puts(":");
		dbg_putu(o->ch_max);
	}
	if (evinfo[o->cmd].nranges >= 1) {
		dbg_puts(" ");
		dbg_putu(o->v0_min);
		dbg_puts(":");
		dbg_putu(o->v0_max);
	}	
	if (evinfo[o->cmd].nranges >= 2) {
		dbg_puts(" ");
		dbg_putu(o->v1_min);
		dbg_puts(":");
		dbg_putu(o->v1_max);
	}
}

/* 
 * check if the given state belongs to the event spec
 */
unsigned
evspec_matchev(struct evspec *es, struct ev *ev) {
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
	if (evinfo[es->cmd].nranges > 0 &&
	    evinfo[ev->cmd].nranges > 0) {
		if (ev->v0 < es->v0_min ||
		    ev->v0 > es->v0_max)
			return 0;
	}	
	if (evinfo[es->cmd].nranges > 1 &&
	    evinfo[ev->cmd].nranges > 1) {
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
evspec_eq(struct evspec *es1, struct evspec *es2) {
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
	if (evinfo[es1->cmd].nranges > 0) {
		if (es1->v0_min != es2->v0_min ||
		    es1->v0_max != es2->v0_max) {
			return 0;
		}
	}
	if (evinfo[es1->cmd].nranges > 1) {
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
evspec_isec(struct evspec *es1, struct evspec *es2) {
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
	if (evinfo[es1->cmd].nranges > 0 &&
	    evinfo[es2->cmd].nranges > 0) {
		if (es1->v0_min > es2->v0_max ||
		    es1->v0_max < es2->v0_min) {
			return 0;
		}
	}
	if (evinfo[es1->cmd].nranges > 1 &&
	    evinfo[es2->cmd].nranges > 1) {
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
evspec_in(struct evspec *es1, struct evspec *es2) {
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
	if (evinfo[es1->cmd].nranges > 0 &&
	    evinfo[es2->cmd].nranges > 0) {
		if (es1->v0_min < es2->v0_min ||
		    es1->v0_max > es2->v0_max) {
			return 0;
		}
	}
	if (evinfo[es1->cmd].nranges > 1 &&
	    evinfo[es2->cmd].nranges > 1) {
		if (es1->v1_min < es2->v1_min ||
		    es1->v1_max > es2->v1_max) {
			return 0;
		}
	}
	return 1;
}

/*
 * configure a controller (set the name and default value)
 */
void
evctl_conf(unsigned num, char *name, unsigned defval) {
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
evctl_unconf(unsigned i) {
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
evctl_lookup(char *name, unsigned *ret) {
	unsigned i;
	struct evctl *ctl;
	
	for (i = 0; i < 128; i++) {
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
evctl_init(void) {
	unsigned i;
	
	for (i = 0; i < 128; i++) {
		evctl_tab[i].name = NULL;
		evctl_tab[i].defval = EV_UNDEF;
	}

	/*
	 * some defaults, for testing ...
	 */
	evctl_conf(1,   "mod", 0);
	evctl_conf(7,   "vol", EV_UNDEF);
	evctl_conf(64,  "sustain", 0);
}

/*
 * free the controller table 
 */
void
evctl_done(void) {
	unsigned i;

	for (i = 0; i < 128; i++) {
		if (evctl_tab[i].name != NULL) {
			str_delete(evctl_tab[i].name);
		}
	}
}

/*
 * return 1 if the controller is reserved
 */
unsigned
evctl_isreserved(unsigned num) {
	if (num == BANK_HI || num == DATAENT_HI || (num >= 32 && num < 64) ||
	    num == RPN_HI || num == RPN_LO || 
	    num == NRPN_HI || num == NRPN_LO) {
		return 1;
	} else {
		return 0;
	}	
}
