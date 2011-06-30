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

/*
 * This program gives an example on how to create a front-end to midish
 *
 * first we create a pair of pipes corresponding to the stdin/stdout of midish
 * and we start it.
 *
 * then, to send commands to midish we write to its standard input (though the
 * first pipe) and to receive the results we read its standart output (trough
 * the second pipe).
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <limits.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MIDISH "midish"

struct midish
{
	int pid;				/* pid of midish */
	int sin;				/* stdin of midish */
	int sout;				/* stdout of midish */
#define MIDISH_BUFLEN 1024
	unsigned char buf[MIDISH_BUFLEN];	/* fifo for reading */
	unsigned buf_start;			/* start position of the buf */
	unsigned buf_used;			/* bytes available  */
	int eof;				/* 1 if eof reached */
};

/*
 * start midish
 */
int
midish_open(struct midish *p, char *path)
{
	int ipipe[2], opipe[2];

	/*
	 * allocate pipes and fork
	 */
	if (pipe(ipipe) < 0 || pipe(opipe) < 0 ) {
		perror("pipe");
		exit(1);
	}
	p->pid = fork();
	if (p->pid < 0) {
		perror("fork");
		return -1;
	}
	if (p->pid == 0) {
		close(ipipe[1]);
		close(opipe[0]);
		if (dup2(ipipe[0], STDIN_FILENO) < 0) {
			perror("dup2");
			exit(1);
		}
		if (dup2(opipe[1], STDOUT_FILENO) < 0) {
			perror("dup2");
			exit(1);
		}
		close(ipipe[0]);
		close(opipe[1]);

		/*
		 * run midish
		 */
		if (execlp(path, path, "-v", (char *)NULL) < 0) {
			perror(path);
		}
		exit(1);
	}
	close(ipipe[0]);
	close(opipe[1]);
	p->sout = opipe[0];
	p->sin = ipipe[1];
	p->eof = 0;
	p->buf_start = 0;
	p->buf_used = 0;

	/*
         * lower scheduling priority
	 */
	if (setpriority(PRIO_PROCESS, getpid(), 15) < 0)
		perror("setpriority");
	return 0;
}

/*
 * close connection to midish, and wait for it to terminate
 */
void
midish_close(struct midish *p)
{
	int status;

	close(p->sout);
	close(p->sin);
	if (waitpid(p->pid, &status, 0) < 0) {
		perror("waitpid");
	}
}

/*
 * read a character from midish's stdout
 */
int
midish_getc(struct midish *p)
{
	unsigned end, avail, count;
	int n, c;

	if (p->eof)
		return -1;
	if (p->buf_used == 0) {
		avail = MIDISH_BUFLEN - p->buf_used;
		end = p->buf_start + p->buf_used;
		if (end >= MIDISH_BUFLEN)
			end -= MIDISH_BUFLEN;
		count = MIDISH_BUFLEN - end;
		if (count > avail)
			count = avail;
		n = read(p->sout, p->buf + end, count);
		if (n < 0) {
			perror("midish_getc: read");
			return -1;
		}
		if (n == 0) {
			p->eof = 1;
			return -1;
		}
		p->buf_used += n;
	}
	c = p->buf[p->buf_start];
	p->buf_start++;
	if (p->buf_start >= MIDISH_BUFLEN)
		p->buf_start -= MIDISH_BUFLEN;
	p->buf_used--;
	return c;
}

/*
 * send a line on midish's stdin
 */
void
midish_puts(struct midish *p, char *str)
{
	if (write(p->sin, str, strlen(str)) < 0) {
		perror("midish_puts: write");
	}
}

/*
 * get a line from midish's stdout
 */
char *
midish_gets(struct midish *p, char *str, size_t maxsize)
{
	int c;
	size_t size = 0;

	maxsize--;
	for (;;) {
		c = midish_getc(p);
		if (c == -1) {
			return NULL;
		}
		if (size == maxsize) {
			fprintf(stderr, "line too long\n");
			return NULL;
		}
		str[size++] = c;
		if (c == '\n') {
			str[size] = '\0';
			break;
		}
	}
	return str;
}

/* ---------------------------------------------------------------- */

#define LINELENGTH 10000
#define PROMPTLENGTH 20

struct midish midish;
char linebuf[LINELENGTH];
char prompt[PROMPTLENGTH] = "[----:--]>";
int quit = 0;

/*
 * check if midish sent asynchronous position notification
 * and if so update the prompt
 */
void
asyncline(void)
{
	unsigned m, b, t;

	if (sscanf(linebuf, "+pos %u %u %u", &m, &b, &t) == 3) {
		snprintf(prompt, PROMPTLENGTH, "[%04u:%02u]> ", m, b);
		rl_set_prompt(prompt);
	}
}

/*
 * wait midish to become ready to accept another line
 */
