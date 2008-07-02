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

#include "dbg.h"
#include "default.h"
#include "node.h"
#include "exec.h"
#include "data.h"
#include "cons.h"
#include "frame.h"
#include "song.h"
#include "user.h"
#include "smf.h"
#include "saveload.h"
#include "textio.h"
#include "mux.h"
#include "rmidi.h"

unsigned
blt_panic(struct exec *o, struct data **r)
{
	dbg_panic();
	/* not reached */
	return 0;
}

unsigned
blt_debug(struct exec *o, struct data **r)
{
	char *flag;
	long value;

	if (!exec_lookupname(o, "flag", &flag) ||
	    !exec_lookuplong(o, "value", &value)) {
		return 0;
	}
	if (str_eq(flag, "rmidi")) {
		rmidi_debug = value;
	} else if (str_eq(flag, "filt")) {
		filt_debug = value;
	} else if (str_eq(flag, "song")) {
		song_debug = value;
	} else if (str_eq(flag, "mem")) {
		mem_debug = value;
	} else {
		cons_err("debug: unknuwn debug-flag");
		return 0;
	}
	return 1;
}

unsigned
blt_exec(struct exec *o, struct data **r)
{
	char *filename;

	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	return exec_runfile(o, filename);
}

unsigned
blt_print(struct exec *o, struct data **r)
{
	struct var *arg;

	arg = exec_varlookup(o, "value");
	if (!arg) {
		dbg_puts("blt_print: 'value': no such param\n");
		dbg_panic();
	}
	data_print(arg->data);
	textout_putstr(tout, "\n");
	*r = data_newnil();
	return 1;
}

unsigned
blt_err(struct exec *o, struct data **r)
{
	char *msg;

	if (!exec_lookupstring(o, "message", &msg)) {
		return 0;
	}
	cons_err(msg);
	return 0;
}

unsigned
blt_ev(struct exec *o, struct data **r)
{
	struct evspec es;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupevspec(o, "evspec", &es)) {
		return 0;
	}
	usong->curev = es;
	return 1;
}

