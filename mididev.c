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
 * mididev_s is a generic midi device structure
 * it doesn't containt any device-specific fields
 * and shoud be extended by other structures
 *
 * this module also, manages a global list of generic
 * midi devices
 */

#include "dbg.h"
#include "default.h"
#include "mididev.h"
#include "data.h"
#include "name.h"
#include "rmidi.h"
#include "pool.h"

#include "cons.h"

/* -------------------------------------------- device management --- */

unsigned mididev_ticrate;	/* global dics per unit */

void
mididev_init(struct mididev_s *o) {
	/* 
	 * by default we don't transmit realtime information 
	 * (midi_tic, midi_start, midi_stop etc...)
	 */
	o->sendrt = 0;
	o->ticrate = DEFAULT_TPU;
	o->ticdelta = 0xdeadbeef;
}

void
mididev_done(struct mididev_s *o) {
}

/* -------------------------------------------- device list stuff --- */

struct mididev_s *mididev_list, *mididev_master;
struct mididev_s *mididev_byunit[DEFAULT_MAXNDEVS];

void
mididev_listinit(void) {
	unsigned i;
	for (i = 0; i < DEFAULT_MAXNDEVS; i++) {
		mididev_byunit[i] = NULL;
	}
	mididev_list = NULL;
	mididev_master = NULL;	/* no master, use internal clock */
}

void
mididev_listdone(void) {
	unsigned i;
	struct mididev_s *dev;
	
	for (i = 0; i < DEFAULT_MAXNDEVS; i++) {
		dev = mididev_byunit[i];
		if (dev != NULL) {
			rmidi_done(RMIDI(dev));
			mididev_byunit[i] = NULL;
		}
	}
	mididev_master = NULL;
	mididev_list = NULL;
}

unsigned
mididev_attach(unsigned unit, char *path, unsigned in, unsigned out) {
	struct mididev_s *dev;

	if (unit >= DEFAULT_MAXNDEVS) {
		cons_err("given unit is too large");
		return 0;
	}
	if (mididev_byunit[unit] != NULL) {
		cons_err("device already exists");
		return 0;
	}
	dev = (struct mididev_s *)rmidi_new();
	RMIDI(dev)->mdep.path = str_new(path);
	dev->next = mididev_list;
	mididev_list = dev;
	mididev_byunit[unit] = dev;
	dev->in = in;
	dev->out = out;
	dev->unit = unit;	
	return 1;
}

unsigned
mididev_detach(unsigned unit) {
	struct mididev_s **i, *dev;
	
	if (unit >= DEFAULT_MAXNDEVS || mididev_byunit[unit] == NULL) {
		cons_err("no such device");
		return 0;
	}
	
	if (mididev_byunit[unit] == mididev_master) {
		cons_err("cant detach master device");
		return 0;
	}
	
	for (i = &mididev_list; *i != NULL; i = &(*i)->next) {
		dev = *i;
		if (dev->unit == unit) {
			*i = dev->next;
			str_delete(RMIDI(dev)->mdep.path);
			rmidi_delete(RMIDI(dev));
			mididev_byunit[unit] = NULL;
			return 1;
		}
	}
	dbg_puts("mididev_detach: the device is not in the list\n");
	dbg_panic();
	return 0;
}


