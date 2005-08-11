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

#ifndef MIDISH_DEFAULT_H
#define MIDISH_DEFAULT_H

	/* 
	 * maximum number of midi devices supported by midish 
	 */
#define DEFAULT_MAXNDEVS	16

	/*
	 * maximum number of instruments
	 */
#define DEFAULT_MAXNCHANS	(DEFAULT_MAXNDEVS * 16)

	/*
	 * maximum number of events
	 */
#define DEFAULT_MAXNSEQEVS	100000

	/*
	 * maximum number of tracks
	 */
#define DEFAULT_MAXNTRACKPTRS	100

	/*
	 * maximum number of filter states (roughly
	 * maximum number of simultaneous notes
	 */
#define DEFAULT_MAXNSTATES	1000

	/*
	 * maximum number of system exclusive messages
	 */
#define DEFAULT_MAXNSYSEXS	2000

	/*
	 * maximum number of chunks (each sysex is
	 * a set of chunks)
	 */
#define DEFAULT_MAXNCHUNKS	(DEFAULT_MAXNSYSEXS * 2)

	/*
	 * default number of tics per beat
	 */
#define DEFAULT_TPB		24

	/*
	 * default beats per measure 
	 */
#define DEFAULT_BPM		4

	/*
	 * default number of tic per unit note
	 */
#define DEFAULT_TPU		96

	/* 
	 * default tempo
	 */
#define DEFAULT_TEMPO		120

	/*
	 * number of milliseconds to wait between
	 * the instrumet config is sent and the playback is stared
	 */
#define DEFAULT_CHANWAIT	50

	/*
	 * nmber of milliseconds to wait after 
	 * each sysex message is sent
	 */
#define DEFAULT_SXWAIT		20

	/*
	 * convert tempo (beats per minute)
	 * to tic length (number of 24-th of microsecond)
	 */
#define TEMPO_TO_USEC24(tempo,tpb) (60L * 24000000L / ((tempo) * (tpb)))

#endif /* DEFAULT_EV_H */
