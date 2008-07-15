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
 * implements filtxxx built-in functions available through the
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

#include "song.h"
#include "user.h"
#include "saveload.h"
#include "textio.h"

/*
 * configure the filt to drop events from a particular device
 */
void
filt_conf_devdrop(struct filt *o, unsigned idev)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = idev;
	to.cmd = EVSPEC_EMPTY;
	filt_mapnew(o, &from, &to);
}

/*
 * configure the filt not to drop events from a particular device
 */
void
filt_conf_nodevdrop(struct filt *o, unsigned idev)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = idev;
	to.cmd = EVSPEC_EMPTY;
	filt_mapdel(o, &from, &to);
}

/*
 * configure the filter to map one device to another
 */
void
filt_conf_devmap(struct filt *o, unsigned idev, unsigned odev)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	from.dev_min = from.dev_max = idev;
	to.dev_min = to.dev_max = odev;
	filt_mapnew(o, &from, &to);
}

/*
 * configure the filter not to map
 * any devices to the given one
 */
void
filt_conf_nodevmap(struct filt *o, unsigned odev)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	to.dev_min = to.dev_max = odev;
	filt_mapdel(o, &from, &to);
}

/*
 * configure the filter to drop a particular channel
 */
void
filt_conf_chandrop(struct filt *o, unsigned idev, unsigned ich)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	to.cmd = EVSPEC_EMPTY;
	filt_mapnew(o, &from, &to);
}

/*
 * configure the filter to drop a particular channel
 */
void
filt_conf_nochandrop(struct filt *o, unsigned idev, unsigned ich)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	to.cmd = EVSPEC_EMPTY;
	filt_mapdel(o, &from, &to);
}

void
filt_conf_chanmap(struct filt *o, unsigned idev, unsigned ich,
    unsigned odev, unsigned och)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	filt_mapnew(o, &from, &to);
}

void
filt_conf_nochanmap(struct filt *o, unsigned odev, unsigned och)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	filt_mapdel(o, &from, &to);
}


void
filt_conf_ctldrop(struct filt *o, unsigned idev, unsigned ich, unsigned ictl)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.cmd = EVSPEC_XCTL;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = from.v0_max = ictl;
	to.cmd = EVSPEC_EMPTY;
	filt_mapnew(o, &from, &to);
}

void
filt_conf_noctldrop(struct filt *o, unsigned idev, unsigned ich, unsigned ictl)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.cmd = EVSPEC_XCTL;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = from.v0_max = ictl;
	to.cmd = EVSPEC_EMPTY;
	filt_mapdel(o, &from, &to);
}

void
filt_conf_ctlmap(struct filt *o,
    unsigned idev, unsigned ich, unsigned odev, unsigned och,
    unsigned ictl, unsigned octl)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	from.cmd = EVSPEC_CTL;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = from.v0_max = ictl;
	to.cmd = EVSPEC_CTL;
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	to.v0_min = to.v0_max = octl;
	filt_mapnew(o, &from, &to);
}

void
filt_conf_noctlmap(struct filt *o,
    unsigned odev, unsigned och, unsigned octl)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	to.cmd = EVSPEC_CTL;
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	to.v0_min = to.v0_max = octl;
	filt_mapdel(o, &from, &to);
}

void
filt_conf_keydrop(struct filt *o, unsigned idev, unsigned ich,
    unsigned keylo, unsigned keyhi)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.cmd = EVSPEC_NOTE;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = keylo;
	from.v0_max = keyhi;
	to.cmd = EVSPEC_EMPTY;
	filt_mapnew(o, &from, &to);
}

void
filt_conf_nokeydrop(struct filt *o, unsigned idev, unsigned ich,
    unsigned keylo, unsigned keyhi)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.cmd = EVSPEC_NOTE;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = keylo;
	from.v0_max = keyhi;
	to.cmd = EVSPEC_EMPTY;
	filt_mapdel(o, &from, &to);
}


