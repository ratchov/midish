/* $Id: user_filt.c,v 1.9 2006/02/14 12:21:41 alex Exp $ */
/*
 * Copyright (c) 2003-2006 Alexandre Ratchov
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
 * implements filtxxx built-in functions
 * available through the interpreter
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



unsigned
user_func_filtlist(struct exec_s *o, struct data_s **r) {
	struct data_s *d, *n;
	struct songfilt_s *i;

	d = data_newlist(NULL);
	for (i = user_song->filtlist; i != NULL; i = (struct songfilt_s *)i->name.next) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
user_func_filtnew(struct exec_s *o, struct data_s **r) {
	char *name;
	struct songfilt_s *i;
	
	if (!exec_lookupname(o, "filtname", &name)) {
		return 0;
	}	
	i = song_filtlookup(user_song, name);
	if (i != NULL) {
		cons_err("filtnew: filt already exists");
		return 0;
	}
	i = songfilt_new(name);
	song_filtadd(user_song, i);
	return 1;
}

unsigned
user_func_filtdelete(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	if (!exec_lookupfilt(o, "filtname", &f)) {
		return 0;
	}
	if (!song_filtrm(user_song, f)) {
		return 0;
	}
	songfilt_delete(f);
	return 1;
}

unsigned
user_func_filtrename(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	char *name;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_filtlookup(user_song, name)) {
		cons_err("name already used by another filt");
		return 0;
	}
	str_delete(f->name.str);
	f->name.str = str_new(name);
	return 1;
}

unsigned
user_func_filtexists(struct exec_s *o, struct data_s **r) {
	char *name;
	struct songfilt_s *i;
	if (!exec_lookupname(o, "filtname", &name)) {
		return 0;
	}
	i = song_filtlookup(user_song, name);
	*r = data_newlong(i != NULL ? 1 : 0);
	return 1;
}

unsigned
user_func_filtinfo(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	
	if (!exec_lookupfilt(o, "filtname", &f)) {
		return 0;
	}
	filt_output(&f->filt, tout);
	textout_putstr(tout, "\n");
	return 1;
}


unsigned
user_func_filtdevdrop(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	long idev;
	
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
user_func_filtnodevdrop(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	long idev;
	
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
user_func_filtdevmap(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	long idev, odev;
	
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
user_func_filtnodevmap(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	long odev;
	
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
user_func_filtchandrop(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned idev, ich;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich)) {
		return 0;
	}
	filt_conf_chandrop(&f->filt, idev, ich);
	return 1;
}


unsigned
user_func_filtnochandrop(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned idev, ich;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich)) {
		return 0;
	}
	filt_conf_nochandrop(&f->filt, idev, ich);
	return 1;
}

unsigned
user_func_filtchanmap(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned idev, ich, odev, och;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och)) {
		return 0;
	}
	filt_conf_chanmap(&f->filt, idev, ich, odev, och);
	return 1;
}

unsigned
user_func_filtnochanmap(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned odev, och;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och)) {
		return 0;
	}
	filt_conf_nochanmap(&f->filt, odev, och);
	return 1;
}

unsigned
user_func_filtctldrop(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned idev, ich;
	long ictl;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich) ||
	    !exec_lookuplong(o, "inctl", &ictl)) {
		return 0;
	}
	if (ictl < 0 || ictl > EV_MAXB0) {
		cons_err("filtctlmap: controllers must be between 0 and 127");
		return 0;
	}
	filt_conf_ctldrop(&f->filt, idev, ich, ictl);
	return 1;
}


unsigned
user_func_filtnoctldrop(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned idev, ich;
	long ictl;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich) ||
	    !exec_lookuplong(o, "inctl", &ictl)) {
		return 0;
	}
	if (ictl < 0 || ictl > EV_MAXB0) {
		cons_err("filtctlmap: controllers must be between 0 and 127");
		return 0;
	}
	filt_conf_noctldrop(&f->filt, idev, ich, ictl);
	return 1;
}


unsigned
user_func_filtctlmap(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned idev, ich, odev, och;
	long ictl, octl;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och) ||
	    !exec_lookuplong(o, "inctl", &ictl) || 
	    !exec_lookuplong(o, "outctl", &octl)) {
		return 0;
	}
	if (ictl < 0 || ictl > EV_MAXB0 || octl < 0 || octl > EV_MAXB0) {
		cons_err("filtctlmap: controllers must be between 0 and 127");
		return 0;
	}
	filt_conf_ctlmap(&f->filt, idev, ich, odev, och, ictl, octl);
	return 1;
}


unsigned
user_func_filtnoctlmap(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned odev, och;
	long octl;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och) || 
	    !exec_lookuplong(o, "outctl", &octl)) {
		return 0;
	}
	if (octl < 0 || octl > EV_MAXB0) {
		cons_err("filtctlmap: controllers must be between 0 and 127");
		return 0;
	}
	filt_conf_noctlmap(&f->filt, odev, och, octl);
	return 1;
}

unsigned
user_func_filtkeydrop(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned idev, ich;
	long kstart, kend;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich) ||
	    !exec_lookuplong(o, "keystart", &kstart) || 
	    !exec_lookuplong(o, "keyend", &kend)) {
		return 0;
	}
	if (kstart < 0 || kstart > EV_MAXB0 || kend < 0 || kend > EV_MAXB0) {
		cons_err("filtkeymap: notes must be between 0 and 127");
		return 0;
	}
	filt_conf_keydrop(&f->filt, idev, ich, kstart, kend);
	return 1;
}


unsigned
user_func_filtnokeydrop(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned idev, ich;
	long kstart, kend;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich) ||
	    !exec_lookuplong(o, "keystart", &kstart) || 
	    !exec_lookuplong(o, "keyend", &kend)) {
		return 0;
	}
	if (kstart < 0 || kstart > EV_MAXB0 || kend < 0 || kend > EV_MAXB0) {
		cons_err("filtkeymap: notes must be between 0 and 127");
		return 0;
	}
	filt_conf_nokeydrop(&f->filt, idev, ich, kstart, kend);
	return 1;
}


unsigned
user_func_filtkeymap(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned idev, ich, odev, och;
	long kstart, kend, kplus;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "inchan", &idev, &ich) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och) ||
	    !exec_lookuplong(o, "keystart", &kstart) || 
	    !exec_lookuplong(o, "keyend", &kend) ||
	    !exec_lookuplong(o, "keyplus", &kplus)) {
		return 0;
	}
	if (kstart < 0 || kstart > EV_MAXB0 || kend < 0 || kend > EV_MAXB0) {
		cons_err("filtkeymap: notes must be between 0 and 127");
		return 0;
	}
	if (kplus < - EV_MAXB0/2 || kplus > EV_MAXB0/2) {
		cons_err("filtkeymap: transpose must be between -63 and 63");
		return 0;
	}
	filt_conf_keymap(&f->filt, idev, ich, odev, och, kstart, kend, kplus);
	return 1;
}


unsigned
user_func_filtnokeymap(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned odev, och;
	long kstart, kend;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "outchan", &odev, &och) ||
	    !exec_lookuplong(o, "keystart", &kstart) || 
	    !exec_lookuplong(o, "keyend", &kend)) {
		return 0;
	}
	if (kstart < 0 || kstart > EV_MAXB0 || kend < 0 || kend > EV_MAXB0) {
		cons_err("filtkeymap: notes must be between 0 and 127");
		return 0;
	}
	filt_conf_nokeymap(&f->filt, odev, och, kstart, kend);
	return 1;
}



unsigned
user_func_filtreset(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	
	if (!exec_lookupfilt(o, "filtname", &f)) {
		return 0;
	}
	filt_reset(&f->filt);
	return 1;
}


unsigned
user_func_filtchgich(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned olddev, oldch, newdev, newch;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "oldchan", &olddev, &oldch) ||
	    !exec_lookupchan_getnum(o, "newchan", &newdev, &newch)) {
		return 0;
	}
	filt_conf_chgich(&f->filt, olddev, oldch, newdev, newch);
	return 1;
}


unsigned
user_func_filtchgidev(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	long olddev, newdev;
	
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
user_func_filtswapich(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned olddev, oldch, newdev, newch;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "oldchan", &olddev, &oldch) ||
	    !exec_lookupchan_getnum(o, "newchan", &newdev, &newch)) {
		return 0;
	}
	filt_conf_swapich(&f->filt, olddev, oldch, newdev, newch);
	return 1;
}


unsigned
user_func_filtswapidev(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	long olddev, newdev;
	
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
user_func_filtchgoch(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned olddev, oldch, newdev, newch;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "oldchan", &olddev, &oldch) ||
	    !exec_lookupchan_getnum(o, "newchan", &newdev, &newch)) {
		return 0;
	}
	filt_conf_chgoch(&f->filt, olddev, oldch, newdev, newch);
	return 1;
}


unsigned
user_func_filtchgodev(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	long olddev, newdev;
	
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
user_func_filtswapoch(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	unsigned olddev, oldch, newdev, newch;
	
	if (!exec_lookupfilt(o, "filtname", &f) ||
	    !exec_lookupchan_getnum(o, "oldchan", &olddev, &oldch) ||
	    !exec_lookupchan_getnum(o, "newchan", &newdev, &newch)) {
		return 0;
	}
	filt_conf_swapoch(&f->filt, olddev, oldch, newdev, newch);
	return 1;
}


unsigned
user_func_filtswapodev(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	long olddev, newdev;
	
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
user_func_filtsetcurchan(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	struct songchan_s *c;
	struct var_s *arg;
	
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
		c = song_chanlookup(user_song, arg->data->val.ref);
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
user_func_filtgetcurchan(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	
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
