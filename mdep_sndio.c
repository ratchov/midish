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
#ifdef USE_SNDIO
#include <sndio.h>
#include <stdio.h>
#include "utils.h"
#include "cons.h"
#include "mididev.h"
#include "str.h"

struct sndio {
	struct mididev mididev;		/* device stuff */
	struct mio_hdl *hdl;		/* sndio handle */
	char *path;			/* eg. "/dev/rmidi3" */
};

void	 sndio_open(struct mididev *);
unsigned sndio_read(struct mididev *, unsigned char *, unsigned);
unsigned sndio_write(struct mididev *, unsigned char *, unsigned);
unsigned sndio_nfds(struct mididev *);
unsigned sndio_pollfd(struct mididev *, struct pollfd *, int);
int	 sndio_revents(struct mididev *, struct pollfd *);
void	 sndio_close(struct mididev *);
void	 sndio_del(struct mididev *);

struct devops sndio_ops = {
	sndio_open,
	sndio_read,
	sndio_write,
	sndio_nfds,
	sndio_pollfd,
	sndio_revents,
	sndio_close,
	sndio_del
};

struct mididev *
sndio_new(char *path, unsigned mode)
{
	struct sndio *dev;

	if (path == NULL) {
		cons_err("path must be set for raw devices");
		return NULL;
	}
	dev = xmalloc(sizeof(struct sndio), "sndio");
	mididev_init(&dev->mididev, &sndio_ops, mode);
	dev->path = str_new(path);
	return (struct mididev *)&dev->mididev;
}

void
sndio_del(struct mididev *addr)
{
	struct sndio *dev = (struct sndio *)addr;

	mididev_done(&dev->mididev);
	str_delete(dev->path);
	xfree(dev);
}

void
sndio_open(struct mididev *addr)
{
	struct sndio *dev = (struct sndio *)addr;
	int mode = 0;

	if (dev->mididev.mode & MIDIDEV_MODE_OUT)
		mode |= MIO_OUT;
	if (dev->mididev.mode & MIDIDEV_MODE_IN)
		mode |= MIO_IN;
	dev->hdl = mio_open(dev->path, mode, 0);
	if (dev->hdl == NULL) {
		log_puts("sndio_open: ");
		log_puts(dev->path);
		log_puts(": failed to open device\n");
		dev->mididev.eof = 1;
		return;
	}
}

void
sndio_close(struct mididev *addr)
{
	struct sndio *dev = (struct sndio *)addr;

	if (dev->hdl == NULL)
		return;
	mio_close(dev->hdl);
	dev->hdl = NULL;
}

unsigned
sndio_read(struct mididev *addr, unsigned char *buf, unsigned count)
{
	struct sndio *dev = (struct sndio *)addr;
	size_t res;

	res = mio_read(dev->hdl, buf, count);
	if (res == 0 && mio_eof(dev->hdl)) {
		log_puts("sndio_read: ");
		log_puts(dev->path);
		log_puts(": read failed\n");
		dev->mididev.eof = 1;
		return 0;
	}
	return res;
}

unsigned
sndio_write(struct mididev *addr, unsigned char *buf, unsigned count)
{
	struct sndio *dev = (struct sndio *)addr;
	size_t res;

	res = mio_write(dev->hdl, buf, count);
	if (res < count) {
		log_puts("sndio_write: ");
		log_puts(dev->path);
		log_puts(": short write\n");
		dev->mididev.eof = 1;
		return 0;
	}
	return res;
}

unsigned
sndio_nfds(struct mididev *addr)
{
	struct sndio *dev = (struct sndio *)addr;

	return mio_nfds(dev->hdl);
}

unsigned
sndio_pollfd(struct mididev *addr, struct pollfd *pfd, int events)
{
	struct sndio *dev = (struct sndio *)addr;

	return mio_pollfd(dev->hdl, pfd, events);
}

int
sndio_revents(struct mididev *addr, struct pollfd *pfd)
{
	struct sndio *dev = (struct sndio *)addr;

	return mio_revents(dev->hdl, pfd);
}
#endif
