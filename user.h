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

extern struct song *user_song;
extern struct exec *user_exec;
extern struct textout *user_stdout;
extern unsigned user_flag_batch;
extern unsigned user_flag_verb;

unsigned user_mainloop(void);
void user_printstr(char *);
void user_printlong(long);
void user_error(char *);
unsigned user_getopts(int *pargc, char ***pargv);

/* useful convertion functions */

unsigned exec_runfile(struct exec *exec, char *filename);
unsigned exec_runrcfile(struct exec *exec);

unsigned exec_lookuptrack(struct exec *o, char *var, struct songtrk **res);
unsigned exec_lookupchan_getnum(struct exec *o, char *var, unsigned *dev, unsigned *ch);
unsigned exec_lookupchan_getref(struct exec *o, char *var, struct songchan **res);
unsigned exec_lookupfilt(struct exec *o, char *var, struct songfilt **res);
unsigned exec_lookupsx(struct exec *o, char *var, struct songsx **res);
unsigned exec_lookupev(struct exec *o, char *name, struct ev *ev);
unsigned exec_lookupevspec(struct exec *o, char *name, struct evspec *e);
unsigned exec_lookupctl(struct exec *o, char *var, unsigned *num);
unsigned exec_lookupval(struct exec *o, char *n, unsigned isfine, unsigned *r);


void	 data_print(struct data *d);
unsigned data_num2chan(struct data *o, unsigned *dev, unsigned *ch);
unsigned data_list2chan(struct data *o, unsigned *dev, unsigned *ch);
unsigned data_list2range(struct data *d, unsigned min, unsigned max, unsigned *lo, unsigned *hi);
unsigned data_matchsysex(struct data *d, struct sysex *sx, unsigned *res);
unsigned data_list2ctl(struct data *d, unsigned *num);
unsigned data_list2ctlset(struct data *d, unsigned *res);

/* track functions */

unsigned user_func_tracklist(struct exec *o, struct data **r);
unsigned user_func_tracknew(struct exec *o, struct data **r);
unsigned user_func_trackdelete(struct exec *o, struct data **r);
unsigned user_func_trackrename(struct exec *o, struct data **r);
unsigned user_func_trackexists(struct exec *o, struct data **r);
unsigned user_func_trackinfo(struct exec *o, struct data **r);
unsigned user_func_trackaddev(struct exec *o, struct data **r);
unsigned user_func_tracksetcurfilt(struct exec *o, struct data **r);
unsigned user_func_trackgetcurfilt(struct exec *o, struct data **r);
unsigned user_func_trackcheck(struct exec *o, struct data **r);
unsigned user_func_trackcut(struct exec *o, struct data **r);
unsigned user_func_trackblank(struct exec *o, struct data **r);
unsigned user_func_trackcopy(struct exec *o, struct data **r);
unsigned user_func_trackinsert(struct exec *o, struct data **r);
unsigned user_func_trackmerge(struct exec *o, struct data **r);
unsigned user_func_trackquant(struct exec *o, struct data **r);
unsigned user_func_tracktransp(struct exec *o, struct data **r);
unsigned user_func_tracksetmute(struct exec *o, struct data **r);
unsigned user_func_trackgetmute(struct exec *o, struct data **r);
unsigned user_func_trackchanlist(struct exec *o, struct data **r);

/* chan functions */

unsigned user_func_chanlist(struct exec *o, struct data **r);
unsigned user_func_channew(struct exec *o, struct data **r);
unsigned user_func_chanset(struct exec *o, struct data **r);
unsigned user_func_chandelete(struct exec *o, struct data **r);
unsigned user_func_chanrename(struct exec *o, struct data **r);
unsigned user_func_chanexists(struct exec *o, struct data **r);
unsigned user_func_changetch(struct exec *o, struct data **r);
unsigned user_func_changetdev(struct exec *o, struct data **r);
unsigned user_func_chanconfev(struct exec *o, struct data **r);
unsigned user_func_chaninfo(struct exec *o, struct data **r);
unsigned user_func_changetcurinput(struct exec *o, struct data **r);
unsigned user_func_chansetcurinput(struct exec *o, struct data **r);
 
/* sysex */

unsigned user_func_sysexlist(struct exec *o, struct data **r);
unsigned user_func_sysexnew(struct exec *o, struct data **r);
unsigned user_func_sysexdelete(struct exec *o, struct data **r);
unsigned user_func_sysexrename(struct exec *o, struct data **r);
unsigned user_func_sysexexists(struct exec *o, struct data **r);
unsigned user_func_sysexinfo(struct exec *o, struct data **r);
unsigned user_func_sysexclear(struct exec *o, struct data **r);
unsigned user_func_sysexsetunit(struct exec *o, struct data **r);
unsigned user_func_sysexadd(struct exec *o, struct data **r);

/* filt functions  */

