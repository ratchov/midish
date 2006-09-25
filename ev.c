/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
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

char *ev_cmdstr[EV_NUMCMD] = { 
	"nil",		"tic",		"start",	"stop", 
	"sysex",	NULL,		NULL,		NULL,
	"noff",		"non",		"kat",		"ctl",
	"pc",		"cat",		"bend",		NULL,
	"tempo",	"timesig"
};


char *evspec_cmdstr[] = {
	"any", "note", "ctl", "pc", "cat", "bend", NULL
};

struct evctl evctl_tab[128];

char *
ev_getstr(struct ev *ev) {
	if (ev->cmd >= EV_NUMCMD) {
		return NULL;
		/*
		dbg_puts("ev_getstr: invalid cmd\n");
		dbg_panic();
		*/
	}
	return ev_cmdstr[ev->cmd];
}

unsigned
ev_str2cmd(struct ev *ev, char *str) {
	unsigned i;
	for (i = 0; i < EV_NUMCMD; i++) {
		if (ev_cmdstr[i] && str_eq(ev_cmdstr[i], str)) {
			ev->cmd = i;
			return 1;
		}
	}
	return 0;
}

	/*
	 * return 1 if the pair of events identical
	 * 0 otherwise
	 */

unsigned
ev_eq(struct ev *ev1, struct ev *ev2) {
	if (ev1->cmd != ev2->cmd) {
		return 0;
	}
	if (EV_ISVOICE(ev1)) {
		if (ev1->data.voice.dev != ev2->data.voice.dev ||
		    ev1->data.voice.ch != ev2->data.voice.ch ||
		    ev1->data.voice.b0 != ev2->data.voice.b0 ||
		    ((ev1->cmd != EV_CAT && ev1->cmd != EV_PC) &&
		    ev1->data.voice.b1 != ev2->data.voice.b1)) {
			return 0;
		}
	} else {
		dbg_puts("ev_eq: not defined\n");
		dbg_panic();
	}
	return 1;
}


	/*
	 * return 1 if the pair of events are of the same
	 * type (same note or same controller or both are tempos etc...)
	 */

unsigned
ev_sameclass(struct ev *ev1, struct ev *ev2) {
	switch (ev1->cmd) {
	case EV_NON:
	case EV_NOFF:
	case EV_KAT:
		if (!EV_ISNOTE(ev2) ||
		    ev1->data.voice.dev != ev2->data.voice.dev ||
		    ev1->data.voice.ch != ev2->data.voice.ch ||
		    ev1->data.voice.b0 != ev2->data.voice.b0) {
			return 0;
		}
		break;		
	case EV_CTL:
		if (ev1->cmd != ev2->cmd ||
		    ev1->data.voice.dev != ev2->data.voice.dev ||
		    ev1->data.voice.ch != ev2->data.voice.ch ||
		    ev1->data.voice.b0 != ev2->data.voice.b0) {
			return 0;
		}
		break;
	case EV_BEND:
	case EV_CAT:
	case EV_PC:
		if (ev1->cmd != ev2->cmd ||
		    ev1->data.voice.dev != ev2->data.voice.dev ||
		    ev1->data.voice.ch != ev2->data.voice.ch) {
			return 0;
		}
		break;
	case EV_TEMPO:
	case EV_TIMESIG:
		if (ev1->cmd != ev2->cmd) {
			return 0;
		}
		break;
	default:
		dbg_puts("ev_sameclass: bad event type\n");
		dbg_panic();
		break;
	}
	return 1;
}


	/*
	 * return 1 if the first event has higher "priority"
	 * than the socond one.
	 */

