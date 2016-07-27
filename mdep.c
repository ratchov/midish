/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * machine and OS dependent code
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#include "defs.h"
#include "mux.h"
#include "mididev.h"
#include "cons.h"
#include "user.h"
#include "exec.h"
#include "tty.h"
#include "utils.h"

#define TIMER_USEC	1000

#ifndef RC_NAME
#define RC_NAME		"midishrc"
#endif

#ifndef RC_DIR
#define RC_DIR		"/etc"
#endif

#define MIDI_BUFSIZE	1024
#define MAXFDS		(DEFAULT_MAXNDEVS + 1)

volatile sig_atomic_t cons_quit = 0, resize_flag = 0, cont_flag = 0;
struct timespec ts, ts_last;

unsigned cons_eof;

#ifdef __APPLE__
#define CLOCK_MONOTONIC 0

int
clock_gettime(int which, struct timespec *ts)
{
	static mach_timebase_info_data_t info;
	unsigned long long ns;

	if (info.denom == 0)
		mach_timebase_info(&info);
	ns = mach_absolute_time() * info.numer / info.denom;
	ts->tv_sec = ns / 1000000000L;
	ts->tv_nsec = ns % 1000000000L;
	return 1;
}
#endif

/*
 * handler for SIGALRM, invoked periodically
 */
void
mdep_sigalrm(int i)
{
	/* nothing to do, we only want poll() to return EINTR */
}

void
mdep_sigwinch(int s)
{
	resize_flag = 1;
}

void
mdep_sigcont(int s)
{
	cont_flag = 1;
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
		log_perror("mux_mdep_open: sigprocmask");
		exit(1);
	}
	if (clock_gettime(CLOCK_MONOTONIC, &ts_last) < 0) {
		log_perror("mux_mdep_open: clock_gettime");
		exit(1);
	}
        sa.sa_flags = SA_RESTART;
        sa.sa_handler = mdep_sigalrm;
        sigfillset(&sa.sa_mask);
        if (sigaction(SIGALRM, &sa, NULL) < 0) {
		log_perror("mux_mdep_open: sigaction");
		exit(1);
	}
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = TIMER_USEC;
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = TIMER_USEC;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) {
		log_perror("mux_mdep_open: setitimer");
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
		log_perror("mux_mdep_close: setitimer");
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
	struct pollfd *pfd, *tty_pfds, pfds[MAXFDS];
	struct mididev *dev;
	static unsigned char midibuf[MIDI_BUFSIZE];
	long long delta_nsec;

	nfds = 0;
	if (!cons_eof) {
		tty_pfds = &pfds[nfds];
		nfds += tty_pollfd(tty_pfds);
	} else
		tty_pfds = NULL;
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
		tty_int();
		return 0;
	}
	if (resize_flag) {
		resize_flag = 0;
		tty_winch();
	}
	if (cont_flag) {
		cont_flag = 0;
		tty_reset();
	}
	res = poll(pfds, nfds, -1);
	if (res < 0 && errno != EINTR) {
		log_perror("mux_mdep_wait: poll");
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
			log_perror("mux_mdep_wait: clock_gettime");
			panic();
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
				log_puts("ignored huge clock delta\n");
			}
		}
	}
	log_flush();
	if (tty_pfds) {
		revents = tty_revents(tty_pfds);
		if (revents & POLLHUP)
			cons_eof = 1;
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
			log_perror("mux_sleep: poll");
			exit(1);
		}
		ts = remain;
	}
	if (clock_gettime(CLOCK_MONOTONIC, &ts_last) < 0) {
		log_perror("mux_sleep: clock_gettime");
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
cons_init(void (*cb)(void *, int), void *arg)
{
	struct sigaction sa;

	cons_eof = 0;
	cons_quit = 0;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = cons_mdep_sigint;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		log_perror("cons_mdep_init: sigaction(int)");
		exit(1);
	}
	sa.sa_handler = mdep_sigwinch;
	if (sigaction(SIGWINCH, &sa, NULL) < 0) {
		log_perror("cons_mdep_init: sigaction(winch) failed");
		exit(1);
	}	
	sa.sa_handler = mdep_sigcont;
	if (sigaction(SIGCONT, &sa, NULL) < 0) {
		log_perror("cons_mdep_init: sigaction(cont) failed");
		exit(1);
	}
	tty_init(cb, arg, user_flag_batch);
	tty_setprompt("> ");
}

void
cons_done(void)
{
	struct sigaction sa;

	tty_done();
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		log_perror("cons_mdep_done: sigaction(int)");
		exit(1);
	}
	if (sigaction(SIGWINCH, &sa, NULL) < 0) {
		log_perror("cons_mdep_done: sigaction(winch)");
		exit(1);
	}
	if (sigaction(SIGCONT, &sa, NULL) < 0) {
		log_perror("cons_mdep_done: sigaction(cont)");
		exit(1);
	}
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
