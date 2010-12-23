/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
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
#include <time.h>

#include "defs.h"
#include "mux.h"
#include "mididev.h"
#include "cons.h"
#include "user.h"
#include "exec.h"
#include "dbg.h"

#define TIMER_USEC	1000

#ifndef RC_NAME
#define RC_NAME		"midishrc"
#endif

#ifndef RC_DIR
#define RC_DIR		"/etc"
#endif

#define MIDI_BUFSIZE	1024
#define CONS_BUFSIZE	1024
#define MAXFDS		(DEFAULT_MAXNDEVS + 1)

struct timespec ts, ts_last;

char cons_buf[CONS_BUFSIZE];
unsigned cons_index, cons_len, cons_eof, cons_quit;
struct pollfd *cons_pfd;

/*
 * handler for SIGALRM, invoked periodically
 */
void
mdep_sigalrm(int i)
{
	/* nothing to do, we only want poll() to return EINTR */
}

/*
 * start the mux, must be called just after devices are opened
 */
void
mux_mdep_open(void)
{
	static struct sigaction sa;
	struct itimerval it;
	sigset_t set;
        
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	if (sigprocmask(SIG_BLOCK, &set, NULL)) {
		perror("mux_mdep_open: sigprocmask");
		exit(1);
	}
	if (clock_gettime(CLOCK_MONOTONIC, &ts_last) < 0) {
		perror("mux_mdep_open: clock_gettime");
		exit(1);
	}
        sa.sa_flags = SA_RESTART;
        sa.sa_handler = mdep_sigalrm;
        sigfillset(&sa.sa_mask);
        if (sigaction(SIGALRM, &sa, NULL) < 0) {
		perror("mux_mdep_open: sigaction");
		exit(1);
	}
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = TIMER_USEC;
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = TIMER_USEC;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) {
		perror("mux_mdep_open: setitimer");
		exit(1);
	}
}

/*
 * stop the mux, must be called just before devices are closed
 */
void
mux_mdep_close(void)
{
	struct itimerval it;

	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 0;
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) {
		perror("mux_mdep_close: setitimer");
		exit(1);
	}
}

/*
 * wait until an input device becomes readable or
 * until the next clock tick. Then process all events.
 * Return 0 if interrupted by a signal
 */
int
mux_mdep_wait(void)
{
	int res, revents;
	nfds_t nfds;
	struct pollfd *pfd, pfds[MAXFDS];
	struct mididev *dev;
	static unsigned char midibuf[MIDI_BUFSIZE];
	long long delta_nsec;

	nfds = 0;
	if (cons_index == cons_len && !cons_eof) {
		cons_pfd = &pfds[nfds];
		cons_pfd->fd = STDIN_FILENO;
		cons_pfd->events = POLLIN | POLLHUP;
		nfds++;
	} else
		cons_pfd = NULL;
	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		if (!(dev->mode & MIDIDEV_MODE_IN) || dev->eof) {
			dev->pfd = NULL;
			continue;
		}
		pfd = &pfds[nfds];
		nfds += dev->ops->pollfd(dev, pfd, POLLIN);
		dev->pfd = pfd;
	}
	if (cons_quit) {
		fprintf(stderr, "\n--interrupt--\n");
		cons_quit = 0;
		return 0;
	}
	res = poll(pfds, nfds, -1);
	if (res < 0 && errno != EINTR) {
		perror("mux_mdep_wait: poll");
		exit(1);
	}
	if (res > 0) {
		for (dev = mididev_list; dev != NULL; dev = dev->next) {
			pfd = dev->pfd;
			if (pfd == NULL)
				continue;
			revents = dev->ops->revents(dev, pfd);
			if (revents & POLLIN) {
				res = dev->ops->read(dev, midibuf, MIDI_BUFSIZE);
				if (dev->eof) {
					mux_errorcb(dev->unit);
					continue;
				}
				if (dev->isensto > 0) {
					dev->isensto = MIDIDEV_ISENSTO;
				}
				mididev_inputcb(dev, midibuf, res);
			}
			if (revents & POLLHUP) {
				dev->eof = 1;
				mux_errorcb(dev->unit);
			}
		}
	}
	if (mux_isopen) {
		if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
			perror("mux_mdep_wait: clock_gettime");
			dbg_panic();
		}

		/*
		 * number of micro-seconds between now and the last
		 * time we called poll(). Warning: because of system
		 * clock changes this value can be negative.
		 */
		delta_nsec = 1000000000LL * (ts.tv_sec - ts_last.tv_sec);
		delta_nsec += ts.tv_nsec - ts_last.tv_nsec;
		if (delta_nsec > 0) {
			ts_last = ts;
			if (delta_nsec < 1000000000LL) {
				/*
				 * update the current position,
				 * (time unit = 24th of microsecond)
				 */
				mux_timercb(24 * delta_nsec / 1000);
			} else {
				/*
				 * delta is too large (eg. the program was
				 * suspended and then resumed), just ignore it
				 */
				dbg_puts("ignored huge clock delta\n");
			}
		}
	}
	dbg_flush();
	if (cons_pfd && (cons_pfd->revents & (POLLIN | POLLHUP))) {
		res = read(STDIN_FILENO, cons_buf, CONS_BUFSIZE);
		if (res < 0) {
			perror("stdin: read");
			cons_eof = 1;
			return 1;
		}
		if (res == 0) {
			cons_eof = 1;
		}
		cons_len = res;
		cons_index = 0;
	}
	return 1;
}

/*
 * sleep for 'millisecs' milliseconds useful when sending system
 * exclusive messages
 *
 * IMPORTANT : must never be called from inside mux_run()
 */
void
mux_sleep(unsigned millisecs)
{
	int res;
	struct timespec ts, remain;

	ts.tv_sec = millisecs / 1000;
	ts.tv_nsec = (millisecs % 1000) * 1000000;

	while (ts.tv_sec > 0 || ts.tv_nsec > 0) {
		res = nanosleep(&ts, &remain);
		if (res >= 0)
			break;
		if (errno != EINTR) {
			perror("mux_sleep: poll");
			exit(1);
		}
		ts = remain;
	}
	if (clock_gettime(CLOCK_MONOTONIC, &ts_last) < 0) {
		perror("mux_sleep: clock_gettime");
		exit(1);
	}
}

void
cons_mdep_sigint(int s)
{
	if (cons_quit)
		_exit(1);
	cons_quit = 1;
}

void
cons_mdep_init(void)
{
	struct sigaction sa;

	cons_index = 0;
	cons_len = 0;
	cons_eof = 0;
	cons_quit = 0;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = cons_mdep_sigint;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		perror("cons_mdep_init: sigaction");
		exit(1);
	}
}

void
cons_mdep_done(void)
{
	struct sigaction sa;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		perror("cons_mdep_done: sigaction");
		exit(1);
	}
}

int
cons_mdep_getc(void)
{
	int quit = 0;

	while (cons_index == cons_len && !cons_eof) {
		if (!mux_mdep_wait()) {
			quit = 1;
			break;
		}
	}
	return (cons_eof || quit) ? EOF : (cons_buf[cons_index++] & 0xff);
}

/*
 * start $HOME/.midishrc script, if it doesn't exist then
 * try /etc/midishrc
 */
unsigned
exec_runrcfile(struct exec *o)
{
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
