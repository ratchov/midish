/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MIDISH_BUILTIN_H
#define MIDISH_BUILTIN_H

struct exec;
struct data;

unsigned blt_info(struct exec *, struct data **);
unsigned blt_shut(struct exec *, struct data **);
unsigned blt_proclist(struct exec *, struct data **);
unsigned blt_builtinlist(struct exec *, struct data **);

unsigned blt_version(struct exec *, struct data **);
unsigned blt_panic(struct exec *, struct data **);
unsigned blt_debug(struct exec *, struct data **);
unsigned blt_exec(struct exec *, struct data **);
unsigned blt_print(struct exec *, struct data **);
unsigned blt_err(struct exec *, struct data **);
unsigned blt_h(struct exec *, struct data **);

unsigned blt_ev(struct exec *, struct data **);
unsigned blt_ci(struct exec *, struct data **);
unsigned blt_geti(struct exec *, struct data **);
unsigned blt_co(struct exec *, struct data **);
unsigned blt_geto(struct exec *, struct data **);
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
unsigned blt_ct(struct exec *, struct data **);
unsigned blt_gett(struct exec *, struct data **);
unsigned blt_cf(struct exec *, struct data **);
unsigned blt_getf(struct exec *, struct data **);
unsigned blt_mute(struct exec *, struct data **);
unsigned blt_unmute(struct exec *, struct data **);
unsigned blt_getmute(struct exec *, struct data **);
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
unsigned blt_mcut(struct exec *, struct data **);
unsigned blt_mdup(struct exec *, struct data **);
unsigned blt_minfo(struct exec *, struct data **);
unsigned blt_mtempo(struct exec *, struct data **);
unsigned blt_msig(struct exec *, struct data **);
unsigned blt_mend(struct exec *, struct data **);
unsigned blt_ctlconf(struct exec *, struct data **);
unsigned blt_ctlconfx(struct exec *, struct data **);
unsigned blt_ctlunconf(struct exec *, struct data **);
unsigned blt_ctlinfo(struct exec *, struct data **);
unsigned blt_evpat(struct exec *, struct data **);
unsigned blt_evinfo(struct exec *, struct data **);
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
unsigned blt_tcut(struct exec *, struct data **);
unsigned blt_tins(struct exec *, struct data **);
unsigned blt_tclr(struct exec *, struct data **);
unsigned blt_tpaste(struct exec *, struct data **);
unsigned blt_tcopy(struct exec *, struct data **);
unsigned blt_tmerge(struct exec *, struct data **);
unsigned blt_tquant(struct exec *, struct data **);
unsigned blt_ttransp(struct exec *, struct data **);
unsigned blt_tevmap(struct exec *, struct data **);
unsigned blt_tclist(struct exec *, struct data **);
unsigned blt_tinfo(struct exec *, struct data **);

unsigned blt_ilist(struct exec *, struct data **);
unsigned blt_iexists(struct exec *, struct data **);
unsigned blt_inew(struct exec *, struct data **);
unsigned blt_idel(struct exec *, struct data **);
unsigned blt_iren(struct exec *, struct data **);
unsigned blt_iset(struct exec *, struct data **);
unsigned blt_igetc(struct exec *, struct data **);
unsigned blt_igetd(struct exec *, struct data **);
unsigned blt_iaddev(struct exec *, struct data **);
unsigned blt_irmev(struct exec *, struct data **);
unsigned blt_iinfo(struct exec *, struct data **);

unsigned blt_olist(struct exec *, struct data **);
unsigned blt_oexists(struct exec *, struct data **);
unsigned blt_onew(struct exec *, struct data **);
unsigned blt_odel(struct exec *, struct data **);
unsigned blt_oren(struct exec *, struct data **);
unsigned blt_oset(struct exec *, struct data **);
unsigned blt_ogetc(struct exec *, struct data **);
unsigned blt_ogetd(struct exec *, struct data **);
unsigned blt_oaddev(struct exec *, struct data **);
unsigned blt_ormev(struct exec *, struct data **);
unsigned blt_oinfo(struct exec *, struct data **);

unsigned blt_flist(struct exec *, struct data **);
unsigned blt_fnew(struct exec *, struct data **);
unsigned blt_fdel(struct exec *, struct data **);
unsigned blt_fren(struct exec *, struct data **);
unsigned blt_fexists(struct exec *, struct data **);
unsigned blt_finfo(struct exec *, struct data **);
unsigned blt_freset(struct exec *, struct data **);
unsigned blt_fmap(struct exec *, struct data **);
unsigned blt_funmap(struct exec *, struct data **);
unsigned blt_ftransp(struct exec *, struct data **);
unsigned blt_fvcurve(struct exec *, struct data **);
unsigned blt_fchgin(struct exec *, struct data **);
unsigned blt_fchgout(struct exec *, struct data **);
unsigned blt_fswapin(struct exec *, struct data **);
unsigned blt_fswapout(struct exec *, struct data **);

unsigned blt_xlist(struct exec *, struct data **);
unsigned blt_xexists(struct exec *, struct data **);
unsigned blt_xnew(struct exec *, struct data **);
unsigned blt_xdel(struct exec *, struct data **);
unsigned blt_xren(struct exec *, struct data **);
unsigned blt_xinfo(struct exec *, struct data **);
unsigned blt_xrm(struct exec *, struct data **);
unsigned blt_xsetd(struct exec *, struct data **);
unsigned blt_xadd(struct exec *, struct data **);
unsigned blt_ximport(struct exec *, struct data **);
unsigned blt_xexport(struct exec *, struct data **);

unsigned blt_dlist(struct exec *, struct data **);
unsigned blt_dnew(struct exec *, struct data **);
unsigned blt_ddel(struct exec *, struct data **);
unsigned blt_dmtcrx(struct exec *, struct data **);
unsigned blt_dmmctx(struct exec *, struct data **);
unsigned blt_dclkrx(struct exec *, struct data **);
unsigned blt_dclktx(struct exec *, struct data **);
unsigned blt_dclkrate(struct exec *, struct data **);
unsigned blt_dinfo(struct exec *, struct data **);
unsigned blt_dixctl(struct exec *, struct data **);
unsigned blt_doxctl(struct exec *, struct data **);
unsigned blt_diev(struct exec *, struct data **);
unsigned blt_doev(struct exec *, struct data **);

#endif /* MIDISH_BUILTIN_H */
