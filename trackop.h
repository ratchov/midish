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

#ifndef MIDISH_TRACKOP_H
#define MIDISH_TRACKOP_H

struct ev_s;
struct evspec_s;
struct track_s;
struct seqptr_s;

void track_frameget(struct track_s *o, struct seqptr_s *p, struct track_s *frame);
void track_frameput(struct track_s *o, struct seqptr_s *p, struct track_s *frame);
unsigned track_framefind(struct track_s *o, struct seqptr_s *p);
void track_frameuniq(struct track_s *o, struct seqptr_s *p, struct track_s *frame);
void track_framecp(struct track_s *s, struct track_s *d);
unsigned track_framematch(struct track_s *s, struct evspec_s *e);
void track_frametransp(struct track_s *o, int halftones);

void track_opcheck(struct track_s *o);
void track_opquantise(struct track_s *o, struct seqptr_s *, unsigned first, unsigned len, unsigned quantum, unsigned rate);
void track_opextract(struct track_s *o, struct seqptr_s *, unsigned len, struct track_s *targ, struct evspec_s *es);
void track_opcopy(struct track_s *o, struct seqptr_s *p, unsigned len, struct track_s *targ);
void track_opcut(struct track_s *o, unsigned start, unsigned len);
void track_opinsert(struct track_s *o, struct seqptr_s *, unsigned len);
void track_opsetchan(struct track_s *o, unsigned dev, unsigned ch);
void track_optransp(struct track_s *o, struct seqptr_s *p, unsigned len, int halftones, struct evspec_s *es);

unsigned track_opfindtic(struct track_s *o, unsigned m0);
void track_optimeinfo(struct track_s *o, unsigned pos, unsigned long *usec24, unsigned *bpm, unsigned *tpb);
void track_opgetmeasure(struct track_s *o, unsigned pos, unsigned *measure, unsigned *beat, unsigned *tic);
void track_opchaninfo(struct track_s *o, char *map);
void track_opconfev(struct track_s *o, struct ev_s *ev);
#endif /* MIDISH_TRACKOP_H */

