/*
 * Copyright (c) 2008 Willem van Engen <wvengen@stack.nl>
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
 * TODO: use the same handle for multiple ports
 */
#ifdef USE_ALSA
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include "dbg.h"
#include "mididev.h"
#include "str.h"

struct alsa {
	struct mididev mididev;		/* device stuff */
	snd_seq_t *seq_handle;		/* sequencer connection */
	int my_port;			/* port id of myself */
	char *path;			/* eg. "128:0", translates in dst */
	snd_midi_event_t *iparser;	/* midi input event parser */
	snd_midi_event_t *oparser;	/* midi output event parser */
	int nfds;
};

void	 alsa_open(struct mididev *);
unsigned alsa_read(struct mididev *, unsigned char *, unsigned);
unsigned alsa_write(struct mididev *, unsigned char *, unsigned);
unsigned alsa_nfds(struct mididev *);
unsigned alsa_pollfd(struct mididev *, struct pollfd *, int);
int	 alsa_revents(struct mididev *, struct pollfd *);
void	 alsa_close(struct mididev *);
void	 alsa_del(struct mididev *);

struct devops alsa_ops = {
	alsa_open,
	alsa_read,
	alsa_write,
	alsa_nfds,
	alsa_pollfd,
	alsa_revents,
	alsa_close,
	alsa_del
};

struct mididev *
alsa_new(char *path, unsigned mode)
{
	struct alsa *dev;

	dev = (struct alsa *)mem_alloc(sizeof(struct alsa));
	mididev_init(&dev->mididev, &alsa_ops, mode);
	dev->path = (path != NULL) ? str_new(path) : NULL;
	dev->seq_handle = NULL;
	dev->my_port = -1;
	dev->iparser = NULL;
	dev->oparser = NULL;
	return (struct mididev *)&dev->mididev;
}

void
alsa_del(struct mididev *addr)
{
	struct alsa *dev = (struct alsa *)addr;

	mididev_done(&dev->mididev);
	if (dev->path)
		str_delete(dev->path);
	mem_free(dev);
}

