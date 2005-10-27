/*
 * Copyright (c) 2003-2005 Alexandre Ratchov
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

struct song_s;
struct songtrk_s;
struct songchan_s;
struct songfilt_s;
struct songsx_s;

struct exec_s;
struct data_s;
struct ev_s;
struct evspec_s;
struct sysex_s;

extern struct song_s *user_song;
extern struct exec_s *user_exec;
extern struct textout_s *user_stdout;
extern unsigned user_flag_batch;
extern unsigned user_flag_verb;

void user_mainloop(void);
void user_printstr(char *);
void user_printlong(long);
void user_error(char *);
unsigned user_getopts(int *pargc, char ***pargv);

/* useful convertion functions */

unsigned exec_runfile(struct exec_s *exec, char *filename);
unsigned exec_runrcfile(struct exec_s *exec);

unsigned exec_lookuptrack(struct exec_s *o, char *var, struct songtrk_s **res);
unsigned exec_lookupchan_getnum(struct exec_s *o, char *var, unsigned *dev, unsigned *ch);
unsigned exec_lookupchan_getref(struct exec_s *o, char *var, struct songchan_s **res);
unsigned exec_lookupfilt(struct exec_s *o, char *var, struct songfilt_s **res);
unsigned exec_lookupsx(struct exec_s *o, char *var, struct songsx_s **res);
unsigned exec_lookupev(struct exec_s *o, char *name, struct ev_s *ev);
unsigned exec_lookupevspec(struct exec_s *o, char *name, struct evspec_s *e);

void	 data_print(struct data_s *d);
unsigned data_num2chan(struct data_s *o, unsigned *dev, unsigned *ch);
unsigned data_list2chan(struct data_s *o, unsigned *dev, unsigned *ch);
unsigned data_list2range(struct data_s *d, unsigned min, unsigned max, unsigned *lo, unsigned *hi);
unsigned data_matchsysex(struct data_s *d, struct sysex_s *sx, unsigned *res);

/* track functions */

unsigned user_func_tracklist(struct exec_s *o, struct data_s **r);
unsigned user_func_tracknew(struct exec_s *o, struct data_s **r);
unsigned user_func_trackdelete(struct exec_s *o, struct data_s **r);
unsigned user_func_trackrename(struct exec_s *o, struct data_s **r);
unsigned user_func_trackexists(struct exec_s *o, struct data_s **r);
unsigned user_func_trackinfo(struct exec_s *o, struct data_s **r);
unsigned user_func_trackaddev(struct exec_s *o, struct data_s **r);
unsigned user_func_tracksetcurfilt(struct exec_s *o, struct data_s **r);
unsigned user_func_trackgetcurfilt(struct exec_s *o, struct data_s **r);
unsigned user_func_trackcheck(struct exec_s *o, struct data_s **r);
unsigned user_func_trackcut(struct exec_s *o, struct data_s **r);
unsigned user_func_trackblank(struct exec_s *o, struct data_s **r);
unsigned user_func_trackcopy(struct exec_s *o, struct data_s **r);
unsigned user_func_trackinsert(struct exec_s *o, struct data_s **r);
unsigned user_func_trackquant(struct exec_s *o, struct data_s **r);
unsigned user_func_tracksetmute(struct exec_s *o, struct data_s **r);
unsigned user_func_trackgetmute(struct exec_s *o, struct data_s **r);
unsigned user_func_trackchanlist(struct exec_s *o, struct data_s **r);

/* chan functions */

unsigned user_func_chanlist(struct exec_s *o, struct data_s **r);
unsigned user_func_channew(struct exec_s *o, struct data_s **r);
unsigned user_func_chanset(struct exec_s *o, struct data_s **r);
unsigned user_func_chandelete(struct exec_s *o, struct data_s **r);
unsigned user_func_chanrename(struct exec_s *o, struct data_s **r);
unsigned user_func_chanexists(struct exec_s *o, struct data_s **r);
unsigned user_func_changetch(struct exec_s *o, struct data_s **r);
unsigned user_func_changetdev(struct exec_s *o, struct data_s **r);
unsigned user_func_chanconfev(struct exec_s *o, struct data_s **r);
unsigned user_func_chaninfo(struct exec_s *o, struct data_s **r);
unsigned user_func_changetcurinput(struct exec_s *o, struct data_s **r);
unsigned user_func_chansetcurinput(struct exec_s *o, struct data_s **r);
 
