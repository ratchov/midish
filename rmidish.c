/*
 * Copyright (c) 2003-2007 Alexandre Ratchov <alex@caoua.org>
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
char prompt[PROMPTLENGTH];
int quit = 0;

/*
 * check if midish sent asynchronous position notification
 * and if so update the prompt
 */
void
asyncline(char *str)
{
	unsigned m, b, t;

	if (sscanf(linebuf, "+pos %u %u %u", &m, &b, &t) == 3) {
		snprintf(prompt, PROMPTLENGTH, "[%04u:%02u]> ", m, b);
		rl_set_prompt(prompt);
		rl_redisplay();
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
		asyncline(linebuf);
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

	rl_callback_handler_install("[----:--]> ", userline);
	waitready();
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
			asyncline(linebuf);
		}
	}
	rl_callback_handler_remove();
	midish_close(&midish);
	fputs("\n", stdout);
	return 0;
}