void
alsa_open(struct mididev *addr)
{
	struct alsa *dev = (struct alsa *)addr;
	struct snd_seq_addr dst;
	unsigned int mode;
	char name[32];
	
	if (snd_seq_open(&dev->seq_handle, "default",
		SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		dbg_puts("alsa_open: could not open ALSA sequencer\n");
		dev->mididev.eof = 1;
		return;	
	}
	snprintf(name, sizeof(name), "midish/%u", dev->mididev.unit);
	if (snd_seq_set_client_name(dev->seq_handle, "midish") < 0) {
		dbg_puts("alsa_open: could set client name\n");
		dev->mididev.eof = 1;
		return;
	}
	mode = 0;
	if (dev->mididev.mode & MIDIDEV_MODE_IN) {
		mode |= SND_SEQ_PORT_CAP_WRITE;
		if (dev->path == NULL)
			mode |= SND_SEQ_PORT_CAP_SUBS_WRITE;
	}
	if (dev->mididev.mode & MIDIDEV_MODE_OUT) {
		mode |= SND_SEQ_PORT_CAP_READ;
		if (dev->path == NULL)
			mode |= SND_SEQ_PORT_CAP_SUBS_READ;
	}
	if (dev->mididev.mode == (MIDIDEV_MODE_IN | MIDIDEV_MODE_OUT))
		mode |= SND_SEQ_PORT_CAP_DUPLEX;
	dev->my_port = snd_seq_create_simple_port(dev->seq_handle, name, mode,
	    SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
	if (dev->my_port < 0) {
		dbg_puts("alsa_open: could not create port\n");
		dev->mididev.eof = 1;
		return;
	}
	
	/*
	 * now we have the port, create parsers
	 */
	if (dev->mididev.mode & MIDIDEV_MODE_IN) {
		if (snd_midi_event_new(MIDIDEV_BUFLEN, &dev->iparser) < 0) {
			dev->iparser = NULL;
			dbg_puts("alsa_open: could not create in parser\n");
			dev->mididev.eof = 1;		
			return;
		}
	}
	if (dev->mididev.mode & MIDIDEV_MODE_OUT) {
		if (snd_midi_event_new(MIDIDEV_BUFLEN, &dev->oparser) < 0) {
			dev->oparser = NULL;
			dbg_puts("alsa_open: couldn't create out parser\n");
			dev->mididev.eof = 1;
			return;
		}  
	}

	/*
	 * if destination is specified, connect the port
	 */
	if (dev->path != NULL) {
		if (snd_seq_parse_address(dev->seq_handle, &dst,
			dev->path) < 0) {
			dbg_puts("alsa_open: couldn't parse alsa port\n");
			dev->mididev.eof = 1;
			return;
		}
		if ((dev->mididev.mode & MIDIDEV_MODE_IN) &&
		    snd_seq_connect_from(dev->seq_handle, 
			dev->my_port, dst.client, dst.port) < 0) {
			dbg_puts("alsa_open: couldn't connect to input\n");
			dev->mididev.eof = 1;
			return;
		}
		if ((dev->mididev.mode & MIDIDEV_MODE_OUT) &&
		    snd_seq_connect_to(dev->seq_handle, 
			dev->my_port, dst.client, dst.port) < 0) {
			dbg_puts("alsa_open: couldn't connect to output\n");
			dev->mididev.eof = 1;
			return;
		}
	}
	dev->nfds = snd_seq_poll_descriptors_count(dev->seq_handle, POLLIN);
}

void
alsa_close(struct mididev *addr)
{
	struct alsa *dev = (struct alsa *)addr;

	if (dev->iparser) {
		snd_midi_event_free(dev->iparser);
		dev->iparser = NULL;
	}
	if (dev->oparser) {
		snd_midi_event_free(dev->oparser);
		dev->oparser = NULL;
	}
	if (dev->my_port) {
		snd_seq_delete_simple_port(dev->seq_handle, dev->my_port);
		dev->my_port = -1;
	}
	(void)snd_seq_close(dev->seq_handle);
	dev->seq_handle = NULL;
	dev->mididev.eof = 1;
}

unsigned
alsa_read(struct mididev *addr, unsigned char *buf, unsigned count)
{
	struct alsa *dev = (struct alsa *)addr;
	unsigned todo = count;
	snd_seq_event_t *ev;
	long len;
	int err;
	
	if (!dev->seq_handle || !dev->iparser)
		return 0;
		
	while (todo > 0 &&
	    snd_seq_event_input_pending(dev->seq_handle, 1) > 0) {
		err = snd_seq_event_input(dev->seq_handle, &ev);
		if (err < 0) {
			dbg_puts("alsa_read: snd_seq_event_input() failed\n");
			dev->mididev.eof = 1;
			return 0;
		}
		len = snd_midi_event_decode(dev->iparser, buf, todo, ev);
		if (len < 0) {
			dbg_puts("alsa_write: failed to encode buf\n");
			dev->mididev.eof = 1;
			return 0;
		}
		todo -= len;			
		buf += len;
	}
	return count - todo;
}

unsigned
alsa_write(struct mididev *addr, unsigned char *buf, unsigned count)
{
	struct alsa *dev = (struct alsa *)addr;
	unsigned todo = count;
	snd_seq_event_t ev;
	long len;
	
	if (!dev->seq_handle || !dev->oparser)
		return 0;	

	while (todo > 0) {
		/*
		 * encode to sequencer commands
		 */
		len = snd_midi_event_encode(dev->oparser, buf, todo, &ev);
		if (len < 0) {
			dbg_puts("alsa_write: failed to encode buf\n");
			dev->mididev.eof = 1;
			return 0;
		}
		buf += len;
		todo -= len;
		if (ev.type == SND_SEQ_EVENT_NONE)
			continue;
		snd_seq_ev_set_direct(&ev);
		snd_seq_ev_set_dest(&ev, SND_SEQ_ADDRESS_SUBSCRIBERS, 255);
		snd_seq_ev_set_source(&ev, dev->my_port);
		if (snd_seq_event_output_direct(dev->seq_handle, &ev) < 0) {
			dev->mididev.eof = 1;
			return 0;
		}
	}
	return count;
}

unsigned
alsa_nfds(struct mididev *addr)
{
	struct alsa *dev = (struct alsa *)addr;

	return dev->nfds;
}

unsigned
alsa_pollfd(struct mididev *addr, struct pollfd *pfd, int events)
{
	struct alsa *dev = (struct alsa *)addr;

  	if (!dev->seq_handle) {
		dbg_puts("alsa_pollfd: no handle\n");
		return 0;
	}
	return snd_seq_poll_descriptors(dev->seq_handle, pfd, INT_MAX, events);
}

int
alsa_revents(struct mididev *addr, struct pollfd *pfd)
{
	struct alsa *dev = (struct alsa *)addr;
	unsigned short revents;

  	if (!dev->seq_handle) {
		dbg_puts("alsa_revents: no handle\n");
		return 0;
	}
	if (snd_seq_poll_descriptors_revents(dev->seq_handle,
		pfd, dev->nfds, &revents) < 0) {
		dbg_puts("alsa_revents: snd_..._revents() failed\n");
		return 0;
	}
	return revents;
}
#endif
