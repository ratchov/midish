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

#ifndef MIDISH_BUILTIN_H
#define MIDISH_BUILTIN_H

struct exec;
struct data;

unsigned blt_panic(struct exec *, struct data **);
unsigned blt_debug(struct exec *, struct data **);
unsigned blt_exec(struct exec *, struct data **);
unsigned blt_print(struct exec *, struct data **);
unsigned blt_err(struct exec *, struct data **);
unsigned blt_ev(struct exec *, struct data **);

unsigned blt_ev(struct exec *, struct data **);
unsigned blt_cc(struct exec *, struct data **);
unsigned blt_getc(struct exec *, struct data **);
unsigned blt_cx(struct exec *, struct data **);
unsigned blt_getx(struct exec *, struct data **);
unsigned blt_setunit(struct exec *, struct data **);
unsigned blt_getunit(struct exec *, struct data **);
unsigned blt_goto(struct exec *, struct data **);
unsigned blt_getpos(struct exec *, struct data **);
unsigned blt_sel(struct exec *, struct data **);
unsigned blt_getlen(struct exec *, struct data **);
unsigned blt_setq(struct exec *, struct data **);
unsigned blt_getq(struct exec *, struct data **);
unsigned blt_fac(struct exec *, struct data **);
unsigned blt_getfac(struct exec *, struct data **);
unsigned blt_ci(struct exec *, struct data **);
unsigned blt_geti(struct exec *, struct data **);
unsigned blt_ct(struct exec *, struct data **);
unsigned blt_gett(struct exec *, struct data **);
unsigned blt_cf(struct exec *, struct data **);
unsigned blt_getf(struct exec *, struct data **);
unsigned blt_ls(struct exec *, struct data **);
unsigned blt_save(struct exec *, struct data **);
unsigned blt_load(struct exec *, struct data **);
unsigned blt_reset(struct exec *, struct data **);
unsigned blt_export(struct exec *, struct data **);
unsigned blt_import(struct exec *, struct data **);
unsigned blt_idle(struct exec *, struct data **);
unsigned blt_play(struct exec *, struct data **);
unsigned blt_rec(struct exec *, struct data **);
unsigned blt_stop(struct exec *, struct data **);
unsigned blt_tempo(struct exec *, struct data **);
unsigned blt_mins(struct exec *, struct data **);
unsigned blt_mdel(struct exec *, struct data **);
unsigned blt_minfo(struct exec *, struct data **);
unsigned blt_mtempo(struct exec *, struct data **);
unsigned blt_msig(struct exec *, struct data **);
unsigned blt_ctlconf(struct exec *, struct data **);
unsigned blt_ctlconfx(struct exec *, struct data **);
unsigned blt_ctlunconf(struct exec *, struct data **);
unsigned blt_ctlinfo(struct exec *, struct data **);
unsigned blt_metro(struct exec *, struct data **);
unsigned blt_metrocf(struct exec *, struct data **);

unsigned blt_tlist(struct exec *, struct data **);
unsigned blt_tnew(struct exec *, struct data **);
unsigned blt_tdel(struct exec *, struct data **);
unsigned blt_tren(struct exec *, struct data **);
unsigned blt_texists(struct exec *, struct data **);
unsigned blt_taddev(struct exec *, struct data **);
unsigned blt_tsetf(struct exec *, struct data **);
unsigned blt_tgetf(struct exec *, struct data **);
unsigned blt_tcheck(struct exec *, struct data **);

#endif /* MIDISH_BUILTIN_H */