unsigned user_func_filtlist(struct exec *o, struct data **r);
unsigned user_func_filtnew(struct exec *o, struct data **r);
unsigned user_func_filtdelete(struct exec *o, struct data **r);
unsigned user_func_filtrename(struct exec *o, struct data **r);
unsigned user_func_filtexists(struct exec *o, struct data **r);
unsigned user_func_filtinfo(struct exec *o, struct data **r);
unsigned user_func_filtdevdrop(struct exec *o, struct data **r);
unsigned user_func_filtnodevdrop(struct exec *o, struct data **r);
unsigned user_func_filtdevmap(struct exec *o, struct data **r);
unsigned user_func_filtnodevmap(struct exec *o, struct data **r);
unsigned user_func_filtchandrop(struct exec *o, struct data **r);
unsigned user_func_filtnochandrop(struct exec *o, struct data **r);
unsigned user_func_filtchanmap(struct exec *o, struct data **r);
unsigned user_func_filtnochanmap(struct exec *o, struct data **r);
unsigned user_func_filtctldrop(struct exec *o, struct data **r);
unsigned user_func_filtnoctldrop(struct exec *o, struct data **r);
unsigned user_func_filtctlmap(struct exec *o, struct data **r);
unsigned user_func_filtnoctlmap(struct exec *o, struct data **r);
unsigned user_func_filtkeydrop(struct exec *o, struct data **r);
unsigned user_func_filtnokeydrop(struct exec *o, struct data **r);
unsigned user_func_filtkeymap(struct exec *o, struct data **r);
unsigned user_func_filtnokeymap(struct exec *o, struct data **r);
unsigned user_func_filtreset(struct exec *o, struct data **r);
unsigned user_func_filtchgich(struct exec *o, struct data **r);
unsigned user_func_filtchgidev(struct exec *o, struct data **r);
unsigned user_func_filtswapich(struct exec *o, struct data **r);
unsigned user_func_filtswapidev(struct exec *o, struct data **r);
unsigned user_func_filtchgoch(struct exec *o, struct data **r);
unsigned user_func_filtchgodev(struct exec *o, struct data **r);
unsigned user_func_filtswapoch(struct exec *o, struct data **r);
unsigned user_func_filtswapodev(struct exec *o, struct data **r);
unsigned user_func_filtsetcurchan(struct exec *o, struct data **r);
unsigned user_func_filtgetcurchan(struct exec *o, struct data **r);

/* song functions  */

unsigned user_func_songsetunit(struct exec *o, struct data **r);
unsigned user_func_songgetunit(struct exec *o, struct data **r);
unsigned user_func_songsetfactor(struct exec *o, struct data **r);
unsigned user_func_songgetfactor(struct exec *o, struct data **r);
unsigned user_func_songsetcurpos(struct exec *o, struct data **r);
unsigned user_func_songgetcurpos(struct exec *o, struct data **r);
unsigned user_func_songsetcurlen(struct exec *o, struct data **r);
unsigned user_func_songgetcurlen(struct exec *o, struct data **r);
unsigned user_func_songsetcurquant(struct exec *o, struct data **r);
unsigned user_func_songgetcurquant(struct exec *o, struct data **r);
unsigned user_func_songsetcurtrack(struct exec *o, struct data **r);
unsigned user_func_songgetcurtrack(struct exec *o, struct data **r);
unsigned user_func_songsetcurfilt(struct exec *o, struct data **r);
unsigned user_func_songgetcurfilt(struct exec *o, struct data **r);
unsigned user_func_songinfo(struct exec *o, struct data **r);
unsigned user_func_songsave(struct exec *o, struct data **r);
unsigned user_func_songload(struct exec *o, struct data **r);
unsigned user_func_songreset(struct exec *o, struct data **r);
unsigned user_func_songexportsmf(struct exec *o, struct data **r);
unsigned user_func_songimportsmf(struct exec *o, struct data **r);
unsigned user_func_songidle(struct exec *o, struct data **r);
unsigned user_func_songplay(struct exec *o, struct data **r);
unsigned user_func_songrecord(struct exec *o, struct data **r);
unsigned user_func_songsettempo(struct exec *o, struct data **r);
unsigned user_func_songtimeins(struct exec *o, struct data **r);
unsigned user_func_songtimerm(struct exec *o, struct data **r);
unsigned user_func_songtimeinfo(struct exec *o, struct data **r);
unsigned user_func_songsetcursysex(struct exec *o, struct data **r);
unsigned user_func_songgetcursysex(struct exec *o, struct data **r);
unsigned user_func_songsetcurchan(struct exec *o, struct data **r);
unsigned user_func_songgetcurchan(struct exec *o, struct data **r);
unsigned user_func_songsetcurinput(struct exec *o, struct data **r);
unsigned user_func_songgetcurinput(struct exec *o, struct data **r);

/* device functions */

unsigned user_func_devlist(struct exec *o, struct data **r);
unsigned user_func_devattach(struct exec *o, struct data **r);
unsigned user_func_devdetach(struct exec *o, struct data **r);
unsigned user_func_devsetmaster(struct exec *o, struct data **r);
unsigned user_func_devgetmaster(struct exec *o, struct data **r);
unsigned user_func_devsendrt(struct exec *o, struct data **r);
unsigned user_func_devticrate(struct exec *o, struct data **r);
unsigned user_func_devinfo(struct exec *o, struct data **r);
unsigned user_func_devixctl(struct exec *o, struct data **r);
unsigned user_func_devoxctl(struct exec *o, struct data **r);
 
/* misc */

unsigned user_func_metroswitch(struct exec *o, struct data **r);
unsigned user_func_metroconf(struct exec *o, struct data **r);
unsigned user_func_shut(struct exec *o, struct data **r);
unsigned user_func_sendraw(struct exec *o, struct data **r);
unsigned user_func_panic(struct exec *o, struct data **r);
unsigned user_func_debug(struct exec *o, struct data **r);
unsigned user_func_exec(struct exec *o, struct data **r);
unsigned user_func_print(struct exec *o, struct data **r);
unsigned user_func_info(struct exec *o, struct data **r);

unsigned user_func_ctlconf(struct exec *o, struct data **r);
unsigned user_func_ctlconfx(struct exec *o, struct data **r);
unsigned user_func_ctlunconf(struct exec *o, struct data **r);
unsigned user_func_ctlinfo(struct exec *o, struct data **r);

#endif /* MIDISH_USER_H */
