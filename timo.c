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
 * trivial timeouts implementation. 
 * 
 * A timeout is used to schedule the call of a routine (the callback)
 * there is a global list of timeouts that is processed inside the
 * event loop ie mux_run(). Timeouts work as follows:
 *
 *	first the timo structure must be initialized with timo_set()
 *
 *	then the timeout is scheduled (only once) with timo_add()
 *
 *	if the timeout expires, the call-back is called; then it can
 *	be scheduled again if needed. It's OK to reschedule it again
 *	from the callback
 *
 *	the timeout can be aborted with timo_del(), it is OK to try to
 *	abort a timout that has expired
 *
 */

#include "dbg.h"
#include "timo.h"

unsigned timo_debug = 0;
struct timo *timo_queue;
unsigned timo_abstime;

/*
 * initialise a timeout structure, arguments are callback and argument
 * that will be passed to the callback
 */
void
timo_set(struct timo *o, void (*cb)(void *), void *arg) 
{
	o->cb = cb;
	o->arg = arg;
	o->set = 0;
}

/*
 * schedule the callback in 'delta' 24-th of microseconds. The timeout
 * must not be already scheduled
 */
void
timo_add(struct timo *o, unsigned delta) 
{
	struct timo **i;
	unsigned val;
	int diff;

#ifdef TIMO_DEBUG
	if (o->set) {
		dbg_puts("timo_set: already set\n");
		dbg_panic();
	}
	if (delta == 0) {
		dbg_puts("timo_set: zero timeout is evil\n");
		dbg_panic();
	}
#endif
	val = timo_abstime + delta;
	for (i = &timo_queue; *i != NULL; i = &(*i)->next) {
		diff = (*i)->val - val;
		if (diff > 0) {
			break;
		}
	}
	o->set = 1;
	o->val = val;
	o->next = *i;
	*i = o;
}

/*
 * abort a scheduled timeout
 */
void
timo_del(struct timo *o) 
{
	struct timo **i;
	
	for (i = &timo_queue; *i != NULL; i = &(*i)->next) {
		if (*i == o) {
			*i = o->next;
			o->set = 0;
			return;
		}
	}
#ifdef TIMO_DEBUG
	if (timo_debug)
		dbg_puts("timo_del: not found\n");
#endif
}

/*
 * routine to be called by the timer when 'delta' 24-th of microsecond
 * elapsed. This routine updates time referece used by timeouts and
 * calls expired timeouts
 */
void
timo_update(unsigned delta) 
{
	struct timo *to;
	int diff;
	
	/*
	 * update time reference
	 */
	timo_abstime += delta;

	/*
	 * remove from the queue and run expired timeouts
	 */
	while (timo_queue != NULL) {
		/* 
		 * there is no overflow here because + and - are
		 * modulo 2^32, they are the same for both signed and
		 * unsigned integers
		 */
		diff = timo_queue->val - timo_abstime;
		if (diff > 0)
			break;
		to = timo_queue;
		timo_queue = to->next;
		to->set = 0;
		to->cb(to->arg);
	}
}

/*
 * initialize timeout queue
 */
void
timo_init(void) 
{
	timo_queue = NULL;
	timo_abstime = 0;
}

/*
 * destroy timeout queue
 */
void
timo_done(void) 
{
#ifdef TIMO_DEBUG
	if (timo_queue != NULL) {
		dbg_puts("timo_done: timo_queue not empty!\n");
		dbg_panic();
	}
	timo_queue = (struct timo *)0xdeadbeef;
#endif
}
