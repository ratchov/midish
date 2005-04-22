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

/*
 * ev_s is an "extended" midi event
 */

#include "dbg.h"
#include "ev.h"
#include "str.h"

char *ev_cmdstr[EV_NUMCMD] = { 
	"nil",		"tic",		"start",	"stop", 
	0,		0,		0,		0,
	"noff",		"non",		"kat",		"ctl",
	"pc",		"cat",		"bend",		0,
	"tempo",	"timesig"
};

char *
ev_getstr(struct ev_s *ev) {
	if (ev->cmd >= EV_NUMCMD) {
		return 0;
		/*
		dbg_puts("ev_getstr: invalid cmd\n");
		dbg_panic();
		*/
	}
	return ev_cmdstr[ev->cmd];
}

unsigned
ev_str2cmd(struct ev_s *ev, char *str) {
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
	 * return 1 if the pair of events are of the same
	 * type (same note or same controller or both are tempos etc...)
	 */

unsigned
ev_sameclass(struct ev_s *ev1, struct ev_s *ev2) {
	if (ev1->cmd != ev2->cmd) {
		return 0;
	}
	switch (ev1->cmd) {
	case EV_NON:
	case EV_NOFF:
	case EV_KAT:
	case EV_CTL:
		if (ev1->data.voice.dev != ev2->data.voice.dev ||
		    ev1->data.voice.ch != ev2->data.voice.ch ||
		    ev1->data.voice.b0 != ev2->data.voice.b0) {
			return 0;
		}
		break;
	case EV_BEND:
	case EV_CAT:
	case EV_PC:
		if (ev1->data.voice.dev != ev2->data.voice.dev ||
		    ev1->data.voice.ch != ev2->data.voice.ch) {
			return 0;
		}
		break;
	case EV_TEMPO:
	case EV_TIMESIG:
		break;
	default:
		dbg_puts("ev_sameclass: bad event type\n");
		dbg_panic();
		break;
	}
	return 1;
}

	
void
ev_dbg(struct ev_s *ev) {
	char *cmdstr;
	cmdstr = ev_getstr(ev);
	if (cmdstr == 0) {
		dbg_puts("unkw");
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


void
evspec_dbg(struct evspec_s *o) {
	dbg_puts("[ ");
	ev_dbg(&o->min);
	dbg_puts(" : ");
	ev_dbg(&o->max);
	dbg_puts(" ]");	
}

unsigned
evspec_matchev(struct evspec_s *o, struct ev_s *e) {
	if (o->min.cmd == EV_NULL) {
		return 1;
	} else if ((EV_ISNOTE(&o->min) && EV_ISNOTE(e)) ||
	    (o->min.cmd == EV_CTL && e->cmd == EV_CTL) ||
	    (o->min.cmd == EV_BEND && e->cmd == EV_BEND)) {
		goto two;
	} else if ((o->min.cmd == EV_CAT && e->cmd == EV_CAT) ||
	    (o->min.cmd == EV_PC && e->cmd == EV_PC)) {
		goto one;
	} else {
		return 0;		
	}

two:	if (e->data.voice.b1 < o->min.data.voice.b1 ||
	    e->data.voice.b1 > o->max.data.voice.b1) {
		return 0;
	}
one:	if (e->data.voice.dev < o->min.data.voice.dev ||
	    e->data.voice.dev > o->max.data.voice.dev ||
	    e->data.voice.ch < o->min.data.voice.ch ||
	    e->data.voice.ch > o->max.data.voice.ch ||
	    e->data.voice.b0 < o->min.data.voice.b0 ||
	    e->data.voice.b0 > o->max.data.voice.b0) {
		return 0;
	}
	return 1;
}