void
filt_conf_keymap(struct filt *o,
    unsigned idev, unsigned ich, unsigned odev, unsigned och,
    unsigned keylo, unsigned keyhi, int keyplus)
{
	struct evspec from, to;

	if ((int)keyhi + keyplus > EV_MAXCOARSE)
		keyhi = EV_MAXCOARSE - keyplus;
	if ((int)keylo + keyplus < 0)
		keylo = -keyplus;
	if (keylo >= keyhi)
		return;

	evspec_reset(&from);
	evspec_reset(&to);
	from.cmd = EVSPEC_NOTE;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = keylo;
	from.v0_max = keyhi;
	to.cmd = EVSPEC_NOTE;
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	to.v0_min = keylo + keyplus;
	to.v0_max = keyhi + keyplus;
	filt_mapnew(o, &from, &to);
}

void
filt_conf_nokeymap(struct filt *o, unsigned odev, unsigned och,
    unsigned keylo, unsigned keyhi)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	to.cmd = EVSPEC_NOTE;
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	to.v0_min = keylo;
	to.v0_max = keyhi;
	filt_mapdel(o, &from, &to);
}

void
filt_conf_chgich(struct filt *o, unsigned olddev, unsigned oldch,
    unsigned newdev, unsigned newch)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = olddev;
	from.ch_min = from.ch_max = oldch;
	evspec_reset(&to);
	to.dev_min = to.dev_max = newdev;
	to.ch_min = to.ch_max = newch;
	filt_chgin(o, &from, &to, 0);
}

void
filt_conf_chgidev(struct filt *o, unsigned olddev, unsigned newdev)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = olddev;
	evspec_reset(&to);
	to.dev_min = to.dev_max = newdev;
	filt_chgin(o, &from, &to, 0);
}


void
filt_conf_swapich(struct filt *o, unsigned olddev, unsigned oldch,
    unsigned newdev, unsigned newch)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = olddev;
	from.ch_min = from.ch_max = oldch;
	evspec_reset(&to);
	to.dev_min = to.dev_max = newdev;
	to.ch_min = to.ch_max = newch;
	filt_chgin(o, &from, &to, 1);
}

void
filt_conf_swapidev(struct filt *o, unsigned olddev, unsigned newdev)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = olddev;
	evspec_reset(&to);
	to.dev_min = to.dev_max = newdev;
	filt_chgin(o, &from, &to, 1);
}

void
filt_conf_chgoch(struct filt *o, unsigned olddev, unsigned oldch,
    unsigned newdev, unsigned newch)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = olddev;
	from.ch_min = from.ch_max = oldch;
	evspec_reset(&to);
	to.dev_min = to.dev_max = newdev;
	to.ch_min = to.ch_max = newch;
	filt_chgout(o, &from, &to, 0);
}


void
filt_conf_chgodev(struct filt *o, unsigned olddev, unsigned newdev)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = olddev;
	evspec_reset(&to);
	to.dev_min = to.dev_max = newdev;
	filt_chgout(o, &from, &to, 0);
}


void
filt_conf_swapoch(struct filt *o, unsigned olddev, unsigned oldch,
    unsigned newdev, unsigned newch)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = olddev;
	from.ch_min = from.ch_max = oldch;
	evspec_reset(&to);
	to.dev_min = to.dev_max = newdev;
	to.ch_min = to.ch_max = newch;
	filt_chgout(o, &from, &to, 1);
}

void
filt_conf_swapodev(struct filt *o, unsigned olddev, unsigned newdev)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = olddev;
	evspec_reset(&to);
	to.dev_min = to.dev_max = newdev;
	filt_chgout(o, &from, &to, 1);
}

unsigned
user_func_filtlist(struct exec *o, struct data **r) {
	struct data *d, *n;
	struct songfilt *i;

	if (!song_try(usong)) {
		return 0;
	}
	d = data_newlist(NULL);
	SONG_FOREACH_FILT(usong, i) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
user_func_filtnew(struct exec *o, struct data **r) {
	char *name;
	struct songfilt *i;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupname(o, "filtname", &name)) {
		return 0;
	}	
	i = song_filtlookup(usong, name);
	if (i != NULL) {
		cons_err("filtnew: filt already exists");
		return 0;
	}
	i = song_filtnew(usong, name);
	return 1;
}

unsigned
user_func_filtdelete(struct exec *o, struct data **r) {
	struct songfilt *f;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f)) {
		return 0;
	}
	song_filtdel(usong, f);
	return 1;
}

unsigned
user_func_filtrename(struct exec *o, struct data **r) {
	struct songfilt *f;
	char *name;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_filtlookup(usong, name)) {
		cons_err("name already used by another filt");
		return 0;
	}
	str_delete(f->name.str);
	f->name.str = str_new(name);
	return 1;
}