/* sysex */

unsigned user_func_sysexlist(struct exec_s *o, struct data_s **r);
unsigned user_func_sysexnew(struct exec_s *o, struct data_s **r);
unsigned user_func_sysexdelete(struct exec_s *o, struct data_s **r);
unsigned user_func_sysexrename(struct exec_s *o, struct data_s **r);
unsigned user_func_sysexexists(struct exec_s *o, struct data_s **r);
unsigned user_func_sysexinfo(struct exec_s *o, struct data_s **r);
unsigned user_func_sysexclear(struct exec_s *o, struct data_s **r);
unsigned user_func_sysexsetunit(struct exec_s *o, struct data_s **r);
unsigned user_func_sysexadd(struct exec_s *o, struct data_s **r);

/* filt functions  */

unsigned user_func_filtlist(struct exec_s *o, struct data_s **r);
unsigned user_func_filtnew(struct exec_s *o, struct data_s **r);
unsigned user_func_filtdelete(struct exec_s *o, struct data_s **r);
unsigned user_func_filtrename(struct exec_s *o, struct data_s **r);
unsigned user_func_filtexists(struct exec_s *o, struct data_s **r);
unsigned user_func_filtinfo(struct exec_s *o, struct data_s **r);
unsigned user_func_filtdevdrop(struct exec_s *o, struct data_s **r);
unsigned user_func_filtnodevdrop(struct exec_s *o, struct data_s **r);
unsigned user_func_filtdevmap(struct exec_s *o, struct data_s **r);
unsigned user_func_filtnodevmap(struct exec_s *o, struct data_s **r);
unsigned user_func_filtchandrop(struct exec_s *o, struct data_s **r);
unsigned user_func_filtnochandrop(struct exec_s *o, struct data_s **r);
unsigned user_func_filtchanmap(struct exec_s *o, struct data_s **r);
unsigned user_func_filtnochanmap(struct exec_s *o, struct data_s **r);
unsigned user_func_filtctldrop(struct exec_s *o, struct data_s **r);
unsigned user_func_filtnoctldrop(struct exec_s *o, struct data_s **r);
unsigned user_func_filtctlmap(struct exec_s *o, struct data_s **r);
unsigned user_func_filtnoctlmap(struct exec_s *o, struct data_s **r);
unsigned user_func_filtkeydrop(struct exec_s *o, struct data_s **r);
unsigned user_func_filtnokeydrop(struct exec_s *o, struct data_s **r);
unsigned user_func_filtkeymap(struct exec_s *o, struct data_s **r);
unsigned user_func_filtnokeymap(struct exec_s *o, struct data_s **r);
unsigned user_func_filtreset(struct exec_s *o, struct data_s **r);
unsigned user_func_filtchgich(struct exec_s *o, struct data_s **r);
unsigned user_func_filtchgidev(struct exec_s *o, struct data_s **r);
unsigned user_func_filtswapich(struct exec_s *o, struct data_s **r);
unsigned user_func_filtswapidev(struct exec_s *o, struct data_s **r);
unsigned user_func_filtchgoch(struct exec_s *o, struct data_s **r);
unsigned user_func_filtchgodev(struct exec_s *o, struct data_s **r);
unsigned user_func_filtswapoch(struct exec_s *o, struct data_s **r);
unsigned user_func_filtswapodev(struct exec_s *o, struct data_s **r);
unsigned user_func_filtsetcurchan(struct exec_s *o, struct data_s **r);
unsigned user_func_filtgetcurchan(struct exec_s *o, struct data_s **r);

