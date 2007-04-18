/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
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

#define EV_NULL		0
#define EV_NOFF		0x8
#define EV_NON		0x9
#define EV_KAT		0xa
#define EV_CTL		0xb
#define EV_PC		0xc
#define EV_CAT		0xd
#define EV_BEND		0xe
#define EV_TEMPO	0x10
#define EV_TIMESIG	0x11

#define EV_NUMCMD	0x12

#define EV_ISVOICE(ev)	(((ev)->cmd < 0xf) && ((ev)->cmd >= 0x8))

#define EV_ISMETA(ev)	(((ev)->cmd < EV_NUMCMD) && ((ev)->cmd >= 0x10))

#define EV_ISNOTE(ev)	((ev)->cmd == EV_NON  || \
			 (ev)->cmd == EV_NOFF || \
			 (ev)->cmd == EV_KAT)

#define EV_GETBEND(ev)	(0x80 * (ev)->data.voice.b1 + (ev)->data.voice.b0)

#define EV_SETBEND(ev,v)				\
	do { 						\
		(ev)->data.voice.b0 = (v) & 0x7f;	\
		(ev)->data.voice.b1 = (v) >> 7;		\
	} while(0);

/*
 * some special values of data.voice.b0 and data.voice.b1
 */
#define EV_NOFF_DEFAULTVEL	100	/* defaul note-off velocity */
#define EV_BEND_DEFAULTLO	0	/* defaul bender val. (low nibble) */
#define EV_BEND_DEFAULTHI	0x40	/* defaul bender val. (high nibble) */
#define EV_CAT_DEFAULT		0	/* default channel aftertouch value */
#define EV_CTL_UNKNOWN		255	/* unknown controller number */
#define EV_CTL_UNDEF		255	/* unknown controller value */

/*
 * an event: structure used to store MIDI events and some
 * midish-sepcific events (tempo changes, etc...). This structure have
 * to be kept as small as possiblea, because its used to store events
 * on tracks, that may contain a lot of events
 */
struct ev {
	unsigned cmd;
	union {
		struct {
			unsigned long usec24;
		} tempo;
		struct {
			unsigned short beats, tics;
		} sign;
		struct voice {
#define EV_MAXDEV	(DEFAULT_MAXNDEVS - 1)
#define EV_MAXCH	15
#define EV_MAXB0	0x7f
#define EV_MAXB1	0x7f
#define EV_MAXBEND	0x3fff
			unsigned short dev, ch, b0, b1;
		} voice;
	} data;
};

/*
 * context of an event; program change events depend on the last bank
 * controller and data entry controllers depend last RPN/NRPN controller.
 * So the context is always a controller, we store it here:
 */
struct evctx {
	unsigned char ctl_hi, ctl_lo;	/* 14bit controller of the context */
	unsigned char val_hi, val_lo;	/* value of the above controller */
};

/*
 * event phase bitmasks
 */
#define EV_PHASE_FIRST		1
#define EV_PHASE_NEXT		2
#define EV_PHASE_LAST		4

/* 
 * event priority: some events have to be played before other, ex:
 * bank changes are played before program changes
 */
#define EV_PRIO_ANY	0
#define EV_PRIO_PC	1
#define EV_PRIO_BANK	2
#define EV_PRIO_RT	3
#define EV_PRIO_MAX	4


/*
 * defines a range of events
 */
struct evspec {
#define EVSPEC_ANY		0
#define EVSPEC_NOTE		1
#define EVSPEC_CTL		2
#define EVSPEC_PC		3
#define EVSPEC_CAT		4
#define EVSPEC_BEND		5
	unsigned cmd;
	unsigned dev_min, dev_max;
	unsigned ch_min, ch_max;
	unsigned b0_min, b0_max;
	unsigned b1_min, b1_max;
};

void	 ev_dbg(struct ev *ev);
unsigned ev_prio(struct ev *ev);
unsigned ev_str2cmd(struct ev *ev, char *str);
unsigned ev_phase(struct ev *ev);

unsigned evspec_str2cmd(struct evspec *ev, char *str);
void	 evspec_dbg(struct evspec *o);
void	 evspec_reset(struct evspec *o);
unsigned evspec_matchev(struct evspec *o, struct ev *e);

/*
 * describes a controller number; this structures defines
 * how varius other routines bahave when a controller
 * event with the same number is found
 */
struct evctl {
#define EVCTL_PARAM	0	/* config. parameter (volume, reverb, ... ) */
#define EVCTL_FRAME	1	/* wheels/switches with default values */
#define EVCTL_BANK	2	/* bank (context for pc events) */
#define EVCTL_NRPN	3	/* nrpn/rpn (context for data entries) */
#define EVCTL_DATAENT	4	/* data entries */
	unsigned type;		/* controller type */
#define EVCTL_7BIT	0	/* 7bit controller */
#define EVCTL_14BIT_HI	1	/* 7bit MSB of a 14bit controller */
#define EVCTL_14BIT_LO	2	/* 7bit LSB of a 14bit controller */
	unsigned bits;		/* controller bitness */
	char *name;		/* controller name or NULL if none */
	unsigned char defval;	/* default value if type == EVCTL_FRAME */
	unsigned char lo;	/* LSB ctrl number if bits != EVCTL_7BIT */
	unsigned char hi;	/* MSB ctrl number if bits != EVCTL_7BIT */
};

extern	struct evctl evctl_tab[128];

#define EV_CTL_ISPARAM(ev) \
	(evctl_tab[(ev)->data.voice.b0].type == EVCTL_PARAM)
#define EV_CTL_ISFRAME(ev) \
	(evctl_tab[(ev)->data.voice.b0].type == EVCTL_FRAME)
#define EV_CTL_ISBANK(ev) \
	(evctl_tab[(ev)->data.voice.b0].type == EVCTL_BANK)
#define EV_CTL_ISNRPN(ev) \
	(evctl_tab[(ev)->data.voice.b0].type == EVCTL_NRPN)
#define EV_CTL_ISDATAENT(ev) \
	(evctl_tab[(ev)->data.voice.b0].type == EVCTL_DATAENT)
#define EV_CTL_IS7BIT(ev) \
	(evctl_tab[(ev)->data.voice.b0].bits == EVCTL_7BIT)
#define EV_CTL_IS14BIT_HI(ev) \
	(evctl_tab[(ev)->data.voice.b0].bits == EVCTL_14BIT_HI)
#define EV_CTL_IS14BIT_LO(ev) \
	(evctl_tab[(ev)->data.voice.b0].bits == EVCTL_14BIT_LO)
#define EV_CTL_DEFVAL(ev) \
	(evctl_tab[(ev)->data.voice.b0].defval)
#define EV_CTL_HI(ev) \
	(evctl_tab[(ev)->data.voice.b0].hi)
#define EV_CTL_LO(ev) \
	(evctl_tab[(ev)->data.voice.b0].lo)

void     evctl_conf(unsigned num_hi, unsigned num_lo,
		    unsigned type, char *name, unsigned defval);
void	 evctl_unconf(unsigned i);
unsigned evctl_lookup(char *name, unsigned *ret);
void	 evctl_init(void);
void	 evctl_done(void);


#endif /* MIDISH_EV_H */
