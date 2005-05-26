#
# compiler and linker flags
#
CC = gcc
CFLAGS = -g -Wall -Wstrict-prototypes -Wundef -Wpointer-arith # -Wconversion
LDFLAGS = 

#
# comment this if you don't have a working readline(3) library
#
RL_CFLAGS = -DHAVE_READLINE -I/usr/local/include
RL_LDFLAGS = -L/usr/local/lib -lreadline -ltermcap

#
# binaries, documentation and examples will be installed in ${PREFIX}/bin,
# ${PREFIX}/share/doc/midish and ${PREFIX}/share/examples/midish
#
PREFIX = ${HOME}

#
# common binaries
#
INSTALL = install
RM = rm

PROG = midish
OBJS = \
cons.o data.o dbg.o ev.o filt.o lex.o main.o mdep.o mididev.o mux.o name.o \
parse.o pool.o rmidi.o saveload.o smf.o song.o str.o sysex.o textio.o \
track.o trackop.o tree.o user.o

all:		${PROG}

${PROG}:	${OBJS}
		${CC} ${OBJS} -o ${PROG} ${LDFLAGS} ${RL_LDFLAGS}

clean:		
		${RM} -f -- ${PROG} mkcurves .depend *~ *.bak *.o *.s *.core core

install:	${PROG}
		${INSTALL} -d ${PREFIX}/bin
		${INSTALL} -d ${PREFIX}/share/doc/midish
		${INSTALL} -d ${PREFIX}/share/examples/midish
		${INSTALL} -m 755 -s ${PROG} ${PREFIX}/bin
		${INSTALL} -m 644 manual.html tutorial.html ${PREFIX}/share/doc/midish
		${INSTALL} -m 644 midishrc sample.sng ${PREFIX}/share/examples/midish
		@echo
		@echo You can copy manually ${PREFIX}/share/examples/midish/midishrc
		@echo into ~/.midishrc
		@echo

mkcurves:	mkcurves.c
		${CC} ${CFLAGS} -o mkcurves mkcurves.c ${LDFLAGS} -lm

.c.o:
		${CC} ${CFLAGS} ${RL_CFLAGS} -c $<

cons.o:		cons.c dbg.h cons.h textio.h
data.o:		data.c dbg.h data.h str.h user.h
dbg.o:		dbg.c dbg.h dbg.h
ev.o:		ev.c dbg.h ev.h default.h str.h
filt.o:		filt.c dbg.h ev.h default.h filt.h pool.h
lex.o:		lex.c dbg.h lex.h textio.h str.h user.h
main.o:		main.c dbg.h cons.h ev.h default.h mux.h track.h pool.h song.h str.h str.h name.h filt.h user.h mididev.h sysex.h
mdep.o:		mdep.c dbg.h default.h mux.h rmidi.h mdep.h mididev.h user.h tree.h str.h str.h name.h
mididev.o:	mididev.c dbg.h default.h mididev.h data.h user.h str.h str.h name.h rmidi.h mdep.h pool.h
mux.o:		mux.c dbg.h ev.h default.h mdep.h mux.h rmidi.h mididev.h sysex.h
name.o:		name.c dbg.h str.h str.h name.h
parse.o:	parse.c dbg.h lex.h textio.h data.h tree.h str.h str.h name.h parse.h
pool.o:		pool.c dbg.h pool.h
rmidi.o:	rmidi.c dbg.h default.h mdep.h ev.h sysex.h mux.h rmidi.h mididev.h
saveload.o:	saveload.c dbg.h str.h name.h song.h track.h trackop.h ev.h default.h pool.h filt.h parse.h lex.h textio.h saveload.h sysex.h
smf.o:		smf.c dbg.h track.h ev.h default.h pool.h song.h str.h name.h filt.h smf.h sysex.h
song.o:		song.c dbg.h mux.h track.h ev.h pool.h trackop.h filt.h sysex.h song.h str.h name.h user.h default.h
str.o:		str.c dbg.h str.h
sysex.o:	sysex.h dbg.h default.h pool.h
textio.o:	textio.c cons.h dbg.h textio.h user.h
track.o:	track.c dbg.h track.h ev.h default.h pool.h
trackop.o:	trackop.c dbg.h trackop.h track.h ev.h pool.h default.h
tree.o:		tree.c dbg.h tree.h str.h name.h data.h user.h
user.o:		user.c dbg.h default.h tree.h str.h name.h data.h lex.h textio.h parse.h mux.h mididev.h trackop.h track.h ev.h pool.h song.h filt.h user.h smf.h saveload.h rmidi.h sysex.h
