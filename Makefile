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

#
# programs to build and install
#
PROGS = midish rmidish

all:		${PROGS}

install:	${PROGS}
		mkdir -p ${BIN_DIR} ${MAN1_DIR} ${DOC_DIR} ${EXAMPLES_DIR}
		cp ${PROGS} smfplay smfrec ${BIN_DIR}
		cp ${PROGS:=.1} smfplay.1 smfrec.1 ${MAN1_DIR}
		cp README manual.html ${DOC_DIR}
		cp midishrc sample.sng ${EXAMPLES_DIR}

check:		midish
		cd regress && ./run-test *.cmd

clean:
		rm -f -- ${PROGS} *.o *~ *.bak *.tmp *.ln *.s *.out *.core core
		cd regress && rm -f -- *~ *.tmp? *.core *.log *.diff *.orig *.rej

# ---------------------------------------------------------- dependencies ---

MIDISH_OBJS = \
cons.o conv.o data.o dbg.o ev.o exec.o filt.o frame.o lex.o \
main.o mdep.o metro.o mididev.o mixout.o mux.o name.o node.o \
norm.o parse.o pool.o rmidi.o saveload.o smf.o song.o state.o \
str.o sysex.o textio.o timo.o track.o user.o user_trk.o \
user_chan.o user_filt.o user_sx.o user_song.o user_dev.o

midish:		${MIDISH_OBJS}
		${CC} ${LDFLAGS} ${MIDISH_OBJS} -o midish

rmidish:	rmidish.c midish
		${CC} ${CFLAGS} ${READLINE_CFLAGS} ${READLINE_INCLUDE} rmidish.c \
		${LDFLAGS} ${READLINE_LDFLAGS} -o rmidish ${READLINE_LIB}

cons.o:		cons.c dbg.h textio.h cons.h user.h
conv.o:		conv.c dbg.h state.h ev.h default.h conv.h
data.o:		data.c dbg.h str.h cons.h data.h
dbg.o:		dbg.c dbg.h
ev.o:		ev.c dbg.h ev.h default.h str.h
exec.o:		exec.c dbg.h exec.h name.h str.h data.h node.h cons.h
filt.o:		filt.c dbg.h ev.h default.h filt.h pool.h mux.h
frame.o:	frame.c dbg.h track.h ev.h default.h frame.h state.h
lex.o:		lex.c dbg.h lex.h str.h textio.h cons.h
main.o:		main.c dbg.h str.h cons.h ev.h default.h mux.h track.h \
		song.h name.h frame.h state.h filt.h sysex.h metro.h \
		timo.h user.h mididev.h textio.h
mdep.o:		mdep.c default.h mux.h rmidi.h mdep.h mididev.h cons.h \
		user.h exec.h name.h str.h dbg.h
metro.o:	metro.c dbg.h mux.h metro.h ev.h default.h timo.h
mididev.o:	mididev.c dbg.h default.h mididev.h rmidi.h mdep.h \
		pool.h cons.h str.h
mixout.o:	mixout.c dbg.h ev.h default.h filt.h pool.h mux.h timo.h \
		state.h
mux.o:		mux.c dbg.h ev.h default.h mdep.h mux.h rmidi.h \
		mididev.h sysex.h timo.h state.h conv.h norm.h
name.o:		name.c dbg.h name.h str.h
node.o:		node.c dbg.h str.h data.h node.h exec.h name.h cons.h
norm.o:		norm.c dbg.h ev.h default.h norm.h state.h timo.h pool.h \
		mux.h
parse.o:	parse.c dbg.h textio.h lex.h data.h parse.h node.h \
		cons.h
pool.o:		pool.c dbg.h pool.h
rmidi.o:	rmidi.c dbg.h default.h mdep.h ev.h sysex.h mux.h \
		rmidi.h mididev.h
saveload.o:	saveload.c dbg.h name.h str.h song.h track.h ev.h \
		default.h frame.h state.h filt.h sysex.h metro.h timo.h \
		parse.h lex.h textio.h saveload.h conv.h
smf.o:		smf.c dbg.h sysex.h track.h ev.h default.h song.h name.h \
		str.h frame.h state.h filt.h metro.h timo.h smf.h cons.h \
		conv.h
song.o:		song.c dbg.h mididev.h mux.h track.h ev.h default.h \
		frame.h state.h filt.h song.h name.h str.h sysex.h \
		metro.h timo.h cons.h mixout.h
state.o:	state.c dbg.h pool.h state.h ev.h default.h
str.o:		str.c dbg.h str.h
sysex.o:	sysex.c dbg.h sysex.h default.h pool.h
textio.o:	textio.c dbg.h textio.h cons.h
timo.o:		timo.c dbg.h timo.h
track.o:	track.c dbg.h pool.h track.h ev.h default.h
user.o:		user.c dbg.h default.h node.h exec.h name.h str.h data.h \
		cons.h textio.h lex.h parse.h mux.h mididev.h track.h \
		ev.h song.h frame.h state.h filt.h sysex.h metro.h \
		timo.h user.h smf.h saveload.h rmidi.h mdep.h
user_trk.o:	user_trk.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h frame.h state.h ev.h track.h song.h filt.h \
		sysex.h metro.h timo.h user.h saveload.h textio.h
user_chan.o:	user_chan.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h frame.h state.h ev.h track.h song.h filt.h \
		sysex.h metro.h timo.h user.h saveload.h textio.h
user_filt.o:	user_filt.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h song.h track.h ev.h frame.h state.h filt.h \
		sysex.h metro.h timo.h user.h saveload.h textio.h
user_sx.o:	user_sx.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h song.h track.h ev.h frame.h state.h filt.h \
		sysex.h metro.h timo.h user.h saveload.h textio.h
user_song.o:	user_song.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h frame.h state.h ev.h song.h track.h filt.h \
		sysex.h metro.h timo.h user.h smf.h saveload.h textio.h
user_dev.o:	user_dev.c dbg.h default.h node.h exec.h name.h str.h \
		data.h cons.h mididev.h song.h track.h ev.h frame.h \
		state.h filt.h sysex.h metro.h timo.h user.h textio.h