/* song functions  */

unsigned user_func_songsetunit(struct exec_s *o, struct data_s **r);
unsigned user_func_songgetunit(struct exec_s *o, struct data_s **r);
unsigned user_func_songsetcurpos(struct exec_s *o, struct data_s **r);
unsigned user_func_songgetcurpos(struct exec_s *o, struct data_s **r);
unsigned user_func_songsetcurlen(struct exec_s *o, struct data_s **r);
unsigned user_func_songgetcurlen(struct exec_s *o, struct data_s **r);
unsigned user_func_songsetcurquant(struct exec_s *o, struct data_s **r);
unsigned user_func_songgetcurquant(struct exec_s *o, struct data_s **r);
unsigned user_func_songsetcurtrack(struct exec_s *o, struct data_s **r);
unsigned user_func_songgetcurtrack(struct exec_s *o, struct data_s **r);
unsigned user_func_songsetcurfilt(struct exec_s *o, struct data_s **r);
unsigned user_func_songgetcurfilt(struct exec_s *o, struct data_s **r);
unsigned user_func_songinfo(struct exec_s *o, struct data_s **r);
unsigned user_func_songsave(struct exec_s *o, struct data_s **r);
unsigned user_func_songload(struct exec_s *o, struct data_s **r);
unsigned user_func_songreset(struct exec_s *o, struct data_s **r);
unsigned user_func_songexportsmf(struct exec_s *o, struct data_s **r);
unsigned user_func_songimportsmf(struct exec_s *o, struct data_s **r);
unsigned user_func_songidle(struct exec_s *o, struct data_s **r);
unsigned user_func_songplay(struct exec_s *o, struct data_s **r);
unsigned user_func_songrecord(struct exec_s *o, struct data_s **r);
unsigned user_func_songsettempo(struct exec_s *o, struct data_s **r);
unsigned user_func_songtimeins(struct exec_s *o, struct data_s **r);
unsigned user_func_songtimerm(struct exec_s *o, struct data_s **r);
unsigned user_func_songtimeinfo(struct exec_s *o, struct data_s **r);
unsigned user_func_songsetcursysex(struct exec_s *o, struct data_s **r);
unsigned user_func_songgetcursysex(struct exec_s *o, struct data_s **r);
unsigned user_func_songsetcurchan(struct exec_s *o, struct data_s **r);
unsigned user_func_songgetcurchan(struct exec_s *o, struct data_s **r);
unsigned user_func_songsetcurinput(struct exec_s *o, struct data_s **r);
unsigned user_func_songgetcurinput(struct exec_s *o, struct data_s **r);

/* device functions */

unsigned user_func_devlist(struct exec_s *o, struct data_s **r);
unsigned user_func_devattach(struct exec_s *o, struct data_s **r);
unsigned user_func_devdetach(struct exec_s *o, struct data_s **r);
unsigned user_func_devsetmaster(struct exec_s *o, struct data_s **r);
unsigned user_func_devgetmaster(struct exec_s *o, struct data_s **r);
unsigned user_func_devsendrt(struct exec_s *o, struct data_s **r);
unsigned user_func_devticrate(struct exec_s *o, struct data_s **r);
unsigned user_func_devinfo(struct exec_s *o, struct data_s **r);
 
/* misc */

unsigned user_func_metroswitch(struct exec_s *o, struct data_s **r);
unsigned user_func_metroconf(struct exec_s *o, struct data_s **r);
unsigned user_func_shut(struct exec_s *o, struct data_s **r);
unsigned user_func_sendraw(struct exec_s *o, struct data_s **r);
unsigned user_func_panic(struct exec_s *o, struct data_s **r);
unsigned user_func_debug(struct exec_s *o, struct data_s **r);
unsigned user_func_exec(struct exec_s *o, struct data_s **r);
unsigned user_func_print(struct exec_s *o, struct data_s **r);
unsigned user_func_info(struct exec_s *o, struct data_s **r);

#endif /* MIDISH_USER_H */
