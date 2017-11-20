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

#ifndef MIDISH_MUX_H
#define MIDISH_MUX_H

#define MUX_STARTWAIT		0	/* waiting for a start event */
#define MUX_START 		1	/* just got a start */
#define MUX_FIRST		2	/* just got the first tic */
#define MUX_NEXT		3	/* just got the next tic */
#define MUX_STOP		4	/* nothing to do */

#define MUX_LINESIZE		1024

struct ev;
struct sysex;

/*
 * modules are chained as follows: mux -> norm -> filt -> song -> output
 * each module calls call-backs of the next module of the chain. In
 * theory we should use function pointers to "connect" modules to
 * each other...
 *
 * But since connection between various modules are hardcoded and not
 * user configurable, we don't use function pointers and other
 * over-engineered stuff. We just call the following call-backs. We
 * save 3 layers of indirection
 */
struct song;
extern struct song *usong;
extern unsigned mux_isopen;
extern unsigned mux_manualstart;
extern unsigned long mux_wallclock;

void song_startcb(struct song *);
void song_stopcb(struct song *);
void song_movecb(struct song *);
void song_evcb(struct song *, struct ev *);
void song_sysexcb(struct song *, struct sysex *);
unsigned song_gotocb(struct song *, unsigned);

struct norm;
void norm_evcb(struct ev *);

struct filt;
void filt_evcb(struct filt *, struct ev *);

/*
 * public functions usable in the rest of the code to send/receive
 * events and to manipulate the clock
 */
void mux_open(void);
void mux_close(void);
void mux_run(void);
void mux_sleep(unsigned);
void mux_flush(void);
void mux_shut(void);
void mux_putev(struct ev *);
void mux_sendraw(unsigned, unsigned char *, unsigned);
unsigned mux_getphase(void);
struct sysex *mux_getsysex(void);
void mux_chgtempo(unsigned long);
void mux_chgticrate(unsigned);
void mux_startreq(int);
void mux_stopreq(void);
void mux_gotoreq(unsigned);
int mux_mdep_wait(int); /* XXX: hide this prototype */

/*
 * call-backs called by midi device drivers
 */
void mux_timercb(unsigned long);
void mux_startcb(void);
void mux_stopcb(void);
void mux_ticcb(void);
void mux_ackcb(unsigned);
void mux_evcb(unsigned, struct ev *);
void mux_sysexcb(unsigned, struct sysex *);
void mux_errorcb(unsigned);

void mux_mtcstart(unsigned);
void mux_mtctick(unsigned);
void mux_mtcstop(void);

#endif /* MIDISH_MUX_H */
