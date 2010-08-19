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

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include "dbg.h"
#include "cons.h"
#include "mididev.h"
#include "str.h"

struct raw {
	struct mididev mididev;		/* device stuff */
	char *path;			/* eg. "/dev/rmidi3" */
	int fd;				/* file desc. */
};

void	 raw_open(struct mididev *);
unsigned raw_read(struct mididev *, unsigned char *, unsigned);
unsigned raw_write(struct mididev *, unsigned char *, unsigned);
unsigned raw_nfds(struct mididev *);
unsigned raw_pollfd(struct mididev *, struct pollfd *, int);
int	 raw_revents(struct mididev *, struct pollfd *);
void	 raw_close(struct mididev *);
void	 raw_del(struct mididev *);

struct devops raw_ops = {
	raw_open,
	raw_read,
	raw_write,
	raw_nfds,
	raw_pollfd,
	raw_revents,
	raw_close,
	raw_del
};

struct mididev *
raw_new(char *path, unsigned mode)
{
	struct raw *dev;

	if (path == NULL) {
		cons_err("path must be set for raw devices");
		return NULL;
	}
	dev = mem_alloc(sizeof(struct raw), "raw");
	mididev_init(&dev->mididev, &raw_ops, mode);
	dev->path = str_new(path);
	return (struct mididev *)&dev->mididev;
}

void
raw_del(struct mididev *addr)
{
	struct raw *dev = (struct raw *)addr;

	mididev_done(&dev->mididev);
	str_delete(dev->path);
	mem_free(dev);
}

void
raw_open(struct mididev *addr)
{
	struct raw *dev = (struct raw *)addr;
	int mode;

	if (dev->mididev.mode == MIDIDEV_MODE_IN) {
		mode = O_RDONLY;
	} else if (dev->mididev.mode == MIDIDEV_MODE_OUT) {
		mode = O_WRONLY;
	} else if (dev->mididev.mode == (MIDIDEV_MODE_IN | MIDIDEV_MODE_OUT)) {
		mode = O_RDWR;
	} else {
		dbg_puts("raw_open: not allowed mode\n");
		dbg_panic();
		mode = 0;
	}
	dev->fd = open(dev->path, mode, 0666);
	if (dev->fd < 0) {
		perror(dev->path);
		dev->mididev.eof = 1;
		return;
	}
}

void
raw_close(struct mididev *addr)
{
	struct raw *dev = (struct raw *)addr;

	if (dev->fd < 0)
		return;
	(void)close(dev->fd);
	dev->fd = -1;
}

unsigned
raw_read(struct mididev *addr, unsigned char *buf, unsigned count)
{
	struct raw *dev = (struct raw *)addr;
	ssize_t res;

	res = read(dev->fd, buf, count);
	if (res < 0) {
		perror(dev->path);
		dev->mididev.eof = 1;
		return 0;
	}
	return res;
}

unsigned
raw_write(struct mididev *addr, unsigned char *buf, unsigned count)
{
	struct raw *dev = (struct raw *)addr;
	ssize_t res;

	res = write(dev->fd, buf, count);
	if (res < 0) {
		perror(dev->path);
		dev->mididev.eof = 1;
		return 0;
	}
	return res;
}

unsigned
raw_nfds(struct mididev *addr)
{
	return 1;
}

unsigned
raw_pollfd(struct mididev *addr, struct pollfd *pfd, int events)
{
	struct raw *dev = (struct raw *)addr;

	pfd->fd = dev->fd;
	pfd->events = events;
	return 1;
}

int
raw_revents(struct mididev *addr, struct pollfd *pfd)
{
	return pfd->revents;
}