unsigned
ev_ordered(struct ev *ev1, struct ev *ev2) {
	if (!EV_ISVOICE(ev1) || !EV_ISVOICE(ev2)) {
		return 1;
	}
	/* both are voice events */
	if (EV_GETDEV(ev1) != EV_GETDEV(ev2) ||
	    EV_GETCH(ev1) != EV_GETCH(ev2)) {
		return 1;
	}
#define ISBANKHI(ev) ((ev)->cmd == EV_CTL && (ev)->data.voice.b0 == 0)
#define ISBANKLO(ev) ((ev)->cmd == EV_CTL && (ev)->data.voice.b0 == 32)
#define ISPC(ev)     ((ev)->cmd == EV_PC)
	if (ISBANKHI(ev1)) {
		return 1;
	}
	if (ISBANKHI(ev2)) {
		return 0;
	}
	if (ISBANKLO(ev1)) {
		return 1;
	}
	if (ISBANKLO(ev2)) {
		return 0;
	}
	if (ISPC(ev1)) {
		return 1;
	}
	if (ISPC(ev2)) {
		return 0;
	}
#undef ISPC
#undef ISBANKLO
#undef ISBANKHI
	return 1;
}

	/*
	 * return the phase of the event within 
	 * a frame:
	 *	- EV_PHASE_FIRST is set if the given event can be the
	 * 	  first event in a sequence (example: note-on,  
	 *	  bender != 0x4000)
	 *	- EV_PHASE_NEXT is set if the given event can be the next
	 * 	  event in a frame after a 'first event' but not the last
	 * 	  one (example: key after-touch, beder != 0x4000)
	 *	- EV_PHASE_LAST is set if the given event can be the last
	 * 	  event in a frame (example: note-off)
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
		if (ev->data.voice.b0 != EV_CAT_DEFAULT) {
			phase = EV_PHASE_FIRST | EV_PHASE_NEXT;
		} else {
			phase = EV_PHASE_LAST;
		}
		break;
	case EV_CTL:
		if (EVCTL_TYPE(ev->data.voice.b0) == EVCTL_TYPE_UNKNOWN) {
			phase = EV_PHASE_FIRST | EV_PHASE_LAST;
		} else {
			if (ev->data.voice.b1 != 
			    EVCTL_DEFAULT(ev->data.voice.b0)) {
				phase = EV_PHASE_FIRST | EV_PHASE_NEXT;
			} else {
				phase = EV_PHASE_LAST;		
			}
		}
		break;
	case EV_BEND:
		if (ev->data.voice.b0 != EV_BEND_DEFAULTLO ||
		    ev->data.voice.b1 != EV_BEND_DEFAULTHI) {
			phase = EV_PHASE_FIRST | EV_PHASE_NEXT;
		} else {
			phase = EV_PHASE_LAST;		
		}
		break;
	case EV_PC:
	default:
		phase = EV_PHASE_FIRST | EV_PHASE_LAST;
		break;
	}
	return phase;
}

	/*
	 * determine the event that will cancel the givent event,
	 * example a note-off cancels a note-off. If the given
	 * event cannot (on need not to) be cancelled, then 0 is returned.
	 * example: program change cannot be cancelled,
	 * note-off cannot be canceled.
	 */

unsigned
ev_cancel(struct ev *ev, struct ev *ca) {
	if (!EV_ISVOICE(ev)) {
		dbg_puts("ev_cancel: must be called with voice argument\n");
		dbg_panic();
	}
	switch(ev->cmd) {
	case EV_NON:
	case EV_KAT:
		ca->cmd = EV_NOFF;
		ca->data.voice.b0  = ev->data.voice.b0;
		ca->data.voice.b1  = EV_NOFF_DEFAULTVEL;
		break;
	case EV_CAT:
		if (ev->data.voice.b0 == EV_CAT_DEFAULT) {
			return 0;
		}
		ca->cmd = EV_CAT;
		ca->data.voice.b0  = EV_CAT_DEFAULT;
		break;
	case EV_CTL:
		if (EVCTL_TYPE(ev->data.voice.b0) == EVCTL_TYPE_UNKNOWN ||
		    ev->data.voice.b1 == EVCTL_DEFAULT(ev->data.voice.b0)) {
			return 0;
		}
		ca->cmd = EV_CTL;
		ca->data.voice.b0  = ev->data.voice.b0;
		ca->data.voice.b1 = EVCTL_DEFAULT(ev->data.voice.b0);
		break;
	case EV_BEND:
		if (ev->data.voice.b0 == EV_BEND_DEFAULTLO &&
		    ev->data.voice.b1 == EV_BEND_DEFAULTHI) {
		    	return 0;
		}
		ca->cmd = EV_BEND;
		ca->data.voice.b0 = EV_BEND_DEFAULTLO;
		ca->data.voice.b1 = EV_BEND_DEFAULTHI;
		break;
	case EV_NOFF:
	case EV_PC:
		return 0;
	default:
		dbg_puts("ev_cancel: unknown event type\n");
		dbg_panic();
	}
	ca->data.voice.dev = ev->data.voice.dev;
	ca->data.voice.ch  = ev->data.voice.ch;
	return 1;
}



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
		case EV_BEND:
			dbg_puts(" {");
			dbg_putx(ev->data.voice.dev);
			dbg_puts(" ");
			dbg_putx(ev->data.voice.ch);
			dbg_puts("} ");
			dbg_putx(ev->data.voice.b0);
			dbg_puts(" ");
			dbg_putx(ev->data.voice.b1);
			break;
		case EV_CAT:
		case EV_PC:
			dbg_puts(" {");
			dbg_putx(ev->data.voice.dev);
			dbg_puts(" ");
			dbg_putx(ev->data.voice.ch);
			dbg_puts("} ");
			dbg_putx(ev->data.voice.b0);
			break;
		case EV_TEMPO:
			dbg_puts(" ");
			dbg_putu((unsigned)ev->data.tempo.usec24);
			break;
		case EV_TIMESIG:
			dbg_puts(" ");
			dbg_putx(ev->data.sign.beats);
			dbg_puts(" ");
			dbg_putx(ev->data.sign.tics);
			break;
		}
	}
}


