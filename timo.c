/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
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
 * trivial timeouts implementation. 
 * 
 * A timeout is used to schedule the call to a routine 
 * (the callback).
 * 
 */

#include "dbg.h"
#include "timo.h"

unsigned timo_debug = 0;
struct timo *timo_queue;

/*
 * initialise a timeout structure, arguments are 
 * callback and argument that will be passed to the callback
 */
void
timo_set(struct timo *o, void (*cb)(void *), void *arg) {
	o->cb = cb;
	o->arg = arg;
	o->set = 0;
}

/*
 * schedule the callback in 'delta' 24-th of microseconds. The
 * timeout must not be already scheduled
 */
void
timo_add(struct timo *o, unsigned delta) {
	struct timo **i;
	if (o->set) {
		dbg_puts("timo_set: already set\n");
		dbg_panic();
	}
	o->set = 1;
	o->val = delta;
	for (i = &timo_queue; *i != NULL && (*i)->val < delta; i = &(*i)->next)
		; /* nothing */
	o->next = *i;
	*i = o;
}

/*
 * abort a scheduled timeout
 */
void
timo_del(struct timo *o) {
	struct timo **i;
	
	for (i = &timo_queue; *i != NULL; i = &(*i)->next) {
		if (*i == o) {
			*i = o->next;
			o->set = 0;
			return;
		}
	}
#ifdef TIMO_DEBUG
	dbg_puts("timo_del: not found\n");
#endif
}

/*
 * routine to be called by the timer when 'delta'
 * 24-th of microsecond elapsed. This routine calls
 * callbacks when necessary
 */
void
timo_update(unsigned delta) {
	struct timo **i, *to, *xhead, **xtail;
	
	/*
	 * initialize queue of expired timeouts
	 */
	xhead = NULL;
	xtail = &xhead;

	/*
	 * iterate over all timeouts, update values and move expired
	 * timeouts to the "expired queue"
	 */
	i = &timo_queue;
	for (;;) {
		to = *i;
		if (to == NULL)
			break;
		if (to->val < delta) {
			to->set = 0;
			*i = to->next;
			to->next = NULL;
			*xtail = to;
		} else {
#ifdef TIMO_DEBUG
			dbg_puts("timo_update: val: ");
			dbg_putu(to->val);
#endif
			to->val -= delta;
#ifdef TIMO_DEBUG
			dbg_puts(" -> ");
			dbg_putu(to->val);
			dbg_puts("\n");
#endif
			i = &(*i)->next;
		}
	}

	/*
	 * call call expired timeouts
	 */
	while (xhead) {
		to = xhead;
		xhead = to->next;
		to->cb(to->arg);
	}
}

/*
 * initialize the timeouts queue
 */
void
timo_init(void) {
	timo_queue = NULL;
}

/*
 * destroy the timeout queue
 */
void
timo_done(void) {
	if (timo_queue != NULL) {
		dbg_puts("timo_done: timo_queue not empty!\n");
		dbg_panic();
	}
	timo_queue = (struct timo *)0xdeadbeef;
}
