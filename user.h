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

#ifndef MIDISH_USER_H
#define MIDISH_USER_H

struct song;
struct songtrk;
struct songchan;
struct songfilt;
struct songsx;

struct exec;
struct data;
struct ev;
struct evspec;
struct sysex;

extern struct song *usong;
extern unsigned user_flag_batch;
extern unsigned user_flag_verb;

unsigned user_mainloop(void);
void user_printstr(char *);
void user_printlong(long);
void user_error(char *);

unsigned user_getopts(int *, char ***);

/* useful conversion functions */

unsigned exec_runfile(struct exec *, char *);
unsigned exec_runrcfile(struct exec *);

unsigned exec_lookuptrack(struct exec *, char *, struct songtrk **);
unsigned exec_lookupchan_getnum(struct exec *, char *,
    unsigned *, unsigned *, int);
unsigned exec_lookupchan_getref(struct exec *, char *,
    struct songchan **, int);
unsigned exec_lookupfilt(struct exec *, char *, struct songfilt **);
unsigned exec_lookupsx(struct exec *, char *, struct songsx **);
unsigned exec_lookupev(struct exec *, char *, struct ev *, int);
unsigned exec_lookupevspec(struct exec *, char *, struct evspec *, int);
unsigned exec_lookupctl(struct exec *, char *, unsigned *);
unsigned exec_lookupval(struct exec *, char *, unsigned, unsigned *);


void data_print(struct data *);
unsigned data_num2chan(struct data *, unsigned *, unsigned *);
unsigned data_list2chan(struct data *, unsigned *, unsigned *, int);
unsigned data_list2range(struct data *, unsigned, unsigned, unsigned *, unsigned *);
unsigned data_matchsysex(struct data *, struct sysex *, unsigned *);
unsigned data_list2ctl(struct data *, unsigned *);
unsigned data_list2ctlset(struct data *, unsigned *);
unsigned data_getctl(struct data *, unsigned *);

/* track functions */

unsigned user_func_tracklist(struct exec *, struct data **);
unsigned user_func_tracknew(struct exec *, struct data **);
unsigned user_func_trackdelete(struct exec *, struct data **);
unsigned user_func_trackrename(struct exec *, struct data **);
unsigned user_func_trackexists(struct exec *, struct data **);
unsigned user_func_trackinfo(struct exec *, struct data **);
unsigned user_func_trackaddev(struct exec *, struct data **);
unsigned user_func_tracksetcurfilt(struct exec *, struct data **);
unsigned user_func_trackgetcurfilt(struct exec *, struct data **);
unsigned user_func_trackcheck(struct exec *, struct data **);
unsigned user_func_trackcut(struct exec *, struct data **);
unsigned user_func_trackblank(struct exec *, struct data **);
unsigned user_func_trackcopy(struct exec *, struct data **);
unsigned user_func_trackinsert(struct exec *, struct data **);
unsigned user_func_trackmerge(struct exec *, struct data **);
unsigned user_func_trackquant(struct exec *, struct data **);
unsigned user_func_tracktransp(struct exec *, struct data **);
unsigned user_func_tracksetmute(struct exec *, struct data **);
unsigned user_func_trackgetmute(struct exec *, struct data **);
unsigned user_func_trackchanlist(struct exec *, struct data **);

/* chan functions */

unsigned user_func_chanlist(struct exec *, struct data **);
unsigned user_func_channew(struct exec *, struct data **);
unsigned user_func_chanset(struct exec *, struct data **);
unsigned user_func_chandelete(struct exec *, struct data **);
unsigned user_func_chanrename(struct exec *, struct data **);
unsigned user_func_chanexists(struct exec *, struct data **);
unsigned user_func_changetch(struct exec *, struct data **);
unsigned user_func_changetdev(struct exec *, struct data **);
unsigned user_func_chanconfev(struct exec *, struct data **);
unsigned user_func_chanunconfev(struct exec *, struct data **);
unsigned user_func_chaninfo(struct exec *, struct data **);
unsigned user_func_changetcurinput(struct exec *, struct data **);
unsigned user_func_chansetcurinput(struct exec *, struct data **);

/* sysex */

unsigned user_func_sysexlist(struct exec *, struct data **);
unsigned user_func_sysexnew(struct exec *, struct data **);
unsigned user_func_sysexdelete(struct exec *, struct data **);
unsigned user_func_sysexrename(struct exec *, struct data **);
unsigned user_func_sysexexists(struct exec *, struct data **);
unsigned user_func_sysexinfo(struct exec *, struct data **);
unsigned user_func_sysexclear(struct exec *, struct data **);
unsigned user_func_sysexsetunit(struct exec *, struct data **);
unsigned user_func_sysexadd(struct exec *, struct data **);

/* filt functions  */