/* ------------------------------------------------------------------ */


unsigned
evspec_str2cmd(struct evspec *ev, char *str) {
	unsigned i;

	for (i = 0; evspec_cmdstr[i]; i++) {
		if (str_eq(evspec_cmdstr[i], str)) {
			ev->cmd = i;
			return 1;
		}
	}
	return 0;
}

void
evspec_reset(struct evspec *o) {
	o->cmd = EVSPEC_ANY;
	o->dev_min = 0;
	o->dev_max = EV_MAXDEV;
	o->ch_min  = 0;
	o->ch_max  = EV_MAXCH;
	o->b0_min  = 0;
	o->b0_max  = EV_MAXB0;
	o->b1_min  = 0;
	o->b1_max  = EV_MAXB1;
}

void
evspec_dbg(struct evspec *o) {
	unsigned i;
	
	i = 0;
	for (;;) {
		if (evspec_cmdstr[i] == 0) {
			dbg_puts("unknown");
			break;
		}
		if (o->cmd == i) {
			dbg_puts(evspec_cmdstr[i]);
			break;
		}
		i++;
	}
	dbg_puts(" ");
	dbg_putu(o->dev_min);
	dbg_puts(":");
	dbg_putu(o->dev_max);
		
	dbg_puts(" ");
	dbg_putu(o->ch_min);
	dbg_puts(":");
	dbg_putu(o->ch_max);

	if (o->cmd != EVSPEC_ANY) {
		dbg_puts(" ");
		dbg_putu(o->b0_min);
		dbg_puts(":");
		dbg_putu(o->b0_max);

		if (o->cmd != EVSPEC_CAT && 
		    o->cmd != EVSPEC_PC &&
		    o->cmd != EVSPEC_BEND) {
			dbg_puts(" ");
			dbg_putu(o->b1_min);
			dbg_puts(":");
			dbg_putu(o->b1_max);
		}
	}
}

unsigned
evspec_matchev(struct evspec *o, struct ev *e) {
	switch(o->cmd) {
	case EVSPEC_ANY:
		goto ch;
	case EVSPEC_NOTE:
		if (EV_ISNOTE(e)) {
			goto b0;
		}
		break;
	case EVSPEC_CTL:
		if (e->cmd == EV_CTL) {
			goto b0;
		}
		break;
	case EVSPEC_CAT:
		if (e->cmd == EV_CAT) {
			goto b1;
		}
		break;
	case EVSPEC_PC:
		if (e->cmd == EV_PC) {
			goto b1;
		}
		break;
	case EVSPEC_BEND:
		if (e->cmd == EV_BEND) {
			goto b1;
		}
		break;
	default:
		break;
	}
	return 0;

b1:	if (e->data.voice.b1 < o->b1_min ||
	    e->data.voice.b1 > o->b1_max) {
		return 0;
	}
b0:	if (e->data.voice.b0 < o->b0_min ||
	    e->data.voice.b0 > o->b0_max) {
		return 0;
	}
ch:	if (e->data.voice.dev < o->dev_min ||
	    e->data.voice.dev > o->dev_max ||
	    e->data.voice.ch < o->ch_min ||
	    e->data.voice.ch > o->ch_max) {
		return 0;
	}
	return 1;
}

	/*
	 * configure a controller (set the typ, name, etc...)
	 */

void
evctl_conf(unsigned i, unsigned type, unsigned defval, char *name) {
	struct evctl *o = &evctl_tab[i];
	if (o->name != NULL) {
		str_delete(o->name);
	}
	o->name = str_new(name);
	o->type = type;
	o->defval = defval;
}

	/*
	 * unconfigure a controller (clear its name, set
	 * its type to "unknown")
	 */

void
evctl_unconf(unsigned i) {
	struct evctl *o = &evctl_tab[i];
	if (o->name != NULL) {
		str_delete(o->name);
		o->name = NULL;
	}
	o->type = EVCTL_TYPE_UNKNOWN;
	o->defval = 0;
}

	/*
	 * find the controller number corresponding the given
	 * controller name. Return 1 if found, 0 if not
	 */
	
unsigned
evctl_lookup(char *name, unsigned *ret) {
	unsigned i;
	struct evctl *ctl;
	
	for (i = 0; i < 128; i++) {
		ctl = &evctl_tab[0];
		if (ctl->name != NULL && str_eq(ctl->name, name)) {
			*ret = i;
			return 1;
		}
	}
	return 0;
}

	/*
	 * initialise the controller table
	 */

void
evctl_init(void) {
	unsigned i;
	
	for (i = 0; i < 128; i++) {
		evctl_tab[i].type = EVCTL_TYPE_UNKNOWN;
		evctl_tab[i].name = NULL;
		evctl_tab[i].defval = 0;
	}
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

