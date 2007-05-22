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
#include "state.h"
#include "conv.h"

#define CHAN_MATCH(e1, e2) \
	((e1)->ch == (e2)->ch && (e1)->dev == (e2)->dev)

#define CTL_MATCH(e1, e2) \
	((e1)->ctl_num == (e2)->ctl_num && CHAN_MATCH((e1), (e2)))

void
conv_setctl(struct statelist *slist, struct ev *ev) {
	struct state *i;

	for (i = slist->first; i != NULL; i = i->next) {
		if (CTL_MATCH(&i->ev, ev)) {
			i->ev.ctl_val = ev->ctl_val;
			return;
		}
	}
	i = state_new();
	statelist_add(slist, i);
	i->ev = *ev;
}

unsigned
conv_getctl(struct statelist *slist, struct ev *ev, unsigned num) {
	struct state *i;

	for (i = slist->first; i != NULL; i = i->next) {
		if (i->ev.ctl_num == num && CHAN_MATCH(&i->ev, ev)) {
			return i->ev.ctl_val;
		}
	}
	return EV_UNDEF;
}

void
conv_rmctl(struct statelist *slist, struct ev *ev, unsigned num) {
	struct state *i;

	for (i = slist->first; i != NULL; i = i->next) {
		if (i->ev.ctl_num == num && CHAN_MATCH(&i->ev, ev)) {
			statelist_rm(slist, i);
			state_del(i);
			break;
		}
	}
}

unsigned
conv_getctx(struct statelist *slist, struct ev *ev, unsigned hi, unsigned lo) {
	unsigned vhi, vlo;

	vlo = conv_getctl(slist, ev, lo);
	if (vlo == EV_UNDEF) {
		return EV_UNDEF;
	}
	vhi = conv_getctl(slist, ev, hi);
	if (vhi == EV_UNDEF) {
		return EV_UNDEF;
	}
	return vlo + (vhi << 7);
}

/*
 * convert an old-style MIDI event (ie CTL, PC) to new-style midish
 * event (ie XCTL, NRPN, RPN, XPC). If an event is available, 'rev'
 * parameter is filled and 1 is returned.
 */
unsigned
conv_packev(struct statelist *l, struct ev *ev, struct ev *rev) {
	unsigned num, val;

	if (ev->cmd == EV_PC) {
		rev->cmd = EV_XPC;
		rev->dev = ev->dev;
		rev->ch = ev->ch;
		rev->pc_prog = ev->pc_prog;
		rev->pc_bank = conv_getctx(l, ev, BANK_HI, BANK_LO);
		return 1;
	} else if (ev->cmd == EV_CTL) {
		switch (ev->ctl_num) {
		case BANK_HI:
			conv_rmctl(l, ev, BANK_LO);
			conv_setctl(l, ev);
			break;
		case RPN_HI:
			conv_rmctl(l, ev, NRPN_LO);
			conv_rmctl(l, ev, RPN_LO);
			conv_setctl(l, ev);
			break;
		case NRPN_HI:
			conv_rmctl(l, ev, RPN_LO);
			conv_rmctl(l, ev, NRPN_LO);
			conv_setctl(l, ev);
			break;
		case DATAENT_HI:
			conv_rmctl(l, ev, DATAENT_LO);
			conv_setctl(l, ev);
			break;
		case BANK_LO:
			conv_setctl(l, ev);
			break;
		case NRPN_LO:
			conv_rmctl(l, ev, RPN_LO);
			conv_setctl(l, ev);
			break;
		case RPN_LO:
			conv_rmctl(l, ev, NRPN_LO);
			conv_setctl(l, ev);
			break;
		case DATAENT_LO:
			num = conv_getctx(l, ev, NRPN_HI, NRPN_LO);
			if (num != EV_UNDEF) {
				rev->cmd = EV_NRPN;
			} else {
				num = conv_getctx(l, ev, RPN_HI, NRPN_LO);
				if (num == EV_UNDEF) {
					break;
				}
				rev->cmd = EV_RPN;
			}
			val = conv_getctl(l, ev, DATAENT_HI);
			if (val == EV_UNDEF) {
				break;
			}
			rev->dev = ev->dev;
			rev->ch = ev->ch;
			rev->ctl_num = num;
			rev->ctl_val = ev->ctl_val + (val << 7);
			return 1;
		default:
			/* 
			 * XXX: do something with 14bit controllers here
			 */
			rev->cmd = EV_XCTL;
			rev->dev = ev->dev;
			rev->ch = ev->ch;
			rev->ctl_num = ev->ctl_num;
			rev->ctl_val = ev->ctl_val << 7;
			return 1;
		}
		return 0;
	} else {
		*rev = *ev;
		return 1;
	}
}

