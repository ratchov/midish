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

/*
 * machine and OS dependent code
 */
 
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "default.h"
#include "mux.h"
#include "rmidi.h"
#include "cons.h"
#include "mdep.h"
#include "user.h"
#include "exec.h"
#include "dbg.h"

#ifndef RC_NAME
#define RC_NAME		"midishrc"
#endif

#ifndef RC_DIR
#define RC_DIR		"/etc"
#endif

#define MIDI_BUFSIZE	1024
#define CONS_BUFSIZE	1024
#define MAXFDS		(DEFAULT_MAXNDEVS + 1)

struct timeval tv, tv_last;

char cons_buf[CONS_BUFSIZE];
unsigned cons_index, cons_len, cons_eof;
struct pollfd *cons_pfd;

/*
 * open midi devices
 */
void
mux_mdep_open(void) {
	if (gettimeofday(&tv_last, NULL) < 0) {
		perror("mux_mdep_open: initial gettimeofday() failed");
		exit(1);
	}
}

/*
 * close midi devices
 */
void
mux_mdep_close(void) {
}

void
mux_mdep_wait(void)
{
	int res;
	nfds_t nfds;
	struct pollfd *pfd, pfds[MAXFDS];
	struct mididev *dev;
	static unsigned char midibuf[MIDI_BUFSIZE];
	long delta_usec;

	nfds = 0;
	cons_pfd = &pfds[nfds++];
	cons_pfd->fd = STDIN_FILENO;
	cons_pfd->events = POLLIN;
	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		if (!(dev->mode & MIDIDEV_MODE_IN)) {
			RMIDI(dev)->mdep.pfd = NULL;
			continue;
		}
		pfd = &pfds[nfds++];
		pfd->fd = RMIDI(dev)->mdep.fd;
		pfd->events = POLLIN;
		RMIDI(dev)->mdep.pfd = pfd;
	}
	res = poll(pfds, nfds, mux_isopen ? 1 : -1);
	if (res < 0) {
		perror("mux_run: poll failed");
		exit(1);
	}
	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		pfd = RMIDI(dev)->mdep.pfd;
		if (pfd == NULL)
			continue;
		if (pfd->revents & POLLIN) {
			res = read(pfd->fd, midibuf, MIDI_BUFSIZE);
			if (res < 0) {
				perror(RMIDI(dev)->mdep.path);
				RMIDI(dev)->mdep.idying = 1;
				mux_errorcb(dev->unit);
				continue;
			}
			if (dev->isensto > 0) {
				dev->isensto = MIDIDEV_ISENSTO;
			}
			rmidi_inputcb(RMIDI(dev), midibuf, res);
		}
	}
	
	if (mux_isopen) {
		if (gettimeofday(&tv, NULL) < 0) {
			perror("mux_run: gettimeofday failed");
			return;
		}
		
		/*
		 * number of micro-seconds between now and the last
		 * time we called poll(). Warning: because of system
		 * clock changes this value can be negative.
		 */
		delta_usec = 1000000L * (tv.tv_sec - tv_last.tv_sec);
		delta_usec += tv.tv_usec - tv_last.tv_usec;
		if (delta_usec > 0) {
			tv_last = tv;
			/*
			 * update the current position, 
			 * (time unit = 24th of microsecond
			 */
			mux_timercb(24 * delta_usec);
		}
	}
	
	if ((cons_pfd->revents & POLLIN) && (cons_index == cons_len)) {
		res = read(STDIN_FILENO, cons_buf, CONS_BUFSIZE);
		if (res < 0) {
			perror("stdin");
			cons_eof = 1;
		}
		if (res == 0) {
			cons_eof = 1;
		}
		cons_len = res;
		cons_index = 0;
	}
}

/* 
 * sleep for 'millisecs' milliseconds useful when sending system
 * exclusive messages
 *
 * IMPORTANT : must never be called from inside mux_run()
 */
void 
mux_sleep(unsigned millisecs) {
	while (poll(NULL, (nfds_t)0, millisecs) < 0) {
		perror("mux_sleep: poll failed");
		exit(1);
	}
}