unsigned user_func_filtlist(struct exec *, struct data **);
unsigned user_func_filtnew(struct exec *, struct data **);
unsigned user_func_filtdelete(struct exec *, struct data **);
unsigned user_func_filtrename(struct exec *, struct data **);
unsigned user_func_filtexists(struct exec *, struct data **);
unsigned user_func_filtinfo(struct exec *, struct data **);
unsigned user_func_filtdevdrop(struct exec *, struct data **);
unsigned user_func_filtnodevdrop(struct exec *, struct data **);
unsigned user_func_filtdevmap(struct exec *, struct data **);
unsigned user_func_filtnodevmap(struct exec *, struct data **);
unsigned user_func_filtchandrop(struct exec *, struct data **);
unsigned user_func_filtnochandrop(struct exec *, struct data **);
unsigned user_func_filtchanmap(struct exec *, struct data **);
unsigned user_func_filtnochanmap(struct exec *, struct data **);
unsigned user_func_filtctldrop(struct exec *, struct data **);
unsigned user_func_filtnoctldrop(struct exec *, struct data **);
unsigned user_func_filtctlmap(struct exec *, struct data **);
unsigned user_func_filtnoctlmap(struct exec *, struct data **);
unsigned user_func_filtkeydrop(struct exec *, struct data **);
unsigned user_func_filtnokeydrop(struct exec *, struct data **);
unsigned user_func_filtkeymap(struct exec *, struct data **);
unsigned user_func_filtnokeymap(struct exec *, struct data **);
unsigned user_func_filtreset(struct exec *, struct data **);
unsigned user_func_filtchgich(struct exec *, struct data **);
unsigned user_func_filtchgidev(struct exec *, struct data **);
unsigned user_func_filtswapich(struct exec *, struct data **);
unsigned user_func_filtswapidev(struct exec *, struct data **);
unsigned user_func_filtchgoch(struct exec *, struct data **);
unsigned user_func_filtchgodev(struct exec *, struct data **);
unsigned user_func_filtswapoch(struct exec *, struct data **);
unsigned user_func_filtswapodev(struct exec *, struct data **);
unsigned user_func_filtevmap(struct exec *, struct data **);
unsigned user_func_filtevunmap(struct exec *, struct data **);
unsigned user_func_filtchgin(struct exec *, struct data **);
unsigned user_func_filtchgout(struct exec *, struct data **);
unsigned user_func_filtswapgin(struct exec *, struct data **);
unsigned user_func_filtswapout(struct exec *, struct data **);
unsigned user_func_filtsetcurchan(struct exec *, struct data **);
unsigned user_func_filtgetcurchan(struct exec *, struct data **);

/* song functions  */

unsigned user_func_songsetunit(struct exec *, struct data **);
unsigned user_func_songgetunit(struct exec *, struct data **);
unsigned user_func_songsetfactor(struct exec *, struct data **);
unsigned user_func_songgetfactor(struct exec *, struct data **);
unsigned user_func_songsetcurpos(struct exec *, struct data **);
unsigned user_func_songgetcurpos(struct exec *, struct data **);
unsigned user_func_songsetcurlen(struct exec *, struct data **);
unsigned user_func_songgetcurlen(struct exec *, struct data **);
unsigned user_func_songsetcurquant(struct exec *, struct data **);
unsigned user_func_songgetcurquant(struct exec *, struct data **);
unsigned user_func_songsetcurtrack(struct exec *, struct data **);
unsigned user_func_songgetcurtrack(struct exec *, struct data **);
unsigned user_func_songsetcurfilt(struct exec *, struct data **);
unsigned user_func_songgetcurfilt(struct exec *, struct data **);
unsigned user_func_songinfo(struct exec *, struct data **);
unsigned user_func_songsave(struct exec *, struct data **);
unsigned user_func_songload(struct exec *, struct data **);
unsigned user_func_songreset(struct exec *, struct data **);
unsigned user_func_songexportsmf(struct exec *, struct data **);
unsigned user_func_songimportsmf(struct exec *, struct data **);
unsigned user_func_songidle(struct exec *, struct data **);
unsigned user_func_songplay(struct exec *, struct data **);
unsigned user_func_songrecord(struct exec *, struct data **);
unsigned user_func_songstop(struct exec *, struct data **);
unsigned user_func_songsettempo(struct exec *, struct data **);
unsigned user_func_songtimeins(struct exec *, struct data **);
unsigned user_func_songtimerm(struct exec *, struct data **);
unsigned user_func_songtimeinfo(struct exec *, struct data **);
unsigned user_func_songgettempo(struct exec *, struct data **);
unsigned user_func_songgetsign(struct exec *, struct data **);
unsigned user_func_songsetcursysex(struct exec *, struct data **);
unsigned user_func_songgetcursysex(struct exec *, struct data **);
unsigned user_func_songsetcurchan(struct exec *, struct data **);
unsigned user_func_songgetcurchan(struct exec *, struct data **);
unsigned user_func_songsetcurinput(struct exec *, struct data **);
unsigned user_func_songgetcurinput(struct exec *, struct data **);

/* device functions */

unsigned user_func_devlist(struct exec *, struct data **);
unsigned user_func_devattach(struct exec *, struct data **);
unsigned user_func_devdetach(struct exec *, struct data **);
unsigned user_func_devsetmaster(struct exec *, struct data **);
unsigned user_func_devgetmaster(struct exec *, struct data **);
unsigned user_func_devsendrt(struct exec *, struct data **);
unsigned user_func_devticrate(struct exec *, struct data **);
unsigned user_func_devinfo(struct exec *, struct data **);
unsigned user_func_devixctl(struct exec *, struct data **);
unsigned user_func_devoxctl(struct exec *, struct data **);

/* misc */

unsigned user_func_metroswitch(struct exec *, struct data **);
unsigned user_func_metroconf(struct exec *, struct data **);
unsigned user_func_shut(struct exec *, struct data **);
unsigned user_func_sendraw(struct exec *, struct data **);
unsigned user_func_panic(struct exec *, struct data **);
unsigned user_func_debug(struct exec *, struct data **);
unsigned user_func_exec(struct exec *, struct data **);
unsigned user_func_print(struct exec *, struct data **);
unsigned user_func_info(struct exec *, struct data **);
unsigned user_func_proclist(struct exec *, struct data **);
unsigned user_func_builtinlist(struct exec *, struct data **);

unsigned user_func_ctlconf(struct exec *, struct data **);
unsigned user_func_ctlconfx(struct exec *, struct data **);
unsigned user_func_ctlunconf(struct exec *, struct data **);
unsigned user_func_ctlinfo(struct exec *, struct data **);

#endif /* MIDISH_USER_H */
