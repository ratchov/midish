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
 * mididev is a generic midi device structure it doesn't contain any
 * device-specific fields and shoud be extended by other structures
 *
 * this module also, manages a global table of generic midi
 * devices. The table is indexed by the device "unit" number, the
 * same that is stored in the event structure
 */

#include "dbg.h"
#include "default.h"
#include "mididev.h"
#include "rmidi.h"
#include "pool.h"
#include "cons.h"
#include "str.h"

struct mididev *mididev_list, *mididev_master;
struct mididev *mididev_byunit[DEFAULT_MAXNDEVS];

/*
 * initialize the device independent part of the device structure
 */
void
mididev_init(struct mididev *o, unsigned mode) 
{
	/* 
	 * by default we don't transmit realtime information 
	 * (midi_tic, midi_start, midi_stop etc...)
	 */
	o->sendrt = 0;
	o->ticrate = DEFAULT_TPU;
	o->ticdelta = 0xdeadbeef;
	o->mode = mode;
	o->ixctlset = 0;	/* all input controllers are 7bit */
	o->oxctlset = 0;
}

/*
 * release the device independent part of the device structure: for
 * future use
 */
void
mididev_done(struct mididev *o) 
{
}

/*
 * initialize the device table
 */
void
mididev_listinit(void) 
{
	unsigned i;
	for (i = 0; i < DEFAULT_MAXNDEVS; i++) {
		mididev_byunit[i] = NULL;
	}
	mididev_list = NULL;
	mididev_master = NULL;	/* no master, use internal clock */
}

/*
 * unregister all entries of the device table
 */
void
mididev_listdone(void) 
{
	unsigned i;
	struct mididev *dev;
	
	for (i = 0; i < DEFAULT_MAXNDEVS; i++) {
		dev = mididev_byunit[i];
		if (dev != NULL) {
			str_delete(RMIDI(dev)->mdep.path);
			rmidi_delete(RMIDI(dev));
			mididev_byunit[i] = NULL;
		}
	}
	mididev_master = NULL;
	mididev_list = NULL;
}

/*
 * register a new device number (ie "unit")
 */
unsigned
mididev_attach(unsigned unit, char *path, unsigned mode) 
{
	struct mididev *dev;

	if (unit >= DEFAULT_MAXNDEVS) {
		cons_err("given unit is too large");
		return 0;
	}
	if (mididev_byunit[unit] != NULL) {
		cons_err("device already exists");
		return 0;
	}
	dev = (struct mididev *)rmidi_new(mode);
	RMIDI(dev)->mdep.path = str_new(path);
	dev->next = mididev_list;
	mididev_list = dev;
	mididev_byunit[unit] = dev;
	dev->unit = unit;	
	return 1;
}

/*
 * unregister the given device number
 */
unsigned
mididev_detach(unsigned unit) 
{
	struct mididev **i, *dev;
	
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
