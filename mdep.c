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

/*
 * machine and OS dependent code
 */
 
#include <sys/param.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <termios.h> 		/* for tcflush() in mux_run */
#include <stdio.h>
#include <stdlib.h>

#include "default.h"
#include "mux.h"
#include "rmidi.h"
#include "mdep.h"

#include "user.h"
#include "tree.h"

#ifndef RC_NAME
#define RC_NAME		".midishrc"
#endif

#define MIDI_BUFSIZE	1024
#define CONS_LINESIZE	1024

unsigned char mdep_midibuf[MIDI_BUFSIZE];
struct timeval mdep_tv;
unsigned mdep_lineused;

void
mux_mdep_init(void) {
	struct mididev_s *i;
	for (i = mididev_list; i != 0; i = i->next) {
		RMIDI(i)->mdep.fd = open(RMIDI(i)->mdep.path, O_RDWR);
		if (RMIDI(i)->mdep.fd < 0) {
			perror(RMIDI(i)->mdep.path);
			RMIDI(i)->mdep.dying = 1;
		} else {
			RMIDI(i)->mdep.dying = 0;
		}		
	}
	if (gettimeofday(&mdep_tv, 0) < 0) {
		perror("mux_mdep_init: gettimeofday failed\n");
		exit(1);
	}
}


void
mux_mdep_done(void) {
	struct mididev_s *i;
	for (i = mididev_list; i != 0; i = i->next) {
		if (RMIDI(i)->mdep.fd >= 0) {
			close(RMIDI(i)->mdep.fd);
		}
	}
}


void
mux_run(void) {
	int ifds, res, consfd;
	struct timeval tv;
	struct pollfd fds[DEFAULT_MAXNDEVS + 1];
	struct mididev_s *dev, *index2dev[DEFAULT_MAXNDEVS];
	static char conspath[] = "/dev/tty";
	static char waitmsg[] = "\r\npress enter to finish\n";
	static char stoppedmsg[] = "\r\n";
	unsigned long delta_usec;
	unsigned i;

	consfd = open(conspath, O_RDWR);
	if (consfd < 0) {
		perror(conspath);
		goto bad1;
	}

	ifds = 0;
	for (dev = mididev_list; dev != 0; dev = dev->next) {
		fds[ifds].fd = RMIDI(dev)->mdep.fd;
		fds[ifds].events = POLLIN;
		index2dev[ifds] = dev;
		ifds++;
	}
	fds[ifds].fd = consfd;
	fds[ifds].events = POLLIN;
	
	if (write(consfd, waitmsg, sizeof(waitmsg)) < 0) {
		perror(conspath);
		goto bad2;
	}
	
	for (;;) {
		res = poll(fds, ifds + 1, 1);
		if (res < 0) {
			perror("mux_run: poll failed");
			goto bad2;
		}
		
		for (i = 0; i < ifds; i++) {
			if (fds[i].revents & POLLIN) {
				dev = index2dev[i];
				res = read(fds[i].fd, mdep_midibuf, MIDI_BUFSIZE);
				if (res < 0) {
					perror(RMIDI(dev)->mdep.path);
					RMIDI(dev)->mdep.dying = 1;
				} else {
					rmidi_inputcb(RMIDI(dev), mdep_midibuf, (unsigned)res);
				}
			}
		}
		
		if (gettimeofday(&tv, 0) < 0) {
			perror("mux_run: gettimeofday failed");
			goto bad2;
		}

		/*
		 * number of micro-seconds between now
		 * and the last timeout of poll()
		 */
		delta_usec = 1000000 * (tv.tv_sec - mdep_tv.tv_sec);
		delta_usec += tv.tv_usec - mdep_tv.tv_usec;
		mdep_tv = tv;

		/*
		 * update the current position, 
		 * (time unit = 24th of microsecond
		 */
		mux_timercb(24 * delta_usec);

		if (fds[ifds].revents & POLLIN) {
			if (tcflush(consfd, TCIOFLUSH) < 0) {
				perror(conspath);
				goto bad2;
			}
			if (write(consfd, stoppedmsg, sizeof(stoppedmsg)) < 0) {
				perror(conspath);
				goto bad2;
			}
			close(consfd);
			return;
		}
	}
	bad2:
	close(consfd);
	bad1:
	return;
}


void
rmidi_mdep_init(struct rmidi_s *o) {
}


void
rmidi_mdep_done(struct rmidi_s *o) {
}


void
rmidi_flush(struct rmidi_s *o) {
	if (!RMIDI(o)->mdep.dying) {
		if (write(o->mdep.fd, o->obuf, o->oused) != o->oused) {
			perror(RMIDI(o)->mdep.path);
			RMIDI(o)->mdep.dying = 1;
		}
	}
	o->oused = 0;
}


char *
user_rcname(void) {
	char *home;
	static char name[PATH_MAX];
	
	home = getenv("HOME");
	if (home == NULL) {
		home = ".";
	}
	
	snprintf(name, PATH_MAX, "%s" "/" RC_NAME, home);
	return name;
}
