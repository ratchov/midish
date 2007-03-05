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

#ifndef MIDISH_RMIDI_H
#define MIDISH_RMIDI_H

#define RMIDI_BUFLEN		0x400

#include "mdep.h"
#include "mididev.h"

struct ev;

struct rmidi {
	struct mididev    mididev;		/* generic mididev */
	struct rmidi_mdep mdep;			/* os-specific stuff */
	unsigned	  istatus;		/* input running status */
	unsigned 	  icount;		/* bytes in idata[] */
	unsigned char	  idata[2];		/* current event's data */
	unsigned 	  oused;		/* bytes in obuf */
	unsigned	  ostatus;		/* output running status */
	unsigned char	  obuf[RMIDI_BUFLEN];
	struct sysex	 *isysex;
};

#define RMIDI(o) ((struct rmidi *)(o))

struct rmidi *rmidi_new(unsigned mode);
void rmidi_delete(struct rmidi *o);
void rmidi_init(struct rmidi *, unsigned mode);
void rmidi_done(struct rmidi *);
void rmidi_out(struct rmidi *, unsigned);
void rmidi_flush(struct rmidi *);
void rmidi_putev(struct rmidi *, struct ev *);
void rmidi_sendraw(struct rmidi *, unsigned char *, unsigned);

void rmidi_mdep_init(struct rmidi *);
void rmidi_mdep_done(struct rmidi *);

void rmidi_inputcb(struct rmidi *, unsigned char *, unsigned);

extern unsigned rmidi_debug;

#endif /* MIDISH_RMIDI_H */
