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

#ifndef MIDISH_EV_H
#define MIDISH_EV_H

#include "default.h"

#define EV_NULL		0
#define EV_TIC		0x1
#define EV_START	0x2
#define EV_STOP		0x3
#define EV_SYSEX	0x4

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
#define EV_ISRT(ev)	(((ev)->cmd < 0x8) && ((ev)->cmd >= 0x1))
#define EV_ISMETA(ev)	(((ev)->cmd < EV_NUMCMD) && ((ev)->cmd >= 0x10))

#define EV_ISNOTE(ev)	(((ev)->cmd == EV_NON) || \
			 ((ev)->cmd == EV_NOFF) || \
			 ((ev)->cmd == EV_KAT))

#define EV_GETNOTE(ev)	((ev)->data.voice.b0)
#define EV_GETCH(ev)	((ev)->data.voice.ch)
#define EV_GETDEV(ev)	((ev)->data.voice.dev)
#define EV_GETBEND(ev)	(0x80 * (ev)->data.voice.b1 + (ev)->data.voice.b0)
#define EV_SETBEND(ev,v)				\
	do { 						\
		(ev)->data.voice.b0 = (v) & 0x7f;	\
		(ev)->data.voice.b1 = (v) >> 7;		\
	} while(0);

#define EV_NOFF_DEFAULTVEL	100
#define EV_BEND_DEFAULTLO	0
#define EV_BEND_DEFAULTHI	0x40
#define EV_CAT_DEFAULT		0

struct ev_s {
	unsigned cmd;
	union {
		struct {
			unsigned long usec24;
		} tempo;
		struct {
			unsigned short beats, tics;
		} sign;
		struct voice_s {
#define EV_MAXDEV	(DEFAULT_MAXNDEVS - 1)
#define EV_MAXCH	15
#define EV_MAXB0	0x7f
#define EV_MAXB1	0x7f
#define EV_MAXBEND	0x3fff
			unsigned char dev, ch, b0, b1;
		} voice;
	} data;
};

#define EV_PHASE_FIRST		1
#define EV_PHASE_NEXT		2
#define EV_PHASE_LAST		4


#define EVSPEC_ANY		0
#define EVSPEC_NOTE		1
#define EVSPEC_CTL		2
#define EVSPEC_PC		3
#define EVSPEC_CAT		4
#define EVSPEC_BEND		5

struct evspec_s {
	unsigned cmd;
	unsigned dev_min, dev_max;
	unsigned ch_min, ch_max;
	unsigned b0_min, b0_max;
	unsigned b1_min, b1_max;
};

void	 ev_dbg(struct ev_s *ev);
unsigned ev_sameclass(struct ev_s *ev1, struct ev_s *ev2);
unsigned ev_ordered(struct ev_s *ev1, struct ev_s *ev2);
unsigned ev_str2cmd(struct ev_s *ev, char *str);
unsigned ev_phase(struct ev_s *ev);

unsigned evspec_str2cmd(struct evspec_s *ev, char *str);
void	 evspec_dbg(struct evspec_s *o);
void	 evspec_reset(struct evspec_s *o);
unsigned evspec_matchev(struct evspec_s *o, struct ev_s *e);

struct evctl_s {
#define EVCTL_TYPE_UNKNOWN	0
#define EVCTL_TYPE_CONT		1
#define EVCTL_TYPE_SWITCH	2
	unsigned type;
	char *name;
	unsigned defval;
};

#define EVCTL_TYPE(i)		(evctl_tab[(i)].type)
#define EVCTL_DEFAULT(i)	(evctl_tab[(i)].defval)

extern	struct evctl_s evctl_tab[128];

void	 evctl_conf(unsigned i, unsigned type, unsigned defval, char *name);
void	 evctl_unconf(unsigned i);
unsigned evctl_lookup(char *name, unsigned *ret);
void	 evctl_init(void);
void	 evctl_done(void);


#endif /* MIDISH_EV_H */
