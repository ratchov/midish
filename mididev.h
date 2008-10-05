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

#ifndef MIDISH_MIDIDEV_H
#define MIDISH_MIDIDEV_H

/*
 * timeouts for active sensing
 * (as usual units are 24th of microsecond)
 */
#define MIDIDEV_OSENSTO		(250 * 24 * 1000)
#define MIDIDEV_ISENSTO		(350 * 24 * 1000)

/*
 * modes for devices
 */
#define MIDIDEV_MODE_IN		1	/* can input */
#define MIDIDEV_MODE_OUT	2	/* can output */

/*
 * device output buffer lenght in bytes
 */
#define MIDIDEV_BUFLEN	0x400

struct pollfd;
struct mididev;
struct ev;

struct devops {
	/*
	 * open the device so that read() and write() work
	 * must set ``eof'' flag on error
	 */
	void (*open)(struct mididev *);
	/*
	 * try to read the given number of bytes, and retrun the number
	 * of bytes actually read. Must set the ``eof'' flag on error
	 */
	unsigned (*read)(struct mididev *, unsigned char *, unsigned);
	/*
	 * try to write the given number of bytes, and retrun the number
	 * of bytes actually read. Must set the ``eof'' flag on error
	 */
	unsigned (*write)(struct mididev *, unsigned char *, unsigned);
	/*
	 * return the number of pollfd structures the device requires
	 */
	unsigned (*nfds)(struct mididev *);
	/*
	 * fill the given array of pollfd structures with the given
	 * events and return the number of elemets filled
	 */
	unsigned (*pollfd)(struct mididev *, struct pollfd *, int);
	/*
	 * return the events set in the array of pollfd structures
	 */
	int (*revents)(struct mididev *, struct pollfd *);
	/*
	 * close the device
	 */
	void (*close)(struct mididev *);
	/*
	 * free the mididev structure
	 */
	void (*del)(struct mididev *);
};

struct mididev {
	struct devops *ops;

	/*
	 * device list iteration stuff
	 */
	struct pollfd *pfd;
	struct mididev *next;
	
	/*
	 * device settings
	 */
	unsigned unit;			/* index in the mididev table */
	unsigned ticrate, ticdelta;	/* tick rate (default 96) */
	unsigned sendrt;		/* send timing information */
	unsigned isensto, osensto;	/* active sensing timeouts */
	unsigned mode;			/* read, write */
	unsigned ixctlset, oxctlset;	/* bitmap of 14bit controllers */
	unsigned eof;			/* i/o error pending */

	/*
	 * midi events parser state
	 */
	unsigned	  istatus;		/* input running status */
	unsigned 	  icount;		/* bytes in idata[] */
	unsigned char	  idata[2];		/* current event's data */
	unsigned 	  oused;		/* bytes in obuf */
	unsigned	  ostatus;		/* output running status */
	unsigned char	  obuf[MIDIDEV_BUFLEN];	/* output buffer */
	struct sysex	 *isysex;	
};

void mididev_init(struct mididev *, struct devops *, unsigned);
void mididev_done(struct mididev *);
void mididev_flush(struct mididev *);
void mididev_putstart(struct mididev *);
void mididev_putstop(struct mididev *);
void mididev_puttic(struct mididev *);
void mididev_putack(struct mididev *);
void mididev_putev(struct mididev *, struct ev *);
void mididev_sendraw(struct mididev *, unsigned char *, unsigned);
void mididev_open(struct mididev *);
void mididev_close(struct mididev *);
void mididev_inputcb(struct mididev *, unsigned char *, unsigned);

extern unsigned mididev_debug;

extern struct mididev *mididev_list;
extern struct mididev *mididev_master;
extern struct mididev *mididev_byunit[];

struct mididev *raw_new(char *, unsigned);

void mididev_listinit(void);
void mididev_listdone(void);
unsigned mididev_attach(unsigned, char *, unsigned);
unsigned mididev_detach(unsigned);

#endif /* MIDISH_MIDIDEV_H */