unsigned
user_func_filtexists(struct exec *o, struct data **r) {
	char *name;
	struct songfilt *i;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupname(o, "filtname", &name)) {
		return 0;
	}
	i = song_filtlookup(usong, name);
	*r = data_newlong(i != NULL ? 1 : 0);
	return 1;
}

unsigned
user_func_filtinfo(struct exec *o, struct data **r) {
	struct songfilt *f;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f)) {
		return 0;
	}
	filt_output(&f->filt, tout);
	textout_putstr(tout, "\n");
	return 1;
}

unsigned
user_func_filtdevdrop(struct exec *o, struct data **r) {
	struct songfilt *f;
	long idev;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookuplong(o, "indev", &idev)) {
		return 0;
	}
	if (idev < 0 || idev > EV_MAXDEV) {
	    	cons_err("device number out of range");
		return 0;
	}
	filt_conf_devdrop(&f->filt, idev);
	return 1;
}

unsigned
user_func_filtnodevdrop(struct exec *o, struct data **r) {
	struct songfilt *f;
	long idev;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookuplong(o, "indev", &idev)) {
		return 0;
	}
	if (idev < 0 || idev > EV_MAXDEV) {
	    	cons_err("device number out of range");
		return 0;
	}
	filt_conf_nodevdrop(&f->filt, idev);
	return 1;
}

unsigned
user_func_filtdevmap(struct exec *o, struct data **r) {
	struct songfilt *f;
	long idev, odev;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookuplong(o, "indev", &idev) ||
	    !exec_lookuplong(o, "outdev", &odev)) {
		return 0;
	}
	if (idev < 0 || idev > EV_MAXDEV ||
	    odev < 0 || odev > EV_MAXDEV) {
	    	cons_err("device number out of range");
		return 0;
	}
	filt_conf_devmap(&f->filt, idev, odev);
	return 1;
}

unsigned
user_func_filtnodevmap(struct exec *o, struct data **r) {
	struct songfilt *f;
	long odev;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookuplong(o, "outdev", &odev)) {
		return 0;
	}
	if (odev < 0 || odev > EV_MAXDEV) {
	    	cons_err("device number out of range");
		return 0;
	}
	filt_conf_nodevmap(&f->filt, odev);
	return 1;
}

unsigned
user_func_filtchandrop(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned idev, ich;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich, 1)) {
		return 0;
	}
	filt_conf_chandrop(&f->filt, idev, ich);
	return 1;
}

unsigned
user_func_filtnochandrop(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned idev, ich;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich, 1)) {
		return 0;
	}
	filt_conf_nochandrop(&f->filt, idev, ich);
	return 1;
}

unsigned
user_func_filtchanmap(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned idev, ich, odev, och;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich, 1) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och, 0)) {
		return 0;
	}
	filt_conf_chanmap(&f->filt, idev, ich, odev, och);
	return 1;
}

unsigned
user_func_filtnochanmap(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned odev, och;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och, 0)) {
		return 0;
	}
	filt_conf_nochanmap(&f->filt, odev, och);
	return 1;
}

unsigned
user_func_filtctldrop(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned idev, ich, ictl;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich, 1) ||
	    !exec_lookupctl(o, "inctl", &ictl)) {
		return 0;
	}
	filt_conf_ctldrop(&f->filt, idev, ich, ictl);
	return 1;
}

unsigned
user_func_filtnoctldrop(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned idev, ich, ictl;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich, 1) ||
	    !exec_lookupctl(o, "inctl", &ictl)) {
		return 0;
	}
	filt_conf_noctldrop(&f->filt, idev, ich, ictl);
	return 1;
}

unsigned
user_func_filtctlmap(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned idev, ich, odev, och, ictl, octl;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich, 1) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och, 0) ||
	    !exec_lookupctl(o, "inctl", &ictl) || 
	    !exec_lookupctl(o, "outctl", &octl)) {
		return 0;
	}
	filt_conf_ctlmap(&f->filt, idev, ich, odev, och, ictl, octl);
	return 1;
}

