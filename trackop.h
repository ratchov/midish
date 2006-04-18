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

#ifndef MIDISH_TRACKOP_H
#define MIDISH_TRACKOP_H

struct ev;
struct evspec;
struct track;
struct seqptr;


void track_opcheck(struct track *o);
void track_opquantise(struct track *o, unsigned start, unsigned len, unsigned offset, unsigned quantum, unsigned rate);
void track_opcut(struct track *o, unsigned start, unsigned len);
void track_opinsert(struct track *o, unsigned start, unsigned len);
void track_opblank(struct track *o, unsigned start, unsigned len, struct evspec *es);
void track_opcopy(struct track *o, unsigned start, unsigned len, struct evspec *es, struct track *trag);
void track_opsetchan(struct track *o, unsigned dev, unsigned ch);
void track_optransp(struct track *o, struct seqptr *p, unsigned len, int halftones, struct evspec *es);

unsigned track_opfindtic(struct track *o, unsigned m0);
void track_optimeinfo(struct track *o, unsigned pos, unsigned long *usec24, unsigned *bpm, unsigned *tpb);
void track_opgetmeasure(struct track *o, unsigned pos, unsigned *measure, unsigned *beat, unsigned *tic);
void track_opchaninfo(struct track *o, char *map);
void track_opconfev(struct track *o, struct ev *ev);
#endif /* MIDISH_TRACKOP_H */

