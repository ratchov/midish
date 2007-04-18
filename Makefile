#
# parameters for the GNU readline(3) library, used to
# build rmidish (front-end to midish)
#
READLINE_CFLAGS = 			# additionnal compiler flags
READLINE_LDFLAGS = -L/usr/local/lib	# path to libraries
READLINE_INCLUDE = -I/usr/local/include # path to header files
READLINE_LIB = -lreadline -ltermcap	# list of readline libraries

#
# binaries, documentation, man pages and examples will be installed in 
# ${BIN_DIR}, ${MAN1_DIR}, ${DOC_DIR} and ${EXAMPLES_DIR}
#
PREFIX = /usr/local
BIN_DIR = ${PREFIX}/bin
MAN1_DIR = ${PREFIX}/man/man1
DOC_DIR = ${PREFIX}/share/doc/midish
EXAMPLES_DIR = ${PREFIX}/share/examples/midish

PROGS = midish rmidish mkcurves

all:		midish rmidish

install:	install-midish install-rmidish

clean:
		rm -f -- ${PROGS} ${MIDISH_OBJS} ${RMIDISH_OBJS} \
		*~ *.bak *.tmp *.ln *.s *.out *.core core

# ---------------------------------------------- dependencies for midish ---

MIDISH_OBJS = \
cons.o data.o dbg.o ev.o exec.o filt.o frame.o lex.o main.o mdep.o metro.o \
mididev.o mux.o name.o node.o parse.o pool.o rmidi.o saveload.o smf.o \
song.o state.o str.o sysex.o textio.o timo.o track.o user.o \
user_trk.o user_chan.o user_filt.o user_sx.o user_song.o user_dev.o 

midish:		${MIDISH_OBJS}
		${CC} ${LDFLAGS} ${MIDISH_OBJS} -o midish

install-midish:	midish smfplay smfrec midish.1 smfplay.1 smfrec.1 \
		README manual.html midishrc sample.sng
		mkdir -p ${BIN_DIR} ${MAN1_DIR} ${DOC_DIR} ${EXAMPLES_DIR}
		cp midish smfplay smfrec ${BIN_DIR}
		cp midish.1 smfplay.1 smfrec.1 ${MAN1_DIR}
		cp README manual.html ${DOC_DIR}
		cp midishrc sample.sng ${EXAMPLES_DIR}

cons.o:		cons.c dbg.h textio.h cons.h user.h
data.o:		data.c dbg.h str.h cons.h data.h
dbg.o:		dbg.c dbg.h
ev.o:		ev.c dbg.h ev.h default.h str.h
exec.o:		exec.c dbg.h exec.h name.h str.h data.h node.h cons.h
filt.o:		filt.c dbg.h ev.h default.h filt.h state.h timo.h pool.h \
		mux.h
frame.o:	frame.c dbg.h track.h ev.h default.h state.h frame.h
lex.o:		lex.c dbg.h lex.h str.h textio.h cons.h
main.o:		main.c dbg.h str.h cons.h ev.h default.h mux.h track.h \
		state.h song.h name.h frame.h filt.h timo.h sysex.h \
		metro.h user.h mididev.h textio.h
mdep.o:		mdep.c default.h mux.h rmidi.h mdep.h mididev.h cons.h \
		user.h exec.h name.h str.h dbg.h
metro.o:	metro.c dbg.h mux.h metro.h ev.h default.h timo.h
mididev.o:	mididev.c dbg.h default.h mididev.h data.h name.h str.h \
		rmidi.h mdep.h pool.h cons.h
mux.o:		mux.c dbg.h ev.h default.h mdep.h mux.h rmidi.h \
		mididev.h sysex.h timo.h
name.o:		name.c dbg.h name.h str.h
node.o:		node.c dbg.h str.h data.h node.h exec.h name.h cons.h
parse.o:	parse.c dbg.h textio.h lex.h data.h parse.h node.h \
		cons.h
pool.o:		pool.c dbg.h pool.h
rmidi.o:	rmidi.c dbg.h default.h mdep.h ev.h sysex.h mux.h \
		rmidi.h mididev.h
rmidish.o:	rmidish.c
saveload.o:	saveload.c dbg.h name.h str.h song.h track.h ev.h \
		default.h state.h frame.h filt.h timo.h sysex.h metro.h \
		parse.h lex.h textio.h saveload.h
smf.o:		smf.c dbg.h sysex.h track.h ev.h default.h state.h \
		song.h name.h str.h frame.h filt.h timo.h metro.h smf.h \
		cons.h
song.o:		song.c dbg.h mididev.h mux.h track.h ev.h default.h \
		state.h frame.h filt.h timo.h song.h name.h str.h \
		sysex.h metro.h cons.h
state.o:	state.c dbg.h pool.h state.h ev.h default.h
str.o:		str.c dbg.h str.h
sysex.o:	sysex.c dbg.h sysex.h default.h pool.h
textio.o:	textio.c dbg.h textio.h cons.h
timo.o:		timo.c dbg.h timo.h
track.o:	track.c dbg.h pool.h track.h ev.h default.h state.h
user.o:		user.c dbg.h default.h node.h exec.h name.h str.h data.h \
		cons.h textio.h lex.h parse.h mux.h mididev.h track.h \
		ev.h state.h song.h frame.h filt.h timo.h sysex.h \
		metro.h user.h smf.h saveload.h rmidi.h mdep.h
user_chan.o:	user_chan.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h frame.h state.h ev.h track.h song.h filt.h \
		timo.h sysex.h metro.h user.h saveload.h textio.h
user_dev.o:	user_dev.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h mididev.h song.h track.h ev.h state.h \
		frame.h filt.h timo.h sysex.h metro.h user.h textio.h
user_filt.o:	user_filt.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h song.h track.h ev.h state.h frame.h filt.h \
		timo.h sysex.h metro.h user.h saveload.h textio.h
user_song.o:	user_song.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h frame.h state.h ev.h song.h track.h filt.h \
		timo.h sysex.h metro.h user.h smf.h saveload.h textio.h
user_sx.o:	user_sx.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h song.h track.h ev.h state.h frame.h filt.h \
		timo.h sysex.h metro.h user.h saveload.h textio.h
user_trk.o:	user_trk.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h frame.h state.h ev.h track.h song.h filt.h \
		timo.h sysex.h metro.h user.h saveload.h textio.h

# --------------------------------------------- dependencies for rmidish ---

RMIDISH_OBJS = rmidish.o

rmidish:	midish ${RMIDISH_OBJS}
		${CC} ${LDFLAGS} ${READLINE_LDFLAGS} ${RMIDISH_OBJS} \
		-o rmidish ${READLINE_LIB}

install-rmidish:rmidish
		mkdir -p ${BIN_DIR} ${MAN1_DIR}
		cp rmidish ${BIN_DIR}
		cp rmidish.1 ${MAN1_DIR}

rmidish.o:	rmidish.c
		${CC} -c ${CFLAGS} ${READLINE_CFLAGS} ${READLINE_INCLUDE} $<

