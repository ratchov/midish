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

#ifndef MIDISH_EV_H
#define MIDISH_EV_H

#include "default.h"

#define EV_NULL		0		/* "null" or end-of-track */
#define EV_TEMPO	0x2		/* tempo change */
#define EV_TIMESIG	0x3		/* time signature change */
#define EV_NRPN		0x4		/* NRPN + data entry */
#define EV_RPN		0x5		/* RPN + data entry */
#define EV_XCTL		0x6		/* 14bit controller */
#define EV_XPC		0x7		/* prog change + bank select */
#define EV_NOFF		0x8		/* MIDI note off */
#define EV_NON		0x9		/* MIDI note on */
#define EV_KAT		0xa		/* MIDI key after-toutch */
#define EV_CTL		0xb		/* MIDI controller */
#define EV_PC		0xc		/* MIDI prog. change */
#define EV_CAT		0xd		/* MIDI channel aftertouch */
#define EV_BEND		0xe		/* MIDI pitch bend */
#define EV_NUMCMD	0xf

#define EV_ISVOICE(ev)	(((ev)->cmd <= EV_BEND) && ((ev)->cmd >= EV_NRPN))

#define EV_ISMETA(ev)	(((ev)->cmd <= EV_TIMESIG) && ((ev)->cmd >= EV_TEMPO))

#define EV_ISNOTE(ev)	((ev)->cmd == EV_NON  || \
			 (ev)->cmd == EV_NOFF || \
			 (ev)->cmd == EV_KAT)

/*
 * some special values for events
 */
#define EV_NOFF_DEFAULTVEL	100	/* defaul note-off velocity */
#define EV_BEND_DEFAULT		0x2000	/* defaul bender value */
#define EV_CAT_DEFAULT		0	/* default channel aftertouch value */
#define EV_CTL_UNKNOWN		255	/* unknown controller number */

/*
 * an event: structure used to store MIDI events and some
 * midish-sepcific events (tempo changes, etc...). This structure have
 * to be kept as small as possible, because its used to store events
 * on tracks, that may contain a lot of events
 */
struct ev {
	unsigned char cmd, dev, ch;
#define note_num	v0
#define note_vel	v1
#define note_kat	v1
#define ctl_num		v0
#define ctl_val		v1
#define pc_prog		v0
#define pc_bank		v1
#define cat_val		v0
#define bend_val	v0
#define rpn_num		v0
#define rpn_val		v1
#define tempo_usec24	v0
#define timesig_beats	v0
#define timesig_tics	v1
	unsigned v0, v1;
#define EV_UNDEF	0xffff
#define EV_MAXDEV	(DEFAULT_MAXNDEVS - 1)
#define EV_MAXCH	15
#define EV_MAXCOARSE	0x7f
#define EV_MAXFINE	0x3fff
};

/*
 * event phase bitmasks
 */
#define EV_PHASE_FIRST		1
#define EV_PHASE_NEXT		2
#define EV_PHASE_LAST		4


/*
 * range of events, the cmd argument is the event type. To facilitate
 * matching 'struct ev' agains 'struct evspec', we try (when possible)
 * to use the same constants. Currently this works for all events
 * except EV_NON, EV_KAT, EV_NOFF, which all correspond to EVSPEC_NOTE
 */
struct evspec {
#define EVSPEC_EMPTY		EV_NULL
#define EVSPEC_ANY		1
#define EVSPEC_NOTE		EV_NON
#define EVSPEC_CTL		EV_CTL
#define EVSPEC_PC		EV_PC
#define EVSPEC_CAT		EV_CAT
#define EVSPEC_BEND		EV_BEND
#define EVSPEC_NRPN		EV_NRPN
#define EVSPEC_RPN		EV_RPN
#define EVSPEC_XCTL		EV_XCTL
#define EVSPEC_XPC		EV_XPC
	unsigned cmd;
	unsigned dev_min, dev_max;	/* except for EMPTY */
	unsigned ch_min, ch_max;	/* except for EMPTY */
	unsigned v0_min, v0_max;	/* except for EMPTY, ANY */
	unsigned v1_min, v1_max;	/* except for EMPTY, ANY, CAT, PC */
};


/*
 * we use a static array (indexed by 'cmd') of the following
 * structures to lookup for events properties
 */
struct evinfo {
	char *ev, *spec;
#define EV_HAS_DEV	0x01	/* if ev->dev is used */
#define EV_HAS_CH	0x02	/* if ev->ch is used */
	unsigned flags;		/* bitmap of above */
	unsigned nparams;	/* number of params (ie v0, v1) used */
	unsigned nranges;	/* number of ranges (ie v0, v1) used */
	unsigned v0_min;	/* min ev->v0 */
	unsigned v0_max;	/* max ev->v0 */
	unsigned v1_min;	/* min ev->v1 */
	unsigned v1_max;	/* max ev->v1 */
};

extern struct evinfo evinfo[EV_NUMCMD];

void	 ev_dbg(struct ev *);
unsigned ev_prio(struct ev *);
unsigned ev_str2cmd(struct ev *, char *);
unsigned ev_phase(struct ev *);

unsigned evspec_str2cmd(struct evspec *, char *);
void	 evspec_dbg(struct evspec *);
void	 evspec_reset(struct evspec *);
unsigned evspec_matchev(struct evspec *, struct ev *);
unsigned evspec_eq(struct evspec *, struct evspec *);
unsigned evspec_isec(struct evspec *, struct evspec *);
unsigned evspec_in(struct evspec *, struct evspec *);

/*
 * describes a controller number; this structures defines
 * how varius other routines bahave when a controller
 * event with the same number is found
 */
struct evctl {
	char *name;		/* controller name or NULL if none */
	unsigned defval;	/* default value if type == EVCTL_FRAME */
};

extern	struct evctl evctl_tab[EV_MAXCOARSE + 1];

#define EV_CTL_ISPARAM(ev) \
	(evctl_tab[(ev)->ctl_num].defval == EV_UNDEF)
#define EV_CTL_ISFRAME(ev) \
	(evctl_tab[(ev)->ctl_num].defval != EV_UNDEF)
#define EV_CTL_DEFVAL(ev) \
	(evctl_tab[(ev)->ctl_num].defval)

/*
 * return true if the given controller number is 14bit (fine)
 * and false if it is 7bit (coarse). The 'ctlbits' is
 * a 32bit bitmap, it is stored in per-device basis in
 * mididev structure
 */
#define EVCTL_ISFINE(xctlset, num)	((xctlset) & (1 << (num)))

void     evctl_conf(unsigned, char *, unsigned);
void	 evctl_unconf(unsigned);
unsigned evctl_lookup(char *, unsigned *);
void	 evctl_init(void);
void	 evctl_done(void);
unsigned evctl_isreserved(unsigned);

#endif /* MIDISH_EV_H */
