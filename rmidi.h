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

#ifndef SEQ_RMIDI_H
#define SEQ_RMIDI_H

#define RMIDI_BUFLEN		0x400
#undef  RMIDI_DEBUG_OUTPUT

#include "mdep.h"
#include "mididev.h"

struct ev_s;

struct	rmidi_s {
	struct mididev_s    mididev;		/* generic mididev */
	struct rmidi_mdep_s mdep;		/* os-specific stuff */
	unsigned	    istatus;		/* input stuff */
	unsigned 	    icount;
	unsigned char	    idata[2];
	unsigned 	    oused, ostatus;	/* output stuff */
	unsigned char	    obuf[RMIDI_BUFLEN];
};

#define RMIDI(o) ((struct rmidi_s *)(o))

struct rmidi_s *rmidi_new(void);
void rmidi_delete(struct rmidi_s *o);
void rmidi_init(struct rmidi_s *);
void rmidi_done(struct rmidi_s *);
void rmidi_out(struct rmidi_s *, unsigned);
void rmidi_flush(struct rmidi_s *);
void rmidi_putev(struct rmidi_s *, struct ev_s *);

void rmidi_mdep_init(struct rmidi_s *);
void rmidi_mdep_done(struct rmidi_s *);

void rmidi_inputcb(struct rmidi_s *, unsigned char *, unsigned);

extern unsigned rmidi_debug;

#endif /* SEQ_RMIDI_H */
