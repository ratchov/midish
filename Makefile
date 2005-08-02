#
# comment the following if you don't have a working readline(3) library
# (don't forget to run 'make clean')
#
READLINE_CFLAGS = -DHAVE_READLINE
READLINE_LDFLAGS = -L/usr/local/lib	# path to readline libraries
READLINE_INCLUDE = -I/usr/local/include # path to readline header files
READLINE_LIB = -lreadline -ltermcap	# readline libraries

#
# binaries, documentation and examples will be installed in 
# ${BIN_DIR}, ${DOC_DIR}, ${EXAMPLES_DIR}
#
PREFIX = /usr/local
BIN_DIR = ${PREFIX}/bin
DOC_DIR = ${PREFIX}/share/doc/midish
EXAMPLES_DIR = ${PREFIX}/share/examples/midish


PROG = midish
OBJS = \
cons.o data.o dbg.o ev.o exec.o filt.o lex.o main.o mdep.o mididev.o \
mux.o name.o node.o parse.o pool.o rmidi.o saveload.o smf.o song.o \
str.o sysex.o textio.o track.o trackop.o user.o \
user_trk.o user_chan.o user_filt.o user_sx.o user_song.o user_dev.o 

all:		${PROG}

${PROG}:	${OBJS}
		${CC} ${LDFLAGS} ${READLINE_LDFLAGS} ${OBJS} -o ${PROG} ${READLINE_LIB}

clean:		
		rm -f -- ${PROG} *~ *.bak *.o *.s *.out *.core core

install:	${PROG}
		mkdir -p ${BIN_DIR}
		mkdir -p ${DOC_DIR}
		mkdir -p ${EXAMPLES_DIR}
		cp ${PROG} ${BIN_DIR}
		cp manual.html tutorial.html ${DOC_DIR}
		cp midishrc sample.sng ${EXAMPLES_DIR}
		@echo
		@echo You can copy manually ${EXAMPLES_DIR}/midishrc
		@echo into ~/.midishrc
		@echo
.c.o:
		${CC} ${CFLAGS} ${READLINE_CFLAGS} ${READLINE_INCLUDE} -c $<

cons.o:		cons.c dbg.h textio.h cons.h
data.o:		data.c dbg.h str.h cons.h data.h
dbg.o:		dbg.c dbg.h
ev.o:		ev.c dbg.h ev.h default.h str.h
exec.o:		exec.c dbg.h exec.h name.h str.h data.h node.h cons.h
filt.o:		filt.c dbg.h ev.h default.h filt.h pool.h
lex.o:		lex.c dbg.h lex.h str.h textio.h cons.h
main.o:		main.c dbg.h str.h cons.h ev.h default.h mux.h track.h song.h name.h filt.h sysex.h user.h mididev.h
mdep.o:		mdep.c default.h mux.h rmidi.h mdep.h mididev.h user.h exec.h name.h str.h
mididev.o:	mididev.c dbg.h default.h mididev.h data.h cons.h name.h str.h rmidi.h mdep.h pool.h
mux.o:		mux.c dbg.h ev.h default.h mdep.h mux.h rmidi.h mididev.h sysex.h
name.o:		name.c dbg.h name.h str.h
node.o:		node.c dbg.h str.h data.h node.h exec.h name.h cons.h
parse.o:	parse.c dbg.h textio.h lex.h data.h parse.h node.h cons.h
pool.o:		pool.c dbg.h pool.h
rmidi.o:	rmidi.c dbg.h default.h mdep.h ev.h sysex.h mux.h rmidi.h mididev.h
saveload.o:	saveload.c dbg.h name.h str.h song.h track.h ev.h default.h filt.h sysex.h parse.h lex.h textio.h saveload.h trackop.h
smf.o:		smf.c dbg.h track.h ev.h default.h trackop.h song.h name.h str.h filt.h sysex.h cons.h smf.h
song.o:		song.c dbg.h mux.h track.h ev.h default.h trackop.h filt.h song.h name.h str.h sysex.h cons.h
str.o:		str.c dbg.h str.h
sysex.o:	sysex.c dbg.h sysex.h default.h pool.h
textio.o:	textio.c dbg.h textio.h cons.h
track.o:	track.c dbg.h pool.h track.h ev.h default.h
trackop.o:	trackop.c dbg.h trackop.h track.h ev.h default.h
user.o: 	user.c dbg.h default.h node.h exec.h name.h str.h data.h textio.h lex.h parse.h mux.h mididev.h trackop.h track.h ev.h song.h filt.h sysex.h user.h smf.h saveload.h rmidi.h mdep.h cons.h
user_chan.o:	user_chan.c dbg.h default.h node.h exec.h name.h str.h data.h cons.h trackop.h track.h ev.h song.h filt.h sysex.h user.h saveload.h textio.h
user_dev.o:	user_dev.c dbg.h default.h node.h exec.h name.h str.h data.h cons.h mididev.h song.h track.h ev.h filt.h sysex.h user.h textio.h
user_filt.o:	user_filt.c dbg.h default.h node.h exec.h name.h str.h data.h cons.h song.h track.h ev.h filt.h sysex.h user.h saveload.h textio.h
user_song.o: 	user_song.c dbg.h default.h node.h exec.h name.h str.h data.h cons.h trackop.h song.h track.h ev.h filt.h sysex.h user.h smf.h saveload.h textio.h
user_sx.o:	user_sx.c dbg.h default.h node.h exec.h name.h str.h data.h cons.h song.h track.h ev.h filt.h sysex.h user.h saveload.h textio.h
user_trk.o:	user_trk.c dbg.h default.h node.h exec.h name.h str.h data.h cons.h trackop.h track.h ev.h song.h filt.h sysex.h user.h  saveload.h textio.h