unsigned
user_func_filtnoctlmap(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned odev, och, octl;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och, 0) || 
	    !exec_lookupctl(o, "outctl", &octl)) {
		return 0;
	}
	filt_conf_noctlmap(&f->filt, odev, och, octl);
	return 1;
}

unsigned
user_func_filtkeydrop(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned idev, ich;
	long kstart, kend;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich, 1) ||
	    !exec_lookuplong(o, "keystart", &kstart) || 
	    !exec_lookuplong(o, "keyend", &kend)) {
		return 0;
	}
	if (kstart < 0 || kstart > EV_MAXCOARSE || 
	    kend < 0 || kend > EV_MAXCOARSE) {
		cons_err("filtkeymap: notes must be between 0 and 127");
		return 0;
	}
	filt_conf_keydrop(&f->filt, idev, ich, kstart, kend);
	return 1;
}

unsigned
user_func_filtnokeydrop(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned idev, ich;
	long kstart, kend;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich, 1) ||
	    !exec_lookuplong(o, "keystart", &kstart) || 
	    !exec_lookuplong(o, "keyend", &kend)) {
		return 0;
	}
	if (kstart < 0 || kstart > EV_MAXCOARSE || 
	    kend < 0 || kend > EV_MAXCOARSE) {
		cons_err("filtkeymap: notes must be between 0 and 127");
		return 0;
	}
	filt_conf_nokeydrop(&f->filt, idev, ich, kstart, kend);
	return 1;
}

unsigned
user_func_filtkeymap(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned idev, ich, odev, och;
	long kstart, kend, kplus;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich, 1) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och, 0) ||
	    !exec_lookuplong(o, "keystart", &kstart) || 
	    !exec_lookuplong(o, "keyend", &kend) ||
	    !exec_lookuplong(o, "keyplus", &kplus)) {
		return 0;
	}
	if (kstart < 0 || kstart > EV_MAXCOARSE || 
	    kend < 0 || kend > EV_MAXCOARSE) {
		cons_err("filtkeymap: notes must be between 0 and 127");
		return 0;
	}
	if (kplus < - EV_MAXCOARSE / 2 || kplus > EV_MAXCOARSE / 2) {
		cons_err("filtkeymap: transpose must be between -63 and 63");
		return 0;
	}
	filt_conf_keymap(&f->filt, idev, ich, odev, och, kstart, kend, kplus);
	return 1;
}

unsigned
user_func_filtnokeymap(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned odev, och;
	long kstart, kend;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och, 0) ||
	    !exec_lookuplong(o, "keystart", &kstart) || 
	    !exec_lookuplong(o, "keyend", &kend)) {
		return 0;
	}
	if (kstart < 0 || kstart > EV_MAXCOARSE || 
	    kend < 0 || kend > EV_MAXCOARSE) {
		cons_err("filtkeymap: notes must be between 0 and 127");
		return 0;
	}
	filt_conf_nokeymap(&f->filt, odev, och, kstart, kend);
	return 1;
}

unsigned
user_func_filtreset(struct exec *o, struct data **r) {
	struct songfilt *f;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f)) {
		return 0;
	}
	filt_reset(&f->filt);
	return 1;
}

unsigned
user_func_filtchgich(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned olddev, oldch, newdev, newch;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "oldchan", &olddev, &oldch, 1) ||
	    !exec_lookupchan_getnum(o, "newchan", &newdev, &newch, 1)) {
		return 0;
	}
	filt_conf_chgich(&f->filt, olddev, oldch, newdev, newch);
	return 1;
}

unsigned
user_func_filtchgidev(struct exec *o, struct data **r) {
	struct songfilt *f;
	long olddev, newdev;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookuplong(o, "olddev", &olddev) ||
	    !exec_lookuplong(o, "newdev", &newdev)) {
		return 0;
	}
	if (olddev < 0 || olddev > EV_MAXDEV || 
	    newdev < 0 || newdev > EV_MAXDEV) {
		cons_err("dev numver out of bounds");
		return 0;
	}
	filt_conf_chgidev(&f->filt, olddev, newdev);
	return 1;
}

unsigned
user_func_filtswapich(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned olddev, oldch, newdev, newch;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "oldchan", &olddev, &oldch, 1) ||
	    !exec_lookupchan_getnum(o, "newchan", &newdev, &newch, 1)) {
		return 0;
	}
	filt_conf_swapich(&f->filt, olddev, oldch, newdev, newch);
	return 1;
}