/*
 * open an already initialized midi device
 */
void
rmidi_open(struct rmidi *o) {
	int mode;

	if (o->mididev.mode == MIDIDEV_MODE_IN) {
		mode = O_RDONLY;
	} else if (o->mididev.mode == MIDIDEV_MODE_OUT) {
		mode = O_WRONLY;
	} else if (o->mididev.mode == (MIDIDEV_MODE_IN | MIDIDEV_MODE_OUT)) {
		mode = O_RDWR;
	} else {
		dbg_puts("rmidi_open: not allowed mode\n");
		dbg_panic();
		mode = 0;
	}
	o->mdep.fd = open(o->mdep.path, mode, 0666);
	if (o->mdep.fd < 0) {
		perror(o->mdep.path);
		o->mdep.idying = 1;
		o->mdep.odying = 1;
		return;
	}
	o->mdep.idying = 0;
	o->mdep.odying = 0;
	o->oused = 0;
	o->istatus = o->ostatus = 0;
	o->isysex = NULL;
}
 
/*
 * close the given midi device
 */
void
rmidi_close(struct rmidi *o) {
	if (o->mdep.fd < 0) {
		return;
	}
	close(o->mdep.fd);
}

/*
 * create/register the new device
 */
void 
rmidi_init(struct rmidi *o, unsigned mode) {
	o->mdep.fd = -1;
	o->mdep.pfd = NULL;
	o->oused = 0;
	o->istatus = o->ostatus = 0;
	o->isysex = NULL;
	mididev_init(&o->mididev, mode);
}

/*
 * unregister/destroy the given device
 */
void 
rmidi_done(struct rmidi *o) {
	if (mux_isopen) {
		rmidi_close(o);
	}
	if (o->oused != 0) {
		dbg_puts("rmidi_done: output buffer is not empty, continuing...\n");
	}
	mididev_done(&o->mididev);
}

/*
 * flush the given midi device
 */
void
rmidi_flush(struct rmidi *o) {
	int res;
	unsigned start, stop;
	
	if (!RMIDI(o)->mdep.odying) {
		start = 0;
		stop = o->oused;
		while (start < stop) {
			res = write(o->mdep.fd, o->obuf, o->oused);
			if (res < 0) {
				perror(RMIDI(o)->mdep.path);
				RMIDI(o)->mdep.odying = 1;
				break;
			}
			start += res;
		}
		if (o->oused) {
			o->mididev.osensto = MIDIDEV_OSENSTO;
		}
	}
	o->oused = 0;
}

void
cons_mdep_init(void)
{
	cons_index = 0;
	cons_len = 0;
	cons_eof = 0;
}

void
cons_mdep_done(void)
{
}

int
cons_mdep_getc(void)
{
	while (cons_index == cons_len && !cons_eof)
		mux_mdep_wait();
	return cons_eof ? EOF : cons_buf[cons_index++];
}

/*
 * start $HOME/.midishrc script, if it doesn't exist then
 * try /etc/midishrc
 */
unsigned
exec_runrcfile(struct exec *o) {
	char *home;
	char name[PATH_MAX];
	struct stat st;
	
	home = getenv("HOME");
	if (home != NULL) {
		snprintf(name, PATH_MAX, "%s" "/" "." RC_NAME, home);
		if (stat(name, &st) == 0) {
			return exec_runfile(o, name);
		}
	}
	if (stat(RC_DIR "/" RC_NAME, &st) == 0) {
		return exec_runfile(o, RC_DIR "/" RC_NAME);
	}
	return 1;
}

extern char *optarg;
extern int optind;

unsigned
user_getopts(int *pargc, char ***pargv) {
	int ch;

	while ((ch = getopt(*pargc, *pargv, "bhv")) != -1) {
		switch (ch) {
		case 'b':
			user_flag_batch = 1;
			break;
		case 'v':
			user_flag_verb = 1;
			break;
		default:
			goto err;
		}
	}
	*pargc -= optind;
	*pargv += optind;
	if (*pargc >= 1) {
	err:
		fputs("usage: midish [-bhv]\n", stderr);
		return 0;
	}
	return 1;
}
