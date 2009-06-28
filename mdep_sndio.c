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
#ifdef USE_SNDIO
#include <sndio.h>
#include <stdio.h>
#include "dbg.h"
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
	dev = (struct sndio *)mem_alloc(sizeof(struct sndio));
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
	mem_free(dev);
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
		dbg_puts("sndio_open: ");
		dbg_puts(dev->path);
		dbg_puts(": failed to open device\n");
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
		dbg_puts("sndio_read: ");
		dbg_puts(dev->path);
		dbg_puts(": read failed\n");
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
		dbg_puts("sndio_write: ");
		dbg_puts(dev->path);
		dbg_puts(": short write\n");
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