unsigned
user_func_filtswapidev(struct exec *o, struct data **r) {
	struct songfilt *f;
	long olddev, newdev;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookuplong(o, "olddev", &olddev) ||
	    !exec_lookuplong(o, "newdev", &newdev)) {
		return 0;
	}
	if (olddev < 0 || olddev > EV_MAXDEV || 
	    newdev < 0 || newdev > EV_MAXDEV) {
		cons_err("dev numver out of bounds");
		return 0;
	}
	filt_conf_swapidev(&f->filt, olddev, newdev);
	return 1;
}

unsigned
user_func_filtchgoch(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned olddev, oldch, newdev, newch;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "oldchan", &olddev, &oldch, 0) ||
	    !exec_lookupchan_getnum(o, "newchan", &newdev, &newch, 0)) {
		return 0;
	}
	filt_conf_chgoch(&f->filt, olddev, oldch, newdev, newch);
	return 1;
}

unsigned
user_func_filtchgodev(struct exec *o, struct data **r) {
	struct songfilt *f;
	long olddev, newdev;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookuplong(o, "olddev", &olddev) ||
	    !exec_lookuplong(o, "newdev", &newdev)) {
		return 0;
	}
	if (olddev < 0 || olddev > EV_MAXDEV || 
	    newdev < 0 || newdev > EV_MAXDEV) {
		cons_err("dev numver out of bounds");
		return 0;
	}
	filt_conf_chgodev(&f->filt, olddev, newdev);
	return 1;
}

unsigned
user_func_filtswapoch(struct exec *o, struct data **r) {
	struct songfilt *f;
	unsigned olddev, oldch, newdev, newch;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "oldchan", &olddev, &oldch, 0) ||
	    !exec_lookupchan_getnum(o, "newchan", &newdev, &newch, 0)) {
		return 0;
	}
	filt_conf_swapoch(&f->filt, olddev, oldch, newdev, newch);
	return 1;
}

unsigned
user_func_filtswapodev(struct exec *o, struct data **r) {
	struct songfilt *f;
	long olddev, newdev;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookuplong(o, "olddev", &olddev) ||
	    !exec_lookuplong(o, "newdev", &newdev)) {
		return 0;
	}
	if (olddev < 0 || olddev > EV_MAXDEV || 
	    newdev < 0 || newdev > EV_MAXDEV) {
		cons_err("dev numver out of bounds");
		return 0;
	}
	filt_conf_swapodev(&f->filt, olddev, newdev);
	return 1;
}

unsigned
user_func_filtevmap(struct exec *o, struct data **r) {
	struct songfilt *f;
	struct evspec from, to;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupevspec(o, "from", &from, 1) ||
	    !exec_lookupevspec(o, "to", &to, 0)) {
		return 0;
	}
	filt_mapnew(&f->filt, &from, &to);
	return 1;
}

unsigned
user_func_filtevunmap(struct exec *o, struct data **r) {
	struct songfilt *f;
	struct evspec from, to;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupevspec(o, "from", &from, 1) ||
	    !exec_lookupevspec(o, "to", &to, 0)) {
		return 0;
	}
	filt_mapdel(&f->filt, &from, &to);
	return 1;
}

unsigned
user_func_filtsetcurchan(struct exec *o, struct data **r) {
	struct songfilt *f;
	struct songchan *c;
	struct var *arg;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f)) {
		return 0;
	}	
	arg = exec_varlookup(o, "channame");
	if (!arg) {
		dbg_puts("user_func_filtsetcurchan: 'channame': no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		f->curchan = NULL;
		return 1;
	} else if (arg->data->type == DATA_REF) {
		c = song_chanlookup(usong, arg->data->val.ref, 0);
		if (!c) {
			cons_err("no such chan");
			return 0;
		}
		f->curchan = c;
		return 1;
	}
	return 0;
}

unsigned
user_func_filtgetcurchan(struct exec *o, struct data **r) {
	struct songfilt *f;
	
	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupfilt(o, "filtname", &f)) {
		return 0;
	}
	if (f->curchan) {
		*r = data_newref(f->curchan->name.str);
	} else {
		*r = data_newnil();
	}		
	return 1;
}
