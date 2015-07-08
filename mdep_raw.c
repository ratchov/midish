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
#ifdef USE_RAW
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include "utils.h"
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
	dev = xmalloc(sizeof(struct raw), "raw");
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
	xfree(dev);
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
		log_puts("raw_open: not allowed mode\n");
		panic();
		mode = 0;
	}
	dev->fd = open(dev->path, mode, 0666);
	if (dev->fd < 0) {
		log_perror(dev->path);
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
		log_perror(dev->path);
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
		log_perror(dev->path);
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
	pfd->revents = 0;
	return 1;
}

int
raw_revents(struct mididev *addr, struct pollfd *pfd)
{
	return pfd->revents;
}
#endif
