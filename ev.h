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

#ifndef SEQ_EV_H
#define SEQ_EV_H

#include "default.h"

struct ev_s {
	unsigned cmd;
	union {
		struct {
			unsigned long usec24;
		} tempo;
		struct {
			unsigned short beats, tics;
		} sign;
		struct {
#define EV_MAXB0	0x7f
#define EV_MAXB1	0x7f
#define EV_MAXCHAN	(16 * DEFAULT_MAXDEVS)
			unsigned char chan, b0, b1;
		} voice;
	} data;
};

struct evspec_s {
	struct ev_s ev1, ev2;
	unsigned len;
};

void ev_dbg(struct ev_s *ev);
unsigned ev_sameclass(struct ev_s *ev1, struct ev_s *ev2);
unsigned ev_str2cmd(struct ev_s *ev, char *str);

#define EV_NULL		0
#define EV_TIC		0x1
#define EV_START	0x2
#define EV_STOP		0x3

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
#define EV_GETCHAN(ev)	((ev)->data.voice.chan)

#define EV_NOFFVEL	100

#endif /* SEQ_EV_H */