void
waitready(void)
{
	for (;;) {
		if (!midish_gets(&midish, linebuf, LINELENGTH)) {
			exit(1);
		}
		if (linebuf[0] != '+') {
			fprintf(stdout, "%s", linebuf);
			continue;
		}
		if (strcmp(linebuf, "+ready\n") == 0)
			return;
		asyncline();
	}
}

/*
 * call-back, called by the readline library once a line is ready
 */
void
userline(char *rl)
{
	if (rl == NULL) {
		fputs("\n", stdout);
		quit = 1;
		return;
	}
	if (rl[0] != '\0') {
		add_history(rl);
	}
	midish_puts(&midish, rl);
	midish_puts(&midish, "\n");
	waitready();
	free(rl);
}

char *builtins[] = {
	/*
	 * generated with:
	 * grep blt_ builtin.h |sed -es/^.*blt_/\"/ -es/\(.*$/\",/ |fmt
	 */
	"panic", "debug", "exec", "print", "err", "h", "ev", "ci", "geti",
	"co", "geto", "cx", "getx", "setunit", "getunit", "goto", "getpos",
	"sel", "getlen", "setq", "getq", "fac", "getfac", "ct", "gett",
	"cf", "getf", "mute", "unmute", "getmute", "ls", "save", "load",
	"reset", "export", "import", "idle", "play", "rec", "stop", "tempo",
	"mins", "mcut", "mdup", "minfo", "mtempo", "msig", "mend", "ctlconf",
	"ctlconfx", "ctlunconf", "ctlinfo", "metro", "metrocf", "tlist",
	"tnew", "tdel", "tren", "texists", "taddev", "tsetf", "tgetf",
	"tcheck", "tcut", "tins", "tclr", "tpaste", "tcopy", "tmerge",
	"tquant", "ttransp", "tevmap", "tclist", "tinfo", "ilist", "iexists",
	"inew", "idel", "iren", "iset", "igetc", "igetd", "iaddev", "irmev",
	"iinfo", "olist", "oexists", "onew", "odel", "oren", "oset", "ogetc",
	"ogetd", "oaddev", "ormev", "oinfo", "flist", "fnew", "fdel", "fren",
	"fexists", "finfo", "freset", "fmap", "funmap", "ftransp", "fvcurve",
	"fchgin", "fchgout", "fswapin", "fswapout", "xlist", "xexists",
	"xnew", "xdel", "xren", "xinfo", "xrm", "xsetd", "xadd", "dnew",
	"ddel", "dmtcrx", "dmmctx", "dclkrx", "dclktx", "dclkrate", "dinfo",
	"dixctl", "doxctl",
	NULL
};


char *
genbuiltin(const char *text, int state)
{
	static char **pname;
	size_t len;

	if (state == 0)
		pname = &builtins[0];

	len = strlen(text);
	while (*pname != NULL) {
		if (strncmp(*pname, text, len) == 0)
			return strdup(*pname++);
		pname++;
	}
	return NULL;
}

char **
complete(const char *text, int start, int end)
{
	if (start != 0)
		return NULL;
	return rl_completion_matches(text, genbuiltin);
}

int
main(int argc, char *argv[])
{
	char path[PATH_MAX];
	unsigned dirlen, filelen;
	struct pollfd pfds[2];
	int n;

	/*
	 * determine the complete path of the midish executable
	 */
	if (argc > 0) {
		dirlen = strlen(argv[0]);
		while(dirlen > 0 && argv[0][dirlen - 1] != '/') dirlen--;
		memcpy(path, argv[0], dirlen);
	} else {
		dirlen = 0;
	}
	filelen = strlen(MIDISH);
	if (dirlen + filelen >= PATH_MAX) {
		fprintf(stderr, "midish file name too long\n");
		exit(1);
	}
	memcpy(path + dirlen, MIDISH, filelen + 1);

	/*
	 * if stdin or stdout is not a tty, then dont start the front end,
	 * just execute midish
	 */
	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
		if (execlp(path, path, (char *)NULL) < 0) {
			perror(path);
		}
		exit(1);
	}

	if (midish_open(&midish, path) < 0) {
		exit(1);
	}
	fprintf(stderr, "send EOF character (control-D) to quit\n");

	pfds[0].fd = STDIN_FILENO;
	pfds[0].events = POLLIN;

	pfds[1].fd = midish.sout;
	pfds[1].events = POLLIN;

	waitready();
	rl_attempted_completion_function = complete;
	rl_callback_handler_install(prompt, userline);
	while (!quit) {
		n = poll(pfds, 2, -1);
		if (pfds[0].revents & POLLIN) {
			/*
			 * got characters on the console,
			 * will call userline()
			 */
			rl_callback_read_char();
		}
		if (pfds[1].revents & POLLIN) {
			/*
			 * got characters from midish
			 */
			if (!midish_gets(&midish, linebuf, LINELENGTH)) {
				exit(1);
			}
			asyncline();
			rl_redisplay();
		}
	}
	rl_callback_handler_remove();
	midish_close(&midish);
	fputs("\n", stdout);
	return 0;
}
