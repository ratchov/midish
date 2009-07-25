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
 * implements devxxx built-in functions available through the
 * interpreter
 *
 * each function is described in the manual.html file
 */

#include "dbg.h"
#include "default.h"
#include "node.h"
#include "exec.h"
#include "data.h"
#include "cons.h"

#include "mididev.h"
#include "song.h"
#include "user.h"
#include "textio.h"



unsigned
user_func_devlist(struct exec *o, struct data **r) {
	struct data *d, *n;
	struct mididev *i;

	EXEC_OBSOLETE(o);

	if (!song_try(usong)) {
		return 0;
	}
	d = data_newlist(NULL);
	for (i = mididev_list; i != NULL; i = i->next) {
		n = data_newlong(i->unit);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
user_func_devattach(struct exec *o, struct data **r) {
	long unit;
	char *path, *modename;
	unsigned mode;
	
	EXEC_OBSOLETE(o);

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "unit", &unit) || 
	    !exec_lookupstring(o, "path", &path) ||
	    !exec_lookupname(o, "mode", &modename)) {
		return 0;
	}
	if (str_eq(modename, "ro")) {
		mode = MIDIDEV_MODE_IN;
	} else if (str_eq(modename, "wo")) {
		mode = MIDIDEV_MODE_OUT;
	} else if (str_eq(modename, "rw")) {
		mode = MIDIDEV_MODE_IN | MIDIDEV_MODE_OUT;
	} else {
		cons_errs("bad mode (allowed: ro, wo, rw)", modename);
		return 0;
	}
	return mididev_attach(unit, path, mode);
}

unsigned
user_func_devdetach(struct exec *o, struct data **r) {
	long unit;

	EXEC_OBSOLETE(o);

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "unit", &unit)) {
		return 0;
	}
	return mididev_detach(unit);
}

unsigned
user_func_devsetmaster(struct exec *o, struct data **r) {
	struct var *arg;
	long unit;
	
	EXEC_OBSOLETE(o);

	if (!song_try(usong)) {
		return 0;
	}
	arg = exec_varlookup(o, "unit");
	if (!arg) {
		dbg_puts("user_func_devsetmaster: no such var\n");
		dbg_panic();
	}
	if (arg->data->type == DATA_NIL) {
		mididev_master = NULL;
		return 0;
	} else if (arg->data->type == DATA_LONG) {
		unit = arg->data->val.num;
		if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
			cons_err("no such device");
			return 0;		
		}
		mididev_master = mididev_byunit[unit];
		return 1;
	}
	
	cons_err("bad argument type for 'unit'");
	return 0;
}

unsigned
user_func_devgetmaster(struct exec *o, struct data **r) {

	EXEC_OBSOLETE(o);

	if (!song_try(usong)) {
		return 0;
	}
	if (mididev_master) {
		*r = data_newlong(mididev_master->unit);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
user_func_devsendrt(struct exec *o, struct data **r) {
	long unit, sendrt;

	EXEC_OBSOLETE(o);

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "unit", &unit) || 
	    !exec_lookupbool(o, "sendrt", &sendrt)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_err("no such device");
		return 0;		
	}
	mididev_byunit[unit]->sendrt = sendrt;
	return 1;
}

unsigned
user_func_devticrate(struct exec *o, struct data **r) {
	long unit, tpu;
	
	EXEC_OBSOLETE(o);

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "unit", &unit) || 
	    !exec_lookuplong(o, "tics_per_unit", &tpu)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_err("no such device");
		return 0;		
	}
	if (tpu < DEFAULT_TPU || (tpu % DEFAULT_TPU)) {
		cons_err("device tpu must be multiple of 96");
		return 0;
	}
	mididev_byunit[unit]->ticrate = tpu;
	return 1;
}

unsigned
user_func_devinfo(struct exec *o, struct data **r) {
	long unit;
	
	EXEC_OBSOLETE(o);

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "unit", &unit)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_err("no such device");
		return 0;		
	}
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);
	
	textout_indent(tout);
	textout_putstr(tout, "unit ");
	textout_putlong(tout, unit);
	textout_putstr(tout, "\n");

	if (mididev_master == mididev_byunit[unit]) {
		textout_indent(tout);
		textout_putstr(tout, "master\t\t\t# master clock source\n");
	}
	if (mididev_byunit[unit]->sendrt) {
		textout_indent(tout);
		textout_putstr(tout, "sendrt\t\t\t# sends real-time messages\n");
	}

	textout_indent(tout);
	textout_putstr(tout, "tics_per_unit ");
	textout_putlong(tout, mididev_byunit[unit]->ticrate);
	textout_putstr(tout, "\n");
	
	textout_shiftleft(tout);
	textout_putstr(tout, "}\n");
	return 1;
}	

unsigned
user_func_devixctl(struct exec *o, struct data **r) {
	long unit;
	struct data *list;
	unsigned ctlset;

	EXEC_OBSOLETE(o);

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "unit", &unit) || 
	    !exec_lookuplist(o, "ctlset", &list)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_err("no such device");
		return 0;		
	}
	if (!data_list2ctlset(list, &ctlset)) {
		return 0;
	}
	mididev_byunit[unit]->ixctlset = ctlset;
	return 1;
}

unsigned
user_func_devoxctl(struct exec *o, struct data **r) {
	long unit;
	struct data *list;
	unsigned ctlset;

	EXEC_OBSOLETE(o);

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "unit", &unit) || 
	    !exec_lookuplist(o, "ctlset", &list)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_err("no such device");
		return 0;		
	}
	if (!data_list2ctlset(list, &ctlset)) {
		return 0;
	}
	mididev_byunit[unit]->oxctlset = ctlset;
	return 1;
}