/*
 * convert midish "advanced" events (XCTL, RPN, NRPN, XPC) to
 * standard controller events. Renturn the number of events 
 * filled
 */
unsigned
conv_unpackev(struct statelist *slist, struct ev *ev, struct ev *rev) {
	unsigned val;
	unsigned nev = 0;

	if (ev->cmd == EV_XCTL) {
		rev->cmd = EV_CTL;
		rev->dev = ev->dev;
		rev->ch = ev->ch;
		rev->ctl_num = ev->ctl_num;
		rev->ctl_val = ev->ctl_val >> 7;
		return 1;
	} else if (ev->cmd == EV_XPC) {
		val = conv_getctx(slist, ev, BANK_HI, BANK_LO);
		if (val != ev->pc_bank && ev->pc_bank != EV_UNDEF) {
			rev->cmd = EV_CTL;
			rev->dev = ev->dev;
			rev->ch = ev->ch;
			rev->ctl_num = BANK_HI;
			rev->ctl_val = ev->pc_bank >> 7;
			conv_setctl(slist, rev);
			rev++; 
			nev++;
			rev->cmd = EV_CTL;
			rev->dev = ev->dev;
			rev->ch = ev->ch;
			rev->ctl_num = BANK_LO;
			rev->ctl_val = ev->pc_bank & 0x7f;
			conv_setctl(slist, rev);
			rev++; 
			nev++;
		}
		rev->cmd = EV_PC;
		rev->dev = ev->dev;
		rev->ch = ev->ch;
		rev->pc_prog = ev->pc_prog;
		rev++;
		nev++;
		return nev;
	} else if (ev->cmd == EV_NRPN) {
		val = conv_getctx(slist, ev, NRPN_HI, NRPN_LO);
		if (val != ev->rpn_num) {
			rev->cmd = EV_CTL;
			rev->dev = ev->dev;
			rev->ch = ev->ch;
			rev->ctl_num = NRPN_HI;
			rev->ctl_val = ev->rpn_num >> 7;
			conv_setctl(slist, rev);
			rev++; 
			nev++;
			rev->cmd = EV_CTL;
			rev->dev = ev->dev;
			rev->ch = ev->ch;
			rev->ctl_num = NRPN_LO;
			rev->ctl_val = ev->rpn_num & 0x7f;
			conv_setctl(slist, rev);
			rev++; 
			nev++;
		}
	dataentry:
		rev->cmd = EV_CTL;
		rev->dev = ev->dev;
		rev->ch = ev->ch;
		rev->ctl_num = DATAENT_HI;
		rev->ctl_val = ev->rpn_val >> 7;
		rev++;
		nev++;
		rev->cmd = EV_CTL;
		rev->dev = ev->dev;
		rev->ch = ev->ch;
		rev->ctl_num = DATAENT_LO;
		rev->ctl_val = ev->rpn_val & 0x7f;
		rev++;
		nev++;
		return nev;
	} else if (ev->cmd == EV_NRPN) {
		val = conv_getctx(slist, ev, NRPN_HI, NRPN_LO);
		if (val != ev->rpn_num) {
			rev->cmd = EV_CTL;
			rev->dev = ev->dev;
			rev->ch = ev->ch;
			rev->ctl_num = NRPN_HI;
			rev->ctl_val = ev->rpn_num >> 7;
			conv_setctl(slist, rev);
			rev++; 
			nev++;
			rev->cmd = EV_CTL;
			rev->dev = ev->dev;
			rev->ch = ev->ch;
			rev->ctl_num = NRPN_LO;
			rev->ctl_val = ev->rpn_num & 0x7f;
			conv_setctl(slist, rev);
			rev++; 
			nev++;
		}
		goto dataentry;
	} else {
		*rev = *ev;
		return 1;
	}
}
