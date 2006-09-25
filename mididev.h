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

#ifndef MIDISH_MIDIDEV_H
#define MIDISH_MIDIDEV_H

/*
 * timeouts for active sensing
 * (as usual units are 24th of microsecond)
 */
#define MIDIDEV_OSENSTO		(250 * 24 * 1000)
#define MIDIDEV_ISENSTO		(350 * 24 * 1000)
#define MIDIDEV_MODE_IN		1
#define MIDIDEV_MODE_OUT	2

struct mididev {
	struct mididev *next;
	unsigned unit;			/* index in the mididev table */
	unsigned ticrate, ticdelta;	/* tick rate (default 96) */
	unsigned sendrt;		/* send timing information */
	unsigned isensto, osensto;	/* active sensing timeouts */
	unsigned mode;			/* read, write */
};

void mididev_init(struct mididev *o, unsigned mode);
void mididev_done(struct mididev *o);

extern struct mididev *mididev_list;
extern struct mididev *mididev_master;
extern struct mididev *mididev_byunit[];

void mididev_listinit(void);
void mididev_listdone(void);
unsigned mididev_attach(unsigned unit, char *path, unsigned mode);
unsigned mididev_detach(unsigned unit);

#endif /* MIDISH_MIDIDEV_H */
